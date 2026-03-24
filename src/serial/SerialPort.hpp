#pragma once

#include "core/Types.hpp"

#include <functional>
#include <string>

#ifdef _WIN32
#include <Windows.h>
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
#endif
  bool connected_{false};
};

} // namespace scalelogger
