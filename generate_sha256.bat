@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
  echo Usage: generate_sha256.bat path\to\ScaleLogger.exe [output\SHA256SUMS.txt]
  exit /b 1
)

set "TARGET_FILE=%~1"
if not exist "%TARGET_FILE%" (
  echo File not found: %TARGET_FILE%
  exit /b 1
)

set "OUTPUT_FILE=%~2"
if "%OUTPUT_FILE%"=="" set "OUTPUT_FILE=%~dp1SHA256SUMS.txt"

set "RAW_HASH="
for /f "skip=1 delims=" %%H in ('certutil -hashfile "%TARGET_FILE%" SHA256 ^| findstr /r /v /c:"hash of file" /c:"CertUtil:"') do (
  if not defined RAW_HASH set "RAW_HASH=%%H"
)

if not defined RAW_HASH (
  echo Failed to extract SHA256 from certutil output.
  exit /b 1
)

set "SHA256_VALUE=%RAW_HASH: =%"
set "OUTPUT_NAME=%~nx1"

> "%OUTPUT_FILE%" echo !SHA256_VALUE!  !OUTPUT_NAME!
if %errorlevel% neq 0 (
  echo Failed to write %OUTPUT_FILE%.
  exit /b %errorlevel%
)

echo Wrote %OUTPUT_FILE%
echo !SHA256_VALUE!  !OUTPUT_NAME!
endlocal
