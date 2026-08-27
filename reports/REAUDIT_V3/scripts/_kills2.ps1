Get-Process astrocs-stage2 -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "killed stage2"
