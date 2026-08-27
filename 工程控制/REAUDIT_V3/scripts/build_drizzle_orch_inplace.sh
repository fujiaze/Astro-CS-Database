#!/usr/bin/env bash
# In-place clean builds: healpix_drizzle + orchestrator (relative paths preserved).
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
SRC="$ROOT/current"
LOG="$ROOT/logs"

echo "=========== DRIZZLE (in-place) ==========="
cd "$SRC/lib/healpix_db/healpix_drizzle"
rm -f *.o *.dll
timeout 1800 make -j1 CXXFLAGS="-O2 -march=native -Wall -Wextra -Wpedantic -std=c++17 -fopenmp -fPIC -Wno-c++20-extensions" > "$LOG/drizzle_build2.log" 2>&1
echo "drizzle exit=$?"
ls -la healpix_drizzle.dll 2>/dev/null && sha256sum healpix_drizzle.dll && file healpix_drizzle.dll || echo NO_DRIZZLE_DLL
grep -cE "error:" "$LOG/drizzle_build2.log" || true

echo "=========== ORCHESTRATOR (in-place, LDFLAGS override) ==========="
cd "$SRC/lib/orchestrator/cpp"
rm -f orchestrator.exe
timeout 1800 make -j1 LDFLAGS="-static" > "$LOG/orchestrator_build2.log" 2>&1
echo "orchestrator exit=$?"
ls -la orchestrator.exe 2>/dev/null && file orchestrator.exe && sha256sum orchestrator.exe || echo NO_ORCHESTRATOR_EXE
grep -cE "error:" "$LOG/orchestrator_build2.log" || true

echo "INPLACE_BUILD_DONE"
