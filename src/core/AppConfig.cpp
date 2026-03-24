#include "core/AppConfig.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace scalelogger {
namespace {
std::string ReadAll(const std::filesystem::path& path) {
  std::ifstream ifs(path);
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

std::string ExtractString(const std::string& text, const std::string& key, const std::string& fallback) {
  const auto pos = text.find("\"" + key + "\"");
  if (pos == std::string::npos) return fallback;
  const auto colon = text.find(':', pos);
  const auto q1 = text.find('"', colon + 1);
  const auto q2 = text.find('"', q1 + 1);
  if (q1 == std::string::npos || q2 == std::string::npos) return fallback;
  return text.substr(q1 + 1, q2 - q1 - 1);
}

bool ExtractBool(const std::string& text, const std::string& key, bool fallback) {
  const auto pos = text.find("\"" + key + "\"");
  if (pos == std::string::npos) return fallback;
  const auto colon = text.find(':', pos);
  if (colon == std::string::npos) return fallback;
  const auto val = text.substr(colon + 1, 8);
  if (val.find("true") != std::string::npos) return true;
  if (val.find("false") != std::string::npos) return false;
  return fallback;
}

int ExtractInt(const std::string& text, const std::string& key, int fallback) {
  const auto pos = text.find("\"" + key + "\"");
  if (pos == std::string::npos) return fallback;
  const auto colon = text.find(':', pos);
  if (colon == std::string::npos) return fallback;
  return std::atoi(text.c_str() + colon + 1);
}

float ExtractFloat(const std::string& text, const std::string& key, float fallback) {
  const auto pos = text.find("\"" + key + "\"");
  if (pos == std::string::npos) return fallback;
  const auto colon = text.find(':', pos);
  if (colon == std::string::npos) return fallback;
  return std::atof(text.c_str() + colon + 1);
}
} // namespace

AppConfig LoadConfig(const std::filesystem::path& path) {
  AppConfig cfg{};
  if (!std::filesystem::exists(path)) return cfg;
  const auto text = ReadAll(path);
  cfg.presetsFolder = ExtractString(text, "presets_folder", cfg.presetsFolder);
  cfg.logsFolder = ExtractString(text, "logs_folder", cfg.logsFolder);
  cfg.connectOnStartup = ExtractBool(text, "connect_on_startup", cfg.connectOnStartup);
  cfg.startupMode = ExtractString(text, "startup_mode", cfg.startupMode);
  cfg.startupPresetName = ExtractString(text, "startup_preset_name", "");
  cfg.lastUsedPresetName = ExtractString(text, "last_used_preset_name", "");
  cfg.logMode = ExtractString(text, "log_mode", "per_session") == "single_file" ? LogMode::SingleFile : LogMode::PerSession;
  cfg.lineLogMode = ExtractString(text, "line_log_mode", "compact") == "verbose" ? LineLogMode::Verbose : LineLogMode::Compact;
  return cfg;
}

void SaveConfig(const std::filesystem::path& path, const AppConfig& config) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream ofs(path);
  ofs << "{\n"
      << "  \"presets_folder\": \"" << config.presetsFolder << "\",\n"
      << "  \"logs_folder\": \"" << config.logsFolder << "\",\n"
      << "  \"log_mode\": \"" << (config.logMode == LogMode::SingleFile ? "single_file" : "per_session") << "\",\n"
      << "  \"line_log_mode\": \"" << (config.lineLogMode == LineLogMode::Verbose ? "verbose" : "compact") << "\",\n"
      << "  \"connect_on_startup\": " << (config.connectOnStartup ? "true" : "false") << ",\n"
      << "  \"startup_mode\": \"" << config.startupMode << "\",\n"
      << "  \"startup_preset_name\": \"" << config.startupPresetName << "\",\n"
      << "  \"last_used_preset_name\": \"" << config.lastUsedPresetName << "\"\n"
      << "}\n";
}

AppSettings LoadPreset(const std::filesystem::path& path) {
  AppSettings s{};
  const auto text = ReadAll(path);
  s.serial.port = ExtractString(text, "port", s.serial.port);
  s.serial.baudRate = ExtractInt(text, "baudrate", s.serial.baudRate);
  s.serial.dataBits = ExtractInt(text, "databits", s.serial.dataBits);
  s.serial.parity = ExtractString(text, "parity", "O")[0];
  s.serial.stopBits = ExtractFloat(text, "stopbits", s.serial.stopBits);
  s.serial.timeoutSeconds = ExtractFloat(text, "timeout", s.serial.timeoutSeconds);
  s.serial.eol = ExtractString(text, "eol", "\\r\\n");
  s.parsing.mode = ExtractString(text, "mode", "parsed") == "raw" ? ParseMode::Raw : ParseMode::Parsed;
  s.parsing.trimWhitespace = ExtractBool(text, "trim_whitespace", true);
  s.parsing.stripSuffix = ExtractBool(text, "strip_suffix", true);
  s.parsing.suffix = ExtractString(text, "suffix", "g");
  s.parsing.normalizeSign = ExtractBool(text, "normalize_sign", true);
  s.parsing.dropPlusSign = ExtractBool(text, "drop_plus_sign", false);
  s.parsing.numericValidation = ExtractBool(text, "numeric_validation", true);
  return s;
}

bool SerialSettingsRequireReconnect(const SerialSettings& lhs, const SerialSettings& rhs) {
  return lhs.port != rhs.port || lhs.baudRate != rhs.baudRate || lhs.dataBits != rhs.dataBits || lhs.parity != rhs.parity ||
         lhs.stopBits != rhs.stopBits || lhs.timeoutSeconds != rhs.timeoutSeconds;
}

} // namespace scalelogger
