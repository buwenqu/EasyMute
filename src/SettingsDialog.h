#pragma once

#include <windows.h>

class App;

// 设置窗口（模型化对话框，关闭仅隐藏）
namespace SettingsDialog {

// 显示（必要时创建）设置窗口
void Show(HINSTANCE instance, App* app);

// 当前设置窗口句柄（未创建时为 nullptr），主消息循环用来处理 Tab 导航
HWND Hwnd();

// 同步显示新的快捷键（例如“恢复默认”后）
void SyncHotkey(unsigned mods, unsigned vk);

}  // namespace SettingsDialog
