$ErrorActionPreference = 'Continue'
$base = 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc'
$missing = @()
for ($f = 1; $f -le 32; $f++) {
  $panel = if ($f -le 11) { 1 } elseif ($f -le 22) { 2 } else { 3 }
  $name = "gc_R_panel${panel}_f$( '{0:D2}' -f $f ).hips"
  $p = Join-Path $base $name
  if (Test-Path $p) { "EXIST $name" } else { $missing += $name; "MISSING $name" }
}
"TOTAL_MISSING=$($missing.Count)"
if ($missing.Count -gt 0) { $missing }
$cfg = 'C:/Users/fujia/stage2_fatduck32.json'
"CFG_EXISTS=$(Test-Path $cfg)"
