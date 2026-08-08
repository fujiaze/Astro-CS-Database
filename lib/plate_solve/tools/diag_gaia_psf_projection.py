# NON_PRODUCTION_TOOL_ONLY
# -*- coding: utf-8 -*-
"""
视觉验证: Gaia 星点重投影 vs sdet 检测 vs DPSF 拟合

用途: 回答 PSF-001 是否影响 platesolve 的视觉验证。
  1. 生产 DLL 重跑: star_detector 检测 -> ipv_solve_from_detections_v1 求解
     (完整 WCS+SIP) -> dynamic_psf 拟合 (psf cx/cy)
  2. 用求解 WCS (astropy) 把 Gaia 星重投影到像素
  3. 叠加三组标记: Gaia(+), sdet(o), PSF(x), 并画 sdet->psf 偏移

坐标约定: 统一到"像素中心坐标 = 像素索引 + 0.5" (sdet/DPSF 输出约定),
astropy WCS 的 FITS 像素坐标减去 0.5 对齐。

用法:
  py -3.12 diag_gaia_psf_projection.py --fits <galaxy_crop_1024.fits> --out <out.png>
"""

import argparse
import ctypes
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from astropy.io import fits as astropy_fits
from astropy.wcs import WCS, Sip

ROOT = r"F:\Astro dev\Astro CS Normalization Database"
plt.rcParams["font.sans-serif"] = ["Microsoft YaHei", "SimHei", "DejaVu Sans"]
plt.rcParams["axes.unicode_minus"] = False


def add_dll_dirs():
    for d in (ROOT + r"\lib\astro_image_io",
              ROOT + r"\lib\star_detector",
              ROOT + r"\lib\dynamic_psf",
              ROOT + r"\lib\plate_solve\cpp\ipv",
              ROOT + r"\lib\photometric_calib\cpp",
              r"C:\msys64\mingw64\bin",
              r"C:\msys64\mingw64\lib"):
        if os.path.isdir(d):
            os.add_dll_directory(d)
            os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")


class SDetParams(ctypes.Structure):
    _fields_ = [("structureLayers", ctypes.c_int),
                ("hotPixelFilterRadius", ctypes.c_int),
                ("iterativeClipSigma", ctypes.c_float),
                ("iterativeMaxRounds", ctypes.c_int),
                ("medianFilterDetail", ctypes.c_int),
                ("maxStars", ctypes.c_int),
                ("fitRadius", ctypes.c_int),
                ("fwhmClipSigma", ctypes.c_float),
                ("maxAxisRatio", ctypes.c_float)]


class IpvParams(ctypes.Structure):
    _fields_ = [("polygon_sides", ctypes.c_int),
                ("n_pivot", ctypes.c_int),
                ("sigma_d_arcsec", ctypes.c_double),
                ("vote_threshold", ctypes.c_int),
                ("ransac_max_iter", ctypes.c_int),
                ("ransac_inlier_threshold_arcsec", ctypes.c_double),
                ("s_min", ctypes.c_double),
                ("s_max", ctypes.c_double),
                ("img_n_target", ctypes.c_int),
                ("gaia_density_ratio", ctypes.c_double),
                ("gaia_query_radius_factor", ctypes.c_double),
                ("m_lim_step", ctypes.c_double),
                ("m_lim_max_iter", ctypes.c_int),
                ("density_tolerance", ctypes.c_double),
                ("log_dir", ctypes.c_char * 256)]


class IpvWcsResult(ctypes.Structure):
    _fields_ = [("cd", ctypes.c_double * 4),
                ("crval", ctypes.c_double * 2),
                ("crpix", ctypes.c_double * 2),
                ("sip_order", ctypes.c_int),
                ("sip_a", ctypes.c_double * 36),
                ("sip_b", ctypes.c_double * 36),
                ("sip_ap_order", ctypes.c_int),
                ("sip_ap", ctypes.c_double * 36),
                ("sip_bp", ctypes.c_double * 36),
                ("rms_px", ctypes.c_double),
                ("rms_arcsec", ctypes.c_double),
                ("n_pairs", ctypes.c_int),
                ("success", ctypes.c_int),
                ("n_detected", ctypes.c_int),
                ("n_catalog", ctypes.c_int),
                ("trans_order", ctypes.c_int),
                ("best_inliers", ctypes.c_int),
                ("ctype1", ctypes.c_char * 16),
                ("ctype2", ctypes.c_char * 16),
                ("error_msg", ctypes.c_char * 256)]


class DPSFFitParams(ctypes.Structure):
    _fields_ = [("fitRadius", ctypes.c_int),
                ("maxIter", ctypes.c_int),
                ("tolerance", ctypes.c_double)]


