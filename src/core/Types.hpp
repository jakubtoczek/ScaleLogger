#pragma once

#include <string>
#include <vector>

namespace scalelogger {

enum class PostAction { Down, Right, Enter, Tab, None, CustomSequence };
enum class ParseMode { Parsed, Raw };
enum class LogMode { None, SingleFile, PerSession };
enum class LineLogMode { Compact, Verbose };

struct SerialSettings {
  std::string port{"COM6"};
  int baudRate{1200};
  int dataBits{7};
  char parity{'O'};
  float stopBits{1.0F};
  float timeoutSeconds{1.0F};
  std::string eol{"\r\n"};
};

struct ParsingSettings {
  ParseMode mode{ParseMode::Parsed};
  bool trimWhitespace{true};
  bool stripSuffix{true};
  std::string suffix{"g"};
  bool normalizeSign{true};
  bool preservePlusSign{true};
  bool preserveMinusSign{true};
  bool numericValidation{true};
};

struct OutputSettings {
  PostAction postAction{PostAction::Down};
  std::vector<std::string> customSequence{};
};

struct AppSettings {
  SerialSettings serial{};
  ParsingSettings parsing{};
  OutputSettings output{};
};

struct AppConfig {
  std::string configFolder{};
  std::string configFileName{"ScaleLogger.config.json"};
  std::string presetsFolder{"presets"};
  std::string logsFolder{"logs"};
  std::string logFilePattern{"ScaleLogger_%Y%m%d_%H%M%S.log"};
  LogMode logMode{LogMode::PerSession};
  LineLogMode lineLogMode{LineLogMode::Compact};
  bool connectOnStartup{true};
  bool darkMode{false};
  std::string startupMode{"last_used_preset"};
  std::string startupPresetName{};
  std::string lastUsedPresetName{};
  bool standaloneMode{false};
};

} // namespace scalelogger
