$ErrorActionPreference = 'Continue'
# wrapper: launch orchestrator stage1 on panel1 Red detached with log
$root = 'F:/Astro dev/Astro CS Normalization Database'
$env:PATH = 'C:/msys64/mingw64/bin;' + $env:PATH
$orch = Join-Path $root 'lib/orchestrator/cpp/orchestrator.exe'
$cfg = Join-Path $root 'lib/orchestrator/configs/stage1_gc_panel1_Red.json'
$log = Join-Path $root 'run/temp/p2_v7/gc/audit_stage1_panel1_Red.out.log'
$err = Join-Path $root 'run/temp/p2_v7/gc/audit_stage1_panel1_Red.err.log'
$p = Start-Process -FilePath $orch -ArgumentList $cfg -NoNewWindow -PassThru -RedirectStandardOutput $log -RedirectStandardError $err
Write-Output ('PID=' + $p.Id)
Write-Output ('STARTED=' + (Get-Date -Format s))
