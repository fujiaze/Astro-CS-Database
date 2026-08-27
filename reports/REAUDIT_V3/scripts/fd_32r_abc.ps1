$ErrorActionPreference = 'Continue'
# fd_32r_abc.ps1 - Build stage2/orchestrator at anchors A/B/C and run 32R A/B/C comparison on Fatduck.
# Anchors (from control doc): A=b38b446e63d0d27eac672b85ce30527399a057fc B=83471979a1dd778b4e557a9c7a92e22c137107f3 C=535e73879662346ee1f599d7a9cae96c6c23680d
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt   = Join-Path $root 'run/temp/p2_ab_worktrees'
$out  = Join-Path $root 'run/temp/p2_v7/abc_32r'
New-Item -ItemType Directory -Force -Path $wt,$out | Out-Null
$anchors = @(
  @{name='A'; sha='b38b446e63d0d27eac672b85ce30527399a057fc'},
  @{name='B'; sha='83471979a1dd778b4e557a9c7a92e22c137107f3'},
  @{name='C'; sha='535e73879662346ee1f599d7a9cae96c6c23680d'}
)
$PATH0 = @(
  'C:/msys64/mingw64/bin',
  (Join-Path $root 'lib/phase2/build'),
  (Join-Path $root 'lib/astro_image_io'),
  (Join-Path $root 'lib/calibration'),
  (Join-Path $root 'lib/healpix_db/healpix_drizzle'),
  (Join-Path $root 'lib/photometric_calib/cpp'),
  (Join-Path $root 'lib/snr_estimator/cpp'),
  (Join-Path $root 'lib/acr/backends/cuda/bridge')
) -join ';'
foreach ($a in $anchors) {
  $wd = Join-Path $wt $a.name
  if (-not (Test-Path -LiteralPath (Join-Path $wd '.git'))) {
    Write-Output ('WORKTREE ' + $a.name + ' missing - create with git worktree add (requires git on FATDUCK side).')
    continue
  }
  Write-Output ('BUILD ' + $a.name + ' @ ' + $a.sha)
  # build lib/phase2 + orchestrator (MinGW) - placeholder; actual cmake invoked per repo docs
  $log = Join-Path $out ('build_' + $a.name + '.log')
  Push-Location $wd
  try {
    & cmake -S . -B build_mingw -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release 2>&1 | Tee-Object -FilePath $log
    & cmake --build build_mingw --target astrocs-stage2 orchestrator -j 4 2>&1 | Tee-Object -FilePath $log -Append
  } finally { Pop-Location }
  $stg2 = Join-Path $wd 'build_mingw/lib/phase2/astrocs-stage2.exe'
  Write-Output ('stage2_exists_' + $a.name + '=' + (Test-Path -LiteralPath $stg2))
}
Write-Output 'ABC_BUILD_DONE'
