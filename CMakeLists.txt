cmake_minimum_required(VERSION 3.24)
project(ScaleLoggerNative VERSION 0.97 LANGUAGES CXX)

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
  # Some environments export RC as numeric placeholders (for example "0"),
  # which breaks CMake resource compiler discovery.
  if(DEFINED ENV{RC} AND "$ENV{RC}" MATCHES "^[0-9]+$")
    message(WARNING "Ignoring invalid RC environment override: '$ENV{RC}'. Falling back to normal RC discovery.")
    unset(ENV{RC})
  endif()
  if(DEFINED CMAKE_RC_COMPILER AND CMAKE_RC_COMPILER MATCHES "^[0-9]+$")
    unset(CMAKE_RC_COMPILER CACHE)
  endif()

  enable_language(RC)
  set(SCALELOGGER_VERSION_COMMA "${PROJECT_VERSION}")
  string(REPLACE "." ";" _version_parts "${SCALELOGGER_VERSION_COMMA}")
  list(LENGTH _version_parts _version_part_count)
  while(_version_part_count LESS 4)
    list(APPEND _version_parts "0")
    math(EXPR _version_part_count "${_version_part_count} + 1")
  endwhile()
  list(SUBLIST _version_parts 0 4 _version_parts)
  list(JOIN _version_parts "," SCALELOGGER_VERSION_COMMA)
  configure_file(resources/ScaleLogger.rc.in ${CMAKE_CURRENT_BINARY_DIR}/ScaleLogger.rc @ONLY)

  add_executable(ScaleLogger WIN32
    src/app/main.cpp
    src/app/AppController.cpp
    src/app/ConfigService.cpp
    src/ui/MainDialog.cpp
    src/ui/SettingsDialogLogic.cpp
    src/ui/AboutDialog.cpp
    src/serial/SerialPort.cpp
    src/serial/PortScanner.cpp
    src/input/InputInjector.cpp
    src/platform/win/WinHelpers.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ScaleLogger.rc
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
