$ErrorActionPreference = 'Continue'
Write-Host "SMOKE_A2_START $(Get-Date -Format o)"
# A worktree exe
$exe = 'C:/Users/fujia/run/temp/p2_ab_worktrees/A/build_ab/astrocs-stage2.exe'
Write-Host "EXE_EXISTS $(Test-Path $exe)"
$cfg = 'C:/Users/fujia/stage2_fatduck32_A.json'
Write-Host "CFG_EXISTS $(Test-Path $cfg)"
if (Test-Path $cfg) {
  $j = Get-Content $cfg -Raw | ConvertFrom-Json
  Write-Host "CFG_OUTPUT $($j.output.hips)"
  Write-Host "CFG_REJ_METHOD $($j.integration.rejection.method)"
  Write-Host "CFG_REJ_LOW $($j.integration.rejection.low)"
  Write-Host "CFG_REJ_HIGH $($j.integration.rejection.high)"
}
# PATH injection same as 32R run
$env:PATH = 'C:/msys64/mingw64/bin;C:/Users/fujia/run/temp/p2_ab_worktrees/A/build_ab;C:/Users/fujia/run/temp/p2_ab_worktrees/A/lib/astro_image_io;C:/Users/fujia/run/temp/p2_ab_worktrees/A/lib/calibration;C:/Users/fujia/run/temp/p2_ab_worktrees/A/lib/healpix_db/healpix_drizzle;C:/Users/fujia/run/temp/p2_ab_worktrees/A/lib/photometric_calib/cpp;C:/Users/fujia/run/temp/p2_ab_worktrees/A/lib/snr_estimator/cpp;C:/Users/fujia/run/temp/p2_ab_worktrees/A/lib/acr/backends/cuda/bridge;C:/Windows/System32;C:/Windows'
if (Test-Path $exe) {
  $p = Start-Process -FilePath $exe -ArgumentList @($cfg) -NoNewWindow -PassThru -RedirectStandardOutput 'C:/Users/fujia/smokeA2_out.log' -RedirectStandardError 'C:/Users/fujia/smokeA2_err.log'
  Start-Sleep -Seconds 8
  if (!$p.HasExited) { Stop-Process -Id $p.Id -Force; Write-Host 'SMOKE_RESULT RUNNING_8S (config OK)' } else { Write-Host "SMOKE_RESULT EXITED code=$($p.ExitCode)" }
} else {
  Write-Host 'SMOKE_RESULT EXE_MISSING'
}
Write-Host '--- stderr ---'
if (Test-Path 'C:/Users/fujia/smokeA2_err.log') { Get-Content 'C:/Users/fujia/smokeA2_err.log' -Tail 30 }
Write-Host '--- stdout tail ---'
if (Test-Path 'C:/Users/fujia/smokeA2_out.log') { Get-Content 'C:/Users/fujia/smokeA2_out.log' -Tail 30 }
Write-Host "SMOKE_A2_END $(Get-Date -Format o)"
