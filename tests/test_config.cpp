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

  const auto configSettingsPath = std::filesystem::temp_directory_path() / "scalelogger_settings_test.json";
  {
    std::ofstream cfgSettings(configSettingsPath);
    cfgSettings << "{\n"
                << "  \"eol\": \"\\\\r\\\\n\",\n"
                << "  \"drop_plus_sign\": false,\n"
                << "  \"custom_sequence\": [\"down\", \"right\"],\n"
                << "  \"post_action\": \"custom_sequence\"\n"
                << "}\n";
  }
  bool usedLegacyCompatibilityMapping = false;
  const auto loadedSettings = LoadConfigSettings(configSettingsPath, &usedLegacyCompatibilityMapping);
  assert(loadedSettings.serial.eol == "\r\n");
  assert(usedLegacyCompatibilityMapping);
  assert(loadedSettings.output.customSequence.size() == 2);
  assert(loadedSettings.output.customSequence[0] == "down");
  assert(loadedSettings.output.customSequence[1] == "right");

  const auto malformedPath = std::filesystem::temp_directory_path() / "scalelogger_malformed_config_test.json";
  {
    std::ofstream malformed(malformedPath);
    malformed << "{\n"
              << "  \"connect_on_startup\": tru,\n"
              << "  \"baud_rates\": [\"bad\", 9600],\n"
              << "  \"data_bits_options\": [\"x\"],\n"
              << "  \"parity_options\": [\"N\", \"\\\"bad\\\"\"],\n"
              << "  \"unknown_key\": \"ignored\"\n"
              << "}\n";
  }
  const auto malformedLoaded = LoadConfig(malformedPath);
  assert(malformedLoaded.connectOnStartup == true);
  assert(malformedLoaded.baudRates.size() == 1 && malformedLoaded.baudRates[0] == 9600);
  assert(malformedLoaded.dataBitsOptions.size() == 2);
  assert(malformedLoaded.parityOptions.size() == 2);

  std::filesystem::remove(path);
  std::filesystem::remove(configSettingsPath);
  std::filesystem::remove(malformedPath);
  return 0;
}
