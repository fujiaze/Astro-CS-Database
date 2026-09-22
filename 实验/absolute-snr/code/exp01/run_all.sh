#!/usr/bin/env bash
# ============================================================================
# EXP-01 一键复跑：SCI-B 单元「问题一（容差 delta）+ 问题二（估计量错配）」全部实验。
#
#   * 纯 Python / NumPy / SciPy / astropy；**不构建、不运行任何 AstroCS 可执行文件**；
#   * **不使用 ulimit**（本仓曾因 RLIMIT_AS 静默改变行为出过事故）；
#   * 固定 seed = 20260924；各脚本峰值内存见日志中的 Maximum resident set size；
#   * 产物 -> ../results/exp01_*.json；日志 -> ../../../run/SCI-B-EXP-01/logs/。
#
# 用法：bash code/exp01/run_all.sh [--quick]
# ============================================================================
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
UNIT="$(cd "$HERE/../.." && pwd)"
ROOT="$(cd "$UNIT/../.." && pwd)"
OUT="$ROOT/run/SCI-B-EXP-01/logs"
mkdir -p "$OUT" "$UNIT/results"
cd "$UNIT"

QUICK=""
if [ "${1:-}" = "--quick" ]; then QUICK="--quick"; fi

echo "== [1/4] Q1: delta 误差预算（三类数据）=="
python3 code/exp01/q1_delta_budget.py $QUICK --out results/exp01_q1_delta.json | tee "$OUT/q1_delta.log"

echo "== [2/4] Q2a: 估计量配对性（解析 + MC + 负例）=="
python3 code/exp01/q2a_estimator_pairing.py $QUICK --out results/exp01_q2a_pairing.json | tee "$OUT/q2a.log"

echo "== [3/4] Q2b: 跨帧可比 + 真实产物 + 帧级口径 =="
python3 code/exp01/q2b_estimator_data.py $QUICK --out results/exp01_q2b_data.json | tee "$OUT/q2b.log"

echo "== [4/4] 判据自审（正常绿 / 注入故障红）=="
python3 code/exp01/negatives_selftest.py --results results | tee "$OUT/negatives.log"

echo "ALL DONE. results: $UNIT/results/exp01_*.json  logs: $OUT"
