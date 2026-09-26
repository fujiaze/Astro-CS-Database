#!/usr/bin/env bash
# P4 重建稠密 SNR · 三路实验统一复现入口
# 固定 seed（写死于各脚本，见 SEEDS.md）；输出与 results/ 存档逐字段同构。
# 依赖：python3 + numpy；无仓库内 import、无网络。单实验 CPU 远低于 5 min 门。
set -euo pipefail
cd "$(dirname "$0")"

echo "== route1 (seed 20260926) =="
for f in route1/exp_p4_0*.py; do echo "-- $f"; python3 "$f"; done

echo "== route2 (seed 20260926..20261003) =="
for f in route2/exp_P4R2_*.py; do echo "-- $f"; python3 "$f"; done

echo "== route3 (seed 20260926) =="
for f in route3/exp0*.py; do echo "-- $f"; python3 "$f"; done

echo "== all done; compare outputs with results/route{1,2,3}/*.json =="
