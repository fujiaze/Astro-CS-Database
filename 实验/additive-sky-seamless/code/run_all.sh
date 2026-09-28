#!/usr/bin/env bash
# 实验/additive-sky-seamless/code/run_all.sh —— SCI-C 一键复跑（固定 seed，无网络，无 git 写）
#
#   bash 实验/additive-sky-seamless/code/run_all.sh            # 全量
#   SKIP_BUILD=1 bash 实验/additive-sky-seamless/code/run_all.sh
#
# 产物：run/SCI-403/{logs,*.bin,*.json}（gitignore）与 <本单元>/results/*.json + figs/
# 路径一律从脚本自身位置推导（BASH_SOURCE），不写死单元目录名。
#
# 重计算纪律（AGENTS.md §3）：每个 python 步骤经 mem_guard 看门狗执行，超时用
# mem_guard 的 --timeout，**不套外层 timeout(1)**（外层只能杀看门狗本身，子进程会
# 逃逸成孤儿）。P5-22 订正点。
#
# HEAD 状态告示（P5 订正轮实测）：
#   - 探针重建已随生产接口退休同步（kappa_max / roughness_penalty 两键已退休，
#     libastrocs_identifiability.a 需显式链入）；
#   - C1–C7 的固化读数（results/c1–c7*.json）**不能**在本脚本下逐位复现：同一 fixture
#     （未校正臂接缝逐位一致）下求解器行为已变（n_params 58→49、κ 3.0e6→3.5e3、
#     iterations 20→28），c1 的 A6_subset_invariance / A9_final_gauge_near_noop 由
#     PASS 转 FAIL ⇒ 本脚本在 HEAD 上 rc=1。历史读数只作历史效力，见 REPORT_paper §6。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNIT="$(cd "$HERE/.." && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
cd "$ROOT"
mkdir -p run/SCI-403/logs "$UNIT/results/figs"

MAX_RSS_GB="${MAX_RSS_GB:-8}"      # 单进程探针档：正常峰值 < 1 GB ⇒ 8 GB 上限
STEP_TIMEOUT="${STEP_TIMEOUT:-3600}"

# 用**函数**而不是字符串变量：本仓根路径含空格（"Astro CS Database"），未加引号的
# ${MG} 展开会被词分割成 /workspace/Astro + CS + Database/... ⇒ 起不动且报怪错。
mg() { python3 "$ROOT/eng/tools/monitoring/mem_guard.py" \
         --max-rss-gb "$MAX_RSS_GB" --timeout "$STEP_TIMEOUT" "$@"; }

# 重跑前把**已入库**的固化读数备份到 run/SCI-403/results_prior/（P5 报名：run_all.sh 直接
# 覆写 results/c1–c7*.json，重跑即污染已固化证据面；此处先备份，比对后再决定是否回滚）。
mkdir -p run/SCI-403/results_prior
cp -f "$UNIT"/results/*.json run/SCI-403/results_prior/ 2>/dev/null || true
echo "[run_all] 固化读数已备份 -> run/SCI-403/results_prior/ ($(ls run/SCI-403/results_prior 2>/dev/null | wc -l) 个文件)"

if [ "${SKIP_BUILD:-0}" != "1" ]; then
  mg bash "$HERE/build_probes.sh" 2>&1 | tee run/SCI-403/logs/build_probes.log
fi

rc=0
for step in c1_additive c2_multiplicative c3_public_plane c4_seam_criterion \
            c5_weights c6_sparse_dense c7_realdata; do
  echo "== $step"
  if mg python3 "$HERE/$step.py" > "run/SCI-403/logs/${step%%_*}.log" 2>&1; then
    echo "   OK"
  else
    echo "   FAILED (see run/SCI-403/logs/${step%%_*}.log)"; rc=1
  fi
  tail -n 20 "run/SCI-403/logs/${step%%_*}.log" | sed 's/^/   /'
done
mg python3 "$HERE/make_figures.py"
echo "== SCI-C done rc=$rc"
exit $rc
