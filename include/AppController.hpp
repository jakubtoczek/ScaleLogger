#pragma once

#include <functional>
#include <memory>

#include "AppConfigService.hpp"
#include "LineParser.hpp"
#include "Logger.hpp"
#include "SerialPort.hpp"
#include "TextInjector.hpp"
#include "Types.hpp"

class AppController {
public:
    using UiLogSink = std::function<void(const std::wstring&)>;
    using StatusSink = std::function<void(HealthState, const std::wstring&)>;

    bool Initialize(const std::wstring& exeDir);
    void SetUiLogSink(UiLogSink sink);
    void SetStatusSink(StatusSink sink);

    void Connect();
    void Disconnect();
    void ToggleConnection();

    void ScanPortsAsync(std::function<void(std::vector<PortInfo>)> onDone);
    void TestReceiveAsync(std::function<void(std::wstring)> onDone);

    const AppConfig& Config() const;
    void UpdateConfig(const AppConfig& config);

private:
    void HandleLine(const SerialLine& line);
    void PublishStatus(HealthState state, const std::wstring& text);

    AppConfigService configService_;
    AppConfig config_;
    LineParser parser_;
    TextInjector injector_;
    SerialPort serial_;
    Logger logger_;
    StatusSink statusSink_;
};
