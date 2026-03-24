#include "ui/MainDialog.hpp"

#ifdef _WIN32
#include "app/AppController.hpp"

#include <cstdlib>
#include <filesystem>
#include <memory>

namespace scalelogger {
namespace {
constexpr int kBtnConnect = 1001;
constexpr int kBtnDisconnect = 1002;
constexpr int kBtnExit = 1003;

std::unique_ptr<AppController> g_controller;

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  (void)lParam;
  switch (msg) {
    case WM_CREATE: {
      CreateWindowW(L"BUTTON", L"Connect", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 16, 16, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kBtnConnect), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Disconnect", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 152, 16, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kBtnDisconnect), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Exit", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 288, 16, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kBtnExit), nullptr, nullptr);
      CreateWindowW(L"STATIC", L"ScaleLogger runtime test shell\nUse Settings JSON/presets under %LOCALAPPDATA%\\ScaleLogger.",
                    WS_VISIBLE | WS_CHILD, 16, 64, 560, 48, hwnd, nullptr, nullptr, nullptr);
      return 0;
    }
    case WM_COMMAND: {
      const int id = LOWORD(wParam);
      if (!g_controller) return 0;
      if (id == kBtnConnect) {
        g_controller->Connect();
      } else if (id == kBtnDisconnect) {
        g_controller->Disconnect();
      } else if (id == kBtnExit) {
        DestroyWindow(hwnd);
      }
      return 0;
    }
    case WM_DESTROY:
      if (g_controller) g_controller->Disconnect();
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}
} // namespace

int RunMainDialog(HINSTANCE hInstance, int nCmdShow) {
  g_controller = std::make_unique<AppController>(
      std::filesystem::path(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : ".") / "ScaleLogger");
  g_controller->Initialize();

  WNDCLASSW wc{};
  wc.lpfnWndProc = MainWndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"ScaleLoggerMainWindow";
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

  g_controller.reset();
  return static_cast<int>(msg.wParam);
}

} // namespace scalelogger
#endif
