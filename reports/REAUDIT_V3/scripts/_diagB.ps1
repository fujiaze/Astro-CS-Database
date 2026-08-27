
try { $c = Get-Counter "\Process(astrocs-stage2)\% Processor Time" -ErrorAction Stop } catch { Write-Output ("NOCOUNTER " + $_.Exception.Message) }
if ($c) { Write-Output ("CPU " + [math]::Round($c.CounterSamples[0].CookedValue,1)) }
$p = tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh
Write-Output ("TASKLIST " + $p.Trim())
Write-Output "DONE"
