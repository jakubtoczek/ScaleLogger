#pragma once

#include <functional>
#include <fstream>
#include <mutex>
#include <string>

#include "Types.hpp"

class Logger {
public:
    using UiSink = std::function<void(const std::wstring&)>;

    void Initialize(const LogSettings& settings, const std::wstring& dataRoot);
    void SetUiSink(UiSink sink);
    void Log(const std::wstring& line);
    static void LogFatal(const std::wstring& line);

private:
    std::wstring ResolvePath(const std::wstring& path, const std::wstring& dataRoot);
    std::wstring Timestamp() const;

    std::mutex mutex_;
    UiSink uiSink_;
    LogSettings settings_;
    std::wofstream file_;
    std::wstring filePath_;
};
