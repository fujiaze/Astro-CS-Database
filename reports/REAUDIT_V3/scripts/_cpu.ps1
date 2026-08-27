
$samples = @()
1..2 | ForEach-Object {
  try { $c = Get-Counter "\Process(astrocs-stage2)\% Processor Time" -ErrorAction Stop } catch { Write-Output ("ERR " + $_.Exception.Message); break }
  $proc = $_.Counters | Select-Object -First 1
  $samples += $c.CounterSamples[0].CookedValue
  Start-Sleep -Seconds 20
}
if ($samples.Count -gt 0) { Write-Output ("CPU% " + (($samples | ForEach-Object { [math]::Round($_,2) }) -join " ")) }
Write-Output "COUNTER_DONE"
