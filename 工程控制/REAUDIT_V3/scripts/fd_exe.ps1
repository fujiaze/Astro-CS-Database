$ErrorActionPreference = 'Stop'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output '=== launch/ ===';
Get-ChildItem -LiteralPath (Join-Path $root 'launch') -Recurse | Select-Object FullName, Length | Format-Table -AutoSize | Out-String -Width 200
Write-Output '=== .exe files under root (top 30) ===';
Get-ChildItem -LiteralPath $root -Recurse -Include *.exe -ErrorAction SilentlyContinue | Select-Object -First 30 -ExpandProperty FullName
