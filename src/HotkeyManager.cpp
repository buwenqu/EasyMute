#include "HotkeyManager.h"

#include "DebugLog.h"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace {
// 使用 0x0000–0xBFFF 区间内的 id（窗口热键与线程热键场景下都安全）
constexpr int kHotkeyId = 0x0E45;      // 正式注册 ID
constexpr int kHotkeyIdTemp = 0x0E46;  // 临时校验 ID
}  // namespace

bool HotkeyManager::Apply(unsigned mods, unsigned vk) {
    if (!hwnd_) return false;
    DebugLog(L"[hk] Apply(mods=%u vk=%u) hwnd=%p registered=%d\n", mods, vk,
             static_cast<void*>(hwnd_), registered_ ? 1 : 0);
    if (!hwnd_) return false;
    if (registered_ && mods == mods_ && vk == vk_) return true;

    // 1. 用临时 ID 验证新组合是否可用（此时不触碰旧键）
    if (!RegisterHotKey(hwnd_, kHotkeyIdTemp, mods | MOD_NOREPEAT, vk)) {
        DebugLog(L"[hk] 步骤1 临时注册失败, err=%lu\n", GetLastError());
        return false;
    }
    DebugLog(L"[hk] 步骤1 临时注册成功\n");

    // 2. 注销旧键与临时键
    if (registered_) UnregisterHotKey(hwnd_, kHotkeyId);
    UnregisterHotKey(hwnd_, kHotkeyIdTemp);

    // 3. 以正式 ID 注册
    if (!RegisterHotKey(hwnd_, kHotkeyId, mods | MOD_NOREPEAT, vk)) {
        DebugLog(L"[hk] 步骤3 正式注册失败, err=%lu\n", GetLastError());
        // 极少量竞态失败：尽力恢复旧键
        if (registered_) RegisterHotKey(hwnd_, kHotkeyId, mods_ | MOD_NOREPEAT, vk_);
        return false;
    }
    DebugLog(L"[hk] 步骤3 正式注册成功\n");
    mods_ = mods;
    vk_ = vk;
    registered_ = true;
    return true;
}

void HotkeyManager::Unregister() {
    if (registered_ && hwnd_) {
        UnregisterHotKey(hwnd_, kHotkeyId);
    }
    registered_ = false;
}
