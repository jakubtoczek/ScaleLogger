#pragma once

namespace scalelogger {

inline constexpr const char* kAppVersion = "0.97";
inline constexpr const wchar_t* kAppVersionWide = L"0.97";

#ifndef SCALELOGGER_BUILD_TAG
#define SCALELOGGER_BUILD_TAG "unknown"
#endif

inline constexpr const char* GetBuildTag() {
  return SCALELOGGER_BUILD_TAG;
}

} // namespace scalelogger
