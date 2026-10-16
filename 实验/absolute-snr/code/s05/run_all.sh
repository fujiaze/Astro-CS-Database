#!/usr/bin/env bash
# S05 / P2：四臂 Oracle + k_corr 单位与标定域 + 117 星真实帧 + P3 协方差对照
# 全部产物写入 run/ACSDSDOC-02/S05/{results,figures}；本脚本不做 git 写操作。
set -eu
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
cd "$ROOT"
OUT=run/ACSDSDOC-02/S05
CODE=实验/absolute-snr/code/s05
mkdir -p "$OUT/results/probe" "$OUT/figures"

echo "[S05] HEAD = $(git rev-parse HEAD)"
python3 "$CODE/s05_arms_oracle.py"    --out "$OUT/results/arms_oracle.json"
bash    run/ACSDSDOC-02/S05/code/build_probe.sh
P=build/linux-sci-s05/s05probe/probe_s05_kcorr
# 生产几何 / 标定几何 / 交叉核对 / P3 顶点构型（固定 + 负例开关）
OMP_NUM_THREADS=1 $P "$OUT/results/probe/real_nside512.json"        0.9586 0.8 512    16 83.2833557851 -6.37428025059
OMP_NUM_THREADS=1 $P "$OUT/results/probe/calib_n512_s300_pf08.json" 300.0  0.8 512    16 45.0 41.810314895778596
OMP_NUM_THREADS=1 $P "$OUT/results/probe/mid_n512_s60_pf08.json"    60.0   0.8 512    64 45.0 41.810314895778596
OMP_NUM_THREADS=1 $P "$OUT/results/probe/xcheck_n8192_s10_pf08.json" 10.0  0.8 8192   64 45.0 41.810314895778596
OMP_NUM_THREADS=1 $P "$OUT/results/probe/xcheck_n8192_s25.8_pf08.json" 25.8 0.8 8192  32 45.0 41.810314895778596
OMP_NUM_THREADS=1 $P "$OUT/results/probe/vertex_nside8192_fixed.json" 0.04 0.8 8192   8 45.0 41.810314895778596
ACSD_DRZ_P3_FAULT=legacy_cross_normal OMP_NUM_THREADS=1 $P "$OUT/results/probe/vertex_nside8192_legacy.json" 0.04 0.8 8192 8 45.0 41.810314895778596
OMP_NUM_THREADS=1 $P "$OUT/results/probe/prod_n262144_s0.9586_pf08.json" 0.9586 0.8 262144 48 83.2833557851 -6.37428025059
ACSD_DRZ_P3_FAULT=legacy_cross_normal OMP_NUM_THREADS=1 $P "$OUT/results/probe/prod_n262144_s0.9586_pf08_legacy.json" 0.9586 0.8 262144 48 83.2833557851 -6.37428025059
OMP_NUM_THREADS=1 $P "$OUT/results/probe/prod_n2097152_s0.9586_pf08.json" 0.9586 0.8 2097152 16 83.2833557851 -6.37428025059
ACSD_DRZ_P3_FAULT=legacy_cross_normal OMP_NUM_THREADS=1 $P "$OUT/results/probe/calib_n512_s300_pf08_legacy.json" 300.0 0.8 512 16 45.0 41.810314895778596

python3 "$CODE/s05_kcorr_units.py" --out "$OUT/results/kcorr_units.json" --probe-dir "$OUT/results/probe"
python3 "$CODE/s05_real117.py"     --out "$OUT/results/real117.json"
python3 "$CODE/s05_figures.py"     --results "$OUT/results" --fig-dir "$OUT/figures"
python3 "$CODE/s05_evidence.py"    --results "$OUT/results" --out 实验/absolute-snr/docs/S05_P2_evidence.json
echo "[S05] done"
