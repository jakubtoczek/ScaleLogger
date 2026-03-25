#include "serial/SerialPort.hpp"

#ifdef _WIN32

#include <chrono>
#include <thread>

namespace scalelogger {
namespace {
bool EndsWith(const std::string& value, const std::string& suffix) {
  if (suffix.size() > value.size()) return false;
  return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}
} // namespace

bool SerialPort::Connect(const SerialSettings& settings, const LineHandler& onLine, const LogHandler& onLog, const LogHandler& onError) {
  Disconnect();

  const std::string full = "\\\\.\\" + settings.port;
  handle_ = CreateFileA(full.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (handle_ == INVALID_HANDLE_VALUE) {
    onError("Serial connection failed on " + settings.port);
    return false;
  }

  DCB dcb{};
  dcb.DCBlength = sizeof(DCB);
  if (!GetCommState(handle_, &dcb)) {
    onError("GetCommState failed.");
    Disconnect();
    return false;
  }
  dcb.BaudRate = static_cast<DWORD>(settings.baudRate);
  dcb.ByteSize = static_cast<BYTE>(settings.dataBits);
  dcb.Parity = settings.parity == 'O' ? ODDPARITY : (settings.parity == 'E' ? EVENPARITY : NOPARITY);
  dcb.StopBits = settings.stopBits == 2.0F ? TWOSTOPBITS : ONESTOPBIT;
  if (!SetCommState(handle_, &dcb)) {
    onError("SetCommState failed.");
    Disconnect();
    return false;
  }

  COMMTIMEOUTS t{};
  t.ReadIntervalTimeout = MAXDWORD;
  t.ReadTotalTimeoutConstant = static_cast<DWORD>(settings.timeoutSeconds * 1000);
  t.ReadTotalTimeoutMultiplier = 0;
  SetCommTimeouts(handle_, &t);

  // Required stale-buffer handling order.
  PurgeComm(handle_, PURGE_RXCLEAR | PURGE_RXABORT);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  char tmp[256];
  DWORD read = 0;
  while (ReadFile(handle_, tmp, sizeof(tmp), &read, nullptr) && read > 0) {
  }
  PurgeComm(handle_, PURGE_RXCLEAR | PURGE_RXABORT);
  onLog("Discarded buffered serial data on connect.");

  settings_ = settings;
  onLine_ = onLine;
  onLog_ = onLog;
  onError_ = onError;
  stopRequested_.store(false);
  connected_ = true;
  onLog("Serial connection opened on " + settings.port + " at " + std::to_string(settings.baudRate) + " baud.");

  receiveThread_ = std::thread([this]() { ReceiveLoop(); });
  return true;
}

void SerialPort::ReceiveLoop() {
  std::string buffer;
  char ch = 0;
  DWORD read = 0;

  while (!stopRequested_.load()) {
    const BOOL ok = ReadFile(handle_, &ch, 1, &read, nullptr);
    if (!ok) {
      if (!stopRequested_.load() && onError_) onError_("Serial read error.");
      break;
    }
    if (read == 0) continue;

    const bool fallbackTerminator = (ch == '\r' || ch == '\n');
    buffer.push_back(ch);
    if (EndsWith(buffer, settings_.eol) || fallbackTerminator) {
      std::string line = buffer;
      if (EndsWith(line, settings_.eol)) line = line.substr(0, line.size() - settings_.eol.size());
      while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
      buffer.clear();
      if (onLine_ && !line.empty()) onLine_(line);
    }
  }
}

void SerialPort::Disconnect() {
  stopRequested_.store(true);
  connected_ = false;

  if (handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
  }

  if (receiveThread_.joinable()) receiveThread_.join();
}

bool SerialPort::IsConnected() const { return connected_; }

} // namespace scalelogger

#else
namespace scalelogger {
bool SerialPort::Connect(const SerialSettings&, const LineHandler&, const LogHandler&, const LogHandler&) { return false; }
void SerialPort::Disconnect() {}
bool SerialPort::IsConnected() const { return false; }
} // namespace scalelogger
#endif
