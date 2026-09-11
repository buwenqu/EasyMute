#include "AppResolver.h"

#include <tlhelp32.h>

#include <deque>
#include <map>
#include <vector>

namespace {

std::wstring GetProcessPath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return L"";

    std::vector<wchar_t> buffer(1024);
    for (;;) {
        DWORD length = static_cast<DWORD>(buffer.size());
        if (QueryFullProcessImageNameW(process, 0, buffer.data(), &length)) {
            CloseHandle(process);
            return std::wstring(buffer.data(), length);
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer.size() > 32768) break;
        buffer.resize(buffer.size() * 2);
    }
    CloseHandle(process);
    return L"";
}

std::wstring FileNameOf(const std::wstring& path) {
    const size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

struct ChildSearchContext {
    DWORD hostPid = 0;
    DWORD foundPid = 0;
};

BOOL CALLBACK FindRealChildWindow(HWND child, LPARAM lParam) {
    auto* ctx = reinterpret_cast<ChildSearchContext*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(child, &pid);
    if (pid != 0 && pid != ctx->hostPid) {
        ctx->foundPid = pid;
        return FALSE;
    }
    return TRUE;
}

bool IsHostProcess(const std::wstring& exeName) {
    // UWP 应用的顶层窗口属于 ApplicationFrameHost 宿主
    return CompareStringOrdinal(exeName.c_str(), -1, L"ApplicationFrameHost.exe", -1, TRUE) == CSTR_EQUAL;
}

// 由进程 ID 构建：进程信息 + 父链 + 子进程集合
bool FillFromPid(DWORD pid, FocusAppInfo& out) {
    std::wstring path = GetProcessPath(pid);
    if (path.empty()) return false;

    out.pid = pid;
    out.exePath = path;
    out.exeName = FileNameOf(path);
    out.relatedPids.clear();
    out.relatedPids.insert(pid);

    std::map<DWORD, DWORD> parentOf;
    std::map<DWORD, std::vector<DWORD>> childrenOf;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = {};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                parentOf[entry.th32ProcessID] = entry.th32ParentProcessID;
                childrenOf[entry.th32ParentProcessID].push_back(entry.th32ProcessID);
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }

    // 父进程链
    DWORD current = pid;
    for (int depth = 0; depth < 64; depth++) {
        const auto it = parentOf.find(current);
        if (it == parentOf.end()) break;
        const DWORD parent = it->second;
        if (parent == 0 || parent == current) break;
        if (!out.relatedPids.insert(parent).second) break;
        current = parent;
    }

    // 全部子进程（BFS）
    std::deque<DWORD> queue;
    queue.push_back(pid);
    while (!queue.empty()) {
        const DWORD currentPid = queue.front();
        queue.pop_front();
        const auto it = childrenOf.find(currentPid);
        if (it == childrenOf.end()) continue;
        for (const DWORD child : it->second) {
            if (out.relatedPids.insert(child).second) queue.push_back(child);
        }
    }
    return true;
}

}  // namespace

bool ResolveFocusApp(FocusAppInfo& out) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return false;

    // UWP：前台窗口可能属于 ApplicationFrameHost 宿主，需要找到真实子窗口进程
    const std::wstring hostPath = GetProcessPath(pid);
    if (!hostPath.empty() && IsHostProcess(FileNameOf(hostPath))) {
        ChildSearchContext ctx;
        ctx.hostPid = pid;
        EnumChildWindows(hwnd, FindRealChildWindow, reinterpret_cast<LPARAM>(&ctx));
        if (ctx.foundPid != 0) {
            pid = ctx.foundPid;
        }
    }

    return FillFromPid(pid, out);
}

bool ResolveAppByPid(DWORD pid, FocusAppInfo& out) {
    if (pid == 0) return false;
    return FillFromPid(pid, out);
}
