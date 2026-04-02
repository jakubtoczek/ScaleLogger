#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

enum class PostAction {
    None,
    Down,
    Right,
    Enter,
    Tab,
    Custom,
};

struct SerialSettings {
    std::wstring port = L"COM1";
    int baudRate = 9600;
    int dataBits = 8;
    int parity = 0;
    int stopBits = 1;
    int timeoutMs = 1000;
    std::wstring eol = L"\\r\\n";
};

struct ParseSettings {
    bool trimWhitespace = true;
    bool stripSuffix = true;
    std::wstring suffix = L"g";
    bool normalizeSign = true;
    bool preservePlus = true;
    bool preserveMinus = true;
    bool requireNumeric = true;
};

struct OutputSettings {
    bool dryRun = false;
    PostAction postAction = PostAction::Down;
    std::wstring customSequence;
};

enum class LogMode {
    None,
    SingleFile,
    PerSession,
};

struct LogSettings {
    std::wstring folder = L"%USERPROFILE%\\ScaleLogger\\logs";
    LogMode mode = LogMode::PerSession;
    bool verbose = false;
};

struct AppConfig {
    std::wstring configPath = L"ScaleLogger.config.json";
    std::wstring dataRoot = L"%USERPROFILE%\\ScaleLogger";
    bool connectOnStartup = false;
    SerialSettings serial;
    ParseSettings parse;
    OutputSettings output;
    LogSettings logging;
};

struct ParseResult {
    bool accepted = false;
    std::wstring raw;
    std::wstring processed;
    std::wstring reason;
};

enum class HealthState {
    Disconnected,
    Active,
    Idle,
    Error,
};

struct SerialLine {
    std::wstring text;
    std::chrono::steady_clock::time_point receivedAt;
};

struct PortInfo {
    std::wstring portName;
    std::wstring friendlyName;
    std::wstring description;
    std::wstring manufacturer;
    std::wstring connectionHint;
};

