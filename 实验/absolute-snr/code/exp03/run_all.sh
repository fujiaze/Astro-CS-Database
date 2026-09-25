#!/usr/bin/env bash
# EXP-03 一键复跑：区域化 sigma_sky 的绝对准确性与跨帧一致性（SCI-B-EXP-03）
# 用法：bash code/exp03/run_all.sh          （全部；约 25 分钟）
#       bash code/exp03/run_all.sh quick    （跳过真实数据臂）
# 纪律：只读 testdata/lib/eng/docs；不运行任何 ACSD 可执行文件；不使用 ulimit -v；
#       外部命令一律带 timeout；RSS 由 /usr/bin/time -v 记录到 run/SCI-B-EXP-03/logs/。
set -u
cd "$(dirname "$0")/../.." || exit 1     # -> 实验/absolute-snr
ROOT="$(cd ../.. && pwd)"
LOGS="$ROOT/run/SCI-B-EXP-03/logs"
mkdir -p "$LOGS" results
MODE="${1:-full}"
FAIL=0

run() {   # run <tag> <timeout_s> <cmd...>
  local tag="$1"; shift
  local tmo="$1"; shift
  echo "=== $tag ==="
  /usr/bin/time -v timeout "$tmo" "$@" > "$LOGS/$tag.log" 2>&1
  local rc=$?
  grep -E "Maximum resident set size|Elapsed \\(wall" "$LOGS/$tag.log" | sed 's/^/    /'
  if [ $rc -ne 0 ]; then echo "    ** $tag exit=$rc **"; FAIL=1; fi
}

run exp03_selftest        600 python3 -u code/exp03/exp03_common.py
run exp03_e1_analytic    3600 python3 -u code/exp03/e1_analytic.py
run exp03_e3_hst         3600 python3 -u code/exp03/e3_hst.py
if [ "$MODE" != "quick" ]; then
  run exp03_e2_real      7200 python3 -u code/exp03/e2_real_premise.py
fi
run exp03_e4_gates       2400 python3 -u code/exp03/e4_gates_selftest.py
run exp03_tables          600 python3 -u code/exp03/make_tables.py

echo
if [ $FAIL -eq 0 ]; then echo "EXP-03 ALL OK"; else echo "EXP-03 HAD FAILURES (see $LOGS)"; fi
exit $FAIL
