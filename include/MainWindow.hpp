#pragma once

#include <memory>
#include <string>

#include <windows.h>

#include "AppController.hpp"

class MainWindow {
public:
    explicit MainWindow(std::shared_ptr<AppController> controller);
    bool Create(HINSTANCE instance);
    int Run();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void AppendLog(const std::wstring& line);
    void AppendLogUi(const std::wstring& line);
    void UpdateStatus(HealthState state, const std::wstring& text);
    void UpdateStatusUi(HealthState state, const std::wstring& text);
    void ShowInfo(const std::wstring& title, const std::wstring& message);
    void OpenSettings();

    static constexpr UINT WM_APP_APPEND_LOG = WM_APP + 101;
    static constexpr UINT WM_APP_UPDATE_STATUS = WM_APP + 102;
    static constexpr UINT WM_APP_SCAN_RESULT = WM_APP + 103;
    static constexpr UINT WM_APP_TEST_RESULT = WM_APP + 104;

    std::shared_ptr<AppController> controller_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND statusLabel_ = nullptr;
    HWND logEdit_ = nullptr;
    HWND connectButton_ = nullptr;
};
