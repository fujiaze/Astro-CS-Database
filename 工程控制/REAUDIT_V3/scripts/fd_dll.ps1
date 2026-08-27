$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output '=== all ACR-ish DLLs under lib/acr ===';
Get-ChildItem -LiteralPath (Join-Path $root 'lib/acr') -Recurse -Include *.dll -ErrorAction SilentlyContinue | Select-Object -First 20 -ExpandProperty FullName
Write-Output '=== cuda_backend or bridge dll anywhere under lib ===';
Get-ChildItem -LiteralPath (Join-Path $root 'lib') -Recurse -Include *cuda*.dll,*acr*.dll -ErrorAction SilentlyContinue | Select-Object -First 20 -ExpandProperty FullName
