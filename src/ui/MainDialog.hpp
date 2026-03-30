#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {

#ifdef _WIN32
int ProbeMainDialogBasic();
int ProbeMainDialogTraceEarly();
int ProbeMainDialogWithArgs(HINSTANCE hInstance, int nCmdShow);
int ProbeMainDialogTouchUi(HINSTANCE hInstance);
int ProbeRunMainDialogImplDirect(HINSTANCE hInstance, int nCmdShow);
int RunMainDialog(HINSTANCE hInstance, int nCmdShow);
#endif

} // namespace scalelogger
