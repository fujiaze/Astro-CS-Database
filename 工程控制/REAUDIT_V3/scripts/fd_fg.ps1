$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$dirs = @(
  'C:/msys64/mingw64/bin',
  (Join-Path $root 'lib/astro_image_io'),
  (Join-Path $root 'lib/calibration'),
  (Join-Path $root 'lib/dynamic_psf'),
  (Join-Path $root 'lib/gaia_xpsd_client'),
  (Join-Path $root 'lib/healpix_db/healpix_drizzle'),
  (Join-Path $root 'lib/photometric_calib/cpp'),
  (Join-Path $root 'lib/plate_solve/cpp/ipv'),
  (Join-Path $root 'lib/snr_estimator/cpp'),
  (Join-Path $root 'lib/star_detector'),
  (Join-Path $root 'lib/acr/backends/cuda/bridge'),
  (Join-Path $root 'lib/orchestrator/cpp')
)
$env:PATH = ($dirs -join ';') + ';' + $env:PATH
$orch = Join-Path $root 'lib/orchestrator/cpp/orchestrator.exe'
$cfg = Join-Path $root 'lib/orchestrator/configs/stage1_gc_panel1_Red.json'
Set-Location -LiteralPath $root
Write-Output '=== FOREGROUND run (captures everything) ===';
& $orch $cfg 2>&1 | Select-Object -First 40
Write-Output ('EXIT=' + $LASTEXITCODE)
