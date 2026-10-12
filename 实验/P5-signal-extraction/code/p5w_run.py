"""P5 第二轮重做：双线性重采样 + 覆盖均匀性硬自检，自检通过后才测判据。

与上一轮的差别（对应派单第 1 条）：
  · 重采样由「最近落格」改为**双线性投影**（与生产 export 的 `SAMPLER=bilinear` 同口径）：
    每个帧像素按 (1-fx)(1-fy), fx(1-fy), (1-fx)fy, fx·fy 分摊到输出网格的 4 个邻像素；
  · 新增**行/列覆盖均匀性硬自检**（`uniformity`）：自检不过就**不测任何判据**；
  · 路径 A 有界化：先按帧级中位数做粗对齐，再拟合「偏移+平面+粗格场」，粗格场幅度以帧级天光量级为上限。

阶段：
  `--stage 1`：投影 + 累积 → 写 raw FITS、缩略图、uniformity 自检、把累加量存盘（.npy）
  `--stage 2`：读回累加量 → 有界路径 A → 写 A FITS → 判据（仅在 uniformity 通过时执行）
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
# 覆盖均匀性阈值（见 docs/13_UNIFORMITY.md 的定阈理由）
ALT_MAX = 0.10          # 行/列计数的相邻差中位 / 计数中位 ≤ 10%
ZERO_INTERIOR_MAX = 0   # 足迹内部不允许整行/整列零覆盖


def sample_bilinear(fw, dw, img, H=None, nx=Z.NOUT, ny=Z.POUT):
    """帧像素 → 输出网格 4 邻像素的双线性分摊。返回 (idx4, w4, v4)。"""
    if H is None:
        H = Z._homography(fw, dw)
    ny_i, nx_i = img.shape
    N = ny_i * nx_i
    flat = np.arange(N, dtype=np.int64)
    p = np.empty((N, 2), dtype=np.float64)
    p[:, 0] = flat % nx_i
    p[:, 1] = flat // nx_i
    q = Z._apply_H(H, p)
    del p
    v = np.asarray(img, dtype=np.float64).reshape(-1)
    xo, yo = q[:, 0], q[:, 1]
    del q
    ok = np.isfinite(v) & (xo >= 0) & (xo <= nx - 1) & (yo >= 0) & (yo <= ny - 1)
    xo, yo, v = xo[ok], yo[ok], v[ok]
    ix = np.floor(xo).astype(np.int64)
    iy = np.floor(yo).astype(np.int64)
    np.clip(ix, 0, nx - 2, out=ix)
    np.clip(iy, 0, ny - 2, out=iy)
    fx = xo - ix
    fy = yo - iy
    idxs, wts, vals = [], [], []
    for dx, dy, wx, wy in ((0, 0, 1 - fx, 1 - fy), (1, 0, fx, 1 - fy),
                           (0, 1, 1 - fx, fy), (1, 1, fx, fy)):
        idxs.append((iy + dy) * nx + (ix + dx))
        wts.append(wx * wy)
        vals.append(v)
    return np.concatenate(idxs), np.concatenate(wts), np.concatenate(vals)


def uniformity(ncov, tag):
    """行/列覆盖均匀性硬自检。"""
    cnt = (ncov > 0)
    rows = cnt.sum(axis=1).astype(np.int64)
    cols = cnt.sum(axis=0).astype(np.int64)
    out = {}
    for name, arr, n in (("rows", rows, cnt.shape[0]), ("cols", cols, cnt.shape[1])):
        nz = arr[arr > 0]
        if nz.size < 3:
            out[name] = {"n_with_data": int(nz.size), "pass": False}
            continue
        med = float(np.median(nz))
        d = np.abs(np.diff(arr.astype(float)))
        alt = float(np.median(d) / max(med, 1e-9))
        # 足迹内部整行/整列零覆盖：两端有数据、内部为 0
        first, last = int(np.argmax(arr > 0)), int(n - 1 - np.argmax(arr[::-1] > 0))
        interior_zero = int(np.sum(arr[first:last + 1] == 0))
        out[name] = {"min": int(nz.min()), "max": int(nz.max()), "median": med,
                     "max_over_min": float(nz.max() / max(nz.min(), 1)),
                     "alt_ratio": alt, "interior_zero": interior_zero,
                     "pass": bool(alt <= ALT_MAX and interior_zero <= ZERO_INTERIOR_MAX)}
    out["pass"] = bool(out.get("rows", {}).get("pass") and out.get("cols", {}).get("pass"))
    out["tag"] = tag
    out["thresholds"] = {"alt_ratio_max": ALT_MAX, "interior_zero_max": ZERO_INTERIOR_MAX,
                         "reason": "帧比输出细（0.967″ vs 1.8″）× 各帧方位角相近时，"
                                   "落格法会在输出行方向拍频；双线性分摊后单行计数应随足迹平滑变化。"
                                   "相邻行计数变化中位超过 10%（约 435 px/行的边界长度）即判为拍频痕。"}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--fits", default=None)
    ap.add_argument("--thumbs", default=None)
    ap.add_argument("--stage", type=int, default=1)
    ap.add_argument("--limit", type=int, default=0)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    dw, dh = Z.ref_grid()
    n = Z.POUT * Z.NOUT
    nyc, nxc = Z.POUT // Z.CELL, Z.NOUT // Z.CELL
    per_cell = nyc * nxc
    cy, cx = np.mgrid[0:nyc, 0:nxc]
    cu = ((cx.ravel() - (nxc - 1) / 2.0) * Z.CELL * Z.CD_DEG).astype(np.float64)
    cv = ((cy.ravel() - (nyc - 1) / 2.0) * Z.CELL * Z.CD_DEG).astype(np.float64)
    B = np.stack([np.ones_like(cu), cu, cv], axis=1)

    if a.stage == 1:
        frames = []
        for tag in ("t2", "t3"):
            for f in sorted(glob.glob(os.path.join(PDIR, tag, "cleaned_*.fts"))):
                b = f.replace("cleaned_", "badcol_")
                if os.path.exists(b):
                    frames.append({"tag": tag, "file": f, "base": os.path.basename(f), "badcol": b,
                                   "photo": f.replace("cleaned_", "photoapplied_")})
        usable, skipped = [], []
        for fr in frames:
            try:
                fw0, _ = Z.frame_wcs(fr["file"]); Z._homography(fw0, dw); usable.append(fr)
            except Exception as exc:
                skipped.append({"file": fr["base"], "reason": f"{type(exc).__name__}: {exc}"})
        frames = usable
        if a.limit:
            frames = frames[:a.limit]
        nf = len(frames)
        acc = {arm: {"den": np.zeros(n), "num": np.zeros(n), "cnt_bits": np.zeros(n, np.int64),
                     "cell_sum": np.zeros((nf, per_cell)), "cell_w": np.zeros((nf, per_cell)),
                     "snr": np.zeros(nf)} for arm in ARMS}
        t0 = time.time()
        for k, fr in enumerate(frames):
            fw, fh = Z.frame_wcs(fr["file"])
            H = Z._homography(fw, dw)
            bad = fits.getdata(fr["badcol"])
            snr = Z.frame_snr(os.path.dirname(fr["file"]))
            for arm in ARMS:
                path = fr["file"] if arm == "cleaned" else fr["photo"]
                if not os.path.exists(path):
                    continue
                img = fits.getdata(path).astype(np.float64)
                img = np.where(bad > 0, np.nan, img)
                idx, w4, v4 = sample_bilinear(fw, dw, img, H)
                A = acc[arm]
                A["snr"][k] = snr
                A["den"] += np.bincount(idx, weights=snr * w4, minlength=n)
                A["num"] += np.bincount(idx, weights=snr * w4 * v4, minlength=n)
                cb = np.bincount(idx, minlength=n) > 0
                A["cnt_bits"] |= cb.astype(np.int64) << k
                cid = (idx // Z.NOUT // Z.CELL) * nxc + ((idx % Z.NOUT) // Z.CELL)
                A["cell_sum"][k] = np.bincount(cid, weights=snr * w4 * v4, minlength=per_cell)
                A["cell_w"][k] = np.bincount(cid, weights=snr * w4, minlength=per_cell)
                del idx, w4, v4, cid
            if k % 4 == 0:
                print("  stage1 %2d/%d %.0fs" % (k + 1, nf, time.time() - t0), flush=True)
        res = {"inputs": {"n_frames": nf, "n_skipped_no_wcs": len(skipped), "skipped": skipped,
                          "files": [f["base"] for f in frames]},
               "resampling": "bilinear_4corner_splat (同生产 SAMPLER=bilinear 口径)",
               "reference_nan_frac": float(np.mean(~np.isfinite(fits.getdata(Z.REF)))),
               "frames": [{"file": f["base"], "tag": f["tag"]} for f in frames]}
        for arm in ARMS:
            A = acc[arm]
            img = np.where(A["den"] > 0, A["num"] / np.maximum(A["den"], 1e-300), np.nan)
            ncov = (A["den"] > 0).reshape(Z.POUT, Z.NOUT).astype(np.int32)
            u = uniformity(ncov, arm)
            res.setdefault("uniformity", {})[arm] = u
            res.setdefault("coverage", {})[arm] = {
                "nan_frac": float(1 - np.mean(ncov > 0)),         # 按 den>0：与位宽无关
                "nan_frac_by_bits": float(1 - np.mean(A["cnt_bits"] > 0)),
                "n_covered_px": int((ncov > 0).sum()),
                "bits_agree_with_den": bool(np.array_equal(A["cnt_bits"] > 0, A["den"] > 0)),
                "n_negative_bits": int(np.sum(A["cnt_bits"] < 0))}
            np.save(os.path.join(a.out, f"stage1_{arm}_den.npy"), A["den"])
            np.save(os.path.join(a.out, f"stage1_{arm}_num.npy"), A["num"])
            np.save(os.path.join(a.out, f"stage1_{arm}_bits.npy"), A["cnt_bits"])
            np.save(os.path.join(a.out, f"stage1_{arm}_cellsum.npy"), A["cell_sum"])
            np.save(os.path.join(a.out, f"stage1_{arm}_cellw.npy"), A["cell_w"])
            np.save(os.path.join(a.out, f"stage1_{arm}_snr.npy"), A["snr"])
            if a.fits:
                os.makedirs(a.fits, exist_ok=True)
                h = Z.ref_header(dh)
                fits.PrimaryHDU(data=img.astype(np.float32).reshape(Z.POUT, Z.NOUT),
                                header=h).writeto(os.path.join(a.fits, f"p5w_{arm}_raw.fits"),
                                                  overwrite=True)
                if a.thumbs:
                    os.makedirs(a.thumbs, exist_ok=True)
                    Z._thumb(img.reshape(Z.POUT, Z.NOUT), os.path.join(a.thumbs, f"p5w_{arm}_raw.png"),
                             f"{arm} raw (bilinear, production grid)")
            print("  [%s] NaN %.2f%% | 行 alt=%.4f min/max=%d/%d 内部零行=%d | 列 alt=%.4f | 自检 %s" % (
                arm, 100 * res["coverage"][arm]["nan_frac"], u["rows"]["alt_ratio"],
                u["rows"]["min"], u["rows"]["max"], u["rows"]["interior_zero"],
                u["cols"]["alt_ratio"], "通过" if u["pass"] else "**不通过**"))
        if a.thumbs:
            Z._thumb(fits.getdata(Z.REF), os.path.join(a.thumbs, "ref_production.png"),
                     "production export (reference grid)")
        res["stage"] = 1
        json.dump(res, open(os.path.join(a.out, "p5w_stage1.json"), "w"), indent=1, default=Z._j)
        print("stage1 完成 %.0fs → %s" % (time.time() - t0, os.path.join(a.out, "p5w_stage1.json")))
        return

    # ---------------- stage 2：有界路径 A + 判据
    st1 = json.load(open(os.path.join(a.out, "p5w_stage1.json")))
    ok_any = [arm for arm in ARMS if st1["uniformity"][arm]["pass"]]
    print("均匀性自检通过臂:", ok_any)
    if not ok_any:
        print("**所有臂的覆盖均匀性自检未通过——按派单要求不测任何判据。**")
        return
    ref_img = fits.getdata(Z.REF).astype(np.float64)
    refcov = np.isfinite(ref_img).reshape(-1)
    res = {"stage": 2, "uniformity": st1["uniformity"], "inputs": st1["inputs"],
           "resampling": st1["resampling"], "arms": {}}
    for arm in ARMS:
        bits = np.load(os.path.join(a.out, f"stage1_{arm}_bits.npy"))
        den = np.load(os.path.join(a.out, f"stage1_{arm}_den.npy"))
        num = np.load(os.path.join(a.out, f"stage1_{arm}_num.npy"))
        cs = np.load(os.path.join(a.out, f"stage1_{arm}_cellsum.npy"))
        cw = np.load(os.path.join(a.out, f"stage1_{arm}_cellw.npy"))
        nf = cs.shape[0]
        raw = np.where(den > 0, num / np.maximum(den, 1e-300), np.nan)
        ncov = (bits > 0).astype(np.int32)
        cm = np.where(cw > 0, cs / np.maximum(cw, 1e-300), np.nan)
        # 粗对齐：逐帧中位数（有界），只用于把量级拉到一起
        med = np.array([np.nanmedian(cm[i]) if np.isfinite(cm[i]).any() else 0.0 for i in range(nf)])
        cm2 = cm - med[:, None] + np.nanmedian(med)
        rows, colsD, data, rhs = [], [], [], []
        r = 0
        for i in range(nf):
            for j in range(i + 1, nf):
                m = np.isfinite(cm2[i]) & np.isfinite(cm2[j])
                nn = int(m.sum())
                if nn < 10:
                    continue
                d = (cm2[i] - cm2[j])[m]; Bm = B[m]
                for c in range(3):
                    rows.append(np.arange(r, r + nn)); colsD.append(np.full(nn, i * 3 + c)); data.append(Bm[:, c])
                    rows.append(np.arange(r, r + nn)); colsD.append(np.full(nn, j * 3 + c)); data.append(-Bm[:, c])
                r += nn
                rhs.append(d)
        As = csr_matrix((np.concatenate(data), (np.concatenate(rows), np.concatenate(colsD))),
                        shape=(r, nf * 3)).toarray()
        G = np.zeros((3, nf * 3))
        for c in range(3):
            for k in range(nf):
                G[c, k * 3 + c] = 1.0
        th = np.linalg.lstsq(np.vstack([As, G * 1e-3]), np.concatenate(rhs + [np.zeros(3)]),
                             rcond=None)[0].reshape(nf, 3)
        # 粗格场：帧间差分残差按格平均，逐格扣跨帧均值；幅度上限 = 帧级天光量级 |med|
        resid = cm2 - (B @ th.T).T
        have = np.isfinite(resid)
        cellcorr = np.where(have, resid, 0.0)
        for _ in range(4):
            numc = have.sum(axis=0)
            mu = np.where(numc > 0, np.where(have, cellcorr, 0.0).sum(axis=0) / np.maximum(numc, 1), 0.0)
            cellcorr = np.where(have, cellcorr - mu[None, :], 0.0)
        cap = float(np.nanmedian(np.abs(med))) if np.isfinite(med).any() else 0.0
        n_clipped = int(np.sum(np.abs(cellcorr) > cap)) if cap > 0 else 0
        cellcorr = np.clip(cellcorr, -cap, cap) if cap > 0 else cellcorr
        rms_before = float(np.sqrt(np.mean(cellcorr ** 2)))
        res["arms"][arm] = {"frame_median_level": float(np.nanmedian(med)),
                            "frame_median_spread": float(np.nanmax(med) - np.nanmin(med)),
                            "theta_const": th[:, 0].tolist(),
                            "cell_field_rms_before_cap": rms_before,
                            "cell_field_rms_after_cap": float(np.sqrt(np.mean(cellcorr ** 2))),
                            "cap": cap, "n_cells_clipped": n_clipped,
                            "coverage": st1["coverage"][arm]}
        # pass 2：重建帧影像，施加有界修正
        frames = []
        for tag in ("t2", "t3"):
            for f in sorted(glob.glob(os.path.join(PDIR, tag, "cleaned_*.fts"))):
                b = f.replace("cleaned_", "badcol_")
                if os.path.exists(b):
                    frames.append({"tag": tag, "file": f, "badcol": b,
                                   "photo": f.replace("cleaned_", "photoapplied_")})
        usable = []
        for fr in frames:
            try:
                fw0, _ = Z.frame_wcs(fr["file"]); Z._homography(fw0, Z.ref_grid()[0]); usable.append(fr)
            except Exception:
                pass
        frames = usable[:nf]
        cden = np.zeros(Z.POUT * Z.NOUT); cnum = np.zeros(Z.POUT * Z.NOUT)
        bits64 = np.zeros(Z.POUT * Z.NOUT, dtype=np.int64)   # 掩码已修：int64，43 帧不再溢出
        t0 = time.time()
        for k, fr in enumerate(frames):
            fw, _ = Z.frame_wcs(fr["file"])
            H = Z._homography(fw, Z.ref_grid()[0])
            path = fr["file"] if arm == "cleaned" else fr["photo"]
            if not os.path.exists(path):
                continue
            img = fits.getdata(path).astype(np.float64)
            bad = fits.getdata(fr["badcol"])
            img = np.where(bad > 0, np.nan, img)
            idx, w4, v4 = sample_bilinear(fw, Z.ref_grid()[0], img, H)
            snr = float(np.load(os.path.join(a.out, f"stage1_{arm}_snr.npy"))[k]) \
                if os.path.exists(os.path.join(a.out, f"stage1_{arm}_snr.npy")) else 1.0
            cid = (idx // Z.NOUT // Z.CELL) * nxc + ((idx % Z.NOUT) // Z.CELL)
            corr = (B[cid] @ th[k]) + cellcorr[k][cid] + (med[k] - np.nanmedian(med))
            cden += np.bincount(idx, weights=snr * w4, minlength=Z.POUT * Z.NOUT)
            cnum += np.bincount(idx, weights=snr * w4 * (v4 - corr), minlength=Z.POUT * Z.NOUT)
            bits64 |= (np.bincount(idx, minlength=Z.POUT * Z.NOUT) > 0).astype(np.int64) << k
            del idx, w4, v4
            if k % 8 == 0:
                print("  stage2 %s %2d/%d %.0fs" % (arm, k + 1, nf, time.time() - t0), flush=True)
        Aimg = np.where(cden > 0, cnum / np.maximum(cden, 1e-300), np.nan).reshape(Z.POUT, Z.NOUT)
        raw2 = raw.reshape(Z.POUT, Z.NOUT)
        # 掩码自检三项（派单要求）
        den_mask = den > 0
        b64_mask = bits64 > 0
        chk = {"coverage_by_den": float(np.mean(den_mask)),
               "coverage_by_bits64": float(np.mean(b64_mask)),
               "bits64_equals_den": bool(np.array_equal(b64_mask, den_mask)),
               "n_negative_bits": int(np.sum(bits64 < 0)),
               "nan_frac_den": float(1 - np.mean(den_mask)),
               "nan_frac_bits64": float(1 - np.mean(b64_mask))}
        res["mask_selfcheck"] = res.get("mask_selfcheck", {}); res["mask_selfcheck"][arm] = chk
        bits = bits64.reshape(Z.POUT, Z.NOUT); ncov = b64_mask.astype(np.int32).reshape(Z.POUT, Z.NOUT)
        p_, q_ = Z._edges(bits, ncov)
        out = {}
        for nm, im in (("raw", raw2), ("A", Aimg)):
            bg = Z._bg(im)
            st = Z._seam(im, p_, q_, bg)
            out[nm] = {"nan_frac": float(np.mean(~np.isfinite(im))), "seam": Z._summ(st),
                       "seam_groups": Z._groups(bits, ncov, st), "_st": st}
        refst = Z._seam(ref_img, p_, q_, Z._bg(ref_img))
        out["prod_ref_on_my_edges"] = {"n_edges": int(np.sum(np.isfinite(refst["rel"]))),
                                       "seam": Z._summ(refst),
                                       "seam_groups": Z._groups(bits, ncov, refst)}
        out["A"]["edges_vs_raw"] = Z._compare(out["raw"]["_st"], out["A"]["_st"])
        out["A"]["edges_vs_ref"] = Z._compare(refst, out["A"]["_st"])
        out["raw"]["edges_vs_ref"] = Z._compare(refst, out["raw"]["_st"])
        ok_cells = np.isfinite(cm)
        with np.errstate(invalid="ignore"):
            cmean = np.where(ok_cells, cm, np.nan)
            cmu = np.nanmean(cmean, axis=0)
            sc = np.sqrt(np.nanmean((cmean - cmu[None, :]) ** 2, axis=0))
        res["arms"][arm]["consistency_cellscale"] = {
            "median": float(np.nanmedian(sc)), "n_cells": int(np.sum(np.isfinite(sc))),
            "cell_arcsec": Z.CELL * Z.CD_DEG * 3600}
        for kk in list(out):
            out[kk].pop("_st", None)
        res["arms"][arm]["paths"] = out
        if a.fits:
            h = Z.ref_header(Z.ref_grid()[1])
            fits.PrimaryHDU(data=Aimg.astype(np.float32), header=h).writeto(
                os.path.join(a.fits, f"p5w_{arm}_A.fits"), overwrite=True)
            if a.thumbs:
                Z._thumb(Aimg, os.path.join(a.thumbs, f"p5w_{arm}_A.png"), f"{arm} path A (bilinear)")
        print("  [%s] 接缝 raw %.4g → A %.4g | 改善 %s" % (
            arm, out["raw"]["seam"]["rel_median_abs"], out["A"]["seam"]["rel_median_abs"],
            out["A"]["edges_vs_raw"]["frac_improved"]))
    json.dump(res, open(os.path.join(a.out, "p5w_stage2.json"), "w"), indent=1, default=Z._j)
    print("stage2 完成 →", os.path.join(a.out, "p5w_stage2.json"))


if __name__ == "__main__":
    main()
