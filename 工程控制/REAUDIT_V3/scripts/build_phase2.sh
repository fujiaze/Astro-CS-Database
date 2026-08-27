#!/usr/bin/env bash
# Phase2 clean CMake build (release, OpenMP OFF default) from exported current source
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
SRC="$ROOT/current/lib/phase2"
B="$ROOT/builds"
LOG="$ROOT/logs"
BDIR="$B/phase2_clean_release_ompOFF"
rm -rf "$BDIR"
mkdir -p "$BDIR" "$LOG"
GD=$(git -C "/home/lighthouse/Astro CS Database" rev-parse --short HEAD 2>/dev/null || echo unknown)
{
  echo "layer=phase2-subproject-linux-release (P2_ENABLE_OPENMP=OFF, documented default)"
  echo "source_tree_commit=HEAD ($GD)"
  echo "compiler=$(g++ --version | head -1)"
  echo "cmake=$(cmake --version | head -1)"
  echo "start_utc=$(date -u +%FT%TZ)"
} > "$BDIR/BUILD_META.txt"
echo "=== CONFIGURE ==="
timeout 300 cmake -S "$SRC" -B "$BDIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DP2_ENABLE_OPENMP=OFF > "$LOG/phase2_cfg_ompOFF.log" 2>&1
CFG_EC=$?
echo "configure exit=$CFG_EC"
if [ $CFG_EC -ne 0 ]; then echo "CONFIG_FAILED"; tail -30 "$LOG/phase2_cfg_ompOFF.log"; exit 1; fi
echo "=== BUILD ==="
/usr/bin/time -v -o "$LOG/phase2_build_ompOFF_time.txt" timeout 3600 cmake --build "$BDIR" -j1 > "$LOG/phase2_build_ompOFF.log" 2>&1
EC=$?
echo "build exit=$EC"
{ echo "end_utc=$(date -u +%FT%TZ)"; echo "configure_exit=$CFG_EC"; echo "build_exit=$EC"; } >> "$BDIR/BUILD_META.txt"
echo "=== targets ==="
find "$BDIR" -maxdepth 2 -type f -executable -o -type f -name "*.so*" -o -type f -name "*.dll" | sort
echo "=== astrocs-stage2 ==="
ls -la "$BDIR/astrocs-stage2" 2>/dev/null && file "$BDIR/astrocs-stage2" && sha256sum "$BDIR/astrocs-stage2" 2>/dev/null
echo "=== warnings/errors ==="
echo "warnings: $(grep -ciE "warning:" "$LOG/phase2_build_ompOFF.log" 2>/dev/null || echo 0)"
echo "errors: $(grep -ciE "error:" "$LOG/phase2_build_ompOFF.log" 2>/dev/null || echo 0)"
echo "=== ctest -N ==="
timeout 60 ctest -N --test-dir "$BDIR" 2>&1 | head -30
echo "PHASE2_RELEASE_BUILD_DONE exit=$EC"
