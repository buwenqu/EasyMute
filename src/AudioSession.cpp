#include "AudioSession.h"

#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include <vector>

#include "ComPtr.h"

namespace {

// 稳定 GUID（来源：Windows SDK 公共接口定义）。
// 这里手工声明而非依赖工具链的 IID 符号，保证 MSVC / MinGW 均可编译；
// 全部取值已与 Windows SDK（MinGW 头文件）逐一核对，并由 easymute_selftest 运行时验证。
const GUID kCLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
const GUID kIID_IMMDeviceEnumerator = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
const GUID kIID_IAudioSessionManager2 = {0x77AA99A0, 0x1BD6, 0x484F, {0x8B, 0xC7, 0x2C, 0x65, 0x4C, 0x9A, 0x9B, 0x6F}};
const GUID kIID_IAudioSessionControl2 = {0xBFB7FF88, 0x7239, 0x4FC9, {0x8F, 0xA2, 0x07, 0xC9, 0x50, 0xBE, 0x9C, 0x6D}};
const GUID kIID_ISimpleAudioVolume = {0x87CE5498, 0x68D6, 0x44E5, {0x92, 0x15, 0x6D, 0xA4, 0x7E, 0xF8, 0x83, 0xD8}};

bool PathEqualNoCase(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

std::wstring ProcessPathOf(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return L"";
    wchar_t buffer[1024] = {};
    DWORD length = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    const bool ok = QueryFullProcessImageNameW(process, 0, buffer, &length) != FALSE;
    CloseHandle(process);
    return ok ? std::wstring(buffer, length) : L"";
}

struct SessionVolume {
    ComPtr<ISimpleAudioVolume> volume;
    bool muted = false;
};

// 枚举某个端点（音频设备）上匹配目标应用的会话
HRESULT CollectEndpointSessions(IMMDevice* device,
                                const std::set<DWORD>& relatedPids,
                                const std::wstring& exePath,
                                std::vector<SessionVolume>& out) {
    ComPtr<IAudioSessionManager2> manager;
    HRESULT hr = device->Activate(kIID_IAudioSessionManager2, CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(manager.Put()));
    if (FAILED(hr)) return hr;

    ComPtr<IAudioSessionEnumerator> sessions;
    hr = manager->GetSessionEnumerator(sessions.Put());
    if (FAILED(hr)) return hr;

    int count = 0;
    hr = sessions->GetCount(&count);
    if (FAILED(hr)) return hr;

    for (int i = 0; i < count; i++) {
        ComPtr<IAudioSessionControl> control;
        if (FAILED(sessions->GetSession(i, control.Put()))) continue;

        AudioSessionState state = AudioSessionStateInactive;
        if (SUCCEEDED(control->GetState(&state)) && state == AudioSessionStateExpired) continue;

        ComPtr<IAudioSessionControl2> control2;
        if (FAILED(control->QueryInterface(kIID_IAudioSessionControl2,
                                           reinterpret_cast<void**>(control2.Put())))) {
            continue;
        }

        DWORD pid = 0;
        if (FAILED(control2->GetProcessId(&pid)) || pid == 0) continue;  // 跳过系统声音等

        bool matched = relatedPids.find(pid) != relatedPids.end();
        if (!matched && !exePath.empty()) {
            // 兜底：同可执行文件路径（覆盖浏览器等“多进程多会话”场景）
            const std::wstring sessionPath = ProcessPathOf(pid);
            matched = !sessionPath.empty() && PathEqualNoCase(sessionPath, exePath);
        }
        if (!matched) continue;

        ComPtr<ISimpleAudioVolume> volume;
        if (FAILED(control->QueryInterface(kIID_ISimpleAudioVolume,
                                           reinterpret_cast<void**>(volume.Put())))) {
            continue;
        }

        BOOL muted = FALSE;
        volume->GetMute(&muted);
        out.push_back(SessionVolume{std::move(volume), muted != FALSE});
    }
    return S_OK;
}

// 收集目标应用在所有默认渲染端点上的会话
MuteToggleResult CollectTargets(const std::set<DWORD>& relatedPids,
                                const std::wstring& exePath,
                                std::vector<SessionVolume>& targets) {
    ComPtr<IMMDeviceEnumerator> enumerator;
    const HRESULT hr = CoCreateInstance(kCLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                        kIID_IMMDeviceEnumerator,
                                        reinterpret_cast<void**>(enumerator.Put()));
    if (FAILED(hr)) return MuteToggleResult::Error;

    std::wstring seenDeviceId;
    const ERole roles[] = {eConsole, eMultimedia};
    int processedEndpoints = 0;
    int failedEndpoints = 0;
    for (const ERole role : roles) {
        ComPtr<IMMDevice> device;
        if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, role, device.Put()))) continue;

