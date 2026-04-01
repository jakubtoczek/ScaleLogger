@echo off
setlocal enableextensions enabledelayedexpansion

set "ROOT=%~dp0"
set "OUTDIR=%ROOT%slmin_runs"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set "APP_EXE=%ROOT%ScaleLogger.exe"
if not exist "%APP_EXE%" set "APP_EXE=%ROOT%out\build\windows-release\ScaleLogger.exe"
set "FATAL_LOG=%TEMP%\ScaleLogger_fatal.log"

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TAG=%%I"
set "SUMMARY=%OUTDIR%\_run_summary_%TAG%.txt"
echo slmin tag=%TAG%> "%SUMMARY%"

call :run_case "1_skip_final" "set SCALELOGGER_SKIP_FINAL_RUNMAIN=1"
call :run_case "2_sentinel" "set SCALELOGGER_MIN_ENTRY_TARGET=sentinel"
call :run_case "3_tiny_window" "set SCALELOGGER_MIN_ENTRY_TARGET=tiny-window"
call :run_case "4_fresh" "set SCALELOGGER_MIN_ENTRY_TARGET=fresh"

echo Done. Summary: %SUMMARY%
exit /b 0

:clear_env
set SCALELOGGER_SKIP_FINAL_RUNMAIN=
set SCALELOGGER_MIN_ENTRY_TARGET=
set SCALELOGGER_USE_LEGACY_RUNMAIN=
set SCALELOGGER_MAIN_CALL_TARGET=
set SCALELOGGER_RUNMAIN_FRESH_CALL_MODE=
goto :eof

:run_case
set "CASE_NAME=%~1"
set "CASE_ENV=%~2"
echo ==== %CASE_NAME% ====
echo ==== %CASE_NAME% ====>> "%SUMMARY%"
call :clear_env
if exist "%FATAL_LOG%" del /q "%FATAL_LOG%"
if not "%CASE_ENV%"=="" call %CASE_ENV%
start "" /wait "%APP_EXE%"
set "EXITCODE=%ERRORLEVEL%"
echo exit_code=%EXITCODE%
echo exit_code=%EXITCODE%>> "%SUMMARY%"
set "CASE_LOG=%OUTDIR%\%CASE_NAME%_%TAG%.log"
if exist "%FATAL_LOG%" copy /y "%FATAL_LOG%" "%CASE_LOG%" >nul
echo fatal_log=%CASE_LOG%>> "%SUMMARY%"
echo.>> "%SUMMARY%"
ping -n 2 127.0.0.1 >nul
goto :eof
