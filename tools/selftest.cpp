// EasyMute 自检工具（控制台程序）
// 用途：命令行验证「前台窗口 → 进程解析 → 音频会话匹配 → 静音切换」核心链路。
// 用法：
//   easymute_selftest.exe            使用当前前台窗口所属应用
//   easymute_selftest.exe <pid>      使用指定进程（便于用播放无声的测试进程验证）
// 说明：会对匹配到的会话执行“切换 → 再切换”两次操作，净效果为还原原状态。

#include <windows.h>
#include <objbase.h>  // CoInitializeEx（WIN32_LEAN_AND_MEAN 下不会自动包含）

#include <fcntl.h>
#include <io.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <vector>

#include "AppResolver.h"
#include "AudioSession.h"
#include "Config.h"
#include "HotkeyName.h"

namespace {

void Print(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    vwprintf(format, args);
    va_end(args);
    fflush(stdout);
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // 模式 config：打印配置解析结果（诊断用）
    if (argc > 1 && wcscmp(argv[1], L"config") == 0) {
        Config config;
        config.Load();
        Print(L"配置文件: %ls\n", Config::FilePath().c_str());
        Print(L"hotkeyMods=%u hotkeyVk=%u（%ls）\n", config.hotkeyMods, config.hotkeyVk,
              FormatHotkey(config.hotkeyMods, config.hotkeyVk).c_str());
        Print(L"autoStart=%d showNotification=%d\n", config.autoStart ? 1 : 0,
              config.showNotification ? 1 : 0);
        CoUninitialize();
        return 0;
    }

    // 模式 list：列出默认输出设备上的全部音频会话（诊断用）
    if (argc > 1 && wcscmp(argv[1], L"list") == 0) {
        Print(L"=== EasyMute 自检（列出全部音频会话）===\n");
        std::vector<SessionEntryInfo> sessions;
        const MuteToggleResult result = EnumerateAllSessions(sessions);
        if (result == MuteToggleResult::Error) {
            Print(L"[失败] 枚举音频会话出错（COM/WASAPI 异常，或没有默认输出设备）\n");
            return 1;
        }
        Print(L"默认输出设备上的音频会话数: %d\n", static_cast<int>(sessions.size()));
        for (const auto& session : sessions) {
            Print(L"  pid=%-6u muted=%ls  %ls\n", static_cast<unsigned>(session.pid),
                  session.muted ? L"是" : L"否",
                  session.exePath.empty() ? L"(未知)" : session.exePath.c_str());
        }
        CoUninitialize();
        return 0;
    }

    Print(L"=== EasyMute 自检 ===\n");

    DWORD targetPid = 0;
    if (argc > 1) {
        targetPid = static_cast<DWORD>(wcstoul(argv[1], nullptr, 10));
    }

    FocusAppInfo info;
    bool resolved = false;
    if (targetPid != 0) {
        resolved = ResolveAppByPid(targetPid, info);
        if (!resolved) {
            Print(L"[失败] 无法解析进程 %u\n", targetPid);
            return 1;
        }
    } else {
        resolved = ResolveFocusApp(info);
        if (!resolved) {
            Print(L"[失败] 未获取到前台窗口。请先激活目标应用窗口后重试，或传入目标进程 ID。\n");
            return 1;
        }
    }

    Print(L"[1] 目标应用: %ls (pid=%u)\n", info.exeName.c_str(), info.pid);
    Print(L"    进程路径: %ls\n", info.exePath.c_str());
    Print(L"    关联进程数: %zu\n", info.relatedPids.size());

    int count = 0;
    int mutedCount = 0;
    const MuteToggleResult query = QueryAppSessions(info.relatedPids, info.exePath, count, mutedCount);
    if (query == MuteToggleResult::Error) {
        Print(L"[失败] 枚举音频会话出错（COM/WASAPI 异常）\n");
        return 1;
    }
    if (count == 0) {
        Print(L"[2] 未找到该应用的音频会话（请让该应用播放声音后重试）\n");
        return 0;
    }
    Print(L"[2] 匹配音频会话: %d 个（其中已静音 %d 个）\n", count, mutedCount);

    bool nowMuted = false;
    if (ToggleAppMute(info.relatedPids, info.exePath, nowMuted) != MuteToggleResult::Ok) {
        Print(L"[失败] 静音切换失败\n");
        return 1;
    }
    Print(L"[3] 第一次切换后: %ls\n", nowMuted ? L"已静音" : L"已恢复");

    if (ToggleAppMute(info.relatedPids, info.exePath, nowMuted) != MuteToggleResult::Ok) {
        Print(L"[失败] 二次切换失败（状态可能未还原，请检查音量合成器）\n");
        return 1;
    }
    Print(L"[4] 二次切换（还原）后: %ls\n", nowMuted ? L"已静音" : L"已恢复");
    Print(L"自检完成。\n");

    CoUninitialize();
    return 0;
}
