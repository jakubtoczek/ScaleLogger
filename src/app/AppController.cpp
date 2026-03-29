#include "app/AppController.hpp"
#include "app/ConfigService.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
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

std::string FormatStopBitsForLog(float value) {
  if (value == 1.5F) return "1.5";
  if (value >= 1.9F) return "2";
  return "1";
}

std::string FormatTimeoutForLog(float value) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << value;
  return oss.str();
}

std::string UpperAscii(std::string text) {
  for (char& ch : text) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  return text;
}

bool PortNamesMatch(const std::string& lhs, const std::string& rhs) { return UpperAscii(lhs) == UpperAscii(rhs); }

} // namespace

AppController::AppController(std::filesystem::path dataRoot)
    : dataRoot_(std::move(dataRoot)), configPath_(dataRoot_ / "ScaleLogger.config.json") {}

void AppController::Initialize() {
  EmitLog("Initialize begin");
  std::string startupSerialSource = "config defaults";
  try {
    const bool hasUserConfig = std::filesystem::exists(configPath_);
    EmitLog("Startup data root: " + dataRoot_.string());
    EmitLog("Startup config path: " + configPath_.string());
    EmitLog(hasUserConfig ? ("Startup config found: " + configPath_.string()) : ("Startup config not found: " + configPath_.string()));

    config_ = LoadConfig(configPath_);
    ConfigService::SanitizeConfig(config_);
    settings_ = LoadPreset(configPath_);
    EmitLog("Startup config source: " + std::string(hasUserConfig ? "disk config file" : "defaults from missing config"));
    if (!hasUserConfig) {
      const auto defaultConfigPath = std::filesystem::current_path() / "default_config.json";
      if (std::filesystem::exists(defaultConfigPath)) {
        config_ = LoadConfig(defaultConfigPath);
        ConfigService::SanitizeConfig(config_);
        settings_ = LoadPreset(defaultConfigPath);
        startupSerialSource = "built-in defaults";
        EmitLog("Startup config source: default_config.json");
      } else {
        startupSerialSource = "built-in defaults";
        EmitLog("Startup config source: built-in defaults");
      }
    }

    if (settings_.serial.port.empty()) settings_.serial.port = "COM6";
    if (settings_.serial.baudRate <= 0) settings_.serial.baudRate = 1200;
    if (settings_.serial.dataBits < 5 || settings_.serial.dataBits > 8) settings_.serial.dataBits = 7;
    const char parity = static_cast<char>(std::toupper(static_cast<unsigned char>(settings_.serial.parity)));
    settings_.serial.parity = (parity == 'N' || parity == 'E' || parity == 'O') ? parity : 'O';
    if (!(settings_.serial.stopBits == 1.0F || settings_.serial.stopBits == 1.5F || settings_.serial.stopBits == 2.0F)) settings_.serial.stopBits = 1.0F;
    if (settings_.serial.timeoutSeconds <= 0.0F) settings_.serial.timeoutSeconds = 1.0F;
    if (settings_.serial.eol.empty()) settings_.serial.eol = "\r\n";

    config_.configFolder = ConfigService::ResolveConfiguredPath(dataRoot_, config_.configFolder).string();
    config_.presetsFolder = ConfigService::ResolveConfiguredPath(dataRoot_, config_.presetsFolder).string();
    config_.logsFolder = ConfigService::ResolveConfiguredPath(dataRoot_, config_.logsFolder).string();
    if (config_.configFileName.empty()) config_.configFileName = "ScaleLogger.config.json";
    configPath_ = std::filesystem::path(config_.configFolder) / config_.configFileName;

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(config_.logsFolder), ec);
    if (ec) EmitLog("WARN: Failed to create logs directory: " + std::filesystem::path(config_.logsFolder).string(), true);
    ec.clear();
    std::filesystem::create_directories(std::filesystem::path(config_.presetsFolder), ec);
    if (ec) EmitLog("WARN: Failed to create presets directory: " + std::filesystem::path(config_.presetsFolder).string(), true);
    FlushBufferedFileLogs();

    const auto presetsDir = std::filesystem::path(config_.presetsFolder);
    std::filesystem::path startupPresetPath;
    std::string startupPresetSource;
    std::string requestedStartupPreset;
    if (config_.startupMode == "specific_preset") {
      startupPresetSource = "startup preset";
      requestedStartupPreset = config_.startupPresetName;
    } else if (config_.startupMode == "last_used_preset") {
      startupPresetSource = "last-used preset";
      requestedStartupPreset = config_.lastUsedPresetName;
    }
    EmitLog("Startup mode: " + config_.startupMode);
    EmitLog("Startup preset name: " + (config_.startupPresetName.empty() ? "<empty>" : config_.startupPresetName));
    EmitLog("Last-used preset name: " + (config_.lastUsedPresetName.empty() ? "<empty>" : config_.lastUsedPresetName));
    if (!requestedStartupPreset.empty()) {
      startupPresetPath = ConfigService::ResolvePresetPath(presetsDir, requestedStartupPreset);
      std::error_code presetEc;
      const bool presetExists = std::filesystem::exists(startupPresetPath, presetEc) && !presetEc;
      EmitLog("Startup preset resolved path: " + startupPresetPath.string());
      EmitLog("Startup preset file exists: " + std::string(presetExists ? "yes" : "no"));
      if (presetExists) {
        try {
          bool usedLegacyCompatibilityMapping = false;
          settings_ = LoadPreset(startupPresetPath, &usedLegacyCompatibilityMapping);
          startupSerialSource = startupPresetSource + " '" + requestedStartupPreset + "'";
          EmitLog("Loaded startup preset: " + startupPresetPath.filename().string());
          if (usedLegacyCompatibilityMapping) {
            EmitLog("Loaded preset with legacy compatibility mapping");
          }
        } catch (const std::exception& presetEx) {
          EmitLog("WARN: Startup preset load failed: " + std::string(presetEx.what()), true);
        }
      } else {
        EmitLog("WARN: Startup preset missing or invalid: " + startupPresetPath.string(), true);
      }
    } else if (config_.startupMode == "specific_preset" || config_.startupMode == "last_used_preset") {
      EmitLog("Startup preset not requested: no preset name resolved.");
    }

    EmitLog("Startup effective serial source: " + startupSerialSource);
    EmitLog("Startup effective serial: port=" + settings_.serial.port + "; baudrate=" + std::to_string(settings_.serial.baudRate) +
            "; databits=" + std::to_string(settings_.serial.dataBits) + "; parity=" + std::string(1, settings_.serial.parity) +
            "; stopbits=" + FormatStopBitsForLog(settings_.serial.stopBits) + "; timeout=" + FormatTimeoutForLog(settings_.serial.timeoutSeconds) +
            "; eol=" + EscapeForLog(settings_.serial.eol));
    const std::string logModeText =
        config_.logMode == LogMode::SingleFile ? "single_file" : (config_.logMode == LogMode::None ? "none" : "per_session");
    EmitLog("Startup effective logging: folder=" + config_.logsFolder + "; mode=" + logModeText + "; pattern=" + config_.logFilePattern);
    EmitLog(config_.connectOnStartup ? "Startup auto-connect enabled" : "Startup auto-connect disabled");
    EmitLog("Startup load completed");
  } catch (const std::exception& ex) {
    config_ = AppConfig{};
    settings_ = AppSettings{};
    config_.configFolder = ConfigService::ResolveConfiguredPath(dataRoot_, config_.configFolder).string();
    config_.presetsFolder = ConfigService::ResolveConfiguredPath(dataRoot_, config_.presetsFolder).string();
    config_.logsFolder = ConfigService::ResolveConfiguredPath(dataRoot_, config_.logsFolder).string();
    configPath_ = std::filesystem::path(config_.configFolder) / config_.configFileName;
    FlushBufferedFileLogs();
    EmitLog("Startup effective serial source: fallback defaults after startup-load failure");
    EmitLog(std::string("ERROR: Startup config/preset load failed. Using defaults. ") + ex.what(), true);
  }
  EmitLog("Initialize end");
  EmitLog("Application start");
}

