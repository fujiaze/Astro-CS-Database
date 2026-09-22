#!/usr/bin/env bash
# EXP-02 一键复现：天光噪声估计的结构污染查证
# 用法：bash code/exp02/run_all.sh [--quick]
# 约束：不运行任何 AstroCS 可执行文件；不使用 ulimit -v；外部命令全部带 timeout。
set -u -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"   # 实验/absolute-snr
LOGDIR="$HERE/../../run/SCI-B-EXP-02/logs"
mkdir -p "$LOGDIR"
cd "$HERE" || exit 2

QUICK=0
[ "${1:-}" = "--quick" ] && QUICK=1

CROP=2048
[ "$QUICK" = "1" ] && CROP=1024

rc=0
run() {
  local name="$1"; shift
  echo "=== $name : $* ==="
  /usr/bin/time -v timeout 3600 "$@" > "$LOGDIR/$name.log" 2>&1
  local e=$?
  echo "    exit=$e  log=$LOGDIR/$name.log  peakRSS=$(grep -oP 'Maximum resident set size \(kbytes\): \K[0-9]+' "$LOGDIR/$name.log" | tail -1) kB"
  [ "$e" -ne 0 ] && rc=1
  return 0
}

# ── 臂 A：纯解析代数合成（含"真值无效应⇒归零"负例）──
if [ "$QUICK" = "1" ]; then
  run e1_analytic python3 code/exp02/e1_analytic_scan.py --quick
else
  run e1_analytic python3 code/exp02/e1_analytic_scan.py
fi

# ── 臂 B：HST M16 真实信号模板 + 完整物理前向仿真 ──
run e2_hst python3 code/exp02/e2_hst_scan.py

# ── 臂 C：testdata 真实数据（只读）──
run e3_real python3 code/exp02/e3_real_data.py --crop "$CROP"

# ── 门自审（任一门失败则 exit 1）──
run e4_gates python3 code/exp02/e4_gates_selftest.py

# ── 图与表 ──
run make_tables python3 code/exp02/make_tables.py

if [ "$rc" -eq 0 ]; then
  echo "ALL ARMS + GATES OK"
else
  echo "SOME STEP FAILED — 见 $LOGDIR"
fi
exit "$rc"