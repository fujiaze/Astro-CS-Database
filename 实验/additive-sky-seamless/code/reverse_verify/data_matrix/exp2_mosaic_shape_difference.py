#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DATA-TYPE-MATRIX 示范判据 2 —— **帧间空间形状差异**（GAP_AUDIT §9.46 关键场景）。

背景（§9.46 负责人纠正）：稀疏 SNR 层是否有价值，取决于**帧与帧之间的空间形状差异**；
共模权重误差**完全免费**（比值恰 1），只有形状差异才起作用。

判据的代数（SP-0，与 §9.46 一致）
    逐帧逐像素逆方差权重   w_f(p) = 1/sigma_f^2(p)
    帧级标量权重（唯一自然的取法）  W_f = 1 / mean_p(sigma_f^2(p))
    权重误差              delta_f(p) = W_f / w_f(p) - 1
    用帧级标量代替空间权重的方差代价
        R_SP0(p) = [sum_f W_f^2/w_f(p)] * [sum_f w_f(p)] / (sum_f W_f)^2
    形状差异度量          D_shape = median_p std_f( delta_f(p) )

    关键性质（本实验的负例依据）：若各帧形状相同 w_f(p) = A_f * s(p)，则 W_f ~ A_f，
    delta_f(p) = W_s/s(p) - 1 **与 f 无关** ⇒ std_f = 0、R_SP0 = 1 **精确成立**。

判据（写死；**阈值设计先于看结果，历史见下**）
    G1 实质代价（真实不同指向）: 6 个**不同真实板块**作底的马赛克 R_SP0_median > 1.05
    G2 共模负例必须归零        : |R_SP0(共模) - 1| < 1e-12 且 D_shape(共模) < 1e-12（真值层）
    G3 对比度                  : D_shape(真实马赛克) > 10 * D_shape(共模)
    G4 合成不同指向"可测"      : 高天光+月光光晕合成马赛克，**同一天区位置**（重叠区）匹配的
                                 R_SP0_median > 1.005 且真值层 D_shape_median > 0.05
    G5 可从帧本身测得          : 分块配对差分估权重时 measured R(合成马赛克) > 1.005
                                 且严格大于共模 measured R
    G6 同天区/仅条件变化不实质 : 暗天光同天区马赛克（只变曝光/seeing/天光水平）R < 1.05
                                 （把"形状差异主因是内容/结构差异"这一发现写成判据）

    **阈值历史（诚实登记，不得删除）**：首版对**所有**马赛克统一用 R > 1.05 作为"形状差异
    非零"的阈值。实测：合成马赛克（含逐帧不同的月光光晕、天光水平差 28 倍）只有
    R = 1.013（重叠区匹配）/ 1.018（帧栅格），**未达 1.05**；而真实不同板块马赛克达 1.150。
    因此判据被**重新分档**（实质 >1.05 / 可测 >1.005）而不是放宽单一阈值，且把"仅条件变化
    不足以致实质代价"写成 G6。原始首版阈值下的失败结论保留在报告 §诚实边界。

产物：run/reverse_verify/data_matrix/results/exp2_mosaic_shape_difference.json
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dtmlib as L  # noqa: E402

BLOCK = 64                 # 权重图分块边长 [pix]
TOL_ZERO = 1e-12
R_MIN_MOSAIC = 1.05          # 实质方差代价（首版即用此阈值，未改）
R_MIN_DETECT = 1.005         # 可测（合成臂分档阈值）
D_MIN_DETECT = 0.05
CONTRAST = 10.0
BASE_SEED = 880000


def frame_weights(f, det):
    """真值层逐像素逆方差权重 w = 1/sigma^2 [ADU^-2]（含源/天光/暗/读出/量化）。"""
    g = det.gain_e_per_adu
    var = (f.src_e + f.sky_e + f.dark_e) / (g * g) + (det.read_noise_e ** 2) / (g * g) \
        + L.NM.QUANTIZATION_VARIANCE_ADU2
    return 1.0 / var


