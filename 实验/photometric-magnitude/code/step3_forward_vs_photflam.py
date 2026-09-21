# -*- coding: utf-8 -*-
"""SCI-A 步骤 3 · 正向合成 vs HST PHOTFLAM 定标通量（跨滤镜/跨星色对拍）

问题：AstroCS 的合成通量约定 F_syn = ∫S(λ)·T(λ)·Q(λ)·λ dλ 里，S(λ) 是 Gaia XP 谱。
      XP 谱的**绝对通量刻度**是否可信？本仓 06.md §6 把它列为"判不了"。
做法：用真实 HST WFC3/UVIS F657N、F673N drz（BUNIT=e-/s，带 PHOTFLAM/PHOTPLAM/PHOTBW）
      对同一批 Gaia XP 星做引导 PSF 测光，得到实测计数率 CR_meas；
      HST 的 PHOTFLAM 定义把 CR 与物理流量密度绑定：
          f_lambda = PHOTFLAM · CR            [erg cm^-2 s^-1 A^-1]
      等价地（同一约定的反解，无自由参数）：
          A_eff = hc / (PHOTFLAM · ∫ λ T dλ)
          CR_pred = ∫ f_lambda^XP(λ) · λ · T(λ) dλ / (PHOTFLAM · ∫ λ T dλ)
      残差 Δmag = -2.5·log10(CR_pred/CR_meas) 即"XP 绝对刻度 vs HST 绝对刻度"的差。
      再按 XP 谱自身的颜色指数分箱，给出色项。
独立校验：SVO FPS 的 HST/WFC3_UVIS2 通带曲线的**枢轴波长**与文件头 PHOTPLAM 逐位对比
      （枢轴波长与曲线整体归一无关 ⇒ 能检验曲线形状）；由 PHOTFLAM 反推的有效面积
      与 HST 几何集光面积对比（检验绝对刻度）。

产出：results/step3_forward_vs_photflam.json
"""
from __future__ import annotations

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scia_common as sc
import scia_gaia as sg
from scia_common import fit_psf

HST_DIR = "testdata/HST_M16"
FILTERS = ("F657N", "F673N", "F502N")   # F502N 为附加第三带（同一代码路径，增加统计量）
HC_ERG_CM = 6.62607015e-27 * 2.99792458e10        # erg·cm
GEOM_AREA_CM2 = np.pi * (120.0 ** 2) * 0.85        # 2.4 m 主镜、~15% 中心遮挡的几何集光面积


def svo_curve(filt):
    """读取并校验 SVO FPS 通带曲线（缓存于 run/SCI-401/data_cache，SHA256 固定）。"""
    p = os.path.join(sc.CACHE, f"svo_HST_WFC3_UVIS2_{filt}.txt")
    if not os.path.exists(p):
        raise FileNotFoundError(
            f"缺少外部通带曲线 {p}。复现命令：bash {sc.CODE}/step0_fetch_refs.sh")
    d = np.loadtxt(p)
    return d[:, 0], d[:, 1], sc.sha256_file(p)


def pivot_and_area(wl, T, photflam):
    """枢轴波长 λ_p = sqrt(∫λT dλ / ∫(T/λ) dλ)；由 PHOTFLAM 反推有效面积。"""
    I = float(np.trapezoid(wl * T, wl))            # Å²
    lp = float(np.sqrt(I / np.trapezoid(T / wl, wl)))
    # PHOTFLAM 以 erg cm^-2 s^-1 A^-1 计；∫λT dλ 用 Å²，换算到 cm 单位需 ×1e-8
    A = HC_ERG_CM / (photflam * I * 1e-8)          # cm²
    return lp, A, I


