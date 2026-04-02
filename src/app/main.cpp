#ifdef _WIN32
#include "ui/MainDialog.hpp"

#include <Windows.h>
#include <exception>
#include <sstream>

namespace {
int ShowFatalMessage(const std::string& details) {
  const char* message = details.empty() ? "Unexpected fatal error." : details.c_str();
  MessageBoxA(nullptr, message, "ScaleLogger", MB_OK | MB_ICONERROR | MB_TOPMOST);
  return 1;
}

#if defined(_MSC_VER)
int RunMainDialogWithSehBoundary(HINSTANCE hInstance, int nCmdShow, unsigned long* exceptionCodeOut) {
  if (exceptionCodeOut) *exceptionCodeOut = 0;
  __try {
    return scalelogger::RunMainDialogFresh(hInstance, nCmdShow);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    if (exceptionCodeOut) *exceptionCodeOut = static_cast<unsigned long>(GetExceptionCode());
    return 1;
  }
}

int RunMainDialogWithSehGuard(HINSTANCE hInstance, int nCmdShow) {
  unsigned long exceptionCode = 0;
  const int code = RunMainDialogWithSehBoundary(hInstance, nCmdShow, &exceptionCode);
  if (exceptionCode == 0) return code;

  std::ostringstream oss;
  oss << "Fatal Windows exception (SEH), code=0x" << std::hex << exceptionCode;
  return ShowFatalMessage(oss.str());
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
    return ShowFatalMessage(std::string(ex.what() ? ex.what() : ""));
  } catch (...) {
    return ShowFatalMessage("Unexpected fatal error.");
  }
}
#endif
