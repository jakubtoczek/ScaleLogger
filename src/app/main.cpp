#ifdef _WIN32
#include "core/AppConfig.hpp"
#include "core/AppVersion.hpp"
#include "ui/MainDialog.hpp"

#include <Windows.h>
#include <cstdint>
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

std::string FormatFnPtr(const void* ptr) {
  std::ostringstream oss;
  oss << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(ptr);
  return oss.str();
}

#if defined(_MSC_VER)
#define SCALELOGGER_MAIN_NOINLINE __declspec(noinline)
#else
#define SCALELOGGER_MAIN_NOINLINE __attribute__((noinline))
#endif

static SCALELOGGER_MAIN_NOINLINE int CallRunMainFromMainThunk(scalelogger::RunMainDialogFn fn, HINSTANCE hInstance, int nCmdShow) {
  AppendFatalLine("TRACE: CallRunMainFromMainThunk entered", true);
  AppendFatalLine("TRACE: CallRunMainFromMainThunk fn=" + FormatFnPtr(reinterpret_cast<const void*>(fn)), true);
  AppendFatalLine("TRACE: CallRunMainFromMainThunk before invoking fn", true);
  const int code = fn(hInstance, nCmdShow);
  AppendFatalLine("TRACE: CallRunMainFromMainThunk after invoking fn code=" + std::to_string(code), true);
  return code;
}

void LogFunctionVirtualMemoryInfo(const char* symbolName, const void* ptr) {
  AppendFatalLine(std::string("TRACE: VQ begin symbol=") + symbolName + " ptr=" + FormatFnPtr(ptr), true);
  MEMORY_BASIC_INFORMATION mbi{};
  const SIZE_T queried = VirtualQuery(ptr, &mbi, sizeof(mbi));
  if (queried == 0) {
    AppendFatalLine(std::string("TRACE: VQ failed symbol=") + symbolName + " gle=" + std::to_string(GetLastError()), true);
    return;
  }
  std::ostringstream oss;
  oss << "TRACE: VQ symbol=" << symbolName << " allocBase=0x" << std::hex << reinterpret_cast<std::uintptr_t>(mbi.AllocationBase) << " base=0x"
      << reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) << std::dec << " regionSize=" << static_cast<unsigned long long>(mbi.RegionSize)
      << " state=0x" << std::hex << static_cast<unsigned long>(mbi.State) << " protect=0x" << static_cast<unsigned long>(mbi.Protect)
      << " type=0x" << static_cast<unsigned long>(mbi.Type);
  AppendFatalLine(oss.str(), true);
}

struct MainCallContext {
  HINSTANCE hInstance{nullptr};
  int nCmdShow{0};
  scalelogger::RunMainDialogFn fn{nullptr};
};

static int InvokeRunMainDirect(void* ctxRaw) {
  auto* ctx = reinterpret_cast<MainCallContext*>(ctxRaw);
  return scalelogger::RunMainDialog(ctx->hInstance, ctx->nCmdShow);
}

static int InvokeRunMainFn(void* ctxRaw) {
  auto* ctx = reinterpret_cast<MainCallContext*>(ctxRaw);
  return ctx->fn(ctx->hInstance, ctx->nCmdShow);
}

static int InvokeRunMainMainThunk(void* ctxRaw) {
  auto* ctx = reinterpret_cast<MainCallContext*>(ctxRaw);
  return CallRunMainFromMainThunk(ctx->fn, ctx->hInstance, ctx->nCmdShow);
}

static int InvokeWrapperProbe(void* ctxRaw) {
  auto* ctx = reinterpret_cast<MainCallContext*>(ctxRaw);
  return scalelogger::ProbeRunMainDialogWrapper(ctx->hInstance, ctx->nCmdShow);
}

static int InvokeImplProbe(void* ctxRaw) {
  auto* ctx = reinterpret_cast<MainCallContext*>(ctxRaw);
  return scalelogger::ProbeRunMainDialogImplDirect(ctx->hInstance, ctx->nCmdShow);
}

