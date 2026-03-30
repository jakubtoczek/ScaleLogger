#include "ui/MainDialog.hpp"

#ifdef _WIN32
#include "app/AppController.hpp"
#include "core/AppVersion.hpp"
#include "core/ValueParser.hpp"
#include "ui/AboutDialog.hpp"
#include "ui/SettingsDialogLogic.hpp"

#include <CommCtrl.h>
#include <CommDlg.h>
#include <ShlObj.h>
#include <Uxtheme.h>
#include <Windows.h>

#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cwctype>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Uxtheme.lib")

namespace scalelogger {
namespace {
constexpr int kLblConnectionStatus = 102;
constexpr int kLblConnectionTitle = 107;
constexpr int kBtnConnect = 103;
constexpr int kBtnSettings = 104;
constexpr int kBtnAbout = 105;
constexpr int kEditLog = 106;
constexpr UINT kMsgUiLogLine = WM_APP + 1;
constexpr UINT kMsgUiConnectionState = WM_APP + 2;
constexpr UINT kMsgStartupAutoConnect = WM_APP + 3;
constexpr UINT kMsgSettingsFinalizeCombos = WM_APP + 4;
constexpr UINT kMsgSettingsFinalizeDisplay = WM_APP + 5;
constexpr UINT kMsgComboEditNormalizeSelection = WM_APP + 6;

constexpr int kSettingsTab = 200;
constexpr int kSettingsApply = 201;
constexpr int kSettingsCancel = 202;
constexpr int kSettingsSaveConfig = 203;
constexpr int kSettingsSaveAsConfig = 204;

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
constexpr int kOutputCaptureKeyBtn = 409;
constexpr int kOutputRemoveLastBtn = 410;
constexpr int kOutputClearBtn = 411;
constexpr int kOutputPreserveMinusCheck = 412;

constexpr int kAppLogsFolderEdit = 501;
constexpr int kAppLogModeCombo = 502;
constexpr int kAppConnectStartupCheck = 503;
constexpr int kAppPathsLabel = 505;
constexpr int kAppLogsBrowseBtn = 507;
constexpr int kAppConfigFolderEdit = 508;
constexpr int kAppConfigBrowseBtn = 509;
constexpr int kAppConfigFileNameEdit = 510;
constexpr int kAppDarkModeCheck = 511;
constexpr int kSerialSummaryEdit = 520;
constexpr int kOutputSummaryEdit = 521;
constexpr wchar_t kSettingsWindowClassName[] = L"ScaleLoggerSettingsWindow";

struct UiState {
  std::unique_ptr<AppController> controller;
  HINSTANCE hInstance{nullptr};
  HWND mainWindow{nullptr};
  HWND settingsButton{nullptr};
  HWND aboutButton{nullptr};
  HWND connectButton{nullptr};
  HWND connectionIndicator{nullptr};
  HWND connectionTitle{nullptr};
  HWND connectionStatus{nullptr};
  HWND logEdit{nullptr};
  HWND settingsWindow{nullptr};
  HWND settingsTab{nullptr};
  bool settingsClassRegistered{false};
  bool settingsNormalizeFocusPending{false};
  bool captureCustomSequenceKey{false};
  std::unordered_map<std::string, std::filesystem::path> configMap{};
  std::unordered_map<std::string, std::string> portDisplayToPort{};
  std::vector<HWND> serialTabControls{};
  std::vector<HWND> outputTabControls{};
  std::vector<HWND> applicationTabControls{};
};

UiState g_ui;
HBRUSH g_darkBrush = CreateSolidBrush(RGB(32, 32, 32));
enum class ConnectionUiState { Disconnected, Connecting, Connected };
ConnectionUiState g_connectionUiState = ConnectionUiState::Disconnected;
void LoadSettingsIntoControls(HWND settingsHwnd);
std::wstring GetControlText(HWND control);
void AddLogLine(const std::string& text);

namespace uilayout {
constexpr int kMargin = 12;
constexpr int kTopRowY = 14;
constexpr int kTopRowHeight = 28;
constexpr int kStatusTitleWidth = 46;
constexpr int kStatusStateWidth = 124;
constexpr int kTopButtonGap = 8;
constexpr int kTopButtonConnectWidth = 106;
constexpr int kTopButtonSettingsWidth = 84;
constexpr int kTopButtonAboutWidth = 68;
constexpr int kLogTopY = 50;
constexpr int kStandardControlHeight = 24;
constexpr int kSettingsBottomButtonHeight = 32;
}

bool IsComboDebugLoggingEnabled() {
  return g_ui.controller && g_ui.controller->Config().debugComboLogging;
}

LRESULT CALLBACK EditableComboEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
  auto comboNameFromId = [](int id) -> const char* {
    switch (id) {
      case kSerialPortCombo: return "Port";
      case kSerialBaudCombo: return "Baud";
      case kSerialDataBitsCombo: return "DataBits";
      case kSerialParityCombo: return "Parity";
      case kSerialStopBitsCombo: return "StopBits";
      case kSerialTimeoutCombo: return "Timeout";
      case kSerialEolCombo: return "LineEnding";
      default: return "Unknown";
    }
  };
  auto logComboState = [&](const char* phase, const char* messageName) {
    if (!IsComboDebugLoggingEnabled()) return;
    HWND combo = GetParent(hwnd);
    const int comboId = combo ? GetDlgCtrlID(combo) : 0;
    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(hwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    const int length = GetWindowTextLengthW(hwnd);
    std::ostringstream oss;
    oss << "DEBUG_COMBO: field=" << comboNameFromId(comboId) << "; phase=" << phase << "; msg=" << messageName
        << "; edit_hwnd=0x" << std::hex << reinterpret_cast<std::uintptr_t>(hwnd) << "; combo_hwnd=0x"
        << reinterpret_cast<std::uintptr_t>(combo) << "; focus_hwnd=0x" << reinterpret_cast<std::uintptr_t>(GetFocus()) << std::dec
        << "; len=" << length << "; sel=" << start << ".." << end;
    AddLogLine(oss.str());
  };
  const auto normalizeIfFullySelected = [hwnd]() {
    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(hwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    const int length = GetWindowTextLengthW(hwnd);
    if (length > 0 && start == 0 && static_cast<int>(end) == length) {
      SendMessageW(hwnd, EM_SETSEL, length, length);
      RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
      HWND combo = GetParent(hwnd);
      if (combo) RedrawWindow(combo, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
    }
  };

  switch (msg) {
    case WM_SETFOCUS:
    case WM_LBUTTONUP:
      logComboState("PRE", msg == WM_SETFOCUS ? "WM_SETFOCUS" : "WM_LBUTTONUP");
      PostMessageW(hwnd, kMsgComboEditNormalizeSelection, 0, 0);
      logComboState("POST", msg == WM_SETFOCUS ? "WM_SETFOCUS" : "WM_LBUTTONUP");
      break;
    case WM_KILLFOCUS:
      logComboState("PRE", "WM_KILLFOCUS");
      break;
    case WM_LBUTTONDOWN:
      logComboState("PRE", "WM_LBUTTONDOWN");
      break;
    case WM_MOUSEACTIVATE:
      logComboState("PRE", "WM_MOUSEACTIVATE");
      break;
    case kMsgComboEditNormalizeSelection:
      logComboState("PRE", "kMsgComboEditNormalizeSelection");
      normalizeIfFullySelected();
      logComboState("POST", "kMsgComboEditNormalizeSelection");
      return 0;
    case WM_NCDESTROY:
      logComboState("PRE", "WM_NCDESTROY");
      RemoveWindowSubclass(hwnd, EditableComboEditSubclassProc, 1);
      break;
    default: break;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

std::wstring ToWide(std::string_view text) {
  if (text.empty()) return {};
  const int sizeNeeded = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (sizeNeeded <= 0) return std::wstring(text.begin(), text.end());
  std::wstring out(static_cast<std::size_t>(sizeNeeded), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), sizeNeeded);
  return out;
}

std::string ToUtf8(const std::wstring& text) {
  if (text.empty()) return {};
  const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (sizeNeeded <= 0) return {};
  std::string out(static_cast<std::size_t>(sizeNeeded), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), sizeNeeded, nullptr, nullptr);
  return out;
}

void TraceEarly(const std::string& message) {
  OutputDebugStringA((message + "\n").c_str());
  std::fprintf(stderr, "%s\n", message.c_str());
}

std::string ExtractPortToken(const std::string& display) {
  const auto emDashPos = display.find(" — ");
  const auto cutPos = emDashPos == std::string::npos ? display.find(" - ") : emDashPos;
  return cutPos == std::string::npos ? display : display.substr(0, cutPos);
}

bool IsLikelySerialPortName(const std::string& port) {
  if (port.size() < 4) return false;
  if (!(port[0] == 'C' || port[0] == 'c') || !(port[1] == 'O' || port[1] == 'o') || !(port[2] == 'M' || port[2] == 'm')) return false;
  for (std::size_t i = 3; i < port.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(port[i]))) return false;
  }
  return true;
}

bool PortExistsInScan(const std::vector<std::string>& scannedPorts, const std::string& port) {
  auto upper = [](std::string value) {
    for (char& ch : value) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return value;
  };
  const auto wanted = upper(port);
  for (const auto& entry : scannedPorts) {
    if (upper(ExtractPortToken(entry)) == wanted) return true;
  }
  return false;
}

bool IsDarkModeEnabled() {
  return g_ui.controller && g_ui.controller->Config().darkMode;
}

LRESULT HandleDarkCtlColor(HDC hdc) {
  if (!IsDarkModeEnabled()) return 0;
  SetTextColor(hdc, RGB(235, 235, 235));
  SetBkColor(hdc, RGB(32, 32, 32));
  return reinterpret_cast<LRESULT>(g_darkBrush);
}

LRESULT HandleSettingsTabCustomDraw(LPARAM lParam) {
  auto* draw = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
  if (!draw || !IsDarkModeEnabled()) return CDRF_DODEFAULT;

  switch (draw->dwDrawStage) {
    case CDDS_PREPAINT: {
      FillRect(draw->hdc, &draw->rc, g_darkBrush);
      return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    }
    case CDDS_ITEMPREPAINT: {
      const int tabIndex = static_cast<int>(draw->dwItemSpec);
      const int selectedIndex = TabCtrl_GetCurSel(draw->hdr.hwndFrom);
      const COLORREF tabColor = (tabIndex == selectedIndex) ? RGB(58, 58, 58) : RGB(40, 40, 40);

      HBRUSH tabBrush = CreateSolidBrush(tabColor);
      FillRect(draw->hdc, &draw->rc, tabBrush);
      HBRUSH borderBrush = CreateSolidBrush(RGB(78, 78, 78));
      FrameRect(draw->hdc, &draw->rc, borderBrush);
      DeleteObject(borderBrush);
      DeleteObject(tabBrush);

      RECT textRect = draw->rc;
      textRect.left += 8;
      textRect.right -= 8;

      wchar_t text[128] = {};
      TCITEMW item{};
      item.mask = TCIF_TEXT;
      item.pszText = text;
      item.cchTextMax = static_cast<int>(std::size(text));
      if (TabCtrl_GetItem(draw->hdr.hwndFrom, tabIndex, &item)) {
        SetBkMode(draw->hdc, TRANSPARENT);
        SetTextColor(draw->hdc, RGB(235, 235, 235));
        DrawTextW(draw->hdc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
      }
      return CDRF_SKIPDEFAULT;
    }
    case CDDS_POSTPAINT: {
      RECT tabClient = draw->rc;
      TabCtrl_AdjustRect(draw->hdr.hwndFrom, FALSE, &tabClient);
      FillRect(draw->hdc, &tabClient, g_darkBrush);
      return CDRF_DODEFAULT;
    }
    default: return CDRF_DODEFAULT;
  }
}

void ApplySettingsTabTheme(HWND settingsTab) {
  if (!settingsTab) return;
  if (IsDarkModeEnabled()) {
    SetWindowTheme(settingsTab, L"", L"");
  } else {
    SetWindowTheme(settingsTab, nullptr, nullptr);
  }
  InvalidateRect(settingsTab, nullptr, TRUE);
}

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

void PostLogLineToUiThread(const std::string& text) {
  if (!g_ui.mainWindow) return;
  auto* payload = new std::string(text);
  if (!PostMessageW(g_ui.mainWindow, kMsgUiLogLine, 0, reinterpret_cast<LPARAM>(payload))) delete payload;
}

void PostConnectionStateToUiThread(bool connected) {
  if (!g_ui.mainWindow) return;
  PostMessageW(g_ui.mainWindow, kMsgUiConnectionState, connected ? 1 : 0, 0);
}

void UpdateConnectionUi(ConnectionUiState state) {
  if (!g_ui.connectButton || !g_ui.connectionStatus || !g_ui.connectionIndicator) return;
  g_connectionUiState = state;
  const bool connected = state == ConnectionUiState::Connected;
  SetWindowTextW(g_ui.connectButton, connected ? L"Disconnect" : L"Connect");
  const wchar_t* label = state == ConnectionUiState::Connected ? L"Connected"
                        : (state == ConnectionUiState::Connecting ? L"Connecting" : L"Disconnected");
  SetWindowTextW(g_ui.connectionStatus, label);
  SetWindowTextW(g_ui.connectionIndicator, L"\x25CF");
  InvalidateRect(g_ui.connectionIndicator, nullptr, TRUE);
}

void UpdateConnectionUi(bool connected) {
  UpdateConnectionUi(connected ? ConnectionUiState::Connected : ConnectionUiState::Disconnected);
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

  bool showAbout = true;
  bool showSettings = true;
  bool showConnect = true;
  bool showIndicator = true;
  bool showStatusTitle = true;
  bool showStatus = true;

  auto requiredWidth = [&]() {
    int total = uilayout::kMargin * 2;
    if (showIndicator) total += 14 + uilayout::kTopButtonGap;
    if (showStatusTitle) total += uilayout::kStatusTitleWidth + uilayout::kTopButtonGap;
    if (showStatus) total += uilayout::kStatusStateWidth + uilayout::kTopButtonGap;
    if (showConnect) total += uilayout::kTopButtonConnectWidth + uilayout::kTopButtonGap;
    if (showSettings) total += uilayout::kTopButtonSettingsWidth + uilayout::kTopButtonGap;
    if (showAbout) total += uilayout::kTopButtonAboutWidth + uilayout::kTopButtonGap;
    return total;
  };

  while (requiredWidth() > rc.right) {
    if (showAbout) {
      showAbout = false;
      continue;
    }
    if (showSettings) {
      showSettings = false;
      continue;
    }
    if (showConnect) {
      showConnect = false;
      continue;
    }
    if (showStatus) {
      showStatus = false;
      continue;
    }
    if (showStatusTitle) {
      showStatusTitle = false;
      continue;
    }
    if (showIndicator) {
      showIndicator = false;
      continue;
    }
    break;
  }

  ShowWindow(g_ui.connectButton, showConnect ? SW_SHOW : SW_HIDE);
  ShowWindow(g_ui.settingsButton, showSettings ? SW_SHOW : SW_HIDE);
  ShowWindow(g_ui.aboutButton, showAbout ? SW_SHOW : SW_HIDE);
  ShowWindow(g_ui.connectionIndicator, showIndicator ? SW_SHOW : SW_HIDE);
  ShowWindow(g_ui.connectionTitle, showStatusTitle ? SW_SHOW : SW_HIDE);
  ShowWindow(g_ui.connectionStatus, showStatus ? SW_SHOW : SW_HIDE);

  int left = uilayout::kMargin;
  if (showIndicator) {
    MoveWindow(g_ui.connectionIndicator, left, uilayout::kTopRowY + 5, 14, 20, TRUE);
    left += 14 + uilayout::kTopButtonGap;
  }
  if (showStatusTitle) {
    MoveWindow(g_ui.connectionTitle, left, uilayout::kTopRowY + 4, uilayout::kStatusTitleWidth, 22, TRUE);
    left += uilayout::kStatusTitleWidth + 4;
  }
  if (showStatus) {
    MoveWindow(g_ui.connectionStatus, left, uilayout::kTopRowY + 4, uilayout::kStatusStateWidth, 22, TRUE);
  }

  int right = rc.right - uilayout::kMargin;
  if (showAbout) {
    right -= uilayout::kTopButtonAboutWidth;
    MoveWindow(g_ui.aboutButton, right, uilayout::kTopRowY, uilayout::kTopButtonAboutWidth, uilayout::kTopRowHeight, TRUE);
    right -= uilayout::kTopButtonGap;
  }
  if (showSettings) {
    right -= uilayout::kTopButtonSettingsWidth;
    MoveWindow(g_ui.settingsButton, right, uilayout::kTopRowY, uilayout::kTopButtonSettingsWidth, uilayout::kTopRowHeight, TRUE);
    right -= uilayout::kTopButtonGap;
  }
  if (showConnect) {
    right -= uilayout::kTopButtonConnectWidth;
    MoveWindow(g_ui.connectButton, right, uilayout::kTopRowY, uilayout::kTopButtonConnectWidth, uilayout::kTopRowHeight, TRUE);
  }

  const int logTop = uilayout::kLogTopY;
  const int logBottomMargin = uilayout::kMargin;
  MoveWindow(g_ui.logEdit, uilayout::kMargin, logTop, rc.right - uilayout::kMargin * 2, rc.bottom - logTop - logBottomMargin, TRUE);
}

void PopulateComboWithValues(HWND combo, const std::vector<std::wstring>& values) {
  const std::wstring previous = GetControlText(combo);
  SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  for (const auto& value : values) {
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
  }
  if (!previous.empty()) {
    const LRESULT idx = SendMessageW(combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(previous.c_str()));
    if (idx != CB_ERR) {
      SendMessageW(combo, CB_SETCURSEL, idx, 0);
      return;
    }
    SetWindowTextW(combo, previous.c_str());
    return;
  }
  if (!values.empty()) SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

void SetComboToText(HWND combo, const std::wstring& text) {
  if (text.empty()) return;
  auto comboNameFromId = [](int id) -> const char* {
    switch (id) {
      case kSerialPortCombo: return "Port";
      case kSerialBaudCombo: return "Baud";
      case kSerialDataBitsCombo: return "DataBits";
      case kSerialParityCombo: return "Parity";
      case kSerialStopBitsCombo: return "StopBits";
      case kSerialTimeoutCombo: return "Timeout";
      case kSerialEolCombo: return "LineEnding";
      default: return "Other";
    }
  };
  if (IsComboDebugLoggingEnabled()) {
    std::ostringstream oss;
    oss << "DEBUG_COMBO: field=" << comboNameFromId(GetDlgCtrlID(combo)) << "; phase=CALL; msg=SetComboToText"
        << "; combo_hwnd=0x" << std::hex << reinterpret_cast<std::uintptr_t>(combo) << std::dec
        << "; text_len=" << text.size();
    AddLogLine(oss.str());
  }
  const LONG_PTR style = GetWindowLongPtrW(combo, GWL_STYLE);
  if ((style & CBS_DROPDOWNLIST) == 0) {
    SetWindowTextW(combo, text.c_str());
    return;
  }
  const LRESULT idx = SendMessageW(combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(text.c_str()));
  if (idx != CB_ERR) {
    SendMessageW(combo, CB_SETCURSEL, idx, 0);
  } else {
    const LRESULT count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
    if (count > 0) SendMessageW(combo, CB_SETCURSEL, 0, 0);
  }
}

std::wstring GetControlText(HWND control) {
  const int length = GetWindowTextLengthW(control);
  std::wstring text(length, L'\0');
  GetWindowTextW(control, text.data(), length + 1);
  return text;
}

std::wstring FormatStopBits(float value) {
  if (value == 1.5F) return L"1.5";
  if (value >= 1.9F) return L"2";
  return L"1";
}

std::wstring FormatTimeout(float value) {
  std::wstringstream ss;
  ss << std::fixed << std::setprecision(2) << value;
  return ss.str();
}

std::wstring FormatEolForSummary(const std::string& eol) {
  if (eol == "\r\n") return L"\\r\\n";
  if (eol == "\n") return L"\\n";
  if (eol == "\r") return L"\\r";
  return ToWide(eol);
}

void LayoutSettingsWindow(HWND hwnd);

void FinalizeEditableComboFirstPaint(HWND settingsHwnd) {
  for (int comboId : {kSerialPortCombo, kSerialBaudCombo, kSerialDataBitsCombo, kSerialParityCombo, kSerialStopBitsCombo, kSerialTimeoutCombo,
                      kSerialEolCombo}) {
    HWND combo = GetDlgItem(settingsHwnd, comboId);
    if (!combo) continue;
    COMBOBOXINFO info{};
    info.cbSize = sizeof(COMBOBOXINFO);
    if (!GetComboBoxInfo(combo, &info) || !info.hwndItem) continue;
    SetWindowSubclass(info.hwndItem, EditableComboEditSubclassProc, 1, 0);
    PostMessageW(info.hwndItem, kMsgComboEditNormalizeSelection, 0, 0);
    if (IsComboDebugLoggingEnabled()) {
      std::ostringstream oss;
      oss << "DEBUG_COMBO: field_id=" << comboId << "; phase=CALL; msg=SetWindowSubclass"
          << "; combo_hwnd=0x" << std::hex << reinterpret_cast<std::uintptr_t>(combo)
          << "; edit_hwnd=0x" << reinterpret_cast<std::uintptr_t>(info.hwndItem) << std::dec;
      AddLogLine(oss.str());
    }
  }
}

void FinalizeSettingsDisplay(HWND settingsHwnd) {
  LayoutSettingsWindow(settingsHwnd);
  ApplySettingsTabTheme(g_ui.settingsTab);
  if (g_ui.settingsNormalizeFocusPending) {
    HWND applyButton = GetDlgItem(settingsHwnd, kSettingsApply);
    if (applyButton) SetFocus(applyButton);
    g_ui.settingsNormalizeFocusPending = false;
  }
  RedrawWindow(settingsHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
  PostMessageW(settingsHwnd, kMsgSettingsFinalizeCombos, 0, 0);
}

std::string ParseEolFromUiText(const std::wstring& eolText) {
  const auto eolDisplay = ToUtf8(eolText);
  if (eolDisplay == "\\r\\n") return "\r\n";
  if (eolDisplay == "\\n") return "\n";
  if (eolDisplay == "\\r") return "\r";
  if (eolDisplay == "\r\n" || eolDisplay == "\n" || eolDisplay == "\r") return eolDisplay;
  return "\r\n";
}

bool TryParseInt(const std::wstring& text, int& out) {
  if (text.empty()) return false;
  wchar_t* end = nullptr;
  const long value = std::wcstol(text.c_str(), &end, 10);
  if (!end || *end != L'\0') return false;
  out = static_cast<int>(value);
  return true;
}

bool TryParseFloat(const std::wstring& text, float& out) {
  if (text.empty()) return false;
  wchar_t* end = nullptr;
  const float value = std::wcstof(text.c_str(), &end);
  if (!end || *end != L'\0') return false;
  out = value;
  return true;
}

void UpdateCustomSequenceUiState(HWND settingsHwnd) {
  const auto action = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kOutputActionCombo)));
  const bool enabled = action == "custom_sequence";
  EnableWindow(GetDlgItem(settingsHwnd, kOutputCustomSequenceEdit), enabled ? TRUE : FALSE);
  EnableWindow(GetDlgItem(settingsHwnd, kOutputCaptureKeyBtn), enabled ? TRUE : FALSE);
  EnableWindow(GetDlgItem(settingsHwnd, kOutputRemoveLastBtn), enabled ? TRUE : FALSE);
  EnableWindow(GetDlgItem(settingsHwnd, kOutputClearBtn), enabled ? TRUE : FALSE);
}

void UpdateParseControlsUiState(HWND settingsHwnd) {
  const auto mode = ToUtf8(GetControlText(GetDlgItem(settingsHwnd, kOutputModeCombo)));
  const bool parsedMode = mode != "raw";
  for (int id : {kOutputTrimCheck, kOutputStripSuffixCheck, kOutputSuffixEdit, kOutputNormalizeCheck, kOutputPreservePlusCheck,
                 kOutputPreserveMinusCheck,
                 kOutputRequireNumericCheck}) {
    EnableWindow(GetDlgItem(settingsHwnd, id), parsedMode ? TRUE : FALSE);
  }
}

int CALLBACK BrowseCallbackProc(HWND hwnd, UINT msg, LPARAM, LPARAM lpData) {
  if (msg == BFFM_INITIALIZED && lpData != 0) {
    const auto* initial = reinterpret_cast<const wchar_t*>(lpData);
    SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, reinterpret_cast<LPARAM>(initial));
  }
  return 0;
}

settingslogic::Context BuildSettingsLogicContext() {
  settingslogic::Context ctx{};
  ctx.controller = g_ui.controller.get();
  ctx.mainWindow = g_ui.mainWindow;
  ctx.settingsWindow = g_ui.settingsWindow;
  ctx.portDisplayToPort = &g_ui.portDisplayToPort;
  ctx.addLogLine = [](const std::string& m) { AddLogLine(m); };
  ctx.toWide = [](std::string_view t) { return ToWide(t); };
  ctx.toUtf8 = [](const std::wstring& t) { return ToUtf8(t); };
  ctx.getControlText = [](HWND c) { return GetControlText(c); };
  ctx.setComboToText = [](HWND c, const std::wstring& t) { SetComboToText(c, t); };
  ctx.loadSettingsIntoControls = [](HWND h) { LoadSettingsIntoControls(h); };
  ctx.updateCustomSequenceUiState = [](HWND h) { UpdateCustomSequenceUiState(h); };
  ctx.updateParseControlsUiState = [](HWND h) { UpdateParseControlsUiState(h); };
  return ctx;
}

bool BrowseForFolder(HWND owner, std::wstring& output, const std::wstring& initialPath) {
  BROWSEINFOW bi{};
  bi.hwndOwner = owner;
  bi.lpszTitle = L"Select folder";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  bi.lpfn = BrowseCallbackProc;
  bi.lParam = reinterpret_cast<LPARAM>(initialPath.c_str());
  PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
  if (!pidl) return false;
  wchar_t path[MAX_PATH]{};
  const bool ok = SHGetPathFromIDListW(pidl, path) == TRUE;
  CoTaskMemFree(pidl);
  if (!ok) return false;
  output = path;
  return true;
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
  settingslogic::LoadSettingsIntoControls(BuildSettingsLogicContext(), settingsHwnd);
}

bool ReadSerialSettingsFromControls(HWND settingsHwnd, AppSettings& settingsOut, std::string& error) {
  return settingslogic::ReadSerialSettingsFromControls(BuildSettingsLogicContext(), settingsHwnd, settingsOut, error);
}

void ApplySettingsFromControls(HWND settingsHwnd, bool saveRequested) {
  settingslogic::ApplySettingsFromControls(BuildSettingsLogicContext(), settingsHwnd, saveRequested);
}

void CreateTopRow(HWND hwnd) {
  g_ui.connectionIndicator = CreateWindowW(L"STATIC", L"\x25CF", WS_CHILD | WS_VISIBLE, uilayout::kMargin, 19, 14, 20, hwnd, nullptr, nullptr, nullptr);
  g_ui.connectionTitle = CreateWindowW(L"STATIC", L"Status:", WS_CHILD | WS_VISIBLE, 34, 17, uilayout::kStatusTitleWidth, 22, hwnd,
                                       reinterpret_cast<HMENU>(kLblConnectionTitle), nullptr, nullptr);
  g_ui.connectionStatus = CreateWindowW(L"STATIC", L"Disconnected", WS_CHILD | WS_VISIBLE, 84, 17, uilayout::kStatusStateWidth, 22, hwnd,
                                        reinterpret_cast<HMENU>(kLblConnectionStatus), nullptr, nullptr);
  g_ui.connectButton = CreateWindowW(L"BUTTON", L"Connect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 628, uilayout::kTopRowY,
                                     uilayout::kTopButtonConnectWidth, uilayout::kTopRowHeight, hwnd,
                                     reinterpret_cast<HMENU>(kBtnConnect), nullptr, nullptr);
  g_ui.settingsButton = CreateWindowW(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 736, uilayout::kTopRowY,
                                      uilayout::kTopButtonSettingsWidth, uilayout::kTopRowHeight, hwnd,
                                      reinterpret_cast<HMENU>(kBtnSettings), nullptr, nullptr);
  g_ui.aboutButton = CreateWindowW(L"BUTTON", L"About", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 822, uilayout::kTopRowY,
                                   uilayout::kTopButtonAboutWidth, uilayout::kTopRowHeight, hwnd,
                                   reinterpret_cast<HMENU>(kBtnAbout), nullptr, nullptr);
}

void CreateLogPane(HWND hwnd) {
  g_ui.logEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                                                                      ES_READONLY,
                                 uilayout::kMargin, uilayout::kLogTopY, 840, 500, hwnd, reinterpret_cast<HMENU>(kEditLog), nullptr, nullptr);
}

