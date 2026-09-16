
cd "/workspace/Astro CS Database"
echo "### quick/full downstream:"
sed -n "2340,2360p" cli/commands.cpp
echo
echo "### mode usage in profile_gen_v2:"
grep -rn "quick\|Quick\|kQuick" lib/backend_host/profile_gen_v2.* tools/*/profile_gen* 2>/dev/null | head -10
echo
echo "### gaia by_coords / GaiaDbType sanity:"
grep -n "by_coords\|GaiaDbType" lib/gaia_xpsd_client/src/module_entry.c | head -8