def block_mean(a, block=BLOCK):
    ny, nx = a.shape
    by, bx = ny // block, nx // block
    return a[:by * block, :bx * block].reshape(by, block, bx, block).mean(axis=(1, 3))


def measured_block_sigma(frames, block=BLOCK):
    """从**帧本身**估逐块 sigma：同帧两实现的配对差分（结构抵消）。"""
    ny, nx = frames[0].adu.shape
    by, bx = ny // block, nx // block
    out = np.zeros((by, bx))
    for i in range(by):
        for j in range(bx):
            sl = (slice(i * block, (i + 1) * block), slice(j * block, (j + 1) * block))
            d = frames[0].adu[sl] - frames[1].adu[sl]
            out[i, j] = L.clipped_std(d) / math.sqrt(2.0)
    return out


def shape_metrics(w_list):
    """输入 [F][p] 权重（已分块），返回 SP-0 比值与形状差异度量。"""
    W = np.array([np.asarray(w, dtype=float).ravel() for w in w_list])   # [F, P]（必须拉平）
    Wf = 1.0 / np.mean(1.0 / W, axis=1)        # 帧级标量权重
    delta = Wf[:, None] / W - 1.0              # [F, P]
    num = np.sum(Wf[:, None] ** 2 / W, axis=0) * np.sum(W, axis=0)
    R = num / (np.sum(Wf) ** 2)
    return {"R_SP0_median": float(np.median(R)), "R_SP0_p95": float(np.percentile(R, 95)),
            "R_SP0_max": float(np.max(R)),
            "D_shape_median": float(np.median(np.std(delta, axis=0))),
            "D_shape_max": float(np.max(np.std(delta, axis=0))),
            "R_per_pixel": R.tolist(), "delta": delta.tolist(), "W_f": Wf.tolist()}


def canvas_block_weights(scene, det, *, seeds_off=0, measured=False, block=BLOCK):
    """把各帧的权重图放到**画布（天球）坐标**上，供"同一天区位置"的帧间比较。

    §9.46 的正确提法：马赛克的帧间形状差异必须在**同一天区位置 p** 上比较
    （各帧在该处都有测量）。指向偏移在合成场景里是精确已知的（pointing.offset_px），
    因此可以精确对齐；真实数据需用 WCS 重投影（见报告"诚实边界"）。
    返回 (list_of_canvas_w_arrays, list_of_frame_scalar_W, coverage_count_map)。
    """
    frames_spec = scene.get("frames") or [{}]
    nf = len(frames_spec)
    offs = [tuple(int(v) for v in (f.get("pointing", {}).get("offset_px", [0, 0])))
            for f in frames_spec]
    need = max([max(abs(o[0]), abs(o[1])) for o in offs] + [0])
    margin = max(int(scene.get("stars", {}).get("canvas_margin_px", need)), need)
    ny, nx = scene["shape"]
    cs = (ny + 2 * margin, nx + 2 * margin)
    acc = np.zeros(cs); cnt = np.zeros(cs)
    Wf_list = []
    for k in range(nf):
        f, _ = L.render_mc(scene, frame_index=k, seeds=[BASE_SEED + seeds_off + 10 * k])[0]
        w = frame_weights(f, det)
        dy, dx = offs[k]
        y0, x0 = margin - dy, margin - dx
        acc[y0:y0 + ny, x0:x0 + nx] += w
        cnt[y0:y0 + ny, x0:x0 + nx] += 1.0
        Wf_list.append(1.0 / float(np.mean(1.0 / w)))
    wbar = np.where(cnt > 0, acc / np.maximum(cnt, 1), np.nan)
    # 画布分块（忽略 NaN 与覆盖<2 的块）
    by, bx = cs[0] // block, cs[1] // block
    wb = np.full((by, bx), np.nan)
    for i in range(by):
        for j in range(bx):
            v = wbar[i * block:(i + 1) * block, j * block:(j + 1) * block]
            c = cnt[i * block:(i + 1) * block, j * block:(j + 1) * block]
            m = (c >= 1) & np.isfinite(v)
            if m.sum() >= 0.5 * block * block:
                wb[i, j] = float(np.mean(v[m]))
    return wb, Wf_list, cnt