// Caller-side SEH guards are diagnostics-only to localize crash boundary behavior; not production crash-handling design.
static SCALELOGGER_MAIN_NOINLINE int CallWithSehGuard(const char* label, int sehReturnCode, int (*invoke)(void*), void* ctxRaw) {
  AppendFatalLine(std::string("TRACE: CallWithSehGuard entered label=") + label, true);
  AppendFatalLine(std::string("TRACE: CallWithSehGuard before guarded invoke label=") + label, true);
  __try {
    const int code = invoke(ctxRaw);
    AppendFatalLine(std::string("TRACE: CallWithSehGuard after guarded invoke label=") + label + " code=" + std::to_string(code), true);
    return code;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    std::ostringstream oss;
    oss << "TRACE: SEH trapped in " << label << " code=0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
        << static_cast<unsigned long>(GetExceptionCode());
    AppendFatalLine(oss.str(), true);
    return sehReturnCode;
  }
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  LoadFatalDiagnosticsConfig();
  AppendFatalLine("===== ScaleLogger fatal session start =====", false);
  AppendFatalLine("TRACE: ScaleLogger version: " + std::string(scalelogger::kAppVersion), true);
  AppendFatalLine("TRACE: Build tag: " + std::string(scalelogger::GetBuildTag()), true);
  AppendFatalLine("TRACE: Executable path: " + GetExecutablePath(), true);
  AppendFatalLine("TRACE: SCALELOGGER_RUNMAIN_STAGE_LIMIT=" + GetEnvOrUnset("SCALELOGGER_RUNMAIN_STAGE_LIMIT"), true);
  AppendFatalLine("TRACE: SCALELOGGER_RUNMAIN_CALL_MODE=" + GetEnvOrUnset("SCALELOGGER_RUNMAIN_CALL_MODE"), true);
  AppendFatalLine("TRACE: SCALELOGGER_MAIN_CALL_TARGET=" + GetEnvOrUnset("SCALELOGGER_MAIN_CALL_TARGET"), true);
  AppendFatalLine("TRACE: SCALELOGGER_RUNMAIN_MATRIX=" + GetEnvOrUnset("SCALELOGGER_RUNMAIN_MATRIX"), true);
  AppendFatalLine("TRACE: SCALELOGGER_SKIP_FINAL_RUNMAIN=" + GetEnvOrUnset("SCALELOGGER_SKIP_FINAL_RUNMAIN"), true);
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
    const bool enableDirectImplProbe = GetEnvOrUnset("SCALELOGGER_ENABLE_DIRECT_IMPL_PROBE") == "1";
    if (enableDirectImplProbe) {
      AppendFatalLine("TRACE: ProbeRunMainDialogImplDirect active", true);
      AppendFatalLine("TRACE: Calling ProbeRunMainDialogImplDirect", true);
      const int probeRunMainDialogImplDirectCode = scalelogger::ProbeRunMainDialogImplDirect(hInstance, nCmdShow);
      AppendFatalLine("TRACE: ProbeRunMainDialogImplDirect returned code=" + std::to_string(probeRunMainDialogImplDirectCode), true);
    } else {
      AppendFatalLine("TRACE: ProbeRunMainDialogImplDirect skipped (env not enabled)", true);
    }
    const bool enableRunMainWrapperProbe = GetEnvOrUnset("SCALELOGGER_ENABLE_RUNMAIN_WRAPPER_PROBE") == "1";
    if (enableRunMainWrapperProbe) {
      AppendFatalLine("TRACE: ProbeRunMainDialogWrapper active", true);
      AppendFatalLine("TRACE: Calling ProbeRunMainDialogWrapper", true);
      const int probeRunMainDialogWrapperCode = scalelogger::ProbeRunMainDialogWrapper(hInstance, nCmdShow);
      AppendFatalLine("TRACE: ProbeRunMainDialogWrapper returned code=" + std::to_string(probeRunMainDialogWrapperCode), true);
    } else {
      AppendFatalLine("TRACE: ProbeRunMainDialogWrapper skipped (env not enabled)", true);
    }

    AppendFatalLine("TRACE: Address RunMainDialog=" + FormatFnPtr(reinterpret_cast<const void*>(&scalelogger::RunMainDialog)), true);
    AppendFatalLine("TRACE: Address ProbeRunMainDialogWrapper=" + FormatFnPtr(reinterpret_cast<const void*>(&scalelogger::ProbeRunMainDialogWrapper)),
                    true);
    AppendFatalLine("TRACE: Address ProbeRunMainDialogImplDirect=" + FormatFnPtr(reinterpret_cast<const void*>(&scalelogger::ProbeRunMainDialogImplDirect)),
                    true);
    AppendFatalLine("TRACE: Address ProbeMainDialogWithArgs=" + FormatFnPtr(reinterpret_cast<const void*>(&scalelogger::ProbeMainDialogWithArgs)), true);
    LogFunctionVirtualMemoryInfo("RunMainDialog", reinterpret_cast<const void*>(&scalelogger::RunMainDialog));
    LogFunctionVirtualMemoryInfo("ProbeRunMainDialogWrapper", reinterpret_cast<const void*>(&scalelogger::ProbeRunMainDialogWrapper));
    LogFunctionVirtualMemoryInfo("ProbeRunMainDialogImplDirect", reinterpret_cast<const void*>(&scalelogger::ProbeRunMainDialogImplDirect));
    LogFunctionVirtualMemoryInfo("ProbeMainDialogWithArgs", reinterpret_cast<const void*>(&scalelogger::ProbeMainDialogWithArgs));

    if (GetEnvOrUnset("SCALELOGGER_SKIP_FINAL_RUNMAIN") == "1") {
      AppendFatalLine("TRACE: Final call branch=skip-final override", true);
      AppendFatalLine("TRACE: Final RunMain call skipped by env override", true);
      return 120;
    }

    MainCallContext mainCtx{};
    mainCtx.hInstance = hInstance;
    mainCtx.nCmdShow = nCmdShow;
    mainCtx.fn = &scalelogger::RunMainDialog;

    if (GetEnvOrUnset("SCALELOGGER_RUNMAIN_MATRIX") == "1") {
      AppendFatalLine("TRACE: Final call branch=matrix mode", true);
      AppendFatalLine("TRACE: Matrix step 1 begin target=impl-probe", true);
      int matrixCode = CallWithSehGuard("impl-probe", 245, InvokeImplProbe, &mainCtx);
      AppendFatalLine("TRACE: Matrix step 1 end target=impl-probe code=" + std::to_string(matrixCode), true);

      AppendFatalLine("TRACE: Matrix step 2 begin target=wrapper-probe", true);
      matrixCode = CallWithSehGuard("wrapper-probe", 244, InvokeWrapperProbe, &mainCtx);
      AppendFatalLine("TRACE: Matrix step 2 end target=wrapper-probe code=" + std::to_string(matrixCode), true);

      AppendFatalLine("TRACE: Matrix step 3 begin target=runmain-fn", true);
      matrixCode = CallWithSehGuard("runmain-fn", 242, InvokeRunMainFn, &mainCtx);
      AppendFatalLine("TRACE: Matrix step 3 end target=runmain-fn code=" + std::to_string(matrixCode), true);

      AppendFatalLine("TRACE: Matrix step 4 begin target=runmain-mainthunk", true);
      matrixCode = CallWithSehGuard("runmain-mainthunk", 243, InvokeRunMainMainThunk, &mainCtx);
      AppendFatalLine("TRACE: Matrix step 4 end target=runmain-mainthunk code=" + std::to_string(matrixCode), true);

      AppendFatalLine("TRACE: Matrix step 5 begin target=runmain-direct", true);
      matrixCode = CallWithSehGuard("runmain-direct", 241, InvokeRunMainDirect, &mainCtx);
      AppendFatalLine("TRACE: Matrix step 5 end target=runmain-direct code=" + std::to_string(matrixCode), true);
      AppendFatalLine("TRACE: Matrix mode complete returning code=130", true);
      return 130;
    }
    AppendFatalLine("TRACE: Final call branch=normal selected call target", true);

    // Caller-side boundary instrumentation to distinguish direct call, fn-pointer call, main-thunk call, wrapper-probe, and impl-probe paths.
    const std::string mainCallTargetRaw = GetEnvOrUnset("SCALELOGGER_MAIN_CALL_TARGET");
    const std::string mainCallTarget = mainCallTargetRaw == "<unset>" ? "runmain-direct" : mainCallTargetRaw;
    if (mainCallTargetRaw == "<unset>") {
      AppendFatalLine("TRACE: Main call target unset, defaulting to runmain-direct", true);
    }
    int exitCode = 0;
    if (mainCallTarget == "runmain-direct") {
      AppendFatalLine("TRACE: Selected main call target=runmain-direct", true);
      AppendFatalLine("TRACE: Before final call path runmain-direct", true);
      exitCode = CallWithSehGuard("runmain-direct", 241, InvokeRunMainDirect, &mainCtx);
      AppendFatalLine("TRACE: After final call path runmain-direct code=" + std::to_string(exitCode), true);
      return exitCode;
    }
    if (mainCallTarget == "runmain-fn") {
      AppendFatalLine("TRACE: Selected main call target=runmain-fn", true);
      AppendFatalLine("TRACE: runmain-fn before binding function pointer", true);
      mainCtx.fn = &scalelogger::RunMainDialog;
      AppendFatalLine("TRACE: runmain-fn after binding function pointer fn=" + FormatFnPtr(reinterpret_cast<const void*>(mainCtx.fn)), true);
      AppendFatalLine("TRACE: Before final call path runmain-fn", true);
      exitCode = CallWithSehGuard("runmain-fn", 242, InvokeRunMainFn, &mainCtx);
      AppendFatalLine("TRACE: After final call path runmain-fn code=" + std::to_string(exitCode), true);
      return exitCode;
    }
    if (mainCallTarget == "runmain-mainthunk") {
      AppendFatalLine("TRACE: Selected main call target=runmain-mainthunk", true);
      AppendFatalLine("TRACE: runmain-mainthunk before binding function pointer", true);
      mainCtx.fn = &scalelogger::RunMainDialog;
      AppendFatalLine("TRACE: runmain-mainthunk after binding function pointer fn=" + FormatFnPtr(reinterpret_cast<const void*>(mainCtx.fn)), true);
      AppendFatalLine("TRACE: Before final call path runmain-mainthunk", true);
      exitCode = CallWithSehGuard("runmain-mainthunk", 243, InvokeRunMainMainThunk, &mainCtx);
      AppendFatalLine("TRACE: After final call path runmain-mainthunk code=" + std::to_string(exitCode), true);
      return exitCode;
    }
    if (mainCallTarget == "wrapper-probe") {
      AppendFatalLine("TRACE: Selected main call target=wrapper-probe", true);
      AppendFatalLine("TRACE: Before final call path wrapper-probe", true);
      exitCode = CallWithSehGuard("wrapper-probe", 244, InvokeWrapperProbe, &mainCtx);
      AppendFatalLine("TRACE: After final call path wrapper-probe code=" + std::to_string(exitCode), true);
      return exitCode;
    }
    if (mainCallTarget == "impl-probe") {
      AppendFatalLine("TRACE: Selected main call target=impl-probe", true);
      AppendFatalLine("TRACE: Before final call path impl-probe", true);
      exitCode = CallWithSehGuard("impl-probe", 245, InvokeImplProbe, &mainCtx);
      AppendFatalLine("TRACE: After final call path impl-probe code=" + std::to_string(exitCode), true);
      return exitCode;
    }
    AppendFatalLine("TRACE: Invalid main call target '" + mainCallTarget + "', falling back to runmain-direct", true);
    AppendFatalLine("TRACE: Before final call path runmain-direct", true);
    exitCode = CallWithSehGuard("runmain-direct", 241, InvokeRunMainDirect, &mainCtx);
    AppendFatalLine("TRACE: After final call path runmain-direct code=" + std::to_string(exitCode), true);
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
