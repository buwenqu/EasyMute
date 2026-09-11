// EasyMute 静音测试桩（控制台程序）
// 作用：在默认输出设备上持续渲染“静音”数据，制造一个真实的音频会话，
//       用于端到端验证 EasyMute 的「进程匹配 → 会话静音」核心链路。
// 用法：easymute_soundstub.exe          一直渲染（Ctrl+C 退出）
//       easymute_soundstub.exe <秒数>  渲染指定秒数后自动退出

#include <windows.h>
#include <objbase.h>

#include <audioclient.h>
#include <mmdeviceapi.h>

#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <cstdlib>

#include "ComPtr.h"

namespace {

volatile LONG g_stop = 0;

BOOL WINAPI ConsoleHandler(DWORD type) {
    switch (type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            InterlockedExchange(&g_stop, 1);
            return TRUE;
        default:
            return FALSE;
    }
}

// 测试桩自带一个可见窗口：便于把它设为“前台应用”，做真实按键的端到端验证
constexpr wchar_t kStubWindowClass[] = L"EasyMuteSoundStubWnd";

LRESULT CALLBACK StubWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            InterlockedExchange(&g_stop, 1);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void PumpMessages() {
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);  // 输出重定向到文件时也写 UTF-8
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    // 创建可见窗口（供端到端测试设为前台应用）
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = StubWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kStubWindowClass;
    RegisterClassExW(&wc);
    HWND stubWindow = CreateWindowExW(0, kStubWindowClass, L"EasyMute SoundStub",
                                      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 360,
                                      140, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (stubWindow != nullptr) ShowWindow(stubWindow, SW_SHOW);
    wprintf(L"[soundstub] 窗口句柄: %p\n", static_cast<void*>(stubWindow));
    fflush(stdout);

    int seconds = 0;
    if (argc > 1) seconds = _wtoi(argv[1]);

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(enumerator.Put()));
    if (FAILED(hr)) {
        wprintf(L"[soundstub] CoCreateInstance(MMDeviceEnumerator) 失败: 0x%08X\n",
                static_cast<unsigned>(hr));
        return 1;
    }

    ComPtr<IMMDevice> device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device.Put());
    if (FAILED(hr)) {
        wprintf(L"[soundstub] GetDefaultAudioEndpoint 失败: 0x%08X（没有可用的音频输出设备？）\n",
                static_cast<unsigned>(hr));
        return 1;
    }

    ComPtr<IAudioClient> client;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(client.Put()));
    if (FAILED(hr)) {
        wprintf(L"[soundstub] Activate(IAudioClient) 失败: 0x%08X\n", static_cast<unsigned>(hr));
        return 1;
    }

    WAVEFORMATEX* mixFormat = nullptr;
    hr = client->GetMixFormat(&mixFormat);
    if (FAILED(hr)) {
        wprintf(L"[soundstub] GetMixFormat 失败: 0x%08X\n", static_cast<unsigned>(hr));
        return 1;
    }

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, mixFormat, nullptr);
    if (FAILED(hr)) {
        wprintf(L"[soundstub] IAudioClient::Initialize 失败: 0x%08X\n", static_cast<unsigned>(hr));
        CoTaskMemFree(mixFormat);
        return 1;
    }

    ComPtr<IAudioRenderClient> render;
    hr = client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(render.Put()));
    if (FAILED(hr)) {
        wprintf(L"[soundstub] GetService(IAudioRenderClient) 失败: 0x%08X\n", static_cast<unsigned>(hr));
        CoTaskMemFree(mixFormat);
        return 1;
    }

    UINT32 bufferFrames = 0;
    client->GetBufferSize(&bufferFrames);

    hr = client->Start();
    if (FAILED(hr)) {
        wprintf(L"[soundstub] IAudioClient::Start 失败: 0x%08X\n", static_cast<unsigned>(hr));
        CoTaskMemFree(mixFormat);
        return 1;
    }

    wprintf(L"[soundstub] 开始渲染静音。pid=%u, buffer=%u frames\n", GetCurrentProcessId(),
            bufferFrames);
    fflush(stdout);

    const ULONGLONG startTick = GetTickCount64();
    while (g_stop == 0) {
        PumpMessages();
        if (seconds > 0 &&
            GetTickCount64() - startTick >= static_cast<ULONGLONG>(seconds) * 1000ULL) {
            break;
        }
        UINT32 padding = 0;
        if (FAILED(client->GetCurrentPadding(&padding))) break;
        UINT32 frames = bufferFrames - padding;
        if (frames > bufferFrames / 2) frames = bufferFrames / 2;
        if (frames > 0) {
            BYTE* buffer = nullptr;
            if (SUCCEEDED(render->GetBuffer(frames, &buffer))) {
                render->ReleaseBuffer(frames, AUDCLNT_BUFFERFLAGS_SILENT);
            }
        }
        Sleep(50);
    }

    client->Stop();
    CoTaskMemFree(mixFormat);
    CoUninitialize();
    wprintf(L"[soundstub] 已退出。\n");
    return 0;
}
