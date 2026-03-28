#include "ui/SettingsDialogLogic.hpp"

#ifdef _WIN32
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <vector>

namespace scalelogger::settingslogic {
namespace {
constexpr int kSerialPortCombo = 300;
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
constexpr int kOutputPreserveMinusCheck = 412;
constexpr int kAppPresetsFolderEdit = 500;
constexpr int kAppLogsFolderEdit = 501;
constexpr int kAppLogModeCombo = 502;
constexpr int kAppConnectStartupCheck = 503;
constexpr int kAppStartupPresetCombo = 504;
constexpr int kAppPathsLabel = 505;
constexpr int kAppConfigFolderEdit = 508;
constexpr int kAppDarkModeCheck = 511;
constexpr int kSerialSummaryEdit = 520;
constexpr int kOutputSummaryEdit = 521;

std::string ExtractPortToken(const std::string& display) {
  const auto emDashPos = display.find(" — ");
  const auto cutPos = emDashPos == std::string::npos ? display.find(" - ") : emDashPos;
  return cutPos == std::string::npos ? display : display.substr(0, cutPos);
}

std::wstring FormatStopBits(float value) {
  if (value == 1.5F) return L"1.5";
  if (value >= 1.9F) return L"2";
  return L"1";
}

std::wstring FormatTimeout(float value) {
  wchar_t buffer[32]{};
  swprintf_s(buffer, L"%.2f", value);
  return buffer;
}

std::wstring FormatEolForSummary(const std::string& eol) {
  if (eol == "\r\n") return L"\\r\\n";
  if (eol == "\n") return L"\\n";
  if (eol == "\r") return L"\\r";
  std::wstring out;
  for (char ch : eol) out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(ch)));
  return out;
}

std::string ParseEolFromUiText(const std::wstring& eolText) {
  if (eolText == L"\\r\\n") return "\r\n";
  if (eolText == L"\\n") return "\n";
  if (eolText == L"\\r") return "\r";
  return std::string(eolText.begin(), eolText.end());
}

bool TryParseInt(const std::wstring& text, int& out) {
  try {
    std::size_t idx = 0;
    out = std::stoi(text, &idx, 10);
    return idx == text.size();
  } catch (...) { return false; }
}

bool TryParseFloat(const std::wstring& text, float& out) {
  try {
    std::size_t idx = 0;
    out = std::stof(text, &idx);
    return idx == text.size();
  } catch (...) { return false; }
}

} // namespace

void RefreshPresetDropdown(const Context& ctx, bool keepSelection) {
  std::wstring previous = keepSelection ? ctx.getControlText(ctx.presetsCombo) : L"";
  SendMessageW(ctx.presetsCombo, CB_RESETCONTENT, 0, 0);
  SendMessageW(ctx.presetsCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Last used / defaults"));
  ctx.presetMap->clear();

  std::size_t count = 0;
  const auto presetDir = std::filesystem::path(ctx.controller->Config().presetsFolder);
  if (std::filesystem::exists(presetDir)) {
    for (const auto& entry : std::filesystem::directory_iterator(presetDir)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
      const auto name = entry.path().stem().string();
      (*ctx.presetMap)[name] = entry.path();
      SendMessageW(ctx.presetsCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ctx.toWide(name).c_str()));
      ++count;
    }
  }

  if (!previous.empty()) ctx.setComboToText(ctx.presetsCombo, previous);
  else SendMessageW(ctx.presetsCombo, CB_SETCURSEL, 0, 0);
  ctx.addLogLine("Config list refreshed (" + std::to_string(count) + " entries).");
}

void ApplySelectedConfig(const Context& ctx) {
  const auto selected = ctx.toUtf8(ctx.getControlText(ctx.presetsCombo));
  if (selected.empty() || selected == "Last used / defaults") return;
  const auto it = ctx.presetMap->find(selected);
  if (it == ctx.presetMap->end()) return;
  const auto configPath = it->second;
  AppConfig nextConfig = ctx.controller->Config();
  nextConfig.lastUsedPresetName = configPath.stem().string();
  bool usedLegacyCompatibilityMapping = false;
  ctx.controller->ApplySettings(LoadPreset(configPath, &usedLegacyCompatibilityMapping), nextConfig);
  if (usedLegacyCompatibilityMapping) ctx.addLogLine("Loaded config with legacy compatibility mapping");
  if (ctx.settingsWindow) ctx.loadSettingsIntoControls(ctx.settingsWindow);
  ctx.addLogLine("Loaded config: " + selected);
}

