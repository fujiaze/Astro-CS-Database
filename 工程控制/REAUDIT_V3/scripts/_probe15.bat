@echo off
echo === A now ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
echo === tasklist ===
tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh 2>&1
echo === build C signal file list ===
powershell -NoProfile -c "Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f.mosaic.hips/signal' -Recurse -Filter *.fits | ForEach-Object { $_.FullName.Substring($_.FullName.IndexOf('signal')+7) } | Out-File -Encoding ascii C:/Users/fujia/C_signal_list.txt; (Get-Content C:/Users/fujia/C_signal_list.txt | Measure-Object -Line).Count" 2>&1
echo PROBE_END
