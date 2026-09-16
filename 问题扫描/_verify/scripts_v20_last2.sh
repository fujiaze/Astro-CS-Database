
cd "/workspace/Astro CS Database"
echo "### p3_writer.json producers:"
grep -rn "p3_writer.json" --include="*.cpp" --include="*.h" --include="*.py" lib cli tests runtime | head -10
echo
echo "### verify reads module_build_id line:"
grep -n "module_build_id" lib/core/src/module_adapters.cpp
echo
echo "### hips plan max_workers literal + lease:"
sed -n "664,668p;842,846p" lib/hips/src/module_entry.cpp
echo
echo "### sha256 of cited files (行锚时点):"
sha256sum lib/core/src/module_adapters.cpp lib/drizzle/src/module_entry.cpp lib/hips/src/module_entry.cpp lib/snr_estimator/src/module_entry.cpp cli/commands.cpp cli/parser.cpp cli/resource_events.h lib/phase3_session/p3_session.cpp | sed 's/^\(.\{12\}\).* \(.*\)$/\1  \2/'
