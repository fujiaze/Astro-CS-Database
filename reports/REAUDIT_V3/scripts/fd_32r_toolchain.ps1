$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
"--- msys64 cmake/make ---"
foreach ($p in @('C:/msys64/mingw64/bin/cmake.exe','C:/msys64/usr/bin/cmake.exe','C:/msys64/mingw64/bin/make.exe','C:/msys64/usr/bin/make.exe','C:/msys64/mingw64/bin/g++.exe','C:/msys64/mingw64/bin/gcc.exe')) { "$p = $(Test-Path -LiteralPath $p)" }
"--- existing phase2 build dir ---"
$b = Join-Path $root 'lib/phase2/build'
if (Test-Path -LiteralPath $b) { Get-ChildItem -LiteralPath $b -Filter '*.exe' | ForEach-Object { "exe=$($_.Name) $([math]::Round($_.Length/1MB,1))MB" }; Get-ChildItem -LiteralPath $b -Filter 'CMakeCache.txt' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 2 | ForEach-Object { "cache=$($_.FullName)" }; Get-ChildItem -LiteralPath $b -Filter '*.cmake' -ErrorAction SilentlyContinue | Select-Object -First 3 | ForEach-Object { "cmakefile=$($_.Name)" } }
"--- repo root build scripts ---"
Get-ChildItem -LiteralPath $root -Filter '*.sh' -ErrorAction SilentlyContinue | Select-Object -First 10 | ForEach-Object { "sh=$($_.Name)" }
Get-ChildItem -LiteralPath $root -Filter '*.ps1' -ErrorAction SilentlyContinue | Select-Object -First 10 | ForEach-Object { "ps1=$($_.Name)" }
"--- build dirs any depth (cmake presets) ---"
Get-ChildItem -LiteralPath $root -Recurse -Depth 2 -Filter 'CMakeCache.txt' -ErrorAction SilentlyContinue | Select-Object -First 5 | ForEach-Object { "cache2=$($_.FullName)" }
"--- CMakePresets/CMakeLists at root ---"
"presets=$(Test-Path -LiteralPath (Join-Path $root 'CMakePresets.json'))"
"cmakelists=$(Test-Path -LiteralPath (Join-Path $root 'CMakeLists.txt'))"
DONE
