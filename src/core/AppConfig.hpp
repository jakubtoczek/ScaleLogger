#pragma once

#include "core/Types.hpp"

#include <filesystem>

namespace scalelogger {

AppConfig LoadConfig(const std::filesystem::path& path);
void SaveConfig(const std::filesystem::path& path, const AppConfig& config);
AppSettings LoadPreset(const std::filesystem::path& path);

bool SerialSettingsRequireReconnect(const SerialSettings& lhs, const SerialSettings& rhs);

} // namespace scalelogger
