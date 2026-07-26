$env:Path = "f:\Astro dev\Astro CS Normalization Database\lib\orchestrator\cpp;C:\msys64\mingw64\bin;" + $env:Path
Set-Location "f:\Astro dev\Astro CS Normalization Database"
$scen = "s1_4_2_tiny_image"
$out = "engineering\evidence\P05-003\hiss\$scen.hiss"
if (Test-Path $out) { Remove-Item $out -Force }
$proc = Start-Process -FilePath "lib\orchestrator\cpp\orchestrator.exe" `
    -ArgumentList @("stage1","--frame","engineering\evidence\P05-003\fixtures\tiny_10x10.fits","--output",$out,"--config","engineering\evidence\P05-003\configs\allow_no_calib_config.json","--log-level","INFO") `
    -RedirectStandardOutput "engineering\evidence\P05-003\logs\$scen\stdout.log" `
    -RedirectStandardError "engineering\evidence\P05-003\logs\$scen\stderr.log" `
    -NoNewWindow -PassThru
$proc.WaitForExit(180000) | Out-Null
$exit_code = $proc.ExitCode
$output_exists = Test-Path $out
Write-Output "exit_code=$exit_code output_exists=$output_exists"
