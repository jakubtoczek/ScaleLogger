@echo off
setlocal EnableExtensions EnableDelayedExpansion

if /I "%1"=="--keep-venv" (
  set KEEP_VENV=1
)

set "APP_VERSION=0.95"
set "RELEASE_DIR=release"
set "OUTPUT_EXE=%RELEASE_DIR%\ScaleLogger.exe"
set "CHECKSUM_FILE=%RELEASE_DIR%\SHA256SUMS.txt"
set "MANIFEST_FILE=%RELEASE_DIR%\BUILD_MANIFEST_%APP_VERSION%.txt"
set "BUILD_SCRIPT=%~nx0"

where py >nul 2>nul
if %errorlevel% neq 0 (
  echo Python launcher 'py' was not found.
  exit /b 1
)

py -3.12-64 -c "import struct; print(struct.calcsize('P') * 8)" >nul 2>nul
if %errorlevel% neq 0 (
  echo Python 3.12 64-bit was not found via 'py -3.12-64'.
  exit /b 1
)

for %%F in (main.py requirements.txt generate_sha256.bat BUILD_MANIFEST_TEMPLATE.md icon.ico) do (
  if not exist "%%F" (
    echo Missing %%F at repository root.
    exit /b 1
  )
)

if defined KEEP_VENV (
  echo Fast build (reusing environment)
) else (
  echo Clean build (fresh environment)
  if exist .venv64 (
    echo Removing existing .venv64 for clean build...
    rmdir /s /q .venv64
  )
)

if not exist .venv64 (
py -3.12-64 -m venv .venv64
if %errorlevel% neq 0 (
  echo Failed to create .venv64.
  exit /b %errorlevel%
)
)

if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"
if %errorlevel% neq 0 (
  echo Failed to create %RELEASE_DIR%.
  exit /b %errorlevel%
)

call .\.venv64\Scripts\activate.bat
python -m pip install --upgrade pip
if %errorlevel% neq 0 exit /b %errorlevel%

python -m pip install -r requirements.txt
if %errorlevel% neq 0 exit /b %errorlevel%

if exist requirements-build.txt (
  python -m pip install -r requirements-build.txt
  if %errorlevel% neq 0 exit /b %errorlevel%
)

python -m compileall main.py app tests
if %errorlevel% neq 0 (
  echo Syntax sanity check failed.
  exit /b %errorlevel%
)

python -m nuitka ^
  --standalone ^
  --onefile ^
  --enable-plugin=pyside6 ^
  --windows-console-mode=disable ^
  --windows-icon-from-ico=icon.ico ^
  --include-data-files=icon.ico=icon.ico ^
  --output-dir=%RELEASE_DIR% ^
  --output-filename=ScaleLogger.exe ^
  --assume-yes-for-downloads ^
  main.py
if %errorlevel% neq 0 (
  echo Release build failed.
  exit /b %errorlevel%
)

if not exist "%OUTPUT_EXE%" (
  echo Expected output not found: %OUTPUT_EXE%
  exit /b 1
)

python -m app.release_support verify "%OUTPUT_EXE%"
if %errorlevel% neq 0 (
  echo Release verification failed. Removing incomplete executable if present.
  del "%OUTPUT_EXE%" >nul 2>nul
  exit /b %errorlevel%
)

call generate_sha256.bat "%OUTPUT_EXE%" "%CHECKSUM_FILE%"
if %errorlevel% neq 0 exit /b %errorlevel%

python -m app.release_support manifest "%MANIFEST_FILE%" "%CHECKSUM_FILE%" "ScaleLogger.exe" "%BUILD_SCRIPT%"
if %errorlevel% neq 0 (
  echo Failed to generate the build manifest.
  exit /b %errorlevel%
)

python -m app.release_support cleanup "%RELEASE_DIR%" > "%RELEASE_DIR%\cleanup_status.txt"
set "CLEANUP_STATUS=%errorlevel%"
type "%RELEASE_DIR%\cleanup_status.txt"
del "%RELEASE_DIR%\cleanup_status.txt" >nul 2>nul
if not "%CLEANUP_STATUS%"=="0" (
  echo Cleanup reported a problem. Review the messages above.
)

echo.
echo Release build complete.
echo Executable: %OUTPUT_EXE%
echo Checksum:   %CHECKSUM_FILE%
echo Manifest:   %MANIFEST_FILE%
echo Unsigned one-file builds may still trigger SmartScreen or Defender reputation checks.
endlocal
