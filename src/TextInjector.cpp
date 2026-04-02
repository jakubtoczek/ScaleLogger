#include "TextInjector.hpp"

#include <sstream>
#include <vector>

#include <windows.h>

namespace {
std::vector<std::wstring> Tokenize(const std::wstring& sequence) {
    std::vector<std::wstring> tokens;
    std::wstringstream ss(sequence);
    std::wstring token;
    while (std::getline(ss, token, L',')) {
        std::wstringstream inner(token);
        std::wstring part;
        while (inner >> part) {
            tokens.push_back(part);
        }
    }
    return tokens;
}
}

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
            for (const auto& token : Tokenize(settings.customSequence)) {
                if (token == L"DOWN") { if (!SendVk(VK_DOWN)) return false; continue; }
                if (token == L"UP") { if (!SendVk(VK_UP)) return false; continue; }
                if (token == L"LEFT") { if (!SendVk(VK_LEFT)) return false; continue; }
                if (token == L"RIGHT") { if (!SendVk(VK_RIGHT)) return false; continue; }
                if (token == L"ENTER") { if (!SendVk(VK_RETURN)) return false; continue; }
                if (token == L"TAB") { if (!SendVk(VK_TAB)) return false; continue; }
                if (token == L"F1") { if (!SendVk(VK_F1)) return false; continue; }
                if (token == L"F2") { if (!SendVk(VK_F2)) return false; continue; }
                if (token == L"F3") { if (!SendVk(VK_F3)) return false; continue; }
                if (token == L"F4") { if (!SendVk(VK_F4)) return false; continue; }
                if (token == L"F5") { if (!SendVk(VK_F5)) return false; continue; }
                if (token == L"F6") { if (!SendVk(VK_F6)) return false; continue; }
                if (token == L"F7") { if (!SendVk(VK_F7)) return false; continue; }
                if (token == L"F8") { if (!SendVk(VK_F8)) return false; continue; }
                if (token == L"F9") { if (!SendVk(VK_F9)) return false; continue; }
                if (token == L"F10") { if (!SendVk(VK_F10)) return false; continue; }
                if (token == L"F11") { if (!SendVk(VK_F11)) return false; continue; }
                if (token == L"F12") { if (!SendVk(VK_F12)) return false; continue; }
                if (token.rfind(L"TEXT:", 0) == 0) {
                    for (wchar_t ch : token.substr(5)) {
                        if (!SendUnicode(ch)) return false;
                    }
                    continue;
                }
                return false;
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
