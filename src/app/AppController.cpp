#include "app/AppController.hpp"

#include "app/ConfigService.hpp"

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {
namespace {
std::wstring Utf8ToWide(const std::string& text) {
#ifdef _WIN32
  if (text.empty()) return {};
  const int sizeNeeded = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
  if (sizeNeeded <= 0) return std::wstring(text.begin(), text.end());
  std::wstring out(static_cast<std::size_t>(sizeNeeded), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()), out.data(), sizeNeeded);
  return out;
#else
  return std::wstring(text.begin(), text.end());
#endif
}

std::string EscapeForLog(const std::string& value) {
  std::string out;
  out.reserve(value.size() * 2);
  for (char ch : value) {
    switch (ch) {
      case '\r': out += "\\r"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      default: out.push_back(ch); break;
    }
  }
  return out;
}

void SanitizeSerialSettings(AppSettings& settings) {
  if (settings.serial.port.empty()) settings.serial.port = "COM6";
  if (settings.serial.baudRate <= 0) settings.serial.baudRate = 1200;
  if (settings.serial.dataBits < 5 || settings.serial.dataBits > 8) settings.serial.dataBits = 7;
  const char parity = static_cast<char>(std::toupper(static_cast<unsigned char>(settings.serial.parity)));
  settings.serial.parity = (parity == 'N' || parity == 'E' || parity == 'O') ? parity : 'O';
  if (!(settings.serial.stopBits == 1.0F || settings.serial.stopBits == 1.5F || settings.serial.stopBits == 2.0F)) settings.serial.stopBits = 1.0F;
  if (settings.serial.timeoutSeconds <= 0.0F) settings.serial.timeoutSeconds = 1.0F;
  if (settings.serial.eol.empty()) settings.serial.eol = "\r\n";
}

std::string FormatStopBits(float value) {
  if (value == 1.5F) return "1.5";
  if (value >= 1.9F) return "2";
  return "1";
}

std::string FormatTimeout(float value) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << value;
  return oss.str();
}

void ResolveAndSanitize(std::filesystem::path dataRoot, AppSettings& settings, AppConfig& config) {
  SanitizeSerialSettings(settings);
  ConfigService::SanitizeConfig(config);
  config.configFolder = ConfigService::ResolveConfiguredPath(dataRoot, config.configFolder).string();
  config.logsFolder = ConfigService::ResolveConfiguredPath(dataRoot, config.logsFolder).string();
}

} // namespace

AppController::AppController(std::filesystem::path dataRoot)
    : dataRoot_(std::move(dataRoot)), configPath_(dataRoot_ / "ScaleLogger.config.json") {}

void AppController::Initialize() {
  EmitLog("Initialize begin");
  EmitLog("Startup data root: " + dataRoot_.string());

  AppConfig loadedConfig{};
  AppSettings loadedSettings{};
  std::string source = "in-memory defaults";
  std::string resolutionError;

  const auto userConfigPath = configPath_;
  const auto defaultConfigPath = ConfigService::ResolveDefaultConfigPath(dataRoot_);

  try {
    if (std::filesystem::exists(userConfigPath)) {
      loadedConfig = LoadConfig(userConfigPath);
      loadedSettings = LoadConfigSettings(userConfigPath);
      source = "user config: " + userConfigPath.string();
    } else if (std::filesystem::exists(defaultConfigPath)) {
      loadedConfig = LoadConfig(defaultConfigPath);
      loadedSettings = LoadConfigSettings(defaultConfigPath);
      source = "default config: " + defaultConfigPath.string();
    }
    ResolveAndSanitize(dataRoot_, loadedSettings, loadedConfig);
    configPath_ = std::filesystem::path(loadedConfig.configFolder) / loadedConfig.configFileName;
  } catch (const std::exception& ex) {
    loadedConfig = AppConfig{};
    loadedSettings = AppSettings{};
    source = "in-memory defaults";
    resolutionError = ex.what();
    try {
      ResolveAndSanitize(dataRoot_, loadedSettings, loadedConfig);
      configPath_ = std::filesystem::path(loadedConfig.configFolder) / loadedConfig.configFileName;
    } catch (...) {
      // Keep startup alive with plain defaults if sanitize/resolve unexpectedly fails.
    }
  }

  settings_ = loadedSettings;
  config_ = loadedConfig;
  if (configPath_.empty()) {
    configPath_ = std::filesystem::path(config_.configFolder) / config_.configFileName;
  }

  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(config_.logsFolder), ec);
  if (ec) EmitLog("WARN: Failed to create logs directory: " + std::filesystem::path(config_.logsFolder).string(), true);

  if (!resolutionError.empty()) {
    EmitLog("ERROR: Startup config resolution failed; using defaults. " + resolutionError, true);
  }
  EmitLog("Startup config source: " + source);
  EmitLog("Startup effective config path: " + configPath_.string());
  EmitLog("Startup serial: port=" + settings_.serial.port + "; baudrate=" + std::to_string(settings_.serial.baudRate) +
          "; databits=" + std::to_string(settings_.serial.dataBits) + "; parity=" + std::string(1, settings_.serial.parity) +
          "; stopbits=" + FormatStopBits(settings_.serial.stopBits) + "; timeout=" + FormatTimeout(settings_.serial.timeoutSeconds) +
          "; eol=" + EscapeForLog(settings_.serial.eol));
  EmitLog("Initialize end");
}

