#include "ui/MainDialogCompat1715.hpp"

#ifdef _WIN32
#include "app/AppController.hpp"

#include <cstdlib>
#include <filesystem>
#include <memory>

namespace scalelogger {
namespace {
constexpr int kCompat1715BtnConnect = 1001;
constexpr int kCompat1715BtnDisconnect = 1002;
constexpr int kCompat1715BtnExit = 1003;

std::unique_ptr<AppController> g_compat1715Controller;

LRESULT CALLBACK MainCompat1715WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  (void)lParam;
  switch (msg) {
    case WM_CREATE: {
      CreateWindowW(L"BUTTON", L"Connect", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 16, 16, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kCompat1715BtnConnect), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Disconnect", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 152, 16, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kCompat1715BtnDisconnect), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Exit", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 288, 16, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kCompat1715BtnExit), nullptr, nullptr);
      CreateWindowW(L"STATIC", L"ScaleLogger runtime test shell\nUse Settings JSON/presets under %LOCALAPPDATA%\\ScaleLogger.",
                    WS_VISIBLE | WS_CHILD, 16, 64, 560, 48, hwnd, nullptr, nullptr, nullptr);
      return 0;
    }
    case WM_COMMAND: {
      const int id = LOWORD(wParam);
      if (!g_compat1715Controller) return 0;
      if (id == kCompat1715BtnConnect) {
        g_compat1715Controller->Connect();
      } else if (id == kCompat1715BtnDisconnect) {
        g_compat1715Controller->Disconnect();
      } else if (id == kCompat1715BtnExit) {
        DestroyWindow(hwnd);
      }
      return 0;
    }
    case WM_DESTROY:
      if (g_compat1715Controller) g_compat1715Controller->Disconnect();
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}
} // namespace

int RunMainDialogCompat1715(HINSTANCE hInstance, int nCmdShow) {
  g_compat1715Controller = std::make_unique<AppController>(
      std::filesystem::path(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : ".") / "ScaleLogger");
  g_compat1715Controller->Initialize();

  WNDCLASSW wc{};
  wc.lpfnWndProc = MainCompat1715WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"ScaleLoggerMainWindowCompat1715";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"ScaleLogger", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 620, 170, nullptr, nullptr, hInstance, nullptr);

  if (!hwnd) return 1;

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  g_compat1715Controller.reset();
  return static_cast<int>(msg.wParam);
}

} // namespace scalelogger
#endif
