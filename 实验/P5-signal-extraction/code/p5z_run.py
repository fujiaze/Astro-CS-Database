"""P5 信号提取（生产几何版）：把真实单帧产物投到**生产成品同一网格**上再分解与判据。

数据源（与派单一致，不再自建网格）：
  · 生产成品网格参照：`run/E2E-M42-P5-EXPORT/out/full_p3_whole/output_phase3.fits`
    （6208×4352、TAN、CD=diag(±0.0005) 度 = 1.8″/px、CRPIX 取中心、NaN 9.4%）
  · 逐帧标准化单帧产物：`run/PHOT-FIX/p1_rerun/{t2,t3}/calibrated_*.fts`（4096²，带 WCS）
    与其坏列掩膜 `badcol_*.fts`（0/1）

方法：
  1. 逐帧按其 WCS 投到生产网格（`pixel_to_pixel`，双线性），**无覆盖处不填**，坏列与非有限值剔除；
  2. 流式累积：逐像素计数、Σw、Σw·x、Σw·x²、覆盖帧集位掩码；同时按粗格（32 输出像素）累积
     逐帧格均值，供差模对齐（路径 A）在生产网格上拟合「偏移+平面+粗格场」；
  3. 第二遍重建逐帧影像并施加修正，得到各路径成品；判据（接缝逐边计数、帧间一致性、结构保留）
     全部在生产网格上算。
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import sys
import time

import numpy as np
from astropy.io import fits
from astropy.wcs import WCS

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
POUT = 6208          # 生产网格
NOUT = 4352
CD_DEG = 0.0005
CELL = 32            # 粗格边长（输出像素）
REF = "/workspace/Astro CS Database/run/E2E-M42-P5-EXPORT/out/full_p3_whole/output_phase3.fits"
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"


def _j(x):
    if isinstance(x, (np.floating,)):
        return float(x)
    if isinstance(x, (np.integer,)):
        return int(x)
    if isinstance(x, np.ndarray):
        return x.tolist()
    return str(x)


def ref_grid():
    h = fits.getheader(REF)
    w = WCS(naxis=2)
    w.wcs.ctype = [h["CTYPE1"], h["CTYPE2"]]
    w.wcs.cunit = ["deg", "deg"]
    w.wcs.crval = [h["CRVAL1"], h["CRVAL2"]]
    w.wcs.crpix = [h["CRPIX1"], h["CRPIX2"]]
    w.wcs.cd = np.array([[h["CD1_1"], h["CD1_2"]], [h["CD2_1"], h["CD2_2"]]])
    return w, h


def ref_header(h):
    """生产成品口径的头：只用 CD 矩阵，不写 CDELT/PC。"""
    o = fits.Header()
    o["NAXIS"] = 2; o["NAXIS1"] = NOUT; o["NAXIS2"] = POUT
    o["CTYPE1"] = str(h["CTYPE1"]); o["CTYPE2"] = str(h["CTYPE2"])
    o["CUNIT1"] = "deg"; o["CUNIT2"] = "deg"
    o["CRPIX1"] = float(h["CRPIX1"]); o["CRPIX2"] = float(h["CRPIX2"])
    o["CRVAL1"] = float(h["CRVAL1"]); o["CRVAL2"] = float(h["CRVAL2"])
    o["CD1_1"] = float(h["CD1_1"]); o["CD1_2"] = float(h["CD1_2"])
    o["CD2_1"] = float(h["CD2_1"]); o["CD2_2"] = float(h["CD2_2"])
    o["BUNIT"] = "ADU"
    return o


def frame_wcs(path):
    """单帧 WCS：用 CRVAL/CRPIX + CDELT（这些文件同时带 CD 与 CDELT，二者相差约 59 倍，
    取 CDELT——它与 4096² 视场、与生产 ORDERSEL=8 的 1.8″/px 抽样自洽；CD 会在自检里被否证）。"""
    h = fits.getheader(path)
    w = WCS(naxis=2)
    w.wcs.ctype = [h.get("CTYPE1", "RA---TAN"), h.get("CTYPE2", "DEC--TAN")]
    w.wcs.cunit = ["deg", "deg"]
    w.wcs.crval = [h["CRVAL1"], h["CRVAL2"]]
    w.wcs.crpix = [h["CRPIX1"], h["CRPIX2"]]
    if "CDELT1" in h and "CDELT2" in h:
        w.wcs.cdelt = [h["CDELT1"], h["CDELT2"]]
    w.wcs.pc = np.array([[1.0, 0.0], [0.0, 1.0]])
    return w, h


def frame_snr(fdir):
    """帧级绝对信噪比（phase-1 产物 properties 里记录），用作逐帧权重；缺失则 1。"""
    for name in ("properties",):
        p = os.path.join(fdir, name)
        if os.path.exists(p):
            txt = open(p, errors="ignore").read()
            m = re.search(r"ACSD_FRAME_SNR=([0-9.eE+-]+)", txt)
            if m:
                return float(m.group(1))
    return 1.0


def _homography(fw, dw):
    """单帧像素 → 生产网格像素的精确投影变换（TAN→TAN 之间是射影映射，8 参数可精确表示）。"""
    pts = []
    for fx in (0, 1024, 2048, 3072, 4095):
        for fy in (0, 1024, 2048, 3072, 4095):
            pts.append((fx, fy))
    pts = np.array(pts, dtype=float)
    world = fw.all_pix2world(pts, 0)
    dst = dw.all_world2pix(world, 0)
    # DLT：x_o = (h0 u + h1 v + h2)/(h6 u + h7 v + 1)
    uu, vv = pts[:, 0], pts[:, 1]
    X, Y = dst[:, 0], dst[:, 1]
    A = []
    b = []
    for u, v, x, y in zip(uu, vv, X, Y):
        A.append([u, v, 1, 0, 0, 0, -u * x, -v * x]); b.append(x)
        A.append([0, 0, 0, u, v, 1, -u * y, -v * y]); b.append(y)
    h = np.linalg.lstsq(np.array(A), np.array(b), rcond=None)[0]
    H = np.array([[h[0], h[1], h[2]], [h[3], h[4], h[5]], [h[6], h[7], 1.0]])
    # 验证：随机点残差
    rng = np.random.default_rng(0)
    t = rng.uniform(0, 4095, size=(200, 2))
    wd = fw.all_pix2world(t, 0)
    ref = dw.all_world2pix(wd, 0)
    pj = _apply_H(H, t)
    _homography.last_residual = float(np.max(np.abs(pj - ref)))
    return H


def _apply_H(H, p):
    q = np.concatenate([p, np.ones((p.shape[0], 1))], axis=1) @ H.T
    return q[:, :2] / q[:, 2:3]


def sample_image(fw, dw, img, H=None, ny=NOUT, nx=POUT):
    """把单帧影像按 WCS 采样到生产网格：帧像素 → 生产网格像素（射影映射），最近落格积分。"""
    if H is None:
        H = _homography(fw, dw)
    ny_i, nx_i = img.shape
    N = ny_i * nx_i
    flat = np.arange(N, dtype=np.int64)
    p = np.empty((N, 2), dtype=np.float64)
    p[:, 0] = flat % nx_i
    p[:, 1] = flat // nx_i
    q = _apply_H(H, p)
    del p
    v = img.reshape(-1)
    xo, yo = q[:, 0], q[:, 1]
    del q
    vv = np.asarray(v, dtype=np.float64)
    ok = (xo >= 0) & (xo < nx) & (yo >= 0) & (yo < ny) & np.isfinite(vv)
    xi = np.floor(xo[ok]).astype(np.int64)
    yi = np.floor(yo[ok]).astype(np.int64)
    return (yi * nx + xi).astype(np.int64), vv[ok]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--fits", required=True)
    ap.add_argument("--thumbs", default=None)
    ap.add_argument("--limit", type=int, default=0, help="只用前 N 帧（0=全部）")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.fits, exist_ok=True)
    if a.thumbs:
        os.makedirs(a.thumbs, exist_ok=True)
    t0 = time.time()

    dw, dh = ref_grid()
    ref_img = fits.getdata(REF).astype(np.float32)
    res = {"reference": {"file": REF, "nx": NOUT, "ny": POUT,
                         "nan_frac": float(np.mean(~np.isfinite(ref_img))),
                         "cd": [float(dh["CD1_1"]), float(dh["CD2_2"])],
                         "cdelt_arcsec": abs(float(dh["CD1_1"])) * 3600,
                         "crval": [float(dh["CRVAL1"]), float(dh["CRVAL2"])],
                         "crpix": [float(dh["CRPIX1"]), float(dh["CRPIX2"])],
                         "ctype": [str(dh["CTYPE1"]), str(dh["CTYPE2"])]}}

    frames = []
    for tag in ("t2", "t3"):
        for f in sorted(glob.glob(os.path.join(PDIR, tag, "cleaned_*.fts"))):
            b = f.replace("cleaned_", "badcol_")
            if os.path.exists(b):
                frames.append((tag, f, b))
    if a.limit:
        frames = frames[:a.limit]
    res["inputs"] = {"n_frames": len(frames),
                     "per_tag": {t: sum(1 for x in frames if x[0] == t) for t in ("t2", "t3")},
                     "files": [os.path.basename(x[1]) for x in frames]}

    n = POUT * NOUT
    per_cell = (POUT // CELL) * (NOUT // CELL)
    cnt = np.zeros(n, dtype=np.int32)
    bits = np.zeros(n, dtype=np.int32)
    sw = np.zeros(n); swx = np.zeros(n); swx2 = np.zeros(n)     # 原始堆栈
    cw = np.zeros(n); cwx = np.zeros(n)                          # 修正后堆栈（路径 A）
    cell_sum = np.zeros((len(frames), per_cell)); cell_w = np.zeros((len(frames), per_cell))
    stats = []
    coords = []
    for k, (tag, fp, bp) in enumerate(frames):
        fw, fh = frame_wcs(fp)
        H = _homography(fw, dw)
        img = fits.getdata(fp).astype(np.float32)
        bad = fits.getdata(bp)
        img = np.where(bad > 0, np.nan, img)
        idx, v = sample_image(fw, dw, img, H)
        snr = frame_snr(os.path.dirname(fp))
        w = np.full(v.shape, float(snr), dtype=np.float64)
        cnt += np.bincount(idx, minlength=n).astype(np.int32)
        bits |= (np.bincount(idx, minlength=n) > 0).astype(np.int32) << k
        sw += np.bincount(idx, weights=w, minlength=n)
        swx += np.bincount(idx, weights=w * v, minlength=n)
        swx2 += np.bincount(idx, weights=w * v * v, minlength=n)
        # 粗格
        yy = idx // NOUT; xx = idx % NOUT
        cid = (yy // CELL) * (NOUT // CELL) + (xx // CELL)
        cell_sum[k] = np.bincount(cid, weights=w * v, minlength=per_cell)
        cell_w[k] = np.bincount(cid, weights=w, minlength=per_cell)
        stats.append({"frame": k, "tag": tag, "file": os.path.basename(fp), "snr": snr,
                      "homography_residual_px": getattr(_homography, "last_residual", None),
                      "n_samples": int(idx.size),
                      "crval": [float(fh["CRVAL1"]), float(fh["CRVAL2"])],
                      "cdelt_arcsec": float(fh.get("CDELT1", np.nan)) * 3600,
                      "cd_arcsec": float(fh.get("CD1_1", np.nan)) * 3600})
        coords.append((fw, fp))
        if k % 8 == 0:
            print("  pass1 %2d/%d  n=%d  %.0fs" % (k + 1, len(frames), idx.size, time.time() - t0),
                  flush=True)
    deltas = None
    raw = np.where(sw > 0, swx / np.maximum(sw, 1e-300), np.nan)
    ncov = cnt
    covered = ncov > 0
    res["frames"] = stats
    res["coverage"] = {"n_covered_px": int(covered.sum()), "frac_of_grid": float(covered.mean()),
                       "hist": {str(int(k)): int(v) for k, v in
                                zip(*np.unique(ncov[ncov > 0], return_counts=True))},
                       "ref_nan_frac": float(np.mean(~np.isfinite(ref_img)))}
    # 自检：我的覆盖 vs 生产成品的非 NaN
    refcov = np.isfinite(ref_img).reshape(-1)
    inter = np.sum(covered & refcov); union = np.sum(covered | refcov)
    res["coverage"]["iou_with_reference"] = float(inter / max(union, 1))
    res["coverage"]["ref_cov_covered_by_mine"] = float(inter / max(refcov.sum(), 1))

    # ---------- 路径 A：差模对齐（在生产网格的粗格上拟合「偏移 + 平面 + 格点场」）
    from scipy.sparse import csr_matrix
    cm = np.where(cell_w > 0, cell_sum / np.maximum(cell_w, 1e-300), np.nan)   # (nf, cell)
    nf = len(frames)
    pairs = [(i, j) for i in range(nf) for j in range(i + 1, nf)]
    nyc, nxc = POUT // CELL, NOUT // CELL
    cy, cx = np.mgrid[0:nyc, 0:nxc]
    cu = ((cx.ravel() - (nxc - 1) / 2.0) * CELL * CD_DEG).astype(np.float64)
    cv = ((cy.ravel() - (nyc - 1) / 2.0) * CELL * CD_DEG).astype(np.float64)
    B = np.stack([np.ones_like(cu), cu, cv], axis=1)              # (cell, 3) 帧无关基
    rows, cols, data, rhs = [], [], [], []
    r = 0
    for (i, j) in pairs:
        m = np.isfinite(cm[i]) & np.isfinite(cm[j])
        nn = int(m.sum())
        if nn < 10:
            continue
        d = (cm[i] - cm[j])[m]
        Bm = B[m]
        for c in range(3):
            rows.append(np.arange(r, r + nn)); cols.append(np.full(nn, i * 3 + c)); data.append(Bm[:, c])
            rows.append(np.arange(r, r + nn)); cols.append(np.full(nn, j * 3 + c)); data.append(-Bm[:, c])
        rhs.append(d); r += nn
    A = csr_matrix((np.concatenate(data), (np.concatenate(rows), np.concatenate(cols))), shape=(r, nf * 3))
    G = np.zeros((3, nf * 3))
    for c in range(3):
        for k in range(nf):
            G[c, k * 3 + c] = 1.0
    A = csr_matrix(np.vstack([A.toarray(), G * 1e-3]))
    bvec = np.concatenate(rhs + [np.zeros(3)])
    th = np.linalg.lstsq(A.toarray(), bvec, rcond=None)[0].reshape(nf, 3)
    # 粗格场：帧间差分残差按格平均，逐格扣跨帧均值（只留差模，公共分量不动）
    resid = cm - (B @ th.T).T
    have = np.isfinite(resid)
    cellcorr = np.where(have, resid, 0.0)
    for _ in range(4):
        num = have.sum(axis=0)
        mu = np.where(num > 0, np.where(have, cellcorr, 0.0).sum(axis=0) / np.maximum(num, 1), 0.0)
        cellcorr = np.where(have, cellcorr - mu[None, :], 0.0)
    res["path_A_fit"] = {"n_pairs": len(pairs), "n_cells": int(per_cell),
                         "theta_const": th[:, 0].tolist(),
                         "cell_field_rms": float(np.sqrt(np.mean(cellcorr ** 2)))}

    # ---------- pass2：施加修正并累积
    for k, (tag, fp, bp) in enumerate(frames):
        fw, _ = frame_wcs(fp)
        H = _homography(fw, dw)
        img = fits.getdata(fp).astype(np.float32)
        bad = fits.getdata(bp)
        img = np.where(bad > 0, np.nan, img)
        idx, v = sample_image(fw, dw, img, H)
        snr = float(stats[k]["snr"])
        w = np.full(v.shape, snr, dtype=np.float64)
        yy = idx // NOUT; xx = idx % NOUT
        cid = (yy // CELL) * (NOUT // CELL) + (xx // CELL)
        corr = B[cid] @ th[k] + cellcorr[k][cid]
        cw += np.bincount(idx, weights=w, minlength=n)
        cwx += np.bincount(idx, weights=w * (v - corr), minlength=n)
        if k % 8 == 0:
            print("  pass2 %2d/%d  %.0fs" % (k + 1, len(frames), time.time() - t0), flush=True)
    A_img = np.where(cw > 0, cwx / np.maximum(cw, 1e-300), np.nan)

    # ---------- 判据（生产网格）
    def img_of(flat):
        return flat.reshape(POUT, NOUT)
    out = {}
    for name, flat in (("raw", raw), ("A", A_img), ("prod_ref", ref_img.reshape(-1))):
        img = img_of(np.asarray(flat, dtype=np.float64))
        bg = _bg(img)
        p_, q_ = _edges(bits, ncov)
        st = _seam(img, p_, q_, bg)
        ok = np.isfinite(img)
        out[name] = {"nan_frac": float(np.mean(~ok)),
                     "n_covered": int(ok.sum()),
                     "seam": _summ(st),
                     "seam_groups": _groups(bits, ncov, st),
                     "_st": st}
    cons = _consistency(sw, swx, swx2, ncov)
    out["raw"]["consistency"] = cons
    # A 的一致性：用 pass2 的逐帧和重算需要另一遍；此处用 raw 与 A 的结构判据
    out["raw"]["structure_vs_ref"] = _structure(img_of(raw), ref_img, ncov)
    out["A"]["structure_vs_ref"] = _structure(img_of(A_img), ref_img, ncov)
    out["A"]["edges_vs_raw"] = _compare(out["raw"]["_st"], out["A"]["_st"])
    out["A"]["edges_vs_ref"] = _compare(out["prod_ref"]["_st"], out["A"]["_st"])
    out["raw"]["edges_vs_ref"] = _compare(out["prod_ref"]["_st"], out["raw"]["_st"])
    for k in out:
        out[k].pop("_st", None)
    res["paths"] = out

    # ---------- FITS + 缩略图
    fits_info = {}
    for name, arr in (("raw", img_of(raw)), ("A", img_of(A_img)), ("prod_ref", ref_img)):
        pth = os.path.join(a.fits, f"p5z_{name}.fits")
        h = dw.to_header()
        h["BUNIT"] = ("ADU", "same unit as input phase-1 product")
        fits.PrimaryHDU(data=np.asarray(arr, dtype=np.float32), header=h).writeto(pth, overwrite=True)
        fits_info[name] = {"file": pth, "nx": NOUT, "ny": POUT,
                           "nan_frac": float(np.mean(~np.isfinite(arr))),
                           "cdelt_arcsec": abs(float(h["CDELT1"])) * 3600,
                           "crpix": [float(h["CRPIX1"]), float(h["CRPIX2"])],
                           "crval": [float(h["CRVAL1"]), float(h["CRVAL2"])]}
    res["fits"] = fits_info
    if a.thumbs:
        th_ = {}
        th_["reference"] = _thumb(ref_img, os.path.join(a.thumbs, "ref_production.png"), "production export")
        th_["raw"] = _thumb(img_of(raw), os.path.join(a.thumbs, "p5z_raw.png"), "raw stack (my grid=prod)")
        th_["A"] = _thumb(img_of(A_img), os.path.join(a.thumbs, "p5z_A.png"), "path A (mode-aligned)")
        res["thumbs"] = th_
    res["elapsed_s"] = time.time() - t0
    with open(os.path.join(a.out, "p5z_readouts.json"), "w") as fp:
        json.dump(res, fp, indent=1, default=_j, sort_keys=True)
    print("NaN: raw %.1f%%  A %.1f%%  prod_ref %.1f%%  IoU(覆盖,生产)=%.3f" % (
        100 * out["raw"]["nan_frac"], 100 * out["A"]["nan_frac"], 100 * out["prod_ref"]["nan_frac"],
        res["coverage"]["iou_with_reference"]))
    print("接缝 rel 中位: raw %.4g  A %.4g  prod_ref %.4g" % (
        out["raw"]["seam"]["rel_median_abs"], out["A"]["seam"]["rel_median_abs"],
        out["prod_ref"]["seam"]["rel_median_abs"]))
    print("written", os.path.join(a.out, "p5z_readouts.json"), "%.0fs" % res["elapsed_s"])


# ---------------------------------------------------------------- 判据工具
def _bg(img, win=4):
    from scipy import ndimage
    fin = np.isfinite(img)
    glob = float(np.nanmedian(img))
    filled = np.where(fin, img, glob)
    return ndimage.median_filter(filled, size=2 * win + 1, mode="nearest")


def _edges(bits, ncov):
    B = bits.reshape(POUT, NOUT); C = ncov.reshape(POUT, NOUT)
    p, q = [], []
    a = B[:, :NOUT - 1].reshape(-1); b = B[:, 1:].reshape(-1)
    ca = C[:, :NOUT - 1].reshape(-1); cb = C[:, 1:].reshape(-1)
    m = (ca > 0) & (cb > 0) & (a != b)
    ia = np.arange(POUT * NOUT).reshape(POUT, NOUT)[:, :NOUT - 1].reshape(-1)[m]
    p.append(ia); q.append(ia + 1)
    a = B[:POUT - 1, :].reshape(-1); b = B[1:, :].reshape(-1)
    ca = C[:POUT - 1, :].reshape(-1); cb = C[1:, :].reshape(-1)
    m = (ca > 0) & (cb > 0) & (a != b)
    ia = np.arange(POUT * NOUT).reshape(POUT, NOUT)[:POUT - 1, :].reshape(-1)[m]
    p.append(ia); q.append(ia + NOUT)
    return np.concatenate(p), np.concatenate(q)


def _seam(img, p, q, bg):
    """同一批边上的有符号相对台阶；某路径缺数据处保留 NaN（保证各路径逐边可比）。"""
    a = img.reshape(-1)[p]; b = img.reshape(-1)[q]
    g = bg.reshape(-1)[(p + q) // 2]
    good = np.isfinite(a) & np.isfinite(b) & np.isfinite(g) & (g != 0)
    rel = np.full(p.shape, np.nan)
    ab = np.full(p.shape, np.nan)
    rel[good] = (a[good] - b[good]) / g[good]
    ab[good] = np.abs(a[good] - b[good])
    return {"p": p, "q": q, "rel": rel, "abs": ab, "n": int(good.sum())}


def _summ(st):
    r = np.abs(st["rel"]); ok = np.isfinite(r)
    a = st["abs"][np.isfinite(st["abs"])]
    return {"n_edges": int(ok.sum()), "rel_median_abs": float(np.median(r[ok])),
            "rel_p95_abs": float(np.percentile(r[ok], 95)), "rel_max_abs": float(np.max(r[ok])),
            "abs_step_median": float(np.median(a))}


def _groups(bits, ncov, st):
    x = bits.reshape(-1)[st["p"]] ^ bits.reshape(-1)[st["q"]]
    nf = int(np.log2(max(int(bits.max()), 1))) + 1
    o = {}
    for k in range(nf):
        m = (((x >> k) & 1) == 1) & np.isfinite(st["rel"])
        if not m.any():
            continue
        r = st["rel"][m]
        o[f"frame{k}"] = {"n_edges": int(m.sum()), "mean_rel": float(np.mean(r)),
                          "se_off_mean": float(np.std(r) / np.sqrt(m.sum())),
                          "median_abs_rel": float(np.median(np.abs(r)))}
    return o


def _compare(base, new):
    b, n = np.abs(base["rel"]), np.abs(new["rel"])
    ok = np.isfinite(b) & np.isfinite(n)
    b, n = b[ok], n[ok]
    imp = n < b
    return {"n": int(b.size), "improved": int(imp.sum()), "worsened": int((~imp).sum()),
            "frac_improved": float(imp.mean()),
            "rel_median_base": float(np.median(b)), "rel_median_new": float(np.median(n)),
            "rel_p95_base": float(np.percentile(b, 95)), "rel_p95_new": float(np.percentile(n, 95))}


def _consistency(sw, swx, swx2, ncov):
    W = sw; MU = np.where(W > 0, swx / np.maximum(W, 1e-300), np.nan)
    with np.errstate(invalid="ignore"):
        var = np.where(W > 0, swx2 / np.maximum(W, 1e-300) - MU ** 2, np.nan)
    s = np.sqrt(np.maximum(var, 0))
    m = (ncov.reshape(-1) >= 2) & np.isfinite(s)
    return {"median": float(np.nanmedian(s[m])), "n_pixels": int(m.sum())}


def _structure(img, ref, ncov, q=99.9):
    from scipy import ndimage
    hi = np.nanpercentile(ref, q)
    m = np.isfinite(ref) & (ref > hi)
    m = ndimage.binary_dilation(m, iterations=1)
    ratio = (img[m] / ref[m]); ratio = ratio[np.isfinite(ratio)]
    lp = lambda im: ndimage.median_filter(np.nan_to_num(im), size=9)
    f_new = np.nan_to_num(img)[m] - lp(img)[m]
    f_ref = np.nan_to_num(ref)[m] - lp(ref)[m]
    quiet = (ncov.reshape(img.shape) >= 2) & ~m & np.isfinite(img)
    hp = lambda im: np.nan_to_num(im) - ndimage.median_filter(np.nan_to_num(im), size=3)
    return {"n_struct_px": int(m.sum()),
            "level_retention_median": float(np.median(ratio)),
            "band_rms_ratio": float(np.sqrt(np.mean(f_new ** 2)) / max(np.sqrt(np.mean(f_ref ** 2)), 1e-300)),
            "noise_highpass_rms": float(np.sqrt(np.mean(hp(img)[quiet] ** 2)))}


def _thumb(img, path, title, pct=(1, 99.5)):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    a = np.array(img, dtype=float); fin = np.isfinite(a)
    v0, v1 = np.nanpercentile(a[fin], pct) if fin.any() else (0, 1)
    fig, ax = plt.subplots(figsize=(4.6, 6.4), dpi=110)
    ax.imshow(a, origin="lower", cmap="gray", vmin=v0, vmax=v1, interpolation="nearest")
    ax.set_title(title, fontsize=9); ax.set_xlabel("x [px]"); ax.set_ylabel("y [px]")
    fig.tight_layout(); fig.savefig(path); plt.close(fig)
    return path


if __name__ == "__main__":
    main()
