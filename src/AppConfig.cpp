#include "AppConfigService.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>

#include <windows.h>

AppConfig AppConfigService::Load(const std::wstring& exeDir) {
    AppConfig config = Defaults(exeDir);
    const auto explicitPath = ResolvePath(config.dataRoot, config.configPath);

    if (std::filesystem::exists(explicitPath)) {
        TryReadJsonOverrides(config, explicitPath);
        config.configPath = explicitPath;
    } else {
        const auto fallback = std::filesystem::path(exeDir) / L"default_config.json";
        if (std::filesystem::exists(fallback)) {
            TryReadJsonOverrides(config, fallback.wstring());
            config.configPath = explicitPath;
        }
    }
    Sanitize(config);
    return config;
}

bool AppConfigService::Save(const AppConfig& config, const std::wstring& path) {
    std::wofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << L"{\n";
    out << L"  \"data_root\": \"" << config.dataRoot << L"\",\n";
    out << L"  \"connect_on_startup\": " << (config.connectOnStartup ? L"true" : L"false") << L",\n";
    out << L"  \"logging\": {\n";
    out << L"    \"folder\": \"" << config.logging.folder << L"\",\n";
    out << L"    \"mode\": " << static_cast<int>(config.logging.mode) << L",\n";
    out << L"    \"verbose\": " << (config.logging.verbose ? L"true" : L"false") << L"\n";
    out << L"  },\n";
    out << L"  \"serial\": {\n";
    out << L"    \"port\": \"" << config.serial.port << L"\",\n";
    out << L"    \"baud_rate\": " << config.serial.baudRate << L",\n";
    out << L"    \"data_bits\": " << config.serial.dataBits << L",\n";
    out << L"    \"parity\": " << config.serial.parity << L",\n";
    out << L"    \"stop_bits\": " << config.serial.stopBits << L",\n";
    out << L"    \"timeout_ms\": " << config.serial.timeoutMs << L",\n";
    out << L"    \"eol\": \"" << config.serial.eol << L"\"\n";
    out << L"  },\n";
    out << L"  \"parse\": {\n";
    out << L"    \"trim_whitespace\": " << (config.parse.trimWhitespace ? L"true" : L"false") << L",\n";
    out << L"    \"strip_suffix\": " << (config.parse.stripSuffix ? L"true" : L"false") << L",\n";
    out << L"    \"suffix\": \"" << config.parse.suffix << L"\",\n";
    out << L"    \"normalize_sign\": " << (config.parse.normalizeSign ? L"true" : L"false") << L",\n";
    out << L"    \"preserve_plus\": " << (config.parse.preservePlus ? L"true" : L"false") << L",\n";
    out << L"    \"preserve_minus\": " << (config.parse.preserveMinus ? L"true" : L"false") << L",\n";
    out << L"    \"require_numeric\": " << (config.parse.requireNumeric ? L"true" : L"false") << L"\n";
    out << L"  },\n";
    out << L"  \"output\": {\n";
    out << L"    \"dry_run\": " << (config.output.dryRun ? L"true" : L"false") << L",\n";
    out << L"    \"post_action\": " << static_cast<int>(config.output.postAction) << L",\n";
    out << L"    \"custom_sequence\": \"" << config.output.customSequence << L"\"\n";
    out << L"  }\n";
    out << L"}\n";
    return true;
}

AppConfig AppConfigService::Defaults(const std::wstring& exeDir) const {
    AppConfig config;
    config.dataRoot = ExpandEnv(config.dataRoot);
    if (!std::filesystem::exists(config.dataRoot)) {
        std::filesystem::create_directories(config.dataRoot);
    }
    config.configPath = ResolvePath(config.dataRoot, config.configPath);
    return config;
}

std::wstring AppConfigService::ExpandEnv(const std::wstring& input) const {
    wchar_t expanded[MAX_PATH] = {};
    ExpandEnvironmentStringsW(input.c_str(), expanded, MAX_PATH);
    return expanded;
}

