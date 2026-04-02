#ifdef _WIN32
#include "ui/MainDialog.hpp"

#include <Windows.h>
#include <exception>
#include <sstream>

namespace {
int ShowFatalMessage(const char* details) {
  MessageBoxA(nullptr, details ? details : "Unexpected fatal error.", "ScaleLogger", MB_OK | MB_ICONERROR | MB_TOPMOST);
  return 1;
}

#if defined(_MSC_VER)
int RunMainDialogWithSehGuard(HINSTANCE hInstance, int nCmdShow) {
  __try {
    return scalelogger::RunMainDialogFresh(hInstance, nCmdShow);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    std::ostringstream oss;
    oss << "Fatal Windows exception (SEH), code=0x" << std::hex << static_cast<unsigned long>(GetExceptionCode());
    return ShowFatalMessage(oss.str().c_str());
  }
}
#else
int RunMainDialogWithSehGuard(HINSTANCE hInstance, int nCmdShow) {
  return scalelogger::RunMainDialogFresh(hInstance, nCmdShow);
}
#endif
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  try {
    return RunMainDialogWithSehGuard(hInstance, nCmdShow);
  } catch (const std::exception& ex) {
    return ShowFatalMessage(ex.what());
  } catch (...) {
    return ShowFatalMessage("Unexpected fatal error.");
  }
}
#endif
