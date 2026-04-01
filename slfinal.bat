@echo off
setlocal enableextensions enabledelayedexpansion

set "ROOT=%~dp0"
set "OUTDIR=%ROOT%slfinal_runs"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set "APP_EXE=%ROOT%ScaleLogger.exe"
if not exist "%APP_EXE%" set "APP_EXE=%ROOT%out\build\windows-release\ScaleLogger.exe"
set "FATAL_LOG=%TEMP%\ScaleLogger_fatal.log"

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TAG=%%I"
set "SUMMARY=%OUTDIR%\_run_summary_%TAG%.txt"
echo slfinal tag=%TAG%> "%SUMMARY%"
echo app=%APP_EXE%>> "%SUMMARY%"
echo.>> "%SUMMARY%"

call :run_case "1_skip_final" "set SCALELOGGER_SKIP_FINAL_RUNMAIN=1"
call :run_case "2_final_sentinel_args" "set SCALELOGGER_MAIN_CALL_TARGET=sentinel-args"
call :run_case "3_final_probe_with_args" "set SCALELOGGER_MAIN_CALL_TARGET=probe-with-args"
call :run_case "4_final_fresh_probe" "set SCALELOGGER_MAIN_CALL_TARGET=fresh-probe"
call :run_case "5_final_fresh_body_probe" "set SCALELOGGER_MAIN_CALL_TARGET=fresh-body-probe"
call :run_case "6_final_fresh_direct" "set SCALELOGGER_MAIN_CALL_TARGET=runmain-fresh-direct"
call :run_case "7_final_fresh_fn" "set SCALELOGGER_MAIN_CALL_TARGET=runmain-fresh-direct && set SCALELOGGER_RUNMAIN_FRESH_CALL_MODE=fn"
call :run_case "8_final_fresh_mainthunk" "set SCALELOGGER_MAIN_CALL_TARGET=runmain-fresh-direct && set SCALELOGGER_RUNMAIN_FRESH_CALL_MODE=mainthunk"
call :run_case "9_final_legacy_direct" "set SCALELOGGER_MAIN_CALL_TARGET=legacy-direct"

echo Done. Summary: %SUMMARY%
exit /b 0

:clear_env
set SCALELOGGER_SKIP_FINAL_RUNMAIN=
set SCALELOGGER_MAIN_CALL_TARGET=
set SCALELOGGER_RUNMAIN_FRESH_CALL_MODE=
set SCALELOGGER_USE_LEGACY_RUNMAIN=
set SCALELOGGER_USE_THIN_FRESH_STARTUP=
set SCALELOGGER_USE_PREVIOUS_DEFERRED_STARTUP=
set SCALELOGGER_DISABLE_DEFERRED_CONTROLLER_INIT=
set SCALELOGGER_FRESH_STAGE_LIMIT=
set SCALELOGGER_FRESH_SUBSTAGE_LIMIT=
set SCALELOGGER_FRESH_SUBSTAGE_LIMIT_B=
set SCALELOGGER_FRESH_POSTED_STARTUP_LIMIT=
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
