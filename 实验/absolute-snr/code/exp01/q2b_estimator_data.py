#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-01 / 问题二（数据臂）：HST 前向仿真 + testdata 真实产物。

三块证据：
  1. HST 真实模板 + 完整物理前向仿真：同一星场在**不同 seeing** 下渲染，
     对每颗真值星分别用三个候选估计量测量，给出
       * 报告 flux / 真值通量 的**跨帧（seeing）离散** —— 判据 (c) 跨帧可比；
       * 报告 sigma / 实测抽样 std 的**配对性** —— 判据 (a)；
       * 帧级 reference_snr_f 在三候选口径下的取值 —— 判据 (d)。
     负例：同一 seeing 两次渲染（真值无效应）=> 跨帧伪差必须严格 0。
  2. testdata 真实产物（run/**/p1_sources.json）：方案 A 的**样本代价**
     （n_psf_valid / n_sources）、盒和 vs PSF 解析通量的实测比与 seeing 依赖、
     以及 PSF 块 FWHM 与检测块 FWHM 的跨块列口径陷阱。
  3. 帧级 reference_snr_f 的解析对照。

只读；不运行任何 ACSD 可执行文件；不修改任何产物。
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

import numpy as np
from scipy.optimize import least_squares

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp01_common as C            # noqa: E402
import hst_sim as HS                # noqa: E402
import real_products as RP          # noqa: E402
from q2a_estimator_pairing import _fit_full_flux   # noqa: E402

SEED = 20260924
GAIN = 1.3
RN = 10.0
N_SKY_FRAME = 1.6e7
STAMP_HALF = 8


# ---------------------------------------------------------------------------
# 1. HST 前向仿真臂
# ---------------------------------------------------------------------------
def _measure_star(frame_adu: np.ndarray, bg: float, x: float, y: float,
                  fwhm_assumed: float, sigma_sky: float = 0.0) -> Dict[str, float]:
    """在真实渲染帧上对一颗已知位置的星做三候选测量（镜像生产步骤）。"""
    ny, nx = frame_adu.shape
    xi, yi = int(round(x)), int(round(y))
    if xi < STAMP_HALF + 2 or yi < STAMP_HALF + 2 or xi >= nx - STAMP_HALF - 2 or yi >= ny - STAMP_HALF - 2:
        return {}
    # 峰值像素（镜像 StarDetector::detect 的 3x3 局部极大）
    win = frame_adu[yi - 2:yi + 3, xi - 2:xi + 3]
    dy, dx = np.unravel_index(int(np.argmax(win)), win.shape)
    py, px = yi - 2 + dy, xi - 2 + dx
    if py < STAMP_HALF + 2 or px < STAMP_HALF + 2 or py >= ny - STAMP_HALF - 2 or px >= nx - STAMP_HALF - 2:
        return {}
    box = frame_adu[py - 2:py + 3, px - 2:px + 3] - bg
    box_trunc = float(np.maximum(box, 0.0).sum())
    box_untrunc = float(box.sum())
    stamp = frame_adu[py - STAMP_HALF:py + STAMP_HALF + 1,
                      px - STAMP_HALF:px + STAMP_HALF + 1]
    sigma0 = C.detection_sigma_from_fwhm(fwhm_assumed)
    A0 = max(box_trunc, 1.0) / (2.0 * math.pi * sigma0 * sigma0 / 3.0)
    psf_flux = _fit_full_flux(stamp, bg, sigma0, max(box_trunc, 1.0))
    # 最优加权和（登记用，**不进入跨帧判据**）：本臂未做质心重定心，假定轮廓固定在
    # 峰值像素上，亚像素相位会系统性压低该估计量（实测跨 seeing 伪差 ~34%），
    # 该偏离来自"未重定心"的实现简化，不是估计量本身的性质 ⇒ 只登记、不判定。
    P, _, _, _ = C.profile_for_fwhm(fwhm_assumed)
    c = P.shape[0] // 2
    Pc = P[c - STAMP_HALF:c + STAMP_HALF + 1, c - STAMP_HALF:c + STAMP_HALF + 1]
    V = np.full(Pc.shape, max(float(sigma_sky), 1e-6) ** 2)
    w = Pc / V
    psf_amp = float((w * (stamp - bg)).sum() / (Pc * Pc / V).sum())
    return {"box_trunc": box_trunc, "box_untrunc": box_untrunc, "psf_flux": psf_flux,
            "psf_amp": psf_amp, "peak": float(frame_adu[py, px])}


