@echo off
set PATH=C:\msys64\mingw64\bin;%PATH%
set QT_PLUGIN_PATH=C:\msys64\mingw64\share\Qt6\plugins
set QT_DEBUG_PLUGINS=1
"f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build\healpix_browser_qt.exe" "f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red\drizzle\T4_2x_nside65536.hiss"
