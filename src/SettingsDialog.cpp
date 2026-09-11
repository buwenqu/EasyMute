#include "SettingsDialog.h"

#include "App.h"
#include "Config.h"
#include "HotkeyBox.h"
#include "resource.h"

namespace {

HWND g_hwnd = nullptr;
App* g_app = nullptr;

constexpr wchar_t kVersionText[] = L"版本 v1.0.0";

void CenterWindow(HWND dlg) {
    RECT rc = {};
    GetWindowRect(dlg, &rc);
    POINT cursor = {};
    GetCursorPos(&cursor);
    const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(monitor, &mi)) return;

    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    const int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;
    const int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;
    SetWindowPos(dlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void RefreshControls(HWND dlg) {
    if (!g_app) return;
    const Config& config = g_app->GetConfig();
    CheckDlgButton(dlg, IDC_CHK_AUTOSTART, config.autoStart ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_CHK_NOTIFY, config.showNotification ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(dlg, IDC_HOTKEYBOX, HKM_SETVALUE,
                        static_cast<WPARAM>((config.hotkeyMods << 16) | (config.hotkeyVk & 0xFFFFu)), 0);
}

INT_PTR CALLBACK DialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG: {
            g_app = reinterpret_cast<App*>(lp);

            // 中文文案统一在代码中设置（资源文件保持 ASCII，避免编码差异）
            SetWindowTextW(dlg, L"EasyMute 设置");
            SetDlgItemTextW(dlg, IDC_STATIC_HKLABEL, L"静音快捷键");
            SetDlgItemTextW(dlg, IDC_BTN_DEFAULT, L"恢复默认");
            SetDlgItemTextW(dlg, IDC_CHK_AUTOSTART, L"开机自动启动");
            SetDlgItemTextW(dlg, IDC_CHK_NOTIFY, L"操作后显示气泡提示");
            SetDlgItemTextW(dlg, IDC_STATIC_VERSION, kVersionText);
            SetDlgItemTextW(dlg, IDC_BTN_CLOSE, L"关闭");

            RefreshControls(dlg);
            CenterWindow(dlg);
            SetFocus(GetDlgItem(dlg, IDC_BTN_CLOSE));
            return TRUE;
        }
        case WM_COMMAND: {
            const WORD id = LOWORD(wp);
            const WORD code = HIWORD(wp);
            switch (id) {
                case IDC_CHK_AUTOSTART:
                    if (code == BN_CLICKED && g_app) {
                        g_app->ApplyAutoStart(IsDlgButtonChecked(dlg, IDC_CHK_AUTOSTART) == BST_CHECKED);
                    }
                    return TRUE;
                case IDC_CHK_NOTIFY:
                    if (code == BN_CLICKED && g_app) {
                        g_app->ApplyNotification(IsDlgButtonChecked(dlg, IDC_CHK_NOTIFY) == BST_CHECKED);
                    }
                    return TRUE;
                case IDC_BTN_DEFAULT:
                    if (g_app) g_app->ApplyDefaultHotkey();
                    return TRUE;
                case IDC_HOTKEYBOX:
                    if (code == HKN_CHANGED && g_app) {
                        const LRESULT packed = SendDlgItemMessageW(dlg, IDC_HOTKEYBOX, HKM_GETVALUE, 0, 0);
                        const unsigned mods = static_cast<unsigned>((packed >> 16) & 0xFFFF);
                        const unsigned vk = static_cast<unsigned>(packed & 0xFFFF);
                        if (g_app->TryApplyHotkey(mods, vk, true)) {
                            SendDlgItemMessageW(dlg, IDC_HOTKEYBOX, HKM_SETVALUE,
                                                static_cast<WPARAM>((mods << 16) | vk), 0);
                        } else {
                            SendDlgItemMessageW(dlg, IDC_HOTKEYBOX, HKM_REJECT, 0, 0);
                        }
                    }
                    return TRUE;
                case IDC_BTN_CLOSE:
                    ShowWindow(dlg, SW_HIDE);
                    return TRUE;
                case IDOK:
                    // 无默认按钮；按 Enter 不产生动作（快捷键录入由控件自行处理）
                    return TRUE;
                case IDCANCEL:
                    if (GetFocus() == GetDlgItem(dlg, IDC_HOTKEYBOX)) {
                        SendDlgItemMessageW(dlg, IDC_HOTKEYBOX, HKM_CANCEL, 0, 0);
                    } else {
                        ShowWindow(dlg, SW_HIDE);
                    }
                    return TRUE;
                default:
                    break;
            }
            return FALSE;
        }
        case WM_CLOSE:
            ShowWindow(dlg, SW_HIDE);
            return TRUE;
        case WM_DESTROY:
            g_hwnd = nullptr;
            g_app = nullptr;
            return TRUE;
        default:
            break;
    }
    return FALSE;
}

}  // namespace

void SettingsDialog::Show(HINSTANCE instance, App* app) {
    if (!g_hwnd) {
        g_hwnd = CreateDialogParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), app->MainWnd(),
                                    DialogProc, reinterpret_cast<LPARAM>(app));
        if (!g_hwnd) return;
    } else {
        g_app = app;
        RefreshControls(g_hwnd);
    }

    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
    SetFocus(GetDlgItem(g_hwnd, IDC_BTN_CLOSE));
}

HWND SettingsDialog::Hwnd() { return g_hwnd; }

void SettingsDialog::SyncHotkey(unsigned mods, unsigned vk) {
    if (!g_hwnd) return;
    SendDlgItemMessageW(g_hwnd, IDC_HOTKEYBOX, HKM_SETVALUE,
                        static_cast<WPARAM>((mods << 16) | (vk & 0xFFFFu)), 0);
}
