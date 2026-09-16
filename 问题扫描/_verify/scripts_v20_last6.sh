
cd "/workspace/Astro CS Database"
sed -n "4425,4450p" lib/core/src/module_adapters.cpp
echo "..."
grep -n "~0ull\|~0ULL" lib/core/src/module_adapters.cpp | head -8
