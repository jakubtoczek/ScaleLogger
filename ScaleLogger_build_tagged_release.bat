@echo off
setlocal enabledelayedexpansion

echo [STEP] ScaleLogger tagged release build starting...

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmm"') do set BUILD_TAG=%%I
if not defined BUILD_TAG (
  echo [ERROR] Failed to create build tag.
  exit /b 1
)

echo [INFO] Build tag: %BUILD_TAG%

set ROOT_DIR=%~dp0
if "%ROOT_DIR:~-1%"=="\" set ROOT_DIR=%ROOT_DIR:~0,-1%
set BUILD_DIR=%ROOT_DIR%\out\build\tagged-release\%BUILD_TAG%
set RELEASE_DIR=%ROOT_DIR%\release
set TARGET_NAME=ScaleLogger
set EXE_PATH=%BUILD_DIR%\Release\%TARGET_NAME%.exe
set OUT_EXE=%RELEASE_DIR%\%TARGET_NAME%_%BUILD_TAG%.exe
set MANIFEST=%RELEASE_DIR%\BUILD_MANIFEST_%BUILD_TAG%.txt
set README_RELEASE=%RELEASE_DIR%\README.txt
set SHA_FILE=%RELEASE_DIR%\SHA256SUMS.txt

for /f %%I in ('powershell -NoProfile -Command "(Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')"') do set BUILD_TIME_UTC=%%I
if not defined BUILD_TIME_UTC set BUILD_TIME_UTC=unknown

set BRANCH_NAME=unknown
set COMMIT_HASH=unknown
where git >nul 2>&1
if %errorlevel%==0 (
  for /f "delims=" %%I in ('git -C "%ROOT_DIR%" rev-parse --abbrev-ref HEAD 2^>nul') do set BRANCH_NAME=%%I
  for /f "delims=" %%I in ('git -C "%ROOT_DIR%" rev-parse HEAD 2^>nul') do set COMMIT_HASH=%%I
)

echo [STEP] Preparing directories...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%" || exit /b 1
if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%" || exit /b 1

echo [STEP] Configuring CMake (Release)...
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 || exit /b 1

echo [STEP] Building CMake target (Release)...
cmake --build "%BUILD_DIR%" --config Release || exit /b 1

if not exist "%EXE_PATH%" (
  echo [ERROR] Expected output not found: "%EXE_PATH%"
  exit /b 1
)

echo [STEP] Collecting release artifacts...
copy /Y "%EXE_PATH%" "%OUT_EXE%" >nul || exit /b 1
copy /Y "%ROOT_DIR%\default_config.json" "%RELEASE_DIR%\default_config.json" >nul || exit /b 1

(
  echo ScaleLogger tagged release package
  echo.
  echo Executable: %TARGET_NAME%_%BUILD_TAG%.exe
  echo Config: default_config.json
  echo.
  echo Notes:
  echo - Place files in the same folder.
  echo - Run the executable directly.
  echo - Edit ScaleLogger.config.json in %%USERPROFILE%%\ScaleLogger after first run.
) > "%README_RELEASE%"

echo [STEP] Writing build manifest...
(
  echo ScaleLogger Build Manifest
  echo App name: ScaleLogger
  echo Build tag: %BUILD_TAG%
  echo Build time ^(UTC^): %BUILD_TIME_UTC%
  echo Git branch: %BRANCH_NAME%
  echo Git commit: %COMMIT_HASH%
  echo CMake generator: Visual Studio 17 2022
  echo Platform: x64
  echo Configuration: Release
  echo Output executable: %TARGET_NAME%_%BUILD_TAG%.exe
  echo Included config: default_config.json
) > "%MANIFEST%"

echo [STEP] Computing SHA256SUMS...
powershell -NoProfile -Command "Get-FileHash -Algorithm SHA256 '%OUT_EXE%' | ForEach-Object { '{0}  {1}' -f $_.Hash.ToLower(), [System.IO.Path]::GetFileName($_.Path) }" > "%SHA_FILE%" || exit /b 1
powershell -NoProfile -Command "Get-FileHash -Algorithm SHA256 '%RELEASE_DIR%\default_config.json' | ForEach-Object { '{0}  {1}' -f $_.Hash.ToLower(), [System.IO.Path]::GetFileName($_.Path) }" >> "%SHA_FILE%" || exit /b 1
powershell -NoProfile -Command "Get-FileHash -Algorithm SHA256 '%MANIFEST%' | ForEach-Object { '{0}  {1}' -f $_.Hash.ToLower(), [System.IO.Path]::GetFileName($_.Path) }" >> "%SHA_FILE%" || exit /b 1
powershell -NoProfile -Command "Get-FileHash -Algorithm SHA256 '%README_RELEASE%' | ForEach-Object { '{0}  {1}' -f $_.Hash.ToLower(), [System.IO.Path]::GetFileName($_.Path) }" >> "%SHA_FILE%" || exit /b 1

echo [STEP] Cleaning temporary build tree...
if exist "%BUILD_DIR%" (
  rmdir /S /Q "%BUILD_DIR%"
  if exist "%BUILD_DIR%" (
    echo [WARN] Could not remove temporary build directory: "%BUILD_DIR%"
  ) else (
    echo [INFO] Removed temporary build directory: "%BUILD_DIR%"
  )
) else (
  echo [INFO] Temporary build directory not found, cleanup skipped.
)

for %%D in ("%ROOT_DIR%\\out\\build\\tagged-release" "%ROOT_DIR%\\out\\build" "%ROOT_DIR%\\out") do (
  if exist "%%~fD" (
    dir /B "%%~fD" >nul 2>&1
    if errorlevel 1 (
      rmdir "%%~fD" >nul 2>&1
    )
  )
)

echo [SUCCESS] Tagged release build complete.
echo [SUCCESS] Release folder: "%RELEASE_DIR%"
exit /b 0