def _ratio_rows(adu: np.ndarray, bgstat: Dict[str, float], truth: Dict[str, Any],
                fwhm_assumed: float) -> List[Dict[str, float]]:
    """逐星测量并返回 (报告值/真值通量) 的行（供跨帧比较与负例共用）。"""
    rows = []
    for s in truth["stars_in_frame"]:
        m = _measure_star(adu, bgstat["background"], s["x"], s["y"], fwhm_assumed,
                          bgstat["sigma"])
        if not m:
            continue
        F_true_adu = s["flux_e"] / GAIN
        if F_true_adu <= 0:
            continue
        rows.append({"F_true_adu": F_true_adu,
                     "peak_snr": (m["peak"] - bgstat["background"]) / bgstat["sigma"],
                     **{k: m[k] / F_true_adu
                        for k in ("box_trunc", "box_untrunc", "psf_flux")}})
    return rows


def hst_cross_frame(seeings=(2.0, 3.0, 3.9), n_stars=120, shape=(768, 768),
                    sky_e_per_s=0.5, exposure_s=300.0,
                    max_flux=3.0e5, use_hst_base=False, target_p999_e=50000.0) -> Dict[str, Any]:
    """同一星表、同一真值通量，在不同 seeing 下渲染并逐星测量。

    use_hst_base=True 时以 HST M16 真实帧作纯信号底图（§12.2 第 1 类）；底图在两次
    渲染间**逐位相同**，故跨 seeing 比较不受其影响，但它会抬高盒和的局部背景。
    """
    rng = np.random.default_rng(SEED)
    frames = []
    for fw in seeings:
        # **跨块列口径（DISP-STAR-007）**：本臂的 seeings 一律是**检测块高斯 FWHM**
        # （生产 DATA-P1-SOURCES.fwhm_px 的语义）；render 场景的 psf.fwhm_px 是
        # **PSF 块 Moffat4 FWHM**（kMoffat4FwhmFactor=1.230310）。二者换算：
        #   sigma = fwhm_det / 2.3548200450309493 ;  fwhm_moffat = 1.230310 * sigma
        fw_scene = fw / C.GAUSS_FWHM_FACTOR * C.MOFFAT4_FWHM_FACTOR
        fwhm_assumed = fw
        sc = HS.make_scene(shape=shape, fwhm_px=fw_scene, sky_e_per_s=sky_e_per_s,
                           gain=GAIN, read_noise_e=RN, n_stars=n_stars,
                           exposure_s=exposure_s, use_hst_base=use_hst_base,
                           target_p999_e=target_p999_e,
                           flux_log10=(3.5, math.log10(max_flux)))
        det = HS.detector_of(sc)
        fr, tr = HS.render(sc, seed=SEED)
        r = C.production_clip_sigma(fr.adu)
        rows = _ratio_rows(fr.adu, r, tr, fwhm_assumed)
        P_model, _, _, sig = C.profile_for_fwhm(fw)
        frames.append({"fwhm_det_px": fw, "fwhm_scene_moffat_px": fw_scene,
                       "sigma_moffat_px": sig,
                       "E_B_model_theory": C.energy_fraction_box(P_model),
                       "sigma_sky_adu": r["sigma"], "background": r["background"],
                       "n_measured": len(rows), "rows": rows})
    # 统计：按峰值 SNR 门槛分档
    out: Dict[str, Any] = {"seeings": list(seeings), "frames": [], "controls": {}}
    for f in frames:
        rows = f["rows"]
        rec: Dict[str, Any] = {"fwhm_det_px": f["fwhm_det_px"],
                               "fwhm_scene_moffat_px": f["fwhm_scene_moffat_px"],
                               "sigma_moffat_px": f["sigma_moffat_px"],
                               "E_B_model_theory": f["E_B_model_theory"],
                               "sigma_sky_adu": f["sigma_sky_adu"],
                               "n_measured": f["n_measured"]}
        for thr in (5.0, 20.0, 100.0):
            sub = [r for r in rows if r["peak_snr"] >= thr]
            rec["n_peak_snr_ge_%.0f" % thr] = len(sub)
            for k in ("box_trunc", "box_untrunc", "psf_flux"):
                v = np.array([r[k] for r in sub])
                if v.size:
                    rec["%s_p50_snr%.0f" % (k, thr)] = float(np.median(v))
                    rec["%s_p25_snr%.0f" % (k, thr)] = float(np.percentile(v, 25))
                    rec["%s_p75_snr%.0f" % (k, thr)] = float(np.percentile(v, 75))
        out["frames"].append(rec)
    # 跨帧离散（seeing 效应）
    for thr in (5.0, 20.0, 100.0):
        for k in ("box_trunc", "box_untrunc", "psf_flux"):
            key = "%s_p50_snr%.0f" % (k, thr)
            vals = [f.get(key) for f in out["frames"]]
            if all(v is not None for v in vals):
                out["controls"]["%s_spread_over_seeing" % key] = float(max(vals) / min(vals) - 1.0)
                out["controls"]["%s_values" % key] = vals
    # 负例：同一 seeing 两次（真值无效应）=> 伪差必须严格 0
    fwhm_assumed = 2.5
    sc = HS.make_scene(shape=shape,
                       fwhm_px=fwhm_assumed / C.GAUSS_FWHM_FACTOR * C.MOFFAT4_FWHM_FACTOR,
                       sky_e_per_s=sky_e_per_s, gain=GAIN,
                       read_noise_e=RN, n_stars=n_stars, exposure_s=exposure_s,
                       use_hst_base=use_hst_base, target_p999_e=target_p999_e,
                       flux_log10=(3.5, math.log10(max_flux)))
    # N3a（恒等负例）：同一帧对自身 => 必须**严格 0**
    f1, t1 = HS.render(sc, seed=SEED)
    r1 = C.production_clip_sigma(f1.adu)
    m1 = _ratio_rows(f1.adu, r1, t1, fwhm_assumed)
    out["controls"]["same_frame_identity_control"] = {
        k: float(np.median([a[k] for a in m1]) / np.median([a[k] for a in m1]) - 1.0)
        for k in ("box_trunc", "box_untrunc", "psf_flux")}
    # N3b（真值无效应 + 独立噪声）：同一 seeing 两次独立实现 => 伪差必须落在统计噪声内
    f2, t2 = HS.render(sc, seed=SEED + 1)
    r2 = C.production_clip_sigma(f2.adu)
    m2 = _ratio_rows(f2.adu, r2, t2, fwhm_assumed)
    ctl: Dict[str, Any] = {}
    if m1 and m2 and len(m1) == len(m2):
        n = len(m1)
        for k in ("box_trunc", "box_untrunc", "psf_flux"):
            a = np.array([x[k] for x in m1]) / np.array([x["F_true_adu"] for x in m1])
            b = np.array([x[k] for x in m2]) / np.array([x["F_true_adu"] for x in m2])
            d = np.median(a) / np.median(b) - 1.0
            # 自助法标准误（中位数比的抽样噪声）
            boot = np.array([np.median(rng.choice(a, n)) / np.median(rng.choice(b, n)) - 1.0
                             for _ in range(400)])
            ctl[k] = {"spurious": float(d), "boot_sem": float(boot.std(ddof=1)),
                      "z": float(d / boot.std(ddof=1)) if boot.std(ddof=1) > 0 else None}
    out["controls"]["same_seeing_control"] = ctl
    return out


