#include "app/AppController.hpp"

#include "app/ConfigService.hpp"

#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <mutex>
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

std::string LogModeText(LogMode mode) {
  switch (mode) {
    case LogMode::SingleFile: return "single_file";
    case LogMode::None: return "none";
    case LogMode::PerSession:
    default: return "per_session";
  }
}

void ResolveAndSanitize(std::filesystem::path dataRoot, AppSettings& settings, AppConfig& config) {
  SanitizeSerialSettings(settings);
  ConfigService::SanitizeConfig(config);
  config.configFolder = ConfigService::ResolveConfiguredPath(dataRoot, config.configFolder).string();
  config.logsFolder = ConfigService::ResolveConfiguredPath(dataRoot, config.logsFolder).string();
}

bool PortNamesMatch(const std::string& lhs, const std::string& rhs) {
  auto upper = [](std::string value) {
    for (char& ch : value) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return value;
  };
  return upper(lhs) == upper(rhs);
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
  } catch (const std::exception& ex) {
    loadedConfig = AppConfig{};
    loadedSettings = AppSettings{};
    source = std::string("fallback defaults after load failure: ") + ex.what();
  }

  ResolveAndSanitize(dataRoot_, loadedSettings, loadedConfig);

  settings_ = loadedSettings;
  config_ = loadedConfig;
  configPath_ = std::filesystem::path(config_.configFolder) / config_.configFileName;

  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(config_.logsFolder), ec);
  if (ec) EmitLog("WARN: Failed to create logs directory: " + std::filesystem::path(config_.logsFolder).string(), true);

  FlushBufferedFileLogs();

  EmitLog("Startup config source: " + source);
  EmitLog("Startup effective config path: " + configPath_.string());
  EmitLog("Startup serial: port=" + settings_.serial.port + "; baudrate=" + std::to_string(settings_.serial.baudRate) +
          "; databits=" + std::to_string(settings_.serial.dataBits) + "; parity=" + std::string(1, settings_.serial.parity) +
          "; stopbits=" + FormatStopBits(settings_.serial.stopBits) + "; timeout=" + FormatTimeout(settings_.serial.timeoutSeconds) +
          "; eol=" + EscapeForLog(settings_.serial.eol));
  EmitLog("Startup logging: folder=" + config_.logsFolder + "; mode=" + LogModeText(config_.logMode) + "; pattern=" + config_.logFilePattern);
  EmitLog("Initialize end");
}

void AppController::Connect() {
  EmitLog("Connect begin");
  if (const char* forceNoSerial = std::getenv("SCALELOGGER_FORCE_NO_SERIAL")) {
    if (std::string(forceNoSerial) == "1") {
      connected_ = false;
      EmitConnectionState(false);
      EmitLog("Connect skipped: SCALELOGGER_FORCE_NO_SERIAL=1");
      return;
    }
  }

  if (connected_ || serial_.IsConnected()) {
    connected_ = true;
    EmitConnectionState(true);
    EmitLog("Connect skipped: already connected");
    return;
  }

  const bool connected = serial_.Connect(
      settings_.serial,
      [this](const std::string& rawLine) {
        try {
          const auto parsed = parser_.Process(rawLine, settings_.parsing);
          if (!parsed.ok) {
            EmitLog("Parse rejected: " + parsed.message + " raw='" + rawLine + "'", true);
            return;
          }

          EmitLog("Parsed value: '" + parsed.processed + "' from raw='" + rawLine + "'");
          const auto sendResult = injector_.SendTextAndAction(Utf8ToWide(parsed.processed), settings_.output);
          if (sendResult.status == InputInjector::SendStatus::Success) {
            EmitLog("Injection succeeded for parsed value: '" + parsed.processed + "'");
            return;
          }

          if (sendResult.status == InputInjector::SendStatus::TextFailed) {
            EmitLog("Text injection failed for value: '" + parsed.processed + "'", true);
            return;
          }

          if (!sendResult.failedToken.empty()) {
            EmitLog("Post-action failed for token: " + sendResult.failedToken, true);
          } else {
            EmitLog("Post-action key injection failed", true);
          }
        } catch (const std::exception& ex) {
          EmitLog(std::string("ERROR: Exception while handling serial line callback: ") + ex.what(), true);
        }
      },
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
  const bool logDestinationChanged = config_.logsFolder != resolvedConfig.logsFolder || config_.logMode != resolvedConfig.logMode ||
                                     config_.logFilePattern != resolvedConfig.logFilePattern;

  settings_ = resolvedSettings;
  config_ = resolvedConfig;
  configPath_ = std::filesystem::path(config_.configFolder) / config_.configFileName;

  if (logDestinationChanged) {
    std::lock_guard<std::mutex> lock(fileLogMutex_);
    if (logFile_.is_open()) logFile_.close();
    activeLogPath_.clear();
    sessionLogName_.clear();
    logWriteErrorNotified_ = false;
  }

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

  bool existedBeforeSave = false;
  AppConfig diskBeforeConfig{};
  AppSettings diskBeforeSettings{};
  try {
    existedBeforeSave = std::filesystem::exists(result.path);
    if (existedBeforeSave) {
      diskBeforeConfig = LoadConfig(result.path);
      diskBeforeSettings = LoadConfigSettings(result.path);
      ResolveAndSanitize(dataRoot_, diskBeforeSettings, diskBeforeConfig);
    } else {
      ResolveAndSanitize(dataRoot_, diskBeforeSettings, diskBeforeConfig);
    }
  } catch (...) {
    ResolveAndSanitize(dataRoot_, diskBeforeSettings, diskBeforeConfig);
  }

  result.changedFieldCount = ConfigService::CountConfigDifferences(diskBeforeConfig, resolvedConfig) +
                             ConfigService::CountSettingsDifferences(diskBeforeSettings, resolvedSettings);
  if (existedBeforeSave && result.changedFieldCount == 0) {
    result.status = SaveConfigStatus::Unchanged;
    return result;
  }

  if (!SaveConfig(result.path, resolvedConfig, &resolvedSettings)) {
    result.status = SaveConfigStatus::Failed;
    return result;
  }

  config_ = resolvedConfig;
  settings_ = resolvedSettings;
  result.status = existedBeforeSave ? SaveConfigStatus::Updated : SaveConfigStatus::Created;
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
  if (logSink_) logSink_(message, isError);
}

void AppController::EmitConnectionState(bool connected) const {
  if (connectionStateSink_) connectionStateSink_(connected);
}

void AppController::WriteLogFileLine(const std::string& message, bool isError) const {
  (void)isError;
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
  if (config_.logMode == LogMode::None) return {};
  if (config_.logMode == LogMode::SingleFile) return logsDir / "ScaleLogger.log";

  if (sessionLogName_.empty()) {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tmNow{};
#ifdef _WIN32
    localtime_s(&tmNow, &now);
#else
    localtime_r(&now, &tmNow);
#endif

    char buffer[128]{};
    const auto pattern = config_.logFilePattern.empty() ? std::string("ScaleLogger_%Y%m%d_%H%M%S.log") : config_.logFilePattern;
    std::strftime(buffer, sizeof(buffer), pattern.c_str(), &tmNow);
    sessionLogName_ = buffer;
  }

  return logsDir / sessionLogName_;
}

} // namespace scalelogger
