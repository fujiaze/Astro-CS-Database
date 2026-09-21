#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B / B3：dense / sparse_reconstruct / frame_reconstruct 三口径适用域图谱。

三数据面（最高设计 §12.2 三类数据）：
  ① 纯合成 GRF σ 场（真值解析已知、ℓ 由构造控制）—— Δ/ℓ 扫描与失效边界；
  ② HST M16 F657N 真实信号 + 完整物理噪声（真值解析已知）—— 高对比高分辨率域；
  ③ testdata M42 Red 300 s 真实帧（无解析真值）—— 地面视宁度受限域，1 px 棋盘
     hold-out 真值（控制值与评价目标零像素重叠）。

判据：
  R1 排序（配对：同一帧、同一评价掩膜）；R2 精度 RMSE(log10 ρ) ≤ τ_A；
  R3 偏差 |level_bias| ≤ τ_B；R4 权重效率损失 E = Var_w/Var_opt − 1（非退化判据，
     全局尺度相消 ⇒ σ̂ ∝ σ_true 时为 0）；R5 代价（字节/帧、重建时间）。
输出：results/b3_domain_map.json
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C  # noqa: E402

P_PATCH = 32
DELTA_GRID = [16, 32, 64, 128, 256, 512]
CROP = 2048
BUDGET_BYTES = 1024 * 1024          # HiPS 每帧 SNR 层预算 1 MiB（EXP-205 冻结门）
M16 = os.path.join(C.HST_M16, "hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits")
M42 = [os.path.join(C.TESTDATA, p) for p in [
    "M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts",
    "M42_T2T3_mosaic_Flying_dutchman/T2/M2/M42_M2_T2_flying_dutchman-20251212@020002-300S-Red.fts",
    "M42_T2T3_mosaic_Flying_dutchman/T2/M4/M42_M4_T2_flying_dutchman-20251224@045919-300S-Red.fts"]]
# 声明坐标（与 EXP-205 同口径，不做物理闭合反推）
GAIN, RN, DARK, SIG_MED_E, SKY_E = 1.3, 10.0, 0.5, 200.0, 200.0
ELL_SYNTH = [16.0, 32.0, 64.0, 128.0, 256.0]
S_FIELD_SYNTH = [0.0, 0.03, 0.10, 0.30]     # log10 σ 场的真实 std（0 = 真值无空间效应负例）


