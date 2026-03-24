param(
  [string]$Version = "0.95",
  [string]$OutputExe = "ScaleLogger.exe",
  [string]$ChecksumFile = "SHA256SUMS.txt"
)
@"
ScaleLogger Build Manifest
Version: $Version
Output filename: $OutputExe
SHA256 file: $ChecksumFile
"@ | Out-File -Encoding utf8 BUILD_MANIFEST.md
