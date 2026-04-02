#pragma once

#include "Types.hpp"

class LineParser {
public:
    ParseResult Parse(const std::wstring& rawLine, const ParseSettings& settings) const;
};
