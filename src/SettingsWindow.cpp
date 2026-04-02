#include "SettingsWindow.hpp"

#include <string>

namespace {
constexpr int IDC_PORT = 2001;
constexpr int IDC_BAUD = 2002;
constexpr int IDC_DATABITS = 2003;
constexpr int IDC_PARITY = 2004;
constexpr int IDC_STOPBITS = 2005;
constexpr int IDC_TIMEOUT = 2006;
constexpr int IDC_EOL = 2007;
constexpr int IDC_DRYRUN = 2008;
constexpr int IDC_SUFFIX = 2009;
constexpr int IDC_CONNECTSTART = 2010;
constexpr int IDC_POSTACTION = 2011;
constexpr int IDC_OK = 2012;
constexpr int IDC_CANCEL = 2013;

const wchar_t* kWndClass = L"ScaleLoggerSettingsWnd";

struct SettingsContext {
    AppConfig working;
    bool accepted = false;
    HWND hwnd = nullptr;
};

std::wstring ToPostAction(PostAction action) {
    switch (action) {
        case PostAction::None: return L"None";
        case PostAction::Down: return L"Down";
        case PostAction::Right: return L"Right";
        case PostAction::Enter: return L"Enter";
        case PostAction::Tab: return L"Tab";
        case PostAction::Custom: return L"Custom";
    }
    return L"Down";
}

PostAction ParsePostAction(const std::wstring& value) {
    if (value == L"None") return PostAction::None;
    if (value == L"Down") return PostAction::Down;
    if (value == L"Right") return PostAction::Right;
    if (value == L"Enter") return PostAction::Enter;
    if (value == L"Tab") return PostAction::Tab;
    if (value == L"Custom") return PostAction::Custom;
    return PostAction::Down;
}

std::wstring GetCtrlText(HWND hwnd, int id) {
    wchar_t buf[256] = {};
    GetWindowTextW(GetDlgItem(hwnd, id), buf, 256);
    return buf;
}

void AddComboPreset(HWND hwnd, std::initializer_list<const wchar_t*> values) {
    for (auto* value : values) {
        SendMessageW(hwnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* ctx = reinterpret_cast<SettingsContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }

    switch (msg) {
        case WM_CREATE: {
            ctx = reinterpret_cast<SettingsContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            ctx->hwnd = hwnd;

            auto mkLabel = [&](const wchar_t* text, int x, int y) {
                CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, 130, 20, hwnd, nullptr, nullptr, nullptr);
            };
            auto mkEdit = [&](int id, int x, int y, const std::wstring& value) {
                auto h = CreateWindowW(L"COMBOBOX", value.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN,
                    x, y, 180, 300, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                return h;
            };

            mkLabel(L"COM port", 20, 20);
            auto hPort = mkEdit(IDC_PORT, 160, 20, ctx->working.serial.port);
            AddComboPreset(hPort, {L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6"});

            mkLabel(L"Baud rate", 20, 50);
            auto hBaud = mkEdit(IDC_BAUD, 160, 50, std::to_wstring(ctx->working.serial.baudRate));
            AddComboPreset(hBaud, {L"1200", L"2400", L"4800", L"9600", L"19200"});

            mkLabel(L"Data bits", 20, 80);
            auto hDataBits = mkEdit(IDC_DATABITS, 160, 80, std::to_wstring(ctx->working.serial.dataBits));
            AddComboPreset(hDataBits, {L"7", L"8"});

            mkLabel(L"Parity", 20, 110);
            auto hParity = mkEdit(IDC_PARITY, 160, 110, std::to_wstring(ctx->working.serial.parity));
            AddComboPreset(hParity, {L"0", L"1", L"2"});

            mkLabel(L"Stop bits", 20, 140);
            auto hStopBits = mkEdit(IDC_STOPBITS, 160, 140, std::to_wstring(ctx->working.serial.stopBits));
            AddComboPreset(hStopBits, {L"1", L"2"});

            mkLabel(L"Timeout ms", 20, 170);
            CreateWindowW(L"EDIT", std::to_wstring(ctx->working.serial.timeoutMs).c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                160, 170, 180, 22, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TIMEOUT)), nullptr, nullptr);

            mkLabel(L"Line ending", 20, 200);
            auto hEol = mkEdit(IDC_EOL, 160, 200, ctx->working.serial.eol);
            AddComboPreset(hEol, {L"\\r\\n", L"\\n", L"\\r"});

            mkLabel(L"Suffix", 20, 230);
            CreateWindowW(L"EDIT", ctx->working.parse.suffix.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                160, 230, 180, 22, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SUFFIX)), nullptr, nullptr);

            mkLabel(L"Post action", 20, 260);
            auto hPost = mkEdit(IDC_POSTACTION, 160, 260, ToPostAction(ctx->working.output.postAction));
            AddComboPreset(hPost, {L"None", L"Down", L"Right", L"Enter", L"Tab", L"Custom"});

            CreateWindowW(L"BUTTON", L"Dry run", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                20, 294, 120, 22, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DRYRUN)), nullptr, nullptr);
            if (ctx->working.output.dryRun) SendMessageW(GetDlgItem(hwnd, IDC_DRYRUN), BM_SETCHECK, BST_CHECKED, 0);

