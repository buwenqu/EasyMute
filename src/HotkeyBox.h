#pragma once

#include <windows.h>

// 自定义控件：快捷键录入框
// 在对话框资源中使用类名 "EasyMuteHotkeyBox"（见 res/EasyMute.rc）

// 注册控件窗口类（进程内调用一次）
bool RegisterHotkeyBoxClass(HINSTANCE instance);

// ---- 控件消息（父窗口 → 控件） ----
constexpr UINT HKM_GETVALUE = WM_APP + 20;  // 返回 (mods << 16) | vk
constexpr UINT HKM_SETVALUE = WM_APP + 21;  // wParam = (mods << 16) | vk，直接显示并结束录入
constexpr UINT HKM_REJECT   = WM_APP + 22;  // 组合被占用：显示提示并继续等待输入
constexpr UINT HKM_CANCEL   = WM_APP + 23;  // 取消本次录入，恢复显示

// ---- 控件通知（控件 → 父窗口 WM_COMMAND 的 HIWORD） ----
constexpr WORD HKN_CHANGED = 0x100;  // 用户按下了合法组合，父窗口应查询并尝试注册
