#include "HotkeyName.h"

#include <cstdio>

std::wstring VkDisplayName(unsigned vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        wchar_t buf[8] = {};
        swprintf(buf, 8, L"F%u", vk - VK_F1 + 1);
        return buf;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return L"小键盘 " + std::wstring(1, static_cast<wchar_t>(L'0' + (vk - VK_NUMPAD0)));
    }
    switch (vk) {
        case VK_SPACE:    return L"空格";
        case VK_RETURN:   return L"回车";
        case VK_TAB:      return L"Tab";
        case VK_ESCAPE:   return L"Esc";
        case VK_BACK:     return L"退格";
        case VK_DELETE:   return L"Delete";
        case VK_INSERT:   return L"Insert";
        case VK_HOME:     return L"Home";
        case VK_END:      return L"End";
        case VK_PRIOR:    return L"PageUp";
        case VK_NEXT:     return L"PageDown";
        case VK_LEFT:     return L"左方向键";
        case VK_RIGHT:    return L"右方向键";
        case VK_UP:       return L"上方向键";
        case VK_DOWN:     return L"下方向键";
        case VK_PAUSE:    return L"Pause";
        case VK_SCROLL:   return L"ScrollLock";
        case VK_CAPITAL:  return L"CapsLock";
        case VK_SNAPSHOT: return L"PrintScreen";
        case VK_OEM_1:    return L";";
        case VK_OEM_PLUS: return L"=";
        case VK_OEM_COMMA: return L",";
        case VK_OEM_MINUS: return L"-";
        case VK_OEM_PERIOD: return L".";
        case VK_OEM_2:    return L"/";
        case VK_OEM_3:    return L"`";
        case VK_OEM_4:    return L"[";
        case VK_OEM_5:    return L"\\";
        case VK_OEM_6:    return L"]";
        case VK_OEM_7:    return L"'";
        case VK_ADD:      return L"小键盘 +";
        case VK_SUBTRACT: return L"小键盘 -";
        case VK_MULTIPLY: return L"小键盘 *";
        case VK_DIVIDE:   return L"小键盘 /";
        case VK_DECIMAL:  return L"小键盘 .";
        default:          break;
    }
    wchar_t buf[32] = {};
    swprintf(buf, 32, L"键 0x%02X", vk);
    return buf;
}

std::wstring FormatHotkey(unsigned mods, unsigned vk) {
    std::wstring text;
    if (mods & MOD_CONTROL) text += L"Ctrl + ";
    if (mods & MOD_ALT)     text += L"Alt + ";
    if (mods & MOD_SHIFT)   text += L"Shift + ";
    if (mods & MOD_WIN)     text += L"Win + ";
    text += VkDisplayName(vk);
    return text;
}

bool IsModifierKey(unsigned vk) {
    switch (vk) {
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
        case VK_MENU:    case VK_LMENU:    case VK_RMENU:
        case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:
        case VK_LWIN:    case VK_RWIN:
            return true;
        default:
            return false;
    }
}

bool IsSingleKeyAllowed(unsigned vk) {
    if (vk >= VK_F1 && vk <= VK_F24) return true;
    return vk == VK_PAUSE || vk == VK_SCROLL;
}

bool ValidateHotkey(unsigned mods, unsigned vk, std::wstring* error) {
    if (vk == 0) {
        if (error) *error = L"请按下一个按键";
        return false;
    }
    if (IsModifierKey(vk)) {
        if (error) *error = L"请继续按下主键";
        return false;
    }
    if (vk == VK_ESCAPE || vk == VK_BACK) {
        if (error) *error = L"该按键为保留键";  // 用于取消 / 清除
        return false;
    }
    if ((mods & ~(MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN)) != 0) {
        if (error) *error = L"不支持的修饰键";
        return false;
    }
    if (mods == 0 && !IsSingleKeyAllowed(vk)) {
        if (error) *error = L"该按键不能单独使用，请配合 Ctrl / Alt / Shift / Win";
        return false;
    }
    return true;
}
