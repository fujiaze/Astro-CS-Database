#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-05 臂 E：**判据自检（能红能绿）+ 故障注入**。

每条判据都必须：(a) 在真实结果上给出判定；(b) 在**故障注入**下翻红。
故障注入在进程内完成（不改任何产物文件），注入后立即恢复。

固定 seed：SEED = 20260926。不运行任何 ACSD 可执行文件。
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp05_common as X  # noqa: E402
import e4_weight as E4    # noqa: E402

SEED = X.SEED
DELTA = 64


def load(p: Path) -> Dict[str, Any]:
    if not p.exists():
        raise SystemExit("缺少前置产物 %s；请先跑 e1/e2/e3/e4" % p)
    return json.loads(p.read_text(encoding="utf-8"))


# ===========================================================================
# 已知噪声场装置：per-patch 真值**解析已知**，且与被检验量不同源
# ---------------------------------------------------------------------------
# G08-05 R2 复核第 1 条的整改基础。原装置用 toy_frame（有结构），其逐 patch
# 真值无法解析给出，于是判据只能拿 sp/sp2 互相当参照 —— 那就是「est 与 true 是
# 同一个数组」的恒等式（恒真门三型①）。这里换成**逐块恒定**的已知噪声场：
#   img_k(p) = N(0,1) * v_k[p]，v_k 在 DELTA×DELTA 块上取常数
# ⇒ 该块内每个像素的噪声标准差**精确**等于 v_k[p]，无结构污染、无聚合歧义，
#   故真值 SNR 场 T_k = 1/v_k 由**生成参数**解析给出。
# 关键：T_k 从不经过 recon_absolute / represent_absolute / effective_sigma_relative /
# cross_frame_ratio_dev 的 est 臂，因此**不是待检验量的逆函数**，也不是其导出量。
KNOWN_NOISE_SHAPE = (512, 512)
# 跨帧相对一致性容差。推导：每 patch 裁剪 RMS 的相对标准误 ~1/sqrt(2*4096)=1.10e-2，
# 经 64 个 patch 取中位降到 1.2533*1.10e-2/8=1.73e-3，比值再吃 sqrt(2) => 2.45e-3；
# 实测 60 组种子对 sd=3.03e-3、max|dev|=6.39e-3。取 2.5e-2 ≈ 8.2×解析 3σ、3.9×实测极值。
TOL_TRUTH_REL = 2.5e-2
# 逐帧绝对标度容差。受限于 sigma_frame/sigma_patch_raw 自身的 MAD 裁剪偏差
# （平坦场实测 |median(SNR̂)/median(T)-1| = 1.49e-2），取 1e-1 保留 6.7× 裕度：
# 足以抓 sp×1000 / 量纲颠倒这类粗尺度错，同时不被估计器自身偏差误伤。
TOL_TRUTH_LEVEL = 1.0e-1
# 逐 patch 形态容差：median(|SNR̂_ij/T_ij − 1|)。这是唯一能看见**聚合/布局**类缺陷
# （把逐 patch 值换成 patch 均值、把 block 排布转置）的量级：跨帧比值与电平都是
# 中位/比值型统计量，对「整场被压平」不敏感。
# 正确实现下该值由裁剪 RMS 的抽样散布主导，实测恒为 1.7546e-2（p90 = 3.1646e-2，
# 与帧型无关）；取 5e-2 留 2.85× 裕度。
TOL_P50_PATTERN = 5.0e-2
# c_eff 对解析聚合失配 c_agg=RMS(v)/median(v) 的**粗**独立对拍容差。
# 实测偏差 flat +7.5e-4、ramp -8.1e-2、stripe -5.2e-2（全部来自 MAD 裁剪偏差，
# c_agg 闭式本身不含该偏差），故只能取 2e-1 才能不误伤正确实现。
# 它的作用是抓 frame_common_factor 的**粗**错（×2 / 取倒数 / 漏乘 median 项），
# 不是精确判据 —— 精确判据由上面的解析真值门承担。
TOL_C_AGG = 0.20
# 相对臂的**可判性前提**：设计的 c 离散必须显著非零，否则相对臂等于没被检验
# （此时若仍判绿就是覆盖面被高估）。不满足即 fail-closed 判红。
C_SPREAD_MIN = 0.05