        LPWSTR rawId = nullptr;
        if (SUCCEEDED(device->GetId(&rawId)) && rawId != nullptr) {
            const std::wstring id = rawId;
            CoTaskMemFree(rawId);
            if (!seenDeviceId.empty() && id == seenDeviceId) continue;  // 同一设备避免重复
            if (seenDeviceId.empty()) seenDeviceId = id;
        }
        if (SUCCEEDED(CollectEndpointSessions(device.Get(), relatedPids, exePath, targets))) {
            processedEndpoints++;
        } else {
            failedEndpoints++;
        }
    }
    if (processedEndpoints == 0 && failedEndpoints > 0) return MuteToggleResult::Error;
    return MuteToggleResult::Ok;
}

// 收集某端点上的全部会话（不做目标匹配，供诊断使用）
HRESULT CollectAllEndpointSessions(IMMDevice* device, std::vector<SessionEntryInfo>& out) {
    ComPtr<IAudioSessionManager2> manager;
    HRESULT hr = device->Activate(kIID_IAudioSessionManager2, CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(manager.Put()));
    if (FAILED(hr)) return hr;

    ComPtr<IAudioSessionEnumerator> sessions;
    hr = manager->GetSessionEnumerator(sessions.Put());
    if (FAILED(hr)) return hr;

    int count = 0;
    hr = sessions->GetCount(&count);
    if (FAILED(hr)) return hr;

    for (int i = 0; i < count; i++) {
        ComPtr<IAudioSessionControl> control;
        if (FAILED(sessions->GetSession(i, control.Put()))) continue;

        ComPtr<IAudioSessionControl2> control2;
        if (FAILED(control->QueryInterface(kIID_IAudioSessionControl2,
                                           reinterpret_cast<void**>(control2.Put())))) {
            continue;
        }
        DWORD pid = 0;
        control2->GetProcessId(&pid);

        SessionEntryInfo entry;
        entry.pid = pid;
        entry.exePath = pid != 0 ? ProcessPathOf(pid) : L"";
        ComPtr<ISimpleAudioVolume> volume;
        if (SUCCEEDED(control->QueryInterface(kIID_ISimpleAudioVolume,
                                              reinterpret_cast<void**>(volume.Put())))) {
            BOOL muted = FALSE;
            volume->GetMute(&muted);
            entry.muted = muted != FALSE;
        }
        out.push_back(std::move(entry));
    }
    return S_OK;
}

}  // namespace

MuteToggleResult EnumerateAllSessions(std::vector<SessionEntryInfo>& out) {
    out.clear();

    ComPtr<IMMDeviceEnumerator> enumerator;
    const HRESULT hr = CoCreateInstance(kCLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                        kIID_IMMDeviceEnumerator,
                                        reinterpret_cast<void**>(enumerator.Put()));
    if (FAILED(hr)) return MuteToggleResult::Error;

    std::wstring seenDeviceId;
    const ERole roles[] = {eConsole, eMultimedia};
    int processedEndpoints = 0;
    int failedEndpoints = 0;
    for (const ERole role : roles) {
        ComPtr<IMMDevice> device;
        if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, role, device.Put()))) continue;

        LPWSTR rawId = nullptr;
        if (SUCCEEDED(device->GetId(&rawId)) && rawId != nullptr) {
            const std::wstring id = rawId;
            CoTaskMemFree(rawId);
            if (!seenDeviceId.empty() && id == seenDeviceId) continue;
            if (seenDeviceId.empty()) seenDeviceId = id;
        }
        if (SUCCEEDED(CollectAllEndpointSessions(device.Get(), out))) {
            processedEndpoints++;
        } else {
            failedEndpoints++;
        }
    }
    if (processedEndpoints == 0 && failedEndpoints > 0) return MuteToggleResult::Error;
    return MuteToggleResult::Ok;
}

MuteToggleResult QueryAppSessions(const std::set<DWORD>& relatedPids,
                                  const std::wstring& exePath,
                                  int& sessionCount,
                                  int& mutedCount) {
    sessionCount = 0;
    mutedCount = 0;

    std::vector<SessionVolume> targets;
    const MuteToggleResult result = CollectTargets(relatedPids, exePath, targets);
    if (result != MuteToggleResult::Ok) return result;

    sessionCount = static_cast<int>(targets.size());
    for (const auto& item : targets) {
        if (item.muted) mutedCount++;
    }
    return MuteToggleResult::Ok;
}

MuteToggleResult ToggleAppMute(const std::set<DWORD>& relatedPids,
                               const std::wstring& exePath,
                               bool& nowMuted) {
    std::vector<SessionVolume> targets;
    const MuteToggleResult result = CollectTargets(relatedPids, exePath, targets);
    if (result != MuteToggleResult::Ok) return result;
    if (targets.empty()) return MuteToggleResult::NoSession;

    // 规则：全部已静音 => 视为“已静音”，本次恢复；否则全部静音
    bool allMuted = true;
    for (const auto& item : targets) {
        if (!item.muted) {
            allMuted = false;
            break;
        }
    }
    const bool target = !allMuted;

    bool anyApplied = false;
    for (auto& item : targets) {
        if (SUCCEEDED(item.volume->SetMute(target, nullptr))) anyApplied = true;
    }
    if (!anyApplied) return MuteToggleResult::Error;

    nowMuted = target;
    return MuteToggleResult::Ok;
}