void AppController::Connect() {
  EmitLog("Connect begin");
  if (const char* forceNoSerial = std::getenv("SCALELOGGER_FORCE_NO_SERIAL")) {
    if (std::string(forceNoSerial) == "1") {
      EmitLog("Serial subsystem fully disabled by env override");
      connected_ = false;
      EmitConnectionState(false);
      EmitLog("Connect end: skipped by environment override");
      return;
    }
  }
  if (connected_) {
    EmitLog("Connect end: already connected");
    return;
  }

  EmitLog("TRACE: AppController::Connect before serial_.Connect()");
  const bool connected = serial_.Connect(
      settings_.serial,
      [this](const std::string& rawLine) {
        try {
          const auto parsed = parser_.Process(rawLine, settings_.parsing);
          if (!parsed.ok) {
            if (config_.lineLogMode == LineLogMode::Compact) {
              EmitLog("Scale input raw='" + rawLine + "' parse_error='" + parsed.message + "'", true);
            } else {
              EmitLog("Raw received line: '" + rawLine + "'");
            }
            EmitLog("Parse rejected: " + parsed.message + " raw='" + rawLine + "'", true);
            return;
          }
          if (config_.lineLogMode == LineLogMode::Compact) {
            EmitLog("Scale input raw='" + rawLine + "' parsed='" + parsed.processed + "'");
          } else if (settings_.parsing.mode == ParseMode::Parsed) {
            EmitLog("Raw received line: '" + rawLine + "'");
            EmitLog("Parsed value: '" + parsed.processed + "'");
          }
          if (settings_.output.postAction == PostAction::CustomSequence) {
            std::string seq;
            for (std::size_t i = 0; i < settings_.output.customSequence.size(); ++i) {
              if (i > 0) seq += " -> ";
              seq += settings_.output.customSequence[i];
            }
            EmitLog("Executing custom sequence: " + seq);
          }
          const auto sendResult = injector_.SendTextAndAction(Utf8ToWide(parsed.processed), settings_.output);
          if (sendResult.status != InputInjector::SendStatus::Success) {
            if (sendResult.status == InputInjector::SendStatus::TextFailed) {
              EmitLog("Text injection failed for value: " + parsed.processed, true);
            } else {
              if (settings_.output.postAction == PostAction::CustomSequence && !sendResult.failedToken.empty()) {
                EmitLog("WARN: Unrecognized or failed custom sequence token: " + sendResult.failedToken, true);
                EmitLog("Custom sequence execution failed after text injection", true);
              } else {
                EmitLog("Post-action key injection failed after text injection", true);
              }
            }
          }
        } catch (const std::exception& ex) {
          EmitLog(std::string("ERROR: Exception while handling serial line callback: ") + ex.what(), true);
        } catch (...) {
          EmitLog("ERROR: Non-standard exception while handling serial line callback.", true);
        }
      },
      [this](const std::string& m) { EmitLog(m); }, [this](const std::string& m) { EmitLog(m, true); });

  EmitLog(std::string("TRACE: AppController::Connect after serial_.Connect() result=") + (connected ? "success" : "failure"));
  connected_ = connected;
  EmitConnectionState(connected_);
  if (connected_) EmitLog("Connected to " + settings_.serial.port + ". No valid scale data received yet.");
  EmitLog(std::string("Connect end: ") + (connected_ ? "success" : "failed"));
}

