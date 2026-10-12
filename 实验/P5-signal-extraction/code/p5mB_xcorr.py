"""T1+T2：同一次测量里同时给图像层与源表层的跨组偏移（v2，修正基函数）。

基函数：10 项三次多项式，u、v 由各自方向显式归一化（正变换用 sky 归一，逆变换用像素归一）。
"""
import json, os, sys
import numpy as np
from astropy.io import fits
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location("m5", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5m5_star_ratio_sip.py"))
m5 = importlib.util.module_from_spec(spec); spec.loader.exec_module(m5)
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
PIX, N, HALF, DEG = 0.967, 512, 40, 1.0 / 57.29577951308232


def basis(u, v):
    u = np.atleast_1d(np.asarray(u, float)); v = np.atleast_1d(np.asarray(v, float))
    return np.stack([np.ones_like(u), u, v, u * u, u * v, v * v, u**3, u * u * v, u * v * v, v**3], 1)


def geom(wj):
    w = json.load(open(wj)); sm = w["samples"]
    x = np.array([p["x"] for p in sm]); y = np.array([p["y"] for p in sm])
    ra = np.array([p["ra"] for p in sm]); dec = np.array([p["dec"] for p in sm])
    ra0, dec0 = float(np.median(ra)), float(np.median(dec))
    x0, y0 = float(np.mean(x)), float(np.mean(y))
    Bf = basis((ra - ra0) * np.cos(np.radians(dec)) / DEG, (dec - dec0) / DEG)
    cx, *_ = np.linalg.lstsq(Bf, x, rcond=None); cy, *_ = np.linalg.lstsq(Bf, y, rcond=None)
    rx = basis((x - x0) / 2048.0, (y - y0) / 2048.0)
    cra, *_ = np.linalg.lstsq(rx, (ra - ra0) * np.cos(np.radians(dec)), rcond=None)
    cdec, *_ = np.linalg.lstsq(rx, dec - dec0, rcond=None)
    def fwd(ra_q, dec_q):
        Bq = basis((np.asarray(ra_q, float) - ra0) * np.cos(np.radians(dec_q)) / DEG,
                   (np.asarray(dec_q, float) - dec0) / DEG)
        return Bq @ cx, Bq @ cy
    def inv(xq, yq):
        Bq = basis((np.asarray(xq, float) - x0) / 2048.0, (np.asarray(yq, float) - y0) / 2048.0)
        return ra0 + (Bq @ cra) / np.cos(np.radians(dec0 + Bq @ cdec)), dec0 + Bq @ cdec
    return fwd, inv


def sample(img, gx, gy):
    ix = np.floor(gx); iy = np.floor(gy)
    ok = (ix >= 0) & (ix < img.shape[1] - 1) & (iy >= 0) & (iy < img.shape[0] - 1)
    ix = np.clip(ix, 0, img.shape[1] - 2).astype(np.int64); iy = np.clip(iy, 0, img.shape[0] - 2).astype(np.int64)
    fx = gx - ix; fy = gy - iy
    v = ((1 - fx) * (1 - fy) * img[iy, ix] + fx * (1 - fy) * img[iy, ix + 1] +
         (1 - fx) * fy * img[iy + 1, ix] + fx * fy * img[iy + 1, ix + 1])
    return np.where(ok & np.isfinite(v), v, np.nan)


def meta(tag, fid):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    stem = [os.path.basename(e["input_light"]).replace(".fts", "") for e in man["frames"] if e["frame_id"] == fid][0]
    return stem, os.path.join(PDIR, tag, f"cleaned_{stem}.fts")


def patch(tag, fid, ra_c, dec_c, scale_deg):
    fwd, _ = geom(os.path.join(PDIR, tag, fid, "p1_wcs.json"))
    _, path = meta(tag, fid); img = fits.getdata(path).astype(np.float64)
    g = (np.arange(N) - N / 2) * scale_deg
    U, V = np.meshgrid(g, g)
    ra = ra_c + U / np.cos(np.radians(dec_c)); dec = dec_c + V
    gx, gy = fwd(ra.ravel(), dec.ravel())
    return sample(img, gx.reshape(N, N), gy.reshape(N, N))


def xcorr(a, b):
    m = np.isfinite(a) & np.isfinite(b)
    A = np.where(m, a - np.nanmean(a[m]), 0.0); B = np.where(m, b - np.nanmean(b[m]), 0.0)
    c = N // 2
    R = np.fft.fftshift(np.real(np.fft.ifft2(np.fft.fft2(A) * np.conj(np.fft.fft2(B)))))
    win = R[c - HALF:c + HALF + 1, c - HALF:c + HALF + 1]
    k = np.unravel_index(np.argmax(win), win.shape); dy, dx = k[0] - HALF, k[1] - HALF
    w2 = win.copy(); w2[max(0, k[0] - 3):k[0] + 4, max(0, k[1] - 3):k[1] + 4] = -np.inf
    sec = float(np.max(w2)); pk = float(win[k]); rms = float(np.std(win))
    return {"peak_px": [int(dx), int(dy)], "peak_arcsec": [round(dx * PIX, 2), round(dy * PIX, 2)],
            "peak_value": pk, "zero_lag": float(win[HALF, HALF]), "second_peak": sec,
            "peak_over_second": float(pk / sec) if sec > 0 else None,
            "peak_over_rms": float(pk / rms) if rms > 0 else None,
            "n_valid": int(m.sum()), "single_peak": bool(pk > 3 * sec)}


def srcoff(tag, fidA, fidB):
    P = {}
    for k, fid in (("A", fidA), ("B", fidB)):
        _, inv = geom(os.path.join(PDIR, tag, fid, "p1_wcs.json"))
        stem, _ = meta(tag, fid)
        fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
        fr = [f for f in fl["frames"] if f["file"].replace("cleaned_", "").replace(".fts", "") == stem][0]
        xy = np.array([[s["x"], s["y"]] for s in fr["results"] if s.get("valid") and s["flux"] > 0])
        P[k] = inv(xy[:, 0], xy[:, 1])
    ra0, dec0 = float(np.median(P["A"][0])), float(np.median(P["A"][1]))
    ua = (P["A"][0] - ra0) * np.cos(np.radians(P["A"][1])); va = P["A"][1] - dec0
    ub = (P["B"][0] - ra0) * np.cos(np.radians(P["B"][1])); vb = P["B"][1] - dec0
    tb = cKDTree(np.stack([ub, vb], 1))
    idx = tb.query_ball_point(np.stack([ua, va], 1), r=60 / 3600.0)
    nb = 2001; H = np.zeros((nb, nb), np.int32)
    for i, js in enumerate(idx):
        for j in js:
            di = int(round((ub[j] - ua[i]) * 3600)) + nb // 2; dj = int(round((vb[j] - va[i]) * 3600)) + nb // 2
            if 0 <= di < nb and 0 <= dj < nb: H[di, dj] += 1
    k = np.unravel_index(np.argmax(H), H.shape)
    return {"peak_offset_arcsec": [int(k[0] - nb // 2), int(k[1] - nb // 2)], "peak_count": int(H[k]),
            "zero_offset_count": int(H[nb // 2, nb // 2]), "n_sources": [int(ua.size), int(ub.size)]}


man = json.load(open(f"{PDIR}/t2/p1_products.json"))
def fid_of(part):
    for e in man["frames"]:
        if part in os.path.basename(e["input_light"]):
            return os.path.basename(e["input_light"]).replace(".fts", ""), e["frame_id"]
    return None, None
cases = {"cross_M2_M3": (fid_of("M2_T2_flying_dutchman-20251212"), fid_of("M3_T2_flying_dutchman-20251224")),
         "within_M2": (fid_of("M2_T2_flying_dutchman-20251212"), fid_of("M2_T2_flying_dutchman-20251216"))}
res = {}
for lbl, ((sA, fA), (sB, fB)) in cases.items():
    if not fA or not fB:
        print("[%s] 帧未找到（清单枚举失败）" % lbl); res[lbl] = {"error": "frame not found"}; continue
    _, invA = geom(os.path.join(PDIR, "t2", fA, "p1_wcs.json"))
    _, invB = geom(os.path.join(PDIR, "t2", fB, "p1_wcs.json"))
    xa, ya = invA(2048.0, 2048.0); xb, yb = invB(2048.0, 2048.0)
    ra_c, dec_c = float((xa[0] + xb[0]) / 2), float((ya[0] + yb[0]) / 2)
    A = patch("t2", fA, ra_c, dec_c, PIX / 3600.0); B = patch("t2", fB, ra_c, dec_c, PIX / 3600.0)
    r1 = xcorr(A, B); r2 = srcoff("t2", fA, fB)
    res[lbl] = {"T1_image_xcorr": r1, "T2_source_hist": r2, "frames": [fA, fB]}
    print("[%s] %s × %s" % (lbl, sA[:32], sB[:32]))
    print("   T1 图像层：峰位 %s px = %s″ | 峰/次峰 %.2f | 峰/RMS %.1f | 零延迟 %.4g | 有效 %d | 单峰 %s" % (
        r1["peak_px"], r1["peak_arcsec"], r1["peak_over_second"] or -1, r1["peak_over_rms"] or -1,
        r1["zero_lag"], r1["n_valid"], r1["single_peak"]))
    print("   T2 源表层：峰位 %s″ | 峰计数 %d | 零偏移计数 %d | 源数 %s" % (
        r2["peak_offset_arcsec"], r2["peak_count"], r2["zero_offset_count"], r2["n_sources"]))
json.dump(res, open(f"{OUT}/p5mB_xcorr.json", "w"), indent=1)
print("写入 results/p5w/p5mB_xcorr.json")
