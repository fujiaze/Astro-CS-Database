#!/usr/bin/env bash
# F-INSTR (裁决 A6) 实验一键复跑. 只读研究; 不改 lib/ docs/ tests/ ci/; 零 git 写.
set -euo pipefail
export TMPDIR=${TMPDIR:-/dev/shm/astrocs_finstr}
mkdir -p "$TMPDIR"
cd "$(dirname "$0")/../../.."   # 仓库根
D=reverse_verify/experiments/f_instr
mkdir -p run/reverse_verify/f_instr/logs
for s in exp0_scene_and_noise exp1_recovery exp2_aperture_dependence exp3_seeing_null exp4_gain_recovery exp5_psf_shape; do
  echo "=== $s ==="
  python3 "$D/$s.py" 2>&1 | tee "run/reverse_verify/f_instr/logs/$s.log"
done
rm -rf "$TMPDIR" 2>/dev/null || true
echo "OK: run/reverse_verify/f_instr/"
