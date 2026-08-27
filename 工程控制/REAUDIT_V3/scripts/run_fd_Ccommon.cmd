@echo off
set PATH=C:\msys64\mingw64\bin;F:\Astro dev\Astro CS Normalization Database\lib\phase2\build;F:\Astro dev\Astro CS Normalization Database\lib\astro_image_io;F:\Astro dev\Astro CS Normalization Database\lib\calibration;F:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_drizzle;F:\Astro dev\Astro CS Normalization Database\lib\photometric_calib\cpp;F:\Astro dev\Astro CS Normalization Database\lib\snr_estimator\cpp;F:\Astro dev\Astro CS Normalization Database\lib\acr\backends\cuda\bridge;%PATH%
echo START %date% %time% > C:\Users\fujia\stage2_32f_Ccommon.log
"F:\Astro dev\Astro CS Normalization Database\lib\phase2\build\astrocs-stage2.exe" C:\Users\fujia\stage2_fatduck32_Ccommon.json >> C:\Users\fujia\stage2_32f_Ccommon.log 2>&1
echo EXIT %errorlevel% %date% %time% >> C:\Users\fujia\stage2_32f_Ccommon.log
