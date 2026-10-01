#!/usr/bin/env bash
# audit_rework/run_all.sh -- P5 审计重做三路 + 两个补实验 统一复现入口
#
# 事实源: 独立审计/实验重做/总编对账/{分歧台账.md,五单元成稿简报.md}
# 纪律: 固定 seed（见各脚本头部，写死于源码内）; 纯 python3+numpy; 零仓库 import; 零 git 写。
# 输出: 各子目录内就地生成 results/*.json（若脚本按相对路径写盘）。
#       本目录上游的 results/audit_rework/ 是已固化的一次运行拷贝，重跑不会覆盖它。
#
# 用法:
#   bash 实验/additive-sky-seamless/code/audit_rework/run_all.sh
#
# 各子目录可独立运行（脚本对工作目录不敏感，产物落在各自 results/ 下）。

# set -e 不是可选项：没有它，`run()` 子壳里的 exit 1 只终止子壳，外层继续往下跑，
# 脚本最后一条命令是 echo ⇒ 退出码恒为 0，一条腿失败也会报「全绿」。
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"

run() {
  local dir="$1"; shift
  echo "== $dir =="
  ( cd "$HERE/$dir" && for s in "$@"; do
      echo "  -- $s"
      python3 "$s" || { echo "FAILED: $dir/$s"; exit 1; }
    done )
}

# 路线1（seed 20260319/20260320/20260325/20260926 系，逐脚本头标注）
run route1 c1_median_variance.py c2_kcorr_drizzle.py c3_seam_gate.py c4_variance_ratio.py \
    c5_huber_efficiency.py c6_identifiability_dof.py c7_share_vs_abs_weight.py \
    c8_scale_tolerance.py c9_representation_boundary.py c10_anchor_forensics.py

# 路线2（seed 20250926）
run route2 e1_control_variance_median.py e2_kcorr_drizzle_mc.py e3_seam_gate_1e-2.py \
    e4_variance_ratio_blindness.py e5_huber_delta_1345.py e6_quality_factor_share_weights.py \
    e7_smoothing_lambda_biasvariance.py e8_identifiability_rank_rtol.py \
    e9_node_spacing_representation.py e10_sigma_floor_crossscale.py

# 路线3（seed 20250926 系）
run route3 exp01_seam_gate.py exp02_variance_ratio.py exp03_median_variance.py exp04_kcorr_mc.py \
    exp05_huber_efficiency.py exp06_dof_rankeff.py exp07_rank_rtol.py exp08_node_spacing.py \
    exp09_zero_anchor.py exp10_weight_arms.py exp11_scale_invariance.py exp12_quality_factor.py

# 补实验 A/B（seed 20250926 = p5c_common.SEED_BASE；约各 1 分钟）
run supp_507_relstep ea_507_scale_scan.py eb_relstep_calibration.py

# P2 补实验 control_variance（D-07 定稿口径的三脚本；跨单元证据，P5 论文按 D-07 引用）
run supp_control_variance finiteN_control_variance.py production_chain_control_variance.py \
    spotcheck_independent_seed.py

# ---------------------------------------------------------------------------
# 判据 rollup —— 修「已判红的门被静默吞掉」
#
# 本子树 38 个脚本**无一 sys.exit(1)**（实测 grep "sys.exit|exit(1)" 零命中），
# 所以上面 run() 里的 `|| exit 1` 只能抓到「脚本崩溃」，抓不到「门判红」。
# 后果：固化 JSON 里已记 false 的门照样被引用，而本脚本照旧打印 ALL DONE。
# 这违反 08 规范 §5「红灯不以 waiver 覆盖，SKIP 不计通过」。
#
# rollup.py 按 GATE_DISCLOSURE.json 逐点显式披露并判决：
#   0 无 open_red 且无未披露 false / 1 有 open_red / 2 披露文件自身陈旧或冲突
# 未披露的 false 一律记 UNDECLARED 并判红（fail-closed），open_red 不豁免。
# 落盘 JSON 里的 false 有四种语义（demonstration/diagnostic/table_cell/open_red），
# 一刀切「见 false 就红」会产生大量假警报而使本汇总器恒红，故必须逐点披露、不能猜。
# ---------------------------------------------------------------------------
set +e
python3 "$HERE/rollup.py"
rollup_rc=$?
set -e

if [ "$rollup_rc" -ne 0 ]; then
  echo ""
  echo "!! 判据 rollup 未通过（rollup.py 退出码 $rollup_rc）"
  echo "!! 各脚本本身不因门判红而失败，故此处必须显式拦下。"
  echo "!! **不得**据本轮结果宣称 audit_rework 子树全绿。"
  exit "$rollup_rc"
fi

echo "ALL DONE + 判据 rollup 全绿。k_corr 查表补实验（P3 单元）不在本单元复现清单内，见 results/audit_rework/p3_kcorr/ 固化读数。"
