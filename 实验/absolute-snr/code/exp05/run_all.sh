#!/usr/bin/env bash
# EXP-05（SNR-ABS-DERIVE-01）一键复跑：推导「稀疏层存绝对 SNR」是否正确的三类数据实验。
# 固定 seed（SEED=20260926）；全部只读依赖 实验/shared 与 exp02/exp03 的公共库。
# 不运行任何 ACSD 可执行文件；不使用 ulimit -v；无 git 写操作。
set -euo pipefail
cd "$(dirname "$0")"

LOG_DIR="${EXP05_LOG_DIR:-../../../../run/SNR-ABS-DERIVE-01/logs}"   # 仓库根/run/...
mkdir -p "$LOG_DIR"
OUT="${EXP05_OUT:-../../results}"   # 实验/absolute-snr/results
mkdir -p "$OUT"

run() {
  local name="$1"; shift
  echo "=== $name ==="
  /usr/bin/time -f "[%e s, %M KB]" timeout 3600 python3 "$@" 2>&1 | tee "$LOG_DIR/$name.log"
}

run e1_analytic e1_analytic.py --out "$OUT/exp05_e1_analytic.json"
run e2_hst e2_hst.py --out "$OUT/exp05_e2_hst.json"
run e3_real e3_real.py --out "$OUT/exp05_e3_real.json"
run e6_mechanism e6_mechanism.py --out "$OUT/exp05_e6_mechanism.json"
run e4_weight e4_weight.py --real-json "$OUT/exp05_e3_real.json" --out "$OUT/exp05_e4_weight.json"
run e5_gates e5_gates.py --out "$OUT/exp05_e5_gates.json"
run make_tables make_tables.py --out "$OUT/EXP05_TABLES.md"

echo "ALL DONE"
