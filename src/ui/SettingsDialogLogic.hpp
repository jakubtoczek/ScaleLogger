#pragma once

#ifdef _WIN32

#include "app/AppController.hpp"

#include <Windows.h>

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace scalelogger::settingslogic {

struct Context {
  AppController* controller{nullptr};
  HWND mainWindow{nullptr};
  HWND settingsWindow{nullptr};
  std::unordered_map<std::string, std::string>* portDisplayToPort{nullptr};

  std::function<void(const std::string&)> addLogLine;
  std::function<std::wstring(std::string_view)> toWide;
  std::function<std::string(const std::wstring&)> toUtf8;
  std::function<std::wstring(HWND)> getControlText;
  std::function<void(HWND, const std::wstring&)> setComboToText;
  std::function<void(HWND)> loadSettingsIntoControls;
  std::function<void(HWND)> updateCustomSequenceUiState;
  std::function<void(HWND)> updateParseControlsUiState;
};

void LoadSettingsIntoControls(const Context& ctx, HWND settingsHwnd);
bool ReadSerialSettingsFromControls(const Context& ctx, HWND settingsHwnd, AppSettings& settingsOut, std::string& error);
void ApplySettingsFromControls(const Context& ctx, HWND settingsHwnd, bool saveRequested = false);

} // namespace scalelogger::settingslogic

#endif