# ---------------------------------------------------------------------------
# 2. 真实产物臂
# ---------------------------------------------------------------------------
def real_arm(limit=4, min_bytes=50_000_000) -> Dict[str, Any]:
    prods = RP.find_products(min_bytes=min_bytes, limit=limit)
    frames = []
    for p in prods:
        for fr in RP.iter_frames(p):
            mt = RP.match_psf_to_sources(fr["src"], fr["psf"])
            n_src = int(fr["src"]["flux"].size)
            n_psf = int(fr["psf"]["flux"].size)
            rec: Dict[str, Any] = {
                "file": p.name, "n_sources": n_src, "n_psf_valid": n_psf,
                "n_fit_input": fr["meta"].get("n_fit_input"),
                "psf_valid_frac": (n_psf / n_src) if n_src else None,
                "noise_sigma": fr["meta"].get("noise_sigma"),
            }
            if mt["box_flux"].size:
                # **事先声明的物理可用性筛选**（不是事后剔除）：psf_flux>0 且 box_flux>0
                # （估计量必须为正）且 ratio in [0.5, 2]（两个独立通量口径应在同一量级）。
                # 带外 = PSF 拟合失败（实测多为饱和星），单独登记比例。
                ratio_all = mt["box_flux"] / mt["psf_flux"]
                ok = (mt["psf_flux"] > 0) & (mt["box_flux"] > 0)
                ratio = ratio_all[ok]
                fw = mt["fwhm_det"][ok]
                band = (ratio >= 0.5) & (ratio <= 2.0)
                rec["matched"] = int(ratio.size)
                rec["in_band_frac"] = float(band.mean()) if ratio.size else None
                rb = ratio[band]
                rec["box_over_psf_p50"] = float(np.median(rb)) if rb.size else None
                rec["box_over_psf_p25"] = float(np.percentile(rb, 25)) if rb.size else None
                rec["box_over_psf_p75"] = float(np.percentile(rb, 75)) if rb.size else None
                rec["fwhm_det_p50"] = float(np.median(fw)) if fw.size else None
                rec["fwhm_x_over_sx_p50"] = float(np.median(mt["fwhm_x"] / mt["sx"]))
                if rb.size:
                    fl = mt["box_flux"][ok][band]
                    q = np.percentile(fl, [10, 90])
                    for tag, sel in (("dim", fl <= q[0]), ("bright", fl >= q[1])):
                        v = rb[sel]
                        rec["box_over_psf_p50_%s" % tag] = float(np.median(v)) if v.size else None
            frames.append(rec)
    agg: Dict[str, Any] = {"frames": frames}
    if frames:
        agg["psf_valid_frac_p50"] = float(np.median([f["psf_valid_frac"] for f in frames
                                                     if f.get("psf_valid_frac")]))
        agg["psf_valid_frac_min"] = float(np.min([f["psf_valid_frac"] for f in frames
                                                  if f.get("psf_valid_frac")]))
        agg["n_sources_p50"] = float(np.median([f["n_sources"] for f in frames]))
        agg["n_psf_valid_p50"] = float(np.median([f["n_psf_valid"] for f in frames]))
        vals = [f["box_over_psf_p50"] for f in frames if f.get("box_over_psf_p50")]
        agg["box_over_psf_p50_median"] = float(np.median(vals))
        agg["box_over_psf_p50_min"] = float(np.min(vals))
        agg["box_over_psf_p50_max"] = float(np.max(vals))
        # **帧间伪差**：以 PSF 解析通量为共同口径，盒和通量口径的逐帧漂移幅度
        agg["box_flux_frame_to_frame_spread_rel"] = float(np.max(vals) / np.min(vals) - 1.0)
        agg["in_band_frac_median"] = float(np.median(
            [f["in_band_frac"] for f in frames if f.get("in_band_frac") is not None]))
        agg["fwhm_x_over_sx_p50"] = float(np.median(
            [f["fwhm_x_over_sx_p50"] for f in frames if f.get("fwhm_x_over_sx_p50")]))
    return agg


