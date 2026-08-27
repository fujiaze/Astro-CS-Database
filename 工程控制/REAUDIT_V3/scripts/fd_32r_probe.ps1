$ErrorActionPreference = 'Continue'
# fd_32r_probe.ps1 - First run when Fatduck is online: inventory the 32R puzzle.
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output ('TIME=' + (Get-Date -Format s))
Write-Output '--- A. Stage1 configs (panel) ---'
Get-ChildItem -LiteralPath (Join-Path $root 'lib/orchestrator/configs') -Filter 'stage1_gc_*.json' -Name 2>$null
Write-Output '--- B. T4 Red FITS per panel (first 20 names each) ---'
foreach ($p in 1,2,3) {
  $d = Join-Path $root ('testdata/Galaxy_Center_T4/lights/panel' + $p)
  $fs = @(Get-ChildItem -LiteralPath $d -Filter '*-Red.fts' -Name -ErrorAction SilentlyContinue)
  Write-Output ('panel' + $p + ' count=' + $fs.Count)
  $fs | Select-Object -First 20 | ForEach-Object { Write-Output ('  ' + $_) }
}
Write-Output '--- C. Existing per-frame HiPS (gc_R_*) ---'
$gc = Join-Path $root 'run/temp/p2_v7/gc'
$h = @(Get-ChildItem -LiteralPath $gc -Filter 'gc_R_*.hips' -Directory -ErrorAction SilentlyContinue)
Write-Output ('gc_R_hips count=' + $h.Count)
$h | ForEach-Object { Write-Output ('  ' + $_.Name) } | Select-Object -First 60
Write-Output '--- D. Mosaics already produced ---'
Get-ChildItem -LiteralPath $gc -Filter 'audit_stage2_*.mosaic.hips' -Directory -Name -ErrorAction SilentlyContinue
Write-Output '--- E. stage2 exe + DLLs present ---'
Write-Output ('stage2_exe=' + (Test-Path -LiteralPath (Join-Path $root 'lib/phase2/build/astrocs-stage2.exe')))
Write-Output ('cuda_bridge=' + (Test-Path -LiteralPath (Join-Path $root 'lib/acr/backends/cuda/bridge/acr_cuda_bridge.dll')))
Write-Output '--- F. free disk on F: ---'
Get-PSDrive F -ErrorAction SilentlyContinue | ForEach-Object { Write-Output ('F: free_GB=' + [math]::Round($_.Free/1GB,1) + ' used_GB=' + [math]::Round($_.Used/1GB,1)) }
Write-Output 'PROBE_DONE'
