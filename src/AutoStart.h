#pragma once

// 开机自启：写入 / 删除 HKCU\Software\Microsoft\Windows\CurrentVersion\Run
namespace AutoStart {

// 当前启动项是否已指向本程序
bool IsEnabled();

// 启用 / 禁用开机自启（启用时会写入当前 exe 路径）
bool Set(bool enabled);

}  // namespace AutoStart
