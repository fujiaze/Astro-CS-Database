
try { $c = Get-Counter "\Process(astrocs-stage2)\% Processor Time" -ErrorAction Stop } catch { Write-Output ("ERR " + $_.Exception.Message); exit }
Start-Sleep -Seconds 15
$c2 = Get-Counter "\Process(astrocs-stage2)\% Processor Time" -ErrorAction SilentlyContinue
Write-Output ("CPU1 " + [math]::Round($c.CounterSamples[0].CookedValue,1) + " CPU2 " + [math]::Round($c2.CounterSamples[0].CookedValue,1))
Write-Output "DONE"
