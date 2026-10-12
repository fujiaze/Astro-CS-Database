"""P5 信号提取（生产几何，双输入臂）：`cleaned`（原生计数，含加性天光）与 `photoapplied`
（P1 已把它放到同一测光强度上，即负责人前提里「信号已归一」的那一支）。

网格完全取自生产成品 `output_phase3.fits`（6208×4352、TAN、CD=diag(±0.0005)、CRPIX 取中心），
不自建网格；无覆盖处保持 NaN，不做任何填充。

一条命令：
  python3 p5z2_run.py --out <结果目录> --fits <FITS 目录> [--thumbs <目录>] [--limit N]
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import sys
import time

import numpy as np
from astropy.io import fits
from scipy.sparse import csr_matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p5z_run as Z                      # noqa: E402

ARMS = ("cleaned", "photoapplied")
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--fits", required=True)
    ap.add_argument("--thumbs", default=None)
    ap.add_argument("--limit", type=int, default=0)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.fits, exist_ok=True)
    if a.thumbs:
        os.makedirs(a.thumbs, exist_ok=True)
    t0 = time.time()

    dw, dh = Z.ref_grid()
    ref_img = fits.getdata(Z.REF).astype(np.float32)
    n = Z.POUT * Z.NOUT
    nyc, nxc = Z.POUT // Z.CELL, Z.NOUT // Z.CELL
    per_cell = nyc * nxc
    cy, cx = np.mgrid[0:nyc, 0:nxc]
    cu = ((cx.ravel() - (nxc - 1) / 2.0) * Z.CELL * Z.CD_DEG).astype(np.float64)
    cv = ((cy.ravel() - (nyc - 1) / 2.0) * Z.CELL * Z.CD_DEG).astype(np.float64)
    B = np.stack([np.ones_like(cu), cu, cv], axis=1)

    frames = []
    for tag in ("t2", "t3"):
        for f in sorted(glob.glob(os.path.join(PDIR, tag, "cleaned_*.fts"))):
            b = f.replace("cleaned_", "badcol_")
            if os.path.exists(b):
                frames.append({"tag": tag, "file": f, "base": os.path.basename(f),
                               "badcol": b,
                               "photo": f.replace("cleaned_", "photoapplied_")})
    if a.limit:
        frames = frames[:a.limit]
    # 先把没有 WCS 关键字的帧剔掉并如实登记（不清洗、不猜 WCS）
    usable, skipped = [], []
    for fr in frames:
        try:
            fw0, fh0 = Z.frame_wcs(fr["file"])
            Z._homography(fw0, dw)
            usable.append(fr)
        except Exception as exc:
            skipped.append({"file": fr["base"], "reason": f"{type(exc).__name__}: {exc}"})
    frames = usable
    nf = len(frames)

    acc = {arm: {"cnt": np.zeros(n, np.int32), "bits": np.zeros(n, np.int32),
                 "sw": np.zeros(n), "swx": np.zeros(n), "swx2": np.zeros(n),
                 "cell_sum": np.zeros((nf, per_cell)), "cell_w": np.zeros((nf, per_cell))}
           for arm in ARMS}
    stats = []
    with open(os.path.join(a.out, "p5z_partial.json"), "w") as fp:
        json.dump({"reference": {"nan_frac": float(np.mean(~np.isfinite(ref_img)))},
                   "inputs": {"n_frames": nf, "n_skipped_no_wcs": len(skipped), "skipped": skipped,
                              "files": [f["base"] for f in frames]}}, fp, indent=1, default=Z._j)
    # ---------------- pass 1：投影 + 累积
    for k, fr in enumerate(frames):
        fw, fh = Z.frame_wcs(fr["file"])
        H = Z._homography(fw, dw)
        bad = fits.getdata(fr["badcol"])
        snr = Z.frame_snr(os.path.dirname(fr["file"]))
        st = {"frame": k, "tag": fr["tag"], "file": fr["base"], "snr": snr,
              "homography_residual_px": getattr(Z._homography, "last_residual", None),
              "crval": [float(fh["CRVAL1"]), float(fh["CRVAL2"])],
              "cdelt_arcsec": float(fh.get("CDELT1", np.nan)) * 3600,
              "cd_arcsec": float(fh.get("CD1_1", np.nan)) * 3600}
        for arm in ARMS:
            path = fr["file"] if arm == "cleaned" else fr["photo"]
            if not os.path.exists(path):
                st[arm] = {"missing": True}
                continue
            img = fits.getdata(path).astype(np.float32)
            img = np.where(bad > 0, np.nan, img)
            idx, v = Z.sample_image(fw, dw, img, H)
            w = np.full(v.shape, float(snr), dtype=np.float64)
            A = acc[arm]
            A["cnt"] += np.bincount(idx, minlength=n).astype(np.int32)
            A["bits"] |= (np.bincount(idx, minlength=n) > 0).astype(np.int32) << k
            A["sw"] += np.bincount(idx, weights=w, minlength=n)
            A["swx"] += np.bincount(idx, weights=w * v, minlength=n)
            A["swx2"] += np.bincount(idx, weights=w * v * v, minlength=n)
            yy = idx // Z.NOUT; xx = idx % Z.NOUT
            cid = (yy // Z.CELL) * nxc + (xx // Z.CELL)
            A["cell_sum"][k] = np.bincount(cid, weights=w * v, minlength=per_cell)
            A["cell_w"][k] = np.bincount(cid, weights=w, minlength=per_cell)
            st[arm] = {"n_samples": int(idx.size),
                       "median": float(np.nanmedian(v)) if v.size else None,
                       "min": float(np.nanmin(v)) if v.size else None,
                       "max": float(np.nanmax(v)) if v.size else None}
        stats.append(st)
        if k % 5 == 0:
            print("  pass1 %2d/%d %.0fs" % (k + 1, nf, time.time() - t0), flush=True)

    res = {"reference": {"file": Z.REF, "nx": Z.NOUT, "ny": Z.POUT,
                         "nan_frac": float(np.mean(~np.isfinite(ref_img))),
                         "cd": [float(dh["CD1_1"]), float(dh["CD2_2"])],
                         "cdelt_arcsec": abs(float(dh["CD1_1"])) * 3600,
                         "crval": [float(dh["CRVAL1"]), float(dh["CRVAL2"])],
                         "crpix": [float(dh["CRPIX1"]), float(dh["CRPIX2"])],
                         "ctype": [str(dh["CTYPE1"]), str(dh["CTYPE2"])]},
           "inputs": {"n_frames": nf, "n_skipped_no_wcs": len(skipped), "skipped": skipped,
                      "per_tag": {t: sum(1 for f in frames if f["tag"] == t) for t in ("t2", "t3")},
                      "files": [f["base"] for f in frames]},
           "frames": stats}
    refcov = np.isfinite(ref_img).reshape(-1)
    # ---------------- 拟合路径 A（逐臂）并累积修正堆栈
    fits_info = {}
    arms_out = {}
    for arm in ARMS:
        A = acc[arm]
        cm = np.where(A["cell_w"] > 0, A["cell_sum"] / np.maximum(A["cell_w"], 1e-300), np.nan)
        rows, cols, data, rhs = [], [], [], []
        r = 0
        for i in range(nf):
            for j in range(i + 1, nf):
                m = np.isfinite(cm[i]) & np.isfinite(cm[j])
                nn = int(m.sum())
                if nn < 10:
                    continue
                d = (cm[i] - cm[j])[m]; Bm = B[m]
                for c in range(3):
                    rows.append(np.arange(r, r + nn)); cols.append(np.full(nn, i * 3 + c)); data.append(Bm[:, c])
                    rows.append(np.arange(r, r + nn)); cols.append(np.full(nn, j * 3 + c)); data.append(-Bm[:, c])
                rhs.append(d); r += nn
        As = csr_matrix((np.concatenate(data), (np.concatenate(rows), np.concatenate(cols))),
                        shape=(r, nf * 3)).toarray()
        G = np.zeros((3, nf * 3))
        for c in range(3):
            for k in range(nf):
                G[c, k * 3 + c] = 1.0
        th = np.linalg.lstsq(np.vstack([As, G * 1e-3]), np.concatenate(rhs + [np.zeros(3)]),
                             rcond=None)[0].reshape(nf, 3)
        resid = cm - (B @ th.T).T
        have = np.isfinite(resid)
        cellcorr = np.where(have, resid, 0.0)
        for _ in range(4):
            num = have.sum(axis=0)
            mu = np.where(num > 0, np.where(have, cellcorr, 0.0).sum(axis=0) / np.maximum(num, 1), 0.0)
            cellcorr = np.where(have, cellcorr - mu[None, :], 0.0)
        # pass 2
        cw = np.zeros(n); cwx = np.zeros(n)
        for k, fr in enumerate(frames):
            fw, _ = Z.frame_wcs(fr["file"])
            H = Z._homography(fw, dw)
            path = fr["file"] if arm == "cleaned" else fr["photo"]
            if not os.path.exists(path):
                continue
            img = fits.getdata(path).astype(np.float32)
            bad = fits.getdata(fr["badcol"])
            img = np.where(bad > 0, np.nan, img)
            idx, v = Z.sample_image(fw, dw, img, H)
            w = np.full(v.shape, float(stats[k]["snr"]), dtype=np.float64)
            yy = idx // Z.NOUT; xx = idx % Z.NOUT
            cid = (yy // Z.CELL) * nxc + (xx // Z.CELL)
            corr = B[cid] @ th[k] + cellcorr[k][cid]
            cw += np.bincount(idx, weights=w, minlength=n)
            cwx += np.bincount(idx, weights=w * (v - corr), minlength=n)
            if k % 9 == 0:
                print("  pass2 %s %2d/%d %.0fs" % (arm, k + 1, nf, time.time() - t0), flush=True)
        raw = np.where(A["sw"] > 0, A["swx"] / np.maximum(A["sw"], 1e-300), np.nan)
        Aimg = np.where(cw > 0, cwx / np.maximum(cw, 1e-300), np.nan)
        cov = A["cnt"] > 0
        inter = np.sum(cov & refcov); union = np.sum(cov | refcov)
        arms_out[arm] = {
            "coverage": {"n_covered_px": int(cov.sum()), "frac_of_grid": float(cov.mean()),
                         "nan_frac": float(1 - cov.mean()),
                         "iou_with_reference": float(inter / max(union, 1)),
                         "ref_cov_covered_by_mine": float(inter / max(refcov.sum(), 1)),
                         "hist": {str(int(kk)): int(vv) for kk, vv in
                                  zip(*np.unique(A["cnt"][cov], return_counts=True))}},
            "path_A_fit": {"theta_const": th[:, 0].tolist(),
                           "cell_field_rms": float(np.sqrt(np.mean(cellcorr ** 2)))},
            "arm_median": float(np.nanmedian(raw)) if np.isfinite(raw).any() else None}
        # 判据
        p_, q_ = Z._edges(A["bits"], A["cnt"])
        out = {}
        for name, flat in (("raw", raw), ("A", Aimg)):
            img = np.asarray(flat, dtype=np.float64).reshape(Z.POUT, Z.NOUT)
            bg = Z._bg(img)
            stt = Z._seam(img, p_, q_, bg)
            out[name] = {"nan_frac": float(np.mean(~np.isfinite(img))), "seam": Z._summ(stt),
                         "seam_groups": Z._groups(A["bits"], A["cnt"], stt), "_st": stt}
        refst = Z._seam(ref_img.astype(np.float64), p_, q_, Z._bg(ref_img.astype(np.float64)))
        out["prod_ref_on_my_edges"] = {"n_edges": int(np.sum(np.isfinite(refst["rel"]))),
                                       "seam": Z._summ(refst), "seam_groups": Z._groups(A["bits"], A["cnt"], refst)}
        out["A"]["edges_vs_raw"] = Z._compare(out["raw"]["_st"], out["A"]["_st"])
        out["A"]["edges_vs_ref"] = Z._compare(refst, out["A"]["_st"])
        out["raw"]["edges_vs_ref"] = Z._compare(refst, out["raw"]["_st"])
        out["raw"]["consistency"] = Z._consistency(A["sw"], A["swx"], A["swx2"], A["cnt"])
        out["raw"]["structure_vs_ref"] = Z._structure(np.asarray(raw).reshape(Z.POUT, Z.NOUT), ref_img, A["cnt"])
        out["A"]["structure_vs_ref"] = Z._structure(np.asarray(Aimg).reshape(Z.POUT, Z.NOUT), ref_img, A["cnt"])
        for kk in list(out):
            out[kk].pop("_st", None)
        arms_out[arm]["paths"] = out
        # FITS
        h = fits.Header()                      # 逐字对齐生产成品的 WCS 关键字（CD 矩阵，无 CDELT/PC）
        h["NAXIS"] = 2; h["NAXIS1"] = Z.NOUT; h["NAXIS2"] = Z.POUT
        h["CTYPE1"] = "RA---TAN"; h["CTYPE2"] = "DEC--TAN"
        h["CUNIT1"] = "deg"; h["CUNIT2"] = "deg"
        h["CRPIX1"] = float(dh["CRPIX1"]); h["CRPIX2"] = float(dh["CRPIX2"])
        h["CRVAL1"] = float(dh["CRVAL1"]); h["CRVAL2"] = float(dh["CRVAL2"])
        h["CD1_1"] = float(dh["CD1_1"]); h["CD1_2"] = float(dh["CD1_2"])
        h["CD2_1"] = float(dh["CD2_1"]); h["CD2_2"] = float(dh["CD2_2"])
        h["BUNIT"] = ("ADU", "same unit as the input phase-1 frame product")
        h["P5ARM"] = (arm[:20], "input frame product arm")
        for name, arr in (("raw", raw), ("A", Aimg)):
            pth = os.path.join(a.fits, f"p5z_{arm}_{name}.fits")
            fits.PrimaryHDU(data=np.asarray(arr, dtype=np.float32).reshape(Z.POUT, Z.NOUT),
                            header=h).writeto(pth, overwrite=True)
            fits_info[f"{arm}_{name}"] = {"file": pth, "nx": Z.NOUT, "ny": Z.POUT,
                                          "nan_frac": float(np.mean(~np.isfinite(arr))),
                                          "cdelt_arcsec": abs(float(h["CD1_1"])) * 3600,
                                          "cd": [float(h["CD1_1"]), float(h["CD2_2"])],
                                          "crpix": [float(h["CRPIX1"]), float(h["CRPIX2"])],
                                          "crval": [float(h["CRVAL1"]), float(h["CRVAL2"])]}
        if a.thumbs:
            Z._thumb(np.asarray(raw).reshape(Z.POUT, Z.NOUT),
                     os.path.join(a.thumbs, f"p5z_{arm}_raw.png"), f"{arm} raw stack (production grid)")
            Z._thumb(np.asarray(Aimg).reshape(Z.POUT, Z.NOUT),
                     os.path.join(a.thumbs, f"p5z_{arm}_A.png"), f"{arm} path A (production grid)")
        res["arms_%s" % arm] = arms_out[arm]
        del A["swx2"]
    res["fits"] = fits_info
    if a.thumbs:
        Z._thumb(ref_img, os.path.join(a.thumbs, "ref_production.png"), "production export (reference grid)")
        res["thumbs"] = "deliver/thumbs_p5z/*.png"
    res["elapsed_s"] = time.time() - t0
    with open(os.path.join(a.out, "p5z_readouts.json"), "w") as fp:
        json.dump(res, fp, indent=1, default=Z._j, sort_keys=True)
    for arm in ARMS:
        o = res["arms_%s" % arm]
        print("== %s == NaN raw %.1f%% / A %.1f%% | 覆盖 %.1f%% of 生产成品 | IoU %.3f" % (
            arm, 100 * o["paths"]["raw"]["nan_frac"], 100 * o["paths"]["A"]["nan_frac"],
            100 * o["coverage"]["ref_cov_covered_by_mine"], o["coverage"]["iou_with_reference"]))
        for nm in ("raw", "A", "prod_ref_on_my_edges"):
            s = o["paths"][nm]["seam"]
            print("   接缝 %-22s rel中位=%.4g p95=%.4g  逐帧系统电平 %s" % (
                nm, s["rel_median_abs"], s["rel_p95_abs"],
                {g: round(v["mean_rel"], 4) for g, v in o["paths"][nm]["seam_groups"].items()}))
        print("   逐边 A vs raw:", json.dumps(o["paths"]["A"]["edges_vs_raw"]))
    print("written", os.path.join(a.out, "p5z_readouts.json"), "%.0fs" % res["elapsed_s"])


if __name__ == "__main__":
    main()
