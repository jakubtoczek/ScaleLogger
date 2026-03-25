#include "ui/MainDialog.hpp"

#ifdef _WIN32
#include "app/AppController.hpp"
#include "ui/AboutDialog.hpp"

#include <CommCtrl.h>
#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#pragma comment(lib, "Comctl32.lib")

namespace scalelogger {
namespace {
constexpr int kComboPresets = 100;
constexpr int kBtnRefreshPresets = 101;
constexpr int kLblConnectionStatus = 102;
constexpr int kBtnConnect = 103;
constexpr int kBtnSettings = 104;
constexpr int kBtnAbout = 105;
constexpr int kEditLog = 106;

constexpr int kSettingsTab = 200;
constexpr int kSettingsApply = 201;
constexpr int kSettingsCancel = 202;
constexpr int kSettingsSaveConfig = 203;
constexpr int kSettingsSavePreset = 204;

constexpr int kSerialPortCombo = 300;
constexpr int kSerialScanBtn = 301;
constexpr int kSerialTestBtn = 302;
constexpr int kSerialBaudCombo = 303;
constexpr int kSerialDataBitsCombo = 304;
constexpr int kSerialParityCombo = 305;
constexpr int kSerialStopBitsCombo = 306;
constexpr int kSerialTimeoutCombo = 307;
constexpr int kSerialEolCombo = 308;

constexpr int kOutputModeCombo = 400;
constexpr int kOutputTrimCheck = 401;
constexpr int kOutputStripSuffixCheck = 402;
constexpr int kOutputSuffixEdit = 403;
constexpr int kOutputNormalizeCheck = 404;
constexpr int kOutputPreservePlusCheck = 405;
constexpr int kOutputRequireNumericCheck = 406;
constexpr int kOutputActionCombo = 407;
constexpr int kOutputCustomSequenceEdit = 408;

constexpr int kAppPresetsFolderEdit = 500;
constexpr int kAppLogsFolderEdit = 501;
constexpr int kAppLogModeCombo = 502;
constexpr int kAppConnectStartupCheck = 503;
constexpr int kAppStartupPresetCombo = 504;
constexpr int kAppPathsLabel = 505;
constexpr wchar_t kSettingsWindowClassName[] = L"ScaleLoggerSettingsWindow";

struct UiState {
  std::unique_ptr<AppController> controller;
  HINSTANCE hInstance{nullptr};
  HWND mainWindow{nullptr};
  HWND presetsLabel{nullptr};
  HWND presetsCombo{nullptr};
  HWND refreshButton{nullptr};
  HWND settingsButton{nullptr};
  HWND aboutButton{nullptr};
  HWND connectButton{nullptr};
  HWND connectionStatus{nullptr};
  HWND logEdit{nullptr};
  HWND settingsWindow{nullptr};
  HWND settingsTab{nullptr};
  bool settingsClassRegistered{false};
  std::vector<HWND> serialTabControls{};
  std::vector<HWND> outputTabControls{};
  std::vector<HWND> applicationTabControls{};
};

UiState g_ui;

std::wstring ToWide(std::string_view text) { return std::wstring(text.begin(), text.end()); }

std::string ToUtf8(const std::wstring& text) { return std::string(text.begin(), text.end()); }

void AddLogLine(const std::string& text) {
  if (!g_ui.logEdit) return;

  SYSTEMTIME now{};
  GetLocalTime(&now);
  std::ostringstream stamp;
  stamp << '[' << std::setfill('0') << std::setw(2) << now.wHour << ':' << std::setw(2) << now.wMinute << ':' << std::setw(2)
        << now.wSecond << "] " << text << "\r\n";

  const auto lineW = ToWide(stamp.str());
  const auto currentLen = GetWindowTextLengthW(g_ui.logEdit);
  SendMessageW(g_ui.logEdit, EM_SETSEL, currentLen, currentLen);
  SendMessageW(g_ui.logEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(lineW.c_str()));
}

void UpdateConnectionUi(bool connected) {
  if (!g_ui.connectButton || !g_ui.connectionStatus) return;
  SetWindowTextW(g_ui.connectButton, connected ? L"Disconnect" : L"Connect");
  SetWindowTextW(g_ui.connectionStatus, connected ? L"Connected" : L"Disconnected");
}

std::string DescribeLastErrorEnglish(DWORD error) {
  switch (error) {
    case ERROR_CLASS_DOES_NOT_EXIST: return "Window class is not registered.";
    case ERROR_CANNOT_FIND_WND_CLASS: return "Window class cannot be found.";
    case ERROR_INVALID_WINDOW_HANDLE: return "Invalid window handle.";
    case ERROR_NOT_ENOUGH_MEMORY: return "Not enough memory to create window.";
    case ERROR_OUTOFMEMORY: return "Out of memory.";
    case 0: return "No Win32 error code reported by the failing call.";
    default: return "Win32 error code " + std::to_string(error) + ".";
  }
}

void LayoutMainControls(HWND hwnd) {
  RECT rc{};
  GetClientRect(hwnd, &rc);

  const int margin = 16;
  const int top = 14;
  const int rowH = 28;
  const int gap = 8;

  int left = margin;
  MoveWindow(g_ui.presetsLabel, left, top + 3, 58, 22, TRUE);
  left += 64;
  MoveWindow(g_ui.presetsCombo, left, top, 250, 300, TRUE);
  left += 250 + gap;
  MoveWindow(g_ui.refreshButton, left, top, 72, rowH, TRUE);

  int right = rc.right - margin;
  right -= 64;
  MoveWindow(g_ui.aboutButton, right, top, 64, rowH, TRUE);
  right -= gap + 82;
  MoveWindow(g_ui.settingsButton, right, top, 82, rowH, TRUE);
  right -= gap + 102;
  MoveWindow(g_ui.connectButton, right, top, 102, rowH, TRUE);
  right -= gap + 130;
  MoveWindow(g_ui.connectionStatus, right, top + 4, 130, 22, TRUE);

  MoveWindow(g_ui.logEdit, margin, 52, rc.right - margin * 2, rc.bottom - 68, TRUE);
}

void PopulateComboWithValues(HWND combo, const std::vector<std::wstring>& values) {
  SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  for (const auto& value : values) {
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
  }
  if (!values.empty()) SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

void SetComboToText(HWND combo, const std::wstring& text) {
  const LRESULT idx = SendMessageW(combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(text.c_str()));
  if (idx != CB_ERR) {
    SendMessageW(combo, CB_SETCURSEL, idx, 0);
  } else {
    SetWindowTextW(combo, text.c_str());
  }
}

std::wstring GetControlText(HWND control) {
  const int length = GetWindowTextLengthW(control);
  std::wstring text(length, L'\0');
  GetWindowTextW(control, text.data(), length + 1);
  return text;
}

void ShowTab(std::size_t index) {
  auto applyVisibility = [index](const std::vector<HWND>& controls, std::size_t tabIndex) {
    for (HWND control : controls) ShowWindow(control, index == tabIndex ? SW_SHOW : SW_HIDE);
  };
  applyVisibility(g_ui.serialTabControls, 0);
  applyVisibility(g_ui.outputTabControls, 1);
  applyVisibility(g_ui.applicationTabControls, 2);
}

void LoadSettingsIntoControls(HWND settingsHwnd) {
  const auto& settings = g_ui.controller->Settings();
  const auto& config = g_ui.controller->Config();

  SetComboToText(GetDlgItem(settingsHwnd, kSerialPortCombo), ToWide(settings.serial.port));
  SetComboToText(GetDlgItem(settingsHwnd, kSerialBaudCombo), ToWide(std::to_string(settings.serial.baudRate)));
  SetComboToText(GetDlgItem(settingsHwnd, kSerialDataBitsCombo), ToWide(std::to_string(settings.serial.dataBits)));
  SetComboToText(GetDlgItem(settingsHwnd, kSerialParityCombo), std::wstring(1, static_cast<wchar_t>(settings.serial.parity)));
  SetComboToText(GetDlgItem(settingsHwnd, kSerialStopBitsCombo), ToWide(std::to_string(settings.serial.stopBits)));
  SetComboToText(GetDlgItem(settingsHwnd, kSerialTimeoutCombo), ToWide(std::to_string(settings.serial.timeoutSeconds)));

  if (settings.serial.eol == "\r\n") SetComboToText(GetDlgItem(settingsHwnd, kSerialEolCombo), L"\\r\\n");
  else if (settings.serial.eol == "\n") SetComboToText(GetDlgItem(settingsHwnd, kSerialEolCombo), L"\\n");
  else if (settings.serial.eol == "\r") SetComboToText(GetDlgItem(settingsHwnd, kSerialEolCombo), L"\\r");

  SetComboToText(GetDlgItem(settingsHwnd, kOutputModeCombo), settings.parsing.mode == ParseMode::Parsed ? L"parsed" : L"raw");
  SendMessageW(GetDlgItem(settingsHwnd, kOutputTrimCheck), BM_SETCHECK, settings.parsing.trimWhitespace ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(GetDlgItem(settingsHwnd, kOutputStripSuffixCheck), BM_SETCHECK, settings.parsing.stripSuffix ? BST_CHECKED : BST_UNCHECKED, 0);
  SetWindowTextW(GetDlgItem(settingsHwnd, kOutputSuffixEdit), ToWide(settings.parsing.suffix).c_str());
  SendMessageW(GetDlgItem(settingsHwnd, kOutputNormalizeCheck), BM_SETCHECK, settings.parsing.normalizeSign ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(GetDlgItem(settingsHwnd, kOutputPreservePlusCheck), BM_SETCHECK, settings.parsing.dropPlusSign ? BST_UNCHECKED : BST_CHECKED, 0);
  SendMessageW(GetDlgItem(settingsHwnd, kOutputRequireNumericCheck), BM_SETCHECK, settings.parsing.numericValidation ? BST_CHECKED : BST_UNCHECKED, 0);

  std::wstring action = L"down";
  switch (settings.output.postAction) {
    case PostAction::Right: action = L"right"; break;
    case PostAction::Enter: action = L"enter"; break;
    case PostAction::Tab: action = L"tab"; break;
    case PostAction::None: action = L"none"; break;
    case PostAction::CustomSequence: action = L"custom_sequence"; break;
    case PostAction::Down:
    default: break;
  }
  SetComboToText(GetDlgItem(settingsHwnd, kOutputActionCombo), action);

  std::wstring sequence;
  for (std::size_t i = 0; i < settings.output.customSequence.size(); ++i) {
    if (i) sequence += L",";
    sequence += ToWide(settings.output.customSequence[i]);
  }
  SetWindowTextW(GetDlgItem(settingsHwnd, kOutputCustomSequenceEdit), sequence.c_str());

  SetWindowTextW(GetDlgItem(settingsHwnd, kAppPresetsFolderEdit), ToWide(config.presetsFolder).c_str());
  SetWindowTextW(GetDlgItem(settingsHwnd, kAppLogsFolderEdit), ToWide(config.logsFolder).c_str());
  SetComboToText(GetDlgItem(settingsHwnd, kAppLogModeCombo), config.logMode == LogMode::SingleFile ? L"Single file" : L"New file per session");
  SendMessageW(GetDlgItem(settingsHwnd, kAppConnectStartupCheck), BM_SETCHECK, config.connectOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);
  SetComboToText(GetDlgItem(settingsHwnd, kAppStartupPresetCombo), config.startupMode == "specific_preset" ? ToWide(config.startupPresetName) : L"Last used / defaults");

  const auto dataRoot = g_ui.controller->DataRoot();
  const std::wstring pathSummary = L"Data: " + dataRoot.wstring() + L"\r\nConfig: " + (dataRoot / L"ScaleLogger.config.json").wstring() +
                                   L"\r\nPresets: " + (dataRoot / ToWide(config.presetsFolder)).wstring() +
                                   L"\r\nLogs: " + (dataRoot / ToWide(config.logsFolder)).wstring();
  SetWindowTextW(GetDlgItem(settingsHwnd, kAppPathsLabel), pathSummary.c_str());
}

void ApplySettingsFromControls(HWND settingsHwnd) {
  AppSettings nextSettings = g_ui.controller->Settings();
  AppConfig nextConfig = g_ui.controller->Config();

  nextSettings.serial.port = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kSerialPortCombo)));
  nextSettings.serial.baudRate = std::atoi(ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kSerialBaudCombo))).c_str());
  nextSettings.serial.dataBits = std::atoi(ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kSerialDataBitsCombo))).c_str());
  const auto parityText = GetControlText(GetDlgItem(settingsHwnd, kSerialParityCombo));
  nextSettings.serial.parity = parityText.empty() ? 'O' : static_cast<char>(parityText[0]);
  nextSettings.serial.stopBits = static_cast<float>(std::atof(ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kSerialStopBitsCombo))).c_str()));
  nextSettings.serial.timeoutSeconds =
      static_cast<float>(std::atof(ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kSerialTimeoutCombo))).c_str()));

  const auto eolDisplay = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kSerialEolCombo)));
  if (eolDisplay == "\\r\\n") nextSettings.serial.eol = "\r\n";
  else if (eolDisplay == "\\n") nextSettings.serial.eol = "\n";
  else if (eolDisplay == "\\r") nextSettings.serial.eol = "\r";

  nextSettings.parsing.mode = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kOutputModeCombo))) == "raw" ? ParseMode::Raw : ParseMode::Parsed;
  nextSettings.parsing.trimWhitespace = SendMessageW(GetDlgItem(settingsHwnd, kOutputTrimCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  nextSettings.parsing.stripSuffix = SendMessageW(GetDlgItem(settingsHwnd, kOutputStripSuffixCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  nextSettings.parsing.suffix = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kOutputSuffixEdit)));
  nextSettings.parsing.normalizeSign = SendMessageW(GetDlgItem(settingsHwnd, kOutputNormalizeCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  nextSettings.parsing.dropPlusSign = SendMessageW(GetDlgItem(settingsHwnd, kOutputPreservePlusCheck), BM_GETCHECK, 0, 0) != BST_CHECKED;
  nextSettings.parsing.numericValidation = SendMessageW(GetDlgItem(settingsHwnd, kOutputRequireNumericCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;

  const auto action = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kOutputActionCombo)));
  if (action == "right") nextSettings.output.postAction = PostAction::Right;
  else if (action == "enter") nextSettings.output.postAction = PostAction::Enter;
  else if (action == "tab") nextSettings.output.postAction = PostAction::Tab;
  else if (action == "none") nextSettings.output.postAction = PostAction::None;
  else if (action == "custom_sequence") nextSettings.output.postAction = PostAction::CustomSequence;
  else nextSettings.output.postAction = PostAction::Down;

  nextConfig.presetsFolder = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kAppPresetsFolderEdit)));
  nextConfig.logsFolder = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kAppLogsFolderEdit)));
  nextConfig.logMode = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kAppLogModeCombo))) == "Single file" ? LogMode::SingleFile : LogMode::PerSession;
  nextConfig.connectOnStartup = SendMessageW(GetDlgItem(settingsHwnd, kAppConnectStartupCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;

  const auto startupPreset = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kAppStartupPresetCombo)));
  if (startupPreset == "Last used / defaults") {
    nextConfig.startupMode = "last_used_preset";
    nextConfig.startupPresetName.clear();
  } else {
    nextConfig.startupMode = "specific_preset";
    nextConfig.startupPresetName = startupPreset;
  }

  g_ui.controller->ApplySettings(nextSettings, nextConfig);
  AddLogLine("Settings applied");
}

