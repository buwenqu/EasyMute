#include "TrayIcon.h"

namespace {
constexpr UINT kIconId = 1;
}  // namespace

bool TrayIcon::Add(HWND owner, HICON icon, const std::wstring& tip) {
    icon_ = icon;
    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = kIconId;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data_.uCallbackMessage = kTrayCallbackMessage;
    data_.hIcon = icon_;
    lstrcpynW(data_.szTip, tip.c_str(), ARRAYSIZE(data_.szTip));
    added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    return added_;
}

void TrayIcon::Remove() {
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
}

void TrayIcon::ReAdd() {
    if (!added_) {
        data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    } else {
        Shell_NotifyIconW(NIM_MODIFY, &data_);
    }
}

void TrayIcon::SetTip(const std::wstring& tip) {
    if (!added_) return;
    lstrcpynW(data_.szTip, tip.c_str(), ARRAYSIZE(data_.szTip));
    data_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &data_);
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& text, bool isError) {
    if (!added_) return;
    lstrcpynW(data_.szInfoTitle, title.c_str(), ARRAYSIZE(data_.szInfoTitle));
    lstrcpynW(data_.szInfo, text.c_str(), ARRAYSIZE(data_.szInfo));
    data_.dwInfoFlags = NIIF_NOSOUND | (isError ? NIIF_WARNING : NIIF_INFO);
    data_.uTimeout = 4000;
    data_.uFlags = NIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &data_);
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}
