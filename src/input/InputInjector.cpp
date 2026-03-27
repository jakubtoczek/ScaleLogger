#include "input/InputInjector.hpp"

#include "core/KeySequence.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {
#ifdef _WIN32
namespace {
bool SendVirtualKey(WORD vk) {
  INPUT in[2]{};
  in[0].type = INPUT_KEYBOARD;
  in[0].ki.wVk = vk;
  if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR ||
      vk == VK_NEXT || vk == VK_INSERT || vk == VK_DELETE) {
    in[0].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  in[1] = in[0];
  in[1].ki.dwFlags |= KEYEVENTF_KEYUP;
  return SendInput(2, in, sizeof(INPUT)) == 2;
}

WORD TokenToVk(const std::string& token) {
  if (token == "down") return VK_DOWN;
  if (token == "right") return VK_RIGHT;
  if (token == "left") return VK_LEFT;
  if (token == "up") return VK_UP;
  if (token == "enter") return VK_RETURN;
  if (token == "tab") return VK_TAB;
  if (token == "esc") return VK_ESCAPE;
  if (token == "space") return VK_SPACE;
  if (token == "backspace") return VK_BACK;
  if (token == "delete") return VK_DELETE;
  if (token == "home") return VK_HOME;
  if (token == "end") return VK_END;
  if (token == "pageup") return VK_PRIOR;
  if (token == "pagedown") return VK_NEXT;
  if (token.rfind("num", 0) == 0 && token.size() == 4 && token[3] >= '0' && token[3] <= '9') return static_cast<WORD>(VK_NUMPAD0 + (token[3] - '0'));
  if (token.size() == 1 && token[0] >= '0' && token[0] <= '9') return static_cast<WORD>(token[0]);
  if (token.size() == 1 && token[0] >= 'a' && token[0] <= 'z') return static_cast<WORD>(token[0] - 32);
  if (token == "f10") return VK_F10;
  if (token == "f11") return VK_F11;
  if (token == "f12") return VK_F12;
  if (token.size() == 2 && token[0] == 'f' && token[1] >= '1' && token[1] <= '9') return static_cast<WORD>(VK_F1 + (token[1] - '1'));
  return 0;
}

InputInjector::SendResult SendPostAction(const OutputSettings& output) {
  auto sendToken = [](const std::string& token) -> InputInjector::SendResult {
    const auto normalized = NormalizeKeyToken(token);
    if (!normalized.has_value()) return {InputInjector::SendStatus::PostActionFailed, token};
    const WORD vk = TokenToVk(*normalized);
    if (vk == 0 || !SendVirtualKey(vk)) return {InputInjector::SendStatus::PostActionFailed, token};
    return {InputInjector::SendStatus::Success, {}};
  };

  switch (output.postAction) {
    case PostAction::Down: return SendVirtualKey(VK_DOWN) ? InputInjector::SendResult{} : InputInjector::SendResult{InputInjector::SendStatus::PostActionFailed, "down"};
    case PostAction::Right:
      return SendVirtualKey(VK_RIGHT) ? InputInjector::SendResult{} : InputInjector::SendResult{InputInjector::SendStatus::PostActionFailed, "right"};
    case PostAction::Enter:
      return SendVirtualKey(VK_RETURN) ? InputInjector::SendResult{} : InputInjector::SendResult{InputInjector::SendStatus::PostActionFailed, "enter"};
    case PostAction::Tab: return SendVirtualKey(VK_TAB) ? InputInjector::SendResult{} : InputInjector::SendResult{InputInjector::SendStatus::PostActionFailed, "tab"};
    case PostAction::None: return {InputInjector::SendStatus::Success, {}};
    case PostAction::CustomSequence:
      if (output.customSequence.empty()) return {InputInjector::SendStatus::Success, {}};
      for (const auto& token : output.customSequence) {
        const auto tokenResult = sendToken(token);
        if (tokenResult.status != InputInjector::SendStatus::Success) return tokenResult;
      }
      return {InputInjector::SendStatus::Success, {}};
  }
  return {InputInjector::SendStatus::PostActionFailed, {}};
}
} // namespace
#endif

InputInjector::SendResult InputInjector::SendTextAndAction(const std::wstring& text, const OutputSettings& output) {
#ifdef _WIN32
  if (text.empty()) return {SendStatus::TextFailed, {}};
  for (wchar_t ch : text) {
    INPUT in[2]{};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.dwFlags = KEYEVENTF_UNICODE;
    in[0].ki.wScan = ch;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    if (SendInput(2, in, sizeof(INPUT)) != 2) return {SendStatus::TextFailed, {}};
  }
  return SendPostAction(output);
#else
  (void)text;
  (void)output;
  return {SendStatus::TextFailed, {}};
#endif
}

} // namespace scalelogger
