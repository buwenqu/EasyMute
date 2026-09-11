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

    // 需求（2026-09-12 变更）：热键注册完全静默——不做占用检测、不向用户提示结果；
    // 失败时静音功能暂不可用，用户重新设置（改键）会自动重试。
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
        DebugLog(L"[app] WM_HOTKEY (id=%u)\n", static_cast<unsigned>(wp));
        // 录入框正在等待输入时按下“当前快捷键”：该按键会被系统热键机制拦截、控件收不到，
        // 这里把它解释为“设置为当前快捷键”并完成录入（需求 2026-09-12）。
        if (SettingsDialog::TryCompleteCaptureWithCurrentHotkey(hotkey_.Modifiers(), hotkey_.Key())) {
            return 0;
        }
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
        DebugLog(L"[app] toggle: 无法解析前台窗口\n");
        return;
    }
    if (CompareStringOrdinal(info.exeName.c_str(), -1, L"explorer.exe", -1, TRUE) == CSTR_EQUAL) {
        DebugLog(L"[app] toggle: 焦点是桌面 / 资源管理器，忽略\n");
        return;
    }

    bool nowMuted = false;
    switch (ToggleAppMute(info.relatedPids, info.exePath, nowMuted)) {
        case MuteToggleResult::Ok:
            // 需求（2026-09-12 变更）：仅在静音状态真正切换时提示一次
            Notify((nowMuted ? L"已静音：" : L"已恢复：") + info.exeName);
            break;
        case MuteToggleResult::NoSession:
            DebugLog(L"[app] toggle: 无音频会话（%ls）\n", info.exeName.c_str());
            break;
        case MuteToggleResult::Error:
            DebugLog(L"[app] toggle: 音频系统错误\n");
            break;
    }
}

void App::Notify(const std::wstring& text) {
    if (!config_.showNotification) return;  // 设置开关（默认开启)

    // 防抖：同一文本 300ms 内只提示一次，避免重复触发造成刷屏
    static DWORD lastTick = 0;
    static std::wstring lastText;
    const DWORD now = GetTickCount();
    if (text == lastText && now - lastTick < 300) return;
    lastText = text;
    lastTick = now;

    DebugLog(L"[app] balloon: %ls\n", text.c_str());
    tray_.ShowBalloon(L"EasyMute", text, false);
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
    // 需求（2026-09-12 变更）：不在界面标注“未生效”，直接展示用户设置的组合
    std::wstring tip = L"EasyMute —— " + FormatHotkey(config_.hotkeyMods, config_.hotkeyVk);
    tip += L"：静音 / 恢复当前应用";
    tray_.SetTip(tip);
}

void App::ExitApp() {
    hotkey_.Unregister();
    tray_.Remove();
    DestroyWindow(hwnd_);
}

bool App::TryApplyHotkey(unsigned mods, unsigned vk, bool persist) {
    // 需求（2026-09-11/09-12 变更）：直接接受并保存用户选择，尽力注册；
    // 不做占用检测、不在界面标注注册结果，失败时静默保留旧键可用。
    const bool registered = hotkey_.Apply(mods, vk);
    config_.hotkeyMods = mods;
    config_.hotkeyVk = vk;
    if (persist) config_.Save();
    UpdateTrayTip();
    if (!registered) DebugLog(L"[app] hotkey not registered (silent), setting kept\n");
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
