
cd "/workspace/Astro CS Database"
echo "### cal build_out_manifest return + caller:"
sed -n "824,830p" lib/calibration/src/module_entry.cpp; sed -n "1058,1064p" lib/calibration/src/module_entry.cpp
echo "### cal/cos/gaia max_workers consume:"
sed -n "940,946p" lib/calibration/src/module_entry.cpp
sed -n "758,764p" lib/cosmetic/src/module_entry.cpp
sed -n "659,665p" lib/gaia_xpsd_client/src/module_entry.c
echo "### hips acquire + snr acquire:"
sed -n "841,846p" lib/hips/src/module_entry.cpp
sed -n "937,943p" lib/snr_estimator/src/module_entry.cpp
echo "### snr se_status + frame path anchors:"
sed -n "2662,2677p" lib/core/src/module_adapters.cpp | head -6
sed -n "5298,5307p" lib/core/src/module_adapters.cpp