void AppController::Connect() {
  EmitLog("Connect begin");
  if (connected_ || serial_.IsConnected()) {
    connected_ = true;
    EmitConnectionState(true);
    EmitLog("Connect skipped: already connected");
    return;
  }

  const bool connected = serial_.Connect(
      settings_.serial,
      [this](const std::string& rawLine) { HandleSerialLine(rawLine); },
      [this](const std::string& m) { EmitLog(m); },
      [this](const std::string& m) { EmitLog(m, true); });

  connected_ = connected;
  EmitConnectionState(connected_);
  EmitLog(std::string("Connect end: ") + (connected_ ? "success" : "failed"));
}

void AppController::Disconnect() {
  if (serial_.IsConnected()) serial_.Disconnect();
  connected_ = false;
  EmitConnectionState(false);
  EmitLog("Disconnected");
}

void AppController::ApplySettings(const AppSettings& nextSettings, const AppConfig& nextConfig, bool persistToDisk) {
  AppSettings resolvedSettings = nextSettings;
  AppConfig resolvedConfig = nextConfig;
  ResolveAndSanitize(dataRoot_, resolvedSettings, resolvedConfig);

  const bool settingsChanged = ConfigService::CountSettingsDifferences(settings_, resolvedSettings) > 0;
  const bool configChanged = ConfigService::CountConfigDifferences(config_, resolvedConfig) > 0;
  if (!settingsChanged && !configChanged) return;

  const bool serialReconnectRequired = SerialSettingsRequireReconnect(settings_.serial, resolvedSettings.serial);

  settings_ = resolvedSettings;
  config_ = resolvedConfig;
  configPath_ = std::filesystem::path(config_.configFolder) / config_.configFileName;

  if (persistToDisk) {
    if (SaveConfig(configPath_, config_, &settings_)) EmitLog("Configuration saved");
    else EmitLog("ERROR: Failed to save configuration: " + configPath_.string(), true);
  }

  if (connected_ && serialReconnectRequired) {
    EmitLog("Reconnecting due to serial settings update");
    Disconnect();
    Connect();
  }
}

AppController::SaveConfigResult AppController::SaveResolvedConfiguration() {
  SaveConfigResult result{};
  result.path = configPath_;

  AppConfig resolvedConfig = config_;
  AppSettings resolvedSettings = settings_;
  ResolveAndSanitize(dataRoot_, resolvedSettings, resolvedConfig);

  const bool existedBeforeSave = std::filesystem::exists(result.path);
  if (!SaveConfig(result.path, resolvedConfig, &resolvedSettings)) {
    result.status = SaveConfigStatus::Failed;
    return result;
  }

  config_ = resolvedConfig;
  settings_ = resolvedSettings;
  result.changedFieldCount = 0;
  result.status = existedBeforeSave ? SaveConfigStatus::Updated : SaveConfigStatus::Created;
  return result;
}

std::vector<std::string> AppController::ScanPorts() const { return ScanComPorts(); }

void AppController::SetLogSink(LogSink sink) { logSink_ = std::move(sink); }

void AppController::SetConnectionStateSink(ConnectionStateSink sink) { connectionStateSink_ = std::move(sink); }

void AppController::LogMessage(const std::string& message, bool isError) { EmitLog(message, isError); }

bool AppController::IsConnected() const { return connected_ || serial_.IsConnected(); }

void AppController::EmitLog(const std::string& message, bool isError) const {
  if (logSink_) logSink_(message, isError);
}

void AppController::EmitConnectionState(bool connected) const {
  if (connectionStateSink_) connectionStateSink_(connected);
}

void AppController::HandleSerialLine(const std::string& rawLine) {
  try {
    const auto parsed = parser_.Process(rawLine, settings_.parsing);
    if (!parsed.ok) {
      EmitLog("Parse rejected: " + parsed.message + " raw='" + rawLine + "'", true);
      return;
    }
    HandleParsedValue(rawLine, parsed.processed);
  } catch (const std::exception& ex) {
    EmitLog(std::string("ERROR: Exception while handling serial line callback: ") + ex.what(), true);
  }
}

void AppController::HandleParsedValue(const std::string& rawLine, const std::string& processedValue) {
  EmitLog("Raw: '" + rawLine + "' -> Parsed: '" + processedValue + "'");
  const auto sendResult = injector_.SendTextAndAction(Utf8ToWide(processedValue), settings_.output);
  if (sendResult.status == InputInjector::SendStatus::Success) {
    if (config_.lineLogMode == LineLogMode::Verbose) {
      EmitLog("Injection succeeded for parsed value: '" + processedValue + "'");
    }
    return;
  }
  if (sendResult.status == InputInjector::SendStatus::TextFailed) {
    EmitLog("Text injection failed for value: '" + processedValue + "'", true);
    return;
  }
  if (!sendResult.failedToken.empty()) {
    EmitLog("Post-action failed for token: " + sendResult.failedToken, true);
  } else {
    EmitLog("Post-action key injection failed", true);
  }
}

} // namespace scalelogger
