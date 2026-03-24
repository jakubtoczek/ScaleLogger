@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "APP_VERSION=0.95"
set "RELEASE_DIR=release"
set "BUILD_DIR=out\build\windows-vs2022-x64"
set "SOURCE_EXE=%BUILD_DIR%\Release\ScaleLogger.exe"
set "OUTPUT_EXE=%RELEASE_DIR%\ScaleLogger.exe"
set "CHECKSUM_FILE=%RELEASE_DIR%\SHA256SUMS.txt"
set "MANIFEST_FILE=%RELEASE_DIR%\BUILD_MANIFEST_%APP_VERSION%.txt"

where cmake >nul 2>nul
if %errorlevel% neq 0 (
  echo CMake was not found in PATH.
  exit /b 1
)

if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"
if %errorlevel% neq 0 exit /b %errorlevel%

echo Configuring (windows-vs2022-x64)...
cmake --preset windows-vs2022-x64
if %errorlevel% neq 0 exit /b %errorlevel%

echo Building (windows-release)...
cmake --build --preset windows-release
if %errorlevel% neq 0 exit /b %errorlevel%

echo Running tests (windows-test)...
ctest --preset windows-test
if %errorlevel% neq 0 exit /b %errorlevel%

if not exist "%SOURCE_EXE%" (
  echo Build output not found: %SOURCE_EXE%
  exit /b 1
)

copy /y "%SOURCE_EXE%" "%OUTPUT_EXE%" >nul
if %errorlevel% neq 0 (
  echo Failed to copy executable to release folder.
  exit /b %errorlevel%
)

call generate_sha256.bat "%OUTPUT_EXE%" "%CHECKSUM_FILE%"
if %errorlevel% neq 0 exit /b %errorlevel%

powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_manifest.ps1 -Version "%APP_VERSION%" -OutputExe "ScaleLogger.exe" -ChecksumFile "SHA256SUMS.txt"
if %errorlevel% neq 0 exit /b %errorlevel%

if exist BUILD_MANIFEST.md move /y BUILD_MANIFEST.md "%MANIFEST_FILE%" >nul
if %errorlevel% neq 0 (
  echo Failed to move manifest into release folder.
  exit /b %errorlevel%
)

echo.
echo Release build complete.
echo Executable: %OUTPUT_EXE%
echo Checksum:   %CHECKSUM_FILE%
echo Manifest:   %MANIFEST_FILE%

endlocal
