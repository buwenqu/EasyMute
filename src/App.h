#pragma once

#include <windows.h>

#include <string>

#include "Config.h"
#include "HotkeyManager.h"
#include "TrayIcon.h"

// 主窗口类名（单实例查找用）
constexpr wchar_t kMainWindowClass[] = L"EasyMuteWindow";
// 单实例唤起“打开设置”消息名
constexpr wchar_t kOpenSettingsMessage[] = L"EasyMuteOpenSettings";

class App {
public:
    static App* Instance();

    bool Init(HINSTANCE instance);
    int Run();

    HWND MainWnd() const { return hwnd_; }
    const Config& GetConfig() const { return config_; }

    // ---- 供设置窗口调用 ----
    bool TryApplyHotkey(unsigned mods, unsigned vk, bool persist);
    void ApplyDefaultHotkey();
    void ApplyAutoStart(bool enabled);
    void ApplyNotification(bool enabled);

    void OpenSettings();
    void ToggleForegroundAppMute();
    void Notify(const std::wstring& text);  // 仅“静音状态真正切换”时调用，受设置开关控制

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void OnTrayMessage(LPARAM lp);
    void ShowTrayMenu();
    void UpdateTrayTip();
    void ExitApp();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    Config config_;
    HotkeyManager hotkey_;
    TrayIcon tray_;
    UINT taskbarCreatedMsg_ = 0;
    UINT openSettingsMsg_ = 0;
};
