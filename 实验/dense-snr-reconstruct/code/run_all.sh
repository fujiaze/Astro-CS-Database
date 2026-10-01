#!/usr/bin/env bash
# P4 重建稠密 SNR · 统一复现入口
# 固定 seed（写死于各脚本，见 SEEDS.md；仿真腿的 seed 在共享场景配方内）；
# 输出与 results/ 存档逐字段同构。
# 依赖：python3 + numpy + astropy + scipy；无网络。
set -euo pipefail
cd "$(dirname "$0")"
UNIT="$(cd .. && pwd)"
REPO="$(cd "$UNIT/../.." && pwd)"

# 公共前置步：合成数据物理链自检。必须跑**全四项**（m16_mask / m16_scene /
# m16_sampling / noise_selftest），不得只调 noise_selftest.py 的 A/B 系列 ——
# 后者用独立原语重实现，只验「方法」，不经过生产代码；生产面的门禁责任在
# m16_scene --selftest、m16_sampling --selftest，以及 noise_selftest 的
# **C 系列**（C1–C6 直接调 NM.expose / NM.sky_surface_e_per_s / 解析一阶矩）。
# 任何组件判红则本单元读数不可作为证据。
# 注：m16_sampling 的 V6（存活底图残差预算）与 V10（真实 FITS 加载）读真实模板
# testdata/HST_M16/，该目录被 .gitignore 排除；模板缺失时这两条记红（不静默跳过）
# —— 需先备齐数据再跑本入口。
echo "== [0/4] 合成数据物理链自检（公共前置步）=="
if [ -x "$REPO/实验/shared/synthetic/run_selftests.sh" ] \
   || [ -f "$REPO/实验/shared/synthetic/run_selftests.sh" ]; then
  TMPDIR="${TMPDIR:-/var/tmp/astrocs}" bash "$REPO/实验/shared/synthetic/run_selftests.sh"
else
  echo "缺少 实验/shared/synthetic/run_selftests.sh —— 物理链自检不可跳过" >&2
  exit 2
fi

echo "== [1/4] route1 (seed 20260926) =="
for f in route1/exp_p4_0*.py; do echo "-- $f"; python3 "$f"; done

echo "== [2/4] route2 (seed 20260926..20261003) =="
for f in route2/exp_P4R2_*.py; do echo "-- $f"; python3 "$f"; done

echo "== [3/4] route3 (seed 20260926) =="
for f in route3/exp0*.py; do echo "-- $f"; python3 "$f"; done

echo "== [4/4] M16 物理前向仿真腿（哈勃物理仿真数据类）=="
# 逐像素 SNR 真值解析已知；判据只用不受噪声相关长度影响的那部分。
echo "-- sim/exp_sim01_m16_forward_snr_truth.py"
python3 sim/exp_sim01_m16_forward_snr_truth.py

echo "== all done; compare outputs with results/{route1,route2,route3,sim}/*.json =="
