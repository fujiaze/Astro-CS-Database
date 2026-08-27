$ErrorActionPreference = 'Continue'
# fd_32r_stage2.ps1 - run stage2 over the full 32R frame set (panel1 f01-f11 + panel2 f12-f22 + panel3 f23-f32).
$root = 'F:/Astro dev/Astro CS Normalization Database'
$stg2 = Join-Path $root 'lib/phase2/build/astrocs-stage2.exe'
$cfg  = 'C:/Users/fujia/stage2_fatduck32.json'
$runlog = 'C:/Users/fujia/stage2_32f.log'
$PATH = @(
  'C:/msys64/mingw64/bin',
  (Join-Path $root 'lib/phase2/build'),
  (Join-Path $root 'lib/astro_image_io'),
  (Join-Path $root 'lib/calibration'),
  (Join-Path $root 'lib/healpix_db/healpix_drizzle'),
  (Join-Path $root 'lib/photometric_calib/cpp'),
  (Join-Path $root 'lib/snr_estimator/cpp'),
  (Join-Path $root 'lib/acr/backends/cuda/bridge')
) -join ';'
$env:PATH = $PATH + ';' + $env:PATH
Write-Output ('START=' + (Get-Date -Format s) + ' cfg=' + $cfg)
if (-not (Test-Path -LiteralPath $cfg)) { Write-Output 'MISSING_CONFIG'; exit 20 }
& $stg2 $cfg 2>&1 | Tee-Object -FilePath $runlog
$FINAL_EXIT = $LASTEXITCODE
Write-Output ('DONE=' + (Get-Date -Format s) + ' EXIT=' + $FINAL_EXIT)
exit $FINAL_EXIT
