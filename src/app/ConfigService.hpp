#pragma once

#include "core/AppConfig.hpp"

#include <filesystem>

namespace scalelogger {

class ConfigService {
 public:
  static std::filesystem::path ResolveDefaultConfigPath(const std::filesystem::path& dataRoot);
  static std::filesystem::path ResolveConfiguredPath(const std::filesystem::path& root, const std::string& configuredPath);
  static std::filesystem::path ResolvePresetPath(const std::filesystem::path& presetsDir, const std::string& presetName);
  static void SanitizeConfig(AppConfig& config);
  static int CountConfigDifferences(const AppConfig& before, const AppConfig& after);
  static int CountSettingsDifferences(const AppSettings& before, const AppSettings& after);
};

} // namespace scalelogger
