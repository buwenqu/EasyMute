#pragma once

#include <windows.h>

class App;

// 设置窗口（模型化对话框，关闭仅隐藏）
namespace SettingsDialog {

// 显示（必要时创建）设置窗口
void Show(HINSTANCE instance, App* app);

// 当前设置窗口句柄（未创建时为 nullptr），主消息循环用来处理 Tab 导航
HWND Hwnd();

// 同步显示新的快捷键（effective = 是否已实际注册生效；未生效时控件会标注“（未生效）”）
void SyncHotkey(unsigned mods, unsigned vk, bool effective);

// 录入框正在等待输入时按下了“当前快捷键”（按键会被系统热键机制拦截、控件收不到）：
// 将其解释为“设置为当前快捷键”并完成录入。返回 true 表示已处理。
bool TryCompleteCaptureWithCurrentHotkey(unsigned mods, unsigned vk);

}  // namespace SettingsDialog
