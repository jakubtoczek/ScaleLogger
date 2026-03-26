#include "core/AppConfig.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace scalelogger {
namespace {
std::string ReadAll(const std::filesystem::path& path) {
  std::ifstream ifs(path);
  if (!ifs) return {};
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

std::string JsonEscape(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char ch : value) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\r': out += "\\r"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) out += ' ';
        else out += ch;
        break;
    }
  }
  return out;
}

bool ParseJsonStringAt(const std::string& text, std::size_t quotePos, std::string& out, std::size_t& endPos) {
  if (quotePos >= text.size() || text[quotePos] != '"') return false;
  out.clear();
  for (std::size_t i = quotePos + 1; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '"') {
      endPos = i;
      return true;
    }
    if (ch == '\\' && i + 1 < text.size()) {
      const char esc = text[++i];
      switch (esc) {
        case '\\': out.push_back('\\'); break;
        case '"': out.push_back('"'); break;
        case 'r': out.push_back('\r'); break;
        case 'n': out.push_back('\n'); break;
        case 't': out.push_back('\t'); break;
        default: out.push_back(esc); break;
      }
      continue;
    }
    out.push_back(ch);
  }
  return false;
}

std::string ExtractString(const std::string& text, const std::string& key, const std::string& fallback) {
  const auto pos = text.find("\"" + key + "\"");
  if (pos == std::string::npos) return fallback;
  const auto colon = text.find(':', pos);
  if (colon == std::string::npos) return fallback;
  const auto q1 = text.find('"', colon + 1);
  if (q1 == std::string::npos) return fallback;
  std::string out;
  std::size_t endPos = std::string::npos;
  if (!ParseJsonStringAt(text, q1, out, endPos)) return fallback;
  return out;
}

bool ContainsKey(const std::string& text, const std::string& key) {
  return text.find("\"" + key + "\"") != std::string::npos;
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

std::vector<std::string> ExtractStringArray(const std::string& text, const std::string& key) {
  std::vector<std::string> out;
  const auto pos = text.find("\"" + key + "\"");
  if (pos == std::string::npos) return out;
  const auto lb = text.find('[', pos);
  const auto rb = text.find(']', lb + 1);
  if (lb == std::string::npos || rb == std::string::npos) return out;
  const auto content = text.substr(lb + 1, rb - lb - 1);

  std::size_t i = 0;
  while (i < content.size()) {
    while (i < content.size() && (std::isspace(static_cast<unsigned char>(content[i])) != 0 || content[i] == ',')) ++i;
    if (i >= content.size()) break;
    if (content[i] != '"') {
      ++i;
      continue;
    }
    std::string value;
    std::size_t endPos = std::string::npos;
    if (!ParseJsonStringAt(content, i, value, endPos)) break;
    out.push_back(value);
    i = endPos + 1;
  }
  return out;
}

std::vector<int> ExtractIntArray(const std::string& text, const std::string& key) {
  std::vector<int> out;
  const auto pos = text.find("\"" + key + "\"");
  if (pos == std::string::npos) return out;
  const auto lb = text.find('[', pos);
  const auto rb = text.find(']', lb + 1);
  if (lb == std::string::npos || rb == std::string::npos) return out;
  const auto content = text.substr(lb + 1, rb - lb - 1);
  std::stringstream ss(content);
  std::string token;
  while (std::getline(ss, token, ',')) {
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front())) != 0) token.erase(token.begin());
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())) != 0) token.pop_back();
    if (token.empty()) continue;
    out.push_back(std::atoi(token.c_str()));
  }
  return out;
}

std::string DecodeEolString(const std::string& value) {
  if (value == "\\r\\n") return "\r\n";
  if (value == "\\n") return "\n";
  if (value == "\\r") return "\r";
  if (value == "\r\n" || value == "\n" || value == "\r") return value;
  return "\r\n";
}

