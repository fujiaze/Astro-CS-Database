$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
$dirs = @(
  'lib/astro_image_io',
  'lib/calibration',
  'lib/healpix_db/healpix_drizzle',
  'lib/photometric_calib/cpp',
  'lib/snr_estimator/cpp',
  'lib/acr/backends/cuda/bridge',
  'lib/phase2/build'
)
foreach ($tag in @('C','A','B')) {
  if ($tag -eq 'C') { $base = $root } else { $base = Join-Path $wt $tag }
  Write-Output ('=== ' + $tag + ' base=' + $base)
  foreach ($d in $dirs) {
    $p = Join-Path $base $d
    if (Test-Path -LiteralPath $p) {
      $dlls = Get-ChildItem -LiteralPath $p -Filter '*.dll' -ErrorAction SilentlyContinue | ForEach-Object { $_.Name }
      Write-Output ($d + ' => ' + ($dlls -join ', '))
    } else {
      Write-Output ($d + ' => MISSING_DIR')
    }
  }
}
Write-Output 'PROBE_DONE'