def known_noise_blockmap(kind: str, b: int) -> np.ndarray:
    """逐块恒定的噪声标准差图（生成参数，**解析真值**）。"""
    ii, jj = np.meshgrid(np.arange(b), np.arange(b), indexing="ij")
    if kind == "flat":
        return np.full((b, b), 20.0)
    if kind == "ramp":
        return 20.0 * (1.0 + 2.5 * ii / (b - 1))
    if kind == "stripe":
        return 20.0 * (1.0 + 2.0 * (jj % 2))
    if kind == "skew":
        # 25% 的 patch 亮 10 倍：让「整场压平」类缺陷在中位数上也无法藏身
        return 20.0 * (1.0 + 9.0 * ((ii + 2 * jj) % 4 == 0))
    raise ValueError("unknown known-noise blockmap %r" % kind)


def known_noise_frame(kind: str, seed: int, shape=KNOWN_NOISE_SHAPE,
                      delta: int = DELTA):
    """返回 (逐 patch 解析真值 v, 实测 patch sigma, 实测帧级 sigma)。"""
    b = shape[0] // delta
    v = known_noise_blockmap(kind, b)
    full = np.repeat(np.repeat(v, delta, axis=0), delta, axis=1)
    img = np.random.default_rng(seed).normal(0.0, 1.0, size=shape) * full
    return v, X.sigma_patch_raw(img, delta), X.sigma_frame(img)


def c_agg_closed(v: np.ndarray) -> float:
    """解析聚合失配 c_agg = RMS(v)/median(v)（帧级平方聚合 vs 控制点中位聚合）。"""
    f = np.asarray(v, dtype=np.float64).ravel()
    return float(np.sqrt(np.mean(f * f)) / np.median(f))


