#include "app/AppController.hpp"

#include <iostream>

namespace scalelogger {

AppController::AppController(std::filesystem::path dataRoot)
    : dataRoot_(std::move(dataRoot)), configPath_(dataRoot_ / "ScaleLogger.config.json") {}

void AppController::Initialize() {
  std::filesystem::create_directories(dataRoot_ / "logs");
  std::filesystem::create_directories(dataRoot_ / "presets");
  config_ = LoadConfig(configPath_);
  std::cout << "Application start" << std::endl;
  if (config_.connectOnStartup) {
    std::cout << "Auto-connecting to " << settings_.serial.port << std::endl;
    Connect();
  }
}

void AppController::Connect() {
  serial_.Connect(settings_.serial, [](const std::string&) {}, [](const std::string& m) { std::cout << m << std::endl; },
                  [](const std::string& m) { std::cerr << m << std::endl; });
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
