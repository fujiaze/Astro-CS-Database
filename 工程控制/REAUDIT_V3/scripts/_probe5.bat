@echo off
echo === A proc start/cpu ===
powershell -NoProfile -c "Get-Process astrocs-stage2 | Select-Object Id,StartTime,CPU,WS | Format-List | Out-String" 2>&1
echo === A upm head ===
powershell -NoProfile -c "$j = Get-Content 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f_A.mosaic.hips/upm_sparse.json' -Raw | ConvertFrom-Json; Write-Output ('model_hash=' + $j.model_hash); Write-Output ('controls=' + $j.control_count); Write-Output ('obs=' + $j.observation_count); Write-Output ('mu=' + $j.use_ivar_weight); Write-Output ('frames=' + $j.component_count)" 2>&1
echo === C model compare ===
powershell -NoProfile -c "$j = Get-Content 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f.mosaic.hips/upm_sparse.json' -Raw | ConvertFrom-Json; Write-Output ('C model_hash=' + $j.model_hash); Write-Output ('C controls=' + $j.control_count); Write-Output ('C obs=' + $j.observation_count); Write-Output ('C use_ivar=' + $j.use_ivar_weight)" 2>&1
echo PROBE_END
