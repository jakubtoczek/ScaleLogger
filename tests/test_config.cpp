#include "core/AppConfig.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

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

  const auto presetPath = std::filesystem::temp_directory_path() / "scalelogger_preset_test.json";
  {
    std::ofstream preset(presetPath);
    preset << "{\n"
           << "  \"eol\": \"\\\\r\\\\n\",\n"
           << "  \"drop_plus_sign\": false,\n"
           << "  \"custom_sequence\": [\"down\", \"right\"],\n"
           << "  \"post_action\": \"custom_sequence\"\n"
           << "}\n";
  }
  bool usedLegacyCompatibilityMapping = false;
  const auto presetSettings = LoadPreset(presetPath, &usedLegacyCompatibilityMapping);
  assert(presetSettings.serial.eol == "\r\n");
  assert(usedLegacyCompatibilityMapping);
  assert(presetSettings.output.customSequence.size() == 2);
  assert(presetSettings.output.customSequence[0] == "down");
  assert(presetSettings.output.customSequence[1] == "right");

  std::filesystem::remove(path);
  std::filesystem::remove(presetPath);
  return 0;
}
