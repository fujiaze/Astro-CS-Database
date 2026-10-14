#!/usr/bin/env bash
# s06_run_all.sh — S06（P4 重建统计目标与域外失败）一键复现
#
# 纪律：
#   - 只读链接既有生产静态库（build/linux-sci 或 build/linux-sci-s06），不新建其它 build 目录；
#   - 不改 lib/**、docs/**、testdata/**；不做任何 git 写操作；
#   - 全部输入为内存合成真值场 + 生产探针，固定 seed，无网络。
#
# 产物（默认 S06_OUT=run/ACSDSDOC-02/S06/results）：
#   probe_s06_results.json          生产状态机 / 域外 / 退化 / 钳制（PRODUCTION）
#   S06_A_two_objects.json          两对象冻结与混用反例（ORACLE）
#   S06_B_overshoot.json            [0,2,0,0] 过冲重测 + 形状保持对照（ORACLE）
#   S06_C_holdout_cv.json           六臂留出 CV + dense/cell/frame 复核（ORACLE）
#   S06_D_degenerate_states.json    0 距 / NaN / NaN mask / 负值 clamp（ORACLE+PRODUCTION）
#   S06_E_idw_power_audit.json      idw_power 有效默认分裂审计（PRODUCTION 源码）
#   figures/fig_S06_*.png           只由上述真实读数绘制
#   实验/dense-snr-reconstruct/docs/S06_P4_evidence.json  验收格式汇总
#
# 用法： bash 实验/dense-snr-reconstruct/code/s06/s06_run_all.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
cd "$ROOT"
CODE="实验/dense-snr-reconstruct/code/s06"
export S06_OUT="${S06_OUT:-run/ACSDSDOC-02/S06/results}"
export S06_FIG="${S06_FIG:-run/ACSDSDOC-02/S06/figures}"
export PYTHONPATH="$CODE${PYTHONPATH:+:$PYTHONPATH}"
mkdir -p "$S06_OUT" "$S06_FIG"

# 生产静态库：优先自建目录，其次 Lead 已验证的 build/linux-sci
LIB=""
for cand in build/linux-sci-s06 build/linux-sci build/linux-control; do
  p="$cand/lib/algorithms/integration/phase2_integrate/libacsd_phase2_integrate.a"
  [ -f "$p" ] && LIB="$p" && BUILD="$cand" && break
done
[ -n "$LIB" ] || { echo "[S06] no phase2_integrate static library found" >&2; exit 2; }

echo "[S06] git HEAD        = $(git rev-parse HEAD)"
if [ -f "$BUILD/build_stamp_manifest.json" ]; then
  echo "[S06] library build   = $BUILD ($(python3 -c "import json;print(json.load(open('$BUILD/build_stamp_manifest.json'))['head_sha'])"))"
fi
echo "[S06] library path    = $LIB"
echo "[S06] p4 lib unchanged vs task start HEAD:"
git diff --stat 9f1e99f2a HEAD -- lib/algorithms/integration/phase2_integrate lib/algorithms/noise_snr \
  lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h | cat

echo "[S06] (1/6) compile probe_s06"
g++ -O2 -std=c++17 -fopenmp -I lib/algorithms/integration/phase2_integrate/include \
  "$CODE/probe_s06.cpp" -o "$S06_OUT/probe_s06" "$LIB"

echo "[S06] (2/6) run probe_s06 (OMP_NUM_THREADS=4, W=H=192)"
OMP_NUM_THREADS="${OMP_NUM_THREADS:-4}" "$S06_OUT/probe_s06" \
  "$S06_OUT/probe_s06_results.json" 192 2> "$S06_OUT/probe_s06.stderr"

echo "[S06] (3/6) python experiments A/B"
python3 "$CODE/s06_a_two_objects.py"
python3 "$CODE/s06_b_overshoot.py"

echo "[S06] (4/6) python experiments C/D/E"
python3 "$CODE/s06_c_holdout_cv.py"
python3 "$CODE/s06_d_degenerate_states.py"
python3 "$CODE/s06_e_idw_power_audit.py"

echo "[S06] (5/6) figures"
python3 "$CODE/s06_figures.py"

echo "[S06] (6/6) aggregate evidence JSON"
python3 "$CODE/s06_evidence.py"

echo "[S06] sha256:"
sha256sum "$S06_OUT"/probe_s06_results.json "$S06_OUT"/S06_*.json "$CODE"/*.py "$CODE"/probe_s06.cpp \
  | sed 's#^#  #'
echo "[S06] done"