class GaiaSpectrumStar(ctypes.Structure):
    _fields_ = [("ra", ctypes.c_double),
                ("dec", ctypes.c_double),
                ("magG", ctypes.c_double)]


def load_image(fits_path):
    with astropy_fits.open(fits_path) as hdul:
        header = hdul[0].header
        raw = hdul[0].data
    if raw.dtype != np.uint16:
        bzero = float(header.get("BZERO", 0.0))
        bscale = float(header.get("BSCALE", 1.0))
        img = (raw.astype(np.float64) * bscale + bzero)
    else:
        img = raw.astype(np.float64)
    img16 = np.clip(img, 0, 65535).astype(np.uint16)
    return img16, header


def detect_and_solve(img16, header):
    h, w = img16.shape
    sdet = ctypes.CDLL(ROOT + r"\lib\star_detector\star_detector.dll")
    sp = SDetParams(structureLayers=5, hotPixelFilterRadius=1,
                    iterativeClipSigma=9.0, iterativeMaxRounds=5,
                    medianFilterDetail=1, maxStars=2000, fitRadius=0,
                    fwhmClipSigma=3.0, maxAxisRatio=2.0)
    sdet.sdet_create.restype = ctypes.c_void_p
    handle = sdet.sdet_create(ctypes.byref(sp))
    x = ctypes.POINTER(ctypes.c_double)(); y = ctypes.POINTER(ctypes.c_double)()
    flux = ctypes.POINTER(ctypes.c_float)(); sat = ctypes.POINTER(ctypes.c_int)()
    mag = ctypes.POINTER(ctypes.c_float)(); has_sat = ctypes.POINTER(ctypes.c_int)()
    count = ctypes.c_int(0)
    arr = np.ascontiguousarray(img16)
    ptr = arr.ctypes.data_as(ctypes.POINTER(ctypes.c_uint16))
    sdet.sdet_detect_ex.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint16),
                                    ctypes.c_int, ctypes.c_int,
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_int)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_int)),
                                    ctypes.POINTER(ctypes.c_int),
                                    ctypes.POINTER(ctypes.c_char_p), ctypes.c_int,
                                    ctypes.POINTER(ctypes.POINTER(ctypes.POINTER(ctypes.c_float)))]
    sdet.sdet_detect_ex.restype = ctypes.c_int
    sdet.sdet_detect_ex(handle, ptr, w, h, ctypes.byref(x), ctypes.byref(y),
                        ctypes.byref(flux), ctypes.byref(sat), ctypes.byref(mag),
                        ctypes.byref(has_sat), ctypes.byref(count), None, 0, None)
    n = count.value
    xs = np.ctypeslib.as_array(x, shape=(n,)).copy()
    ys = np.ctypeslib.as_array(y, shape=(n,)).copy()
    fluxes = np.ctypeslib.as_array(flux, shape=(n,)).copy()
    mags = np.ctypeslib.as_array(mag, shape=(n,)).copy()
    sats = np.ctypeslib.as_array(sat, shape=(n,)).copy()
    has_sats = np.ctypeslib.as_array(has_sat, shape=(n,)).copy()
    det = np.column_stack([xs, ys, fluxes, mags, sats, has_sats])
    sdet.sdet_free_detect_ex.argtypes = [ctypes.POINTER(ctypes.c_double),
                                         ctypes.POINTER(ctypes.c_double),
                                         ctypes.POINTER(ctypes.c_float),
                                         ctypes.POINTER(ctypes.c_int),
                                         ctypes.POINTER(ctypes.c_float),
                                         ctypes.POINTER(ctypes.c_int),
                                         ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
                                         ctypes.c_int]
    sdet.sdet_destroy.argtypes = [ctypes.c_void_p]
    sdet.sdet_free_detect_ex(x, y, flux, sat, mag, has_sat, None, 0)
    sdet.sdet_destroy(handle)

    # ipv solve
    ipv = ctypes.CDLL(ROOT + r"\lib\plate_solve\cpp\ipv\ipv_solver.dll")
    ipv.ipv_solve_create.restype = ctypes.c_void_p
    solver = ipv.ipv_solve_create()
    gaia = ctypes.CDLL(ROOT + r"\lib\photometric_calib\cpp\gaia_client.dll")
    gaia.gaia_client_create_ex.restype = ctypes.c_void_p
    gaia.gaia_client_create_ex.argtypes = [ctypes.c_char_p, ctypes.c_int]
    gaia_h = gaia.gaia_client_create_ex((ROOT + r"\GaiaDR3SP").encode(), 2)
    ipv.ipv_set_gaia_handle.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    ipv.ipv_set_gaia_handle(solver, ctypes.cast(gaia_h, ctypes.c_void_p))
    ipv.ipv_set_detector_handle.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    ipv.ipv_set_detector_handle(solver, ctypes.cast(ctypes.c_void_p(0), ctypes.c_void_p))
    ipv.ipv_get_default_params.argtypes = [ctypes.POINTER(IpvParams)]
    params = IpvParams()
    ipv.ipv_get_default_params(ctypes.byref(params))
    log_dir = (ROOT + r"\run\logs\plate_solve").encode()
    params.log_dir = log_dir[:255]

    ra0 = 272.808333  # OBJCTRA '18 11 14.00'
    dec0 = -23.223889  # OBJCTDEC '-23 13 26.0'
    if "OBJCTRA" in header:
        ra_hms = str(header["OBJCTRA"])
        hh, mm, ss = (float(v) for v in ra_hms.replace(" ", ":").split(":"))
        ra0 = 15.0 * (hh + mm / 60.0 + ss / 3600.0)
    if "OBJCTDEC" in header:
        dec_dms = str(header["OBJCTDEC"])
        sign = -1.0 if dec_dms.strip().startswith("-") else 1.0
        dd, mm, ss = (float(v) for v in dec_dms.replace(" ", ":").lstrip("+-").split(":"))
        dec0 = sign * (dd + mm / 60.0 + ss / 3600.0)
    focal = float(header.get("FOCALLEN", 200.0))
    pixsz = float(header.get("XPIXSZ", 6.0))

    det_c = np.ascontiguousarray(det.astype(np.float64))
    res = IpvWcsResult()
    ipv.ipv_solve_from_detections_v1.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.c_int,
        ctypes.c_int, ctypes.c_int, ctypes.c_double, ctypes.c_double,
        ctypes.c_double, ctypes.c_double, ctypes.POINTER(IpvParams),
        ctypes.POINTER(IpvWcsResult)]
    ipv.ipv_solve_from_detections_v1.restype = ctypes.c_int
    ret = ipv.ipv_solve_from_detections_v1(
        solver, det_c.ctypes.data_as(ctypes.POINTER(ctypes.c_double)), n,
        w, h, ra0, dec0, focal, pixsz, ctypes.byref(params), ctypes.byref(res))
    print(f"[diag] solve ret={ret} success={res.success} rms={res.rms_arcsec:.4f}\" "
          f"n_pairs={res.n_pairs} sip_order={res.sip_order}")

    # PSF fit
    dpsf = ctypes.CDLL(ROOT + r"\lib\dynamic_psf\dynamic_psf.dll")
    fp = DPSFFitParams(fitRadius=8, maxIter=100, tolerance=1e-6)
    out = (ctypes.c_double * (n * 9))()
    nv = ctypes.c_int(0)
    imgf = np.ascontiguousarray(img16.astype(np.float32))
    ptrf = imgf.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
    dpsf.dpsf_fit_batch_f32.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.c_int,
                                        ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                                        ctypes.c_int, ctypes.POINTER(DPSFFitParams),
                                        ctypes.POINTER(ctypes.c_double),
                                        ctypes.POINTER(ctypes.c_int)]
    dpsf.dpsf_fit_batch_f32.restype = ctypes.c_int
    dpsf.dpsf_fit_batch_f32(ptrf, w, h,
                            det_c.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
                            n, ctypes.byref(fp), out, ctypes.byref(nv))
    psf_arr = np.ctypeslib.as_array(out).reshape(n, 9).copy()
    psf_ok = ~np.isnan(psf_arr[:, 2]) & ~np.isnan(psf_arr[:, 3])
    psf_x = psf_arr[:, 2]
    psf_y = psf_arr[:, 3]

    # 在 solver 生命周期内读取权威 inlier 对 (det 为图像中心原点, Y 向上)
    inliers = None
    try:
        ipv.ipv_get_last_inlier_count.restype = ctypes.c_int
        ipv.ipv_get_last_inlier_count.argtypes = [ctypes.c_void_p]
        cnt = ipv.ipv_get_last_inlier_count(solver)
        if cnt > 0:
            buf = (ctypes.c_double * (cnt * 9))()
            ipv.ipv_get_last_inliers.restype = ctypes.c_int
            ipv.ipv_get_last_inliers.argtypes = [ctypes.c_void_p,
                                                 ctypes.POINTER(ctypes.c_double),
                                                 ctypes.c_int]
            nrows = ipv.ipv_get_last_inliers(solver, buf, cnt)
            if nrows > 0:
                inliers = np.ctypeslib.as_array(buf).reshape(nrows, 9).copy()
    except Exception:  # noqa: BLE001
        inliers = None

    # Gaia 目录 (用于全图投影)
    gaia_ra = gaia_dec = gaia_mag = np.zeros(0)
    try:
        gaia_ra, gaia_dec, gaia_mag = query_gaia(gaia_h, gaia,
                                                 float(res.crval[0]),
                                                 float(res.crval[1]), 1.4)
    except Exception:  # noqa: BLE001
        pass
    return xs, ys, psf_x, psf_y, psf_ok, res, gaia_ra, gaia_dec, gaia_mag, inliers


