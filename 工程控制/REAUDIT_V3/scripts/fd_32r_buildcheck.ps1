$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
$outdir = Join-Path $root 'run/temp/p2_v7/abc_32r'
foreach ($n in @('A','B')) {
  $wd = Join-Path $wt $n
  $exe = Join-Path $wd 'build_ab/astrocs-stage2.exe'
  "stage2_$n = $(Test-Path -LiteralPath $exe) $(if (Test-Path -LiteralPath $exe) { [math]::Round((Get-Item -LiteralPath $exe).Length/1MB,1) } else { 'NA' })MB"
}
"--- build log tails ---"
foreach ($n in @('A','B')) {
  $log = Join-Path $outdir ('build_' + $n + '.log')
  "== $n log tail =="
  if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Tail 8 } else { 'LOG_MISSING' }
}
