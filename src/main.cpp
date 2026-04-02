#include <memory>
#include <string>

#include <windows.h>

#include "AppController.hpp"
#include "MainWindow.hpp"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    try {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring path = exePath;
        const auto slash = path.find_last_of(L"\\/");
        const std::wstring exeDir = (slash == std::wstring::npos) ? L"." : path.substr(0, slash);

        auto controller = std::make_shared<AppController>();
        if (!controller->Initialize(exeDir)) {
            MessageBoxW(nullptr, L"Failed to initialize ScaleLogger", L"Error", MB_OK | MB_ICONERROR);
            return 1;
        }

        MainWindow mainWindow(controller);
        if (!mainWindow.Create(instance)) {
            MessageBoxW(nullptr, L"Failed to create main window", L"Error", MB_OK | MB_ICONERROR);
            return 1;
        }
        return mainWindow.Run();
    } catch (...) {
        MessageBoxW(nullptr, L"Fatal startup error. See %TEMP%\\ScaleLogger_fatal.log", L"ScaleLogger", MB_OK | MB_ICONERROR);
        return 2;
    }
}