void AddControl(std::vector<HWND>& tabControls, HWND control) { tabControls.push_back(control); }

void RefreshPortList(HWND settingsHwnd) {
  auto ports = g_ui.controller->ScanPorts();
  if (ports.empty()) ports.push_back(g_ui.controller->Settings().serial.port);
  std::sort(ports.begin(), ports.end());
  ports.erase(std::unique(ports.begin(), ports.end()), ports.end());
  g_ui.portDisplayToPort.clear();

  std::vector<std::wstring> values;
  values.reserve(ports.size());
  for (const auto& portEntry : ports) {
    const auto rawPort = ExtractPortToken(portEntry);
    g_ui.portDisplayToPort[portEntry] = rawPort;
    values.push_back(ToWide(portEntry));
  }
  PopulateComboWithValues(GetDlgItem(settingsHwnd, kSerialPortCombo), values);
  const auto currentPort = g_ui.controller->Settings().serial.port;
  auto currentDisplay = currentPort;
  for (const auto& entry : g_ui.portDisplayToPort) {
    if (entry.second == currentPort) {
      currentDisplay = entry.first;
      break;
    }
  }
  SetComboToText(GetDlgItem(settingsHwnd, kSerialPortCombo), ToWide(currentDisplay));
  std::string joined;
  for (std::size_t i = 0; i < ports.size(); ++i) {
    if (i) joined += ", ";
    joined += ports[i];
  }
  AddLogLine("Detected " + std::to_string(ports.size()) + " ports" + (joined.empty() ? "." : (": " + joined)));
}