def _xframe(est, tru, ref: int = 0) -> float:
    """跨帧比值的中位（est 与 true 必须来自**不同源**的两条路径）。"""
    return float(X.cross_frame_ratio_dev(est, tru, ref=ref)[
        "frame_%d_over_%d" % (1, ref)]["median_ratio"])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp05_e5_gates.json"))
    a = ap.parse_args()
    t0 = time.time()
    e1 = load(X.RESULTS / "exp05_e1_analytic.json")
    e2 = load(X.RESULTS / "exp05_e2_hst.json")
    e3 = load(X.RESULTS / "exp05_e3_real.json")
    e4 = load(X.RESULTS / "exp05_e4_weight.json")
    gates: List[Dict[str, Any]] = []

    # ---- G1 共模相消定理（单帧归一化加权 + 尺度等变）----
    d1 = e4["D1_theorem"]
    eq_max = max(d1["operator_scale_equivariance_max_rel"].values())
    X.gate(gates, "G1a_operator_scale_equivariance", eq_max <= 1e-12,
           "三个正齐次算子的 R[a*v]==a*R[v] 最大相对偏差", eq_max, "<= 1e-12")
    X.gate(gates, "G1b_single_frame_weighted_mean_invariant",
           d1["single_frame_mean_rel_diff"] <= 1e-12,
           "单帧归一化加权均值在两种表示下的相对差",
           d1["single_frame_mean_rel_diff"], "<= 1e-12")
    X.gate(gates, "G1c_weight_efficiency_invariant",
           d1["weight_efficiency_rel_diff"] <= 1e-12,
           "权重效率 E 在两种表示下的绝对差", d1["weight_efficiency_rel_diff"], "<= 1e-12")
    X.gate(gates, "G1d_exp1_ratio_1.000000_is_theorem",
           d1["weight_efficiency_rel_diff"] <= 1e-12,
           "EXP-1 实测 ratio=1.000000 是共模相消定理的必然结果（非经验发现）",
           d1["weight_efficiency_rel_diff"], "<= 1e-12")

    # ---- G2 反例：带数据无关先验均值的算子不正齐次 ----
    d3 = e4["D3_counterexample"]
    bad = max(v["max_rel"] for k, v in d3["non_common_mode_residual"].items()
              if "fixed_mean" in k)
    good = max(v["max_rel"] for k, v in d3["non_common_mode_residual"].items()
               if "fixed_mean" not in k)
    X.gate(gates, "G2a_fixed_prior_mean_breaks_common_mode", bad > 1e-3,
           "带固定先验均值的 kriging 重建的非共模残差（定理条件不成立）", bad, "> 1e-3")
    X.gate(gates, "G2b_homogeneous_operator_has_no_residual", good <= 1e-12,
           "正齐次算子的非共模残差", good, "<= 1e-12")

    # ---- G3 非退化负例：平坦 sigma + 无结构 ⇒ 两表示必须一致（DEGENERATE）----
    neg = [s for s in e1["scenarios"] if s["scenario"] == "flat_no_struct"][0]
    gap = abs(neg["rel_sigma_ratio_R0"] / neg["abs_sigma_ratio_R0"] - 1.0)
    thr = max(3.0 * neg["mc_sd_c_sigma_R0"] / max(neg["c_sigma_R0"], 1e-12), 1e-9)
    X.gate(gates, "G3_negative_control_degenerate", gap <= max(thr, 1e-3),
           "真值无效应（平坦 sigma + 无结构）时两表示的重建差必须归零（判 DEGENERATE）",
           {"gap": gap, "threshold": max(thr, 1e-3)}, "<= max(3*sd,1e-3)")

    # ---- G4 活体检查：真实数据上 c 必须显著 != 1 ----
    c_m5 = [p for p in e3["panels"] if p["panel"] == "M5"]
    c_m1 = [p for p in e3["panels"] if p["panel"] == "M1"]
    live_val = min(min(p["c_sigma_per_frame"]) for p in (c_m5 or c_m1))
    X.gate(gates, "G4_anchor_discrepancy_is_live_on_real_data", abs(live_val - 1.0) > 0.1,
           "真实 M42 帧上 |c-1| 必须显著（否则判据不活）", live_val, "|c-1| > 0.1")

    # =======================================================================
    # G5/G6 重建块（G08-05 R2 第 1 条：绝对臂换非同源参照 + 相对臂补解析期望）
    # -----------------------------------------------------------------------
    # 三帧「已知噪声场」装置：逐 patch 真值解析已知，T_k = 1/v_k，与被检验量不同源。
    v1, sp1, sf1 = known_noise_frame("stripe", SEED + 11)
    v2, sp2, sf2 = known_noise_frame("ramp", SEED + 12)
    vf, spf, sff = known_noise_frame("flat", SEED + 13)
    vs, sps, sfs = known_noise_frame("skew", SEED + 14)
    T1, T2, Tf, Ts = 1.0 / v1, 1.0 / v2, 1.0 / vf, 1.0 / vs
    c1 = X.frame_common_factor(sp1, sf1)
    c2 = X.frame_common_factor(sp2, sf2)
    # 相对臂走**两条不同的生产实现**，二者都必须对上同一个预测：
    #   (a) effective_sigma_relative -> 等效 sigma 场（sigma 空间）；
    #   (b) recon_relative           -> 落盘 rho 乘回帧级标量（SNR 空间）。
    # 两条路径在数学上等价，但**代码路径不同**：只测一条会让另一条成为死代码
    # （实测：represent_relative 丢掉中位归一时，effective_sigma_relative 一路仍全绿）。
    rel_snr = lambda s, sf: X.recon_absolute(X.effective_sigma_relative(s, sf))
    rel_rho = lambda s, sf: X.recon_relative(s, sf)

    # ---- G5a 绝对表示必须落在**解析真值**的绝对标度上 ----
    # 跨帧比值 est=recon_absolute(sp_k)、true=T_k（生成参数）：真值不是 est 的逆，
    # 也不是 est 的导出量。原判据把 sp 同时当 est 与 true，比值恒等于 1（恒真门①）。
    abs_ratio = _xframe([X.recon_absolute(sp1), X.recon_absolute(sp2)], [T1, T2])
    abs_dev = abs(abs_ratio - 1.0)
    # 逐帧绝对标度：跨帧比值**结构上**看不见「各帧同乘一个因子」的共模误差
    # （分子分母同除），该盲区只能由逐帧电平对解析真值的比对来关。
    lvl_dev = abs(float(np.median(X.recon_absolute(spf))) / float(np.median(Tf)) - 1.0)
    # 逐 patch 形态：唯一能看见「聚合/布局」类缺陷（中位与比值统计量对整场压平不敏感）。
    p50_pattern = {k: float(np.median(np.abs(X.recon_absolute(sp).ravel()
                                             / T.ravel() - 1.0)))
                   for k, (sp, T) in (("flat", (spf, Tf)), ("ramp", (sp2, T2)),
                                      ("stripe", (sp1, T1)), ("skew", (sps, Ts)))}
    p50_worst = max(p50_pattern.values())
    X.gate(gates, "G5a_absolute_arm_matches_analytic_truth",
           bool(abs_dev <= TOL_TRUTH_REL and lvl_dev <= TOL_TRUTH_LEVEL
                and p50_worst <= TOL_P50_PATTERN),
           "绝对表示的跨帧比值必须等于解析真值比值 1（|dev|<=%.1e），逐帧电平必须等于"
           "解析真值电平（|dev|<=%.1e，用于关闭「各帧同乘因子」的共模盲区），"
           "且逐 patch 形态必须吻合（median|SNR̂/T−1|<=%.1e，用于抓聚合/布局类缺陷）。"
           "实测 跨帧=%.3e、电平=%.3e、形态最差=%.3e"
           % (TOL_TRUTH_REL, TOL_TRUTH_LEVEL, TOL_P50_PATTERN, abs_dev, lvl_dev, p50_worst),
           {"abs_median_ratio": abs_ratio, "abs_dev_from_1": abs_dev,
            "level_dev_flat_frame": lvl_dev, "p50_pattern_by_frame": p50_pattern,
            "p50_pattern_worst": p50_worst,
            "analytic_truth_note": "T_k = 1/v_k，v_k 为生成参数（逐块恒定噪声标准差），"
                                   "不经过任何被检验函数"},
           "|R−1|<=%.1e 且 |level−1|<=%.1e 且 p50<=%1e"
           % (TOL_TRUTH_REL, TOL_TRUTH_LEVEL, TOL_P50_PATTERN))

    # ---- G5b 相对表示被 c 离散污染，且必须**恰好**是 c1/c2（两侧判据）----
    # 原判据只有下界 rel_dev > 0.05 ⇒ 污染越重越绿（实测注入：逐帧标量 x2 使
    # rel_dev 0.228→0.614、sp2 x1000 使 0.228→0.999，两次都判绿）。现改为**双侧**
    # 等式判据：相对臂的跨帧比值必须等于**解析/实测预测的 c 离散**，超出即判红，
    # 无论偏大还是偏小。
    rel_ratio = _xframe([rel_snr(sp1, sf1), rel_snr(sp2, sf2)], [T1, T2])
    rel_ratio_rho = _xframe([rel_rho(sp1, sf1), rel_rho(sp2, sf2)], [T1, T2])
    rel_pred = float(c1 / c2)
    rel_dev = abs(rel_ratio - rel_pred)
    rel_dev_rho = abs(rel_ratio_rho - rel_pred)
    c_spread = abs(rel_pred - 1.0)
    # 可判性前提：设计的 c 离散必须显著非零，否则相对臂等于没被检验。
    X.gate(gates, "G5b_relative_arm_polluted_by_exactly_c_spread",
           bool(rel_dev <= TOL_TRUTH_REL and rel_dev_rho <= TOL_TRUTH_REL
                and c_spread >= C_SPREAD_MIN),
           "相对表示的跨帧比值必须等于预测的 c 离散 c1/c2（**双侧** |dev|<=%.1e，"
           "effective_sigma_relative 与 recon_relative 两条实现都必须对上）；"
           "且设计的 c 离散须 >=%.2f（可判性前提，不满足即 fail-closed）。"
           "实测预测 c1/c2=%.6f、两路实测=%.6f / %.6f"
           % (TOL_TRUTH_REL, C_SPREAD_MIN, rel_pred, rel_ratio, rel_ratio_rho),
           {"c1": c1, "c2": c2, "predicted_c_ratio": rel_pred,
            "measured_ratio_eff_sigma": rel_ratio, "abs_dev_eff_sigma": rel_dev,
            "measured_ratio_recon_relative": rel_ratio_rho,
            "abs_dev_recon_relative": rel_dev_rho,
            "c_spread": c_spread, "c_spread_min": C_SPREAD_MIN},
           "两路 |measured−c1/c2|<=%.1e 且 |c1/c2−1|>=%.2f" % (TOL_TRUTH_REL, C_SPREAD_MIN))

    # ---- G5c 帧级标量故障必须**只**落在相对臂，且绝对臂仍锁在解析真值上 ----
    # 故障取**非对称**注入（只把帧 2 的帧级标量 x2）：跨帧比值对**共模**乘性因子
    # 结构性相消（est/true 同除），非对称注入才可观测——这是判据的固有分辨率下限，
    # 不是缺陷。注入后重算 c'，相对臂必须跟上**新的**预测 c1/c2'，绝对臂必须仍对
    # 解析真值成立（若绝对臂被误接进 sf，这里立刻偏离 1 判红）。
    sf2_fault = 2.0 * sf2
    c2_fault = X.frame_common_factor(sp2, sf2_fault)
    rel_fault = _xframe([rel_snr(sp1, sf1), rel_snr(sp2, sf2_fault)], [T1, T2])
    rel_fault_pred = float(c1 / c2_fault)
    abs_fault = _xframe([X.recon_absolute(sp1), X.recon_absolute(sp2)], [T1, T2])
    X.gate(gates, "G5c_frame_scalar_fault_localises_to_relative_arm",
           bool(abs(rel_fault - rel_fault_pred) <= TOL_TRUTH_REL
                and abs(abs_fault - 1.0) <= TOL_TRUTH_REL),
           "只把帧 2 的帧级标量 x2（非对称注入）后：相对臂跨帧比值必须等于**新**预测 "
           "c1/c2'=|dev|<=%.1e，且绝对臂仍须等于解析真值 1（<=%1e ⇒ 帧级标量没渗进"
           "绝对表示）。实测预测=%.6f、实测=%.6f、绝对臂=%.9f"
           % (TOL_TRUTH_REL, TOL_TRUTH_REL, rel_fault_pred, rel_fault, abs_fault),
           {"c2_fault": c2_fault, "predicted_c_ratio_fault": rel_fault_pred,
            "measured_ratio_fault": rel_fault,
            "abs_dev_vs_prediction": abs(rel_fault - rel_fault_pred),
            "abs_arm_ratio": abs_fault, "abs_dev_from_1": abs(abs_fault - 1.0),
            "common_factor_note": "跨帧比值对两帧**共模**乘性因子结构性相消，故注入必须非对称"},
           "|rel−c1/c2'|<=%.1e 且 |abs−1|<=%.1e" % (TOL_TRUTH_REL, TOL_TRUTH_REL))

    # ---- G6 跨帧标量交换：两个方向都必须等于各自的解析预测（双侧）----
    # 注意：交换臂的预测不是 c2/c1，而是**交叉**因子之比
    # frame_common_factor(sp1,sf2)/frame_common_factor(sp2,sf1)（实测 naive 预测会错 1.008）。
    cx12 = X.frame_common_factor(sp1, sf2)
    cx21 = X.frame_common_factor(sp2, sf1)
    xf_ok = _xframe([rel_snr(sp1, sf1), rel_snr(sp2, sf2)], [T1, T2])
    xf_bad = _xframe([rel_snr(sp1, sf2), rel_snr(sp2, sf1)], [T1, T2])
    xf_ok_dev = abs(xf_ok - float(c1 / c2))
    xf_bad_dev = abs(xf_bad - float(cx12 / cx21))
    X.gate(gates, "G6_cross_frame_scalar_swap_matches_cross_prediction",
           bool(xf_ok_dev <= TOL_TRUTH_REL and xf_bad_dev <= TOL_TRUTH_REL),
           "交换两帧的帧级标量后，相对表示的跨帧比值必须等于**交叉**因子预测 "
           "c(sp1,sf2)/c(sp2,sf1)（双侧，<=%1e）；naive 的 c2/c1 预测实测会错 1.008。"
           "实测 正确臂 dev=%.3e、交换臂 dev=%.3e"
           % (TOL_TRUTH_REL, xf_ok_dev, xf_bad_dev),
           {"xf_ok": xf_ok, "xf_ok_pred_c1_over_c2": float(c1 / c2),
            "xf_ok_dev": xf_ok_dev,
            "xf_swapped": xf_bad, "xf_swapped_pred_cross": float(cx12 / cx21),
            "xf_swapped_dev": xf_bad_dev,
            "naive_c2_over_c1": float(c2 / c1)},
           "两臂 |dev|<=%.1e" % TOL_TRUTH_REL)

    # ---- G7 电平度量的真值无效应归零：改锚到**解析真值**（原判据 est==true 是恒等式）----
    lv = X.level_dev(X.recon_absolute(spf), Tf)
    lvl2 = abs(float(lv["median_ratio"]) - 1.0)
    # 非退化对照：把 SNR 场整体压到 1/2（等价于噪声 sigma 翻倍）后，同一度量的
    # **带符号** SNR 偏差必须显著为**正**（SNR 被低估）。用带符号判据而不是 |dev|
    # 是刻意的：level_dev 的 snr_rel_dev 若被写成 (1−1/median) 符号反转，绝对值
    # 判据看不见，带符号判据能看见（实测注入 V10 即被此条抓住）。
    lv_off = X.level_dev(X.recon_absolute(spf) * 0.5, Tf)
    lvl_off_signed = float(lv_off["snr_rel_dev"])
    X.gate(gates, "G7_level_metric_zero_on_truth_and_live_on_injected_scale",
           bool(lvl2 <= TOL_TRUTH_REL and lvl_off_signed > 0.5),
           "电平度量在「估计 == **解析真值**」时必须归零（|median_ratio−1|<=%.1e），"
           "且把 SNR 场压到 1/2 后其**带符号** SNR 偏差必须显著为正（>0.5，"
           "SNR 被低估；用绝对值判据则符号反转缺陷不可见）。实测 %.3e / %.4f"
           % (TOL_TRUTH_REL, lvl2, lvl_off_signed),
           {"level_dev_on_truth": lvl2,
            "snr_rel_dev_on_injected_half": lvl_off_signed,
            "injected_expectation_note": "SNR 压到 1/2 ⇒ snr_rel_dev = 1/median_ratio − 1 > 0",
            "truth_note": "真值取生成参数 T_f = 1/v_f，不是 est 自身（原判据 est==true "
                          "使 median_ratio 恒等于 1，属恒等式）"},
           "|median_ratio−1|<=%.1e 且 注入档 snr_rel_dev>0.5" % TOL_TRUTH_REL)

    # =======================================================================
    # 代数恒等式登记（**不计入门数**）：G08-05 R2 第 5 条 N1/N2 及自查新发现
    # -----------------------------------------------------------------------
    # 下列五条在任意输入、任意帧级标量（含 x1000 与 x1e-9）下都是**逐位/1 ulp 恒等式**
    # （实测见交付件第 5 节），保留在门表里只是充数，按标准 06 §4 移出并写明原因。
    # 它们**验证定义自洽**，不构成实现正确性的证据，不得被引用。
    #   G5a_old  a0/a1 = recon_absolute(sp) 同一函数同一实参两次调用 ⇒ 差恒 0。
    #           （且原注释声称「帧级标量 x2」，但 a1 根本没乘 sf*2.0 —— 注释与代码不符。）
    #   G5b_old  recon_relative(sp,2sf)/recon_relative(sp,sf) = sf/(2sf) = 1/2 恒成立。
    #   G5c_old  recon_relative/recon_absolute = 1/(sf*median(1/sp)) 是**标量常数**
    #           ⇒ std/mean 恒 0、median*c_eff 恒 1。
    #   G7_old   level_dev(sp,sp) ⇒ median_ratio 恒 1、snr_rel_dev 恒 0。
    #   G7b_old  level_dev(1.5sp,sp) ⇒ snr_rel_dev 恒等于 1/1.5-1。
    #   G8_old   recon_absolute(flat) 与 recon_relative(flat,flat) 在 c==1 时恒逐位相同
    #           （对任意 flat 值、任意 c 恒成立）。
    _old_a0 = X.recon_absolute(sp1)
    _old_a1 = X.recon_absolute(sp1)
    _old_r0 = X.recon_relative(sp1, sf1)
    _old_r1 = X.recon_relative(sp1, sf1 * 2.0)
    _old_ratio = _old_r0 / _old_a0
    _old_c = X.frame_common_factor(sp1, sf1)
    _old_flat = np.full((8, 8), 20.0)
    identity_checks = {
        "note": ("代数恒等式自检，**不计入门数**（G08-05 R2 第 5 条 N1/N2 + 自查新发现）。"
                 "每条都对任意输入、任意帧级标量恒成立，实测残差 0 或 1 ulp；"
                 "保留仅为确认定义自洽，不得作为实现正确性的证据。"),
        "G5a_old_same_call_twice_max_reldiff": float(
            np.max(np.abs(_old_a1 - _old_a0) / np.maximum(np.abs(_old_a0), 1e-300))),
        "G5b_old_frame_scalar_halves_exactly": float(
            np.median(np.abs(_old_r1 / _old_r0 - 0.5))),
        "G5c_old_rel_over_abs_is_constant": float(np.std(_old_ratio) / np.mean(_old_ratio)),
        "G5c_old_median_times_c_minus_1": abs(float(np.median(_old_ratio)) * _old_c - 1.0),
        "G7_old_level_dev_identity_snr_rel_dev": abs(X.level_dev(sp1, sp1)["snr_rel_dev"]),
        "G7b_old_injected_offset_residual": abs(
            X.level_dev(sp1 * 1.5, sp1)["snr_rel_dev"] - (1.0 / 1.5 - 1.0)),
        "G8_old_c_eq_1_bitwise_max_abs_diff": float(np.max(np.abs(
            X.recon_absolute(_old_flat) - X.recon_relative(_old_flat, 20.0)))),
    }

    # ---- G9 之后的门保持不变；G5d 的二阶歧义在解析真值装置上更锋利，予以保留 ----
    c_apx1 = X.frame_common_factor_approx(sp1, sf1)
    amb = abs(c_apx1 / c1 - 1.0)
    X.gate(gates, "G5d_sigma_space_vs_snr_space_median_ambiguity",
           amb > 0.0,
           "median(1/sigma)*median(sigma)-1：相对表示的锚点口径在 sigma/SNR 空间不等价（二阶项）",
           {"c_exact": c1, "c_approx": c_apx1, "rel_diff": amb}, "> 0")

    # ---- c_eff 对解析聚合失配的**粗**独立对拍（抓 frame_common_factor 的粗错）----
    cagg_devs = {k: abs(float(X.frame_common_factor(sp, sf)) / c_agg_closed(v) - 1.0)
                 for k, (v, sp, sf) in (("stripe", (v1, sp1, sf1)), ("ramp", (v2, sp2, sf2)),
                                        ("flat", (vf, spf, sff)))}
    cagg_worst = max(cagg_devs.values())
    X.gate(gates, "G5e_common_factor_gross_check_vs_closed_form",
           cagg_worst <= TOL_C_AGG,
           "c_eff 与解析聚合失配 c_agg=RMS(v)/median(v) 的**粗**对拍（<=%1e）。容差由"
           "sigma_frame 自身的 MAD 裁剪偏差决定（实测偏差 -5.2e-2~-8.1e-2），"
           "故只能抓粗错（x2 / 取倒数 / 漏乘 median 项），不是精确判据。"
           % TOL_C_AGG,
           {"rel_dev_by_frame": cagg_devs, "worst": cagg_worst,
            "c_agg_closed": {"stripe": c_agg_closed(v1), "ramp": c_agg_closed(v2),
                             "flat": c_agg_closed(vf)},
            "tolerance": TOL_C_AGG}, "<= %.1e" % TOL_C_AGG)

    # ---- G9 跨帧 c 离散 ⇒ E > 0（严格），且闭式与数值一致 ----
    e_case = [c for c in e4["D2_cross_frame"]["cases"] if c["case"] == "M5"]
    if e_case:
        pairs = e_case[0]["pairs"]
        max_d = max(p["abs_dev"] for p in pairs)
        X.gate(gates, "G9a_cross_frame_E_closed_form_matches", max_d <= 1e-12,
               "K=2 闭式 E=2(c1^4+c2^4)/(c1^2+c2^2)^2-1 与数值 E 的最大差",
               max_d, "<= 1e-12")
        X.gate(gates, "G9b_cross_frame_E_positive_when_c_differs",
               e_case[0]["E"] > 0.0, "跨帧 c 不同 ⇒ 权重效率损失 E 必须 > 0",
               e_case[0]["E"], "> 0")

    # ---- G10 组合 SNR 的电平偏差：共模项 >> 跨帧项（定量分离）----
    if e_case:
        cc = e_case[0]
        X.gate(gates, "G10_combined_snr_level_bias_dominated_by_common_mode",
               abs(cc["combined_snr_dev_common_only"])
               > 10.0 * abs(cc["combined_snr_dev_cross_frame_only"]),
               "组合 SNR 电平偏差的共模项必须远大于跨帧项（决定正确性的是电平，不是权重）",
               {"common": cc["combined_snr_dev_common_only"],
                "xframe": cc["combined_snr_dev_cross_frame_only"]}, "common > 10*xframe")

    # ---- G11 三臂一致性：绝对表示的电平偏差必须远小于相对表示 ----
    for arm, key_abs, key_rel in (("e1_analytic", "abs_snr_dev_R0", "rel_snr_dev_R0"),):
        rows = [s for s in e1["scenarios"] if s["scenario"] in ("flat_with_struct",
                                                                "vary_with_struct")]
        for s in rows:
            X.gate(gates, "G11_abs_closer_than_rel[%s]" % s["scenario"],
                   abs(s[key_abs]) < abs(s[key_rel]) / 3.0,
                   "绝对表示电平偏差必须显著小于相对表示",
                   {"abs": s[key_abs], "rel": s[key_rel]}, "|abs| < |rel|/3")
    for s in e2["scenarios"]:
        if s["scenario"] == "hst_null":
            continue
        X.gate(gates, "G11_abs_closer_than_rel[%s]" % s["scenario"],
               abs(s["abs_snr_dev_R0"]) < abs(s["rel_snr_dev_R0"]) / 3.0,
               "绝对表示电平偏差必须显著小于相对表示",
               {"abs": s["abs_snr_dev_R0"], "rel": s["rel_snr_dev_R0"]}, "|abs| < |rel|/3")
    for p in e3["panels"]:
        worst_abs = max(abs(x) for x in p["abs_snr_dev_per_frame"])
        worst_rel = max(abs(x) for x in p["rel_snr_dev_per_frame"])
        X.gate(gates, "G11_abs_closer_than_rel[real_%s]" % p["panel"],
               worst_abs < worst_rel / 3.0,
               "真实数据上绝对表示电平偏差必须显著小于相对表示",
               {"abs": worst_abs, "rel": worst_rel}, "|abs| < |rel|/3")

    out = {"meta": {"seed": SEED, "n_gates": len(gates),
                    "all_pass": X.all_pass(gates),
                    "n_algebraic_identity_checks": len(identity_checks) - 1,
                    "elapsed_s": time.time() - t0,
                    "supersedes": "results/exp05_e5_gates.json（旧存档，判决已过期，见下）"},
           "gates": gates,
           "algebraic_identity_checks": identity_checks,
           "stale_archive_notice": {
               "path": "results/exp05_e5_gates.json",
               "status": "EXPIRED — 判决已过期，不得作为证据引用",
               "reason": ("该存档的 meta.n_gates=28 / meta.all_pass=true 描述的是**当前代码里"
                          "已不存在的门集合**。存档中 7 条纯恒等门（G5a/G5b/G5c/G6a/G7/G7b/G8，"
                          "读数 0 或逐位恒等）已移入本文件的 identity_checks，不再计入门数；"
                          "另有 6 条真门（G5a/G5b/G5c/G5e/G6/G7 的新名）在本代码里却不在存档里。"),
               "obsolete_gate_names": [
                   "G5a_frame_scalar_fault_leaves_absolute_bitwise",
                   "G5b_frame_scalar_fault_moves_relative_linearly",
                   "G5c_relative_equals_absolute_times_common_factor",
                   "G6a_abs_cross_frame_immune_to_frame_scalar_swap",
                   "G6b_rel_cross_frame_corrupted_by_c_spread",
                   "G7_level_metric_zero_on_identity",
                   "G7b_level_metric_live_on_injected_offset",
                   "G8_degenerate_bitwise_identity"],
               "remediation": ("由前台统一重跑本脚本覆盖生成新存档；本单不得自行覆写 results/。"
                               "在新存档产生前，引用本单元判决一律以本文件的门定义为准，"
                               "不得引用旧存档的 all_pass。")}}
    for g in gates:
        print("%-4s %-52s %s" % (g["verdict"], g["gate"],
                                 json.dumps(g["value"], ensure_ascii=False)[:90]))
    print("ALL_PASS =", out["meta"]["all_pass"],
          "| identity_checks (not counted) =", out["meta"]["n_algebraic_identity_checks"])
    X.save_json(a.out, out)
    print("wrote", a.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
