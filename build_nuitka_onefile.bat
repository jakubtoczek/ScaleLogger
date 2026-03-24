@echo off
setlocal

REM ScaleLogger - Python 3.12 64-bit one-file build
REM Result: a self-contained 64-bit ScaleLogger.exe; Python is not needed on the target machine.

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

if not exist main.py (
  echo Missing main.py at repository root.
  exit /b 1
)

if not exist requirements.txt (
  echo Missing requirements.txt at repository root.
  exit /b 1
)

if not exist icon.ico (
  echo Missing icon.ico at repository root. Add it manually before building.
  exit /b 1
)

if exist .venv64 rmdir /s /q .venv64
py -3.12-64 -m venv .venv64
if %errorlevel% neq 0 (
  echo Failed to create .venv64.
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
  --output-filename=ScaleLogger.exe ^
  --assume-yes-for-downloads ^
  main.py

if %errorlevel% neq 0 (
  echo One-file build failed.
  exit /b %errorlevel%
)

echo One-file build complete. ScaleLogger.exe is a self-contained 64-bit executable.
echo Use generate_sha256.bat on the finished executable before sharing it.
endlocal