void SaveAsConfigFromControls(HWND settingsHwnd) {
  ApplySettingsFromControls(settingsHwnd, false);
  AddLogLine("Save As Config: validated and applied current settings before writing file.");

  const auto configFolder = g_ui.controller->DataRoot();
  std::filesystem::create_directories(configFolder);
  std::wstring initialPath = (configFolder / L"ScaleLogger.config.json").wstring();
  std::vector<wchar_t> pathBuffer(initialPath.begin(), initialPath.end());
  pathBuffer.resize(MAX_PATH, L'\0');

  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = settingsHwnd;
  ofn.lpstrFilter = L"Config JSON (*.json)\0*.json\0\0";
  ofn.lpstrFile = pathBuffer.data();
  ofn.nMaxFile = static_cast<DWORD>(pathBuffer.size());
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  ofn.lpstrDefExt = L"json";

  if (!GetSaveFileNameW(&ofn)) return;

  std::filesystem::path outputPath(ofn.lpstrFile);
  if (SaveConfig(outputPath, g_ui.controller->Config(), &g_ui.controller->Settings())) {
    AddLogLine("Configuration saved to: " + outputPath.string());
  }
  else AddLogLine("ERROR: Failed to save configuration: " + outputPath.string());
}

void RunTestReceive(HWND settingsHwnd) {
  AppSettings testSettings = g_ui.controller->Settings();
  std::string serialError;
  if (!ReadSerialSettingsFromControls(settingsHwnd, testSettings, serialError)) {
    AddLogLine("ERROR: " + serialError);
    MessageBoxW(settingsHwnd, ToWide(serialError).c_str(), L"ScaleLogger", MB_OK | MB_ICONERROR);
    return;
  }

  std::string line;
  std::string error;
  if (g_ui.controller->TestReceive(testSettings.serial, line, error)) {
    AddLogLine("Raw received line: '" + line + "'");
    ValueParser parser;
    const auto parsed = parser.Process(line, testSettings.parsing);
    if (parsed.ok) AddLogLine("Parsed value: '" + parsed.processed + "'");
    else AddLogLine("Parse rejected: " + parsed.message);
  } else {
    AddLogLine("Test Receive failed: " + error);
  }
}

