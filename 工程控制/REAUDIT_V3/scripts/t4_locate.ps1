$ErrorActionPreference = 'SilentlyContinue'
Write-Host "=== PSVERSION ==="
$PSVersionTable.PSVersion.ToString()
Write-Host "=== DRIVES ==="
Get-PSDrive -PSProvider FileSystem | ForEach-Object { Write-Host ("DRIVE " + $_.Name + " " + $_.Root) }
Write-Host "=== LOCATE Galaxy_Center_T4 ==="
foreach ($r in @('C:\', 'D:\', 'E:\', 'F:\')) {
  if (Test-Path $r) {
    $hits = Get-ChildItem -Path $r -Directory -Recurse -Depth 5 -Filter 'Galaxy_Center_T4' -ErrorAction SilentlyContinue
    foreach ($h in $hits) { Write-Host ("FOUND_DIR " + $h.FullName) }
  }
}
Write-Host "=== DONE ==="
