#include "SettingsWindow.hpp"

#include <sstream>

bool SettingsWindow::ShowModal(HINSTANCE, HWND owner, AppConfig& config) {
    std::wstringstream ss;
    ss << L"Serial: " << config.serial.port << L" @ " << config.serial.baudRate << L"\n";
    ss << L"EOL: " << config.serial.eol << L"\n";
    ss << L"Dry run: " << (config.output.dryRun ? L"true" : L"false") << L"\n\n";
    ss << L"This clean reimplementation keeps settings in JSON (" << config.configPath << L").\n";
    ss << L"Edit settings in the JSON file, then click OK to reload at runtime.";

    int rc = MessageBoxW(owner, ss.str().c_str(), L"ScaleLogger Settings", MB_OKCANCEL | MB_ICONINFORMATION);
    return rc == IDOK;
}
