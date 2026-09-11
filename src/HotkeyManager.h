#pragma once

#include <windows.h>

// 全局热键管理：先验证新组合可用，成功后才替换旧键；失败时保留旧键
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
