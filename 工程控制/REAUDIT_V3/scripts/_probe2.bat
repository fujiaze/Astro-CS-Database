@echo off
echo === A proc cmdline ===
wmic process where "name='astrocs-stage2.exe'" get ProcessId,ExecutablePath,CommandLine /format:list 2>&1
echo === worktree A dir ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\A" 2>&1
echo === worktree A build_ab ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\A\build_ab" 2>&1 | findstr /i "exe log"
echo === worktree B build_ab ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_ab_worktrees\B\build_ab" 2>&1 | findstr /i "exe log"
echo PROBE_END
