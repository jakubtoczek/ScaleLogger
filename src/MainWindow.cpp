#include "MainWindow.hpp"

#include <memory>
#include <sstream>

#include "SettingsWindow.hpp"

namespace {
constexpr int ID_CONNECT = 1001;
constexpr int ID_SETTINGS = 1002;
constexpr int ID_ABOUT = 1003;
constexpr int ID_LOG = 1004;
constexpr int ID_STATUS = 1005;
}

MainWindow::MainWindow(std::shared_ptr<AppController> controller) : controller_(std::move(controller)) {}

bool MainWindow::Create(HINSTANCE instance) {
    instance_ = instance;
    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"ScaleLoggerMainWnd";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"ScaleLogger", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 650, nullptr, nullptr, instance, this);
    return hwnd_ != nullptr;
}

int MainWindow::Run() {
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            statusLabel_ = CreateWindowW(L"STATIC", L"Status: Disconnected", WS_CHILD | WS_VISIBLE,
                10, 10, 500, 24, hwnd_, reinterpret_cast<HMENU>(ID_STATUS), instance_, nullptr);
            CreateWindowW(L"BUTTON", L"Connect", WS_CHILD | WS_VISIBLE,
                600, 10, 90, 28, hwnd_, reinterpret_cast<HMENU>(ID_CONNECT), instance_, nullptr);
            CreateWindowW(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE,
                700, 10, 90, 28, hwnd_, reinterpret_cast<HMENU>(ID_SETTINGS), instance_, nullptr);
            CreateWindowW(L"BUTTON", L"About", WS_CHILD | WS_VISIBLE,
                800, 10, 80, 28, hwnd_, reinterpret_cast<HMENU>(ID_ABOUT), instance_, nullptr);
            logEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                10, 45, 870, 550, hwnd_, reinterpret_cast<HMENU>(ID_LOG), instance_, nullptr);

            controller_->SetUiLogSink([this](const std::wstring& line) { AppendLog(line); });
            controller_->SetStatusSink([this](HealthState state, const std::wstring& text) { UpdateStatus(state, text); });
            return 0;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_CONNECT:
                    controller_->ToggleConnection();
                    return 0;
                case ID_SETTINGS:
                    OpenSettings();
                    return 0;
                case ID_ABOUT:
                    MessageBoxW(hwnd_, L"ScaleLogger clean reimplementation\nNative Win32/C++20", L"About", MB_OK | MB_ICONINFORMATION);
                    return 0;
                default:
                    break;
            }
            break;
        }
        case WM_DESTROY:
            controller_->Disconnect();
            PostQuitMessage(0);
            return 0;
        case WM_APP_APPEND_LOG: {
            std::unique_ptr<std::wstring> payload(reinterpret_cast<std::wstring*>(lParam));
            if (payload) {
                AppendLogUi(*payload);
            }
            return 0;
        }
        case WM_APP_UPDATE_STATUS: {
            std::unique_ptr<std::wstring> payload(reinterpret_cast<std::wstring*>(lParam));
            const auto state = static_cast<HealthState>(wParam);
            if (payload) {
                UpdateStatusUi(state, *payload);
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void MainWindow::AppendLog(const std::wstring& line) {
    if (!hwnd_) return;
    auto* payload = new std::wstring(line);
    PostMessageW(hwnd_, WM_APP_APPEND_LOG, 0, reinterpret_cast<LPARAM>(payload));
}

void MainWindow::AppendLogUi(const std::wstring& line) {
    if (!logEdit_) return;
    const int length = GetWindowTextLengthW(logEdit_);
    SendMessageW(logEdit_, EM_SETSEL, length, length);
    const std::wstring text = line + L"\r\n";
    SendMessageW(logEdit_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
}

void MainWindow::UpdateStatus(HealthState state, const std::wstring& text) {
    if (!hwnd_) return;
    auto* payload = new std::wstring(text);
    PostMessageW(hwnd_, WM_APP_UPDATE_STATUS, static_cast<WPARAM>(state), reinterpret_cast<LPARAM>(payload));
}

void MainWindow::UpdateStatusUi(HealthState state, const std::wstring& text) {
    std::wstring dot = L"\x25CF";
    if (state == HealthState::Active) dot = L"\x1F7E2";
    else if (state == HealthState::Idle) dot = L"\x1F7E0";
    else if (state == HealthState::Error) dot = L"\x1F534";
    else dot = L"\x1F534";
    SetWindowTextW(statusLabel_, (dot + L" Status: " + text).c_str());
}

void MainWindow::OpenSettings() {
    SettingsWindow dlg;
    auto cfg = controller_->Config();
    if (dlg.ShowModal(instance_, hwnd_, cfg)) {
        controller_->UpdateConfig(cfg);
        AppendLog(L"Settings applied");
    }
}
