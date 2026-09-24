#!/usr/bin/env bash
# SCI-RECON-SNR-01 / B7 一键复现：编译生产链路驱动 → 跑主 seed → 跑第二 seed（稳健性）。
# 重计算一律经 mem_guard（AGENTS §3）；产物落 run/SCI-RECON-SNR-01/ 与 实验/absolute-snr/results/。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
UNIT="$ROOT/实验/absolute-snr"
RUND="$ROOT/run/SCI-RECON-SNR-01"
mkdir -p "$RUND/logs"
bash "$HERE/b7_build_driver.sh"
cd "$ROOT"
OMP_NUM_THREADS="${OMP_NUM_THREADS:-4}" python3 eng/tools/monitoring/mem_guard.py \
  --max-rss-gb 8 --timeout 3600 -- \
  python3 "$HERE/b7_absolute_snr_recon.py" \
  --out "$UNIT/results/b7_absolute_snr_recon.json" 2>&1 | tee "$RUND/logs/b7_seed_base.log"
OMP_NUM_THREADS="${OMP_NUM_THREADS:-4}" python3 eng/tools/monitoring/mem_guard.py \
  --max-rss-gb 8 --timeout 3600 -- \
  python3 "$HERE/b7_absolute_snr_recon.py" --seed 20260922 \
  --out "$UNIT/results/b7_absolute_snr_recon_seed20260922.json" 2>&1 | tee "$RUND/logs/b7_seed_alt.log"
echo "done"