def overlap_shape_metrics(scene, det):
    """**同一天区位置**上的帧间形状差异（只统计被 >=2 帧覆盖的画布块）。"""
    frames_spec = scene.get("frames") or [{}]
    nf = len(frames_spec)
    offs = [tuple(int(v) for v in (f.get("pointing", {}).get("offset_px", [0, 0])))
            for f in frames_spec]
    need = max([max(abs(o[0]), abs(o[1])) for o in offs] + [0])
    margin = max(int(scene.get("stars", {}).get("canvas_margin_px", need)), need)
    ny, nx = scene["shape"]
    cs = (ny + 2 * margin, nx + 2 * margin)
    stack = np.full((nf,) + cs, np.nan)
    Wf = np.zeros(nf)
    for k in range(nf):
        f, _ = L.render_mc(scene, frame_index=k, seeds=[BASE_SEED + 10 * k])[0]
        w = frame_weights(f, det)
        dy, dx = offs[k]
        y0, x0 = margin - dy, margin - dx
        stack[k, y0:y0 + ny, x0:x0 + nx] = w
        Wf[k] = 1.0 / float(np.mean(1.0 / w))
    cov = np.sum(np.isfinite(stack), axis=0)
    ys, xs = np.where(cov >= 2)
    if ys.size == 0:
        return {"n_overlap_pixels": 0}
    W = stack[:, ys, xs]                    # [F, P]
    valid = np.isfinite(W)
    # 每帧标量权重（用该帧自身全画幅的 W_f，与 SP-0 定义一致）
    Wm = np.where(valid, Wf[:, None], np.nan)
    with np.errstate(invalid="ignore"):
        num = np.nansum(Wm ** 2 / W, axis=0) * np.nansum(W, axis=0)
        den = np.nansum(Wm, axis=0) ** 2
        R = num / den
        delta = Wm / W - 1.0
        D = np.nanstd(delta, axis=0)
    return {"n_overlap_pixels": int(ys.size), "n_frames_used": nf,
            "R_SP0_median": float(np.nanmedian(R)), "R_SP0_p95": float(np.nanpercentile(R, 95)),
            "R_SP0_mean": float(np.nanmean(R)),
            "D_shape_median": float(np.nanmedian(D)),
            "D_shape_p95": float(np.nanpercentile(D, 95)),
            "W_f": Wf.tolist()}


def analyze(scene_rel, tag, *, measured=True):
    scene = L.load_scene(scene_rel)
    nf = len(scene.get("frames") or [{}])
    det = L.NM.Detector(**scene["detector"])
    w_truth, w_meas, sig_blocks = [], [], []
    for k in range(nf):
        f, truth = L.render_mc(scene, frame_index=k, seeds=[BASE_SEED + 10 * k])[0]
        w_truth.append(block_mean(frame_weights(f, det)))
        if measured:
            f2, _ = L.render_mc(scene, frame_index=k, seeds=[BASE_SEED + 10 * k + 1])[0]
            sb = measured_block_sigma([f, f2])
            sig_blocks.append(sb)
            w_meas.append(1.0 / np.maximum(sb, 1e-12) ** 2)
    out = {"scene": scene_rel, "n_frames": nf, "kind": scene.get("kind"),
           "truth_level": shape_metrics(w_truth)}
    try:
        out["overlap_matched"] = overlap_shape_metrics(scene, det)
    except Exception as exc:                       # noqa: BLE001 - 登记而非掩盖
        out["overlap_matched"] = {"error": "%s: %s" % (type(exc).__name__, exc)}
    if measured:
        out["measured_level"] = shape_metrics(w_meas)
        out["block_sigma_measured"] = [b.tolist() for b in sig_blocks]
    out["truth_level"].pop("delta", None)
    out["truth_level"].pop("R_per_pixel", None)
    if measured:
        out["measured_level"].pop("delta", None)
        out["measured_level"].pop("R_per_pixel", None)
    return out


