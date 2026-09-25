#!/usr/bin/env bash
# EXP-04 一键复跑（固定 seed = 20260921；产物落 实验/absolute-snr/results/exp04_*.json 与 EXP04_TABLES.md）。
# 硬约束：不运行任何 ACSD 可执行文件；不用 ulimit -v；不跑 eng/tools/round_start.sh；
#         所有子步骤带 timeout + 外部 RSS 监控（默认上限 3000 MB）。
# 用法：bash run_all.sh [--quick]
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNIT="$(cd "$HERE/../.." && pwd)"
ROOT="$(cd "$UNIT/../.." && pwd)"
OUT="$ROOT/run/SCI-B-EXP-04"
mkdir -p "$OUT/logs" "$UNIT/results"
cd "$HERE"

QUICK=""
[ "${1:-}" = "--quick" ] && QUICK="--quick"
export EXP04_RSS_LIMIT_MB="${EXP04_RSS_LIMIT_MB:-3000}"
export EXP04_RSS_INTERVAL="${EXP04_RSS_INTERVAL:-2.0}"
# 外部 RSS 监控 + 超时包装（$HERE 含空格，必须在函数体内加引号展开，禁止放进未加引号的变量）
run() { timeout 3600 python3 -u "$HERE/rss_guard.py" python3 -u "$@"; }

step() { echo "== $* =="; }

step "[1/10] 算子自检 Oracle（红/绿）"
run selftest_operators.py 2>&1 | tee "$OUT/logs/selftest.log" || echo "WARN selftest rc=$?"

step "[2/10] 臂① 纯解析代数合成（算子对比 + 平坦场退化负例）"
run e1_analytic.py $QUICK 2>&1 | grep -v Warning | tee "$OUT/logs/e1.log" || echo "WARN e1 rc=$?"

step "[3/10] 臂② HST M16 真实模板 + 完整物理前向仿真"
run e2_hst.py $QUICK 2>&1 | grep -v Warning | tee "$OUT/logs/e2.log" || echo "WARN e2 rc=$?"

step "[4/10] 臂③ testdata M42 真实帧（棋盘 hold-out）"
run e3_real.py $QUICK 2>&1 | grep -v Warning | tee "$OUT/logs/e3.log" || echo "WARN e3 rc=$?"

step "[5/10] 负例与判据非退化审查"
run e4_gates.py 2>&1 | tee "$OUT/logs/e4.log" || echo "WARN e4 rc=$?"

step "[6/10] 代价：计算量标度 + 存储量"
run e5_cost.py $QUICK 2>&1 | tee "$OUT/logs/e5.log" || echo "WARN e5 rc=$?"

step "[7/10] 边界行为（无效区 / 阶跃 / 图像边界）"
run e6_boundary.py 2>&1 | tee "$OUT/logs/e6.log" || echo "WARN e6 rc=$?"

step "[8/10] 跨帧一致性"
run e7_crossframe.py 2>&1 | tee "$OUT/logs/e7.log" || echo "WARN e7 rc=$?"

step "[9/10] 复核 B3"
run e8_review_b3.py $QUICK 2>&1 | tee "$OUT/logs/e8.log" || echo "WARN e8 rc=$?"

step "[10/10] 值域钳制对照实验（v2 新增：回答「推荐对象是否等于被测量对象」）"
run e9_clip.py $QUICK 2>&1 | grep -v Warning | tee "$OUT/logs/e9.log" || echo "WARN e9 rc=$?"

step "[汇总] 生成 results/EXP04_TABLES.md"
python3 -u make_tables.py 2>&1 | tee "$OUT/logs/tables.log" || echo "WARN tables rc=$?"

echo "ALL DONE. results: $UNIT/results/exp04_*.json  logs: $OUT/logs"
