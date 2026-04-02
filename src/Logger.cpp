#include "Logger.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include <windows.h>

void Logger::Initialize(const LogSettings& settings, const std::wstring& dataRoot) {
    std::scoped_lock lock(mutex_);
    settings_ = settings;
    filePath_ = ResolvePath(settings.folder, dataRoot);
    if (settings.mode == LogMode::None) {
        return;
    }
    std::filesystem::create_directories(filePath_);
}

void Logger::SetUiSink(UiSink sink) {
    std::scoped_lock lock(mutex_);
    uiSink_ = std::move(sink);
}

void Logger::Log(const std::wstring& line) {
    std::scoped_lock lock(mutex_);
    const auto text = L"[" + Timestamp() + L"] " + line;
    if (uiSink_) {
        uiSink_(text);
    }
    if (settings_.mode == LogMode::None) {
        return;
    }
    if (!file_.is_open()) {
        std::wstring fileName = L"ScaleLogger.log";
        if (settings_.mode == LogMode::PerSession) {
            fileName = L"ScaleLogger_" + Timestamp() + L".log";
            for (auto& ch : fileName) {
                if (ch == L':') ch = L'-';
            }
        }
        file_.open(std::filesystem::path(filePath_) / fileName, std::ios::out | std::ios::app);
    }
    if (file_.is_open()) {
        file_ << text << L"\n";
        file_.flush();
    }
}

void Logger::LogFatal(const std::wstring& line) {
    std::wofstream file(std::filesystem::temp_directory_path() / L"ScaleLogger_fatal.log", std::ios::out | std::ios::app);
    if (file.is_open()) {
        file << line << L"\n";
    }
}

std::wstring Logger::ResolvePath(const std::wstring& path, const std::wstring& dataRoot) {
    wchar_t expanded[MAX_PATH] = {};
    ExpandEnvironmentStringsW(path.c_str(), expanded, MAX_PATH);
    std::filesystem::path p(expanded);
    if (p.is_absolute()) {
        return p.wstring();
    }
    return (std::filesystem::path(dataRoot) / p).wstring();
}

std::wstring Logger::Timestamp() const {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &time);
    std::wstringstream ss;
    ss << std::put_time(&tm, L"%H:%M:%S");
    return ss.str();
}
