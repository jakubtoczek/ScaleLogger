#pragma once

#include "core/Types.hpp"

#include <string>

namespace scalelogger {

class InputInjector {
 public:
  enum class SendStatus { Success, TextFailed, PostActionFailed };
  struct SendResult {
    SendStatus status{SendStatus::Success};
    std::string failedToken{};
  };

  SendResult SendTextAndAction(const std::wstring& text, const OutputSettings& output);
};

} // namespace scalelogger
