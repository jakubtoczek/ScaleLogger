#include "ui/MainDialog2.hpp"

#ifdef _WIN32

namespace scalelogger {
namespace {
constexpr int kDialog2BtnConnect = 2001;
constexpr int kDialog2BtnDisconnect = 2002;
constexpr int kDialog2BtnExit = 2003;
constexpr int kDialog2StatusLabel = 2004;
constexpr int kDialog2EditInput = 2005;
constexpr UINT kDialog2UiReadyMsg = WM_APP + 201;
constexpr int kDialog2OrderlyExitCode = 980;

void SetStatusText(HWND hwnd, const wchar_t* text) {
  const HWND status = GetDlgItem(hwnd, kDialog2StatusLabel);
  if (status) SetWindowTextW(status, text);
}

LRESULT CALLBACK MainDialog2WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  (void)lParam;
  switch (msg) {
    case WM_CREATE: {
      CreateWindowW(L"STATIC", L"ScaleLogger Dialog2 (clean-room app shell)", WS_VISIBLE | WS_CHILD, 16, 14, 420, 22, hwnd, nullptr, nullptr, nullptr);
      CreateWindowW(L"STATIC", L"Status: Idle", WS_VISIBLE | WS_CHILD, 16, 42, 420, 22, hwnd, reinterpret_cast<HMENU>(kDialog2StatusLabel), nullptr,
                    nullptr);
      CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 16, 72, 420, 26, hwnd,
                      reinterpret_cast<HMENU>(kDialog2EditInput), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Connect", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 16, 110, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kDialog2BtnConnect), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Disconnect", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 152, 110, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kDialog2BtnDisconnect), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Exit", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 288, 110, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kDialog2BtnExit), nullptr, nullptr);
      PostMessageW(hwnd, kDialog2UiReadyMsg, 0, 0);
      return 0;
    }
    case WM_COMMAND: {
      const int id = LOWORD(wParam);
      if (id == kDialog2BtnConnect) {
        SetStatusText(hwnd, L"Status: Connect clicked (dummy)");
        return 0;
      }
      if (id == kDialog2BtnDisconnect) {
        SetStatusText(hwnd, L"Status: Disconnect clicked (dummy)");
        return 0;
      }
      if (id == kDialog2BtnExit) {
        DestroyWindow(hwnd);
        return 0;
      }
      return 0;
    }
    case kDialog2UiReadyMsg:
      SetStatusText(hwnd, L"Status: UI ready (posted message)");
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}
} // namespace

int RunMainDialog2(HINSTANCE hInstance, int nCmdShow) {
  WNDCLASSW wc{};
  wc.lpfnWndProc = MainDialog2WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"ScaleLoggerMainDialog2Window";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"ScaleLogger", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 470, 200, nullptr, nullptr, hInstance, nullptr);
  if (!hwnd) return 981;

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return kDialog2OrderlyExitCode;
}

} // namespace scalelogger
#endif
