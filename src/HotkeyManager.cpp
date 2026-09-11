#include "HotkeyManager.h"

#include "DebugLog.h"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace {
// 使用 0x0000–0xBFFF 区间内的 id（窗口热键与线程热键场景下都安全）
constexpr int kHotkeyId = 0x0E45;  // 注册 ID
}  // namespace

bool HotkeyManager::Apply(unsigned mods, unsigned vk) {
    if (!hwnd_) return false;
    DebugLog(L"[hk] Apply(mods=%u vk=%u) hwnd=%p registered=%d\n", mods, vk,
             static_cast<void*>(hwnd_), registered_ ? 1 : 0);
    if (registered_ && mods == mods_ && vk == vk_) return true;

    // 需求（2026-09-12 变更）：不做占用检测、不向用户报告注册结果——
    // 直接尽力注册新组合；失败时静默保留旧键可用。
    const bool hadOld = registered_;
    if (hadOld) {
        UnregisterHotKey(hwnd_, kHotkeyId);
        registered_ = false;
    }
    if (!RegisterHotKey(hwnd_, kHotkeyId, mods | MOD_NOREPEAT, vk)) {
        DebugLog(L"[hk] 注册失败（静默处理）, err=%lu\n", GetLastError());
        if (hadOld && RegisterHotKey(hwnd_, kHotkeyId, mods_ | MOD_NOREPEAT, vk_)) {
            registered_ = true;
        }
        return false;
    }
    DebugLog(L"[hk] 注册成功\n");
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
