#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""M16-SCENE 判据 exp_variance_closure：合成帧逐像素方差闭合（含**平场乘性响应**显式分解）。

CLI::

    export TMPDIR=/dev/shm/astrocs_m16
    python3 reverse_verify/experiments/m16_scene/exp_variance_closure.py

判据
----
C1  关掉平场（prnu=0, low_order=0, vignette=0）时，配对差分方差必须闭合到解析预测
    Var = lam/g^2 + RN^2/g^2 + 1/12（|rel_dev| <= 5%）。
    **已登记的估计量偏差**：实测配对差分方差系统性偏低 ~2%（同块样本方差与 C4 的
    精确逐像素闭合都只偏 +0.1~0.3%）。归因：ADU 取整 —— 增益 g=1.5 e-/ADU 时
    量化台阶 0.667 ADU，round 使相邻像素的量化误差出现微弱负相关，
    配对差分估计量 E[(a_{i+1}-a_i)^2]/2 = Var(a) - Cov(a_i,a_{i+1}) 因而偏低。
    这是**估计量**的性质，不是噪声链缺陷；精确判据以 C4 为准。
C2  打开平场后，方差增量必须等于 lam^2 * Var(m_hf) / g^2
    （m_hf = 平场图的高频分量；配对差分估计量只看得到高频）。
    **M16-SCENE-FIX-001 订正（预测量子式，非容差）**：m_hf 的方差必须用**配对差分可见**的
    定义 Var_pair(m) = [<（Δ_x m)^2> + <（Δ_y m)^2>]/4 —— 与估计量 pair_var() **同一个算子**。
    旧实现用块内总方差 Var(m − mean) 作代理：块内 tilt+vignette 的**平滑分量**占 33–36%
    （256^2 块），该分量对配对差分**不可见**，故代理会把预测抬高 ~1.6x。
    修复前块选择恰好落在平滑份额≈0 的块上（比值 1.015）⇒ 代理缺陷被掩盖；
    x t 缺陷修复后真实底进入期望面，块选择改变（左边缘块，平滑份额 0.361）⇒ 暴露为 z=−2.64。
    订正后 z=−0.30。**容差 3*sqrt(2)*sigma_v 与 5% 判据一字未改。**
    判据用**估计量的抽样误差**定容差：配对差分方差估计量的标准差
    σ(V̂) = V̂·sqrt(8/N)（N = 配对样本数），故
    |ΔV_meas − ΔV_pred| <= 3·sqrt(2)·σ(V̂)。**不用固定百分比** —— 否则暗帧上
    ΔV 本身只有 ~2 ADU²、被估计量噪声主导时判据会假红（首版即如此）。
C3  掩膜传播：无效像素必须为 0，且与 HDU MASK 一致。
C4  **精确逐像素闭合**（用 TRUTHE HDU 的逐像素期望电子数，而非块均值）：
    在真值的 [p1, p99] 体区间上，mean(((adu-bias)*g - lam)^2) 必须闭合到
    mean(lam) + RN^2（|rel| <= 2%）。
    全帧（含极端亮像素）的同一量**必然偏高**且不是缺陷：真实底图里有 lam ~ 1e8 e- 的
    亮星核，其泊松方差 ~1e8 e-^2 比体区间大 5 个数量级，会主导**算术均值**。
    故判据只在体区间上定义，全帧值作为诊断量登记。
