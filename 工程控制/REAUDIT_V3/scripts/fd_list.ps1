$ErrorActionPreference = 'Stop'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output ('ROOT_EXISTS=' + (Test-Path -LiteralPath $root))
if (Test-Path -LiteralPath $root) {
  Get-ChildItem -LiteralPath $root -Name | Select-Object -First 25
}
