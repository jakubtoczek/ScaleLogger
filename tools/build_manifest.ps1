param(
  [string]$Version = "",
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

function Get-VersionFromCMake {
  if (-not (Test-Path "CMakeLists.txt")) { return "unknown" }
  try {
    $cmakeText = Get-Content -Raw -Path "CMakeLists.txt"
    $m = [regex]::Match($cmakeText, 'project\([^\)]*VERSION\s+([0-9]+(?:\.[0-9]+){1,3})', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if ($m.Success) { return $m.Groups[1].Value }
  } catch {}
  return "unknown"
}

$utcNow = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd HH:mm:ss 'UTC'")
if ([string]::IsNullOrWhiteSpace($Version)) {
  $Version = Get-VersionFromCMake
}

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
$defaultConfigDetail = @()
if (Test-Path "default_config.json") {
  try {
    $defaultConfigSha256 = (Get-FileHash -Algorithm SHA256 "default_config.json").Hash.ToLowerInvariant()
    $cfg = Get-Content -Raw -Path "default_config.json" | ConvertFrom-Json
    $defaultConfigSummary = "connect_on_startup=$($cfg.connect_on_startup); log_mode=$($cfg.log_mode); config_file_name=$($cfg.config_file_name); enable_startup_trace=$($cfg.enable_startup_trace); enable_fatal_log_file=$($cfg.enable_fatal_log_file); show_crash_dialog=$($cfg.show_crash_dialog); include_trace_in_crash_dialog=$($cfg.include_trace_in_crash_dialog)"
    $defaultConfigDetail = @(
      "Default config values:",
      "  Application:",
      "    config_folder=$($cfg.config_folder)",
      "    config_file_name=$($cfg.config_file_name)",
      "    presets_folder=$($cfg.presets_folder)",
      "    logs_folder=$($cfg.logs_folder)",
      "    log_file_pattern=$($cfg.log_file_pattern)",
      "    log_mode=$($cfg.log_mode)",
      "    line_log_mode=$($cfg.line_log_mode)",
      "    connect_on_startup=$($cfg.connect_on_startup)",
      "    dark_mode=$($cfg.dark_mode)",
      "    debug_combo_logging=$($cfg.debug_combo_logging)",
      "  Diagnostics:",
      "    enable_startup_trace=$($cfg.enable_startup_trace)",
      "    enable_fatal_log_file=$($cfg.enable_fatal_log_file)",
      "    show_crash_dialog=$($cfg.show_crash_dialog)",
      "    include_trace_in_crash_dialog=$($cfg.include_trace_in_crash_dialog)",
      "  Runtime combo option arrays:",
      "    baud_rates=$([string]::Join(',', $cfg.baud_rates))",
      "    data_bits_options=$([string]::Join(',', $cfg.data_bits_options))",
      "    parity_options=$([string]::Join(',', $cfg.parity_options))",
      "    stop_bits_options=$([string]::Join(',', $cfg.stop_bits_options))",
      "  Serial:",
      "    port=$($cfg.port)",
      "    baudrate=$($cfg.baudrate)",
      "    databits=$($cfg.databits)",
      "    parity=$($cfg.parity)",
      "    stopbits=$($cfg.stopbits)",
      "    timeout=$($cfg.timeout)",
      "    eol=$($cfg.eol)",
      "  Parsing:",
      "    mode=$($cfg.mode)",
      "    trim_whitespace=$($cfg.trim_whitespace)",
      "    strip_suffix=$($cfg.strip_suffix)",
      "    suffix=$($cfg.suffix)",
      "    normalize_sign=$($cfg.normalize_sign)",
      "    preserve_plus_sign=$($cfg.preserve_plus_sign)",
      "    preserve_minus_sign=$($cfg.preserve_minus_sign)",
      "    numeric_validation=$($cfg.numeric_validation)",
      "  Output:",
      "    post_action=$($cfg.post_action)",
      "    custom_sequence=$([string]::Join(',', $cfg.custom_sequence))"
    )
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
$($defaultConfigDetail -join "`n")
"@ | Out-File -Encoding utf8 BUILD_MANIFEST.md
