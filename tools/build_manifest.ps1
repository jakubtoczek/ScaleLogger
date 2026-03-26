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
$gitUpstream = "unknown"
try {
  $gitBranch = Get-FirstLine (& git rev-parse --abbrev-ref HEAD)
  if ($LASTEXITCODE -ne 0) { $gitBranch = "unknown" }
} catch {}
try {
  $gitCommit = Get-FirstLine (& git rev-parse HEAD)
  if ($LASTEXITCODE -ne 0) { $gitCommit = "unknown" }
} catch {}
try {
  $gitUpstream = Get-FirstLine (& git rev-parse --abbrev-ref --symbolic-full-name "@{u}")
  if ($LASTEXITCODE -ne 0) { $gitUpstream = "unknown" }
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
    $line = Get-FirstLine (Get-Content -Path $ChecksumFile)
    if ($line -ne "unknown") {
      $parts = $line.Trim() -split '\s+'
      if ($parts.Count -gt 0 -and $parts[0]) { $checksumValue = $parts[0] }
    }
  } catch {}
}
$defaultConfigSource = if (Test-Path "default_config.json") { "file: default_config.json" } else { "embedded defaults" }
$defaultConfigSha256 = "n/a"
$defaultConfigSummary = "n/a"
if (Test-Path "default_config.json") {
  try {
    $defaultConfigSha256 = (Get-FileHash -Algorithm SHA256 "default_config.json").Hash.ToLowerInvariant()
    $cfg = Get-Content -Raw -Path "default_config.json" | ConvertFrom-Json
    $startupPresetState = if ([string]::IsNullOrWhiteSpace([string]$cfg.startup_preset_name)) { "empty" } else { "set" }
    $lastUsedPresetState = if ([string]::IsNullOrWhiteSpace([string]$cfg.last_used_preset_name)) { "empty" } else { "set" }
    $defaultConfigSummary = "connect_on_startup=$($cfg.connect_on_startup); startup_mode=$($cfg.startup_mode); log_mode=$($cfg.log_mode); line_log_mode=$($cfg.line_log_mode); baudrate=$($cfg.baudrate); parity=$($cfg.parity); stopbits=$($cfg.stopbits); output_action=$($cfg.post_action); config_file_name=$($cfg.config_file_name); startup_preset_name=$startupPresetState; last_used_preset_name=$lastUsedPresetState"
  } catch {}
}

@"
ScaleLogger Build Manifest
App name: ScaleLogger
Version: $Version
Git branch: $gitBranch
Git upstream branch: $gitUpstream
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
Default config source: $defaultConfigSource
Default config SHA256: $defaultConfigSha256
Default config summary: $defaultConfigSummary
"@ | Out-File -Encoding utf8 BUILD_MANIFEST.md
