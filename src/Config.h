#pragma once

#include <string>

#include "HotkeyName.h"

// 用户配置（%APPDATA%\EasyMute\config.ini）
class Config {
public:
    unsigned hotkeyMods = kDefaultHotkeyMods;  // MOD_* 位组合
    unsigned hotkeyVk = kDefaultHotkeyKey;
    bool autoStart = false;
    bool showNotification = true;

    bool Load();
    bool Save() const;

    static std::wstring FilePath();
};
