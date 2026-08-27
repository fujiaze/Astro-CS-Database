$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
"--- A build dir contents (top) ---"
Get-ChildItem -LiteralPath (Join-Path $wt 'A/build_ab') -ErrorAction SilentlyContinue | Select-Object -First 30 | ForEach-Object { "  $($_.Name) $(if ($_.PSIsContainer) {'<dir>'} else {[math]::Round($_.Length/1MB,2).ToString()+'MB'})" }
"--- A/B exe dir DLL check ---"
foreach ($n in @('A','B')) {
  $bd = Join-Path $wt ($n + '/build_ab')
  $dlls = Get-ChildItem -LiteralPath $bd -Filter '*.dll' -ErrorAction SilentlyContinue | ForEach-Object { $_.Name }
  "$n dlls: $($dlls -join ', ')"
  $exe = Join-Path $bd 'astrocs-stage2.exe'
  if (Test-Path -LiteralPath $exe) { "$n exe OK $([math]::Round((Get-Item -LiteralPath $exe).Length/1MB,2))MB" } else { "$n exe MISSING" }
}
"--- stage2 run config (32f) ---"
$cfg = Join-Path $root 'run/temp/p2_v7/gc/stage2_fatduck32.json'
if (Test-Path -LiteralPath $cfg) { Get-Content -LiteralPath $cfg -Raw } else { "CFG MISSING: $cfg" }
"--- stage2 main-tree run dir (how C was run) ---"
$gc = Join-Path $root 'run/temp/p2_v7/gc'
Get-ChildItem -LiteralPath $gc -ErrorAction SilentlyContinue | Select-Object -First 40 | ForEach-Object { "  $($_.Name) $(if ($_.PSIsContainer) {'<dir>'} else {[math]::Round($_.Length/1MB,2).ToString()+'MB'})" }
"--- 32f hips dir (input frames) ---"
$hips = Join-Path $root 'run/temp/p2_v7/gc/audit_32f_hips'
if (Test-Path -LiteralPath $hips) { $c = (Get-ChildItem -LiteralPath $hips -Directory).Count; "hips_dir_count=$c" } else { "HIPS DIR MISSING: $hips" }
Get-ChildItem -LiteralPath $gc -Recurse -Filter '*.hips' -ErrorAction SilentlyContinue | Select-Object -First 15 | ForEach-Object { $_.FullName }
"--- stage2 CLI help ---"
$exeA = Join-Path $wt 'A/build_ab/astrocs-stage2.exe'
if (Test-Path -LiteralPath $exeA) { & $exeA --help 2>&1 | Select-Object -First 30 } else { "A exe missing" }
