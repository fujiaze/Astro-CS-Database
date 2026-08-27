@echo off
cd /d C:\Users\fujia
echo START > C_extract.log
python extract_seam.py "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips" -15.67 C_b12_o7.csv 60 2 7 >> C_extract.log 2>&1
python extract_seam.py "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips" -20.75 C_b23_o7.csv 60 2 7 >> C_extract.log 2>&1
echo DONE >> C_extract.log
