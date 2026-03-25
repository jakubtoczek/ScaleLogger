#pragma once

#include "core/AppConfig.hpp"
#include "core/ValueParser.hpp"
#include "input/InputInjector.hpp"
#include "serial/PortScanner.hpp"
#include "serial/SerialPort.hpp"

#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>
#include <string>

namespace scalelogger {

class AppController {
 public:
  using LogSink = std::function<void(const std::string&, bool)>;
  using ConnectionStateSink = std::function<void(bool)>;

  explicit AppController(std::filesystem::path dataRoot);
  void Initialize();
  void Connect();
  void Disconnect();
  void ApplySettings(const AppSettings& nextSettings, const AppConfig& nextConfig);
  bool SaveCurrentSettingsAsPreset(const std::string& presetName);
  std::vector<std::string> ScanPorts() const;
  bool TestReceive(const SerialSettings& settings, std::string& receivedLine, std::string& errorMessage);

  void SetLogSink(LogSink sink);
  void SetConnectionStateSink(ConnectionStateSink sink);

  const AppSettings& Settings() const { return settings_; }
  const AppConfig& Config() const { return config_; }
  const std::filesystem::path& DataRoot() const { return dataRoot_; }
  bool IsConnected() const;

 private:
  void EmitLog(const std::string& message, bool isError = false) const;
  void EmitConnectionState(bool connected) const;
  void WriteLogFileLine(const std::string& message, bool isError) const;
  std::filesystem::path ResolveLogPath() const;

  std::filesystem::path dataRoot_;
  std::filesystem::path configPath_;
  AppConfig config_{};
  AppSettings settings_{};
  SerialPort serial_{};
  ValueParser parser_{};
  InputInjector injector_{};
  LogSink logSink_{};
  ConnectionStateSink connectionStateSink_{};
  bool connected_{false};
  mutable std::string sessionLogName_{};
  mutable std::ofstream logFile_{};
  mutable std::filesystem::path activeLogPath_{};
  mutable bool logWriteErrorNotified_{false};
};

} // namespace scalelogger