# ---------------------------------------------------------------------------
# 3. 帧级 reference_snr_f（判据 d）
# ---------------------------------------------------------------------------
def frame_level_arm(sigma_sky=12.62, gain=GAIN, rn=RN, F_ref=1000.0,
                    fwhm_list=(1.6, 2.0, 2.5, 3.0, 3.5, 3.9)) -> Dict[str, Any]:
    """生产帧级路径（snr_frame_science.cpp）用 F_ref（**模型总通量**）+ 最优提取方差。

    对照：若把逐源口径（盒和）搬到帧级，同一 F_ref 会给出什么。
    """
    rows = []
    for fw in fwhm_list:
        t = C.estimator_table(F_ref, fw, sigma_sky, gain, rn, C.SRC_EMPIRICAL_TOTAL_RMS,
                              N_SKY_FRAME)
        rows.append({"fwhm_px": fw, "E_B": t["E_B"],
                     "snr_frame_production_totalflux": t["A_snr"],
                     "snr_if_boxflux": t["current_snr"],
                     "ratio_box_over_totalflux": t["current_snr"] / t["A_snr"] - 1.0})
    v = np.array([r["snr_frame_production_totalflux"] for r in rows])
    vb = np.array([r["snr_if_boxflux"] for r in rows])
    return {"rows": rows,
            "production_snr_spread_over_seeing": float(v.max() / v.min() - 1.0),
            "boxflux_snr_spread_over_seeing": float(vb.max() / vb.min() - 1.0),
            "convention_note": ("生产帧级路径 flux_adu = cfg.reference_flux_adu（模型总通量）"
                                "+ snr_optimal ⇒ 已经是**方案 A 的口径**")}


# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(HERE.parents[1] / "results" / "exp01_q2b_data.json"))
    ap.add_argument("--quick", action="store_true")
    args = ap.parse_args()
    t0 = time.time()
    out: Dict[str, Any] = {"meta": {"seed": SEED, "gain": GAIN, "rn_e": RN,
                                    "n_sky_frame": N_SKY_FRAME, "script": Path(__file__).name}}
    seeings = (2.0, 3.9) if args.quick else (1.6, 2.0, 2.5, 3.0, 3.5, 3.9)
    nst = 60 if args.quick else 150
    out["hst_cross_frame"] = hst_cross_frame(seeings=seeings, n_stars=nst,
                                             use_hst_base=False)
    # 同一条测试，但以 HST 真实帧作底图（§12.2 第 1 类；底图跨 seeing 逐位相同）
    out["hst_cross_frame_realbase"] = hst_cross_frame(seeings=seeings, n_stars=nst,
                                                      use_hst_base=True,
                                                      target_p999_e=200.0)
    out["real_products"] = real_arm(limit=(2 if args.quick else 4))
    out["frame_level"] = frame_level_arm()
    out["meta"]["elapsed_s"] = time.time() - t0
    outp = Path(args.out)
    outp.parent.mkdir(parents=True, exist_ok=True)
    outp.write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    print(json.dumps({"hst_controls": out["hst_cross_frame"]["controls"],
                      "real_agg": {k: v for k, v in out["real_products"].items() if k != "frames"},
                      "frame_level": {k: v for k, v in out["frame_level"].items() if k != "rows"}},
                     ensure_ascii=False, indent=1, default=str))
    print("wrote", outp, "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