void LoadSettingsIntoControls(const Context& ctx, HWND settingsHwnd) {
  const auto& settings = ctx.controller->Settings();
  const auto& config = ctx.controller->Config();
  ctx.setComboToText(GetDlgItem(settingsHwnd, kSerialPortCombo), ctx.toWide(settings.serial.port));
  ctx.setComboToText(GetDlgItem(settingsHwnd, kSerialBaudCombo), ctx.toWide(std::to_string(settings.serial.baudRate)));
  ctx.setComboToText(GetDlgItem(settingsHwnd, kSerialDataBitsCombo), ctx.toWide(std::to_string(settings.serial.dataBits)));
  ctx.setComboToText(GetDlgItem(settingsHwnd, kSerialParityCombo), std::wstring(1, static_cast<wchar_t>(settings.serial.parity)));
  ctx.setComboToText(GetDlgItem(settingsHwnd, kSerialStopBitsCombo), FormatStopBits(settings.serial.stopBits));
  ctx.setComboToText(GetDlgItem(settingsHwnd, kSerialTimeoutCombo), FormatTimeout(settings.serial.timeoutSeconds));

  if (settings.serial.eol == "\r\n") ctx.setComboToText(GetDlgItem(settingsHwnd, kSerialEolCombo), L"\\r\\n");
  else if (settings.serial.eol == "\n") ctx.setComboToText(GetDlgItem(settingsHwnd, kSerialEolCombo), L"\\n");
  else if (settings.serial.eol == "\r") ctx.setComboToText(GetDlgItem(settingsHwnd, kSerialEolCombo), L"\\r");

  ctx.setComboToText(GetDlgItem(settingsHwnd, kOutputModeCombo), settings.parsing.mode == ParseMode::Parsed ? L"parsed" : L"raw");
  SendMessageW(GetDlgItem(settingsHwnd, kOutputTrimCheck), BM_SETCHECK, settings.parsing.trimWhitespace ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(GetDlgItem(settingsHwnd, kOutputStripSuffixCheck), BM_SETCHECK, settings.parsing.stripSuffix ? BST_CHECKED : BST_UNCHECKED, 0);
  SetWindowTextW(GetDlgItem(settingsHwnd, kOutputSuffixEdit), ctx.toWide(settings.parsing.suffix).c_str());
  SendMessageW(GetDlgItem(settingsHwnd, kOutputNormalizeCheck), BM_SETCHECK, settings.parsing.normalizeSign ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(GetDlgItem(settingsHwnd, kOutputPreservePlusCheck), BM_SETCHECK, settings.parsing.preservePlusSign ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(GetDlgItem(settingsHwnd, kOutputPreserveMinusCheck), BM_SETCHECK, settings.parsing.preserveMinusSign ? BST_CHECKED : BST_UNCHECKED, 0);
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
  ctx.setComboToText(GetDlgItem(settingsHwnd, kOutputActionCombo), action);

  std::wstring sequence;
  for (std::size_t i = 0; i < settings.output.customSequence.size(); ++i) {
    if (i) sequence += L",";
    sequence += ctx.toWide(settings.output.customSequence[i]);
  }
  SetWindowTextW(GetDlgItem(settingsHwnd, kOutputCustomSequenceEdit), sequence.c_str());
  ctx.updateCustomSequenceUiState(settingsHwnd);
  ctx.updateParseControlsUiState(settingsHwnd);

  const auto configPathText = (std::filesystem::path(config.configFolder) / config.configFileName).wstring();
  SetWindowTextW(GetDlgItem(settingsHwnd, kAppConfigFolderEdit), configPathText.c_str());
  SetWindowTextW(GetDlgItem(settingsHwnd, kAppPresetsFolderEdit), ctx.toWide(config.presetsFolder).c_str());
  SetWindowTextW(GetDlgItem(settingsHwnd, kAppLogsFolderEdit), ctx.toWide(config.logsFolder).c_str());
  const std::wstring logMode = config.logMode == LogMode::None ? L"No file logging"
                                : (config.logMode == LogMode::SingleFile ? L"Single file" : L"New file per session");
  ctx.setComboToText(GetDlgItem(settingsHwnd, kAppLogModeCombo), logMode);
  SendMessageW(GetDlgItem(settingsHwnd, kAppConnectStartupCheck), BM_SETCHECK, config.connectOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);
  auto startupCombo = GetDlgItem(settingsHwnd, kAppStartupPresetCombo);
  SendMessageW(startupCombo, CB_RESETCONTENT, 0, 0);
  SendMessageW(startupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Last used / defaults"));
  for (const auto& entry : *ctx.presetMap) SendMessageW(startupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ctx.toWide(entry.first).c_str()));
  ctx.setComboToText(startupCombo, config.startupMode == "specific_preset" ? ctx.toWide(config.startupPresetName) : L"Last used / defaults");

  const std::wstring pathSummary =
      L"Config: " + (std::filesystem::path(config.configFolder) / ctx.toWide(config.configFileName)).wstring() +
      L"\r\nCurrent config: " + ctx.toWide(config.lastUsedPresetName.empty() ? std::string("(none)") : config.lastUsedPresetName) +
      L"\r\nLogs: " + std::filesystem::path(config.logsFolder).wstring() + L" (" +
      (config.logMode == LogMode::None ? L"none" : (config.logMode == LogMode::SingleFile ? L"single_file" : L"per_session")) + L")" +
      L"\r\nDark mode (experimental): " + std::wstring(config.darkMode ? L"on" : L"off") +
      L"\r\nSerial: " + ctx.toWide(settings.serial.port) + L" @ " + ctx.toWide(std::to_string(settings.serial.baudRate)) + L" baud" +
      L"\r\nOutput: " + (settings.parsing.mode == ParseMode::Raw ? L"raw" : L"parsed") + L"; action=" + action;
  SetWindowTextW(GetDlgItem(settingsHwnd, kAppPathsLabel), pathSummary.c_str());

  const std::wstring serialSummary = L"Port=" + ctx.toWide(settings.serial.port) + L"; Baud=" + ctx.toWide(std::to_string(settings.serial.baudRate)) +
                                     L"; DataBits=" + ctx.toWide(std::to_string(settings.serial.dataBits)) + L"; Parity=" +
                                     std::wstring(1, static_cast<wchar_t>(settings.serial.parity)) + L"; StopBits=" + FormatStopBits(settings.serial.stopBits) +
                                     L"; Timeout=" + FormatTimeout(settings.serial.timeoutSeconds) + L"; EOL=" + FormatEolForSummary(settings.serial.eol);
  SetWindowTextW(GetDlgItem(settingsHwnd, kSerialSummaryEdit), serialSummary.c_str());
  std::wstring outputSummary = std::wstring(L"Mode=") + (settings.parsing.mode == ParseMode::Raw ? L"raw" : L"parsed") +
                               L"; Trim=" + std::wstring(settings.parsing.trimWhitespace ? L"true" : L"false") +
                               L"; StripSuffix=" + std::wstring(settings.parsing.stripSuffix ? L"true" : L"false") +
                               L"; NormalizeSign=" + std::wstring(settings.parsing.normalizeSign ? L"true" : L"false") +
                               L"; PreservePlus=" + std::wstring(settings.parsing.preservePlusSign ? L"true" : L"false") +
                               L"; PreserveMinus=" + std::wstring(settings.parsing.preserveMinusSign ? L"true" : L"false") +
                               L"; PostAction=" + action;
  if (settings.output.postAction == PostAction::CustomSequence) outputSummary += L"; Sequence=" + sequence;
  SetWindowTextW(GetDlgItem(settingsHwnd, kOutputSummaryEdit), outputSummary.c_str());
  SendMessageW(GetDlgItem(settingsHwnd, kAppDarkModeCheck), BM_SETCHECK, config.darkMode ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool ReadSerialSettingsFromControls(const Context& ctx, HWND settingsHwnd, AppSettings& settingsOut, std::string& error) {
  settingsOut.serial.port = ctx.toUtf8(ctx.getControlText(GetDlgItem(settingsHwnd, kSerialPortCombo)));
  const auto mapped = ctx.portDisplayToPort->find(settingsOut.serial.port);
  if (mapped != ctx.portDisplayToPort->end()) settingsOut.serial.port = mapped->second;
  settingsOut.serial.port = ExtractPortToken(settingsOut.serial.port);
  if (settingsOut.serial.port.empty()) {
    error = "Invalid serial port: value is empty.";
    return false;
  }
  if (!TryParseInt(ctx.getControlText(GetDlgItem(settingsHwnd, kSerialBaudCombo)), settingsOut.serial.baudRate) || settingsOut.serial.baudRate <= 0) {
    error = "Invalid baud rate. Enter a positive integer.";
    return false;
  }
  if (!TryParseInt(ctx.getControlText(GetDlgItem(settingsHwnd, kSerialDataBitsCombo)), settingsOut.serial.dataBits) ||
      (settingsOut.serial.dataBits != 5 && settingsOut.serial.dataBits != 6 && settingsOut.serial.dataBits != 7 && settingsOut.serial.dataBits != 8)) {
    error = "Invalid data bits. Use 5, 6, 7, or 8.";
    return false;
  }
  const auto parityText = ctx.getControlText(GetDlgItem(settingsHwnd, kSerialParityCombo));
  if (parityText.empty()) {
    error = "Invalid parity. Use N, E, or O.";
    return false;
  }
  const wchar_t parity = static_cast<wchar_t>(std::towupper(parityText[0]));
  if (parity != L'N' && parity != L'E' && parity != L'O') {
    error = "Invalid parity. Use N, E, or O.";
    return false;
  }
  settingsOut.serial.parity = static_cast<char>(parity);
  std::wstring stopBitsText = ctx.getControlText(GetDlgItem(settingsHwnd, kSerialStopBitsCombo));
  if (stopBitsText == L"1.5") settingsOut.serial.stopBits = 1.5F;
  else if (stopBitsText == L"2" || stopBitsText == L"2.0") settingsOut.serial.stopBits = 2.0F;
  else if (stopBitsText == L"1" || stopBitsText == L"1.0") settingsOut.serial.stopBits = 1.0F;
  else {
    error = "Invalid stop bits. Use 1, 1.5, or 2.";
    return false;
  }
  if (!TryParseFloat(ctx.getControlText(GetDlgItem(settingsHwnd, kSerialTimeoutCombo)), settingsOut.serial.timeoutSeconds) || settingsOut.serial.timeoutSeconds <= 0.0F) {
    error = "Invalid timeout. Enter a positive number (seconds).";
    return false;
  }
  settingsOut.serial.eol = ParseEolFromUiText(ctx.getControlText(GetDlgItem(settingsHwnd, kSerialEolCombo)));
  return true;
}

void ApplySettingsFromControls(const Context& ctx, HWND settingsHwnd, bool saveRequested) {
  AppSettings nextSettings = ctx.controller->Settings();
  AppConfig nextConfig = ctx.controller->Config();
  std::string serialError;
  if (!ReadSerialSettingsFromControls(ctx, settingsHwnd, nextSettings, serialError)) {
    ctx.addLogLine("ERROR: " + serialError);
    MessageBoxW(settingsHwnd, ctx.toWide(serialError).c_str(), L"ScaleLogger", MB_OK | MB_ICONERROR);
    return;
  }
  nextSettings.parsing.mode = ctx.toUtf8(ctx.getControlText(GetDlgItem(settingsHwnd, kOutputModeCombo))) == "raw" ? ParseMode::Raw : ParseMode::Parsed;
  nextSettings.parsing.trimWhitespace = SendMessageW(GetDlgItem(settingsHwnd, kOutputTrimCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  nextSettings.parsing.stripSuffix = SendMessageW(GetDlgItem(settingsHwnd, kOutputStripSuffixCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  nextSettings.parsing.suffix = ctx.toUtf8(ctx.getControlText(GetDlgItem(settingsHwnd, kOutputSuffixEdit)));
  nextSettings.parsing.normalizeSign = SendMessageW(GetDlgItem(settingsHwnd, kOutputNormalizeCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  nextSettings.parsing.preservePlusSign = SendMessageW(GetDlgItem(settingsHwnd, kOutputPreservePlusCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  nextSettings.parsing.preserveMinusSign = SendMessageW(GetDlgItem(settingsHwnd, kOutputPreserveMinusCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  nextSettings.parsing.numericValidation = SendMessageW(GetDlgItem(settingsHwnd, kOutputRequireNumericCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  const auto action = ctx.toUtf8(ctx.getControlText(GetDlgItem(settingsHwnd, kOutputActionCombo)));
  if (action == "right") nextSettings.output.postAction = PostAction::Right;
  else if (action == "enter") nextSettings.output.postAction = PostAction::Enter;
  else if (action == "tab") nextSettings.output.postAction = PostAction::Tab;
  else if (action == "none") nextSettings.output.postAction = PostAction::None;
  else if (action == "custom_sequence") nextSettings.output.postAction = PostAction::CustomSequence;
  else nextSettings.output.postAction = PostAction::Down;

  nextConfig.presetsFolder = ctx.toUtf8(ctx.getControlText(GetDlgItem(settingsHwnd, kAppPresetsFolderEdit)));
  nextConfig.logsFolder = ctx.toUtf8(ctx.getControlText(GetDlgItem(settingsHwnd, kAppLogsFolderEdit)));
  nextConfig.connectOnStartup = SendMessageW(GetDlgItem(settingsHwnd, kAppConnectStartupCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;
  nextConfig.darkMode = SendMessageW(GetDlgItem(settingsHwnd, kAppDarkModeCheck), BM_GETCHECK, 0, 0) == BST_CHECKED;

  ctx.controller->ApplySettings(nextSettings, nextConfig, false);
  LoadSettingsIntoControls(ctx, settingsHwnd);
  if (saveRequested) {
    const auto saveResult = ctx.controller->SaveResolvedConfiguration();
    if (saveResult.status == AppController::SaveConfigStatus::Failed) {
      ctx.controller->LogMessage("Failed to save configuration file: " + saveResult.path.string(), true);
    }
  }
  InvalidateRect(ctx.mainWindow, nullptr, TRUE);
  if (ctx.settingsWindow) InvalidateRect(ctx.settingsWindow, nullptr, TRUE);
}

} // namespace scalelogger::settingslogic

#endif
