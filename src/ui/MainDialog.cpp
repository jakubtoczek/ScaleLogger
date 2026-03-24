#include "ui/MainDialog.hpp"

#ifdef _WIN32
#include "app/AppController.hpp"
#include <cstdlib>
#include <filesystem>

namespace scalelogger {

int RunMainDialog(HINSTANCE, int) {
  AppController controller(std::filesystem::path(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : ".") / "ScaleLogger");
  controller.Initialize();
  MessageBoxW(nullptr, L"ScaleLogger native rewrite baseline is initialized.", L"ScaleLogger", MB_OK);
  return 0;
}

} // namespace scalelogger
#endif
