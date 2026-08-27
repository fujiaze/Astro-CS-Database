$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
"--- all dll under root (depth-limited) ---"
Get-ChildItem -LiteralPath $root -Recurse -Filter '*.dll' -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch 'run\\temp|build_ab|node_modules|\.git' } | Select-Object -First 40 | ForEach-Object { $_.FullName }
"--- phase2 CMakeLists external refs ---"
Get-Content -LiteralPath (Join-Path $root 'lib/phase2/CMakeLists.txt') | Select-String -Pattern 'dll|imported|add_custom_command|copy' | Select-Object -First 30 | ForEach-Object { $_.Line }
"--- astro_image_io dir ---"
$ai = Join-Path $root 'lib/astro_image_io'
Get-ChildItem -LiteralPath $ai -ErrorAction SilentlyContinue | Select-Object -First 20 | ForEach-Object { "  $($_.Name) $(if ($_.PSIsContainer) {'<dir>'} else {[math]::Round($_.Length/1MB,1).ToString()+'MB'})" }
