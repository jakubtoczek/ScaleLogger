param(
  [string]$Version = "0.96",
  [string]$OutputExe = "ScaleLogger.exe",
  [string]$ChecksumFile = "SHA256SUMS.txt",
  [string]$ConfigurePreset = "windows-vs2026-x64",
  [string]$BuildPreset = "windows-release",
  [string]$BuildType = "Release",
  [string]$Platform = "x64"
)

function Get-FirstLine {
  param([string[]]$Lines)
  if (-not $Lines -or $Lines.Count -eq 0) { return "unknown" }
  return $Lines[0].ToString().Trim()
}

$utcNow = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd HH:mm:ss 'UTC'")

$gitBranch = "unknown"
$gitCommit = "unknown"
try {
  $gitBranch = Get-FirstLine (& git rev-parse --abbrev-ref HEAD)
  if ($LASTEXITCODE -ne 0) { $gitBranch = "unknown" }
} catch {}
try {
  $gitCommit = Get-FirstLine (& git rev-parse HEAD)
  if ($LASTEXITCODE -ne 0) { $gitCommit = "unknown" }
} catch {}

$cmakeVersion = "unknown"
try {
  $cmakeVersionLine = Get-FirstLine (& cmake --version)
  if ($cmakeVersionLine -like "cmake version*") {
    $cmakeVersion = ($cmakeVersionLine -replace "cmake version\s*", "").Trim()
  }
} catch {}

$generator = "Visual Studio 18 2026"
try {
  $presets = Get-Content -Raw -Path "CMakePresets.json" | ConvertFrom-Json
  $configure = $presets.configurePresets | Where-Object { $_.name -eq $ConfigurePreset } | Select-Object -First 1
  if ($configure -and $configure.generator) { $generator = $configure.generator }
} catch {}

$msvcVersion = if ($env:VCToolsVersion) { $env:VCToolsVersion } else { "unknown" }
$windowsSdkVersion = if ($env:WindowsSDKVersion) { $env:WindowsSDKVersion.TrimEnd('\\') } else { "unknown" }

$checksumValue = "unknown"
if (Test-Path $ChecksumFile) {
  try {
    $matched = Select-String -Path $ChecksumFile -Pattern [regex]::Escape($OutputExe) | Select-Object -First 1
    if ($matched) {
      $parts = $matched.Line.Trim() -split '\s+'
      if ($parts.Count -gt 0 -and $parts[0]) { $checksumValue = $parts[0] }
    }
  } catch {}
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
SHA256 checksum: $checksumValue
"@ | Out-File -Encoding utf8 BUILD_MANIFEST.md
