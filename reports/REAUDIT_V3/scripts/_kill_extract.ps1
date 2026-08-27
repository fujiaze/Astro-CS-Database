Get-CimInstance Win32_Process -Filter "Name='python.exe'" | Where-Object { $_.CommandLine -match 'extract_seam' } | ForEach-Object { Write-Output ("KILL " + $_.ProcessId + " " + $_.CommandLine.Substring(0,80)); Stop-Process -Id $_.ProcessId -Force }
Write-Output "DONE_KILL"