void CreateTopRow(HWND hwnd) {
  g_ui.presetsLabel = CreateWindowW(L"STATIC", L"Presets", WS_CHILD | WS_VISIBLE, 16, 16, 58, 24, hwnd, nullptr, nullptr, nullptr);
  g_ui.presetsCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 80, 14, 240, 300,
                                    hwnd, reinterpret_cast<HMENU>(kComboPresets), nullptr, nullptr);
  SendMessageW(g_ui.presetsCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Last used / defaults"));
  SendMessageW(g_ui.presetsCombo, CB_SETCURSEL, 0, 0);

  g_ui.refreshButton = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 328, 14, 70, 26, hwnd,
                                     reinterpret_cast<HMENU>(kBtnRefreshPresets), nullptr, nullptr);

  g_ui.connectionStatus = CreateWindowW(L"STATIC", L"Disconnected", WS_CHILD | WS_VISIBLE, 500, 17, 120, 22, hwnd,
                                        reinterpret_cast<HMENU>(kLblConnectionStatus), nullptr, nullptr);
  g_ui.connectButton = CreateWindowW(L"BUTTON", L"Connect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 628, 14, 100, 28, hwnd,
                                     reinterpret_cast<HMENU>(kBtnConnect), nullptr, nullptr);
  g_ui.settingsButton = CreateWindowW(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 736, 14, 82, 28, hwnd,
                                      reinterpret_cast<HMENU>(kBtnSettings), nullptr, nullptr);
  g_ui.aboutButton = CreateWindowW(L"BUTTON", L"About", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 822, 14, 64, 28, hwnd,
                                   reinterpret_cast<HMENU>(kBtnAbout), nullptr, nullptr);
}

