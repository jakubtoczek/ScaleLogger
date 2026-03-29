#include "ui/AboutDialog.hpp"

#ifdef _WIN32
#include "core/AppVersion.hpp"

#include <Windows.h>
#include <string>

namespace scalelogger {
void ShowAbout(HWND parent) {
  MessageBoxW(parent,
              (std::wstring(L"ScaleLogger\nVersion: ") + kAppVersionWide + L"\nRepository: https://github.com/jakubtoczek/ScaleLogger\nLicense: MIT")
                  .c_str(),
              L"About ScaleLogger", MB_OK | MB_ICONINFORMATION);
}
} // namespace scalelogger
#endif
