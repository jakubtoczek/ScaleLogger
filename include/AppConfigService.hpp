#pragma once

#include <string>

#include "Types.hpp"

class AppConfigService {
public:
    AppConfig Load(const std::wstring& exeDir);
    bool Save(const AppConfig& config, const std::wstring& path);

private:
    AppConfig Defaults(const std::wstring& exeDir) const;
    std::wstring ExpandEnv(const std::wstring& input) const;
    std::wstring ResolvePath(const std::wstring& root, const std::wstring& value) const;
    void Sanitize(AppConfig& config) const;
    void TryReadJsonOverrides(AppConfig& config, const std::wstring& path);
};