def refine_shift(w, d, ra, dec, tol=4.0, iters=4):
    """第二轮 WCS 精化：用盲检测峰 + Gaia 位置稳健拟合平移（设计 §4.2 的两轮解算）。

    HLSP drz 的头部 WCS 与 Gaia DR3 之间有 ~1" 量级的系统偏移；星表引导检测前
    必须先把这个偏移消掉，否则拟合的是错的像素。
    """
    from scipy.ndimage import maximum_filter
    from scipy.spatial import cKDTree
    sub = np.asarray(d[::2, ::2], dtype=float)
    mx = maximum_filter(sub, 7)
    ys, xs = np.nonzero((sub == mx) & (sub > 0.5))
    PX, PY = xs * 2.0, ys * 2.0
    t = cKDTree(np.c_[PX, PY])
    x0, y0 = w.all_world2pix(ra, dec, 0)
    H, W = d.shape
    inb = np.isfinite(x0) & np.isfinite(y0) & (x0 > 30) & (x0 < W - 30) & (y0 > 30) & (y0 < H - 30)
    xi, yi = x0[inb], y0[inb]
    sh = np.array([0.0, 0.0]); n_last = 0
    # 粗到细的容差梯：头部 WCS 与 Gaia 的系统偏移可达数十像素，必须先粗搜
    for tt in (60.0, 20.0, 8.0, tol, tol):
        dd, jj = t.query(np.c_[xi + sh[0], yi + sh[1]], distance_upper_bound=tt)
        ok = np.isfinite(dd)
        n_last = int(ok.sum())
        if n_last < 5:
            continue
        dx = PX[jj[ok]] - (xi[ok] + sh[0]); dy = PY[jj[ok]] - (yi[ok] + sh[1])
        step = np.array([np.median(dx), np.median(dy)])
        sh = sh + step
        if np.hypot(*step) < 0.05:
            break
    return sh, n_last, int(inb.sum()), float(tol)


def measure_hst_flux(filt, ra, dec, magG, n_shape=25):
    """对给定 Gaia 位置做 HST 引导 PSF 测光，返回 CR [e-/s] 与质量。"""
    from astropy.io import fits
    from astropy.wcs import WCS
    p = os.path.join(sc.REPO, HST_DIR, f"hlsp_heritage_hst_wfc3-uvis_m16_{filt.lower()}_v1_drz.fits")
    h = fits.open(p, memmap=True)
    hdr = h[0].header
    w = WCS(hdr)
    d = h[0].data
    shift, n_match, n_in, tol = refine_shift(w, d, ra, dec)
    x, y = w.all_world2pix(ra, dec, 0)
    x = x + shift[0]; y = y + shift[1]
    H, W = d.shape
    ok = np.isfinite(x) & np.isfinite(y) & (x > 20) & (x < W - 20) & (y > 20) & (y < H - 20)
    idx = np.nonzero(ok)[0]
    # --- 用最亮的孤立星标定 PSF 形状 (fwhm, beta) ---
    from scipy.optimize import least_squares
    order = idx[np.argsort(magG[idx])][:n_shape]
    shapes = []
    for i in order:
        s = int(round(x[i])); t = int(round(y[i]))
        stamp = np.asarray(d[t - 12:t + 13, s - 12:s + 13], float)
        if stamp.shape != (25, 25) or not np.all(np.isfinite(stamp)):
            continue
        g = np.arange(-12, 13, dtype=float)
        dx, dy = np.meshgrid(g, g)

        def resid(p):
            xx, yy, fw, be, amp, bg = p
            if fw <= 0.3 or be <= 1.05:
                return np.full(stamp.size, 1e6)
            m = amp * sc.moffat_profile(dx - xx, dy - yy, fw, be) + bg
            return (m - stamp).ravel()
        try:
            r = least_squares(resid, [0.0, 0.0, 2.0, 2.5, float(stamp.max()),
                                      float(np.median(stamp))],
                              bounds=([-3, -3, 0.8, 1.1, 0, -np.inf],
                                      [3, 3, 6.0, 8.0, np.inf, np.inf]),
                              max_nfev=600)
            if r.success:
                shapes.append(r.x[2:4])
        except Exception:
            pass
    if not shapes:
        h.close(); return None
    fwhm, beta = np.median(np.asarray(shapes), axis=0)
    # --- 逐星拟合 ---
    CR = np.full(idx.size, np.nan); okf = np.zeros(idx.size, bool); chi = np.full(idx.size, np.nan)
    BGV = np.full(idx.size, np.nan)
    for j, i in enumerate(idx):
        s = int(round(x[i])); t = int(round(y[i]))
        stamp = np.asarray(d[t - 12:t + 13, s - 12:s + 13], float)
        if stamp.shape != (25, 25) or not np.all(np.isfinite(stamp)):
            continue
        g = np.arange(-12, 13, dtype=float)
        dx, dy = np.meshgrid(g, g)

        def resid(p):
            xx, yy, amp, bg = p
            return (amp * sc.moffat_profile(dx - xx, dy - yy, fwhm, beta) + bg - stamp).ravel()
        try:
            r = least_squares(resid, [0.0, 0.0, float(stamp.max()), float(np.median(stamp))],
                              bounds=([-3, -3, 0, -np.inf], [3, 3, np.inf, np.inf]),
                              max_nfev=300)
            if r.success and np.all(np.isfinite(r.x)):
                CR[j] = r.x[2]; okf[j] = True
                BGV[j] = r.x[3]
                chi[j] = float(np.sqrt(np.mean(r.fun ** 2)))
        except Exception:
            pass
    h.close()
    return dict(idx=idx, x=x[idx], y=y[idx], CR=CR, ok=okf, chi=chi, bg=BGV,
                psf=dict(fwhm=float(fwhm), beta=float(beta), n_shape=len(shapes)),
                wcs_shift_px=[float(shift[0]), float(shift[1])],
                wcs_shift_arcsec=float(np.hypot(*shift) * 0.04),
                n_matched_shift=n_match, n_in_frame=n_in, shift_tol_px=tol)