void LayoutSettingsWindow(HWND hwnd) {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int margin = 14;
  const int left = 26;
  const int top = 58;
  const int labelWidth = 116;
  const int fieldLeft = left + labelWidth;
  const int browseWidth = 76;
  const int rightPadding = 26;
  const int buttonY = rc.bottom - 46;
  const int tabBottom = buttonY - 12;

  MoveWindow(g_ui.settingsTab, margin, margin, rc.right - (margin * 2), tabBottom - margin, TRUE);

  const int contentRight = rc.right - rightPadding;
  const int fullFieldWidth = (std::max)(260, contentRight - fieldLeft);
  const int browsedFieldWidth = (std::max)(200, fullFieldWidth - browseWidth - 5);

  auto moveField = [&](int id, int y, int width = -1) {
    const int fieldWidth = width < 0 ? fullFieldWidth : width;
    MoveWindow(GetDlgItem(hwnd, id), fieldLeft, y, fieldWidth, uilayout::kStandardControlHeight, TRUE);
  };
  auto moveCombo = [&](int id, int y, int width = -1) {
    const int fieldWidth = width < 0 ? fullFieldWidth : width;
    MoveWindow(GetDlgItem(hwnd, id), fieldLeft, y, fieldWidth, 220, TRUE);
  };
  auto moveBrowse = [&](int id, int y) {
    MoveWindow(GetDlgItem(hwnd, id), fieldLeft + browsedFieldWidth + 6, y, browseWidth, uilayout::kStandardControlHeight, TRUE);
  };

  const int serialButtonsWidth = 96 + 100 + 9;
  const int serialFieldWidth = (std::max)(150, fullFieldWidth - serialButtonsWidth);
  moveCombo(kSerialPortCombo, top, serialFieldWidth);
  MoveWindow(GetDlgItem(hwnd, kSerialScanBtn), fieldLeft + serialFieldWidth + 6, top, 98, uilayout::kStandardControlHeight, TRUE);
  MoveWindow(GetDlgItem(hwnd, kSerialTestBtn), fieldLeft + serialFieldWidth + 108, top, 102, uilayout::kStandardControlHeight, TRUE);
  moveCombo(kSerialBaudCombo, top + 36);
  moveCombo(kSerialDataBitsCombo, top + 72);
  moveCombo(kSerialParityCombo, top + 108);
  moveCombo(kSerialStopBitsCombo, top + 144);
  moveCombo(kSerialTimeoutCombo, top + 180);
  moveCombo(kSerialEolCombo, top + 216);
  MoveWindow(GetDlgItem(hwnd, kSerialSummaryEdit), fieldLeft, top + 252, fullFieldWidth, 46, TRUE);

  moveCombo(kOutputModeCombo, top + 8);
  moveField(kOutputSuffixEdit, top + 100);
  moveCombo(kOutputActionCombo, top + 246);
  moveField(kOutputCustomSequenceEdit, top + 282);
  const int actionGap = 12;
  const int actionBtnWidth = (std::max)(120, (fullFieldWidth - actionGap * 2) / 3);
  const int actionY = top + 314;
  MoveWindow(GetDlgItem(hwnd, kOutputCaptureKeyBtn), fieldLeft, actionY, actionBtnWidth, uilayout::kStandardControlHeight, TRUE);
  MoveWindow(GetDlgItem(hwnd, kOutputRemoveLastBtn), fieldLeft + actionBtnWidth + actionGap, actionY, actionBtnWidth, uilayout::kStandardControlHeight,
             TRUE);
  MoveWindow(GetDlgItem(hwnd, kOutputClearBtn), fieldLeft + (actionBtnWidth + actionGap) * 2, actionY, actionBtnWidth, uilayout::kStandardControlHeight,
             TRUE);
  MoveWindow(GetDlgItem(hwnd, kOutputSummaryEdit), fieldLeft, top + 346, fullFieldWidth, 46, TRUE);

  moveField(kAppConfigFolderEdit, top, browsedFieldWidth);
  moveBrowse(kAppConfigBrowseBtn, top);
  moveField(kAppLogsFolderEdit, top + 44, browsedFieldWidth);
  moveBrowse(kAppLogsBrowseBtn, top + 44);
  moveCombo(kAppLogModeCombo, top + 80);
  const int pathsLabelAvailableWidth = static_cast<int>(rc.right) - (left + rightPadding + 10);
  const int pathsLabelWidth = (std::max)(280, pathsLabelAvailableWidth);
  MoveWindow(GetDlgItem(hwnd, kAppPathsLabel), left + 10, top + 170, pathsLabelWidth, 104, TRUE);

  MoveWindow(GetDlgItem(hwnd, kSettingsSaveConfig), 20, buttonY, 162, uilayout::kSettingsBottomButtonHeight, TRUE);
  MoveWindow(GetDlgItem(hwnd, kSettingsSaveAsConfig), 192, buttonY, 144, uilayout::kSettingsBottomButtonHeight, TRUE);
  MoveWindow(GetDlgItem(hwnd, kSettingsApply), rc.right - 178, buttonY, 78, uilayout::kSettingsBottomButtonHeight, TRUE);
  MoveWindow(GetDlgItem(hwnd, kSettingsCancel), rc.right - 92, buttonY, 78, uilayout::kSettingsBottomButtonHeight, TRUE);
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      g_ui.settingsTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 12, 12, 840, 500, hwnd,
                                         reinterpret_cast<HMENU>(kSettingsTab), nullptr, nullptr);
      ApplySettingsTabTheme(g_ui.settingsTab);
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
      auto editableCombo = [&](int id, int y, int width, std::vector<HWND>& list) {
        HWND c = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL, fieldLeft, y, width, 300, hwnd,
                               reinterpret_cast<HMENU>(id), nullptr, nullptr);
        AddControl(list, c);
        return c;
      };

      label(L"Port", top, g_ui.serialTabControls);
      HWND port = editableCombo(kSerialPortCombo, top, 430, g_ui.serialTabControls);
      SendMessageW(port, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"COM6"));
      AddControl(g_ui.serialTabControls, CreateWindowW(L"BUTTON", L"Scan Ports", WS_CHILD | WS_VISIBLE, fieldLeft + 440, top, 96, 24, hwnd,
                                                       reinterpret_cast<HMENU>(kSerialScanBtn), nullptr, nullptr));
      AddControl(g_ui.serialTabControls, CreateWindowW(L"BUTTON", L"Test Receive", WS_CHILD | WS_VISIBLE, fieldLeft + 545, top, 100, 24, hwnd,
                                                       reinterpret_cast<HMENU>(kSerialTestBtn), nullptr, nullptr));

      label(L"Baud", top + 36, g_ui.serialTabControls);
      HWND baud = editableCombo(kSerialBaudCombo, top + 36, 645, g_ui.serialTabControls);
      auto baudRates = g_ui.controller->Config().baudRates;
      baudRates.erase(std::remove_if(baudRates.begin(), baudRates.end(), [](int v) { return v <= 0; }), baudRates.end());
      for (int value : {1200, 2400, 4800, 9600}) {
        if (std::find(baudRates.begin(), baudRates.end(), value) == baudRates.end()) baudRates.push_back(value);
      }
      for (int value : baudRates) {
        const auto text = ToWide(std::to_string(value));
        SendMessageW(baud, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
      }

      label(L"Data bits", top + 72, g_ui.serialTabControls);
      HWND bits = editableCombo(kSerialDataBitsCombo, top + 72, 645, g_ui.serialTabControls);
      auto dataBits = g_ui.controller->Config().dataBitsOptions;
      dataBits.erase(std::remove_if(dataBits.begin(), dataBits.end(), [](int v) { return v < 5 || v > 8; }), dataBits.end());
      for (int value : {7, 8}) {
        if (std::find(dataBits.begin(), dataBits.end(), value) == dataBits.end()) dataBits.push_back(value);
      }
      for (int value : dataBits) {
        const auto text = ToWide(std::to_string(value));
        SendMessageW(bits, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
      }

      label(L"Parity", top + 108, g_ui.serialTabControls);
      HWND parity = editableCombo(kSerialParityCombo, top + 108, 645, g_ui.serialTabControls);
      auto parityOptions = g_ui.controller->Config().parityOptions;
      std::vector<std::string> parityClean;
      for (auto value : parityOptions) {
        if (value.empty()) continue;
        value[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[0])));
        if (value[0] == 'N' || value[0] == 'E' || value[0] == 'O') parityClean.emplace_back(1, value[0]);
      }
      for (const char value : {'O', 'N', 'E'}) {
        if (std::find(parityClean.begin(), parityClean.end(), std::string(1, value)) == parityClean.end()) parityClean.emplace_back(1, value);
      }
      for (const auto& value : parityClean) {
        const auto text = ToWide(value);
        SendMessageW(parity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
      }

      label(L"Stop bits", top + 144, g_ui.serialTabControls);
      HWND stopBits = editableCombo(kSerialStopBitsCombo, top + 144, 645, g_ui.serialTabControls);
      auto stopBitsOptions = g_ui.controller->Config().stopBitsOptions;
      std::vector<std::string> stopBitsClean;
      for (const auto& value : stopBitsOptions) {
        if (value == "1" || value == "1.0" || value == "1.5" || value == "2" || value == "2.0") {
          stopBitsClean.push_back(value == "1.0" ? "1" : (value == "2.0" ? "2" : value));
        }
      }
      for (const std::string value : {"1", "1.5", "2"}) {
        if (std::find(stopBitsClean.begin(), stopBitsClean.end(), value) == stopBitsClean.end()) stopBitsClean.push_back(value);
      }
      for (const auto& value : stopBitsClean) {
        const auto text = ToWide(value);
        SendMessageW(stopBits, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
      }

      label(L"Timeout", top + 180, g_ui.serialTabControls);
      HWND timeout = editableCombo(kSerialTimeoutCombo, top + 180, 645, g_ui.serialTabControls);
      for (const wchar_t* value : {L"0.50", L"1.00", L"2.00"}) SendMessageW(timeout, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      label(L"Line ending", top + 216, g_ui.serialTabControls);
      HWND eol = editableCombo(kSerialEolCombo, top + 216, 645, g_ui.serialTabControls);
      for (const wchar_t* value : {L"\\r\\n", L"\\n", L"\\r"}) SendMessageW(eol, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
      AddControl(g_ui.serialTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                 fieldLeft, top + 252, 645, 46, hwnd, reinterpret_cast<HMENU>(kSerialSummaryEdit), nullptr, nullptr));

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
      checkbox(L"Preserve leading plus sign", kOutputPreservePlusCheck, top + 160);
      checkbox(L"Preserve leading minus sign", kOutputPreserveMinusCheck, top + 188);
      checkbox(L"Require numeric result", kOutputRequireNumericCheck, top + 216);
      label(L"After-send action", top + 248, g_ui.outputTabControls);
      HWND postAction = combo(kOutputActionCombo, top + 246, 645, g_ui.outputTabControls);
      for (const wchar_t* value : {L"down", L"right", L"enter", L"tab", L"none", L"custom_sequence"}) {
        SendMessageW(postAction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
      }
      label(L"Custom sequence", top + 284, g_ui.outputTabControls);
      AddControl(g_ui.outputTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY, fieldLeft, top + 282, 645, 24, hwnd,
                                 reinterpret_cast<HMENU>(kOutputCustomSequenceEdit), nullptr, nullptr));
      AddControl(g_ui.outputTabControls,
                 CreateWindowW(L"BUTTON", L"Capture Key", WS_CHILD | WS_VISIBLE, fieldLeft, top + 314, 206, 24, hwnd,
                               reinterpret_cast<HMENU>(kOutputCaptureKeyBtn), nullptr, nullptr));
      AddControl(g_ui.outputTabControls,
                 CreateWindowW(L"BUTTON", L"Remove Last", WS_CHILD | WS_VISIBLE, fieldLeft + 218, top + 314, 206, 24, hwnd,
                               reinterpret_cast<HMENU>(kOutputRemoveLastBtn), nullptr, nullptr));
      AddControl(g_ui.outputTabControls,
                 CreateWindowW(L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE, fieldLeft + 436, top + 314, 209, 24, hwnd,
                               reinterpret_cast<HMENU>(kOutputClearBtn), nullptr, nullptr));
      AddControl(g_ui.outputTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                 fieldLeft, top + 346, 645, 46, hwnd, reinterpret_cast<HMENU>(kOutputSummaryEdit), nullptr, nullptr));

      label(L"Config path", top + 2, g_ui.applicationTabControls);
      AddControl(g_ui.applicationTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, fieldLeft, top, 645, 24, hwnd,
                                 reinterpret_cast<HMENU>(kAppConfigFolderEdit), nullptr, nullptr));
      AddControl(g_ui.applicationTabControls,
                 CreateWindowW(L"BUTTON", L"Browse", WS_CHILD | WS_VISIBLE, fieldLeft + 650, top, 70, 24, hwnd,
                               reinterpret_cast<HMENU>(kAppConfigBrowseBtn), nullptr, nullptr));
      label(L"Logs folder", top + 46, g_ui.applicationTabControls);
      AddControl(g_ui.applicationTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, fieldLeft, top + 44, 645, 24, hwnd,
                                 reinterpret_cast<HMENU>(kAppLogsFolderEdit), nullptr, nullptr));
      AddControl(g_ui.applicationTabControls,
                 CreateWindowW(L"BUTTON", L"Browse", WS_CHILD | WS_VISIBLE, fieldLeft + 650, top + 44, 70, 24, hwnd,
                               reinterpret_cast<HMENU>(kAppLogsBrowseBtn), nullptr, nullptr));
      label(L"Log mode", top + 82, g_ui.applicationTabControls);
      HWND logMode = combo(kAppLogModeCombo, top + 80, 645, g_ui.applicationTabControls);
      for (const wchar_t* value : {L"No file logging", L"New file per session", L"Single file"}) SendMessageW(logMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

      AddControl(g_ui.applicationTabControls,
                 CreateWindowW(L"BUTTON", L"Connect on startup", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, left + 8, top + 142, 220, 24, hwnd,
                               reinterpret_cast<HMENU>(kAppConnectStartupCheck), nullptr, nullptr));
      AddControl(g_ui.applicationTabControls,
                 CreateWindowW(L"BUTTON", L"Dark mode (experimental)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, left + 250, top + 142, 190, 24, hwnd,
                               reinterpret_cast<HMENU>(kAppDarkModeCheck), nullptr, nullptr));
      
      AddControl(g_ui.applicationTabControls,
                 CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                 left + 10, top + 170, 760, 104, hwnd, reinterpret_cast<HMENU>(kAppPathsLabel), nullptr, nullptr));

      for (int comboId : {kSerialPortCombo, kSerialBaudCombo, kSerialDataBitsCombo, kSerialParityCombo, kSerialStopBitsCombo, kSerialTimeoutCombo,
                          kSerialEolCombo, kOutputModeCombo, kOutputActionCombo, kAppLogModeCombo}) {
        SendMessageW(GetDlgItem(hwnd, comboId), CB_SETMINVISIBLE, 8, 0);
      }

      CreateWindowW(L"BUTTON", L"Save Configuration", WS_CHILD | WS_VISIBLE, 20, 520, 140, 32, hwnd,
                    reinterpret_cast<HMENU>(kSettingsSaveConfig), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Save Config As...", WS_CHILD | WS_VISIBLE, 170, 520, 130, 32, hwnd,
                    reinterpret_cast<HMENU>(kSettingsSaveAsConfig), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE, 700, 520, 70, 32, hwnd, reinterpret_cast<HMENU>(kSettingsApply), nullptr,
                    nullptr);
      CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 780, 520, 70, 32, hwnd, reinterpret_cast<HMENU>(kSettingsCancel), nullptr,
                    nullptr);

      ShowTab(0);
      LoadSettingsIntoControls(hwnd);
      LayoutSettingsWindow(hwnd);
      g_ui.settingsNormalizeFocusPending = true;
      PostMessageW(hwnd, kMsgSettingsFinalizeCombos, 0, 0);
      PostMessageW(hwnd, kMsgSettingsFinalizeDisplay, 0, 0);
      return 0;
    }
    case WM_ERASEBKGND: {
      if (!IsDarkModeEnabled()) break;
      RECT rc{};
      GetClientRect(hwnd, &rc);
      FillRect(reinterpret_cast<HDC>(wParam), &rc, g_darkBrush);
      return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
      const auto brush = HandleDarkCtlColor(reinterpret_cast<HDC>(wParam));
      if (brush != 0) return brush;
      break;
    }
    case kMsgSettingsFinalizeCombos:
      FinalizeEditableComboFirstPaint(hwnd);
      return 0;
    case kMsgSettingsFinalizeDisplay:
      FinalizeSettingsDisplay(hwnd);
      return 0;
    case WM_NOTIFY: {
      auto* header = reinterpret_cast<LPNMHDR>(lParam);
      if (header && header->idFrom == kSettingsTab) {
        if (header->code == NM_CUSTOMDRAW) return HandleSettingsTabCustomDraw(lParam);
        if (header->code == TCN_SELCHANGE) ShowTab(static_cast<std::size_t>(TabCtrl_GetCurSel(g_ui.settingsTab)));
      }
      return 0;
    }
    case WM_COMMAND: {
      const WORD commandId = LOWORD(wParam);
      const WORD notifyCode = HIWORD(wParam);
      if (commandId == kSerialPortCombo || commandId == kSerialBaudCombo || commandId == kSerialDataBitsCombo || commandId == kSerialParityCombo ||
          commandId == kSerialStopBitsCombo || commandId == kSerialTimeoutCombo || commandId == kSerialEolCombo) {
        if (IsComboDebugLoggingEnabled() &&
            (notifyCode == CBN_SETFOCUS || notifyCode == CBN_KILLFOCUS || notifyCode == CBN_EDITCHANGE || notifyCode == CBN_SELCHANGE)) {
          std::ostringstream oss;
          const char* notifyName = notifyCode == CBN_SETFOCUS   ? "CBN_SETFOCUS"
                                   : notifyCode == CBN_KILLFOCUS ? "CBN_KILLFOCUS"
                                   : notifyCode == CBN_EDITCHANGE ? "CBN_EDITCHANGE"
                                                                   : "CBN_SELCHANGE";
          oss << "DEBUG_COMBO: field_id=" << commandId << "; phase=NOTIFY; msg=" << notifyName << "; control_hwnd=0x" << std::hex
              << static_cast<std::uintptr_t>(lParam) << "; focus_hwnd=0x" << reinterpret_cast<std::uintptr_t>(GetFocus()) << std::dec;
          AddLogLine(oss.str());
        }
      }
      switch (LOWORD(wParam)) {
        case kSettingsApply:
          ApplySettingsFromControls(hwnd, false);
          return 0;
        case kSettingsSaveConfig:
          ApplySettingsFromControls(hwnd, true);
          return 0;
        case kSettingsSaveAsConfig:
          SaveAsConfigFromControls(hwnd);
          return 0;
        case kSettingsCancel:
          DestroyWindow(hwnd);
          return 0;
        case kSerialScanBtn:
          RefreshPortList(hwnd);
          return 0;
        case kSerialTestBtn:
          RunTestReceive(hwnd);
          return 0;
        case kOutputActionCombo:
          if (HIWORD(wParam) == CBN_SELCHANGE) UpdateCustomSequenceUiState(hwnd);
          return 0;
        case kOutputModeCombo:
          if (HIWORD(wParam) == CBN_SELCHANGE) UpdateParseControlsUiState(hwnd);
          return 0;
        case kOutputCaptureKeyBtn:
          g_ui.captureCustomSequenceKey = true;
          SetFocus(hwnd);
          AddLogLine("Press a key to append to custom sequence.");
          return 0;
        case kOutputRemoveLastBtn: {
          const auto text = ToUtf8(GetControlText(GetDlgItem(hwnd, kOutputCustomSequenceEdit)));
          auto pos = text.find_last_of(',');
          const auto trimmed = pos == std::string::npos ? std::string() : text.substr(0, pos);
          SetWindowTextW(GetDlgItem(hwnd, kOutputCustomSequenceEdit), ToWide(trimmed).c_str());
          return 0;
        }
        case kOutputClearBtn:
          SetWindowTextW(GetDlgItem(hwnd, kOutputCustomSequenceEdit), L"");
          return 0;
        case kAppLogsBrowseBtn: {
          std::wstring selected;
          auto current = GetControlText(GetDlgItem(hwnd, kAppLogsFolderEdit));
          if (current.empty() || !std::filesystem::exists(current)) current = ToWide(g_ui.controller->Config().logsFolder);
          if (BrowseForFolder(hwnd, selected, current)) SetWindowTextW(GetDlgItem(hwnd, kAppLogsFolderEdit), selected.c_str());
          return 0;
        }
        case kAppConfigBrowseBtn: {
          std::wstring selected;
          auto current = GetControlText(GetDlgItem(hwnd, kAppConfigFolderEdit));
          auto currentPath = std::filesystem::path(current);
          auto currentDir = currentPath.parent_path();
          if (currentDir.empty() || !std::filesystem::exists(currentDir)) currentDir = std::filesystem::path(g_ui.controller->Config().configFolder);
          if (BrowseForFolder(hwnd, selected, currentDir.wstring())) {
            auto selectedPath = std::filesystem::path(selected);
            const auto fileNamePath = currentPath.filename().empty()
                                          ? std::filesystem::path(g_ui.controller->Config().configFileName)
                                          : currentPath.filename();
            selectedPath /= fileNamePath;
            SetWindowTextW(GetDlgItem(hwnd, kAppConfigFolderEdit), selectedPath.wstring().c_str());
          }
          return 0;
        }
        default:
          return 0;
      }
    }
    case WM_KEYDOWN:
      if (g_ui.captureCustomSequenceKey) {
        g_ui.captureCustomSequenceKey = false;
        char keyName[32]{};
        const LONG scan = static_cast<LONG>(MapVirtualKeyA(static_cast<UINT>(wParam), MAPVK_VK_TO_VSC) << 16);
        GetKeyNameTextA(scan, keyName, sizeof(keyName));
        if (keyName[0] == '\0') wsprintfA(keyName, "VK_%u", static_cast<unsigned>(wParam));
        const auto existing = ToUtf8(GetControlText(GetDlgItem(hwnd, kOutputCustomSequenceEdit)));
        const auto updated = existing.empty() ? std::string(keyName) : (existing + "," + keyName);
        SetWindowTextW(GetDlgItem(hwnd, kOutputCustomSequenceEdit), ToWide(updated).c_str());
        return 0;
      }
      break;
    case WM_SIZE:
      LayoutSettingsWindow(hwnd);
      PostMessageW(hwnd, kMsgSettingsFinalizeCombos, 0, 0);
      PostMessageW(hwnd, kMsgSettingsFinalizeDisplay, 0, 0);
      return 0;
    case WM_GETMINMAXINFO: {
      auto* mm = reinterpret_cast<MINMAXINFO*>(lParam);
      mm->ptMinTrackSize.x = 700;
      mm->ptMinTrackSize.y = 620;
      return 0;
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      g_ui.settingsWindow = nullptr;
      g_ui.settingsNormalizeFocusPending = false;
      g_ui.serialTabControls.clear();
      g_ui.outputTabControls.clear();
      g_ui.applicationTabControls.clear();
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
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
                                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT, 780, 610,
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
    case kMsgUiLogLine: {
      std::unique_ptr<std::string> payload(reinterpret_cast<std::string*>(lParam));
      if (payload) AddLogLine(*payload);
      return 0;
    }
    case kMsgUiConnectionState:
      UpdateConnectionUi(wParam != 0);
      return 0;
    case kMsgStartupAutoConnect:
      AddLogLine("Handling deferred startup auto-connect.");
      if (g_ui.controller) {
        const auto serial = g_ui.controller->Settings().serial;
        if (serial.port.empty()) {
          AddLogLine("Startup auto-connect skipped: empty startup port.");
          return 0;
        }
        if (!IsLikelySerialPortName(serial.port)) {
          AddLogLine("Startup auto-connect skipped: malformed startup port '" + serial.port + "'.");
          return 0;
        }
        const auto scannedPorts = g_ui.controller->ScanPorts();
        if (!PortExistsInScan(scannedPorts, serial.port)) {
          AddLogLine("Startup port not present in current scan: " + serial.port);
          AddLogLine("Startup auto-connect skipped because requested port is unavailable.");
          return 0;
        }
        if (const char* delayMsText = std::getenv("SCALELOGGER_CONNECT_DELAY_MS")) {
          try {
            const int delayMs = (std::max)(0, std::stoi(delayMsText));
            if (delayMs > 0) {
              AddLogLine("Delaying startup auto-connect by " + std::to_string(delayMs) + " ms (environment override).");
              Sleep(static_cast<DWORD>(delayMs));
            }
          } catch (...) {
            AddLogLine("WARN: Ignoring invalid SCALELOGGER_CONNECT_DELAY_MS value.");
          }
        }
        try {
          g_ui.controller->Connect();
        } catch (const std::exception& ex) {
          AddLogLine(std::string("ERROR: Unhandled exception during startup connect: ") + ex.what());
        } catch (...) {
          AddLogLine("ERROR: Unhandled non-standard exception during startup connect.");
        }
      }
      return 0;
    case WM_ERASEBKGND: {
      RECT rc{};
      GetClientRect(hwnd, &rc);
      FillRect(reinterpret_cast<HDC>(wParam), &rc, IsDarkModeEnabled() ? g_darkBrush : reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));
      return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
      if (reinterpret_cast<HWND>(lParam) == g_ui.connectionIndicator) {
        auto* dc = reinterpret_cast<HDC>(wParam);
        COLORREF indicatorColor = RGB(196, 64, 64);
        if (g_connectionUiState == ConnectionUiState::Connected) indicatorColor = RGB(56, 166, 84);
        else if (g_connectionUiState == ConnectionUiState::Connecting) indicatorColor = RGB(197, 160, 27);
        SetTextColor(dc, indicatorColor);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(IsDarkModeEnabled() ? g_darkBrush : GetSysColorBrush(COLOR_BTNFACE));
      }
      const auto brush = HandleDarkCtlColor(reinterpret_cast<HDC>(wParam));
      if (brush != 0) return brush;
      break;
    }
    case WM_GETMINMAXINFO: {
      auto* mm = reinterpret_cast<MINMAXINFO*>(lParam);
      mm->ptMinTrackSize.x = 540;
      mm->ptMinTrackSize.y = 420;
      return 0;
    }
    case WM_COMMAND: {
      const int id = LOWORD(wParam);
      switch (id) {
        case kBtnConnect:
          if (g_ui.controller->IsConnected()) {
            g_ui.controller->Disconnect();
          } else {
            UpdateConnectionUi(ConnectionUiState::Connecting);
            g_ui.controller->Connect();
          }
          return 0;
        case kBtnSettings:
          if (HIWORD(wParam) == BN_CLICKED) OpenSettingsWindow(g_ui.hInstance);
          return 0;
        case kBtnAbout:
          if (HIWORD(wParam) == BN_CLICKED) ShowAbout(hwnd);
          return 0;
        default:
          return 0;
      }
    }
    case WM_QUERYENDSESSION:
      AddLogLine("Session end: system shutdown/logoff");
      return TRUE;
    case WM_ENDSESSION:
      if (wParam) AddLogLine("Session end: system shutdown/logoff");
      return 0;
    case WM_CLOSE:
      AddLogLine("Session end: user requested close");
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      if (g_ui.controller && g_ui.controller->IsConnected()) {
        AddLogLine("Disconnecting serial before shutdown");
        g_ui.controller->Disconnect();
      }
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
} // namespace

