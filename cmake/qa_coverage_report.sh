#!/bin/sh
# qa_coverage_report.sh — QA-006 (V8-CI-005) ccov 目标第 3/4 步：
# 把 tests/unit 全部 *_test 可执行的 profraw 合并并产出 LLVM 覆盖报告。
# 用法: qa_coverage_report.sh <build_dir> <source_dir>
#   $1 build dir（含 coverage/*.profraw 与 tests/unit/*_test）
#   $2 source dir（覆盖统计只统计 lib/ 与 cli/）
# 仅 stdout 摘要；coverage/coverage.json 为 LLVM export JSON。
set -eu

BUILD_DIR="$1"
SRC_DIR="$2"
COV_DIR="$BUILD_DIR/coverage"

profdata="$COV_DIR/astrocs.profdata"
rm -f "$profdata"
# shellcheck disable=SC2086 — 无引号展开是本脚本契约（多 profraw 多目标）
llvm-profdata merge -sparse "$COV_DIR"/*.profraw -o "$profdata"

objects=""
for b in "$BUILD_DIR"/tests/unit/*_test; do
  [ -f "$b" ] || continue
  objects="$objects -object $b"
done
if [ -z "$objects" ]; then
  echo "qa_coverage_report: 未找到 $BUILD_DIR/tests/unit/*_test" >&2
  exit 1
fi

# shellcheck disable=SC2086
llvm-cov report $objects -instr-profile="$profdata" "$SRC_DIR/lib" "$SRC_DIR/cli"
# shellcheck disable=SC2086
llvm-cov export $objects -instr-profile="$profdata" "$SRC_DIR/lib" "$SRC_DIR/cli" \
  > "$COV_DIR/coverage.json"
echo "qa_coverage_report: coverage.json 已写出 ($COV_DIR/coverage.json)"
