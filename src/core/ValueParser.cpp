#include "core/ValueParser.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace scalelogger {
namespace {
const std::regex kNumericPattern(R"(^[+-]?(?:\d+(?:\.\d*)?|\.\d+)$)");
const std::regex kSpacedSignPattern(R"(^([+-])\s+(.+)$)");
const std::regex kAutoSuffixPattern(R"(^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*([A-Za-zµμ]+)$)");

std::string Trim(const std::string& in) {
  size_t start = 0;
  while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start])) != 0) ++start;
  size_t end = in.size();
  while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1])) != 0) --end;
  return in.substr(start, end - start);
}
} // namespace

ParseResult ValueParser::Process(const std::string& raw, const ParsingSettings& settings) const {
  std::string out = raw;
  if (settings.trimWhitespace) out = Trim(out);

  if (settings.normalizeSign) {
    std::smatch m;
    if (std::regex_match(out, m, kSpacedSignPattern)) out = m[1].str() + Trim(m[2].str());
    if (!settings.preservePlusSign && !out.empty() && out[0] == '+') out.erase(0, 1);
    if (!settings.preserveMinusSign && !out.empty() && out[0] == '-') out.erase(0, 1);
  }

  if (settings.stripSuffix) {
    if (!settings.suffix.empty() && out.size() >= settings.suffix.size() &&
        out.rfind(settings.suffix) == out.size() - settings.suffix.size()) {
      out = Trim(out.substr(0, out.size() - settings.suffix.size()));
    } else {
      std::smatch m;
      if (std::regex_match(out, m, kAutoSuffixPattern)) {
        const std::string unit = m[2].str();
        if (unit == "g" || unit == "mg" || unit == "kg" || unit == "lb" || unit == "oz" || unit == "µg" || unit == "μg")
          out = m[1].str();
      }
    }
  }

  if (settings.mode == ParseMode::Raw) return { !out.empty(), raw, out, out.empty() ? "Processed raw line is empty." : "" };
  if (out.empty()) return {false, raw, out, "Parsed value is empty."};
  if (!settings.numericValidation) return {true, raw, out, ""};
  if (!std::regex_match(out, kNumericPattern)) return {false, raw, out, "Malformed numeric input."};
  return {true, raw, out, ""};
}

} // namespace scalelogger
