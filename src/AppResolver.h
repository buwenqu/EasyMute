#pragma once

#include <windows.h>

#include <set>
#include <string>

// 当前焦点应用的完整信息
struct FocusAppInfo {
    DWORD pid = 0;
    std::wstring exeName;         // 显示名，如 chrome.exe
    std::wstring exePath;         // 可执行文件完整路径
    std::set<DWORD> relatedPids;  // 自身 + 父进程链 + 全部子进程
};

// 解析当前前台窗口对应的应用（自动处理 UWP 宿主、子进程等边界）
bool ResolveFocusApp(FocusAppInfo& out);

// 按进程 ID 解析（自检 / 调试用）
bool ResolveAppByPid(DWORD pid, FocusAppInfo& out);