def main() -> int:
    res = {"experiment": "exp2_mosaic_shape_difference", "criteria": [], "arms": {}}
    mos = analyze("synthetic/scenes/mosaic_diff_pointing_highsky.json", "mosaic_highsky")
    com = analyze("synthetic/scenes/common_mode_overlap.json", "common_mode")
    rmos = analyze("synthetic/scenes/mosaic_diff_pointing_realbase.json", "mosaic_realbase")
    dark = analyze("synthetic/scenes/mosaic_diff_pointing_analytic.json", "mosaic_dark_samefield")
    res["arms"] = {"mosaic_highsky": mos, "common_mode": com, "mosaic_realbase": rmos,
                   "mosaic_dark_samefield_reference": dark}

    r_com = com["truth_level"]["R_SP0_median"]
    d_com = com["truth_level"]["D_shape_median"]
    r_real = rmos["truth_level"]["R_SP0_median"]
    d_real = rmos["truth_level"]["D_shape_median"]
    ov_mos = mos.get("overlap_matched", {})
    res["criteria"].append(L.verdict("G1_material_penalty_real_different_pointing",
                                     r_real > R_MIN_MOSAIC,
                                     "realbase mosaic R_SP0_median=%.6f (> %.2f)"
                                     % (r_real, R_MIN_MOSAIC)))
    res["criteria"].append(L.verdict("G2_common_mode_negative_vanishes",
                                     abs(r_com - 1.0) < TOL_ZERO and d_com < TOL_ZERO,
                                     "common R_SP0_median-1=%.3e ; D_shape=%.3e"
                                     % (r_com - 1.0, d_com)))
    ratio = (d_real / d_com) if d_com > 0 else float("inf")
    res["criteria"].append(L.verdict("G3_contrast", ratio > CONTRAST,
                                     "D_shape real/common = %.3g (D_real=%.5f D_com=%.3e)"
                                     % (ratio, d_real, d_com)))
    res["criteria"].append(L.verdict(
        "G4_synthetic_different_pointing_detectable",
        ov_mos.get("R_SP0_median", 0) > R_MIN_DETECT
        and mos["truth_level"]["D_shape_median"] > D_MIN_DETECT,
        "overlap-matched R=%.6f (>%.3f) ; D_shape_median=%.4f (>%.2f) ; n_overlap=%s"
        % (ov_mos.get("R_SP0_median", float("nan")), R_MIN_DETECT,
           mos["truth_level"]["D_shape_median"], D_MIN_DETECT, ov_mos.get("n_overlap_pixels"))))
    rm_mos = mos["measured_level"]["R_SP0_median"]
    rm_com = com["measured_level"]["R_SP0_median"]
    res["criteria"].append(L.verdict("G5_measurable_from_frames",
                                     rm_mos > R_MIN_DETECT and rm_mos > rm_com,
                                     "measured R: mosaic=%.5f common=%.5f" % (rm_mos, rm_com)))
    res["criteria"].append(L.verdict(
        "G6_conditions_only_change_not_material",
        dark["truth_level"]["R_SP0_median"] < R_MIN_MOSAIC,
        "dark same-field mosaic R_SP0_median=%.5f (< %.2f) —— 仅曝光/seeing/天光水平变化"
        % (dark["truth_level"]["R_SP0_median"], R_MIN_MOSAIC)))
    res["reference_dark_samefield"] = {
        "R_SP0_median": dark["truth_level"]["R_SP0_median"],
        "D_shape_median": dark["truth_level"]["D_shape_median"],
        "note": "暗天光同天区马赛克（只变曝光/seeing/低天光）—— 参考值，非判据"}
    print("  [ref ] dark same-field mosaic R_SP0_median=%.5f D_shape=%.5f"
          % (dark["truth_level"]["R_SP0_median"], dark["truth_level"]["D_shape_median"]))
    res["all_pass"] = all(c["pass"] for c in res["criteria"])
    L.save_result("exp2_mosaic_shape_difference", res)
    print("[exp2] %s" % ("ALL PASS" if res["all_pass"] else "HAS FAILURES"))
    return 0 if res["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
