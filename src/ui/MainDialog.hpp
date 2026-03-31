#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif

namespace scalelogger {

#ifdef _WIN32
using RunMainDialogFn = int (*)(HINSTANCE, int);
int ProbeMainDialogBasic();
int ProbeMainDialogTraceEarly();
int ProbeMainDialogWithArgs(HINSTANCE hInstance, int nCmdShow);
int ProbeMainDialogTouchUi(HINSTANCE hInstance);
int ProbeRunMainDialogImplDirect(HINSTANCE hInstance, int nCmdShow);
int ProbeRunMainDialogWrapper(HINSTANCE hInstance, int nCmdShow);
int ProbeRunMainDialogImplDirectViaTrampoline(HINSTANCE hInstance, int nCmdShow);
int ProbeRunMainDialogWrapperViaTrampoline(HINSTANCE hInstance, int nCmdShow);
int RunMainDialog(HINSTANCE hInstance, int nCmdShow);
#endif

} // namespace scalelogger
