#pragma once

#include <windows.h>

// 全局热键管理：尽力注册用户选择的组合；注册失败时静默保留旧键
// （需求 2026-09-12：不做占用检测、不向用户报告注册结果）
class HotkeyManager {
public:
    ~HotkeyManager() { Unregister(); }

    void Attach(HWND hwnd) { hwnd_ = hwnd; }

    bool Apply(unsigned mods, unsigned vk);
    void Unregister();

    bool Has() const { return registered_; }
    unsigned Modifiers() const { return mods_; }
    unsigned Key() const { return vk_; }

private:
    HWND hwnd_ = nullptr;
    bool registered_ = false;
    unsigned mods_ = 0;
    unsigned vk_ = 0;
};
