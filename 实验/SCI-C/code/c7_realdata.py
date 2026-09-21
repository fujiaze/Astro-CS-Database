#!/usr/bin/env python3
# 实验/SCI-C/code/c7_realdata.py
"""C7 真实数据衔接：M42 真实帧的帧间失配量化 + 真实样本上的接缝度量分布。

数据：testdata/M42_T2T3_mosaic_Flying_dutchman/T3/M1/*Red.fts（只读，真实观测）
      与 testdata/Galaxy_Center_T4（若可用）。
**诚实边界**：覆盖图案（x 向条带）是人工施加的；帧间失配与接缝度量是真数据实测。
"看着没缝"只作互证，不作科学证据（判据是本文件算出的度量）。

判据（证据分级见 README §5）：
  R1  真实帧间背景失配被量化：乘性斜率 a 与加性偏移 c 的分布（含不确定度）
  R2  真实样本上生产天光面可解（rc=0，rank 满，每帧 δ_k 可辨识）
  R3  真实数据 + 人工覆盖图案：raw−δ_k 的接缝度量中位 < 0.5 × 未校正
  R4  度量红的位置目检确有可见台阶（裁图落 results/figs，与 VIS-401 同法对照）
  R5  度量分布与"无接缝"零假设一致（|step|/背景 < 1%）
"""
from __future__ import annotations

import glob
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sci_c_common as S
from c1_additive import SKY_CFG

CROP = (3000, 3000)     # (y0, x0) 背景为主区域
NC = 512
BOUND = [128, 192, 256, 320, 384, 448]


def phase_shift(ref, img, sigma=8.0):
    """FFT 相位相关求整数位移 (dy,dx)：把 img 对齐到 ref。"""
    from scipy.ndimage import gaussian_filter
    from numpy.fft import fft2, ifft2
    a = ref - gaussian_filter(ref, sigma)
    b = img - gaussian_filter(img, sigma)
    wy = np.hanning(a.shape[0])[:, None]
    wx = np.hanning(a.shape[1])[None, :]
    a = a * wy * wx
    b = b * wy * wx
    F = fft2(a) * np.conj(fft2(b))
    c = np.abs(ifft2(F / np.maximum(np.abs(F), 1e-12)))
    dy, dx = np.unravel_index(np.argmax(c), c.shape)
    if dy > a.shape[0] // 2:
        dy -= a.shape[0]
    if dx > a.shape[1] // 2:
        dx -= a.shape[1]
    return int(dy), int(dx)


def load_real_frames(n=4):
    from astropy.io import fits
    fs = sorted(glob.glob(str(S.ROOT / "testdata/M42_T2T3_mosaic_Flying_dutchman/T3/M1/*Red.fts")))
    if not fs:
        return None
    ref = None
    out = []
    for f in fs:
        with fits.open(f, memmap=False) as h:
            hdr = h[0].header
            d = np.asarray(h[0].data, dtype=np.float64)
        if ref is None:
            ref = hdr
        M = 96                      # 对齐余量
        dx = (hdr["CRVAL1"] - ref["CRVAL1"]) / hdr["CDELT1"]
        dy = (hdr["CRVAL2"] - ref["CRVAL2"]) / hdr["CDELT2"]
        ix, iy = int(round(dx)), int(round(dy))
        y0, x0 = CROP[0] + iy - M, CROP[1] + ix - M
        if y0 < 0 or x0 < 0 or y0 + NC + 2 * M > d.shape[0] or x0 + NC + 2 * M > d.shape[1]:
            continue
        big = d[y0:y0 + NC + 2 * M, x0:x0 + NC + 2 * M].copy()
        if not out:
            sy = sx = 0
            img = big[M:M + NC, M:M + NC].copy()
        else:
            sy, sx = phase_shift(out[0]["big"], big)
            img = big[M - sy:M - sy + NC, M - sx:M - sx + NC].copy()
        out.append(dict(path=f, name=Path(f).name, img=img, big=big,
                        wcs_shift=[float(dx), float(dy)],
                        phase_shift=[int(sy), int(sx)],
                        wcs=dict(crval=[hdr["CRVAL1"], hdr["CRVAL2"]],
                                 crpix=[hdr["CRPIX1"], hdr["CRPIX2"]],
                                 cdelt=[hdr["CDELT1"], hdr["CDELT2"]],
                                 ctype=[hdr.get("CTYPE1", "RA---TAN"), hdr.get("CTYPE2", "DEC--TAN")]),
                        exptime=hdr.get("EXPTIME")))
        if len(out) >= n:
            break
    return out


