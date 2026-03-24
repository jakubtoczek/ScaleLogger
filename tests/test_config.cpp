#include "core/AppConfig.hpp"

#include <cassert>
#include <filesystem>

using namespace scalelogger;

int RunConfigTests() {
  const auto path = std::filesystem::temp_directory_path() / "scalelogger_config_test.json";
  AppConfig cfg;
  cfg.connectOnStartup = true;
  cfg.lineLogMode = LineLogMode::Compact;
  SaveConfig(path, cfg);
  auto loaded = LoadConfig(path);
  assert(loaded.connectOnStartup);
  SerialSettings a, b;
  assert(!SerialSettingsRequireReconnect(a, b));
  b.timeoutSeconds = 2.0F;
  assert(SerialSettingsRequireReconnect(a, b));
  std::filesystem::remove(path);
  return 0;
}