$ErrorActionPreference = 'Continue'
$base = 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc'
$names = @('audit_stage2_5f.mosaic.hips','audit_stage2_26f.mosaic.hips','audit_stage2_32f.mosaic.hips')
foreach ($n in $names) {
  $dj = Join-Path $base ($n + '/diagnostics.json')
  if (Test-Path $dj) {
    "=== $n ==="
    $d = Get-Content $dj -Raw | ConvertFrom-Json
    "model_hash=$($d.model_hash)"
    "controls_with_depth_ge_2=$($d.controls_with_depth_ge_2)"
    "observations=$($d.observations)"
    "runtime_seconds=$($d.runtime_seconds)"
    "upm_hash=$($d.upm_hash)"
  } else { "MISSING $dj" }
}
