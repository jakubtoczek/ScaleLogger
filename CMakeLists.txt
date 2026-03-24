cmake_minimum_required(VERSION 3.24)
project(ScaleLoggerNative VERSION 0.96 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
  message(WARNING "ScaleLogger native app targets Windows. Parser/config tests still build cross-platform.")
endif()

add_library(scalelogger_core
  src/core/AppConfig.cpp
  src/core/ValueParser.cpp
  src/core/KeySequence.cpp
)
target_include_directories(scalelogger_core PUBLIC src)

if(WIN32)
  add_executable(ScaleLogger WIN32
    src/app/main.cpp
    src/app/AppController.cpp
    src/ui/MainDialog.cpp
    src/ui/AboutDialog.cpp
    src/serial/SerialPort.cpp
    src/serial/PortScanner.cpp
    src/input/InputInjector.cpp
    src/platform/win/WinHelpers.cpp
    resources/ScaleLogger.rc
  )
  target_include_directories(ScaleLogger PRIVATE src)
  target_link_libraries(ScaleLogger PRIVATE scalelogger_core)
  target_compile_definitions(ScaleLogger PRIVATE UNICODE _UNICODE)
endif()

add_executable(scalelogger_tests
  tests/test_main.cpp
  tests/test_parser.cpp
  tests/test_config.cpp
  tests/test_key_sequence.cpp
)
target_include_directories(scalelogger_tests PRIVATE src)
target_link_libraries(scalelogger_tests PRIVATE scalelogger_core)

include(CTest)
if(BUILD_TESTING)
  add_test(NAME scalelogger_tests COMMAND scalelogger_tests)
endif()