def synth_sigma_face(ell_px, s_field, n=2048, seed_off=0):
    """① 纯合成：σ(x,y)=10^(s·g)，g 为 SE(ℓ) 单位方差 GRF；真值解析已知。

    s_field = log10 σ 场的真实 std（s=0 ⇒ 平坦场，真值无空间效应）。"""
    r = C.rng(seed_off)
    ny = nx = n
    fy = np.fft.fftfreq(ny)[:, None]; fx = np.fft.rfftfreq(nx)[None, :]
    k2 = fy ** 2 + fx ** 2
    # SE 核的功率谱 ∝ exp(-2π²ℓ²k²)（Gaussian 核 FT 仍为 Gaussian）
    pk = np.exp(-2.0 * (np.pi ** 2) * (ell_px ** 2) * k2)
    w = r.normal(size=(ny, nx // 2 + 1)) + 1j * r.normal(size=(ny, nx // 2 + 1))
    g = np.fft.irfft2(w * np.sqrt(pk), s=(ny, nx))
    g = g / g.std()
    sigma = 10.0 ** (s_field * g)
    return sigma


def synth_data_face(sigma, seed_off=1):
    r = C.rng(seed_off)
    return r.normal(0.0, 1.0, size=sigma.shape) * sigma, sigma


def load_m16(seed_off=2):
    from astropy.io import fits
    with fits.open(M16, memmap=False) as h:
        raw = np.asarray(h[0].data, dtype=np.float64)
        hdr = h[0].header
    ny, nx = raw.shape
    y0, x0 = (ny - CROP) // 2, (nx - CROP) // 2
    sig = np.maximum(raw[y0:y0 + CROP, x0:x0 + CROP], 0.0)
    scale = SIG_MED_E / max(np.median(sig), 1e-12)
    sig_e = sig * scale                                    # 纯信号模板 [e-]
    r = C.rng(seed_off)
    lam = sig_e + SKY_E + DARK
    d = r.poisson(lam).astype(np.float64) + r.normal(0.0, RN, size=lam.shape)
    img = d / GAIN
    truth = np.sqrt((sig_e + SKY_E + DARK + RN ** 2) / GAIN ** 2)   # 解析逐像素 σ（含源泊松）
    return img, truth, dict(file=os.path.basename(M16), bunit=str(hdr.get("BUNIT", "")).strip(),
                            scale_to_median_e=float(scale), median_signal_e=float(np.median(sig_e)),
                            max_over_median=float(sig_e.max() / max(np.median(sig_e), 1e-12)),
                            crop=CROP)


def load_m42(path, seed_off=3):
    from astropy.io import fits
    with fits.open(path, memmap=False) as h:
        raw = np.asarray(h[0].data, dtype=np.float64)
        hdr = h[0].header
    ny, nx = raw.shape
    y0, x0 = (ny - CROP) // 2, (nx - CROP) // 2
    img = raw[y0:y0 + CROP, x0:x0 + CROP]
    return img, dict(file=os.path.basename(path), exptime=float(hdr.get("EXPTIME", 0.0)),
                     filter=str(hdr.get("FILTER", "")), crop=CROP)


# hold-out 真值自身噪声 [dex]：1.4826×MAD 的相对标准误 1.166/√N（Rousseeuw-Croux 渐近式）
# 换算到 dex 必须除 ln10（NOISE_MODEL.md:86 的 1.44/√N 是**相对**量，不是 dex）
EPS_REF = 1.166 / np.log(10.0) / np.sqrt(P_PATCH * P_PATCH / 2.0)


def holdout_arms(img):
    """1 px 棋盘 hold-out：估计量只用族 0 像素，真值只用族 1 像素（零重叠）。"""
    yy, xx = np.mgrid[0:img.shape[0], 0:img.shape[1]]
    fam = (yy + xx) % 2
    img0 = np.where(fam == 0, img, np.nan)
    img1 = np.where(fam == 1, img, np.nan)
    est_dense = C.sigma_field_fast(img0, P_PATCH)
    truth = C.sigma_field_fast(img1, P_PATCH)
    return est_dense, truth, img0, img1


def eval_arms(img, truth, eval_cell_mask, meta, f32=None, img_stat=None):
    """在同一评价 cell 集上评价三条口径。返回每 Δ 的结果。

    f32：估计量侧 P=32 patch σ 场（默认用全像素；hold-out 面传入族 0 版本）。
    img_stat：估计整帧 MAD 用的像素集（默认全帧）。
    """
    rows = []
    if f32 is None:
        f32 = C.sigma_field_fast(img, P_PATCH)
    ny, nx = f32.shape
    base = eval_cell_mask
    s_field = float(np.nanstd(np.log10(np.where(base, f32, np.nan))))
    frame_med = float(np.nanmedian(np.where(base, f32, np.nan)))
    src = img if img_stat is None else img_stat
    vals = src[np.isfinite(src)]
    frame_mad = float(C.K_MAD_TO_SIGMA * np.median(np.abs(vals - np.median(vals))))  # 整帧未裁剪 MAD（已知错误构造）
    for D in DELTA_GRID:
        t0 = time.time()
        cell = C.sigma_field_fast(img if img_stat is None else img_stat, D)
        up = C.bilinear_upsample(cell, img.shape, D)
        up_cell = up.reshape(ny, P_PATCH, nx, P_PATCH).transpose(0, 2, 1, 3).reshape(ny, nx, -1)
        est_sparse = np.nanmedian(up_cell, axis=2)
        rec_t = time.time() - t0
        for name, est in [("sparse", est_sparse), ("dense", f32),
                          ("frame_median", np.full_like(f32, frame_med)),
                          ("frame_mad", np.full_like(f32, frame_mad))]:
            m = base & np.isfinite(est) & np.isfinite(truth) & (est > 0) & (truth > 0)
            if m.sum() < 16:
                # 缺臂必须显式记录（禁止静默丢弃：SKIP 充数 = 未完成）
                rows.append(dict(delta_px=D, arm=name, rmse_log_rho=float("nan"),
                                 rmse_log_rho_corrected=float("nan"), level_bias_dex=float("nan"),
                                 eff_loss=float("nan"), n_eval=int(m.sum()), recon_s=None,
                                 skipped=True, skip_reason="n_eval<16 (mask=%d)" % int(m.sum())))
                continue
            a = est[m] / np.median(est[m]); b = truth[m] / np.median(truth[m])
            rmse = float(np.sqrt(np.mean((np.log10(a) - np.log10(b)) ** 2)))
            bias = float(np.median(np.log10(est[m] / truth[m])))
            # 逐 cell 权重效率损失（在同一评价 cell 上）
            e = C.weight_efficiency_loss(est[m], truth[m], np.ones(m.sum(), bool))
            rmse_corr = float(np.sqrt(max(rmse ** 2 - EPS_REF ** 2, 0.0)))
            rows.append(dict(delta_px=D, arm=name, rmse_log_rho=rmse, rmse_log_rho_corrected=rmse_corr,
                             level_bias_dex=bias, eff_loss=e, n_eval=int(m.sum()),
                             recon_s=rec_t if name == "sparse" else None,
                             skipped=False, skip_reason=None))
    # 存储代价（按 4096² 生产帧与本次 crop 两种口径）
    n_px_prod = 4096 * 4096
    cost = dict(
        dense_bytes_4096=4 * n_px_prod, dense_MiB_4096=4 * n_px_prod / 1048576.0,
        frame_bytes=4,
        sparse_bytes_4096={str(D): 4 * int(np.ceil(4096 / D)) ** 2 for D in DELTA_GRID},
        budget_bytes=BUDGET_BYTES,
        dense_over_budget_factor=(4 * n_px_prod) / BUDGET_BYTES,
        sparse64_over_budget_factor=(4 * int(np.ceil(4096 / 64)) ** 2) / BUDGET_BYTES,
        dense_bytes_crop=4 * img.size, crop=CROP)
    return dict(meta=meta, s_field_log10_std=s_field, frame_median_adu=frame_med,
                frame_mad_adu=frame_mad, rows=rows, cost=cost,
                ell_px=C.estimate_ell(f32, P_PATCH)[0], n_cells=int(base.sum()))


def delta_star(rows, ell, ell_axis=None):
    """失效边界 Δ*：sparse 的 RMSE 首次超过 frame_median 的最小 Δ。

    status: crossed | no_crossing | not_computed（缺臂/被跳过 ⇒ not_computed，必须判红）
    """
    axis = ell_axis if (ell_axis and np.isfinite(ell_axis)) else ell
    out = {}
    skipped = sorted({r["delta_px"] for r in rows if r.get("skipped")})
    for D in DELTA_GRID:
        sp = next((r["rmse_log_rho"] for r in rows if r["delta_px"] == D and r["arm"] == "sparse"), None)
        fr = next((r["rmse_log_rho"] for r in rows if r["delta_px"] == D and r["arm"] == "frame_median"), None)
        if sp is None or fr is None or not np.isfinite(sp) or not np.isfinite(fr):
            out["delta_star_px"] = None
            out["delta_star_over_ell"] = None
            out["ell_axis_px"] = float(axis) if axis and np.isfinite(axis) else None
            out["status"] = "not_computed"
            out["skipped_deltas"] = skipped
            return out
        if sp > fr:
            out["delta_star_px"] = D
            out["delta_star_over_ell"] = float(D / axis) if axis and np.isfinite(axis) else None
            out["ell_axis_px"] = float(axis) if axis and np.isfinite(axis) else None
            out["status"] = "crossed"
            out["skipped_deltas"] = skipped
            return out
    out["delta_star_px"] = None
    out["delta_star_over_ell"] = None
    out["ell_axis_px"] = float(axis) if axis and np.isfinite(axis) else None
    out["status"] = "no_crossing"
    out["skipped_deltas"] = skipped
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "b3_domain_map.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    faces = {}

    # ---- 面 ①：纯合成 GRF（ℓ 已知） ----
    syn = []
    i = 0
    for s_true in S_FIELD_SYNTH:
        for ell in ELL_SYNTH:
            i += 1
            sigma = synth_sigma_face(ell, s_true, seed_off=100 + i)
            img, truth = synth_data_face(sigma, seed_off=200 + i)
            ny = nx = CROP // P_PATCH
            base = np.ones((ny, nx), bool)
            meta = dict(face="synthetic_grf", ell_nominal_px=ell, s_field_true_log10=s_true)
            tc = truth.reshape(ny, P_PATCH, nx, P_PATCH).transpose(0, 2, 1, 3).reshape(ny, nx, -1)
            truth_cell = np.sqrt(np.mean(tc ** 2, axis=2))
            res = eval_arms(img, truth_cell, base, meta)
            res["delta_star"] = delta_star(res["rows"], res["ell_px"], ell_axis=ell)
            syn.append(res)
            print("[synth ℓ=%.0f s=%.2f] ell_meas=%.1f s_field_meas=%.4f %s"
                  % (ell, s_true, res["ell_px"], res["s_field_log10_std"], res["delta_star"]), flush=True)
    faces["synthetic_grf"] = syn

    # ---- 面 ②：HST M16（解析真值） ----
    img2, truth2, meta2 = load_m16()
    ny = nx = CROP // P_PATCH
    tcell = truth2.reshape(ny, P_PATCH, nx, P_PATCH).transpose(0, 2, 1, 3).reshape(ny, nx, -1)
    truth_cell = np.sqrt(np.mean(tcell ** 2, axis=2))          # cell 内 RMS σ（真值）
    f2 = C.sigma_field_fast(img2, P_PATCH)
    base2 = np.isfinite(truth_cell) & np.isfinite(f2)
    res2 = eval_arms(img2, truth_cell, base2, dict(face="hst_m16", **meta2))
    res2["delta_star"] = delta_star(res2["rows"], res2["ell_px"])
    faces["hst_m16"] = res2
    print("[hst] ell=%.1f s_field=%.4f %s" % (res2["ell_px"], res2["s_field_log10_std"], res2["delta_star"]), flush=True)

    # ---- 面 ③：testdata M42（hold-out 真值） ----
    m42 = []
    for i, path in enumerate(M42):
        if not os.path.exists(path):
            raise RuntimeError("testdata 缺失（fail-closed，禁止静默少面）: " + path)
        img3, meta3 = load_m42(path)
        est_dense3, truth3, img0_3, img1_3 = holdout_arms(img3)
        base3 = np.isfinite(truth3) & np.isfinite(est_dense3)
        res3 = eval_arms(img3, truth3, base3, dict(face="testdata_m42", **meta3),
                         f32=est_dense3, img_stat=img0_3)
        res3["delta_star"] = delta_star(res3["rows"], res3["ell_px"])
        m42.append(res3)
        print("[m42 %s] ell=%.1f s_field=%.4f %s" % (meta3["file"][:28], res3["ell_px"], res3["s_field_log10_std"], res3["delta_star"]), flush=True)
    faces["testdata_m42"] = m42

    # ---- 适用域汇总 ----
    summary = []
    for res in syn + [res2] + m42:
        for D in DELTA_GRID:
            rr = {r["arm"]: r for r in res["rows"] if r["delta_px"] == D}
            if not rr or "sparse" not in rr or "frame_median" not in rr:
                continue
            best = min(rr, key=lambda k: rr[k]["rmse_log_rho"])
            ratio = (float(rr["sparse"]["rmse_log_rho"] / rr["frame_median"]["rmse_log_rho"])
                     if rr["frame_median"]["rmse_log_rho"] > 0 else float("inf"))
            summary.append(dict(face=res["meta"]["face"], delta_px=D, ell_px=res["ell_px"],
                                delta_over_ell=float(D / res["ell_px"]) if res["ell_px"] else None,
                                rmse={k: rr[k]["rmse_log_rho"] for k in rr},
                                eff_loss={k: rr[k]["eff_loss"] for k in rr},
                                best_arm=best, sparse_over_frame=ratio))
    gates = {}
    for res in syn + [res2] + m42:
        tag = res["meta"]["face"] + (":" + res["meta"]["file"][:20] if "file" in res["meta"] else
                                     (":ell%.0f_s%.2f" % (res["meta"]["ell_nominal_px"], res["meta"]["s_field_true_log10"])
                                      if "ell_nominal_px" in res["meta"] else ""))
        r64 = {r["arm"]: r for r in res["rows"] if r["delta_px"] == 64}
        gates[tag] = dict(ell_px=res["ell_px"], s_field_log10_std=res["s_field_log10_std"],
                          delta_star=res["delta_star"],
                          at_delta64={k: dict(rmse=r64[k]["rmse_log_rho"], bias=r64[k]["level_bias_dex"],
                                              eff_loss=r64[k]["eff_loss"]) for k in r64},
                          cost=res["cost"])
    # 关键面 Δ=64 明细（供报告引用）
    detail = {}
    for res in syn + [res2] + m42:
        tag = res["meta"]["face"] + (":" + res["meta"]["file"][:18] if "file" in res["meta"] else
                                     (":ell%.0f_s%.2f" % (res["meta"]["ell_nominal_px"], res["meta"]["s_field_true_log10"])
                                      if "ell_nominal_px" in res["meta"] else ""))
        r64 = {r["arm"]: dict(rmse=r["rmse_log_rho"], rmse_corr=r["rmse_log_rho_corrected"],
                              bias=r["level_bias_dex"], eff_loss=r["eff_loss"])
               for r in res["rows"] if r["delta_px"] == 64}
        detail[tag] = dict(ell_meas=res["ell_px"], s_field=res["s_field_log10_std"], at_delta64=r64)
    gates["delta64_detail"] = detail
    # 非退化负例：s_true=0（平坦场）时空间口径不得胜出
    flat = [r for r in syn if r["meta"]["s_field_true_log10"] == 0.0]
    flat_bad = []
    for res in flat:
        for D in DELTA_GRID:
            rr = {r["arm"]: r for r in res["rows"] if r["delta_px"] == D}
            if "sparse" in rr and "frame_median" in rr:
                if rr["sparse"]["rmse_log_rho"] < rr["frame_median"]["rmse_log_rho"]:
                    flat_bad.append(dict(ell=res["meta"]["ell_nominal_px"], delta=D,
                                         rmse_sparse=rr["sparse"]["rmse_log_rho"],
                                         rmse_frame=rr["frame_median"]["rmse_log_rho"]))
    # 注意：s=0 时帧级常数臂 RMSE 精确为 0 ⇒ "sparse 严格更优"在数学上不可能，
    # 该排序判据在此场景**退化**（恒真），只作"判据退化区"的记录，不计入非退化负例 PASS。
    gates["DEGENERATE_flat_field_ranking_never_true"] = bool(len(flat_bad) == 0)
    gates["DEGENERATE_flat_field_ranking_note"] = (
        "恒真门：帧级常数臂 RMSE≡0（真值场为常数），任何数据都不可能翻红；仅记录，不作证据")
    gates["DEGENERATE_flat_field_violations"] = flat_bad
    # 非退化硬门：任何 (面,Δ) 的臂都不得被跳过；delta_star 不得为 not_computed
    all_rows = [r for res in (syn + [faces["hst_m16"]] + faces["testdata_m42"]) for r in res["rows"]]
    skipped = [dict(face=res["meta"].get("face"), ell=res["meta"].get("ell_nominal_px"),
                    delta=r["delta_px"], arm=r["arm"], n_eval=r["n_eval"], reason=r.get("skip_reason"))
               for res in (syn + [faces["hst_m16"]] + faces["testdata_m42"]) for r in res["rows"]
               if r.get("skipped")]
    gates["N2_no_arm_skipped"] = bool(len(skipped) == 0)
    gates["N2_skipped_arms"] = skipped
    nc = [res["meta"] for res in (syn + [faces["hst_m16"]] + faces["testdata_m42"])
          if res["delta_star"].get("status") == "not_computed"]
    gates["N3_delta_star_all_computed"] = bool(len(nc) == 0)
    gates["N3_not_computed_faces"] = nc
    obj = dict(experiment="SCI-B / B3 dense|sparse|frame domain map (ground seeing-limited vs HST high-contrast)",
               frozen_config=dict(P_patch=P_PATCH, delta_grid=DELTA_GRID, crop=CROP,
                                  gain=GAIN, rn=RN, dark=DARK, sig_median_e=SIG_MED_E, sky_e=SKY_E,
                                  ell_synth=ELL_SYNTH, s_field_synth=S_FIELD_SYNTH,
                                  budget_bytes=BUDGET_BYTES, seed_base=C.SEED_BASE,
                                  m16=M16, m42=M42),
               faces=faces, summary=summary, gates=gates,
               generated_at=C.now(), wall_s=time.time() - t0)
    C.save_json(a.out, obj)
    print("wrote", a.out)


if __name__ == "__main__":
    main()
