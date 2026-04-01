@echo off
setlocal enableextensions enabledelayedexpansion

set "ROOT=%~dp0"
set "OUTDIR=%ROOT%slwide_runs"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set "APP_EXE=%ROOT%ScaleLogger.exe"
if not exist "%APP_EXE%" set "APP_EXE=%ROOT%out\build\windows-release\ScaleLogger.exe"

set "FATAL_LOG=%TEMP%\ScaleLogger_fatal.log"

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TAG=%%I"
set "SUMMARY=%OUTDIR%\_run_summary_%TAG%.txt"

echo slwide run tag=%TAG%> "%SUMMARY%"
echo app=%APP_EXE%>> "%SUMMARY%"
echo fatal_log=%FATAL_LOG%>> "%SUMMARY%"
echo.>> "%SUMMARY%"

call :run_case "1_skip_final" "set SCALELOGGER_SKIP_FINAL_RUNMAIN=1"
call :run_case "2_baseline_fresh_default" ""
call :run_case "3_legacy_default" "set SCALELOGGER_USE_LEGACY_RUNMAIN=1"
call :run_case "4_previous_deferred_startup" "set SCALELOGGER_USE_PREVIOUS_DEFERRED_STARTUP=1"
call :run_case "5_thin_startup_explicit" "set SCALELOGGER_USE_THIN_FRESH_STARTUP=1"
call :run_case "6_posted_limit_1" "set SCALELOGGER_FRESH_POSTED_STARTUP_LIMIT=1"
call :run_case "7_posted_limit_2" "set SCALELOGGER_FRESH_POSTED_STARTUP_LIMIT=2"

echo Done. Summary: %SUMMARY%
exit /b 0

:clear_env
set SCALELOGGER_SKIP_FINAL_RUNMAIN=
set SCALELOGGER_USE_LEGACY_RUNMAIN=
set SCALELOGGER_USE_PREVIOUS_DEFERRED_STARTUP=
set SCALELOGGER_USE_THIN_FRESH_STARTUP=
set SCALELOGGER_FRESH_STAGE_LIMIT=
set SCALELOGGER_FRESH_SUBSTAGE_LIMIT=
set SCALELOGGER_FRESH_SUBSTAGE_LIMIT_B=
set SCALELOGGER_FRESH_POSTED_STARTUP_LIMIT=
set SCALELOGGER_ENABLE_DIRECT_IMPL_PROBE=
set SCALELOGGER_ENABLE_RUNMAIN_WRAPPER_PROBE=
set SCALELOGGER_ENABLE_DIRECT_IMPL_TRAMPOLINE_PROBE=
set SCALELOGGER_ENABLE_WRAPPER_TRAMPOLINE_PROBE=
set SCALELOGGER_ENABLE_MAINDIALOG_SENTINEL_PROBE=
set SCALELOGGER_ENABLE_MAINDIALOG_SENTINEL_ARGS_PROBE=
set SCALELOGGER_ENABLE_WRAPPER_FRESH_PROBE=
set SCALELOGGER_ENABLE_IMPL_FRESH_PROBE=
set SCALELOGGER_ENABLE_WRAPPER_BODY_FRESH_PROBE=
set SCALELOGGER_ENABLE_IMPL_BODY_FRESH_PROBE=
set SCALELOGGER_ENABLE_RUNMAIN_FRESH_PROBE=
set SCALELOGGER_ENABLE_RUNMAIN_FRESH_BODY_PROBE=
set SCALELOGGER_DISABLE_DEFERRED_CONTROLLER_INIT=
goto :eof

:run_case
set "CASE_NAME=%~1"
set "CASE_ENV=%~2"
echo ==== %CASE_NAME% ====
echo ==== %CASE_NAME% ====>> "%SUMMARY%"
call :clear_env
if exist "%FATAL_LOG%" del /q "%FATAL_LOG%"
if not "%CASE_ENV%"=="" call %CASE_ENV%

if not exist "%APP_EXE%" (
  echo missing exe: %APP_EXE%
  echo missing exe>> "%SUMMARY%"
  echo.>> "%SUMMARY%"
  goto :eof
)

start "" /wait "%APP_EXE%"
set "EXITCODE=%ERRORLEVEL%"
echo exit_code=%EXITCODE%
echo exit_code=%EXITCODE%>> "%SUMMARY%"

set "CASE_LOG=%OUTDIR%\%CASE_NAME%_%TAG%.log"
if exist "%FATAL_LOG%" (
  copy /y "%FATAL_LOG%" "%CASE_LOG%" >nul
  echo fatal_log=%CASE_LOG%>> "%SUMMARY%"
) else (
  echo fatal_log=<missing> >> "%SUMMARY%"
)
echo.>> "%SUMMARY%"
ping -n 2 127.0.0.1 >nul
goto :eof
