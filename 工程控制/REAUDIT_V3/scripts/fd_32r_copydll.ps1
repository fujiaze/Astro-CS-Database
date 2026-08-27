$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
$pairs = @(
  @('lib/astro_image_io','astro_image_io.dll'),
  @('lib/calibration','astro_calibration.dll'),
  @('lib/calibration','cosmetic_corrector.dll'),
  @('lib/healpix_db/healpix_drizzle','healpix_drizzle.dll'),
  @('lib/photometric_calib/cpp','gaia_client.dll'),
  @('lib/photometric_calib/cpp','photometric_calib.dll'),
  @('lib/snr_estimator/cpp','snr_estimator.dll'),
  @('lib/acr/backends/cuda/bridge','acr_cuda_bridge.dll')
)
foreach ($tag in @('A','B')) {
  $destBase = Join-Path $wt $tag
  Write-Output ('=== copy to ' + $tag + ' ===')
  foreach ($p in $pairs) {
    $srcDir = Join-Path $root $p[0]
    $src = Join-Path $srcDir $p[1]
    $dstDir = Join-Path $destBase $p[0]
    $dst = Join-Path $dstDir $p[1]
    if (Test-Path -LiteralPath $src) {
      New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
      Copy-Item -LiteralPath $src -Destination $dst -Force
      $len = (Get-Item -LiteralPath $dst).Length
      Write-Output ($p[1] + ' OK ' + $len)
    } else {
      Write-Output ($p[1] + ' SRC_MISSING')
    }
  }
}
Write-Output 'COPY_DONE'
