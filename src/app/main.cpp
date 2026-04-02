#ifdef _WIN32
#include "ui/MainDialog.hpp"

#include <Windows.h>
#include <exception>

namespace {
int ShowFatalMessage(const char* details) {
  MessageBoxA(nullptr, details ? details : "Unexpected fatal error.", "ScaleLogger", MB_OK | MB_ICONERROR | MB_TOPMOST);
  return 1;
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  try {
    return scalelogger::RunMainDialogFresh(hInstance, nCmdShow);
  } catch (const std::exception& ex) {
    return ShowFatalMessage(ex.what());
  } catch (...) {
    return ShowFatalMessage("Unexpected fatal error.");
  }
}
#endif
