$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/temp/p2_ab_worktrees'
$stg2 = Join-Path $wt 'A/build_ab/astrocs-stage2.exe'
$cfg  = 'C:/Users/fujia/stage2_fatduck32_A.json'
$log  = 'C:/Users/fujia/stage2_32f_A_smoke.log'
$PATH = @('C:/msys64/mingw64/bin', (Join-Path $wt 'A/build_ab'), (Join-Path $wt 'A/lib/astro_image_io'), (Join-Path $wt 'A/lib/calibration'), (Join-Path $wt 'A/lib/healpix_db/healpix_drizzle'), (Join-Path $wt 'A/lib/photometric_calib/cpp'), (Join-Path $wt 'A/lib/snr_estimator/cpp'), (Join-Path $wt 'A/lib/acr/backends/cuda/bridge')) -join ';'
$env:PATH = $PATH + ';' + $env:PATH
Write-Output ('SMOKE_START=' + (Get-Date -Format s))
$p = Start-Process -FilePath $stg2 -ArgumentList $cfg -RedirectStandardOutput $log -RedirectStandardError ($log + '.err') -PassThru -NoNewWindow
Start-Sleep -Seconds 8
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; Write-Output 'KILLED_AFTER_8S' } else { Write-Output ('EXITED_EARLY code=' + $p.ExitCode) }
Start-Sleep -Seconds 1
if (Test-Path -LiteralPath $log) { Write-Output '--- STDOUT(head 25) ---'; Get-Content -LiteralPath $log -TotalCount 25 }
if (Test-Path -LiteralPath ($log + '.err')) { Write-Output '--- STDERR(head 25) ---'; Get-Content -LiteralPath ($log + '.err') -TotalCount 25 }
Write-Output 'SMOKE_DONE'
