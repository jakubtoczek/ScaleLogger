#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#include "Types.hpp"

class SerialPort {
public:
    using LineCallback = std::function<void(const SerialLine&)>;

    ~SerialPort();

    bool Connect(const SerialSettings& settings);
    void Disconnect();
    bool IsConnected() const;
    HealthState Health() const;

    void SetLineCallback(LineCallback callback);
    std::optional<std::wstring> TestReceive(const SerialSettings& settings, int waitMs);
    std::vector<PortInfo> ScanPorts() const;

private:
    void RunReadLoop();
    std::wstring DecodeEol(const std::wstring& escaped) const;

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::thread readThread_;
    std::atomic<bool> running_{false};
    std::atomic<HealthState> health_{HealthState::Disconnected};
    SerialSettings settings_;
    LineCallback callback_;
    mutable std::mutex callbackMutex_;
    std::chrono::steady_clock::time_point lastActivity_{};
};
