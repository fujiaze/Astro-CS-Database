$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$env:PATH = 'C:/msys64/mingw64/bin;' + $env:PATH
$orch = Join-Path $root 'lib/orchestrator/cpp/orchestrator.exe'
$cfg = Join-Path $root 'lib/orchestrator/configs/stage1_gc_panel1_Red.json'
$out = Join-Path $root 'run/temp/p2_v7/gc/audit_s1_out.log'
$err = Join-Path $root 'run/temp/p2_v7/gc/audit_s1_err.log'
Remove-Item -LiteralPath $out,$err -ErrorAction SilentlyContinue
# fully detached: own hidden window, survives ssh disconnect
$p = Start-Process -FilePath $orch -ArgumentList ('"' + $cfg + '"') -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
Write-Output ('PID=' + $p.Id)
Write-Output ('HAS_EXITED=' + $p.HasExited)
