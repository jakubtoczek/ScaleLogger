#include "app/MinEntryTinyWindow.hpp"

#ifdef _WIN32
namespace scalelogger {
namespace {
LRESULT CALLBACK TinyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_CLOSE) {
    DestroyWindow(hwnd);
    return 0;
  }
  if (msg == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
} // namespace

int LaunchTinyWindow(HINSTANCE hInstance, int nCmdShow) {
  OutputDebugStringA("TRACE: TinyWindow entered\n");
  WNDCLASSW wc{};
  wc.lpfnWndProc = TinyWndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"ScaleLoggerTinyWindow";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);
  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"ScaleLogger Tiny", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              CW_USEDEFAULT, CW_USEDEFAULT, 320, 120, nullptr, nullptr, hInstance, nullptr);
  if (!hwnd) return 970;
  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);
  OutputDebugStringA("TRACE: TinyWindow success\n");
  PostMessageW(hwnd, WM_CLOSE, 0, 0);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 970;
}
} // namespace scalelogger
#endif