def main():
    res = {"step": "3_forward_vs_photflam", "seed": sc.SCIA_SEED}
    cat = sg.XpCatalog(sg.dump_cone(274.7216, -13.8415, 0.075, 21.5, "m16"))
    from astropy.io import fits
    curves, hdrs = {}, {}
    for filt in FILTERS:
        wl, T, sha = svo_curve(filt)
        h = fits.open(os.path.join(sc.REPO, HST_DIR,
                                   f"hlsp_heritage_hst_wfc3-uvis_m16_{filt.lower()}_v1_drz.fits"),
                      memmap=True)
        hd = h[0].header
        hdrs[filt] = dict(PHOTFLAM=float(hd["PHOTFLAM"]), PHOTPLAM=float(hd["PHOTPLAM"]),
                          PHOTBW=float(hd["PHOTBW"]), EXPTIME=float(hd["EXPTIME"]),
                          BUNIT=hd["BUNIT"], FILTER=hd["FILTER"])
        h.close()
        lp, A, I = pivot_and_area(wl, T, hdrs[filt]["PHOTFLAM"])
        curves[filt] = dict(wl=wl, T=T, sha256=sha, pivot=lp, area_cm2=A, int_lambda_T=I,
                            npts=int(wl.size), wl_min=float(wl.min()), wl_max=float(wl.max()),
                            T_max=float(T.max()))
    res["filters"] = list(FILTERS)
    res["throughput_curves"] = {
        "source": "SVO Filter Profile Service (HST/WFC3_UVIS2.<filt>, ascii)",
        "url_template": "https://svo2.cab.inta-csic.es/svo/theory/fps/getdata.php?format=ascii&id=HST/WFC3_UVIS2.{filt}",
        "note": "枢轴波长与曲线整体归一无关 ⇒ 用它检验曲线**形状**；由 PHOTFLAM 反推的有效面积检验**绝对刻度**",
        "validation": []}
    for filt in ("F657N", "F673N"):
        c = curves[filt]; hd = hdrs[filt]
        res["throughput_curves"]["validation"].append(dict(
            filter=filt, sha256=c["sha256"], npts=c["npts"],
            wl_range_nm=[c["wl_min"] / 10.0, c["wl_max"] / 10.0], T_max=c["T_max"],
            pivot_from_curve_A=c["pivot"], pivot_header_A=hd["PHOTPLAM"],
            pivot_rel_diff=abs(c["pivot"] / hd["PHOTPLAM"] - 1.0),
            area_from_photflam_cm2=c["area_cm2"], geometric_area_cm2=GEOM_AREA_CM2,
            area_ratio=c["area_cm2"] / GEOM_AREA_CM2,
            header=hd))

    # --- 逐星对拍 ---
    rows = []
    for filt in FILTERS:
        c = curves[filt]; hd = hdrs[filt]
        m = measure_hst_flux(filt, cat.ra, cat.dec, cat.magG)
        if m is None:
            continue
        res.setdefault("wcs_refinement", {})[filt] = dict(
            shift_px=m["wcs_shift_px"], shift_arcsec=m["wcs_shift_arcsec"],
            n_matched_within_tol=m["n_matched_shift"], n_in_frame=m["n_in_frame"],
            tol_px=m["shift_tol_px"], psf=m["psf"],
            note="HLSP drz 头部 WCS 与 Gaia DR3 的系统偏移；引导检测前必须精化（设计 §4.2 第二轮）")
        wl_h, T_h = c["wl"], c["T"]
        I = c["int_lambda_T"]
        for j, i in enumerate(m["idx"]):
            if not m["ok"][j] or not np.isfinite(m["CR"][j]) or m["CR"][j] <= 0:
                continue
            S = cat.spectrum(int(i))                                   # W m^-2 nm^-1
            # XP 网格单位是 nm，HST 曲线单位是 Å ⇒ 先统一到 nm 再换算流量密度单位
            fl = 100.0 * np.interp(wl_h / 10.0, cat.wl_nm, S, left=0.0, right=0.0)  # erg cm^-2 s^-1 A^-1
            num = float(np.trapezoid(fl * wl_h * T_h, wl_h))
            CR_pred = num / (hd["PHOTFLAM"] * I)
            if not np.isfinite(CR_pred) or CR_pred <= 0:
                continue
            dmag = -2.5 * np.log10(CR_pred / m["CR"][j])
            if not np.isfinite(dmag) or abs(dmag) > 10.0:
                continue
            # XP 谱颜色指数（500 nm 与 800 nm 处的相对通量斜率）
            f500 = float(np.interp(500.0, cat.wl_nm, S))
            f800 = float(np.interp(800.0, cat.wl_nm, S))
            color = -2.5 * np.log10(max(f500, 1e-30) / max(f800, 1e-30))
            rows.append(dict(filter=filt, gaia_i=int(i), ra=float(cat.ra[i]),
                             dec=float(cat.dec[i]), magG=float(cat.magG[i]),
                             CR_meas=float(m["CR"][j]), CR_pred=float(CR_pred),
                             dmag=float(dmag), chi=float(m["chi"][j]), color_500_800=color,
                             bg_local=float(m["bg"][j]), x=float(m["x"][j]), y=float(m["y"][j])))
    res["n_pairs"] = len(rows)
    by = {}
    rb = sc.rng("step3:bootstrap")
    for filt in FILTERS:
        sel = [r for r in rows if r["filter"] == filt]
        if len(sel) < 5:
            continue
        v = np.array([r["dmag"] for r in sel])
        col = np.array([r["color_500_800"] for r in sel])
        mg = np.array([r["magG"] for r in sel])
        bg = np.array([r["bg_local"] for r in sel])
        A = np.vstack([np.ones_like(col), col]).T
        coef, *_ = np.linalg.lstsq(A, v, rcond=None)
        resid = v - A @ coef
        bs = np.array([np.median(rb.choice(v, v.size, replace=True)) for _ in range(2000)])
        clean = (mg < 17.0) & (bg < np.median(bg))
        vc, colc = v[clean], col[clean]
        cc = dict(n=int(vc.size))
        if vc.size >= 6:
            Ac = np.vstack([np.ones_like(colc), colc]).T
            cf, *_ = np.linalg.lstsq(Ac, vc, rcond=None)
            cc.update(median_dmag=float(np.median(vc)), mad_sigma=float(sc.mad_sigma(vc)),
                      color_slope_mag_per_mag=float(cf[1]),
                      mad_after_detrend=float(sc.mad_sigma(vc - Ac @ cf)))
        by[filt] = dict(n=int(v.size), median_dmag=float(np.median(v)),
                        median_bootstrap_sigma=float(np.std(bs)),
                        mad_sigma_dmag=float(sc.mad_sigma(v)), std_dmag=float(np.std(v)),
                        color_slope_mag_per_mag=float(coef[1]),
                        color_intercept=float(coef[0]),
                        mad_after_detrend=float(sc.mad_sigma(resid)),
                        dmag_p10=float(np.percentile(v, 10)),
                        dmag_p90=float(np.percentile(v, 90)),
                        bright_G_lt_16_median=(float(np.median(v[mg < 16.0]))
                                               if (mg < 16.0).sum() >= 4 else None),
                        bright_G_lt_16_n=int((mg < 16.0).sum()),
                        clean_sample=cc)
    res["per_filter"] = by
    if "F657N" in by and "F673N" in by:
        d1 = {r["gaia_i"]: r["dmag"] for r in rows if r["filter"] == "F657N"}
        d2 = {r["gaia_i"]: r["dmag"] for r in rows if r["filter"] == "F673N"}
        common = sorted(set(d1) & set(d2))
        dd = np.array([d1[i] for i in common]); ee = np.array([d2[i] for i in common])
        res["cross_filter"] = dict(
            note="**同一批星**在两个滤镜上的 XP-vs-PHOTFLAM 残差之差（跨滤镜项，星内相消）",
            n=int(len(common)), median_diff=float(np.median(dd - ee)),
            mad_diff=float(sc.mad_sigma(dd - ee)), std_diff=float(np.std(dd - ee)))
    res["stars"] = rows[:400]
    sc.jdump(res, os.path.join(sc.RESULTS, "step3_forward_vs_photflam.json"))


if __name__ == "__main__":
    main()
