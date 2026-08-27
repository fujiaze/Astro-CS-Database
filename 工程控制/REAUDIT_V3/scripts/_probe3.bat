@echo off
echo === C mosaic subdirs ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips" 2>&1 | findstr /i "signal support variance ivar reject"
echo === C mosaic total size ===
dir /s "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips" 2>&1 | findstr /i "File(s)"
echo === B build logs ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\B" 2>&1 | findstr /i log
echo === A worktree HEAD state ===
cd /d "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\A" && git rev-parse HEAD 2>&1
cd /d "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\B" && git rev-parse HEAD 2>&1
echo PROBE_END
