#include "core/KeySequence.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace scalelogger {
std::optional<std::string> NormalizeKeyToken(const std::string& token) {
  std::string t;
  for (char c : token) if (!std::isspace(static_cast<unsigned char>(c))) t.push_back(static_cast<char>(std::tolower(c)));
  static const std::unordered_set<std::string> valid = {
      "down", "right", "left", "up", "enter", "tab", "esc", "space"};
  if (valid.contains(t)) return t;
  return std::nullopt;
}

std::vector<std::string> ParseSequenceCsv(const std::string& text) {
  std::vector<std::string> out;
  std::stringstream ss(text);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (auto n = NormalizeKeyToken(item); n.has_value()) out.push_back(*n);
  }
  return out;
}

} // namespace scalelogger