PostAction ParsePostAction(const std::string& action) {
  if (action == "right") return PostAction::Right;
  if (action == "enter") return PostAction::Enter;
  if (action == "tab") return PostAction::Tab;
  if (action == "none") return PostAction::None;
  if (action == "custom_sequence") return PostAction::CustomSequence;
  return PostAction::Down;
}

std::string ToPostAction(PostAction action) {
  switch (action) {
    case PostAction::Right: return "right";
    case PostAction::Enter: return "enter";
    case PostAction::Tab: return "tab";
    case PostAction::None: return "none";
    case PostAction::CustomSequence: return "custom_sequence";
    case PostAction::Down:
    default: return "down";
  }
}
} // namespace

AppConfig LoadConfig(const std::filesystem::path& path) {
  AppConfig cfg{};
  if (!std::filesystem::exists(path)) return cfg;
  const auto text = ReadAll(path);
  cfg.presetsFolder = ExtractString(text, "presets_folder", cfg.presetsFolder);
  cfg.logsFolder = ExtractString(text, "logs_folder", cfg.logsFolder);
  cfg.configFolder = ExtractString(text, "config_folder", cfg.configFolder);
  cfg.configFileName = ExtractString(text, "config_file_name", cfg.configFileName);
  cfg.logFilePattern = ExtractString(text, "log_file_pattern", cfg.logFilePattern);
  cfg.connectOnStartup = ExtractBool(text, "connect_on_startup", cfg.connectOnStartup);
  cfg.darkMode = ExtractBool(text, "dark_mode", cfg.darkMode);
  cfg.startupMode = ExtractString(text, "startup_mode", cfg.startupMode);
  cfg.startupPresetName = ExtractString(text, "startup_preset_name", "");
  cfg.lastUsedPresetName = ExtractString(text, "last_used_preset_name", "");
  (void)ExtractBool(text, "standalone_mode", false); // legacy key ignored
  const auto baudRates = ExtractIntArray(text, "baud_rates");
  if (!baudRates.empty()) cfg.baudRates = baudRates;
  const auto dataBitsOptions = ExtractIntArray(text, "data_bits_options");
  if (!dataBitsOptions.empty()) cfg.dataBitsOptions = dataBitsOptions;
  const auto parityOptions = ExtractStringArray(text, "parity_options");
  if (!parityOptions.empty()) cfg.parityOptions = parityOptions;
  const auto stopBitsOptions = ExtractStringArray(text, "stop_bits_options");
  if (!stopBitsOptions.empty()) cfg.stopBitsOptions = stopBitsOptions;
  const auto logMode = ExtractString(text, "log_mode", "per_session");
  cfg.logMode = logMode == "single_file" ? LogMode::SingleFile : (logMode == "none" ? LogMode::None : LogMode::PerSession);
  cfg.lineLogMode = ExtractString(text, "line_log_mode", "compact") == "verbose" ? LineLogMode::Verbose : LineLogMode::Compact;
  return cfg;
}

