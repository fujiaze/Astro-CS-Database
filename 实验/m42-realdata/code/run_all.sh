#!/usr/bin/env bash
# 实验/m42-realdata/code/run_all.sh
# 第五实验单元（真实数据腿）一键复现。全部判据固定 seed，只读既有端到端产物，不重跑三命令。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
LOGDIR="$ROOT/run/M42-REALDATA-01/logs"
mkdir -p "$LOGDIR"
cd "$HERE"

rc=0
for s in c1_photometry c2_absolute_snr c3_seam_additive c4_leaf_allocation; do
  echo "=== $s ==="
  /usr/bin/time -v timeout 3600 python3 "$s.py" > "$LOGDIR/$s.log" 2>&1
  st=$?
  echo "  exit=$st  log=$LOGDIR/$s.log"
  [ $st -ne 0 ] && rc=$st
done

# 结果快照（完整性锚）
cd "$ROOT"
find "实验/m42-realdata" -type f \( -name '*.py' -o -name '*.md' -o -name '*.json' -o -name '*.sh' \) \
  -not -path '*/__pycache__/*' -print0 | sort -z | xargs -0 sha256sum > "实验/m42-realdata/results/SNAPSHOT.sha256"
echo "SNAPSHOT written: 实验/m42-realdata/results/SNAPSHOT.sha256"
exit $rc
