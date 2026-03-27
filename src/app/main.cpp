#ifdef _WIN32
#include "ui/MainDialog.hpp"

#include <Windows.h>
#include <exception>
#include <fstream>
#include <string>

namespace {
void WriteFatalStartupLog(const std::string& message) {
  char tempPath[MAX_PATH]{};
  const DWORD len = GetTempPathA(MAX_PATH, tempPath);
  std::string logPath = (len > 0 && len < MAX_PATH) ? std::string(tempPath) + "ScaleLogger_fatal.log" : "ScaleLogger_fatal.log";
  std::ofstream out(logPath, std::ios::out | std::ios::app);
  if (out.is_open()) out << message << "\n";
  OutputDebugStringA((message + "\n").c_str());
}

LONG WINAPI FatalSehHandler(EXCEPTION_POINTERS* exceptionInfo) {
  const auto code = exceptionInfo && exceptionInfo->ExceptionRecord ? exceptionInfo->ExceptionRecord->ExceptionCode : 0;
  WriteFatalStartupLog("FATAL: structured exception (SEH) occurred. code=0x" + std::to_string(static_cast<unsigned long>(code)));
  return EXCEPTION_EXECUTE_HANDLER;
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  SetUnhandledExceptionFilter(FatalSehHandler);
  try {
    return scalelogger::RunMainDialog(hInstance, nCmdShow);
  } catch (const std::exception& ex) {
    WriteFatalStartupLog(std::string("FATAL: unhandled exception reached main: ") + ex.what());
    return 1;
  } catch (...) {
    WriteFatalStartupLog("FATAL: unhandled exception reached main");
    return 1;
  }
}
#endif
