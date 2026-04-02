#pragma once

#include <string>

#include "Types.hpp"

class TextInjector {
public:
    bool InjectText(const std::wstring& text);
    bool ExecutePostAction(const OutputSettings& settings);

private:
    bool SendVk(WORD vk);
    bool SendUnicode(wchar_t ch);
};
