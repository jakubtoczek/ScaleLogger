#include "serial/PortScanner.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {
std::vector<std::string> ScanComPorts() {
  std::vector<std::string> ports;
#ifdef _WIN32
  char target[16];
  char devicePathBuffer[4096];
  for (int i = 1; i <= 256; ++i) {
    wsprintfA(target, "COM%d", i);
    const DWORD result = QueryDosDeviceA(target, devicePathBuffer, static_cast<DWORD>(sizeof(devicePathBuffer)));
    if (result != 0) ports.emplace_back(target);
  }
#endif
  return ports;
}
} // namespace scalelogger
