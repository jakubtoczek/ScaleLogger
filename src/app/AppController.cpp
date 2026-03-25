#include "app/AppController.hpp"

#include <filesystem>

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
  const bool reconnect = serial_.IsConnected() && SerialSettingsRequireReconnect(settings_.serial, nextSettings.serial);
  settings_ = nextSettings;
  config_ = nextConfig;
  SaveConfig(configPath_, config_);
  EmitLog("Configuration saved");
  if (reconnect) {
    EmitLog("Reconnecting with updated serial settings on " + settings_.serial.port);
    Disconnect();
    Connect();
  }
}

void AppController::SetLogSink(LogSink sink) { logSink_ = std::move(sink); }

void AppController::SetConnectionStateSink(ConnectionStateSink sink) { connectionStateSink_ = std::move(sink); }

bool AppController::IsConnected() const { return connected_ || serial_.IsConnected(); }

void AppController::EmitLog(const std::string& message, bool isError) const {
  if (logSink_) {
    logSink_(message, isError);
  }
}

void AppController::EmitConnectionState(bool connected) const {
  if (connectionStateSink_) {
    connectionStateSink_(connected);
  }
}

} // namespace scalelogger
