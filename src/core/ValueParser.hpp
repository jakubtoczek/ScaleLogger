#pragma once

#include "core/Types.hpp"

#include <string>

namespace scalelogger {

struct ParseResult {
  bool ok{false};
  std::string raw;
  std::string processed;
  std::string message;
};

class ValueParser {
 public:
  ParseResult Process(const std::string& raw, const ParsingSettings& settings) const;
};

} // namespace scalelogger
