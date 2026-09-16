
cd "/workspace/Astro CS Database"
echo "### __workers injection sites:"
grep -rn "__workers" --include="*.cpp" --include="*.h" --include="*.py" lib cli runtime tests docs 2>/dev/null | head -20
echo
echo "### p2_session cpu_workers source:"
grep -rn "cpu_workers" lib/phase2_session/*.cpp lib/phase2/src/upm.cpp | head -10
