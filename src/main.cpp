// EasyMute - 常驻系统托盘，一键静音当前焦点应用
#include <windows.h>
#include <objbase.h>  // CoInitializeEx（WIN32_LEAN_AND_MEAN 下不会自动包含）

#include "App.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // 单实例：已有实例时唤起其设置窗口后退出
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"EasyMute.SingleInstance");
    if (mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kMainWindowClass, nullptr);
        if (existing != nullptr) {
            const UINT message = RegisterWindowMessageW(kOpenSettingsMessage);
            if (message != 0) PostMessageW(existing, message, 0, 0);
        }
        return 0;
    }

    App app;
    if (!app.Init(instance)) {
        MessageBoxW(nullptr, L"EasyMute 初始化失败，程序即将退出。", L"EasyMute", MB_ICONERROR);
        return 1;
    }

    const int code = app.Run();
    CoUninitialize();
    if (mutex != nullptr) CloseHandle(mutex);
    return code;
}
