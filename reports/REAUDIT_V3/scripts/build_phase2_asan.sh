#!/usr/bin/env bash
# Phase2 ASan+UBSan build (Control §6.2 layer 5) + run applicable tests under sanitizers.
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
SRC="$ROOT/current/lib/phase2"
B="$ROOT/builds"
LOG="$ROOT/logs"
BDIR="$B/phase2_asan_ubsan"
rm -rf "$BDIR"; mkdir -p "$BDIR"
echo "=== ASAN CONFIGURE ==="
timeout 300 cmake -S "$SRC" -B "$BDIR" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" -DP2_ENABLE_OPENMP=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "$LOG/phase2_asan_cfg.log" 2>&1
echo "asan configure exit=$?"
echo "=== ASAN BUILD ==="
timeout 3600 cmake --build "$BDIR" -j1 > "$LOG/phase2_asan_build.log" 2>&1
echo "asan build exit=$?"
grep -cE "error:" "$LOG/phase2_asan_build.log" || true
echo "=== RUN synthetic_gate under ASan (subset: fast tests) ==="
if [ -x "$BDIR/phase2_synthetic_gate" ]; then
  # run a focused subset: identity + spatial truth + frame order (fast, core UPM)
  /usr/bin/time -v -o "$LOG/phase2_asan_time.txt" timeout 900 "$BDIR/phase2_synthetic_gate" --gtest_filter="Phase2Upm.S0IdentityCalibrationNoChange:Phase2Upm.S1KnownAdditiveFieldRecovered:Phase2Upm.G1SpatialFieldTruth:Phase2Upm.FrameOrderInvariance:Phase2Upm.SparseEqualsDense" > "$LOG/phase2_asan_test.log" 2>&1
  echo "asan test exit=$?"
  tail -20 "$LOG/phase2_asan_test.log"
else
  echo "NO_ASAN_TEST_BINARY"
fi
echo "ASAN_DONE"
