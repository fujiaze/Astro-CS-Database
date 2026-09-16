
cd "/workspace/Astro CS Database"
echo "### cancel_active in docs/tests/tools/ci:"
grep -rn "cancel_active" docs tests tools ci lib/gaia_xpsd_client/tests 2>/dev/null | head -6
echo
echo "### p3 validate sites:"
grep -n "coverage_output\|projection\|\"frame\"" lib/phase3_session/p3_session.cpp | head -15
echo
echo "### p3n_check_request_fields sites:"
grep -n "coverage_output\|projection\|frame" lib/core/src/module_adapters.cpp | sed -n "1,25p"
