#pragma once

#include <windows.h>

#include <string>

// 默认快捷键：Ctrl + Alt + M
constexpr unsigned kDefaultHotkeyMods = MOD_CONTROL | MOD_ALT;
constexpr unsigned kDefaultHotkeyKey = 'M';

// 将 (mods, vk) 格式化为可读文本，如 "Ctrl + Alt + M"
std::wstring FormatHotkey(unsigned mods, unsigned vk);

// 按键显示名，如 "M"、"F9"、"空格"
std::wstring VkDisplayName(unsigned vk);

// 是否为修饰键（Ctrl / Alt / Shift / Win）
bool IsModifierKey(unsigned vk);

// 单键（无修饰键）是否允许：仅 F1–F24、Pause、ScrollLock
bool IsSingleKeyAllowed(unsigned vk);

// 校验组合是否合法，不合法时通过 error 返回原因
bool ValidateHotkey(unsigned mods, unsigned vk, std::wstring* error);