def build_wcs(res):
    crpix = np.array([res.crpix[0], res.crpix[1]])
    cd = np.array([[res.cd[0], res.cd[1]], [res.cd[2], res.cd[3]]])
    crval = np.array([res.crval[0], res.crval[1]])
    w = WCS(naxis=2)
    w.wcs.ctype = ["RA---TAN-SIP", "DEC--TAN-SIP"]
    w.wcs.crval = crval
    w.wcs.crpix = crpix
    w.wcs.cd = cd
    if res.sip_order > 0:
        # ipv 的 sip_a[i*6+j] = FITS A_i_j (i=x 幂, j=y 幂);
        # astropy Sip a[i][j] 对应 u^i v^j, 直接 reshape (行优先, i 为第一维)
        a = np.array(res.sip_a).reshape(6, 6)
        b = np.array(res.sip_b).reshape(6, 6)
        ap = np.array(res.sip_ap).reshape(6, 6) if res.sip_ap_order > 0 else None
        bp = np.array(res.sip_bp).reshape(6, 6) if res.sip_ap_order > 0 else None
        w.sip = Sip(a, b, ap, bp, crpix)
    return w


def query_gaia(gaia_h, gaia, ra_c, dec_c, radius_deg):
    gaia.gaia_client_cone_search_with_spectrum.restype = ctypes.c_int
    gaia.gaia_client_cone_search_with_spectrum.argtypes = [
        ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double,
        ctypes.c_double, ctypes.c_double, ctypes.POINTER(ctypes.POINTER(GaiaSpectrumStar)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_int)]
    stars = ctypes.POINTER(GaiaSpectrumStar)()
    spec = ctypes.POINTER(ctypes.c_uint8)()
    n = ctypes.c_int(0)
    rc = gaia.gaia_client_cone_search_with_spectrum(
        gaia_h, ra_c, dec_c, radius_deg, 6.0, 16.0,
        ctypes.byref(stars), ctypes.byref(spec), ctypes.byref(n))
    if rc != 0 or n.value <= 0:
        return np.zeros(0), np.zeros(0), np.zeros(0)
    arr = np.ctypeslib.as_array(stars, shape=(n.value,)).copy()
    return arr["ra"].copy(), arr["dec"].copy(), arr["magG"].copy()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fits", default=ROOT + r"\run\temp\l2_crop\galaxy_crop_1024.fits")
    ap.add_argument("--out", default=ROOT + r"\run\screenshots\gaia_psf_projection_t4.png")
    args = ap.parse_args()
    add_dll_dirs()

    img16, header = load_image(args.fits)
    (xs, ys, psf_x, psf_y, psf_ok, res,
     gaia_ra, gaia_dec, gaia_mag, inliers) = detect_and_solve(img16, header)
    w = build_wcs(res)

    # Gaia 投影 (astropy FITS 1-based 像素坐标)
    cx, cy = img16.shape[1] / 2.0, img16.shape[0] / 2.0
    gp = w.all_world2pix(gaia_ra, gaia_dec, 1)
    gx = gp[0]
    gy = gp[1]
    in_img = (gx >= 20) & (gx < img16.shape[1] - 20) & (gy >= 20) & (gy < img16.shape[0] - 20)
    gx, gy, gaia_mag = gx[in_img], gy[in_img], gaia_mag[in_img]
    print(f"[diag] Gaia 投影在图像内: {len(gx)} 颗; 检测 {len(xs)}; PSF 成功 {int(psf_ok.sum())}")

    # sdet 与 PSF 偏移统计 (验证 PSF-001)
    d = np.hypot(psf_x - xs, psf_y - ys)
    d_ok = d[psf_ok]
    print(f"[diag] sdet->PSF 偏移: median={np.median(d_ok):.4f}px p95={np.percentile(d_ok,95):.4f}px")

    # 匹配 sdet 到最近 Gaia (验证 platesolve/WCS 对齐)
    best_d = []
    for k in range(len(xs)):
        dd = np.hypot(gx - xs[k], gy - ys[k])
        if dd.size:
            best_d.append(dd.min())
    best_d = np.array(best_d)
    print(f"[diag] sdet->最近Gaia: median={np.median(best_d):.4f}px p95={np.percentile(best_d,95):.4f}px")

    # 权威 inlier 对: astropy 投影 inlier Gaia vs 求解器 det (转图像坐标)
    if inliers is not None and len(inliers):
        i_ra = inliers[:, 2]
        i_dec = inliers[:, 3]
        i_gp = w.all_world2pix(i_ra, i_dec, 1)
        i_det_x = 512.0 + inliers[:, 0]   # 中心原点 -> 图像
        i_det_y = 512.0 - inliers[:, 1]
        i_err = np.hypot(i_gp[0] - i_det_x, i_gp[1] - i_det_y)
        print(f"[diag] inlier: astropy(gaia)->pix vs 求解器 det: "
              f"median={np.median(i_err):.4f}px max={i_err.max():.4f}px")

    # 绘图
    fig = plt.figure(figsize=(16, 10))
    gs = fig.add_gridspec(2, 4)
    ax0 = fig.add_subplot(gs[0, :])
    ax0.imshow(img16, origin="lower", cmap="gray", vmin=0, vmax=65535,
               interpolation="nearest")
    ax0.plot(gx, gy, "+", color="red", ms=7, mew=1.2, label="Gaia 投影 (求解WCS)")
    ax0.plot(xs, ys, "o", color="blue", ms=4, mfc="none", label="sdet 检测")
    ax0.plot(psf_x[psf_ok], psf_y[psf_ok], "x", color="lime", ms=5,
             label="DPSF 拟合")
    ax0.set_title(f"T4 裁剪 1024x1024 | 求解 rms={res.rms_arcsec:.3f}\" | "
                  f"sdet->PSF 偏移 median={np.median(d_ok):.3f}px | "
                  f"sdet->Gaia median={np.median(best_d):.3f}px")
    ax0.legend(loc="upper right", fontsize=9)
    ax0.set_xlim(0, img16.shape[1]); ax0.set_ylim(0, img16.shape[0])

    # 3 个放大区: 按 mag 选亮星 (det 无 mag? 有 flux) 用 flux 排序
    # 选 3 颗不同径向距离的检测星
    rk = np.array([np.hypot(xs[k] - cx, ys[k] - cy) for k in range(len(xs))])
    picks = []
    for target_r in (60, 250, 430):
        j = int(np.argmin(np.abs(rk - target_r)))
        picks.append(j)
    for i, j in enumerate(picks):
        ax = fig.add_subplot(gs[1, i])
        x0, y0 = xs[j], ys[j]
        s = 48
        xlo, xhi = int(max(0, x0 - s)), int(min(img16.shape[1], x0 + s))
        ylo, yhi = int(max(0, y0 - s)), int(min(img16.shape[0], y0 + s))
        ax.imshow(img16[ylo:yhi, xlo:xhi], origin="lower", cmap="gray",
                  vmin=0, vmax=65535, interpolation="nearest",
                  extent=[xlo, xhi, ylo, yhi])
        ax.plot(gx, gy, "+", color="red", ms=9, mew=1.5)
        ax.plot(xs[j], ys[j], "o", color="blue", ms=8, mfc="none")
        if psf_ok[j]:
            ax.plot(psf_x[j], psf_y[j], "x", color="lime", ms=9)
            ax.annotate("", xy=(psf_x[j], psf_y[j]), xytext=(xs[j], ys[j]),
                        arrowprops=dict(arrowstyle="->", color="orange", lw=1.5))
        ax.set_title(f"r={rk[j]:.0f}px | sdet->PSF={d[j]:.3f}px")
        ax.set_xlim(xlo, xhi); ax.set_ylim(ylo, yhi)

    ax4 = fig.add_subplot(gs[1, 3])
    ax4.hist(d_ok, bins=40, color="steelblue", edgecolor="white")
    ax4.axvline(np.median(d_ok), color="orange", ls="--", label=f"median {np.median(d_ok):.3f}px")
    ax4.axvline(0.5, color="red", ls=":", label="0.5px (像素中心偏移)")
    ax4.set_title("sdet -> DPSF 偏移分布")
    ax4.legend(fontsize=8)
    ax4.set_xlabel("像素")

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    fig.tight_layout()
    fig.savefig(args.out, dpi=130)
    print(f"[diag] 已保存: {args.out}")


if __name__ == "__main__":
    main()
