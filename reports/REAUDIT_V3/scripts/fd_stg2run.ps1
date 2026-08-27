$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$dirs = @(
  'C:/msys64/mingw64/bin',
  (Join-Path $root 'lib/phase2/build'),
  (Join-Path $root 'lib/astro_image_io'),
  (Join-Path $root 'lib/calibration'),
  (Join-Path $root 'lib/healpix_db/healpix_drizzle'),
  (Join-Path $root 'lib/photometric_calib/cpp'),
  (Join-Path $root 'lib/snr_estimator/cpp'),
  (Join-Path $root 'lib/acr/backends/cuda/bridge')
)
$env:PATH = ($dirs -join ';') + ';' + $env:PATH
$stg2 = Join-Path $root 'lib/phase2/build/astrocs-stage2.exe'
$cfg = 'C:/Users/fujia/stage2_fatduck.json'
$runlog = Join-Path $root 'run/temp/p2_v7/gc/audit_stage2_f01f11.log'
Set-Location -LiteralPath $root
& $stg2 $cfg 2>&1 | Tee-Object -FilePath $runlog
Write-Output ('FINAL_EXIT=' + $LASTEXITCODE)
