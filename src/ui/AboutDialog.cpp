#ifdef _WIN32
#include <Windows.h>

namespace scalelogger {
void ShowAbout(HWND parent) {
  MessageBoxW(parent, L"ScaleLogger 0.95\nRepository: github.com/jakubtoczek/ScaleLogger\nLicense: MIT\nDevelopment assisted by OpenAI ChatGPT and Codex", L"About ScaleLogger", MB_OK);
}
} // namespace scalelogger
#endif
