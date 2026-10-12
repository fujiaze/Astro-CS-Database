#!/usr/bin/env bash
# P5 信号提取（生产网格版）：一键复跑。
#   网格完全取自生产成品 output_phase3.fits，不自建网格；无覆盖处留 NaN。
#   用法: bash run_production_grid.sh [结果目录] [FITS 目录] [缩略图目录]
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:-$HERE/../results/p5z}"
FITS="${2:-$HERE/../deliver/fits_p5z}"
THUMBS="${3:-$HERE/../deliver/thumbs_p5z}"
cd "$HERE"
python3 p5z2_run.py --out "$OUT" --fits "$FITS" --thumbs "$THUMBS"
# 结构保留（A 相对 raw，同单位自洽）
python3 structure_selfcheck.py "$FITS" "$OUT"
