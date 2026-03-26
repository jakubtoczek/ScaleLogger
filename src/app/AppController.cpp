#include "app/AppController.hpp"

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {
namespace {
std::filesystem::path ResolvePresetPath(const std::filesystem::path& presetsDir, const std::string& presetName) {
  if (presetName.empty()) return {};
  return presetsDir / (presetName + ".json");
}

std::wstring Utf8ToWide(const std::string& text) {
  return std::wstring(text.begin(), text.end());
}

std::string ExpandPathPlaceholders(std::string value) {
  if (value.empty()) return value;
#ifdef _WIN32
  const std::wstring wide(value.begin(), value.end());
  std::vector<wchar_t> buffer(32768, L'\0');
  const DWORD written = ExpandEnvironmentStringsW(wide.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
  if (written > 0 && written < buffer.size()) {
    value.assign(buffer.data(), buffer.data() + written - 1);
  }
#endif
  return value;
}
} // namespace

AppController::AppController(std::filesystem::path dataRoot)
    : dataRoot_(std::move(dataRoot)), configPath_(dataRoot_ / "ScaleLogger.config.json") {}

void AppController::Initialize() {
  config_ = LoadConfig(configPath_);
  settings_ = LoadPreset(configPath_);
  if (!std::filesystem::exists(configPath_)) {
    const auto defaultConfigPath = std::filesystem::current_path() / "default_config.json";
    if (std::filesystem::exists(defaultConfigPath)) {
      config_ = LoadConfig(defaultConfigPath);
      settings_ = LoadPreset(defaultConfigPath);
      EmitLog("Loaded defaults from default_config.json");
    }
  }

  if (config_.configFolder.empty()) config_.configFolder = dataRoot_.string();
  config_.configFolder = ExpandPathPlaceholders(config_.configFolder);
  config_.presetsFolder = ExpandPathPlaceholders(config_.presetsFolder);
  config_.logsFolder = ExpandPathPlaceholders(config_.logsFolder);
  configPath_ = std::filesystem::path(config_.configFolder) / config_.configFileName;

  if (!config_.standaloneMode) {
    std::filesystem::create_directories(std::filesystem::path(config_.logsFolder));
    std::filesystem::create_directories(std::filesystem::path(config_.presetsFolder));
  }

  const auto presetsDir = dataRoot_ / config_.presetsFolder;
  std::filesystem::path startupPresetPath;
  if (config_.startupMode == "specific_preset") {
    startupPresetPath = ResolvePresetPath(presetsDir, config_.startupPresetName);
  } else if (config_.startupMode == "last_used_preset") {
    startupPresetPath = ResolvePresetPath(presetsDir, config_.lastUsedPresetName);
  }
  if (!startupPresetPath.empty() && std::filesystem::exists(startupPresetPath)) {
    bool usedLegacyCompatibilityMapping = false;
    settings_ = LoadPreset(startupPresetPath, &usedLegacyCompatibilityMapping);
    EmitLog("Loaded startup preset: " + startupPresetPath.filename().string());
    if (usedLegacyCompatibilityMapping) {
      EmitLog("Loaded preset with legacy compatibility mapping");
    }
  }

  EmitLog("Application start");
  if (config_.connectOnStartup) {
    EmitLog("Auto-connecting to " + settings_.serial.port);
    Connect();
  }
}

void AppController::Connect() {
  if (connected_) return;

  const bool connected = serial_.Connect(
      settings_.serial,
      [this](const std::string& rawLine) {
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
        if (!injector_.SendTextAndAction(Utf8ToWide(parsed.processed), settings_.output)) {
          if (settings_.output.postAction == PostAction::CustomSequence) {
            EmitLog("Custom sequence execution failed", true);
          }
          EmitLog("Injection failed for value: " + parsed.processed, true);
        }
      },
      [this](const std::string& m) { EmitLog(m); }, [this](const std::string& m) { EmitLog(m, true); });

  connected_ = connected;
  EmitConnectionState(connected_);
  if (connected_) EmitLog("Connected to " + settings_.serial.port + ". No valid scale data received yet.");
}

void AppController::Disconnect() {
  if (!connected_ && !serial_.IsConnected()) return;
  serial_.Disconnect();
  connected_ = false;
  EmitConnectionState(false);
  EmitLog("Disconnected");
}

void AppController::ApplySettings(const AppSettings& nextSettings, const AppConfig& nextConfig, bool persistToDisk) {
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
      config_.configFolder != nextConfig.configFolder || config_.configFileName != nextConfig.configFileName ||
      config_.presetsFolder != nextConfig.presetsFolder || config_.logsFolder != nextConfig.logsFolder ||
      config_.logFilePattern != nextConfig.logFilePattern ||
      config_.logMode != nextConfig.logMode || config_.lineLogMode != nextConfig.lineLogMode ||
      config_.connectOnStartup != nextConfig.connectOnStartup || config_.darkMode != nextConfig.darkMode ||
      config_.startupMode != nextConfig.startupMode ||
      config_.startupPresetName != nextConfig.startupPresetName || config_.lastUsedPresetName != nextConfig.lastUsedPresetName ||
      config_.standaloneMode != nextConfig.standaloneMode;
  if (!settingsChanged && !configChanged) return;

  const bool reconnect = serial_.IsConnected() && SerialSettingsRequireReconnect(settings_.serial, nextSettings.serial);
  settings_ = nextSettings;
  config_ = nextConfig;
  configPath_ = std::filesystem::path(config_.configFolder) / config_.configFileName;
  if (persistToDisk && configChanged && !config_.standaloneMode) {
    if (SaveConfig(configPath_, config_, &settings_)) EmitLog("Configuration saved");
    else EmitLog("ERROR: Failed to save configuration: " + configPath_.string(), true);
  }
  if (settingsChanged) EmitLog("Settings applied");
  if (reconnect) {
    EmitLog("Reconnecting with updated serial settings on " + settings_.serial.port);
    Disconnect();
    Connect();
  }
}

bool AppController::SaveCurrentSettingsAsPreset(const std::string& presetName) {
  if (presetName.empty()) return false;
  const auto presetPath = dataRoot_ / config_.presetsFolder / (presetName + ".json");
  if (config_.standaloneMode) return false;
  SavePreset(presetPath, settings_, &config_);
  config_.lastUsedPresetName = presetName;
  if (!SaveConfig(configPath_, config_, &settings_)) EmitLog("ERROR: Failed to save configuration: " + configPath_.string(), true);
  EmitLog("Preset saved: " + presetName);
  return true;
}

std::vector<std::string> AppController::ScanPorts() const { return ScanComPorts(); }

bool AppController::TestReceive(const SerialSettings& settings, std::string& receivedLine, std::string& errorMessage) {
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
    if (errorMessage.empty()) errorMessage = "Unable to open serial port for test receive.";
    return false;
  }

  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, std::chrono::seconds(3), [&]() { return done; });
  }
  probe.Disconnect();

  if (!done) {
    errorMessage = "No data received — check device or COM port";
    return false;
  }
  return ok;
}

void AppController::SetLogSink(LogSink sink) { logSink_ = std::move(sink); }

void AppController::SetConnectionStateSink(ConnectionStateSink sink) { connectionStateSink_ = std::move(sink); }

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
  if (config_.logMode == LogMode::None || config_.standaloneMode) return;

  const auto path = ResolveLogPath();
  if (path.empty()) return;

  if (activeLogPath_ != path) {
    if (logFile_.is_open()) logFile_.close();
    std::filesystem::create_directories(path.parent_path());
    logFile_.open(path, std::ios::out | std::ios::app);
    activeLogPath_ = path;
    logWriteErrorNotified_ = false;
  }
  if (!logFile_.is_open()) {
    if (!logWriteErrorNotified_ && logSink_) {
      logSink_("ERROR: Unable to write to log file: " + path.string(), true);
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
  if (!logFile_ && !logWriteErrorNotified_ && logSink_) {
    logSink_("ERROR: Failed while flushing log file: " + path.string(), true);
    logWriteErrorNotified_ = true;
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