bool SaveConfig(const std::filesystem::path& path, const AppConfig& config, const AppSettings* settings) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) return false;
  std::ofstream ofs(path);
  if (!ofs) return false;
  ofs << "{\n"
      << "  \"presets_folder\": \"" << JsonEscape(config.presetsFolder) << "\",\n"
      << "  \"logs_folder\": \"" << JsonEscape(config.logsFolder) << "\",\n"
      << "  \"config_folder\": \"" << JsonEscape(config.configFolder) << "\",\n"
      << "  \"config_file_name\": \"" << JsonEscape(config.configFileName) << "\",\n"
      << "  \"log_file_pattern\": \"" << JsonEscape(config.logFilePattern) << "\",\n"
      << "  \"log_mode\": \""
      << (config.logMode == LogMode::SingleFile ? "single_file" : (config.logMode == LogMode::None ? "none" : "per_session")) << "\",\n"
      << "  \"line_log_mode\": \"" << (config.lineLogMode == LineLogMode::Verbose ? "verbose" : "compact") << "\",\n"
      << "  \"connect_on_startup\": " << (config.connectOnStartup ? "true" : "false") << ",\n"
      << "  \"dark_mode\": " << (config.darkMode ? "true" : "false") << ",\n"
      << "  \"startup_mode\": \"" << JsonEscape(config.startupMode) << "\",\n"
      << "  \"startup_preset_name\": \"" << JsonEscape(config.startupPresetName) << "\",\n"
      << "  \"last_used_preset_name\": \"" << JsonEscape(config.lastUsedPresetName) << "\",\n"
      << "  \"baud_rates\": [";
  for (std::size_t i = 0; i < config.baudRates.size(); ++i) {
    if (i) ofs << ", ";
    ofs << config.baudRates[i];
  }
  ofs << "],\n"
      << "  \"data_bits_options\": [";
  for (std::size_t i = 0; i < config.dataBitsOptions.size(); ++i) {
    if (i) ofs << ", ";
    ofs << config.dataBitsOptions[i];
  }
  ofs << "],\n"
      << "  \"parity_options\": [";
  for (std::size_t i = 0; i < config.parityOptions.size(); ++i) {
    if (i) ofs << ", ";
    ofs << '"' << JsonEscape(config.parityOptions[i]) << '"';
  }
  ofs << "],\n"
      << "  \"stop_bits_options\": [";
  for (std::size_t i = 0; i < config.stopBitsOptions.size(); ++i) {
    if (i) ofs << ", ";
    ofs << '"' << JsonEscape(config.stopBitsOptions[i]) << '"';
  }
  ofs << "]";
  if (settings) {
    const auto mode = settings->parsing.mode == ParseMode::Raw ? "raw" : "parsed";
    const auto parity = std::string(1, settings->serial.parity);
    std::string eol = "\\r\\n";
    if (settings->serial.eol == "\n") eol = "\\n";
    else if (settings->serial.eol == "\r") eol = "\\r";
    std::string postAction = "down";
    switch (settings->output.postAction) {
      case PostAction::Right: postAction = "right"; break;
      case PostAction::Enter: postAction = "enter"; break;
      case PostAction::Tab: postAction = "tab"; break;
      case PostAction::None: postAction = "none"; break;
      case PostAction::CustomSequence: postAction = "custom_sequence"; break;
      case PostAction::Down:
      default: break;
    }
    ofs << ",\n"
        << "  \"port\": \"" << JsonEscape(settings->serial.port) << "\",\n"
        << "  \"baudrate\": " << settings->serial.baudRate << ",\n"
        << "  \"databits\": " << settings->serial.dataBits << ",\n"
        << "  \"parity\": \"" << JsonEscape(parity) << "\",\n"
        << "  \"stopbits\": " << settings->serial.stopBits << ",\n"
        << "  \"timeout\": " << settings->serial.timeoutSeconds << ",\n"
        << "  \"eol\": \"" << JsonEscape(eol) << "\",\n"
        << "  \"mode\": \"" << mode << "\",\n"
        << "  \"trim_whitespace\": " << (settings->parsing.trimWhitespace ? "true" : "false") << ",\n"
        << "  \"strip_suffix\": " << (settings->parsing.stripSuffix ? "true" : "false") << ",\n"
        << "  \"suffix\": \"" << JsonEscape(settings->parsing.suffix) << "\",\n"
        << "  \"normalize_sign\": " << (settings->parsing.normalizeSign ? "true" : "false") << ",\n"
        << "  \"preserve_plus_sign\": " << (settings->parsing.preservePlusSign ? "true" : "false") << ",\n"
        << "  \"preserve_minus_sign\": " << (settings->parsing.preserveMinusSign ? "true" : "false") << ",\n"
        << "  \"numeric_validation\": " << (settings->parsing.numericValidation ? "true" : "false") << ",\n"
        << "  \"post_action\": \"" << JsonEscape(postAction) << "\",\n"
        << "  \"custom_sequence\": [";
    for (std::size_t i = 0; i < settings->output.customSequence.size(); ++i) {
      if (i) ofs << ", ";
      ofs << '"' << JsonEscape(settings->output.customSequence[i]) << '"';
    }
    ofs << "]";
  }
  ofs << "\n}\n";
  return static_cast<bool>(ofs);
}

