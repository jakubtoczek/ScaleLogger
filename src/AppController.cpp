#include "AppController.hpp"

#include <filesystem>
#include <thread>

bool AppController::Initialize(const std::wstring& exeDir) {
    Logger::LogFatal(L"startup: initialize begin");
    config_ = configService_.Load(exeDir);
    std::filesystem::create_directories(std::filesystem::path(config_.dataRoot) / L"logs");
    logger_.Initialize(config_.logging, config_.dataRoot);
    serial_.SetLineCallback([this](const SerialLine& line) { HandleLine(line); });
    logger_.Log(L"ScaleLogger started");
    PublishStatus(HealthState::Disconnected, L"Disconnected");

    if (config_.connectOnStartup) {
        logger_.Log(L"connect_on_startup=true; waiting for explicit user connect by design");
    }
    return true;
}

void AppController::SetUiLogSink(UiLogSink sink) {
    logger_.SetUiSink(std::move(sink));
}

void AppController::SetStatusSink(StatusSink sink) {
    statusSink_ = std::move(sink);
}

void AppController::Connect() {
    if (serial_.Connect(config_.serial)) {
        logger_.Log(L"Connected: " + config_.serial.port);
        PublishStatus(HealthState::Active, L"Connected");
    } else {
        logger_.Log(L"Connection failed");
        PublishStatus(HealthState::Error, L"Error");
    }
}

void AppController::Disconnect() {
    serial_.Disconnect();
    logger_.Log(L"Disconnected");
    PublishStatus(HealthState::Disconnected, L"Disconnected");
}

void AppController::ToggleConnection() {
    if (serial_.IsConnected()) {
        Disconnect();
    } else {
        Connect();
    }
}

void AppController::ScanPortsAsync(std::function<void(std::vector<PortInfo>)> onDone) {
    std::thread([this, onDone = std::move(onDone)]() mutable {
        auto ports = serial_.ScanPorts();
        onDone(std::move(ports));
    }).detach();
}

void AppController::TestReceiveAsync(std::function<void(std::wstring)> onDone) {
    std::thread([this, onDone = std::move(onDone)]() mutable {
        auto res = serial_.TestReceive(config_.serial, 500);
        if (res) onDone(*res);
        else onDone(L"No line captured");
    }).detach();
}

const AppConfig& AppController::Config() const {
    return config_;
}

void AppController::UpdateConfig(const AppConfig& config) {
    config_ = config;
    configService_.Save(config_, config_.configPath);
    logger_.Log(L"Configuration updated");
}

void AppController::HandleLine(const SerialLine& line) {
    ParseResult parsed = parser_.Parse(line.text, config_.parse);
    if (!parsed.accepted) {
        logger_.Log(L"Rejected line: " + parsed.raw + L" reason=" + parsed.reason);
        return;
    }

    logger_.Log(L"Accepted line: " + parsed.processed);
    if (config_.output.dryRun) {
        logger_.Log(L"DryRun enabled; skipping injection");
        return;
    }

    if (!injector_.InjectText(parsed.processed)) {
        logger_.Log(L"Inject failed");
        return;
    }
    if (!injector_.ExecutePostAction(config_.output)) {
        logger_.Log(L"Post action failed");
    }
}

void AppController::PublishStatus(HealthState state, const std::wstring& text) {
    if (statusSink_) {
        statusSink_(state, text);
    }
}
