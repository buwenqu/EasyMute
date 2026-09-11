#include "HotkeyBox.h"

#include <string>

#include "HotkeyName.h"

namespace {

constexpr wchar_t kBoxClass[] = L"EasyMuteHotkeyBox";

struct BoxData {
    unsigned mods = 0;  // 当前显示的快捷键
    unsigned vk = 0;
    unsigned pendingMods = 0;  // 刚捕获、等待父窗口确认的候选组合
    unsigned pendingVk = 0;
    bool hasPending = false;
    bool capturing = false;
    HFONT font = nullptr;
    std::wstring hint;  // 录入过程中的提示文字（占位 / 错误）
    bool hintError = false;
};

BoxData* DataOf(HWND hwnd) {
    return reinterpret_cast<BoxData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

unsigned CurrentMods() {
    unsigned mods = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= MOD_WIN;
    return mods;
}

void BeginCapture(HWND hwnd, BoxData* data) {
    data->capturing = true;
    data->hint = L"请按下新快捷键…";
    data->hintError = false;
    InvalidateRect(hwnd, nullptr, TRUE);
}

void EndCapture(HWND hwnd, BoxData* data) {
    data->capturing = false;
    data->hasPending = false;  // 放弃未确认的候选值
    data->hint.clear();
    data->hintError = false;
    InvalidateRect(hwnd, nullptr, TRUE);
}

void PaintBox(HWND hwnd, BoxData* data) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);

    RECT rc = {};
    GetClientRect(hwnd, &rc);

    const bool focused = GetFocus() == hwnd;
    HBRUSH background = CreateSolidBrush(focused ? RGB(0xEA, 0xF3, 0xFF) : RGB(0xFF, 0xFF, 0xFF));
    FillRect(dc, &rc, background);
    DeleteObject(background);

    HBRUSH frame = CreateSolidBrush(focused ? RGB(0x2B, 0x5F, 0xE0) : RGB(0xB8, 0xB8, 0xB8));
    FrameRect(dc, &rc, frame);
    DeleteObject(frame);

    std::wstring text;
    COLORREF color = RGB(0x1F, 0x1F, 0x1F);
    if (data->capturing) {
        color = data->hintError ? RGB(0xD9, 0x30, 0x25) : RGB(0x8A, 0x8A, 0x8A);
        text = data->hint.empty() ? L"请按下新快捷键…" : data->hint;
    } else {
        // 需求（2026-09-12 变更）：只如实展示用户设置的组合，不标注注册结果
        text = FormatHotkey(data->mods, data->vk);
    }

    HFONT oldFont = nullptr;
    if (data->font) oldFont = static_cast<HFONT>(SelectObject(dc, data->font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    RECT textRect = rc;
    textRect.left += 6;
    textRect.right -= 6;
    DrawTextW(dc, text.c_str(), -1, &textRect,
              DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    if (oldFont) SelectObject(dc, oldFont);

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK BoxProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    BoxData* data = DataOf(hwnd);
    switch (msg) {
        case WM_NCCREATE: {
            auto* created = new BoxData();
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_NCDESTROY: {
            delete data;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_SETFONT:
            if (data) data->font = reinterpret_cast<HFONT>(wp);
            return 0;
        case WM_GETDLGCODE:
            // 捕获所有按键（含 Enter），Tab 导航在 WM_KEYDOWN 中手工转发
            return DLGC_WANTALLKEYS;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            if (data) PaintBox(hwnd, data);
            return 0;
        case WM_SETFOCUS:
            if (data) BeginCapture(hwnd, data);
            return 0;
        case WM_KILLFOCUS:
            if (data) EndCapture(hwnd, data);
            return 0;
        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            if (data) BeginCapture(hwnd, data);
            return 0;
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_DEADCHAR:
            return 0;
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN: {
            if (!data) return 0;
            const unsigned vk = static_cast<unsigned>(wp);
            if (vk == VK_PROCESSKEY) return 0;   // IME 处理
            if (vk == VK_TAB) {
                // 手工转发 Tab / Shift+Tab，保持对话框导航能力
                SendMessageW(GetParent(hwnd), WM_NEXTDLGCTL,
                             (GetKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE, FALSE);
                return 0;
            }
            if (IsModifierKey(vk)) return 0;     // 等待主键
            const unsigned mods = CurrentMods();
            if (vk == VK_ESCAPE) {
                EndCapture(hwnd, data);
                return 0;
            }
            if (vk == VK_BACK) {
                data->capturing = true;
                data->hasPending = false;
                data->hint = L"请按下新快捷键…";
                data->hintError = false;
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            std::wstring error;
            if (!ValidateHotkey(mods, vk, &error)) {
                data->capturing = true;
                data->hint = error;
                data->hintError = true;
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            // 关键：必须先把候选组合存入控件，再通知父窗口；
            // 父窗口通过 HKM_GETVALUE 读到的就是本次捕获的新组合。
            // （曾因漏掉这一步导致"改键永远提示被占用"）
            data->pendingMods = mods;
            data->pendingVk = vk;
            data->hasPending = true;
            SendMessageW(GetParent(hwnd), WM_COMMAND,
                         MAKEWPARAM(static_cast<WORD>(GetDlgCtrlID(hwnd)), HKN_CHANGED),
                         reinterpret_cast<LPARAM>(hwnd));
            return 0;
        }
        case HKM_SETVALUE: {
            if (!data) return 0;
            data->mods = (static_cast<unsigned>(wp) >> 16) & 0xFFFFu;
            data->vk = static_cast<unsigned>(wp) & 0xFFFFu;
            data->hasPending = false;
            data->capturing = false;
            data->hint.clear();
            data->hintError = false;
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        case HKM_GETVALUE: {
            if (!data) return 0;
            const unsigned mods = data->hasPending ? data->pendingMods : data->mods;
            const unsigned vk = data->hasPending ? data->pendingVk : data->vk;
            return static_cast<LRESULT>((mods << 16) | (vk & 0xFFFFu));
        }
        case HKM_ISCAPTURING: {
            if (!data) return 0;
            return data->capturing ? 1 : 0;
        }
        case HKM_REJECT: {
            if (!data) return 0;
            data->capturing = true;
            data->hint = L"该快捷键被占用，请更换";
            data->hintError = true;
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        case HKM_CANCEL: {
            if (data) EndCapture(hwnd, data);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

}  // namespace

bool RegisterHotkeyBoxClass(HINSTANCE instance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = BoxProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kBoxClass;
    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
