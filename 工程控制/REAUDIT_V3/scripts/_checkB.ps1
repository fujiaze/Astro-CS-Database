Get-Process astrocs-stage2 -ErrorAction SilentlyContinue | ForEach-Object { Write-Output ("CPU=" + $_.CPU + " start=" + $_.StartTime.ToString("HH:mm:ss") + " WS=" + $_.WS) }
Write-Output "PROCDONE"
