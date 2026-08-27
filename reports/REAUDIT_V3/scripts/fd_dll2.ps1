$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output '=== candidate module DLLs (latest per module) ===';
$dlls = Get-ChildItem -LiteralPath (Join-Path $root 'lib') -Recurse -Filter *.dll -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch 'build2|_deps|archive' }
$dlls | Select-Object -First 40 -ExpandProperty FullName
