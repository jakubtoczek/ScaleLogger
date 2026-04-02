cmake_minimum_required(VERSION 3.24)
project(ScaleLogger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ScaleLogger WIN32
  src/main.cpp
  src/AppController.cpp
  src/AppConfig.cpp
  src/LineParser.cpp
  src/SerialPort.cpp
  src/TextInjector.cpp
  src/Logger.cpp
  src/MainWindow.cpp
  src/SettingsWindow.cpp
)

target_include_directories(ScaleLogger PRIVATE include)

target_compile_definitions(ScaleLogger PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(ScaleLogger PRIVATE user32 gdi32 comdlg32 setupapi)