"""
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
RV = HERE.parents[1]
ROOT = RV.parent
sys.path.insert(0, str(RV / "synthetic"))

import noise_model as NM      # noqa: E402
import m16_scene as MS        # noqa: E402

OUT = ROOT / "run" / "reverse_verify" / "m16_scene" / "closure"
CASES = [("m16_nebula_core", "F657N"), ("m16_starfield", "F502N"), ("m16_dark_lowsnr", "F673N")]
G = 1.5
BLK0 = 256


def pair_var(blk):
    d1 = np.diff(blk, axis=1); d2 = np.diff(blk, axis=0)
    return float((np.mean(d1 ** 2) + np.mean(d2 ** 2)) / 4.0)


def flat_blocks(yy, xx, band):
    """重建该帧的平场图并取块（确定性：与渲染用同一种子/参数）。"""
    return None


def main():
    res = {}
    ok_all = True
    for sid, band in CASES:
        sc = MS.load_scene(RV / "synthetic" / "scenes" / ("%s.json" % sid))
        sc_noflat = json.loads(json.dumps(sc))
        sc_noflat["flat"] = {"prnu_rms": 0.0, "low_order": 0.0, "vignette": 0.0}
        sc_noflat["scene_id"] = sid + "_noflat"
        f1, t1, v1 = MS.render_m16_frame(sc, seed=20260924)
        f2, t2, v2 = MS.render_m16_frame(sc_noflat, seed=20260924)
        a1 = np.where(v1, f1.adu, 0.0); a2 = np.where(v2, f2.adu, 0.0)
        tr = f1.truth_e
        ny, nx = a1.shape
        # 选最平坦的全有效块；块尺寸自适应（暗帧可能没有 256^2 的全有效块）
        best = None
        for Bt in (BLK0, BLK0 // 2, BLK0 // 4):
            best = None
            for yy in range(0, ny - Bt, Bt):
                for xx in range(0, nx - Bt, Bt):
                    if not v1[yy:yy+Bt, xx:xx+Bt].all():
                        continue
                    w = tr[yy:yy+Bt, xx:xx+Bt]
                    c = float(w.std() / max(abs(w.mean()), 1e-9))
                    if best is None or c < best[0]:
                        best = (c, yy, xx, Bt)
            if best is not None:
                break
        if best is None:
            raise RuntimeError("no fully-valid block found for %s" % sid)
        c, yy, xx, Bblk = best
        lam = float(tr[yy:yy+Bblk, xx:xx+Bblk].mean())
        v_on = pair_var(a1[yy:yy+Bblk, xx:xx+Bblk])
        v_off = pair_var(a2[yy:yy+Bblk, xx:xx+Bblk])
        rn = float(t1["detector"]["read_noise_e"])
        pred = lam / G ** 2 + rn ** 2 / G ** 2 + NM.QUANTIZATION_VARIANCE_ADU2
        # 平场高频方差（用确定性重建的平场图）
        flat_cfg = dict(sc.get("flat", {}))
        fseed = int(flat_cfg.pop("seed", 1234))
        m = NM.flat_response((ny, nx), np.random.default_rng(fseed), **flat_cfg)
        mb = m[yy:yy+Bblk, xx:xx+Bblk]
        # **配对差分可见**的平场方差（与估计量 pair_var 同一算子）；
        # 旧式 np.var(mb - mb.mean()) 含块内平滑分量（tilt+vignette），对配对差分不可见。
        var_m_hf = pair_var(mb)
        var_m_block_total = float(np.var(mb - mb.mean()))
        pred_flat = lam ** 2 * var_m_hf / G ** 2
        rec = {"dataset": sid, "band": band, "block": [int(yy), int(xx), int(Bblk)],
               "block_flatness": c, "mean_truth_e": lam,
               "var_pair_flat_off": v_off, "var_pair_flat_on": v_on,
               "var_pred_analytic": pred, "rel_dev_flat_off": v_off / pred - 1.0,
               "delta_var_measured": v_on - v_off,
               "delta_var_pred_from_flat": pred_flat,
               "var_m_pair_visible": var_m_hf,
               "var_m_block_total": var_m_block_total,
               "smooth_flat_fraction": (1.0 - var_m_hf / var_m_block_total
                                        if var_m_block_total > 0 else 0.0),
               "delta_var_pred_from_flat_totalvar_legacy": (
                   lam ** 2 * var_m_block_total / G ** 2),
               "delta_rel_dev": (v_on - v_off) / pred_flat - 1.0 if pred_flat > 0 else None,
               "read_noise_e": rn, "gain_e_per_adu": G,
               "invalid_all_zero": bool(np.all(a1[~v1] == 0.0)) if (~v1).any() else True}
        n_pairs = 2 * Bblk * (Bblk - 1)
        sig_v = v_on * np.sqrt(8.0 / n_pairs)
        tol = 3.0 * np.sqrt(2.0) * sig_v
        rec["n_pairs"] = int(n_pairs)
        rec["sigma_pair_var_estimator_adu2"] = float(sig_v)
        rec["C2_tolerance_adu2"] = float(tol)
        rec["C2_z_score"] = float((rec["delta_var_measured"] - pred_flat) / tol) if tol > 0 else 0.0
        # ---- C4 精确逐像素闭合（体区间） ----
        det_cfg = dict(sc["detector"])
        det = NM.Detector(**{k: v for k, v in det_cfg.items()
                             if k in ("gain_e_per_adu", "read_noise_e", "bias_adu",
                                      "full_well_e", "dark_current_e_per_s",
                                      "dark_ref_temp_c", "dark_double_temp_c",
                                      "quantize", "saturate")})
        lam_px = f2.truth_e                       # 无平场臂的逐像素期望电子数
        resid_e = (a2 - det.bias_adu) * G - lam_px
        sq = resid_e ** 2
        lo, hi = np.percentile(lam_px, [1.0, 99.0])
        bulk = (lam_px >= lo) & (lam_px <= hi)
        c4_meas = float(sq[bulk].mean()); c4_pred = float(lam_px[bulk].mean()) + rn ** 2
        c4_all = float(sq.mean()); c4_all_pred = float(lam_px.mean()) + rn ** 2
        rec["C4_bulk_lam_range_e"] = [float(lo), float(hi)]
        rec["C4_bulk_var_meas_e2"] = c4_meas
        rec["C4_bulk_var_pred_e2"] = c4_pred
        rec["C4_bulk_rel_dev"] = c4_meas / c4_pred - 1.0
        rec["C4_allpix_var_meas_e2"] = c4_all
        rec["C4_allpix_var_pred_e2"] = c4_all_pred
        rec["C4_allpix_rel_dev"] = c4_all / c4_all_pred - 1.0
        rec["C4_pass"] = bool(abs(rec["C4_bulk_rel_dev"]) <= 0.02)
        rec["C1_pass"] = bool(abs(rec["rel_dev_flat_off"]) <= 0.05)
        rec["C2_pass"] = bool(abs(rec["delta_var_measured"] - pred_flat) <= tol)
        rec["C3_pass"] = rec["invalid_all_zero"]
        ok_all = (ok_all and rec["C1_pass"] and rec["C2_pass"] and rec["C3_pass"]
                  and rec["C4_pass"])
        res[sid] = rec
        print("%-18s lam=%8.1f e-  flatOFF var=%8.2f pred=%8.2f (%+6.2f%%)  "
              "flatON var=%8.2f  dvar=%7.2f +-%5.2f  dvar_pred=%7.2f  z=%+5.2f  "
              "C4_bulk=%+6.3f%% C4_all=%+7.2f%%  C1=%s C2=%s C3=%s C4=%s" % (
                  sid, lam, v_off, pred, 100 * rec["rel_dev_flat_off"], v_on,
                  rec["delta_var_measured"], tol, pred_flat, rec["C2_z_score"],
                  100 * rec["C4_bulk_rel_dev"], 100 * rec["C4_allpix_rel_dev"],
                  rec["C1_pass"], rec["C2_pass"], rec["C3_pass"], rec["C4_pass"]))
    res["_all_pass"] = bool(ok_all)
    OUT.mkdir(parents=True, exist_ok=True)
    with open(OUT / "variance_closure.json", "w", encoding="utf-8") as fh:
        json.dump(res, fh, indent=1, default=float)
    print("\n[closure] all_pass =", ok_all, "->", OUT / "variance_closure.json")
    return 0 if ok_all else 1


if __name__ == "__main__":
    raise SystemExit(main())