# P04-004 evidence capture script
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
Set-Location "f:\Astro dev\Astro CS Normalization Database\lib\orchestrator\cpp"
$ev = "f:\Astro dev\Astro CS Normalization Database\engineering\evidence\P04-004"

# 1. capabilities (cancelled event + TIMEOUT/CANCELLED error codes)
& .\orchestrator.exe capabilities 1>"$ev\capabilities.stdout.log" 2>"$ev\capabilities.stderr.log"
"capabilities exit=$LASTEXITCODE" | Out-File -FilePath "$ev\capture_summary.txt" -Encoding UTF8

# 2. --cancel-on-signal accepted (stage1 nonexistent.fits)
& .\orchestrator.exe stage1 --frame nonexistent.fits --output "$ev\cancel_test.hiss" --cancel-on-signal 1>"$ev\cancel_signal.stdout.log" 2>"$ev\cancel_signal.stderr.log"
"cancel-on-signal exit=$LASTEXITCODE" | Out-File -FilePath "$ev\capture_summary.txt" -Append -Encoding UTF8

# 3. atomic cleanup (stage1 fails, partial output deleted)
$atomicPath = "$ev\atomic_test.hiss"
"PARTIAL_CONTENT" | Out-File -FilePath $atomicPath -Encoding ASCII -NoNewline
$before = Test-Path $atomicPath
& .\orchestrator.exe stage1 --frame nonexistent.fits --output $atomicPath 1>"$ev\atomic_cleanup.stdout.log" 2>"$ev\atomic_cleanup.stderr.log"
$after = Test-Path $atomicPath
"atomic cleanup exit=$LASTEXITCODE before=$before after=$after" | Out-File -FilePath "$ev\capture_summary.txt" -Append -Encoding UTF8

# 4. allow_partial_output=true (partial output kept)
$partialPath = "$ev\partial_test.hiss"
$partialCfg = "$ev\partial_config.json"
'{"allow_partial_output":true,"log_level":"ERROR"}' | Out-File -FilePath $partialCfg -Encoding UTF8
"PARTIAL_KEEP" | Out-File -FilePath $partialPath -Encoding ASCII -NoNewline
$beforeP = Test-Path $partialPath
& .\orchestrator.exe stage1 --frame nonexistent.fits --output $partialPath --config $partialCfg 1>"$ev\partial_keep.stdout.log" 2>"$ev\partial_keep.stderr.log"
$afterP = Test-Path $partialPath
"allow_partial exit=$LASTEXITCODE before=$beforeP after=$afterP" | Out-File -FilePath "$ev\capture_summary.txt" -Append -Encoding UTF8

# 5. stage_timeouts config parsing
$timeoutCfg = "$ev\timeout_config.json"
'{"stage_timeouts":{"READ_FITS":10.0,"CALIBRATE":60.0,"PLATESOLVE":120.0,"DRIZZLE":300.0},"log_level":"ERROR"}' | Out-File -FilePath $timeoutCfg -Encoding UTF8
& .\orchestrator.exe stage1 --frame nonexistent.fits --output "$ev\timeout_parse.hiss" --config $timeoutCfg 1>"$ev\timeout_parse.stdout.log" 2>"$ev\timeout_parse.stderr.log"
"timeout_parse exit=$LASTEXITCODE" | Out-File -FilePath "$ev\capture_summary.txt" -Append -Encoding UTF8

# 6. timeout trigger (real FITS + 0.001s timeout)
$fitsFile = (Get-ChildItem -Path "../../../testdata" -Recurse -Include *.fits -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if ($fitsFile) {
    $triggerCfg = "$ev\timeout_trigger_config.json"
    '{"stage_timeouts":{"READ_FITS":0.001},"log_level":"INFO"}' | Out-File -FilePath $triggerCfg -Encoding UTF8
    & .\orchestrator.exe stage1 --frame $fitsFile --output "$ev\timeout_trigger.hiss" --config $triggerCfg 1>"$ev\timeout_trigger.stdout.log" 2>"$ev\timeout_trigger.stderr.log"
    $triggerAfter = Test-Path "$ev\timeout_trigger.hiss"
    "timeout_trigger exit=$LASTEXITCODE (9=TIMEOUT) output_exists=$triggerAfter" | Out-File -FilePath "$ev\capture_summary.txt" -Append -Encoding UTF8
} else {
    "timeout_trigger: SKIP (no testdata fits)" | Out-File -FilePath "$ev\capture_summary.txt" -Append -Encoding UTF8
}

Write-Host "Evidence capture complete. Summary:"
Get-Content "$ev\capture_summary.txt"
