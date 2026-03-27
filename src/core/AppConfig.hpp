#pragma once

#include "core/Types.hpp"

#include <filesystem>

namespace scalelogger {

AppConfig LoadConfig(const std::filesystem::path& path);
bool SaveConfig(const std::filesystem::path& path, const AppConfig& config, const AppSettings* settings = nullptr);
AppSettings LoadPreset(const std::filesystem::path& path, bool* usedLegacyCompatibilityMapping = nullptr);
bool SavePreset(const std::filesystem::path& path, const AppSettings& settings, const AppConfig* config = nullptr);

bool SerialSettingsRequireReconnect(const SerialSettings& lhs, const SerialSettings& rhs);

} // namespace scalelogger