def real_pix_to_sky(fr):
    from astropy.wcs import WCS
    w = WCS(naxis=2)
    w.wcs.ctype = fr["wcs"]["ctype"]
    w.wcs.crval = fr["wcs"]["crval"]
    w.wcs.crpix = [fr["wcs"]["crpix"][0] - CROP[1], fr["wcs"]["crpix"][1] - CROP[0]]
    w.wcs.cdelt = fr["wcs"]["cdelt"]
    return w


def robust_linfit(x, y):
    """Huber IRLS 稳健 y = a x + c。"""
    A = np.stack([x, np.ones_like(x)], axis=1)
    w = np.ones_like(x)
    for _ in range(10):
        Aw = A * w[:, None]
        coef, *_ = np.linalg.lstsq(Aw, y * w, rcond=None)
        r = y - A @ coef
        s = 1.4826 * np.median(np.abs(r - np.median(r))) + 1e-12
        w = np.where(np.abs(r) <= 1.345 * s, 1.0, 1.345 * s / np.maximum(np.abs(r), 1e-12))
    return float(coef[0]), float(coef[1])


def main():
    g = S.Gates()
    res = {}
    frames = load_real_frames(4)
    if not frames:
        print("SKIP: 未找到 M42 真实帧")
        return 2
    res["data"] = [dict(name=f["name"], exptime=f["exptime"],
                        wcs_shift=f.get("wcs_shift"), phase_shift=f.get("phase_shift"))
                   for f in frames]

    # ---- R1 帧间失配（乘性 a / 加性 c）
    from scipy.ndimage import binary_dilation, maximum_filter
    pairs = []
    masks = []
    for f in frames:
        m = binary_dilation(f["img"] > (np.median(f["img"]) +
                                        8 * 1.4826 * np.median(np.abs(f["img"] - np.median(f["img"])))),
                            iterations=3)
        masks.append(m)
    # 分块中位（PMM 式 bin）：像素级回归会被噪声主导（结构像素被掩膜），
    # 分块中位 SNR 高且跨越背景梯度/星云电平范围 ⇒ 可辨识乘性斜率与加性偏移。
    BS = 64
    for i in range(len(frames)):
        for j in range(i + 1, len(frames)):
            m = masks[i] | masks[j]
            a_img, b_img = frames[i]["img"], frames[j]["img"]
            bx, by = [], []
            for yy0 in range(0, NC - BS + 1, BS):
                for xx0 in range(0, NC - BS + 1, BS):
                    sl = (slice(yy0, yy0 + BS), slice(xx0, xx0 + BS))
                    good = (~m[sl]) & np.isfinite(a_img[sl]) & np.isfinite(b_img[sl])
                    if good.sum() < 0.3 * BS * BS:
                        continue
                    bx.append(float(np.median(b_img[sl][good])))
                    by.append(float(np.median(a_img[sl][good])))
            bx = np.array(bx)
            by = np.array(by)
            if bx.size < 8:
                continue
            a, c = robust_linfit(bx, by)
            lv_i = float(np.median(a_img[~m]))
            lv_j = float(np.median(b_img[~m]))
            pairs.append(dict(i=frames[i]["name"], j=frames[j]["name"],
                              slope=a, intercept=c, n_bins=int(bx.size),
                              level_i=lv_i, level_j=lv_j,
                              level_ratio=lv_i / lv_j if lv_j else np.nan,
                              offset_frac=c / lv_j if lv_j else np.nan,
                              bin_range=[float(bx.min()), float(bx.max())]))
    res["pair_mismatch"] = pairs
    slopes = np.array([p["slope"] for p in pairs])
    offs = np.array([p["offset_frac"] for p in pairs])
    lv = np.array([p["level_ratio"] for p in pairs])
    rho_si = float(np.corrcoef(slopes, offs)[0, 1]) if slopes.size > 2 else np.nan
    span = float(np.median([p["bin_range"][1] - p["bin_range"][0] for p in pairs]))
    res["mismatch_summary"] = dict(
        slope=S.stats(slopes), offset_frac=S.stats(offs), level_ratio=S.stats(lv),
        corr_slope_intercept=rho_si, median_bin_span_adu=span,
        identifiability_note="分块中位动态范围仅 %.1f ADU ⇒ 斜率与截距强负相关（ρ=%.4f），"
                             "截距/加性偏移**不可辨识**；良态量是 level_ratio（帧间背景电平比）。"
                             % (span, rho_si))
    g.add("R1_real_mismatch_quantified",
          "真实帧间失配被量化：良态量 level_ratio（帧间背景电平比）+ 斜率（声明其杠杆臂窄）",
          dict(level_ratio_med=float(np.median(lv)), level_ratio_min=float(np.min(lv)),
               level_ratio_max=float(np.max(lv)), slope_med=float(np.median(slopes)),
               corr_slope_intercept=rho_si, bin_span_adu=span),
          bool(np.all(np.isfinite(lv)) and np.all(np.isfinite(slopes))))

    # ---- 真实样本 + 人工覆盖图案 → 生产天光面 → 接缝度量
    names = ["A", "B", "C", "D"][:len(frames)]
    yy, xx = np.mgrid[0:NC, 0:NC]
    step, win = 16, 9
    ys = np.arange(win // 2, NC - win // 2, step)
    xs = np.arange(win // 2, NC - win // 2, step)
    gy_g, gx_g = np.meshgrid(ys, xs, indexing="ij")
    samples = []
    for k, f in enumerate(frames):
        nm = names[k]
        w = real_pix_to_sky(f)
        ra, dec = w.all_pix2world(gx_g.ravel().astype(float), gy_g.ravel().astype(float), 0)
        img = f["img"]
        m = masks[k]
        cov = S.coverage_mask(nm)
        for t in range(gx_g.size):
            x, y = int(gx_g.ravel()[t]), int(gy_g.ravel()[t])
            gx, gy = x // S.CELL, y // S.CELL
            if not cov[gy, gx] or m[y - win // 2:y + win // 2 + 1,
                                    x - win // 2:x + win // 2 + 1].any():
                continue
            v = img[y - win // 2:y + win // 2 + 1, x - win // 2:x + win // 2 + 1]
            val = float(np.median(v))
            sig = float(1.4826 * np.median(np.abs(v - val)))
            var = S.K_CORR * (np.pi / 2) * max(sig, 1e-6) ** 2 / int(v.size)
            samples.append(dict(frame_id=S.FRAME_IDS[nm], control_id=t + 1,
                                ra_deg=float(ra[t]), dec_deg=float(dec[t]), value=val,
                                variance=float(var), snr=float(max(val, 1e-6) / np.sqrt(var)),
                                flags=0))
    wref = real_pix_to_sky(frames[0])
    ra_f, dec_f = wref.all_pix2world(xx.ravel().astype(float), yy.ravel().astype(float), 0)
    o, D, B = S.run_sky_probe(
        dict(cfg=SKY_CFG, samples=samples, frames=[S.FRAME_IDS[n] for n in names],
             probe=dict(ra_deg=ra_f.tolist(), dec_deg=dec_f.tolist())), "c7_real")
    res["sky_build_real"] = {k: o.get(k) for k in
                             ("rc_build", "n_used", "n_frames", "n_nodes", "n_params",
                              "rank", "kappa", "chi2_red", "iterations", "model_hash")}
    g.add("R2_real_build",
          "真实样本上生产天光面 rc=0、rank == n_nodes（约化系统满秩）、每帧 δ_k 可辨识",
          res["sky_build_real"],
          o.get("rc_build") == 0 and o.get("rank") == o.get("n_nodes") and
          o.get("n_frames") == len(names) and
          all(v is not None for v in o.get("frame_delta_coeffs", {}).values()))

    nF = len(names)
    Dg = D.reshape(nF, NC, NC)
    Bg = B.reshape(nF, NC, NC)
    # 人工覆盖图案下的叠加（真实像素，权重用样本 variance 的逆）
    wt = {}
    for s_ in samples:
        nm = [k for k, v in S.FRAME_IDS.items() if v == s_["frame_id"]][0]
        gx, gy = None, None
        idx = s_["control_id"] - 1
        gx = int(gx_g.ravel()[idx]) // S.CELL
        gy = int(gy_g.ravel()[idx]) // S.CELL
        wt[(nm, gx, gy)] = 1.0 / s_["variance"]
    fr = {nm: frames[k]["img"] for k, nm in enumerate(names)}
    z = [np.zeros((NC, NC))] * nF
    mos_un = S.stack_mosaic(fr, names, z, wt, nx=NC, ny=NC)
    mos_d = S.stack_mosaic(fr, names, Dg, wt, nx=NC, ny=NC)
    st_un = S.seam_steps(mos_un, boundaries=BOUND)
    st_d = S.seam_steps(mos_d, boundaries=BOUND)
    res["real_seam"] = dict(
        uncorrected=st_un, raw_minus_delta=st_d,
        un_excess_med=float(np.nanmedian(np.abs([s["excess"] for s in st_un]))),
        d_excess_med=float(np.nanmedian(np.abs([s["excess"] for s in st_d]))),
        un_med=float(np.nanmedian(np.abs([s["step"] for s in st_un]))),
        d_med=float(np.nanmedian(np.abs([s["step"] for s in st_d]))),
                            level=float(np.nanmedian(mos_d)),
                            d_rel_med=float(np.nanmedian([abs(s["rel"]) for s in st_d
                                                          if np.isfinite(s["rel"])])))
    g.add("R3_real_seam_reduced",
          "真实数据：raw−δ_k 的 **excess**（off-locus 对照后）接缝中位 < 0.5 × 未校正",
          dict(un_step=res["real_seam"]["un_med"], d_step=res["real_seam"]["d_med"],
               un_excess=res["real_seam"]["un_excess_med"],
               d_excess=res["real_seam"]["d_excess_med"]),
          res["real_seam"]["d_excess_med"] < 0.5 * res["real_seam"]["un_excess_med"])
    g.add("R5_seam_below_1pct", "真实数据接缝度量中位 |step|/背景 < 1%",
          res["real_seam"]["d_rel_med"], res["real_seam"]["d_rel_med"] < 0.01)

    # ---- R4 裁图（目检互证，不作科学证据）
    S.FIGS.mkdir(parents=True, exist_ok=True)
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    worst = max(st_d, key=lambda s: abs(s["step"]) if np.isfinite(s["step"]) else -1)
    xb = worst["x"]
    lo, hi = max(0, xb - 96), min(NC, xb + 96)
    fig, ax = plt.subplots(1, 2, figsize=(11, 5))
    for k, (m, ttl) in enumerate(((mos_un, "raw (uncorrected)"),
                                  (mos_d, "raw - delta_k (multi-refund)"))):
        sub = m[192:320, lo:hi]
        im = ax[k].imshow(sub, origin="lower", cmap="gray",
                          vmin=np.nanpercentile(sub, 5), vmax=np.nanpercentile(sub, 95))
        ax[k].axvline(xb - lo, color="r", lw=1.0, ls="--")
        ax[k].set_title("%s  x=%d  step=%.3g" % (ttl, xb,
                                                 [s["step"] for s in (st_un if k == 0 else st_d)
                                                  if s["x"] == xb][0]))
        plt.colorbar(im, ax=ax[k], fraction=0.046)
    fig.suptitle("C7 real data (M42 M1 Red) seam crop at metric-flagged boundary")
    fig.tight_layout()
    fig.savefig(S.FIGS / "fig_c7_real_crop.png", dpi=110)
    plt.close(fig)
    res["crop"] = dict(x=worst["x"], step=worst["step"], rel=worst["rel"],
                       fig=str(S.FIGS / "fig_c7_real_crop.png"))
    g.add("R4_crop_saved", "度量最红位置的裁图已落 results/figs（目检互证）",
          res["crop"], Path(S.FIGS / "fig_c7_real_crop.png").exists())

    res["gates"] = g.summary()
    p = S.json_dump(res, "c7_realdata.json")
    print("== C7 真实数据衔接 ==")
    for r in res["gates"]["rows"]:
        print("  [%s] %-26s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    print("  -> %s" % p)
    return 0 if res["gates"]["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
