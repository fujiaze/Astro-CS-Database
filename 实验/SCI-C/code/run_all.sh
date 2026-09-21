#!/usr/bin/env bash
# 实验/SCI-C/code/run_all.sh —— SCI-C 一键复跑（固定 seed，无网络，无 git 写）
#
#   bash 实验/SCI-C/code/run_all.sh            # 全量
#   SKIP_BUILD=1 bash 实验/SCI-C/code/run_all.sh
#
# 产物：run/SCI-403/{logs,*.bin,*.json}（gitignore）与 实验/SCI-C/results/*.json + figs/
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
cd "$ROOT"
mkdir -p run/SCI-403/logs 实验/SCI-C/results/figs

if [ "${SKIP_BUILD:-0}" != "1" ]; then
  bash "$HERE/build_probes.sh" 2>&1 | tee run/SCI-403/logs/build_probes.log
fi

rc=0
for step in c1_additive c2_multiplicative c3_public_plane c4_seam_criterion \
            c5_weights c6_sparse_dense c7_realdata; do
  echo "== $step"
  if timeout 3600 python3 "$HERE/$step.py" > "run/SCI-403/logs/${step%%_*}.log" 2>&1; then
    echo "   OK"
  else
    echo "   FAILED (see run/SCI-403/logs/${step%%_*}.log)"; rc=1
  fi
  tail -n 20 "run/SCI-403/logs/${step%%_*}.log" | sed 's/^/   /'
done
python3 "$HERE/make_figures.py"
echo "== SCI-C done rc=$rc"
exit $rc
