#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {

#ifdef _WIN32
int RunMainDialog2(HINSTANCE hInstance, int nCmdShow);
#endif

} // namespace scalelogger
