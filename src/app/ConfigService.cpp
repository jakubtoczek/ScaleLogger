#include "app/ConfigService.hpp"

#include <algorithm>
#include <cctype>
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

std::string WideToUtf8(const std::wstring& text) {
#ifdef _WIN32
  if (text.empty()) return {};
  const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (sizeNeeded <= 0) return {};
  std::string out(static_cast<std::size_t>(sizeNeeded), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), sizeNeeded, nullptr, nullptr);
  return out;
#else
  return std::string(text.begin(), text.end());
#endif
}

std::string ExpandPathPlaceholders(std::string value) {
  if (value.empty()) return value;
#ifdef _WIN32
  const std::wstring wide = Utf8ToWide(value);
  std::vector<wchar_t> buffer(32768, L'\0');
  const DWORD written = ExpandEnvironmentStringsW(wide.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
  if (written > 0 && written < buffer.size()) {
    value = WideToUtf8(std::wstring(buffer.data(), buffer.data() + written - 1));
  }
#endif
  return value;
}

template <typename T>
void Dedup(std::vector<T>& values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
}
} // namespace

std::filesystem::path ConfigService::ResolveDefaultConfigPath(const std::filesystem::path& dataRoot) {
#ifdef _WIN32
  wchar_t modulePath[MAX_PATH]{};
  const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  if (length > 0 && length < MAX_PATH) {
    std::filesystem::path exePath(modulePath);
    return exePath.parent_path() / "default_config.json";
  }
#endif
  return dataRoot / "default_config.json";
}

std::filesystem::path ConfigService::ResolveConfiguredPath(const std::filesystem::path& root, const std::string& configuredPath) {
  const auto expanded = ExpandPathPlaceholders(configuredPath);
  std::filesystem::path path(expanded);
  if (path.empty()) return root;
  if (path.is_absolute()) return path.lexically_normal();
  return (root / path).lexically_normal();
}

void ConfigService::SanitizeConfig(AppConfig& config) {
  if (config.configFileName.empty()) config.configFileName = "ScaleLogger.config.json";

  config.baudRates.erase(std::remove_if(config.baudRates.begin(), config.baudRates.end(), [](int v) { return v <= 0; }), config.baudRates.end());
  if (config.baudRates.empty()) config.baudRates = {1200, 2400, 4800, 9600};
  Dedup(config.baudRates);

  config.dataBitsOptions.erase(std::remove_if(config.dataBitsOptions.begin(), config.dataBitsOptions.end(), [](int v) { return v < 5 || v > 8; }),
                               config.dataBitsOptions.end());
  if (config.dataBitsOptions.empty()) config.dataBitsOptions = {7, 8};
  Dedup(config.dataBitsOptions);

  for (auto& parity : config.parityOptions) {
    if (!parity.empty()) parity = std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(parity[0]))));
  }
  config.parityOptions.erase(std::remove_if(config.parityOptions.begin(), config.parityOptions.end(), [](const std::string& p) {
                             return !(p == "N" || p == "E" || p == "O");
                           }),
                           config.parityOptions.end());
  if (config.parityOptions.empty()) config.parityOptions = {"O", "N", "E"};
  Dedup(config.parityOptions);

  config.stopBitsOptions.erase(std::remove_if(config.stopBitsOptions.begin(), config.stopBitsOptions.end(), [](const std::string& value) {
                              return !(value == "1" || value == "1.0" || value == "1.5" || value == "2" || value == "2.0");
                            }),
                            config.stopBitsOptions.end());
  for (auto& stopBits : config.stopBitsOptions) {
    if (stopBits == "1.0") stopBits = "1";
    if (stopBits == "2.0") stopBits = "2";
  }
  if (config.stopBitsOptions.empty()) config.stopBitsOptions = {"1", "1.5", "2"};
  Dedup(config.stopBitsOptions);
}

int ConfigService::CountConfigDifferences(const AppConfig& before, const AppConfig& after) {
  int count = 0;
  if (before.configFolder != after.configFolder) ++count;
  if (before.configFileName != after.configFileName) ++count;
  if (before.logsFolder != after.logsFolder) ++count;
  if (before.logFilePattern != after.logFilePattern) ++count;
  if (before.logMode != after.logMode) ++count;
  if (before.lineLogMode != after.lineLogMode) ++count;
  if (before.connectOnStartup != after.connectOnStartup) ++count;
  if (before.darkMode != after.darkMode) ++count;
  if (before.baudRates != after.baudRates) ++count;
  if (before.dataBitsOptions != after.dataBitsOptions) ++count;
  if (before.parityOptions != after.parityOptions) ++count;
  if (before.stopBitsOptions != after.stopBitsOptions) ++count;
  return count;
}

int ConfigService::CountSettingsDifferences(const AppSettings& before, const AppSettings& after) {
  int count = 0;
  if (before.serial.port != after.serial.port) ++count;
  if (before.serial.baudRate != after.serial.baudRate) ++count;
  if (before.serial.dataBits != after.serial.dataBits) ++count;
  if (before.serial.parity != after.serial.parity) ++count;
  if (before.serial.stopBits != after.serial.stopBits) ++count;
  if (before.serial.timeoutSeconds != after.serial.timeoutSeconds) ++count;
  if (before.serial.eol != after.serial.eol) ++count;
  if (before.parsing.mode != after.parsing.mode) ++count;
  if (before.parsing.trimWhitespace != after.parsing.trimWhitespace) ++count;
  if (before.parsing.stripSuffix != after.parsing.stripSuffix) ++count;
  if (before.parsing.suffix != after.parsing.suffix) ++count;
  if (before.parsing.normalizeSign != after.parsing.normalizeSign) ++count;
  if (before.parsing.preservePlusSign != after.parsing.preservePlusSign) ++count;
  if (before.parsing.preserveMinusSign != after.parsing.preserveMinusSign) ++count;
  if (before.parsing.numericValidation != after.parsing.numericValidation) ++count;
  if (before.output.postAction != after.output.postAction) ++count;
  if (before.output.customSequence != after.output.customSequence) ++count;
  return count;
}

} // namespace scalelogger
