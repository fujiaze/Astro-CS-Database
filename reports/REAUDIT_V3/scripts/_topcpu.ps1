Get-Process | Where-Object { $_.CPU -gt 5 } | Sort-Object CPU -Descending | Select-Object -First 10 Id,ProcessName,CPU,WS | Format-Table -AutoSize | Out-String -Width 120
