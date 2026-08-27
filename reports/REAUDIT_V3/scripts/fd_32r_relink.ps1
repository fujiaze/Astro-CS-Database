$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
$outdir = Join-Path $root 'run/temp/p2_v7/abc_32r'
New-Item -ItemType Directory -Force -Path $outdir | Out-Null
$env:Path = 'C:/msys64/mingw64/bin;C:/msys64/usr/bin;' + $env:Path
# Source DLLs from main worktree C
$src_aio = Join-Path $root 'lib/astro_image_io/astro_image_io.dll'
$src_cuda = Join-Path $root 'lib/acr/backends/cuda/bridge/acr_cuda_bridge.dll'
foreach ($n in @('A','B')) {
  $wd = Join-Path $wt $n
  $dst_aio = Join-Path $wd 'lib/astro_image_io/astro_image_io.dll'
  $dst_cuda = Join-Path $wd 'lib/acr/backends/cuda/bridge/acr_cuda_bridge.dll'
  New-Item -ItemType Directory -Force -Path (Split-Path $dst_aio) | Out-Null
  New-Item -ItemType Directory -Force -Path (Split-Path $dst_cuda) | Out-Null
  Copy-Item -LiteralPath $src_aio -Destination $dst_aio -Force
  Copy-Item -LiteralPath $src_cuda -Destination $dst_cuda -Force
  "COPIED $n aio=$(Test-Path -LiteralPath $dst_aio) cuda=$(Test-Path -LiteralPath $dst_cuda)"
  # relink (build dir already configured + phase2.a built)
  $bd = Join-Path $wd 'build_ab'
  $log = Join-Path $outdir ('relink_' + $n + '.log')
  Push-Location $bd
  try {
    & cmake --build $bd --target astrocs-stage2 -j 4 2>&1 | Tee-Object -FilePath $log
  } finally { Pop-Location }
  $exe = Join-Path $bd 'astrocs-stage2.exe'
  if (Test-Path -LiteralPath $exe) { "stage2_$n = OK $([math]::Round((Get-Item -LiteralPath $exe).Length/1MB,1))MB" } else { "stage2_$n = FAIL" }
}
"RELINK_ALL_DONE"