#if defined(_MSC_VER)
#define SCALELOGGER_NOINLINE __declspec(noinline)
#else
#define SCALELOGGER_NOINLINE __attribute__((noinline))
#endif

int GetRunMainStageLimit() {
  const char* raw = std::getenv("SCALELOGGER_RUNMAIN_STAGE_LIMIT");
  if (!raw || !*raw) return -1;
  char* end = nullptr;
  const long parsed = std::strtol(raw, &end, 10);
  if (end == raw || (end && *end != '\0') || parsed < 0 || parsed > 1000) return -1;
  return static_cast<int>(parsed);
}

bool ShouldReturnAtStage(int stageLimit, int stage) {
  return stageLimit >= 0 && stageLimit == stage;
}

static int RunMainDialogImpl(HINSTANCE hInstance, int nCmdShow);

int ProbeMainDialogBasic() {
  OutputDebugStringA("TRACE: ProbeMainDialogBasic entered\n");
  return 101;
}

int ProbeMainDialogTraceEarly() {
  TraceEarly("TRACE: ProbeMainDialogTraceEarly entered");
  return 102;
}

SCALELOGGER_NOINLINE int ProbeMainDialogWithArgs(HINSTANCE, int nCmdShow) {
  TraceEarly("TRACE: ProbeMainDialogWithArgs entered nCmdShow=" + std::to_string(nCmdShow));
  return 103;
}

