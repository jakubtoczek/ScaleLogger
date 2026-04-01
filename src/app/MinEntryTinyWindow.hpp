#pragma once

#ifdef _WIN32
#include <Windows.h>

namespace scalelogger {
int LaunchTinyWindow(HINSTANCE hInstance, int nCmdShow);
}
#endif
