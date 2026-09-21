#!/usr/bin/env bash
# 开新一轮运行：回收旧轮次产物 → 建目录 → 写轮次元数据（负责人 2026-09-21 指令）。
# 用法：tools/round_start.sh <ROUND-ID>       例：tools/round_start.sh RELEASE-05
# 说明：run/ 是 gitignore 的临时区，历史轮次会累积到上百 GB；本脚本把"先清旧轮次"变成固定动作。
# 细节与硬护栏见 tools/run_gc.py 头部注释；保留清单见 tools/run_keep.txt。
set -euo pipefail

ROUND="${1:?用法: tools/round_start.sh <ROUND-ID>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "== 1/3 回收旧轮次产物（保留 run_keep.txt 与 $ROUND）=="
python3 tools/run_gc.py --apply --keep "$ROUND"

echo "== 2/3 建立 run/$ROUND =="
mkdir -p "run/$ROUND/logs" "run/$ROUND/evidence"

echo "== 3/3 写轮次元数据 =="
if [ ! -f "run/$ROUND/ROUND.md" ]; then
  {
    echo "# run/$ROUND"
    echo
    echo "- 开始: $(date -Iseconds)"
    echo "- commit: $(git rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "- 目的: （填写本轮目标）"
  } > "run/$ROUND/ROUND.md"
fi
echo "run/$ROUND 就绪；日志落 run/$ROUND/logs/，证据落 run/$ROUND/evidence/"
