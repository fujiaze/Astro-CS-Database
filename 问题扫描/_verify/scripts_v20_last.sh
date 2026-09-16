
cd "/workspace/Astro CS Database"
echo "### (1) geom.frame / coverage_output read sites:"
grep -rn "g->frame\|g\.frame\|geom\.frame\|->coverage_output\|\.coverage_output" lib/core/src/module_adapters.cpp lib/phase3_session/*.cpp | head -20
echo
echo "### (2) ci/run.py plan_only handling:"
grep -n "plan_only\|plan-only" ci/run.py | head -12
echo
echo "### (3) p3 writer artifact keys:"
sed -n "5866,5886p" lib/core/src/module_adapters.cpp | grep -o '"[a-z_0-9]*"' | tr '\n' ' '
echo
echo "### (4) RADESYS / coordinate_frame written by p3_output:"
grep -rn "RADESYS\|coordinate_frame\|CSYS" lib/phase3_session/p3_output.cpp | head -8
