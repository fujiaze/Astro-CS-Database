#!/usr/bin/env bash
# EXP-06（EXP-06-SNR-PHYS）一键复跑：帧内 SNR 的物理建模与重建
# （臂 A 解析 / 臂 B HST / 臂 C 真实 / 消融 / 门 / 作用域图谱 / 表格）。
# 固定 seed（SEED=20260927）；只读依赖 实验/shared 与 exp02..exp05 的公共库。
# 不运行任何 ACSD 可执行文件；不使用 ulimit -v；无 git 写操作。
# 任一步失败即以非零码退出（不静默继续），末行打印端到端判定。
set -euo pipefail
cd "$(dirname "$0")"

LOG_DIR="${EXP06_LOG_DIR:-../../../../run/EXP-06-SNR-PHYS/logs}"
mkdir -p "$LOG_DIR"
OUT="${EXP06_OUT:-../../results}"
mkdir -p "$OUT"

FAILED=0
run() {
  local name="$1"; shift
  echo "=== $name ==="
  set +e
  /usr/bin/time -f "[%e s, %M KB]" timeout 3000 python3 "$@" 2>&1 | tee "$LOG_DIR/$name.log"
  local rc=${PIPESTATUS[0]}
  set -e
  if [ "$rc" -ne 0 ]; then
    echo "!!! $name FAILED rc=$rc"
    FAILED=1
  fi
  return 0
}

run e1_analytic e1_analytic.py --seeds 6 --out "$OUT/exp06_e1_analytic.json"
run e2_hst      e2_hst.py                --out "$OUT/exp06_e2_hst.json"
run e3_real     e3_real.py               --out "$OUT/exp06_e3_real.json"
run e4_gates    e4_gates.py              --out "$OUT/exp06_e4_gates.json"
run e5_scope    e5_scope.py --seeds 3    --out "$OUT/exp06_e5_scope.json"
run e6_ablation e6_ablation.py           --out "$OUT/exp06_e6_ablation.json"
run make_tables make_tables.py           --out "$OUT/EXP06_TABLES.md"

# 端到端判定：门与消融的 ALL_PASS 必须为真（写入日志，供复核者直接引用）
python3 - "$OUT" <<'PY'
import json, sys, os
out = sys.argv[1]
bad = []
for f, key in (("exp06_e4_gates.json", "all_pass"), ("exp06_e6_ablation.json", "all_pass")):
    p = os.path.join(out, f)
    d = json.load(open(p, encoding="utf-8"))
    n = len(d.get("gates", []))
    npass = sum(1 for g in d["gates"] if g.get("verdict") == "PASS")
    print("%-26s gates=%d pass=%d ALL_PASS=%s" % (f, n, npass, d.get(key)))
    if not d.get(key):
        bad.append(f)
print("END_TO_END_ALL_PASS =", not bad)
PY

if [ "$FAILED" -ne 0 ]; then
  echo "RUN_ALL_FAILED"
  exit 1
fi
echo "ALL DONE"
