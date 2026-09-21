#!/usr/bin/env bash
# AstroCS 根级 Linux configure/build/test 入口 (BLD-001)
# 用法: ./build.sh [Debug|Release]        (default Release)
# 行为:
#   1) 在 build/linux-<type> 内 cmake 配置 (相对仓库根, 可重复);
#   2) 构建 phase2 模块 + 其测试目标;
#   3) 从仓库根运行 phase2 测试套件 (stage2_exe() 用相对路径, 须仓库根 CWD);
#   4) 输出汇总并返回 0/非 0。
# 说明: 仅覆盖 CMake 模块 (phase2); drizzle/browser_qt/orchestrator 尚无统一 CMake 入口 (见
#   reports/REAUDIT_V3/v3_exec/G5_linux_prebuild_baseline.md 的 BLD-001 缺口)。

set -uo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
BUILD_TYPE="${1:-Release}"
# 每次用全新 build 目录 (TST-001 不复用旧对象); 启用 OpenMP (P2_ENABLE_OPENMP=ON)。
BUILD_DIR="$REPO/build/run-${BUILD_TYPE,,}"
LOG="$REPO/run/logs/build_${BUILD_TYPE,,}.log"
mkdir -p "$REPO/run/logs"
: > "$LOG"
rm -rf "$BUILD_DIR"

echo "[BLD-001] type=$BUILD_TYPE  dir=$BUILD_DIR  log=$LOG"

echo "[1/4] cmake configure ..."
if ! cmake -S "$REPO/lib/phase2" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DP2_ENABLE_OPENMP=ON \
      >> "$LOG" 2>&1; then
  echo "CONFIGURE FAIL (see $LOG)"; exit 1
fi

echo "[2/4] cmake build (phase2 + tests) ..."
if ! cmake --build "$BUILD_DIR" --target phase2 phase2_synthetic_gate \
      phase2_ivar_wiring phase2_execution_options phase2_routing phase2_async_io \
      -j "$(nproc)" >> "$LOG" 2>&1; then
  echo "BUILD FAIL (see $LOG)"; exit 1
fi

echo "[3/4] run tests (repo root CWD) ..."
cd "$REPO"
PASS=0; FAIL=0
for t in phase2_synthetic_gate phase2_ivar_wiring phase2_execution_options \
         phase2_routing phase2_async_io; do
  printf "  %-32s" "$t:"
  if timeout 300 "$BUILD_DIR/$t" >> "$LOG" 2>&1; then
    echo " PASS"; PASS=$((PASS+1))
  else
    echo " FAIL"; FAIL=$((FAIL+1))
  fi
done

echo "[4/4] summary: PASS=$PASS FAIL=$FAIL (log: $LOG)"
echo "BLD-001_RESULT=$([ "$FAIL" -eq 0 ] && echo OK || echo FAIL)"
[ "$FAIL" -eq 0 ] || exit 1
