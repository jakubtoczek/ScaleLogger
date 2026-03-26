#include "serial/PortScanner.hpp"

#ifdef _WIN32
#include <SetupAPI.h>
#include <Windows.h>
#include <devguid.h>
#include <regstr.h>
#pragma comment(lib, "Setupapi.lib")
#endif

namespace scalelogger {
std::vector<std::string> ScanComPorts() {
  std::vector<std::string> ports;
#ifdef _WIN32
  HDEVINFO devInfo = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
  if (devInfo != INVALID_HANDLE_VALUE) {
    SP_DEVINFO_DATA devData{};
    devData.cbSize = sizeof(devData);
    for (DWORD index = 0; SetupDiEnumDeviceInfo(devInfo, index, &devData); ++index) {
      char friendlyName[256]{};
      if (!SetupDiGetDeviceRegistryPropertyA(devInfo, &devData, SPDRP_FRIENDLYNAME, nullptr, reinterpret_cast<PBYTE>(friendlyName),
                                             sizeof(friendlyName), nullptr)) {
        continue;
      }
      std::string friendly(friendlyName);
      const auto l = friendly.find("(COM");
      const auto r = friendly.find(')', l == std::string::npos ? 0 : l);
      if (l == std::string::npos || r == std::string::npos || r <= l + 1) continue;
      const std::string comPort = friendly.substr(l + 1, r - l - 1);
      ports.push_back(comPort + " — " + friendly);
    }
    SetupDiDestroyDeviceInfoList(devInfo);
  }
  if (ports.empty()) {
    char target[16];
    char devicePathBuffer[4096];
    for (int i = 1; i <= 256; ++i) {
      wsprintfA(target, "COM%d", i);
      const DWORD result = QueryDosDeviceA(target, devicePathBuffer, static_cast<DWORD>(sizeof(devicePathBuffer)));
      if (result != 0) ports.emplace_back(target);
    }
  }
#endif
  return ports;
}
} // namespace scalelogger
