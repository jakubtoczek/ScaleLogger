#pragma once

#include "core/Types.hpp"

#include <string>

namespace scalelogger {

class InputInjector {
 public:
  bool SendTextAndAction(const std::wstring& text, const OutputSettings& output);
};

} // namespace scalelogger