AppSettings LoadPreset(const std::filesystem::path& path, bool* usedLegacyCompatibilityMapping) {
  AppSettings s{};
  if (usedLegacyCompatibilityMapping) *usedLegacyCompatibilityMapping = false;
  if (!std::filesystem::exists(path)) return s;
  const auto text = ReadAll(path);
  s.serial.port = ExtractString(text, "port", s.serial.port);
  s.serial.baudRate = ExtractInt(text, "baudrate", s.serial.baudRate);
  s.serial.dataBits = ExtractInt(text, "databits", s.serial.dataBits);
  const auto parity = ExtractString(text, "parity", "O");
  s.serial.parity = parity.empty() ? 'O' : parity[0];
  s.serial.stopBits = ExtractFloat(text, "stopbits", s.serial.stopBits);
  s.serial.timeoutSeconds = ExtractFloat(text, "timeout", s.serial.timeoutSeconds);
  s.serial.eol = DecodeEolString(ExtractString(text, "eol", "\\r\\n"));
  s.parsing.mode = ExtractString(text, "mode", "parsed") == "raw" ? ParseMode::Raw : ParseMode::Parsed;
  s.parsing.trimWhitespace = ExtractBool(text, "trim_whitespace", true);
  s.parsing.stripSuffix = ExtractBool(text, "strip_suffix", true);
  s.parsing.suffix = ExtractString(text, "suffix", "g");
  const bool hasLegacyNormalizeSign = ContainsKey(text, "normalize_sign");
  const bool hasLegacyDropPlusSign = ContainsKey(text, "drop_plus_sign");
  s.parsing.normalizeSign = ExtractBool(text, "normalize_sign", true);
  const bool dropPlusSign = ExtractBool(text, "drop_plus_sign", false);
  s.parsing.preservePlusSign = ExtractBool(text, "preserve_plus_sign", !dropPlusSign);
  s.parsing.preserveMinusSign = ExtractBool(text, "preserve_minus_sign", true);
  if (usedLegacyCompatibilityMapping && ((hasLegacyDropPlusSign && !ContainsKey(text, "preserve_plus_sign")) ||
                                         (hasLegacyNormalizeSign && !ContainsKey(text, "preserve_minus_sign")))) {
    *usedLegacyCompatibilityMapping = true;
  }
  s.parsing.numericValidation = ExtractBool(text, "numeric_validation", true);
  s.output.postAction = ParsePostAction(ExtractString(text, "post_action", "down"));
  s.output.customSequence = ExtractStringArray(text, "custom_sequence");
  return s;
}

