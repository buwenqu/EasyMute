#pragma once

#include <windows.h>

#include <set>
#include <string>
#include <vector>

enum class MuteToggleResult {
    Ok,         // 找到会话并已切换
    NoSession,  // 未找到匹配该应用的音频会话
    Error,      // 系统 / COM 错误
};

// 将属于目标应用的全部音频会话静音状态取反。
// relatedPids：焦点进程自身 + 父进程链 + 子进程
// exePath：焦点进程可执行文件完整路径（用于匹配多进程应用的兄弟进程）
// nowMuted：切换后的状态
MuteToggleResult ToggleAppMute(const std::set<DWORD>& relatedPids,
                               const std::wstring& exePath,
                               bool& nowMuted);

// 仅统计匹配的音频会话数量（不改变任何状态）
MuteToggleResult QueryAppSessions(const std::set<DWORD>& relatedPids,
                                  const std::wstring& exePath,
                                  int& sessionCount,
                                  int& mutedCount);

// 单个音频会话的信息（诊断 / 自检用）
struct SessionEntryInfo {
    DWORD pid = 0;
    bool muted = false;
    std::wstring exePath;  // 能解析时填充
};

// 枚举默认渲染端点上的全部音频会话（不改变任何状态）
MuteToggleResult EnumerateAllSessions(std::vector<SessionEntryInfo>& out);
