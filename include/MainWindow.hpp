#pragma once

#include <memory>

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
    void UpdateStatus(HealthState state, const std::wstring& text);
    void OpenSettings();

    std::shared_ptr<AppController> controller_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND statusLabel_ = nullptr;
    HWND logEdit_ = nullptr;
};
