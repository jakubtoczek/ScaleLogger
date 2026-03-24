#include "input/InputInjector.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {

bool InputInjector::SendTextAndAction(const std::wstring& text, const OutputSettings&) {
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
  return true;
#else
  (void)text;
  return false;
#endif
}

} // namespace scalelogger
