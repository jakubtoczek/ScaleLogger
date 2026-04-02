#include "TextInjector.hpp"

#include <windows.h>

bool TextInjector::InjectText(const std::wstring& text) {
    for (const wchar_t ch : text) {
        if (!SendUnicode(ch)) {
            return false;
        }
    }
    return true;
}

bool TextInjector::ExecutePostAction(const OutputSettings& settings) {
    switch (settings.postAction) {
        case PostAction::None:
            return true;
        case PostAction::Down:
            return SendVk(VK_DOWN);
        case PostAction::Right:
            return SendVk(VK_RIGHT);
        case PostAction::Enter:
            return SendVk(VK_RETURN);
        case PostAction::Tab:
            return SendVk(VK_TAB);
        case PostAction::Custom:
            for (wchar_t ch : settings.customSequence) {
                if (!SendUnicode(ch)) return false;
            }
            return true;
    }
    return false;
}

bool TextInjector::SendVk(WORD vk) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool TextInjector::SendUnicode(wchar_t ch) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
    inputs[0].ki.wScan = ch;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}
