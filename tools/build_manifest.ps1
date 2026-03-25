param(
  [string]$Version = "0.96",
  [string]$OutputExe = "ScaleLogger.exe",
  [string]$ChecksumFile = "SHA256SUMS.txt",
  [string]$ConfigurePreset = "windows-vs2026-x64",
  [string]$BuildPreset = "windows-release",
  [string]$BuildType = "Release",
  [string]$Platform = "x64"
)

function Get-CommandOutput([string]$Command, [string]$Args) {
  try {
    $result = & $Command $Args 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $result) { return "unknown" }
    return ($result | Select-Object -First 1).ToString().Trim()
  } catch {
    return "unknown"
  }
}

$utcNow = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd HH:mm:ss 'UTC'")
$gitBranch = Get-CommandOutput "git" "rev-parse --abbrev-ref HEAD"
$gitCommit = Get-CommandOutput "git" "rev-parse HEAD"
$cmakeVersionLine = Get-CommandOutput "cmake" "--version"
$cmakeVersion = if ($cmakeVersionLine -like "cmake version*") { $cmakeVersionLine -replace "cmake version\s*", "" } else { $cmakeVersionLine }

$generator = "Visual Studio 18 2026"
try {
  $presets = Get-Content -Raw -Path "CMakePresets.json" | ConvertFrom-Json
  $configure = $presets.configurePresets | Where-Object { $_.name -eq $ConfigurePreset } | Select-Object -First 1
  if ($configure -and $configure.generator) { $generator = $configure.generator }
} catch {
}

$msvcVersion = if ($env:VCToolsVersion) { $env:VCToolsVersion } else { "unknown" }
$windowsSdkVersion = if ($env:WindowsSDKVersion) { $env:WindowsSDKVersion.TrimEnd('\\') } else { "unknown" }

$checksumLine = "unknown"
if (Test-Path $ChecksumFile) {
  try {
    $matched = Select-String -Path $ChecksumFile -Pattern [regex]::Escape($OutputExe) | Select-Object -First 1
    if ($matched) { $checksumLine = $matched.Line.Trim() }
  } catch {
  }
}

@"
ScaleLogger Build Manifest
App name: ScaleLogger
Version: $Version
Git branch: $gitBranch
Git commit SHA: $gitCommit
Build date/time (UTC): $utcNow
Build type: $BuildType
CMake version: $cmakeVersion
Generator: $generator
Platform: $Platform
MSVC version/toolset: $msvcVersion
Windows SDK version: $windowsSdkVersion
Configure preset: $ConfigurePreset
Build preset: $BuildPreset
Output executable name: $OutputExe
SHA256 checksum: $checksumLine
"@ | Out-File -Encoding utf8 BUILD_MANIFEST.md
