@echo off
echo === A dir ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
echo === A diagnostics head ===
type "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips\diagnostics.json" 2>&1 | more +0
echo === B dir ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_B.mosaic.hips" 2>&1
echo === worktrees ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees" 2>&1
echo === running stage2 ===
tasklist /fi "imagename eq astrocs-stage2.exe" 2>&1
echo PROBE_END
