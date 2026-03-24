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
      "down", "right", "left", "up", "enter", "tab", "esc", "space", "backspace", "delete", "home", "end", "pageup", "pagedown"};
  if (valid.contains(t)) return t;
  if (t.size() == 1 && std::isalnum(static_cast<unsigned char>(t[0])) != 0) return t;
  if (t.size() == 2 && t[0] == 'f' && t[1] >= '1' && t[1] <= '9') return t;
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
