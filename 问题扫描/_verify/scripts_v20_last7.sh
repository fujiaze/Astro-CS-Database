
cd "/workspace/Astro CS Database"
echo "### warning flags:"
grep -rn "Wall\|Wextra\|Wunused\|Wno-" CMakeLists.txt cmake/*.cmake 2>/dev/null | head -12
echo
echo "### cal_describe body head:"
sed -n "1076,1100p" lib/calibration/src/module_entry.cpp
echo
echo "### cos_describe:"
sed -n "852,868p" lib/cosmetic/src/module_entry.cpp
