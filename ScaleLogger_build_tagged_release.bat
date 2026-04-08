@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ---------------------------------------------------------------------------
REM ScaleLogger release wrapper with build-tagged executable output.
REM Usage:
REM   ScaleLogger_build_tagged_release.bat                     -> auto UTC build tag (yyyyMMddTHHmmssZ), cleanup enabled
REM   ScaleLogger_build_tagged_release.bat custom_tag          -> explicit build tag override, cleanup enabled
REM   ScaleLogger_build_tagged_release.bat keep                -> auto UTC build tag, preserve out\ and intermediate build artifacts
REM   ScaleLogger_build_tagged_release.bat custom_tag keep     -> explicit build tag + preserve out\ and intermediate build artifacts
REM ---------------------------------------------------------------------------

set "TARGET_DIR=%~dp0"
if "%TARGET_DIR:~-1%"=="\" set "TARGET_DIR=%TARGET_DIR:~0,-1%"
cd /d "%TARGET_DIR%"

set "APP_VERSION="
set "RELEASE_DIR=release"
set "BUILD_DIR=out\build\windows-vs2026-x64"
set "SOURCE_EXE=%BUILD_DIR%\Release\ScaleLogger.exe"
set "KEEP_BUILD=0"
set "BUILD_TAG="

if /I "%~1"=="keep" (
  set "KEEP_BUILD=1"
) else (
  set "BUILD_TAG=%~1"
)
if /I "%~2"=="keep" set "KEEP_BUILD=1"

if not defined BUILD_TAG (
  for /f "usebackq delims=" %%T in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')"`) do (
    set "BUILD_TAG=%%T"
  )
)
if not defined BUILD_TAG (
  echo ERROR: Failed to determine build tag.
  exit /b 1
)

set "OUTPUT_NAME=ScaleLogger_%BUILD_TAG%.exe"
set "OUTPUT_EXE=%RELEASE_DIR%\%OUTPUT_NAME%"
set "CHECKSUM_FILE=%RELEASE_DIR%\SHA256SUMS.txt"

for /f "usebackq delims=" %%V in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content -Raw 'CMakeLists.txt'; $m=[regex]::Match($t,'project\([^\)]*VERSION\s+([0-9]+(?:\.[0-9]+){1,3})','IgnoreCase'); if($m.Success){$m.Groups[1].Value}"`) do (
  set "APP_VERSION=%%V"
)
if not defined APP_VERSION (
  echo ERROR: Failed to derive app version from CMakeLists.txt.
  exit /b 1
)
set "MANIFEST_FILE=%RELEASE_DIR%\BUILD_MANIFEST_%APP_VERSION%.txt"
set "STAGE_DIR=%RELEASE_DIR%\.staging_%BUILD_TAG%"

echo [1/5] Configuring (windows-vs2026-x64)...
cmake --preset windows-vs2026-x64 -U CMAKE_RC_COMPILER
if %errorlevel% neq 0 (
  echo ERROR: Configure failed.
  exit /b %errorlevel%
)

echo [2/5] Building (windows-release)...
cmake --build --preset windows-release
if %errorlevel% neq 0 (
  echo ERROR: Build failed.
  exit /b %errorlevel%
)

if not exist "%SOURCE_EXE%" (
  echo ERROR: Build output not found: %SOURCE_EXE%
  exit /b 1
)

if exist "%STAGE_DIR%" rmdir /s /q "%STAGE_DIR%"
mkdir "%STAGE_DIR%"
if %errorlevel% neq 0 (
  echo ERROR: Failed to create staging directory: %STAGE_DIR%
  exit /b %errorlevel%
)

echo [3/5] Packaging executable...
copy /y "%SOURCE_EXE%" "%STAGE_DIR%\%OUTPUT_NAME%" >nul
if %errorlevel% neq 0 (
  echo ERROR: Failed to copy executable into staging.
  exit /b %errorlevel%
)

echo [4/5] Generating checksum...
call generate_sha256.bat "%STAGE_DIR%\%OUTPUT_NAME%" "%STAGE_DIR%\SHA256SUMS.txt"
if %errorlevel% neq 0 (
  echo ERROR: Failed to generate SHA256 checksum.
  exit /b %errorlevel%
)

echo [5/5] Generating manifest...
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_manifest.ps1 ^
  -OutputExe "%OUTPUT_NAME%" ^
  -BuildTag "%BUILD_TAG%" ^
  -ChecksumFile "%STAGE_DIR%\SHA256SUMS.txt" ^
  -ConfigurePreset "windows-vs2026-x64" ^
  -BuildPreset "windows-release" ^
  -BuildType "Release" ^
  -Platform "x64"
if %errorlevel% neq 0 (
  echo ERROR: Manifest generation failed.
  exit /b %errorlevel%
)

if not exist BUILD_MANIFEST.md (
  echo ERROR: Expected BUILD_MANIFEST.md was not produced.
  exit /b 1
)
move /y BUILD_MANIFEST.md "%STAGE_DIR%\BUILD_MANIFEST_%APP_VERSION%.txt" >nul
if %errorlevel% neq 0 (
  echo ERROR: Failed to stage manifest file.
  exit /b %errorlevel%
)

if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"
copy /y "%STAGE_DIR%\%OUTPUT_NAME%" "%OUTPUT_EXE%" >nul
copy /y "%STAGE_DIR%\SHA256SUMS.txt" "%CHECKSUM_FILE%" >nul
copy /y "%STAGE_DIR%\BUILD_MANIFEST_%APP_VERSION%.txt" "%MANIFEST_FILE%" >nul
if %errorlevel% neq 0 (
  echo ERROR: Failed to publish release artifacts.
  exit /b %errorlevel%
)
rmdir /s /q "%STAGE_DIR%"

if "%KEEP_BUILD%"=="0" (
  echo.
  echo === Cleanup ===
  echo Cleaning build artifacts: %TARGET_DIR%\out
  if exist "%TARGET_DIR%\out" (
    rmdir /s /q "%TARGET_DIR%\out"
    if exist "%TARGET_DIR%\out" (
      echo WARNING: Failed to fully remove out folder
    ) else (
      echo Removed: %TARGET_DIR%\out
    )
  )
) else (
  echo.
  echo === Cleanup skipped ===
  echo KEEP_BUILD=1, preserving build artifacts.
)

echo.
echo Release build complete.
echo Build tag:  %BUILD_TAG%
echo KEEP_BUILD: %KEEP_BUILD%
echo Executable: %OUTPUT_EXE%
echo Checksum:   %CHECKSUM_FILE%
echo Manifest:   %MANIFEST_FILE%

endlocal