            CreateWindowW(L"BUTTON", L"Connect on startup", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                160, 294, 180, 22, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CONNECTSTART)), nullptr, nullptr);
            if (ctx->working.connectOnStartup) SendMessageW(GetDlgItem(hwnd, IDC_CONNECTSTART), BM_SETCHECK, BST_CHECKED, 0);

            CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                160, 330, 80, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_OK)), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                260, 330, 80, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CANCEL)), nullptr, nullptr);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_CANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_OK) {
                try {
                    const auto port = GetCtrlText(hwnd, IDC_PORT);
                    const auto baud = std::stoi(GetCtrlText(hwnd, IDC_BAUD));
                    const auto dataBits = std::stoi(GetCtrlText(hwnd, IDC_DATABITS));
                    const auto parity = std::stoi(GetCtrlText(hwnd, IDC_PARITY));
                    const auto stopBits = std::stoi(GetCtrlText(hwnd, IDC_STOPBITS));
                    const auto timeout = std::stoi(GetCtrlText(hwnd, IDC_TIMEOUT));
                    const auto eol = GetCtrlText(hwnd, IDC_EOL);
                    const auto suffix = GetCtrlText(hwnd, IDC_SUFFIX);
                    const auto post = ParsePostAction(GetCtrlText(hwnd, IDC_POSTACTION));

                    if (port.empty() || timeout < 50 || eol.empty()) {
                        MessageBoxW(hwnd, L"Invalid settings values.", L"Settings", MB_OK | MB_ICONWARNING);
                        return 0;
                    }

                    ctx->working.serial.port = port;
                    ctx->working.serial.baudRate = baud;
                    ctx->working.serial.dataBits = dataBits;
                    ctx->working.serial.parity = parity;
                    ctx->working.serial.stopBits = stopBits;
                    ctx->working.serial.timeoutMs = timeout;
                    ctx->working.serial.eol = eol;
                    ctx->working.parse.suffix = suffix;
                    ctx->working.output.postAction = post;
                    ctx->working.output.dryRun = SendMessageW(GetDlgItem(hwnd, IDC_DRYRUN), BM_GETCHECK, 0, 0) == BST_CHECKED;
                    ctx->working.connectOnStartup = SendMessageW(GetDlgItem(hwnd, IDC_CONNECTSTART), BM_GETCHECK, 0, 0) == BST_CHECKED;
                    ctx->accepted = true;
                    DestroyWindow(hwnd);
                    return 0;
                } catch (...) {
                    MessageBoxW(hwnd, L"One or more numeric fields are invalid.", L"Settings", MB_OK | MB_ICONWARNING);
                    return 0;
                }
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}

bool SettingsWindow::ShowModal(HINSTANCE instance, HWND owner, AppConfig& config) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWndClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    SettingsContext ctx;
    ctx.working = config;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kWndClass, L"ScaleLogger Settings",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 390, 410,
        owner, nullptr, instance, &ctx);

    if (!hwnd) {
        return false;
    }

    EnableWindow(owner, FALSE);
    MSG msg{};
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);

    if (ctx.accepted) {
        config = ctx.working;
    }
    return ctx.accepted;
}
