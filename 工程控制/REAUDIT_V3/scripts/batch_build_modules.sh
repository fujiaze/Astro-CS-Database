#!/usr/bin/env bash
# Batch clean builds of remaining layer targets (Control §6.2) from exported current source.
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
SRC="$ROOT/current"
B="$ROOT/builds"
LOG="$ROOT/logs"
mkdir -p "$B" "$LOG"

# --- layer: healpix_drizzle (linux) ---
echo "=========== DRIZZLE BUILD ==========="
DDIR="$B/drizzle_clean"
rm -rf "$DDIR"; cp -r "$SRC/lib/healpix_db/healpix_drizzle" "$DDIR"
cd "$DDIR"
timeout 1200 make -j1 CXXFLAGS="-O2 -march=native -Wall -Wextra -Wpedantic -std=c++17 -fopenmp -fPIC -Wno-c++20-extensions" AIO_DIR="../../astro_image_io" > "$LOG/drizzle_build.log" 2>&1
echo "drizzle exit=$?"
ls -la healpix_drizzle.dll 2>/dev/null && sha256sum healpix_drizzle.dll || echo "NO_DRIZZLE_DLL"
grep -c "error:" "$LOG/drizzle_build.log" || true

# --- layer: calibration cosmetic_corrector ---
echo "=========== CALIBRATION BUILD ==========="
CDIR="$B/calibration_clean"
rm -rf "$CDIR"; cp -r "$SRC/lib/calibration" "$CDIR"
cd "$CDIR"
timeout 600 make -j1 CXXFLAGS="-O2 -march=native -fopenmp -Wall -std=c++17 -fPIC" > "$LOG/calibration_build.log" 2>&1
echo "calibration exit=$?"
ls -la cosmetic_corrector.dll 2>/dev/null && sha256sum cosmetic_corrector.dll || echo "NO_CALIB_DLL"
grep -c "error:" "$LOG/calibration_build.log" || true

# --- layer: acr subproject (CMake) ---
echo "=========== ACR BUILD ==========="
ADIR="$B/acr_clean"
rm -rf "$ADIR"; mkdir -p "$ADIR"
cd "$ADIR"
timeout 900 cmake -S "$SRC/lib/acr" -B "$ADIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "$LOG/acr_cfg.log" 2>&1
echo "acr configure exit=$?"
timeout 2400 cmake --build "$ADIR" -j1 > "$LOG/acr_build.log" 2>&1
echo "acr build exit=$?"
grep -c "error:" "$LOG/acr_build.log" || true

# --- layer: orchestrator CLI (LDFLAGS override dropping -Wl,--stack) ---
echo "=========== ORCHESTRATOR BUILD ==========="
ODIR="$B/orchestrator_clean"
rm -rf "$ODIR"; cp -r "$SRC/lib/orchestrator/cpp" "$ODIR"
cd "$ODIR"
timeout 1200 make -j1 LDFLAGS="-static" > "$LOG/orchestrator_build.log" 2>&1
echo "orchestrator exit=$?"
ls -la orchestrator.exe 2>/dev/null && file orchestrator.exe && sha256sum orchestrator.exe || echo "NO_ORCHESTRATOR_EXE"
grep -c "error:" "$LOG/orchestrator_build.log" || true

echo "BATCH_BUILD_DONE"
