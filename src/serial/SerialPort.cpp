#include "serial/SerialPort.hpp"

#ifdef _WIN32

#include <chrono>
#include <thread>

namespace scalelogger {

bool SerialPort::Connect(const SerialSettings& settings, const LineHandler&, const LogHandler& onLog, const LogHandler& onError) {
  const std::string full = "\\\\.\\" + settings.port;
  handle_ = CreateFileA(full.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (handle_ == INVALID_HANDLE_VALUE) {
    onError("Serial connection failed on " + settings.port);
    return false;
  }

  DCB dcb{};
  dcb.DCBlength = sizeof(DCB);
  GetCommState(handle_, &dcb);
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

  connected_ = true;
  onLog("Serial connection opened on " + settings.port + ".");
  return true;
}

void SerialPort::Disconnect() {
  connected_ = false;
  if (handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
  }
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
