#include "SerialPort.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

#include <setupapi.h>

SerialPort::~SerialPort() {
    Disconnect();
}

bool SerialPort::Connect(const SerialSettings& settings) {
    const char* forceOff = std::getenv("SCALELOGGER_FORCE_NO_SERIAL");
    if (forceOff && std::string(forceOff) == "1") {
        health_ = HealthState::Error;
        return false;
    }

    Disconnect();
    settings_ = settings;
    std::wstring device = L"\\\\.\\" + settings.port;
    handle_ = CreateFileW(device.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        health_ = HealthState::Error;
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(handle_, &dcb);
    dcb.BaudRate = settings.baudRate;
    dcb.ByteSize = static_cast<BYTE>(settings.dataBits);
    dcb.Parity = static_cast<BYTE>(settings.parity);
    dcb.StopBits = static_cast<BYTE>(settings.stopBits);
    if (!SetCommState(handle_, &dcb)) {
        Disconnect();
        health_ = HealthState::Error;
        return false;
    }

    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout = 50;
    to.ReadTotalTimeoutConstant = settings.timeoutMs;
    to.ReadTotalTimeoutMultiplier = 0;
    SetCommTimeouts(handle_, &to);

    PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
    Sleep(60);
    char drain[256] = {};
    DWORD bytes = 0;
    ReadFile(handle_, drain, sizeof(drain), &bytes, nullptr);
    PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);

    running_ = true;
    lastActivity_ = std::chrono::steady_clock::now();
    health_ = HealthState::Active;
    readThread_ = std::thread(&SerialPort::RunReadLoop, this);
    return true;
}

void SerialPort::Disconnect() {
    running_ = false;
    if (readThread_.joinable()) {
        readThread_.join();
    }
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    health_ = HealthState::Disconnected;
}

bool SerialPort::IsConnected() const {
    return handle_ != INVALID_HANDLE_VALUE;
}

HealthState SerialPort::Health() const {
    if (!IsConnected()) return health_.load();
    auto elapsed = std::chrono::steady_clock::now() - lastActivity_;
    if (elapsed > std::chrono::seconds(3)) return HealthState::Idle;
    return HealthState::Active;
}

void SerialPort::SetLineCallback(LineCallback callback) {
    std::scoped_lock lock(callbackMutex_);
    callback_ = std::move(callback);
}

std::optional<std::wstring> SerialPort::TestReceive(const SerialSettings& settings, int waitMs) {
    if (IsConnected()) return std::nullopt;

    std::wstring device = L"\\\\.\\" + settings.port;
    HANDLE tempHandle = CreateFileW(device.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (tempHandle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(tempHandle, &dcb);
    dcb.BaudRate = settings.baudRate;
    dcb.ByteSize = static_cast<BYTE>(settings.dataBits);
    dcb.Parity = static_cast<BYTE>(settings.parity);
    dcb.StopBits = static_cast<BYTE>(settings.stopBits);
    if (!SetCommState(tempHandle, &dcb)) {
        CloseHandle(tempHandle);
        return std::nullopt;
    }

    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout = 50;
    to.ReadTotalTimeoutConstant = 100;
    SetCommTimeouts(tempHandle, &to);

    PurgeComm(tempHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    Sleep(60);

    const auto eol = DecodeEol(settings.eol);
    const std::string eolNarrow(eol.begin(), eol.end());
    std::string buffer;
    char chunk[128] = {};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
    while (std::chrono::steady_clock::now() < deadline) {
        DWORD bytesRead = 0;
        if (!ReadFile(tempHandle, chunk, sizeof(chunk), &bytesRead, nullptr)) {
            break;
        }
        if (bytesRead == 0) {
            continue;
        }
        buffer.append(chunk, chunk + bytesRead);
        const auto pos = buffer.find(eolNarrow);
        if (pos != std::string::npos) {
            const std::string line = buffer.substr(0, pos);
            CloseHandle(tempHandle);
            return std::wstring(line.begin(), line.end());
        }
    }

    CloseHandle(tempHandle);
    return std::nullopt;
}

std::vector<PortInfo> SerialPort::ScanPorts() const {
    // TODO: enrich with SetupAPI metadata (friendly name, manufacturer, bus type).
    std::vector<PortInfo> ports;
    for (int i = 1; i <= 32; ++i) {
        std::wstring name = L"COM" + std::to_wstring(i);
        std::wstring device = L"\\\\.\\" + name;
        HANDLE h = CreateFileW(device.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            ports.push_back({name, name, L"Serial Port", L"Unknown", L"USB/UART"});
            CloseHandle(h);
        }
    }
    return ports;
}

void SerialPort::RunReadLoop() {
    const std::wstring eol = DecodeEol(settings_.eol);
    std::string buffer;
    char chunk[128] = {};
    while (running_) {
        DWORD bytesRead = 0;
        if (!ReadFile(handle_, chunk, sizeof(chunk), &bytesRead, nullptr)) {
            health_ = HealthState::Error;
            break;
        }
        if (bytesRead == 0) {
            continue;
        }
        lastActivity_ = std::chrono::steady_clock::now();
        buffer.append(chunk, chunk + bytesRead);

        while (true) {
            std::string eolNarrow(eol.begin(), eol.end());
            const auto pos = buffer.find(eolNarrow);
            if (pos == std::string::npos) break;
            const std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + eolNarrow.size());
            std::wstring wide(line.begin(), line.end());
            std::scoped_lock lock(callbackMutex_);
            if (callback_) {
                callback_({wide, std::chrono::steady_clock::now()});
            }
        }
    }
}

std::wstring SerialPort::DecodeEol(const std::wstring& escaped) const {
    std::wstring out;
    for (size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == L'\\' && i + 1 < escaped.size()) {
            if (escaped[i + 1] == L'r') {
                out.push_back(L'\r');
                ++i;
                continue;
            }
            if (escaped[i + 1] == L'n') {
                out.push_back(L'\n');
                ++i;
                continue;
            }
        }
        out.push_back(escaped[i]);
    }
    return out;
}
