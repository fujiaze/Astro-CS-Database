
cd "/workspace/Astro CS Database"
echo "### L1 full-tree cancel_active:"
grep -rn "cancel_active" --include="*.c" --include="*.cpp" --include="*.h" . 2>/dev/null | grep -v "^./build/\|^./run/\|third_party" | head -10
echo
echo "### L2 whole-tree incl docs/tests/scripts:"
grep -rn "cancel_active" . 2>/dev/null | grep -v "^./build/\|^./run/\|third_party\|问题扫描\|^./.git/" | head -10
echo
echo "### p3 single-legal-value sites (current anchors):"
grep -n "coverage_output\|!= \"TAN\"\|projection\"\|\"frame\"" lib/phase3_session/p3_session.cpp lib/core/src/module_adapters.cpp | head -20
