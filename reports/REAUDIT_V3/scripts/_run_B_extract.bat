@echo off
cd /d C:\Users\fujia
echo START > B_extract.log
python extract_seam_np.py "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_B.mosaic.hips" -15.67 B_b12_o7.csv 60 2 7 >> B_extract.log 2>&1
python extract_seam_np.py "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_B.mosaic.hips" -20.75 B_b23_o7.csv 60 2 7 >> B_extract.log 2>&1
echo DONE >> B_extract.log