void CreateLogPane(HWND hwnd) {
  g_ui.logEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                                                                      ES_READONLY,
                                 16, 52, 840, 530, hwnd, reinterpret_cast<HMENU>(kEditLog), nullptr, nullptr);
}

void AddControl(std::vector<HWND>& tabControls, HWND control) { tabControls.push_back(control); }

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      g_ui.settingsTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 12, 12, 840, 500, hwnd,
                                         reinterpret_cast<HMENU>(kSettingsTab), nullptr, nullptr);
      TCITEMW item{};
      item.mask = TCIF_TEXT;
      item.pszText = const_cast<LPWSTR>(L"Serial");
      TabCtrl_InsertItem(g_ui.settingsTab, 0, &item);
      item.pszText = const_cast<LPWSTR>(L"Output");
      TabCtrl_InsertItem(g_ui.settingsTab, 1, &item);
      item.pszText = const_cast<LPWSTR>(L"Application");
      TabCtrl_InsertItem(g_ui.settingsTab, 2, &item);

      const int left = 26;
      const int top = 58;
      const int labelWidth = 110;
      const int fieldLeft = left + labelWidth;

      auto label = [&](const wchar_t* text, int y, std::vector<HWND>& list) {
        AddControl(list, CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, left, y + 2, labelWidth, 22, hwnd, nullptr, nullptr, nullptr));
      };
      auto combo = [&](int id, int y, int width, std::vector<HWND>& list) {
        HWND c = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, fieldLeft, y, width, 300, hwnd,
                               reinterpret_cast<HMENU>(id), nullptr, nullptr);
        AddControl(list, c);
        return c;
      };

      label(L"Port", top, g_ui.serialTabControls);
      HWND port = combo(kSerialPortCombo, top, 430, g_ui.serialTabControls);
      SendMessageW(port, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"COM6"));
      AddControl(g_ui.serialTabControls, CreateWindowW(L"BUTTON", L"Scan Ports", WS_CHILD | WS_VISIBLE, fieldLeft + 440, top, 96, 24, hwnd,
                                                       reinterpret_cast<HMENU>(kSerialScanBtn), nullptr, nullptr));
      AddControl(g_ui.serialTabControls, CreateWindowW(L"BUTTON", L"Test Receive", WS_CHILD | WS_VISIBLE, fieldLeft + 545, top, 100, 24, hwnd,
                                                       reinterpret_cast<HMENU>(kSerialTestBtn), nullptr, nullptr));

      label(L"Baud", top + 36, g_ui.serialTabControls);
      HWND baud = combo(kSerialBaudCombo, top + 36, 645, g_ui.serialTabControls);
      for (const wchar_t* value : {L"1200", L"2400", L"4800", L"9600"}) SendMessageW(baud, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      label(L"Data bits", top + 72, g_ui.serialTabControls);
      HWND bits = combo(kSerialDataBitsCombo, top + 72, 645, g_ui.serialTabControls);
      for (const wchar_t* value : {L"7", L"8"}) SendMessageW(bits, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      label(L"Parity", top + 108, g_ui.serialTabControls);
      HWND parity = combo(kSerialParityCombo, top + 108, 645, g_ui.serialTabControls);
      for (const wchar_t* value : {L"O", L"N", L"E"}) SendMessageW(parity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      label(L"Stop bits", top + 144, g_ui.serialTabControls);
      HWND stopBits = combo(kSerialStopBitsCombo, top + 144, 645, g_ui.serialTabControls);
      for (const wchar_t* value : {L"1", L"1.5", L"2"}) SendMessageW(stopBits, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      label(L"Timeout", top + 180, g_ui.serialTabControls);
      HWND timeout = combo(kSerialTimeoutCombo, top + 180, 645, g_ui.serialTabControls);
      for (const wchar_t* value : {L"0.50", L"1.00", L"2.00"}) SendMessageW(timeout, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      label(L"Line ending", top + 216, g_ui.serialTabControls);
      HWND eol = combo(kSerialEolCombo, top + 216, 645, g_ui.serialTabControls);
      for (const wchar_t* value : {L"\\r\\n", L"\\n", L"\\r"}) SendMessageW(eol, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      label(L"Mode", top + 10, g_ui.outputTabControls);
      HWND mode = combo(kOutputModeCombo, top + 8, 645, g_ui.outputTabControls);
      for (const wchar_t* value : {L"parsed", L"raw"}) SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      auto checkbox = [&](const wchar_t* text, int id, int y) {
        AddControl(g_ui.outputTabControls, CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, left + 8, y, 220, 22, hwnd,
                                                         reinterpret_cast<HMENU>(id), nullptr, nullptr));
      };
      checkbox(L"Trim whitespace", kOutputTrimCheck, top + 46);
      checkbox(L"Strip suffix", kOutputStripSuffixCheck, top + 74);
      label(L"Known suffix", top + 102, g_ui.outputTabControls);
      AddControl(g_ui.outputTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, fieldLeft, top + 100, 645, 24, hwnd,
                                 reinterpret_cast<HMENU>(kOutputSuffixEdit), nullptr, nullptr));
      checkbox(L"Normalize sign spacing", kOutputNormalizeCheck, top + 132);
      checkbox(L"Preserve leading +", kOutputPreservePlusCheck, top + 160);
      checkbox(L"Require numeric result", kOutputRequireNumericCheck, top + 188);
      label(L"After-send action", top + 220, g_ui.outputTabControls);
      HWND postAction = combo(kOutputActionCombo, top + 218, 645, g_ui.outputTabControls);
      for (const wchar_t* value : {L"down", L"right", L"enter", L"tab", L"none", L"custom_sequence"}) {
        SendMessageW(postAction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
      }
      label(L"Custom sequence", top + 256, g_ui.outputTabControls);
      AddControl(g_ui.outputTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, fieldLeft, top + 254, 645, 24, hwnd,
                                 reinterpret_cast<HMENU>(kOutputCustomSequenceEdit), nullptr, nullptr));

      label(L"Presets folder", top + 2, g_ui.applicationTabControls);
      AddControl(g_ui.applicationTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, fieldLeft, top, 645, 24, hwnd,
                                 reinterpret_cast<HMENU>(kAppPresetsFolderEdit), nullptr, nullptr));
      label(L"Logs folder", top + 38, g_ui.applicationTabControls);
      AddControl(g_ui.applicationTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, fieldLeft, top + 36, 645, 24, hwnd,
                                 reinterpret_cast<HMENU>(kAppLogsFolderEdit), nullptr, nullptr));
      label(L"Log mode", top + 74, g_ui.applicationTabControls);
      HWND logMode = combo(kAppLogModeCombo, top + 72, 645, g_ui.applicationTabControls);
      for (const wchar_t* value : {L"New file per session", L"Single file"}) SendMessageW(logMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      AddControl(g_ui.applicationTabControls,
                 CreateWindowW(L"BUTTON", L"Connect on startup", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, left + 8, top + 106, 220, 24, hwnd,
                               reinterpret_cast<HMENU>(kAppConnectStartupCheck), nullptr, nullptr));
      label(L"Startup preset", top + 140, g_ui.applicationTabControls);
      HWND startup = combo(kAppStartupPresetCombo, top + 138, 645, g_ui.applicationTabControls);
      SendMessageW(startup, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Last used / defaults"));

      AddControl(g_ui.applicationTabControls,
                 CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, left + 10, top + 176, 760, 90, hwnd,
                               reinterpret_cast<HMENU>(kAppPathsLabel), nullptr, nullptr));

      CreateWindowW(L"BUTTON", L"Save Configuration", WS_CHILD | WS_VISIBLE, 20, 520, 140, 32, hwnd,
                    reinterpret_cast<HMENU>(kSettingsSaveConfig), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Save as Preset", WS_CHILD | WS_VISIBLE, 170, 520, 120, 32, hwnd,
                    reinterpret_cast<HMENU>(kSettingsSavePreset), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE, 700, 520, 70, 32, hwnd, reinterpret_cast<HMENU>(kSettingsApply), nullptr,
                    nullptr);
      CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 780, 520, 70, 32, hwnd, reinterpret_cast<HMENU>(kSettingsCancel), nullptr,
                    nullptr);

      ShowTab(0);
      LoadSettingsIntoControls(hwnd);
      return 0;
    }
    case WM_NOTIFY: {
      auto* header = reinterpret_cast<LPNMHDR>(lParam);
      if (header && header->idFrom == kSettingsTab && header->code == TCN_SELCHANGE) {
        ShowTab(static_cast<std::size_t>(TabCtrl_GetCurSel(g_ui.settingsTab)));
      }
      return 0;
    }
    case WM_COMMAND: {
      switch (LOWORD(wParam)) {
        case kSettingsApply:
        case kSettingsSaveConfig:
        case kSettingsSavePreset:
          ApplySettingsFromControls(hwnd);
          if (LOWORD(wParam) != kSettingsApply) AddLogLine("Preset/config action routed through Apply in this native pass");
          return 0;
        case kSettingsCancel:
          DestroyWindow(hwnd);
          return 0;
        case kSerialScanBtn:
          AddLogLine("Port scan requested (using current native serial backend)");
          return 0;
        case kSerialTestBtn:
          AddLogLine("Test Receive requested");
          return 0;
        default:
          return 0;
      }
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      g_ui.settingsWindow = nullptr;
      g_ui.serialTabControls.clear();
      g_ui.outputTabControls.clear();
      g_ui.applicationTabControls.clear();
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

void OpenSettingsWindow(HINSTANCE hInstance) {
  if (g_ui.settingsWindow) {
    ShowWindow(g_ui.settingsWindow, SW_SHOW);
    SetForegroundWindow(g_ui.settingsWindow);
    return;
  }

  if (!g_ui.settingsClassRegistered) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kSettingsWindowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    const ATOM atom = RegisterClassW(&wc);
    const DWORD registerError = GetLastError();
    if (atom == 0 && registerError != ERROR_CLASS_ALREADY_EXISTS) {
      AddLogLine("ERROR: Failed to register settings window class (code " + std::to_string(registerError) + "): " +
                 DescribeLastErrorEnglish(registerError));
      MessageBoxW(g_ui.mainWindow, L"Unable to open the settings window.", L"ScaleLogger", MB_OK | MB_ICONERROR);
      return;
    }
    g_ui.settingsClassRegistered = true;
  }

  g_ui.settingsWindow = CreateWindowExW(WS_EX_DLGMODALFRAME, kSettingsWindowClassName, L"ScaleLogger Settings",
                                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 890, 610,
                                        g_ui.mainWindow, nullptr, hInstance, nullptr);
  if (!g_ui.settingsWindow) {
    const DWORD createError = GetLastError();
    AddLogLine("ERROR: Failed to create settings window (code " + std::to_string(createError) + "): " +
               DescribeLastErrorEnglish(createError));
    MessageBoxW(g_ui.mainWindow, L"Unable to open the settings window.", L"ScaleLogger", MB_OK | MB_ICONERROR);
    return;
  }
  ShowWindow(g_ui.settingsWindow, SW_SHOW);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      g_ui.mainWindow = hwnd;
      CreateTopRow(hwnd);
      CreateLogPane(hwnd);
      LayoutMainControls(hwnd);

      AddLogLine("ScaleLogger started.");
      return 0;
    }
    case WM_SIZE: {
      LayoutMainControls(hwnd);
      InvalidateRect(hwnd, nullptr, TRUE);
      return 0;
    }
    case WM_ERASEBKGND: {
      RECT rc{};
      GetClientRect(hwnd, &rc);
      FillRect(reinterpret_cast<HDC>(wParam), &rc, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));
      return 1;
    }
    case WM_COMMAND: {
      const int id = LOWORD(wParam);
      switch (id) {
        case kBtnConnect:
          if (g_ui.controller->IsConnected()) g_ui.controller->Disconnect();
          else g_ui.controller->Connect();
          return 0;
        case kBtnSettings:
          if (HIWORD(wParam) == BN_CLICKED) OpenSettingsWindow(g_ui.hInstance);
          return 0;
        case kBtnAbout:
          if (HIWORD(wParam) == BN_CLICKED) ShowAbout(hwnd);
          return 0;
        case kBtnRefreshPresets:
          AddLogLine("Preset list refresh requested");
          return 0;
        default:
          return 0;
      }
    }
    case WM_DESTROY:
      if (g_ui.controller) g_ui.controller->Disconnect();
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}
} // namespace

int RunMainDialog(HINSTANCE hInstance, int nCmdShow) {
  g_ui.hInstance = hInstance;
  INITCOMMONCONTROLSEX icc{sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES};
  InitCommonControlsEx(&icc);

  g_ui.controller = std::make_unique<AppController>(
      std::filesystem::path(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : ".") / "ScaleLogger");

  g_ui.controller->SetLogSink([](const std::string& message, bool isError) {
    AddLogLine((isError ? "ERROR: " : "") + message);
  });
  g_ui.controller->SetConnectionStateSink([](bool connected) { UpdateConnectionUi(connected); });

  WNDCLASSW wc{};
  wc.lpfnWndProc = MainWndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"ScaleLoggerMainWindow";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"ScaleLogger 0.96", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                                                                       WS_SIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 930, 660, nullptr, nullptr, hInstance, nullptr);

  if (!hwnd) return 1;

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  g_ui.controller->Initialize();
  UpdateConnectionUi(g_ui.controller->IsConnected());

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  g_ui.controller.reset();
  return static_cast<int>(msg.wParam);
}

} // namespace scalelogger
#endif
