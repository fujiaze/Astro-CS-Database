@echo off
echo === A build evidence ===
dir "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\A\build_ab\astrocs-stage2.exe" 2>&1
git -C "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\A" rev-parse HEAD
git -C "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\A" status --porcelain 2>&1 | head -5
echo === B build evidence ===
dir "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\B\build_ab\astrocs-stage2.exe" 2>&1
git -C "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\B" rev-parse HEAD
git -C "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\B" status --porcelain 2>&1 | head -5
echo === C exe ===
dir "F:\Astro dev\Astro CS Normalization Database\lib\phase2\build\astrocs-stage2.exe" 2>&1
echo PROBE_END
