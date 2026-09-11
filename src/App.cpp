#include "App.h"

#include <commctrl.h>

#include "AppResolver.h"
#include "AudioSession.h"
#include "AutoStart.h"
#include "DebugLog.h"
#include "HotkeyBox.h"
#include "HotkeyName.h"
#include "SettingsDialog.h"
#include "resource.h"

namespace {
constexpr UINT kMenuSettings = 2001;
constexpr UINT kMenuExit = 2002;

App* g_instance = nullptr;
}  // namespace

App* App::Instance() { return g_instance; }

bool App::Init(HINSTANCE instance) {
    g_instance = this;
    instance_ = instance;

    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    config_.Load();
    DebugLog(L"[app] config: mods=%u vk=%u autoStart=%d\n", config_.hotkeyMods, config_.hotkeyVk,
             config_.autoStart ? 1 : 0);
    if (config_.autoStart) {
        AutoStart::Set(true);  // 路径自愈：程序被移动后刷新启动项
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kMainWindowClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    // 隐藏消息窗口（不显示、不出现在任务栏/Alt+Tab）
    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, kMainWindowClass, L"EasyMute", WS_OVERLAPPED,
                            0, 0, 0, 0, nullptr, nullptr, instance, this);
    if (!hwnd_) return false;
    DebugLog(L"[app] main window: %p\n", static_cast<void*>(hwnd_));

    RegisterHotkeyBoxClass(instance);

    taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");
    openSettingsMsg_ = RegisterWindowMessageW(kOpenSettingsMessage);
    hotkey_.Attach(hwnd_);

    HICON icon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON),
                                               GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    tray_.Add(hwnd_, icon, L"EasyMute");
    UpdateTrayTip();

    // 需求（2026-09-11 变更）：启动注册失败不再弹“被占用”气泡,
    // 注册状态通过托盘提示（“未生效”）被动呈现。
    const bool hotkeyOk = hotkey_.Apply(config_.hotkeyMods, config_.hotkeyVk);
    DebugLog(L"[app] hotkey apply: %d\n", hotkeyOk ? 1 : 0);
    UpdateTrayTip();
    return true;
}

int App::Run() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        // 模型化设置对话框需要 IsDialogMessage 处理 Tab 导航等
        HWND dialog = SettingsDialog::Hwnd();
        if (dialog != nullptr && IsDialogMessageW(dialog, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
    }
    App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->HandleMessage(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// 注意：必须使用传入的 hwnd 而不是成员 hwnd_——
// 窗口创建期间（WM_NCCREATE/WM_CREATE）成员尚未赋值，用 nullptr 兜底会导致创建失败。
LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_HOTKEY) {
        ToggleForegroundAppMute();
        return 0;
    }
    if (msg == kTrayCallbackMessage) {
        OnTrayMessage(lp);
        return 0;
    }
    if (taskbarCreatedMsg_ != 0 && msg == taskbarCreatedMsg_) {
        tray_.ReAdd();
        return 0;
    }
    if (openSettingsMsg_ != 0 && msg == openSettingsMsg_) {
        OpenSettings();
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void App::ToggleForegroundAppMute() {
    FocusAppInfo info;
    if (!ResolveFocusApp(info)) {
        Notify(L"未能识别当前焦点窗口所属的应用", true, true);
        return;
    }
    if (CompareStringOrdinal(info.exeName.c_str(), -1, L"explorer.exe", -1, TRUE) == CSTR_EQUAL) {
        Notify(L"当前焦点是桌面 / 资源管理器，未执行静音操作", true, false);
        return;
    }

    bool nowMuted = false;
    switch (ToggleAppMute(info.relatedPids, info.exePath, nowMuted)) {
        case MuteToggleResult::Ok:
            Notify((nowMuted ? L"已静音：" : L"已恢复：") + info.exeName, false, false);
            break;
        case MuteToggleResult::NoSession:
            Notify(L"当前应用没有音频输出：" + info.exeName, true, false);
            break;
        case MuteToggleResult::Error:
            Notify(L"静音操作失败（音频系统异常）", true, true);
            break;
    }
}

void App::Notify(const std::wstring& text, bool showAlways, bool isError) {
    if (!showAlways && !config_.showNotification) return;
    tray_.ShowBalloon(L"EasyMute", text, isError);
}

void App::OnTrayMessage(LPARAM lp) {
    switch (LOWORD(lp)) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu();
            break;
        case WM_LBUTTONDBLCLK:
            OpenSettings();
            break;
        default:
            break;
    }
}

void App::ShowTrayMenu() {
    POINT pt = {};
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kMenuSettings, L"设置(&S)...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"退出(&X)");

    SetForegroundWindow(hwnd_);  // 保证菜单可正常收起
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    PostMessageW(hwnd_, WM_NULL, 0, 0);

    if (command == kMenuSettings) {
        OpenSettings();
    } else if (command == kMenuExit) {
        ExitApp();
    }
}

void App::UpdateTrayTip() {
    std::wstring tip = L"EasyMute —— " + FormatHotkey(config_.hotkeyMods, config_.hotkeyVk);
    const bool active = hotkey_.Has() && hotkey_.Modifiers() == config_.hotkeyMods &&
                        hotkey_.Key() == config_.hotkeyVk;
    if (!active) tip += L"（未生效）";
    tip += L"：静音 / 恢复当前应用";
    tray_.SetTip(tip);
}

void App::ExitApp() {
    hotkey_.Unregister();
    tray_.Remove();
    DestroyWindow(hwnd_);
}

bool App::TryApplyHotkey(unsigned mods, unsigned vk, bool persist) {
    // 需求（2026-09-11 变更）：设置快捷键不再因“被占用”拦截用户——
    // 无论注册是否成功都接受并保存；失败时保留旧键可用，托盘提示标注“未生效”。
    const bool registered = hotkey_.Apply(mods, vk);
    config_.hotkeyMods = mods;
    config_.hotkeyVk = vk;
    if (persist) config_.Save();
    UpdateTrayTip();
    if (!registered) DebugLog(L"[app] hotkey not registered (occupied?), setting kept\n");
    return registered;
}

void App::ApplyDefaultHotkey() {
    TryApplyHotkey(kDefaultHotkeyMods, kDefaultHotkeyKey, true);
    SettingsDialog::SyncHotkey(kDefaultHotkeyMods, kDefaultHotkeyKey);
}

void App::ApplyAutoStart(bool enabled) {
    config_.autoStart = enabled;
    AutoStart::Set(enabled);
    config_.Save();
}

void App::ApplyNotification(bool enabled) {
    config_.showNotification = enabled;
    config_.Save();
}

void App::OpenSettings() {
    SettingsDialog::Show(instance_, this);
}
