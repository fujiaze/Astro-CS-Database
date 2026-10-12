#!/usr/bin/env bash
# P5 信号提取：一键复跑（固定输入、无随机源；elapsed_s 是运行时事实，不参与可复现判定）
#   bash run_all.sh [结果目录] [FITS 目录]
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:-$HERE/../results}"
FITS="${2:-$HERE/../deliver/fits}"
cd "$HERE"
python3 p5x_run.py --out "$OUT" --fits "$FITS"
