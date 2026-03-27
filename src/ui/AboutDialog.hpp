#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {

#ifdef _WIN32
void ShowAbout(HWND parent);
#endif

} // namespace scalelogger