void AppController::Disconnect() {
  if (!connected_ && !serial_.IsConnected()) return;
  serial_.Disconnect();
  connected_ = false;
  EmitConnectionState(false);
  EmitLog("Disconnected");
}

void AppController::ApplySettings(const AppSettings& nextSettings, const AppConfig& nextConfig, bool persistToDisk) {
  AppConfig resolvedConfig = nextConfig;
  ConfigService::SanitizeConfig(resolvedConfig);
  resolvedConfig.configFolder = ConfigService::ResolveConfiguredPath(dataRoot_, resolvedConfig.configFolder).string();
  resolvedConfig.presetsFolder = ConfigService::ResolveConfiguredPath(dataRoot_, resolvedConfig.presetsFolder).string();
  resolvedConfig.logsFolder = ConfigService::ResolveConfiguredPath(dataRoot_, resolvedConfig.logsFolder).string();

  const bool settingsChanged =
      settings_.serial.port != nextSettings.serial.port || settings_.serial.baudRate != nextSettings.serial.baudRate ||
      settings_.serial.dataBits != nextSettings.serial.dataBits || settings_.serial.parity != nextSettings.serial.parity ||
      settings_.serial.stopBits != nextSettings.serial.stopBits || settings_.serial.timeoutSeconds != nextSettings.serial.timeoutSeconds ||
      settings_.serial.eol != nextSettings.serial.eol || settings_.parsing.mode != nextSettings.parsing.mode ||
      settings_.parsing.trimWhitespace != nextSettings.parsing.trimWhitespace ||
      settings_.parsing.stripSuffix != nextSettings.parsing.stripSuffix || settings_.parsing.suffix != nextSettings.parsing.suffix ||
      settings_.parsing.normalizeSign != nextSettings.parsing.normalizeSign ||
      settings_.parsing.preservePlusSign != nextSettings.parsing.preservePlusSign ||
      settings_.parsing.preserveMinusSign != nextSettings.parsing.preserveMinusSign ||
      settings_.parsing.numericValidation != nextSettings.parsing.numericValidation ||
      settings_.output.postAction != nextSettings.output.postAction || settings_.output.customSequence != nextSettings.output.customSequence;
  const bool configChanged =
      config_.configFolder != resolvedConfig.configFolder || config_.configFileName != resolvedConfig.configFileName ||
      config_.presetsFolder != resolvedConfig.presetsFolder || config_.logsFolder != resolvedConfig.logsFolder ||
      config_.logFilePattern != resolvedConfig.logFilePattern ||
      config_.logMode != resolvedConfig.logMode || config_.lineLogMode != resolvedConfig.lineLogMode ||
      config_.connectOnStartup != resolvedConfig.connectOnStartup || config_.darkMode != resolvedConfig.darkMode ||
      config_.startupMode != resolvedConfig.startupMode ||
      config_.startupPresetName != resolvedConfig.startupPresetName || config_.lastUsedPresetName != resolvedConfig.lastUsedPresetName;
  if (!settingsChanged && !configChanged) return;

  const bool reconnect = serial_.IsConnected() && SerialSettingsRequireReconnect(settings_.serial, nextSettings.serial);
  const bool logDestinationChanged = config_.logsFolder != resolvedConfig.logsFolder || config_.logMode != resolvedConfig.logMode ||
                                     config_.logFilePattern != resolvedConfig.logFilePattern;
  settings_ = nextSettings;
  config_ = resolvedConfig;
  configPath_ = std::filesystem::path(config_.configFolder) / config_.configFileName;
  if (logDestinationChanged) {
    std::lock_guard<std::mutex> lock(fileLogMutex_);
    if (logFile_.is_open()) logFile_.close();
    activeLogPath_.clear();
    sessionLogName_.clear();
    logWriteErrorNotified_ = false;
  }
  if (persistToDisk && (settingsChanged || configChanged)) {
    if (SaveConfig(configPath_, config_, &settings_)) EmitLog("Configuration saved");
    else EmitLog("ERROR: Failed to save configuration: " + configPath_.string(), true);
  }
  if (reconnect) {
    EmitLog("Reconnecting with updated serial settings on " + settings_.serial.port);
    Disconnect();
    Connect();
  }
}

