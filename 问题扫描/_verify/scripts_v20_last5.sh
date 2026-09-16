
cd "/workspace/Astro CS Database"
echo "### drz max_workers consume:"; grep -n "max_workers" lib/drizzle/src/module_entry.cpp
echo; echo "### snr lease acquire:"; grep -n "acquire(ex->user_data\|acquire(inst->host" lib/snr_estimator/src/module_entry.cpp | head -5
echo; echo "### cos build_out_manifest body:"; sed -n "623,628p" lib/cosmetic/src/module_entry.cpp; grep -n "return ACS_OK" lib/cosmetic/src/module_entry.cpp | sed -n "1,3p"
echo; echo "### cli emit_resource_summary lines:"; grep -n "void emit_resource_summary\|resource_detail\|curve_points\|downsample_max" cli/commands.cpp | head -12
echo; echo "### parser kHelp drizzle line?:"; grep -n "astrocs " cli/parser.cpp | sed -n "1,26p" | tail -14
echo; echo "### HEAD:"; git --no-optional-locks log -1 --format="%h %H %ad" --date=iso; git --no-optional-locks status --porcelain | wc -l
