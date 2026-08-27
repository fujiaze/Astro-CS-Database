$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$p2 = Join-Path $root 'lib/phase2/build'
Write-Output '=== CMakeCache generator + compiler ===';
Select-String -LiteralPath (Join-Path $p2 'CMakeCache.txt') -Pattern 'CMAKE_GENERATOR:|CMAKE_CXX_COMPILER:|CMAKE_MAKE_PROGRAM' | Select-Object -First 6 | ForEach-Object { $_.Line }