SCALELOGGER_NOINLINE int ProbeMainDialogTouchUi(HINSTANCE hInstance) {
  TraceEarly("TRACE: ProbeMainDialogTouchUi before g_ui write");
  g_ui.hInstance = hInstance;
  TraceEarly("TRACE: ProbeMainDialogTouchUi after g_ui write");
  return 104;
}

SCALELOGGER_NOINLINE int ProbeRunMainDialogImplDirect(HINSTANCE hInstance, int nCmdShow) {
  TraceEarly("TRACE: ProbeRunMainDialogImplDirect entered");
  return RunMainDialogImpl(hInstance, nCmdShow);
}

static SCALELOGGER_NOINLINE int RunMainDialogImpl(HINSTANCE hInstance, int nCmdShow) {
  const int stageLimit = GetRunMainStageLimit();
  TraceEarly("TRACE: RunMainDialogImpl stage 0 entered");
  if (ShouldReturnAtStage(stageLimit, 0)) {
    TraceEarly("TRACE: RunMainDialogImpl early return at stage 0");
    return 200;
  }
  TraceEarly("TRACE: RunMainDialogImpl entered");
  TraceEarly("TRACE: RunMainDialog function entry A");
  TraceEarly("TRACE: Before first TraceEarly self-test");
  TraceEarly("TRACE: After first TraceEarly self-test");
  TraceEarly("TRACE: RunMainDialogImpl stage 1 after first local/setup");
  if (ShouldReturnAtStage(stageLimit, 1)) {
    TraceEarly("TRACE: RunMainDialogImpl early return at stage 1");
    return 201;
  }
  TraceEarly("TRACE: Before storing hInstance");
  g_ui.hInstance = hInstance;
  TraceEarly("TRACE: After storing hInstance");
  TraceEarly("TRACE: RunMainDialogImpl stage 2 after g_ui assignment");
  if (ShouldReturnAtStage(stageLimit, 2)) {
    TraceEarly("TRACE: RunMainDialogImpl early return at stage 2");
    return 202;
  }
  TraceEarly("TRACE: Before INITCOMMONCONTROLSEX construction");
  INITCOMMONCONTROLSEX icc{sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES};
  TraceEarly("TRACE: After INITCOMMONCONTROLSEX construction cbSize=" + std::to_string(icc.dwSize) + " classes=" + std::to_string(icc.dwICC));
  TraceEarly("TRACE: RunMainDialogImpl stage 3 after INITCOMMONCONTROLSEX construction");
  if (ShouldReturnAtStage(stageLimit, 3)) {
    TraceEarly("TRACE: RunMainDialogImpl early return at stage 3");
    return 203;
  }
  TraceEarly("TRACE: Before InitCommonControlsEx");
  const BOOL initCommonControlsOk = InitCommonControlsEx(&icc);
  const DWORD initCommonControlsGle = GetLastError();
  TraceEarly("TRACE: After InitCommonControlsEx result=" + std::to_string(initCommonControlsOk) + " gle=" + std::to_string(initCommonControlsGle));
  TraceEarly("TRACE: RunMainDialogImpl stage 4 after InitCommonControlsEx result=" + std::to_string(initCommonControlsOk) +
             " gle=" + std::to_string(initCommonControlsGle));
  if (ShouldReturnAtStage(stageLimit, 4)) {
    TraceEarly("TRACE: RunMainDialogImpl early return at stage 4");
    return 204;
  }
  TraceEarly("TRACE: RunMainDialogImpl stage 5 before next real startup step");
  if (ShouldReturnAtStage(stageLimit, 5)) {
    TraceEarly("TRACE: RunMainDialogImpl early return at stage 5");
    return 205;
  }

  TraceEarly("TRACE: Before data-root resolution");
  const char* userProfile = std::getenv("USERPROFILE");
  const char* localAppData = std::getenv("LOCALAPPDATA");
  std::filesystem::path dataRoot = userProfile     ? std::filesystem::path(userProfile) / "ScaleLogger"
                                   : localAppData ? std::filesystem::path(localAppData) / "ScaleLogger"
                                                  : (std::filesystem::temp_directory_path() / "ScaleLogger");
  TraceEarly("TRACE: After data-root resolution path=" + dataRoot.string());
  TraceEarly("TRACE: Before creating data/log directories");
  std::error_code ec;
  std::filesystem::create_directories(dataRoot, ec);
  if (ec) TraceEarly("WARN: Failed to create data root directory: " + dataRoot.string() + " error=" + std::to_string(ec.value()));
  ec.clear();
  std::filesystem::create_directories(dataRoot / "logs", ec);
  if (ec) TraceEarly("WARN: Failed to create logs directory: " + (dataRoot / "logs").string() + " error=" + std::to_string(ec.value()));
  TraceEarly("TRACE: After creating data/log directories");
  TraceEarly("TRACE: Before AppController construction");
  g_ui.controller = std::make_unique<AppController>(dataRoot);
  TraceEarly("TRACE: After AppController construction");

  TraceEarly("TRACE: Before log/connection sink hookup");
  g_ui.controller->SetLogSink([](const std::string& message, bool isError) { PostLogLineToUiThread((isError ? "ERROR: " : "") + message); });
  g_ui.controller->SetConnectionStateSink([](bool connected) { PostConnectionStateToUiThread(connected); });
  TraceEarly("TRACE: After log/connection sink hookup");

  WNDCLASSW wc{};
  wc.lpfnWndProc = MainWndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"ScaleLoggerMainWindow";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  TraceEarly("TRACE: Before RegisterClassW");
  RegisterClassW(&wc);
  TraceEarly("TRACE: After RegisterClassW success");

  const std::wstring mainWindowTitle = std::wstring(L"ScaleLogger ") + kAppVersionWide;
  TraceEarly("TRACE: Before CreateWindowExW");
  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, mainWindowTitle.c_str(), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 860, 600, nullptr, nullptr, hInstance, nullptr);

  if (!hwnd) {
    TraceEarly("ERROR: CreateWindowExW failed, gle=" + std::to_string(GetLastError()));
    return 1;
  }
  TraceEarly("TRACE: After CreateWindowExW success");

  TraceEarly("TRACE: Before ShowWindow");
  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);
  TraceEarly("TRACE: After ShowWindow/UpdateWindow");

  TraceEarly("TRACE: Before g_ui.controller->Initialize()");
  g_ui.controller->Initialize();
  TraceEarly("TRACE: After g_ui.controller->Initialize()");
  UpdateConnectionUi(g_ui.controller->IsConnected());
  const auto& cfg = g_ui.controller->Config();
  if (cfg.darkMode) AddLogLine(std::string("Dark mode is experimental in ") + kAppVersion + " and is disabled by default.");
  const char* disableStartupConnect = std::getenv("SCALELOGGER_DISABLE_STARTUP_CONNECT");
  AddLogLine(std::string("TRACE: Env SCALELOGGER_DISABLE_STARTUP_CONNECT=") + (disableStartupConnect ? disableStartupConnect : "<unset>"));
  TraceEarly("TRACE: Before startup auto-connect decision");
  const bool startupConnectDisabledByEnv = disableStartupConnect && std::string(disableStartupConnect) == "1";
  if (cfg.connectOnStartup && startupConnectDisabledByEnv) {
    AddLogLine("TRACE: Startup auto-connect DISABLED by env override");
    AddLogLine("Startup auto-connect disabled by environment override.");
  } else if (cfg.connectOnStartup) {
    AddLogLine("Posting deferred startup auto-connect.");
    PostMessageW(hwnd, kMsgStartupAutoConnect, 0, 0);
  }
  TraceEarly("TRACE: After startup auto-connect decision path");

  AddLogLine("Scanning serial ports...");
  const auto ports = g_ui.controller->ScanPorts();
  std::string joined;
  for (std::size_t i = 0; i < ports.size(); ++i) {
    if (i) joined += ", ";
    joined += ports[i];
  }
  AddLogLine("Detected " + std::to_string(ports.size()) + " ports" + (joined.empty() ? "." : (": " + joined)));

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  if (g_ui.controller) g_ui.controller->LogMessage("Session end: normal shutdown");
  g_ui.controller.reset();
  return static_cast<int>(msg.wParam);
}

SCALELOGGER_NOINLINE int RunMainDialog(HINSTANCE hInstance, int nCmdShow) {
  TraceEarly("TRACE: RunMainDialog wrapper entered");
  return RunMainDialogImpl(hInstance, nCmdShow);
}

} // namespace scalelogger
#endif
