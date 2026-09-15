#!/usr/bin/env bash
# IMPL-P3-RSMP-001 验证入口（独立构建 + ctest + 负向 mutation 驱动）。
# 所有命令带 timeout；日志落 run/v6/p3-rsmp/logs/；返回单一 rc。
set -u
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
WORK="$ROOT/run/v6/p3-rsmp"
LOGS="$WORK/logs"
BUILD="$WORK/build"
mkdir -p "$LOGS" "$BUILD"
overall=0

run_step() {
  local name="$1"; shift
  "$@" > "$LOGS/$name.log" 2>&1
  local rc=$?
  printf '%-28s rc=%d\n' "$name" "$rc"
  if [ "$rc" -ne 0 ]; then overall=1; fi
  return 0
}

rm -rf "$BUILD"
run_step 01_configure  timeout 300 cmake -S "$ROOT/tests/unit/v6_p3_rsmp" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
run_step 02_build      timeout 900 cmake --build "$BUILD" -j 8
( cd "$BUILD" && timeout 900 ctest --output-on-failure > "$LOGS/03_ctest.log" 2>&1 )
ctest_rc=$?
printf '%-28s rc=%d\n' "03_ctest" "$ctest_rc"
if [ "$ctest_rc" -ne 0 ]; then overall=1; fi
run_step 04_mutations  timeout 1500 python3 "$ROOT/tests/unit/v6_p3_rsmp/run_mutations.py"
run_step 05_evidence   timeout 300 python3 "$ROOT/tests/unit/v6_p3_rsmp/write_evidence.py"
exit "$overall"
