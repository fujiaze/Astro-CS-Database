@echo off
echo === Norder0/Dir0 files ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips\signal\Norder0\Dir0" 2>&1
echo === Norder1/Dir0 files ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips\signal\Norder1\Dir0" 2>&1
echo === Norder2/Dir0 files (first 8) ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips\signal\Norder2\Dir0" 2>&1 | findstr /i fits
echo === Norder6/Dir0 count ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips\signal\Norder6" 2>&1
echo PROBE_END
