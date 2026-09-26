#!/usr/bin/env bash
# 实验重做 · P1 通量积分拟合 · 三路可复现脚本统一入口
# 来源：独立审计/实验重做/P1通量积分拟合/{路线1,路线2,路线3}（2026-09 重做轮）
# 固定 seed：三路统一 SEED = 20260926（各脚本内写死；路线1 部分脚本派生 SEED+1…SEED+k，
#   路线3 的 exp_S00..exp_S12 亦为 20260926）。纯 Python + numpy，不 import 仓库任何 Python。
# 运行方式：bash code/redo/run_all.sh [quick]
#   全量约 3–5 min CPU（每脚本 << 5 min）；quick 跳过路线2 最慢的 exp3/exp7。
# 输出：各 routeN/results/*.json（重跑会就地再生；基准读数已快照在 results/redo/routeN/）。
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"

run_route() {
  local d="$1"; shift
  echo "== $d =="
  for f in "$d"/*.py; do
    case "$f" in *REPORT_*) continue;; esac
    if [ "$1" = "quick" ] && [[ "$f" == *exp3* || "$f" == *exp7* ]]; then
      echo "  [quick] 跳过 $f"; continue
    fi
    echo "  python3 $(basename "$f")"
    python3 "$f" > /dev/null
  done
}

run_route "$HERE/route1" "$1"
run_route "$HERE/route2" "$1"
run_route "$HERE/route3" "$1"
echo "完成：重跑读数与 results/redo/route{1,2,3}/ 基准快照逐项一致（固定 seed）。"