std::wstring AppConfigService::ResolvePath(const std::wstring& root, const std::wstring& value) const {
    std::filesystem::path path = ExpandEnv(value);
    if (path.is_absolute()) {
        return path.wstring();
    }
    return (std::filesystem::path(root) / path).wstring();
}

void AppConfigService::Sanitize(AppConfig& config) const {
    config.dataRoot = ExpandEnv(config.dataRoot);
    config.configPath = ResolvePath(config.dataRoot, config.configPath);
    if (config.serial.timeoutMs < 100) config.serial.timeoutMs = 100;
    if (config.serial.eol.empty()) config.serial.eol = L"\\r\\n";
    if (config.serial.port.empty()) config.serial.port = L"COM1";
}

void AppConfigService::TryReadJsonOverrides(AppConfig& config, const std::wstring& path) {
    std::wifstream in(path);
    if (!in.is_open()) {
        return;
    }
    std::wstringstream ss;
    ss << in.rdbuf();
    const std::wstring json = ss.str();

    auto readString = [&](const std::wstring& key) -> std::optional<std::wstring> {
        std::wregex re(L"\\\"" + key + L"\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
        std::wsmatch m;
        if (std::regex_search(json, m, re)) return m[1].str();
        return std::nullopt;
    };
    auto readInt = [&](const std::wstring& key) -> std::optional<int> {
        std::wregex re(L"\\\"" + key + L"\\\"\\s*:\\s*(-?\\d+)");
        std::wsmatch m;
        if (std::regex_search(json, m, re)) return std::stoi(m[1].str());
        return std::nullopt;
    };
    auto readBool = [&](const std::wstring& key) -> std::optional<bool> {
        std::wregex re(L"\\\"" + key + L"\\\"\\s*:\\s*(true|false)");
        std::wsmatch m;
        if (std::regex_search(json, m, re)) return m[1].str() == L"true";
        return std::nullopt;
    };

    if (auto v = readString(L"data_root")) config.dataRoot = *v;
    if (auto v = readBool(L"connect_on_startup")) config.connectOnStartup = *v;
    if (auto v = readString(L"folder")) config.logging.folder = *v;
    if (auto v = readInt(L"mode")) config.logging.mode = static_cast<LogMode>(*v);
    if (auto v = readBool(L"verbose")) config.logging.verbose = *v;
    if (auto v = readString(L"port")) config.serial.port = *v;
    if (auto v = readInt(L"baud_rate")) config.serial.baudRate = *v;
    if (auto v = readInt(L"data_bits")) config.serial.dataBits = *v;
    if (auto v = readInt(L"parity")) config.serial.parity = *v;
    if (auto v = readInt(L"stop_bits")) config.serial.stopBits = *v;
    if (auto v = readInt(L"timeout_ms")) config.serial.timeoutMs = *v;
    if (auto v = readString(L"eol")) config.serial.eol = *v;
    if (auto v = readBool(L"trim_whitespace")) config.parse.trimWhitespace = *v;
    if (auto v = readBool(L"strip_suffix")) config.parse.stripSuffix = *v;
    if (auto v = readString(L"suffix")) config.parse.suffix = *v;
    if (auto v = readBool(L"normalize_sign")) config.parse.normalizeSign = *v;
    if (auto v = readBool(L"preserve_plus")) config.parse.preservePlus = *v;
    if (auto v = readBool(L"preserve_minus")) config.parse.preserveMinus = *v;
    if (auto v = readBool(L"require_numeric")) config.parse.requireNumeric = *v;
    if (auto v = readBool(L"dry_run")) config.output.dryRun = *v;
    if (auto v = readInt(L"post_action")) config.output.postAction = static_cast<PostAction>(*v);
    if (auto v = readString(L"custom_sequence")) config.output.customSequence = *v;
}
