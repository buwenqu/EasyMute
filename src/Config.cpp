#include "Config.h"

#include <windows.h>

#include <cstdio>

namespace {
constexpr wchar_t kSectionHotkey[] = L"Hotkey";
constexpr wchar_t kSectionGeneral[] = L"General";

std::wstring GetAppDataDir() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return L".";
    return buf;
}
}  // namespace

std::wstring Config::FilePath() {
    const std::wstring dir = GetAppDataDir() + L"\\EasyMute";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\config.ini";
}

bool Config::Load() {
    const std::wstring path = FilePath();
    hotkeyMods = GetPrivateProfileIntW(kSectionHotkey, L"Modifiers", kDefaultHotkeyMods, path.c_str());
    hotkeyVk = GetPrivateProfileIntW(kSectionHotkey, L"Key", kDefaultHotkeyKey, path.c_str());
    autoStart = GetPrivateProfileIntW(kSectionGeneral, L"AutoStart", 0, path.c_str()) != 0;
    showNotification = GetPrivateProfileIntW(kSectionGeneral, L"ShowNotification", 1, path.c_str()) != 0;

    // 配置损坏时回退默认值
    std::wstring error;
    if (!ValidateHotkey(hotkeyMods, hotkeyVk, &error)) {
        hotkeyMods = kDefaultHotkeyMods;
        hotkeyVk = kDefaultHotkeyKey;
    }
    return true;
}

bool Config::Save() const {
    const std::wstring path = FilePath();
    wchar_t buf[32] = {};

    swprintf(buf, 32, L"%u", hotkeyMods);
    WritePrivateProfileStringW(kSectionHotkey, L"Modifiers", buf, path.c_str());
    swprintf(buf, 32, L"%u", hotkeyVk);
    WritePrivateProfileStringW(kSectionHotkey, L"Key", buf, path.c_str());

    WritePrivateProfileStringW(kSectionGeneral, L"AutoStart", autoStart ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(kSectionGeneral, L"ShowNotification", showNotification ? L"1" : L"0", path.c_str());
    return true;
}
