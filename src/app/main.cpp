#ifdef _WIN32
#include "ui/MainDialog.hpp"

#include <Windows.h>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace {
void WriteFatalStartupLog(const std::string& message) {
  char tempPath[MAX_PATH]{};
  const DWORD len = GetTempPathA(MAX_PATH, tempPath);
  std::string logPath = (len > 0 && len < MAX_PATH) ? std::string(tempPath) + "ScaleLogger_fatal.log" : "ScaleLogger_fatal.log";
  std::ofstream out(logPath, std::ios::out | std::ios::app);
  if (out.is_open()) {
    out << message << "\n";
    out.flush();
  }
  OutputDebugStringA((message + "\n").c_str());
}

LONG WINAPI FatalSehHandler(EXCEPTION_POINTERS* exceptionInfo) {
  const auto code = exceptionInfo && exceptionInfo->ExceptionRecord ? exceptionInfo->ExceptionRecord->ExceptionCode : 0;
  std::ostringstream oss;
  oss << "FATAL: SEH exception occurred. code=0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
      << static_cast<unsigned long>(code);
  WriteFatalStartupLog(oss.str());
  WriteFatalStartupLog("Session end: crash (fatal exception)");
  return EXCEPTION_EXECUTE_HANDLER;
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  WriteFatalStartupLog("TRACE: wWinMain entered");
  SetUnhandledExceptionFilter(FatalSehHandler);
  WriteFatalStartupLog("TRACE: UnhandledExceptionFilter installed");
  try {
    WriteFatalStartupLog("TRACE: Before RunMainDialog");
    const int exitCode = scalelogger::RunMainDialog(hInstance, nCmdShow);
    WriteFatalStartupLog("TRACE: RunMainDialog returned exit_code=" + std::to_string(exitCode));
    return exitCode;
  } catch (const std::exception& ex) {
    WriteFatalStartupLog(std::string("FATAL: unhandled exception reached main: ") + ex.what());
    WriteFatalStartupLog("Session end: crash (fatal exception)");
    return 1;
  } catch (...) {
    WriteFatalStartupLog("FATAL: unhandled exception reached main");
    WriteFatalStartupLog("Session end: crash (fatal exception)");
    return 1;
  }
}
#endif
