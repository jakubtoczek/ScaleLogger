#include "app/AppController.hpp"

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <mutex>

namespace scalelogger {
namespace {
std::filesystem::path ResolvePresetPath(const std::filesystem::path& presetsDir, const std::string& presetName) {
  if (presetName.empty()) return {};
  return presetsDir / (presetName + ".json");
}

std::wstring Utf8ToWide(const std::string& text) {
  return std::wstring(text.begin(), text.end());
}
} // namespace

AppController::AppController(std::filesystem::path dataRoot)
    : dataRoot_(std::move(dataRoot)), configPath_(dataRoot_ / "ScaleLogger.config.json") {}

void AppController::Initialize() {
  std::filesystem::create_directories(dataRoot_ / "logs");
  std::filesystem::create_directories(dataRoot_ / "presets");

  config_ = LoadConfig(configPath_);

  const auto presetsDir = dataRoot_ / config_.presetsFolder;
  std::filesystem::path startupPresetPath;
  if (config_.startupMode == "specific_preset") {
    startupPresetPath = ResolvePresetPath(presetsDir, config_.startupPresetName);
  } else if (config_.startupMode == "last_used_preset") {
    startupPresetPath = ResolvePresetPath(presetsDir, config_.lastUsedPresetName);
  }
  if (!startupPresetPath.empty() && std::filesystem::exists(startupPresetPath)) {
    settings_ = LoadPreset(startupPresetPath);
    EmitLog("Loaded startup preset: " + startupPresetPath.filename().string());
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
          EmitLog("Parse rejected: " + parsed.message + " raw='" + rawLine + "'", true);
          return;
        }
        if (!injector_.SendTextAndAction(Utf8ToWide(parsed.processed), settings_.output)) {
          EmitLog("Injection failed for value: " + parsed.processed, true);
        }
      },
      [this](const std::string& m) { EmitLog(m); }, [this](const std::string& m) { EmitLog(m, true); });

  connected_ = connected;
  EmitConnectionState(connected_);
}

void AppController::Disconnect() {
  if (!connected_ && !serial_.IsConnected()) return;
  serial_.Disconnect();
  connected_ = false;
  EmitConnectionState(false);
  EmitLog("Disconnected");
}

void AppController::ApplySettings(const AppSettings& nextSettings, const AppConfig& nextConfig) {
  const bool settingsChanged =
      settings_.serial.port != nextSettings.serial.port || settings_.serial.baudRate != nextSettings.serial.baudRate ||
      settings_.serial.dataBits != nextSettings.serial.dataBits || settings_.serial.parity != nextSettings.serial.parity ||
      settings_.serial.stopBits != nextSettings.serial.stopBits || settings_.serial.timeoutSeconds != nextSettings.serial.timeoutSeconds ||
      settings_.serial.eol != nextSettings.serial.eol || settings_.parsing.mode != nextSettings.parsing.mode ||
      settings_.parsing.trimWhitespace != nextSettings.parsing.trimWhitespace ||
      settings_.parsing.stripSuffix != nextSettings.parsing.stripSuffix || settings_.parsing.suffix != nextSettings.parsing.suffix ||
      settings_.parsing.normalizeSign != nextSettings.parsing.normalizeSign ||
      settings_.parsing.dropPlusSign != nextSettings.parsing.dropPlusSign ||
      settings_.parsing.numericValidation != nextSettings.parsing.numericValidation ||
      settings_.output.postAction != nextSettings.output.postAction || settings_.output.customSequence != nextSettings.output.customSequence;
  const bool configChanged =
      config_.presetsFolder != nextConfig.presetsFolder || config_.logsFolder != nextConfig.logsFolder ||
      config_.logMode != nextConfig.logMode || config_.lineLogMode != nextConfig.lineLogMode ||
      config_.connectOnStartup != nextConfig.connectOnStartup || config_.startupMode != nextConfig.startupMode ||
      config_.startupPresetName != nextConfig.startupPresetName || config_.lastUsedPresetName != nextConfig.lastUsedPresetName;
  if (!settingsChanged && !configChanged) return;

  const bool reconnect = serial_.IsConnected() && SerialSettingsRequireReconnect(settings_.serial, nextSettings.serial);
  settings_ = nextSettings;
  config_ = nextConfig;
  if (configChanged) {
    SaveConfig(configPath_, config_);
    EmitLog("Configuration saved");
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
  SavePreset(presetPath, settings_);
  config_.lastUsedPresetName = presetName;
  SaveConfig(configPath_, config_);
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
    errorMessage = "No line received within 3 seconds.";
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
  if (config_.logMode == LogMode::None) return;

  const auto path = ResolveLogPath();
  if (path.empty()) return;

  if (activeLogPath_ != path) {
    if (logFile_.is_open()) logFile_.close();
    std::filesystem::create_directories(path.parent_path());
    logFile_.open(path, std::ios::out | std::ios::app);
    activeLogPath_ = path;
  }
  if (!logFile_.is_open()) return;

  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tmNow{};
#ifdef _WIN32
  localtime_s(&tmNow, &now);
#else
  localtime_r(&now, &tmNow);
#endif
  char stamp[16];
  std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &tmNow);
  logFile_ << "[" << stamp << "] " << (isError ? "ERROR: " : "") << message << "\n";
  logFile_.flush();
}

std::filesystem::path AppController::ResolveLogPath() const {
  const auto logsDir = dataRoot_ / config_.logsFolder;
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
      char buffer[64];
      std::strftime(buffer, sizeof(buffer), "ScaleLogger_%Y%m%d_%H%M%S.log", &tmNow);
      sessionLogName_ = buffer;
    }
    return logsDir / sessionLogName_;
  }
  return {};
}

} // namespace scalelogger
