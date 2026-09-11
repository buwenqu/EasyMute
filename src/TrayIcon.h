#pragma once

#include <windows.h>
#include <shellapi.h>  // NOTIFYICONDATA / Shell_NotifyIcon（WIN32_LEAN_AND_MEAN 下不会自动包含）

#include <string>

// 托盘图标回调消息（lParam 低 16 位为鼠标消息）
constexpr UINT kTrayCallbackMessage = WM_APP + 1;

class TrayIcon {
public:
    ~TrayIcon() { Remove(); }

    bool Add(HWND owner, HICON icon, const std::wstring& tip);
    void Remove();
    void ReAdd();  // 资源管理器重启后恢复
    void SetTip(const std::wstring& tip);
    void ShowBalloon(const std::wstring& title, const std::wstring& text, bool isError);

private:
    NOTIFYICONDATAW data_ = {};
    bool added_ = false;
    HICON icon_ = nullptr;
};
