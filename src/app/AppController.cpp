#include "app/AppController.hpp"

#include <filesystem>
#include <iostream>

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

  // Load startup settings before auto-connect so real user serial settings are applied.
  const auto presetsDir = dataRoot_ / config_.presetsFolder;
  std::filesystem::path startupPresetPath;
  if (config_.startupMode == "specific_preset") {
    startupPresetPath = ResolvePresetPath(presetsDir, config_.startupPresetName);
  } else if (config_.startupMode == "last_used_preset") {
    startupPresetPath = ResolvePresetPath(presetsDir, config_.lastUsedPresetName);
  }
  if (!startupPresetPath.empty() && std::filesystem::exists(startupPresetPath)) {
    settings_ = LoadPreset(startupPresetPath);
    std::cout << "Loaded startup preset: " << startupPresetPath.filename().string() << std::endl;
  }

  std::cout << "Application start" << std::endl;
  if (config_.connectOnStartup) {
    std::cout << "Auto-connecting to " << settings_.serial.port << std::endl;
    Connect();
  }
}

void AppController::Connect() {
  serial_.Connect(
      settings_.serial,
      [this](const std::string& rawLine) {
        const auto parsed = parser_.Process(rawLine, settings_.parsing);
        if (!parsed.ok) {
          std::cerr << "Parse rejected: " << parsed.message << " raw='" << rawLine << "'" << std::endl;
          return;
        }
        if (!injector_.SendTextAndAction(Utf8ToWide(parsed.processed), settings_.output)) {
          std::cerr << "Injection failed for value: " << parsed.processed << std::endl;
        }
      },
      [](const std::string& m) { std::cout << m << std::endl; }, [](const std::string& m) { std::cerr << m << std::endl; });
}

void AppController::Disconnect() { serial_.Disconnect(); }

void AppController::ApplySettings(const AppSettings& nextSettings, const AppConfig& nextConfig) {
  const bool reconnect = serial_.IsConnected() && SerialSettingsRequireReconnect(settings_.serial, nextSettings.serial);
  settings_ = nextSettings;
  config_ = nextConfig;
  SaveConfig(configPath_, config_);
  if (reconnect) {
    std::cout << "Reconnecting with updated serial settings on " << settings_.serial.port << std::endl;
    Disconnect();
    Connect();
  }
}

} // namespace scalelogger
