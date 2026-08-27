
$s=@()
1..3 | ForEach-Object {
  try { $c = Get-Counter "\Process(astrocs-stage2)\% Processor Time" -ErrorAction Stop; $s += [math]::Round($c.CounterSamples[0].CookedValue,1) } catch { $s += "ERR" }
  Start-Sleep -Seconds 18
}
Write-Output ("CPU samples: " + ($s -join " "))
$t = tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh
Write-Output ("TL " + $t.Trim())
Write-Output "DONE"
