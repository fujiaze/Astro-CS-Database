$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
foreach ($tag in @('A','B')) {
  $base = Join-Path $wt $tag
  $bd = Join-Path $base 'build_ab'
  Write-Output ('=== ' + $tag + ' build_ab ===')
  if (Test-Path -LiteralPath $bd) {
    Get-ChildItem -LiteralPath $bd | ForEach-Object { Write-Output ($_.Name + '  ' + $_.Length) }
  } else { Write-Output 'MISSING build_ab' }
}
Write-Output '=== OBJDUMP A ==='
& 'C:/msys64/usr/bin/objdump.exe' -p (Join-Path $wt 'A/build_ab/astrocs-stage2.exe') 2>&1 | Select-String -Pattern 'DLL Name' | ForEach-Object { $_.Line.Trim() }
Write-Output '=== OBJDUMP B ==='
& 'C:/msys64/usr/bin/objdump.exe' -p (Join-Path $wt 'B/build_ab/astrocs-stage2.exe') 2>&1 | Select-String -Pattern 'DLL Name' | ForEach-Object { $_.Line.Trim() }
Write-Output 'PROBE2_DONE'
