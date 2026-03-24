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
  in[1] = in[0];
  in[1].ki.dwFlags = KEYEVENTF_KEYUP;
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
  if (token.size() == 1 && token[0] >= '0' && token[0] <= '9') return static_cast<WORD>(token[0]);
  if (token.size() == 1 && token[0] >= 'a' && token[0] <= 'z') return static_cast<WORD>(token[0] - 32);
  if (token == "f10") return VK_F10;
  if (token == "f11") return VK_F11;
  if (token == "f12") return VK_F12;
  if (token.size() == 2 && token[0] == 'f' && token[1] >= '1' && token[1] <= '9') return static_cast<WORD>(VK_F1 + (token[1] - '1'));
  return 0;
}

bool SendPostAction(const OutputSettings& output) {
  auto sendToken = [](const std::string& token) -> bool {
    const auto normalized = NormalizeKeyToken(token);
    if (!normalized.has_value()) return false;
    const WORD vk = TokenToVk(*normalized);
    return vk != 0 && SendVirtualKey(vk);
  };

  switch (output.postAction) {
    case PostAction::Down: return SendVirtualKey(VK_DOWN);
    case PostAction::Right: return SendVirtualKey(VK_RIGHT);
    case PostAction::Enter: return SendVirtualKey(VK_RETURN);
    case PostAction::Tab: return SendVirtualKey(VK_TAB);
    case PostAction::None: return true;
    case PostAction::CustomSequence:
      for (const auto& token : output.customSequence) {
        if (!sendToken(token)) return false;
      }
      return true;
  }
  return false;
}
} // namespace
#endif

bool InputInjector::SendTextAndAction(const std::wstring& text, const OutputSettings& output) {
#ifdef _WIN32
  if (text.empty()) return false;
  for (wchar_t ch : text) {
    INPUT in[2]{};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.dwFlags = KEYEVENTF_UNICODE;
    in[0].ki.wScan = ch;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    if (SendInput(2, in, sizeof(INPUT)) != 2) return false;
  }
  return SendPostAction(output);
#else
  (void)text;
  (void)output;
  return false;
#endif
}

} // namespace scalelogger
