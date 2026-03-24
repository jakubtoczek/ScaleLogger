#pragma once

#include "core/Types.hpp"

#include <atomic>
#include <functional>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <thread>
#endif

namespace scalelogger {

class SerialPort {
 public:
  using LineHandler = std::function<void(const std::string&)>;
  using LogHandler = std::function<void(const std::string&)>;

  bool Connect(const SerialSettings& settings, const LineHandler& onLine, const LogHandler& onLog, const LogHandler& onError);
  void Disconnect();
  bool IsConnected() const;

 private:
#ifdef _WIN32
  void ReceiveLoop();
  HANDLE handle_{INVALID_HANDLE_VALUE};
  std::thread receiveThread_{};
  std::atomic<bool> stopRequested_{false};
  SerialSettings settings_{};
  LineHandler onLine_{};
  LogHandler onLog_{};
  LogHandler onError_{};
#endif
  bool connected_{false};
};

} // namespace scalelogger
