
cd "/workspace/Astro CS Database"
echo "### M5a-G-003 gate flags claim:"
grep -rn "gate-required" 问题扫描/findings/ | head -6
echo
echo "### drz_rows n_bytes hits:"
grep -n "n_bytes" lib/drizzle/src/module_entry.cpp
echo
echo "### writer json key list (5866-5885) contains module_build_id?:"
sed -n "5866,5885p" lib/core/src/module_adapters.cpp | grep -c "module_build_id"
echo "### gaia plan node_id/host void:"
grep -n "(void)node_id\|(void)host\|(void)input_manifest_json" lib/gaia_xpsd_client/src/module_entry.c | head