bool AppController::SaveCurrentSettingsAsPreset(const std::string& presetName) {
  if (presetName.empty()) return false;
  const auto presetPath = std::filesystem::path(config_.presetsFolder) / (presetName + ".json");
  if (!SavePreset(presetPath, settings_, &config_)) {
    EmitLog("ERROR: Failed to save preset: " + presetPath.string(), true);
    return false;
  }
  config_.lastUsedPresetName = presetName;
  if (!SaveConfig(configPath_, config_, &settings_)) EmitLog("ERROR: Failed to save configuration: " + configPath_.string(), true);
  EmitLog("Preset saved: " + presetName);
  return true;
}

AppController::SaveConfigResult AppController::SaveResolvedConfiguration() {
  SaveConfigResult result{};
  result.path = configPath_;
  const bool existed = std::filesystem::exists(result.path);
  AppConfig diskConfig{};
  AppSettings diskSettings{};
  if (existed) {
    diskConfig = LoadConfig(result.path);
    diskSettings = LoadPreset(result.path);
  }
  result.changedFieldCount = ConfigService::CountConfigDifferences(diskConfig, config_) + ConfigService::CountSettingsDifferences(diskSettings, settings_);
  if (existed && result.changedFieldCount == 0) {
    result.status = SaveConfigStatus::Unchanged;
    return result;
  }
  if (!SaveConfig(result.path, config_, &settings_)) {
    result.status = SaveConfigStatus::Failed;
    return result;
  }
  result.status = existed ? SaveConfigStatus::Updated : SaveConfigStatus::Created;
  return result;
}

std::vector<std::string> AppController::ScanPorts() const { return ScanComPorts(); }

bool AppController::TestReceive(const SerialSettings& settings, std::string& receivedLine, std::string& errorMessage) {
  EmitLog("Test receive begin on " + settings.port);
  if (settings.port.empty()) {
    errorMessage = "Test receive failed on <empty port>: no port selected.";
    EmitLog(errorMessage, true);
    return false;
  }
  if (serial_.IsConnected() && PortNamesMatch(settings_.serial.port, settings.port)) {
    errorMessage = "Test Receive cannot run while already connected to " + settings.port + ". Disconnect first.";
    EmitLog(errorMessage, true);
    EmitLog("Test receive end on " + settings.port + ": blocked");
    return false;
  }

  SerialPort probe;
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;
  bool ok = false;

  const bool connected = probe.Connect(
      settings,
      [&](const std::string& line) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!done) {
          receivedLine = line;
          ok = true;
          done = true;
          cv.notify_all();
        }
      },
      [](const std::string&) {},
      [&](const std::string& err) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!done) {
          errorMessage = err;
          done = true;
          cv.notify_all();
        }
      });

  if (!connected) {
    if (errorMessage.empty()) errorMessage = "Unable to open serial port " + settings.port + " for test receive.";
    EmitLog("Test receive end on " + settings.port + ": open failed", true);
    return false;
  }

  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, std::chrono::seconds(3), [&]() { return done; });
  }
  probe.Disconnect();

  if (!done) {
    errorMessage = "No data received on " + settings.port + " — check device or COM port";
    EmitLog("Test receive end on " + settings.port + ": timeout", true);
    return false;
  }
  EmitLog("Test receive end on " + settings.port + ": success");
  return ok;
}

