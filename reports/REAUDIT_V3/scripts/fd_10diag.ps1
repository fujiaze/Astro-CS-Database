$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$out = Join-Path $root 'run/temp/p2_v7/gc/audit_stage2_10f.mosaic.hips'
python3 -c "import json;d=json.load(open(r'$out/diagnostics.json'));print('diag keys=',len(d));[print(k,'=',d[k]) for k in ['model_hash','controls_with_depth_ge_2','observations','runtime_seconds','input_frames','integrated_pixels','fallback_pixels'] if k in d]" 2>&1 | Select-Object -First 8
if (Test-Path -LiteralPath (Join-Path $out 'upm_sparse.json')) {
  python3 -c "import json;d=json.load(open(r'$out/upm_sparse.json'));print('model: controls=',d.get('control_count'),'obs=',d.get('observation_count'),'frames=',d.get('frames'))" 2>&1 | Select-Object -First 3
}