bool SavePreset(const std::filesystem::path& path, const AppSettings& settings, const AppConfig* config) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) return false;
  std::ofstream ofs(path);
  if (!ofs) return false;

  const auto mode = settings.parsing.mode == ParseMode::Raw ? "raw" : "parsed";
  const auto parity = std::string(1, settings.serial.parity);
  std::string eol = "\\r\\n";
  if (settings.serial.eol == "\n") eol = "\\n";
  else if (settings.serial.eol == "\r") eol = "\\r";

  std::string postAction = "down";
  switch (settings.output.postAction) {
    case PostAction::Right: postAction = "right"; break;
    case PostAction::Enter: postAction = "enter"; break;
    case PostAction::Tab: postAction = "tab"; break;
    case PostAction::None: postAction = "none"; break;
    case PostAction::CustomSequence: postAction = "custom_sequence"; break;
    case PostAction::Down:
    default: break;
  }

  ofs << "{\n"
      << "  \"port\": \"" << JsonEscape(settings.serial.port) << "\",\n"
      << "  \"baudrate\": " << settings.serial.baudRate << ",\n"
      << "  \"databits\": " << settings.serial.dataBits << ",\n"
      << "  \"parity\": \"" << JsonEscape(parity) << "\",\n"
      << "  \"stopbits\": " << settings.serial.stopBits << ",\n"
      << "  \"timeout\": " << settings.serial.timeoutSeconds << ",\n"
      << "  \"eol\": \"" << JsonEscape(eol) << "\",\n"
      << "  \"mode\": \"" << JsonEscape(mode) << "\",\n"
      << "  \"trim_whitespace\": " << (settings.parsing.trimWhitespace ? "true" : "false") << ",\n"
      << "  \"strip_suffix\": " << (settings.parsing.stripSuffix ? "true" : "false") << ",\n"
      << "  \"suffix\": \"" << JsonEscape(settings.parsing.suffix) << "\",\n"
      << "  \"normalize_sign\": " << (settings.parsing.normalizeSign ? "true" : "false") << ",\n"
      << "  \"preserve_plus_sign\": " << (settings.parsing.preservePlusSign ? "true" : "false") << ",\n"
      << "  \"preserve_minus_sign\": " << (settings.parsing.preserveMinusSign ? "true" : "false") << ",\n"
      << "  \"numeric_validation\": " << (settings.parsing.numericValidation ? "true" : "false") << ",\n"
      << "  \"post_action\": \"" << JsonEscape(postAction) << "\",\n"
      << "  \"custom_sequence\": [";
  for (std::size_t i = 0; i < settings.output.customSequence.size(); ++i) {
    if (i) ofs << ", ";
    ofs << '"' << JsonEscape(settings.output.customSequence[i]) << '"';
  }
  ofs << "]";
  if (config) {
    ofs << ",\n"
        << "  \"config_folder\": \"" << JsonEscape(config->configFolder) << "\",\n"
        << "  \"config_file_name\": \"" << JsonEscape(config->configFileName) << "\",\n"
        << "  \"presets_folder\": \"" << JsonEscape(config->presetsFolder) << "\",\n"
        << "  \"logs_folder\": \"" << JsonEscape(config->logsFolder) << "\",\n"
        << "  \"log_file_pattern\": \"" << JsonEscape(config->logFilePattern) << "\",\n"
        << "  \"log_mode\": \""
        << (config->logMode == LogMode::SingleFile ? "single_file" : (config->logMode == LogMode::None ? "none" : "per_session")) << "\",\n"
        << "  \"line_log_mode\": \"" << (config->lineLogMode == LineLogMode::Verbose ? "verbose" : "compact") << "\",\n"
        << "  \"connect_on_startup\": " << (config->connectOnStartup ? "true" : "false") << ",\n"
        << "  \"dark_mode\": " << (config->darkMode ? "true" : "false") << ",\n"
        << "  \"startup_mode\": \"" << JsonEscape(config->startupMode) << "\",\n"
        << "  \"startup_preset_name\": \"" << JsonEscape(config->startupPresetName) << "\",\n"
        << "  \"last_used_preset_name\": \"" << JsonEscape(config->lastUsedPresetName) << '"';
  }
  ofs << "\n}\n";
  return static_cast<bool>(ofs);
}

bool SerialSettingsRequireReconnect(const SerialSettings& lhs, const SerialSettings& rhs) {
  return lhs.port != rhs.port || lhs.baudRate != rhs.baudRate || lhs.dataBits != rhs.dataBits || lhs.parity != rhs.parity ||
         lhs.stopBits != rhs.stopBits || lhs.timeoutSeconds != rhs.timeoutSeconds;
}

} // namespace scalelogger
