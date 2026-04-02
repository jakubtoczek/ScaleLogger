#include "LineParser.hpp"

#include <algorithm>
#include <cwctype>

namespace {
std::wstring Trim(const std::wstring& s) {
    auto start = s.begin();
    while (start != s.end() && std::iswspace(*start)) ++start;
    auto end = s.end();
    while (end != start && std::iswspace(*(end - 1))) --end;
    return std::wstring(start, end);
}

bool IsNumeric(const std::wstring& s) {
    if (s.empty()) return false;
    bool hasDigit = false;
    bool hasDot = false;
    size_t i = 0;
    if (s[0] == L'+' || s[0] == L'-') i = 1;
    for (; i < s.size(); ++i) {
        if (std::iswdigit(s[i])) {
            hasDigit = true;
            continue;
        }
        if (s[i] == L'.' && !hasDot) {
            hasDot = true;
            continue;
        }
        return false;
    }
    return hasDigit;
}
}

ParseResult LineParser::Parse(const std::wstring& rawLine, const ParseSettings& settings) const {
    ParseResult result;
    result.raw = rawLine;
    std::wstring value = rawLine;

    if (settings.trimWhitespace) {
        value = Trim(value);
    }
    if (settings.stripSuffix && !settings.suffix.empty()) {
        if (value.size() >= settings.suffix.size() && value.ends_with(settings.suffix)) {
            value = value.substr(0, value.size() - settings.suffix.size());
            if (settings.trimWhitespace) value = Trim(value);
        }
    }
    if (settings.normalizeSign) {
        value.erase(std::remove(value.begin(), value.end(), L' '), value.end());
        if (!settings.preservePlus && !value.empty() && value[0] == L'+') {
            value.erase(value.begin());
        }
        if (!settings.preserveMinus && !value.empty() && value[0] == L'-') {
            value.erase(value.begin());
        }
    }

    if (settings.requireNumeric && !IsNumeric(value)) {
        result.accepted = false;
        result.reason = L"Non-numeric line";
        result.processed = value;
        return result;
    }

    result.accepted = !value.empty();
    result.processed = value;
    if (!result.accepted) {
        result.reason = L"Empty after parsing";
    }
    return result;
}
