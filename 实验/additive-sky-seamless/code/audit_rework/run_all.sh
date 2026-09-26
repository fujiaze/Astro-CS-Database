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

set -u
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

echo "ALL DONE. k_corr 查表补实验（P3 单元）不在本单元复现清单内，见 results/audit_rework/p3_kcorr/ 固化读数。"
