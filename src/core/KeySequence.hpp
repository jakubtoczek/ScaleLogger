#pragma once

#include <optional>
#include <string>
#include <vector>

namespace scalelogger {

std::optional<std::string> NormalizeKeyToken(const std::string& token);
std::vector<std::string> ParseSequenceCsv(const std::string& text);

} // namespace scalelogger
