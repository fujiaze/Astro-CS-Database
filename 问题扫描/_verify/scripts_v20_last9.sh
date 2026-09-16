
cd "/workspace/Astro CS Database"
echo "### which targets get -Wextra (CMakeLists 640-660):"
sed -n "640,660p" CMakeLists.txt
echo
echo "### calibration/cosmetic target flags:"
grep -n "target_compile_options\|add_library\|add_executable" lib/calibration/CMakeLists.txt lib/cosmetic/CMakeLists.txt | head -12
echo
echo "### gaia cancel_active reads:"
grep -n "cancel_active" lib/gaia_xpsd_client/src/module_entry.c
