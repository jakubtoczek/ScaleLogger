param(
  [Parameter(Mandatory=$true)][string]$FilePath,
  [string]$OutFile = "SHA256SUMS.txt"
)
$hash = Get-FileHash -Algorithm SHA256 -Path $FilePath
"$($hash.Hash.ToLower())  $([System.IO.Path]::GetFileName($FilePath))" | Out-File -Encoding ascii -NoNewline $OutFile
Write-Host "Wrote $OutFile"
