#pragma once

#include <windows.h>

#include <cstdarg>
#include <cstdio>

// 调试日志：仅当设置环境变量 EASYMUTE_DEBUG=1 时写入 %TEMP%\easymute_debug.log（UTF-16，追加）
inline void DebugLog(const wchar_t* format, ...) {
    static const bool enabled = []() {
        wchar_t value[8] = {};
        return GetEnvironmentVariableW(L"EASYMUTE_DEBUG", value, 8) > 0;
    }();
    if (!enabled) return;

    wchar_t path[MAX_PATH] = {};
    if (GetTempPathW(MAX_PATH, path) == 0) return;
    lstrcatW(path, L"easymute_debug.log");

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    wchar_t message[1024] = {};
    va_list args;
    va_start(args, format);
    vswprintf(message, 1024, format, args);
    va_end(args);

    DWORD written = 0;
    WriteFile(file, message, static_cast<DWORD>(wcslen(message) * sizeof(wchar_t)), &written,
              nullptr);
    CloseHandle(file);
}
