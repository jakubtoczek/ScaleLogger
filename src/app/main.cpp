#ifdef _WIN32
#include "core/AppConfig.hpp"
#include "core/AppVersion.hpp"
#include "ui/MainDialog.hpp"

#include <Windows.h>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace {
struct FatalDiagnosticsConfig {
  bool enableStartupTrace{true};
  bool enableFatalLogFile{true};
  bool showCrashDialog{true};
  bool includeTraceInCrashDialog{true};
};

FatalDiagnosticsConfig g_fatalDiagnosticsConfig{};
std::string g_fatalTraceBuffer;
bool g_fatalDialogInProgress = false;

std::string GetFatalLogPath() {
  char tempPath[MAX_PATH]{};
  const DWORD len = GetTempPathA(MAX_PATH, tempPath);
  return (len > 0 && len < MAX_PATH) ? std::string(tempPath) + "ScaleLogger_fatal.log" : "ScaleLogger_fatal.log";
}

std::filesystem::path ResolveUserConfigPath() {
  const char* userProfile = std::getenv("USERPROFILE");
  const char* localAppData = std::getenv("LOCALAPPDATA");
  const std::filesystem::path dataRoot = userProfile     ? std::filesystem::path(userProfile) / "ScaleLogger"
                                         : localAppData ? std::filesystem::path(localAppData) / "ScaleLogger"
                                                        : (std::filesystem::temp_directory_path() / "ScaleLogger");
  return dataRoot / "ScaleLogger.config.json";
}

std::string GetEnvOrUnset(const char* name) {
  const char* value = std::getenv(name);
  return value ? std::string(value) : std::string("<unset>");
}

std::string GetExecutablePath() {
  char path[MAX_PATH]{};
  const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
  return (len > 0 && len < MAX_PATH) ? std::string(path, len) : std::string("<unknown>");
}

void LoadFatalDiagnosticsConfig() {
  try {
    const auto configPath = ResolveUserConfigPath();
    if (!std::filesystem::exists(configPath)) return;
    const auto cfg = scalelogger::LoadConfig(configPath);
    g_fatalDiagnosticsConfig.enableStartupTrace = cfg.enableStartupTrace;
    g_fatalDiagnosticsConfig.enableFatalLogFile = cfg.enableFatalLogFile;
    g_fatalDiagnosticsConfig.showCrashDialog = cfg.showCrashDialog;
    g_fatalDiagnosticsConfig.includeTraceInCrashDialog = cfg.includeTraceInCrashDialog;
  } catch (...) {
  }
}

void AppendFatalLine(const std::string& message, bool isTrace) {
  if (isTrace && !g_fatalDiagnosticsConfig.enableStartupTrace) return;
  g_fatalTraceBuffer += message + "\n";
  if (g_fatalDiagnosticsConfig.enableFatalLogFile) {
    std::ofstream out(GetFatalLogPath(), std::ios::out | std::ios::app);
    if (out.is_open()) {
      out << message << "\n";
      out.flush();
    }
  }
  OutputDebugStringA((message + "\n").c_str());
}

std::string NormalizeToWindowsNewlines(const std::string& input) {
  std::string output;
  output.reserve(input.size() + 16);
  for (std::size_t i = 0; i < input.size(); ++i) {
    const char ch = input[i];
    if (ch == '\r') {
      output.push_back('\r');
      if (i + 1 < input.size() && input[i + 1] == '\n') output.push_back('\n');
      else output.push_back('\n');
      if (i + 1 < input.size() && input[i + 1] == '\n') ++i;
      continue;
    }
    if (ch == '\n') {
      output.push_back('\r');
      output.push_back('\n');
      continue;
    }
    output.push_back(ch);
  }
  return output;
}

struct CrashDialogData {
  std::string report;
  HWND editControl{nullptr};
};