void AppController::SetLogSink(LogSink sink) { logSink_ = std::move(sink); }

void AppController::SetConnectionStateSink(ConnectionStateSink sink) { connectionStateSink_ = std::move(sink); }

void AppController::LogMessage(const std::string& message, bool isError) { EmitLog(message, isError); }

bool AppController::IsConnected() const { return connected_ || serial_.IsConnected(); }

void AppController::EmitLog(const std::string& message, bool isError) const {
  WriteLogFileLine(message, isError);
  if (logSink_) {
    logSink_(message, isError);
  }
}

void AppController::EmitConnectionState(bool connected) const {
  if (connectionStateSink_) {
    connectionStateSink_(connected);
  }
}

void AppController::WriteLogFileLine(const std::string& message, bool isError) const {
  if (config_.logMode == LogMode::None && !fileLogBufferingActive_) return;
  std::string warning;
  {
    std::lock_guard<std::mutex> lock(fileLogMutex_);
    if (fileLogBufferingActive_) {
      bufferedFileLogs_.push_back({message, isError});
      return;
    }
    const auto path = ResolveLogPath();
    if (path.empty()) return;

    if (activeLogPath_ != path) {
      if (logFile_.is_open()) logFile_.close();
      std::error_code ec;
      std::filesystem::create_directories(path.parent_path(), ec);
      if (ec) {
        activeLogPath_ = path;
        if (!logWriteErrorNotified_) {
          warning = "ERROR: Unable to prepare log directory: " + path.parent_path().string();
          logWriteErrorNotified_ = true;
        }
        return;
      }
      logFile_.open(path, std::ios::out | std::ios::app);
      activeLogPath_ = path;
      logWriteErrorNotified_ = false;
    }
    if (!logFile_.is_open()) {
      if (!logWriteErrorNotified_) {
        warning = "ERROR: Unable to write to log file: " + path.string();
        logWriteErrorNotified_ = true;
      }
      return;
    }

    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tmNow{};
#ifdef _WIN32
    localtime_s(&tmNow, &now);
#else
    localtime_r(&now, &tmNow);
#endif
    char stamp[16];
    std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &tmNow);
    (void)isError;
    logFile_ << "[" << stamp << "] " << message << "\n";
    logFile_.flush();
    if (!logFile_ && !logWriteErrorNotified_) {
      warning = "ERROR: Failed while flushing log file: " + path.string();
      logWriteErrorNotified_ = true;
    }
  }
  if (!warning.empty() && logSink_) logSink_(warning, true);
}

void AppController::FlushBufferedFileLogs() {
  std::vector<BufferedLogEntry> pending;
  {
    std::lock_guard<std::mutex> lock(fileLogMutex_);
    if (!fileLogBufferingActive_) return;
    fileLogBufferingActive_ = false;
    pending.swap(bufferedFileLogs_);
  }
  for (const auto& entry : pending) {
    WriteLogFileLine("[startup-buffered] " + entry.message, entry.isError);
  }
}

std::filesystem::path AppController::ResolveLogPath() const {
  const auto logsDir = std::filesystem::path(config_.logsFolder);
  if (config_.logMode == LogMode::SingleFile) return logsDir / "ScaleLogger.log";
  if (config_.logMode == LogMode::PerSession) {
    if (sessionLogName_.empty()) {
      const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      std::tm tmNow{};
#ifdef _WIN32
      localtime_s(&tmNow, &now);
#else
      localtime_r(&now, &tmNow);
#endif
      char buffer[128];
      const auto pattern = config_.logFilePattern.empty() ? std::string("ScaleLogger_%Y%m%d_%H%M%S.log") : config_.logFilePattern;
      std::strftime(buffer, sizeof(buffer), pattern.c_str(), &tmNow);
      sessionLogName_ = buffer;
    }
    return logsDir / sessionLogName_;
  }
  return {};
}

} // namespace scalelogger
