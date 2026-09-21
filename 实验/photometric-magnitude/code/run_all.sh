#!/usr/bin/env bash
# SCI-A 一键复跑（固定种子 20260921）
#   用法：bash 实验/photometric-magnitude/code/run_all.sh            # 全量
#         bash 实验/photometric-magnitude/code/run_all.sh quick      # 跳过最慢的 step6 单元消除族
# 约定：构建/测试/检查串行化锁见 AGENTS.md §3；本脚本只跑实验，不碰 build/。
# 路径一律从脚本自身位置推导（BASH_SOURCE），不写死单元目录名。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNIT="$(cd "$HERE/.." && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
cd "$ROOT"
CODE="$HERE"
LOG="run/SCI-401/logs"
mkdir -p "$LOG" run/SCI-401/sim run/SCI-401/data_cache run/SCI-401/bin

echo "== 0) 外部参考数据（唯一需要网络的步骤；缓存 + SHA256 固定） =="
bash "$CODE/step0_fetch_refs.sh" | tee "$LOG/step0.log"

echo "== 1) 纯解析代数合成（Oracle + 负例 + 收敛性） =="
python3 -u "$CODE/step1_analytic.py" 2>&1 | tee "$LOG/step1.log"

echo "== 2) HST M16 真实信号模板 → 物理前向仿真帧 A/B/C =="
python3 -u "$CODE/step2_hst_sim.py" 2>&1 | tee "$LOG/step2.log"

echo "== 3) XP 合成通量 vs HST PHOTFLAM 绝对定标对拍 =="
python3 -u "$CODE/step3_forward_vs_photflam.py" 2>&1 | tee "$LOG/step3.log"

echo "== 4) 星表引导检测 vs 盲检测 =="
python3 -u "$CODE/step4_guided_vs_blind.py" 2>&1 | tee "$LOG/step4.log"

echo "== 5) 逐帧标定 + 双边界判据 + 帧间独立 =="
python3 -u "$CODE/step5_calibration_gate.py" 2>&1 | tee "$LOG/step5.log"

if [ "${1:-full}" != "quick" ]; then
  echo "== 6) apply photometry + 物理单位消除/不可辨识族 =="
  python3 -u "$CODE/step6_apply_and_units.py" 2>&1 | tee "$LOG/step6.log"
fi

echo "== 7) 非退化负例 =="
python3 -u "$CODE/step7_negatives.py" 2>&1 | tee "$LOG/step7.log"

echo "== 8) testdata 真实帧（底参照） =="
python3 -u "$CODE/step8_real_frame.py" 2>&1 | tee "$LOG/step8.log"

echo "== 9) 汇总判据表 =="
python3 -u "$CODE/step9_collect.py" 2>&1 | tee "$LOG/step9.log"

echo "全部完成。判据表：$UNIT/results/GATES.md"