LRESULT CALLBACK CrashDialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto* data = reinterpret_cast<CrashDialogData*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_CREATE: {
      auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
      data = reinterpret_cast<CrashDialogData*>(create->lpCreateParams);
      SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
      data->editControl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", data->report.c_str(),
                                          WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 12, 12, 596, 256,
                                          hwnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
      CreateWindowExA(0, "BUTTON", "Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 420, 276, 90, 28, hwnd, reinterpret_cast<HMENU>(1002), nullptr,
                      nullptr);
      CreateWindowExA(0, "BUTTON", "Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 518, 276, 90, 28, hwnd, reinterpret_cast<HMENU>(1003), nullptr,
                      nullptr);
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wParam) == 1002 && data) {
        if (OpenClipboard(hwnd)) {
          EmptyClipboard();
          HGLOBAL hText = GlobalAlloc(GMEM_MOVEABLE, data->report.size() + 1);
          if (hText) {
            void* ptr = GlobalLock(hText);
            if (ptr) {
              memcpy(ptr, data->report.c_str(), data->report.size() + 1);
              GlobalUnlock(hText);
              SetClipboardData(CF_TEXT, hText);
              hText = nullptr;
            }
          }
          if (hText) GlobalFree(hText);
          CloseClipboard();
        }
        return 0;
      }
      if (LOWORD(wParam) == 1003) {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: break;
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void ShowCrashDialog(const std::string& title, const std::string& body) {
  if (!g_fatalDiagnosticsConfig.showCrashDialog || g_fatalDialogInProgress) return;
  g_fatalDialogInProgress = true;
  CrashDialogData data{NormalizeToWindowsNewlines(body)};
  WNDCLASSA wc{};
  wc.lpfnWndProc = CrashDialogWndProc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = "ScaleLoggerCrashDialogClass";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassA(&wc);
  HWND hwnd = CreateWindowExA(WS_EX_TOPMOST, wc.lpszClassName, title.c_str(), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 632,
                              356, nullptr, nullptr, wc.hInstance, &data);
  if (!hwnd) {
    MessageBoxA(nullptr, data.report.c_str(), title.c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
    g_fatalDialogInProgress = false;
    return;
  }
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  MSG msg;
  while (IsWindow(hwnd) && GetMessageA(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  g_fatalDialogInProgress = false;
}

void ReportFatalCrash(const std::string& headline, unsigned long exceptionCode, bool hasExceptionCode, bool startupCrash) {
  AppendFatalLine(headline, false);
  std::ostringstream report;
  report << "ScaleLogger crash\n\n";
  report << "Version: " << scalelogger::kAppVersion << "\n";
  report << "Build tag: " << scalelogger::GetBuildTag() << "\n";
  report << "Executable path: " << GetExecutablePath() << "\n";
  report << "SCALELOGGER_RUNMAIN_STAGE_LIMIT: " << GetEnvOrUnset("SCALELOGGER_RUNMAIN_STAGE_LIMIT") << "\n";
  report << "SCALELOGGER_FORCE_NO_SERIAL: " << GetEnvOrUnset("SCALELOGGER_FORCE_NO_SERIAL") << "\n\n";
  if (hasExceptionCode) {
    report << "Exception code: 0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << exceptionCode << "\n";
  } else {
    report << "Exception code: unknown\n";
  }
  report << "Startup crash: " << (startupCrash ? "yes" : "no") << "\n";
  report << "Fatal log path: " << GetFatalLogPath() << "\n\n";
  if (g_fatalDiagnosticsConfig.includeTraceInCrashDialog) {
    report << "Fatal trace:\n" << g_fatalTraceBuffer << "\n";
  }
  ShowCrashDialog("ScaleLogger crash", report.str());
}

LONG WINAPI FatalSehHandler(EXCEPTION_POINTERS* exceptionInfo) {
  const auto code = exceptionInfo && exceptionInfo->ExceptionRecord ? exceptionInfo->ExceptionRecord->ExceptionCode : 0;
  std::ostringstream oss;
  oss << "FATAL: SEH exception occurred. code=0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
      << static_cast<unsigned long>(code);
  ReportFatalCrash(oss.str(), static_cast<unsigned long>(code), true, true);
  AppendFatalLine("Session end: crash (fatal exception)", false);
  return EXCEPTION_EXECUTE_HANDLER;
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  LoadFatalDiagnosticsConfig();
  AppendFatalLine("===== ScaleLogger fatal session start =====", false);
  AppendFatalLine("TRACE: ScaleLogger version: " + std::string(scalelogger::kAppVersion), true);
  AppendFatalLine("TRACE: Build tag: " + std::string(scalelogger::GetBuildTag()), true);
  AppendFatalLine("TRACE: Executable path: " + GetExecutablePath(), true);
  AppendFatalLine("TRACE: SCALELOGGER_RUNMAIN_STAGE_LIMIT=" + GetEnvOrUnset("SCALELOGGER_RUNMAIN_STAGE_LIMIT"), true);
  AppendFatalLine("TRACE: SCALELOGGER_FORCE_NO_SERIAL=" + GetEnvOrUnset("SCALELOGGER_FORCE_NO_SERIAL"), true);
  AppendFatalLine("TRACE: wWinMain entered", true);
  SetUnhandledExceptionFilter(FatalSehHandler);
  AppendFatalLine("TRACE: UnhandledExceptionFilter installed", true);
  try {
    AppendFatalLine("TRACE: Calling ProbeMainDialogBasic", true);
    const int probeBasicCode = scalelogger::ProbeMainDialogBasic();
    AppendFatalLine("TRACE: ProbeMainDialogBasic returned code=" + std::to_string(probeBasicCode), true);
    AppendFatalLine("TRACE: Calling ProbeMainDialogTraceEarly", true);
    const int probeTraceEarlyCode = scalelogger::ProbeMainDialogTraceEarly();
    AppendFatalLine("TRACE: ProbeMainDialogTraceEarly returned code=" + std::to_string(probeTraceEarlyCode), true);
    AppendFatalLine("TRACE: Calling ProbeMainDialogWithArgs", true);
    const int probeWithArgsCode = scalelogger::ProbeMainDialogWithArgs(hInstance, nCmdShow);
    AppendFatalLine("TRACE: ProbeMainDialogWithArgs returned code=" + std::to_string(probeWithArgsCode), true);
    AppendFatalLine("TRACE: Calling ProbeMainDialogTouchUi", true);
    const int probeTouchUiCode = scalelogger::ProbeMainDialogTouchUi(hInstance);
    AppendFatalLine("TRACE: ProbeMainDialogTouchUi returned code=" + std::to_string(probeTouchUiCode), true);
    AppendFatalLine("TRACE: ProbeRunMainDialogImplDirect active", true);
    AppendFatalLine("TRACE: Calling ProbeRunMainDialogImplDirect", true);
    const int probeRunMainDialogImplDirectCode = scalelogger::ProbeRunMainDialogImplDirect(hInstance, nCmdShow);
    AppendFatalLine("TRACE: ProbeRunMainDialogImplDirect returned code=" + std::to_string(probeRunMainDialogImplDirectCode), true);
    AppendFatalLine("TRACE: Calling RunMainDialog", true);
    const int exitCode = scalelogger::RunMainDialog(hInstance, nCmdShow);
    AppendFatalLine("TRACE: RunMainDialog returned exit_code=" + std::to_string(exitCode), true);
    return exitCode;
  } catch (const std::exception& ex) {
    ReportFatalCrash(std::string("FATAL: unhandled exception reached main: ") + ex.what(), 0, false, true);
    AppendFatalLine("Session end: crash (fatal exception)", false);
    return 1;
  } catch (...) {
    ReportFatalCrash("FATAL: unhandled exception reached main", 0, false, true);
    AppendFatalLine("Session end: crash (fatal exception)", false);
    return 1;
  }
}
#endif
