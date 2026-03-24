#pragma once

#include "core/AppConfig.hpp"
#include "core/ValueParser.hpp"
#include "input/InputInjector.hpp"
#include "serial/SerialPort.hpp"

#include <filesystem>
#include <memory>

namespace scalelogger {

class AppController {
 public:
  explicit AppController(std::filesystem::path dataRoot);
  void Initialize();
  void Connect();
  void Disconnect();
  void ApplySettings(const AppSettings& nextSettings, const AppConfig& nextConfig);

 private:
  std::filesystem::path dataRoot_;
  std::filesystem::path configPath_;
  AppConfig config_{};
  AppSettings settings_{};
  SerialPort serial_{};
  ValueParser parser_{};
  InputInjector injector_{};
};

} // namespace scalelogger
