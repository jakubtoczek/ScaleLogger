#pragma once

#include <windows.h>

#include "Types.hpp"

class SettingsWindow {
public:
    bool ShowModal(HINSTANCE instance, HWND owner, AppConfig& config);
};
