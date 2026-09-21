#!/usr/bin/env python3
# 实验/SCI-C/code/c3_public_plane.py
"""C3 多退少补与公共面：calibrated = raw − δ_k，B_ref 保留，gauge 承载零点。

判据：
  B1  生产 p2_sky_plane_build rc=0 且每帧都有 δ_k 系数（无帧欠定）
  B2  b_k(x) − δ_k(x) 与帧无关（== B_ref），帧间最大偏差 < 1e-9 [e-]
  B3  calibrated=raw−δ_k 的叠加产品**保留背景**：中位 >> 0，且 ≈ B_ref 中位
  B4  全减臂 calibrated=raw−b_k 的产品中位 ≈ 0（对照：全减即"背景没了"）
  B5  无大面积负值：产品 <0 像素占比 < 1e-3
  B6  gauge 约定承载零点：reference_frame 与 sum_zero 两规范只差常数
      （gauge_shift），接缝严格不变（|Δseam| < 1e-9）
  B7  多退少补把低阶帧间梯度扣除：δ_k 臂接缝 < 未校正臂接缝
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sci_c_common as S
from c1_additive import CFG_FULL, scenario

RA0, DEC0 = 274.0, -13.0
SCALE_DEG = 1.0 / 3600.0     # 1 arcsec/px
NODE_SPACING_DEG = 0.0355    # ≈128 px（与 8x8 control cell 尺度同量级）


def pix_to_sky(x, y):
    """TAN 投影（小视场，astropy.wcs 精确式）。"""
    from astropy.wcs import WCS
    w = WCS(naxis=2)
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    w.wcs.crval = [RA0, DEC0]
    w.wcs.crpix = [S.TILE_PX / 2.0, S.TILE_PX / 2.0]
    w.wcs.cdelt = [-SCALE_DEG, SCALE_DEG]
    ra, dec = w.all_pix2world(x, y, 0)
    return ra, dec


def make_samples(world, step=16, win=9):
    """星点掩膜 + 稀疏采样（每帧都取）：值=局部稳健背景，variance=生产冻结式。"""
    from scipy.ndimage import binary_dilation
    names = world["names"]
    ys = np.arange(win // 2, S.TILE_PX - win // 2, step)
    xs = np.arange(win // 2, S.TILE_PX - win // 2, step)
    yy, xx = np.meshgrid(ys, xs, indexing="ij")
    ra, dec = pix_to_sky(xx.ravel().astype(float), yy.ravel().astype(float))
    m = world["mask"]
    cov = {nm: S.coverage_mask(nm) for nm in names}
    samples = []
    for i, nm in enumerate(names):
        img = world["frames"][nm]
        for k in range(xx.size):
            x, y = int(xx.ravel()[k]), int(yy.ravel()[k])
            gx, gy = x // S.CELL, y // S.CELL
            if not cov[nm][gy, gx]:
                continue
            if m[y - win // 2:y + win // 2 + 1, x - win // 2:x + win // 2 + 1].any():
                continue
            v = img[y - win // 2:y + win // 2 + 1, x - win // 2:x + win // 2 + 1]
            val = float(np.median(v))
            sig = float(1.4826 * np.median(np.abs(v - val)))
            nret = int(v.size)
            var = S.K_CORR * (np.pi / 2) * max(sig, 1e-6) ** 2 / nret
            samples.append(dict(frame_id=S.FRAME_IDS[nm], control_id=k + 1,
                                ra_deg=float(ra[k]), dec_deg=float(dec[k]),
                                value=val, variance=float(var),
                                snr=float(max(val, 1e-6) / np.sqrt(var)), flags=0))
    return samples, ra, dec


def main():
    g = S.Gates()
    res = {}
    world = S.build_world(seed_tag="c3")
    samples, ra, dec = make_samples(world)
    frames = [S.FRAME_IDS[n] for n in world["names"]]
    res["sampling"] = dict(n_samples=len(samples),
                           per_frame={n: int(sum(1 for s in samples
                                                 if s["frame_id"] == S.FRAME_IDS[n]))
                                      for n in world["names"]})
    # 叠加/接缝需要**全网格** δ_k、b_k：探针点用 512² 网格（现场求值，稀疏模型）
    yy_g, xx_g = np.mgrid[0:S.TILE_PX, 0:S.TILE_PX]
    ra_f, dec_f = pix_to_sky(xx_g.ravel().astype(float), yy_g.ravel().astype(float))
    cfg = dict(spline_degree=3, node_spacing_deg=NODE_SPACING_DEG,
               frame_gradient_order=1, roughness_penalty=1e-3, huber_delta=1.345,
               max_iterations=30, tolerance=1e-10, gauge_mode=0, weight_mode=0,
               kappa_max=1e8, rank_rtol=1e-10, min_samples=8,
               min_samples_per_frame=4, max_nodes=8192, max_extrapolation_deg=0.0)

    out0, delta0, b0 = S.run_sky_probe(
        dict(cfg=cfg, samples=samples, frames=frames,
             probe=dict(ra_deg=ra_f.tolist(), dec_deg=dec_f.tolist())), "c3_ref")
    res["sky_build"] = {k: out0.get(k) for k in
                        ("rc_build", "n_used", "n_frames", "n_nodes", "n_params",
                         "rank", "kappa", "rms_weighted", "chi2_red", "iterations",
                         "gauge_shift", "n_masked", "n_rejected", "model_hash")}
    g.add("B1_build_ok", "生产 p2_sky_plane_build rc=0，每帧 δ_k 非空",
          res["sky_build"]["rc_build"],
          out0.get("rc_build") == 0 and all(
              v is not None for v in out0.get("frame_delta_coeffs", {}).values()))

    # B2: b_k − δ_k 与帧无关
    nF = len(frames)
    npix = ra_f.size          # 探针在 512² 全网格上
    D = delta0.reshape(nF, npix)
    B = b0.reshape(nF, npix)
    bref = B - D
    dev = float(np.max(np.abs(bref - bref[0][None, :])))
    res["B_ref_frame_independence"] = dict(max_dev=dev,
                                           median=float(np.median(bref[0])))
    g.add("B2_Bref_shared", "b_k − δ_k 与帧无关（max 帧间偏差 < 1e-9）", dev, dev < 1e-9)

    # 叠加产品：保留背景 vs 全减
    grid = dict(x0=0, y0=0, nx=S.TILE_PX, ny=S.TILE_PX)
    wts = {k: v["control_ivar"] for k, v in world["ctrl"].items()}
    # stack_mosaic 语义 = raw − corr；传 δ_k 得 raw − δ_k（多退少补，保留 B_ref）
    mos_delta = S.stack_mosaic(world["frames"], world["names"], D.reshape(nF, S.TILE_PX, S.TILE_PX), wts)
    mos_full = S.stack_mosaic(world["frames"], world["names"], B.reshape(nF, S.TILE_PX, S.TILE_PX), wts)
    mos_none = S.stack_mosaic(world["frames"], world["names"],
                              [np.zeros((S.TILE_PX, S.TILE_PX))] * nF, wts)
    res["product"] = dict(
        delta_median=float(np.nanmedian(mos_delta)),
        full_median=float(np.nanmedian(mos_full)),
        none_median=float(np.nanmedian(mos_none)),
        Bref_median=float(np.median(bref[0])),
        delta_neg_frac=float(np.nanmean(mos_delta < 0)),
        full_neg_frac=float(np.nanmean(mos_full < 0)),
        delta_p01=float(np.nanpercentile(mos_delta, 1)))
    g.add("B3_background_kept", "calibrated=raw−δ_k 产品保留背景（中位 > 50 e- 且 ≈ B_ref 中位 ±20%）",
          dict(med=res["product"]["delta_median"], bref=res["product"]["Bref_median"]),
          res["product"]["delta_median"] > 50.0 and
          abs(res["product"]["delta_median"] / res["product"]["Bref_median"] - 1) < 0.2)
    g.add("B4_full_subtract_zero", "全减臂 raw−b_k 产品中位 ≈ 0（对照）",
          res["product"]["full_median"], abs(res["product"]["full_median"]) < 5.0)
    g.add("B5_no_negative", "产品 <0 像素占比 < 1e-3", res["product"]["delta_neg_frac"],
          res["product"]["delta_neg_frac"] < 1e-3)

    # B6: gauge 不变性
    out1, delta1, b1 = S.run_sky_probe(
        dict(cfg=dict(cfg, gauge_mode=1), samples=samples, frames=frames,
             probe=dict(ra_deg=ra_f.tolist(), dec_deg=dec_f.tolist())), "c3_sum")
    D1 = delta1.reshape(nF, npix)
    diffs = D1 - D
    span = float(np.max(np.ptp(diffs, axis=1)))
    mos1 = S.stack_mosaic(world["frames"], world["names"],
                          D1.reshape(nF, S.TILE_PX, S.TILE_PX), wts)
    s0 = [s["step"] for s in S.seam_steps(mos_delta)]
    s1 = [s["step"] for s in S.seam_steps(mos1)]
    dseam = float(np.nanmax(np.abs(np.array(s0) - np.array(s1))))
    res["gauge"] = dict(gauge_shift_ref=out0.get("gauge_shift"),
                        gauge_shift_sum=out1.get("gauge_shift"),
                        per_frame_const_spread=span, delta_seam=dseam,
                        seam_ref=s0, seam_sum=s1)
    g.add("B6_gauge_zero_point", "gauge 规范只差常数（帧内 δ 差 spread < 1e-9）且接缝不变（|Δ|<1e-9）",
          dict(spread=span, dseam=dseam), span < 1e-9 and dseam < 1e-9)

    # B7: δ_k 扣除降低接缝（多退少补）
    st_none = [s["step"] for s in S.seam_steps(mos_none)]
    st_del = [s["step"] for s in S.seam_steps(mos_delta)]
    res["seam"] = dict(none=st_none, delta=st_del,
                       none_med=float(np.nanmedian(np.abs(st_none))),
                       delta_med=float(np.nanmedian(np.abs(st_del))))
    g.add("B7_delta_reduces_seam", "δ_k 多退少补后接缝 < 未校正臂（比值 > 2）",
          dict(none=res["seam"]["none_med"], delta=res["seam"]["delta_med"]),
          res["seam"]["delta_med"] < res["seam"]["none_med"] / 2.0)

    res["gates"] = g.summary()
    p = S.json_dump(res, "c3_public_plane.json")
    print("== C3 多退少补与公共面 ==")
    for r in res["gates"]["rows"]:
        print("  [%s] %-26s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    print("  -> %s" % p)
    return 0 if res["gates"]["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
