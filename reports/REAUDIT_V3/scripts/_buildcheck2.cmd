@echo off
echo === B build evidence ===
dir "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\B\build_ab\astrocs-stage2.exe" 2>&1
git -C "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\B" rev-parse HEAD
echo === C exe ===
dir "F:\Astro dev\Astro CS Normalization Database\lib\phase2\build\astrocs-stage2.exe" 2>&1
echo === A out dir + logs ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
dir /b C:\Users\fujia\*.log 2>&1 | findstr /i "32f A"
echo PROBE_END
