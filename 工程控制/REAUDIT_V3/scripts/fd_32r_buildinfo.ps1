$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
"===== toolchain.ps1 ===="
Get-Content -LiteralPath (Join-Path $root 'toolchain.ps1') -ErrorAction SilentlyContinue
"===== CMakeCache.txt key lines ===="
$cache = Join-Path $root 'lib/phase2/build/CMakeCache.txt'
if (Test-Path -LiteralPath $cache) { Get-Content -LiteralPath $cache | Select-String -Pattern 'CMAKE_GENERATOR|CMAKE_CXX_COMPILER:|CMAKE_C_COMPILER:|CMAKE_BUILD_TYPE|CMAKE_MAKE_PROGRAM' | Select-Object -First 20 | ForEach-Object { $_.Line } }
"===== phase2 CMakeLists targets ===="
Get-Content -LiteralPath (Join-Path $root 'lib/phase2/CMakeLists.txt') -ErrorAction SilentlyContinue | Select-String -Pattern 'add_executable|add_library|project\(' | Select-Object -First 20 | ForEach-Object { $_.Line }
"===== build dir structure ===="
Get-ChildItem -LiteralPath (Join-Path $root 'lib/phase2/build') -ErrorAction SilentlyContinue | Select-Object -First 15 | ForEach-Object { "  $($_.Name) $(if ($_.PSIsContainer) {'<dir>'} else {'<file>'})" }
