#ifdef _WIN32
#include "ui/MainDialog.hpp"

#include <Windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  return scalelogger::RunMainDialog(hInstance, nCmdShow);
}
#endif
