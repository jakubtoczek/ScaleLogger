#include "ui/AboutDialog.hpp"

#ifdef _WIN32
#include <Windows.h>

namespace scalelogger {
void ShowAbout(HWND parent) {
  MessageBoxW(parent,
              L"ScaleLogger\nVersion: 0.96\nRepository: https://github.com/jakubtoczek/ScaleLogger\nLicense: MIT",
              L"About ScaleLogger", MB_OK | MB_ICONINFORMATION);
}
} // namespace scalelogger
#endif
