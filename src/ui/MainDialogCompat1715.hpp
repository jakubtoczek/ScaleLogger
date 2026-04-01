#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {

#ifdef _WIN32
int RunMainDialogCompat1715(HINSTANCE hInstance, int nCmdShow);
#endif

} // namespace scalelogger
