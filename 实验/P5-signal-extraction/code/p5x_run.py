"""P5 信号提取：驱动。复算全部路径读数、写 FITS 成品与 JSON。

一条命令跑完：
  python3 p5x_run.py --root <mosaic 产物目录> --out <结果目录> [--fits <目录>]
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p5x_io as io          # noqa: E402
import p5x_metrics as M      # noqa: E402
import p5x_paths as P        # noqa: E402

ROOT = "/workspace/Astro CS Database"
DEF_PROD = ROOT + "/run/LAMBDA-ROLLBACK/base/mosaic"
DEF_STARMASK = ROOT + "/run/STARMASK"


def _j(x):
    if isinstance(x, (np.floating,)):
        return float(x)
    if isinstance(x, (np.integer,)):
        return int(x)
    if isinstance(x, np.ndarray):
        return x.tolist()
    raise TypeError(str(type(x)))


def readouts(prod, stack_img, edges, base=None, tag=""):
    out = {}
    bg = M.local_bg_image(stack_img, prod)
    st = M.seam_readout(prod, stack_img, edges[0], edges[1], bg)
    out["seam"] = M.seam_summary(st)
    out["seam_groups"] = M.seam_groups(prod, st)
    if base is not None:
        out["edges_vs_base"] = M.compare_edges(base, st)
    out["st"] = st
    return out


def path_report(prod, corr, edges, base_st, base_stack, tag):
    st_img, _ = M.stack_weighted(corr, prod)
    r = readouts(prod, st_img, edges, base=base_st, tag=tag)
    r["consistency"] = M.frame_consistency(prod, corr)
    r["structure"] = M.structure_metrics(prod, st_img, base_stack)
    r["stack"] = st_img
    return r


def multiplicative_diag(prod, sample=5, sigma=3.0):
    """逐帧对的乘性/加性残差（在**已校正**帧上做稳健直线拟合）。

    加性天光若被完全扣掉，两帧在同一位置应满足 `corrected_l = corrected_k`；残差分解为
      · 乘性残差 `s − 1`（随电平缩放的部分）
      · 加性残差 `c / level`（与电平无关的部分）
    拟合用 sigma 裁剪的迭代最小二乘。
    """
    valid = prod.valid
    x_all = np.nan_to_num(prod.corrected)
    out = {}
    for k in range(prod.nframe):
        for l in range(k + 1, prod.nframe):
            m = valid[k] & valid[l]
            idx = np.flatnonzero(m)[::sample]
            if idx.size < 200:
                continue
            x, y = x_all[k, idx], x_all[l, idx]
            keep = np.ones(idx.size, dtype=bool)
            s, c = 1.0, 0.0
            for _ in range(6):
                A = np.stack([x[keep], np.ones(keep.sum())], 1)
                s, c = np.linalg.lstsq(A, y[keep], rcond=None)[0]
                r = y - (s * x + c)
                sc = 1.482602218505602 * np.median(np.abs(r[keep] - np.median(r[keep])))
                if not np.isfinite(sc) or sc <= 0:
                    break
                keep = np.abs(r) < sigma * sc
                if keep.sum() < 100:
                    break
            lvl = float(np.median(np.abs(y[keep]))) if keep.sum() else float("nan")
            out[f"{k}-{l}"] = {"slope": float(s), "mult_residual": float(s - 1.0),
                               "add_residual_over_level": float(c / lvl) if lvl else None,
                               "rms_after_fit_over_level": float(np.sqrt(np.mean((y[keep] - s * x[keep] - c) ** 2)) / lvl) if lvl else None,
                               "n": int(keep.sum())}
    return out


def mask_consistency(prod_a, prod_b, tag_a="det1", tag_b="maskoff"):
    """两个产物的逐帧合法掩膜差异与覆盖差异（掩膜不一致是否改变帧集）。"""
    if prod_a.nframe != prod_b.nframe or prod_a.npix != prod_b.npix:
        return {"comparable": False}
    same_tiles = prod_a.tiles == prod_b.tiles
    out = {"comparable": True, "same_union_tiles": bool(same_tiles),
           "per_frame_mask_diff": [], "coverage_diff": {}}
    for k in range(prod_a.nframe):
        a, b = prod_a.valid[k], prod_b.valid[k]
        out["per_frame_mask_diff"].append({"frame": k, "n_a": int(a.sum()), "n_b": int(b.sum()),
                                           "n_xor": int(np.sum(a ^ b)),
                                           "frac_xor_of_union": float(np.mean(a ^ b))})
    ca, cb = prod_a.ncover, prod_b.ncover
    out["coverage_diff"] = {"n_diff_pixels": int(np.sum(ca != cb)),
                            "frac": float(np.mean(ca != cb)),
                            "mean_abs_diff": float(np.mean(np.abs(ca.astype(int) - cb.astype(int))))}
    return out


def write_fits(prod, stack_img, path, scale_arcsec=0.1007, pad=8):
    """把并集网格的堆栈写成单个切平面 FITS（最近邻填充，无插值），WCS 用 TAN。"""
    from astropy.io import fits
    from astropy.wcs import WCS
    import healpy as hp
    s = np.radians(scale_arcsec / 3600.0)
    u, v = prod.u, prod.v
    u0, u1 = u.min() * np.pi / 180, u.max() * np.pi / 180
    v0, v1 = v.min() * np.pi / 180, v.max() * np.pi / 180
    nu = int(np.ceil((u1 - u0) / s)) + 2 * pad
    nv = int(np.ceil((v1 - v0) / s)) + 2 * pad
    cu = (u0 + u1) / 2.0
    cv = (v0 + v1) / 2.0
    iu = np.arange(nu) - (nu - 1) / 2.0
    iv = np.arange(nv) - (nv - 1) / 2.0
    U = cu + iu * s
    V = cv + iv * s
    UU, VV = np.meshgrid(U, V)
    ra0 = np.radians(prod.sp["ra0_deg"])
    dec0 = np.radians(prod.sp["dec0_deg"])
    rho2 = UU ** 2 + VV ** 2
    cosc = 1.0 / np.sqrt(1.0 + rho2)
    # 切平面 (U,V) → (ra,dec)（TAN 逆投影，小角展开与精确式同阶）
    dec = np.arcsin(cosc * (np.sin(dec0) + VV * np.cos(dec0)))
    ra = ra0 + np.arctan2(UU, np.cos(dec0) - VV * np.sin(dec0))
    ipix = hp.ang2pix(io.LEAF_NSIDE, np.degrees(ra).ravel(), np.degrees(dec).ravel(),
                      nest=True, lonlat=True)
    order = np.argsort(prod.leaf_nest)
    pos = np.searchsorted(prod.leaf_nest[order], ipix)
    pos = np.clip(pos, 0, prod.leaf_nest.size - 1)
    hit = prod.leaf_nest[order][pos] == ipix
    img = np.full(ipix.size, np.nan)
    img[hit] = stack_img[order[pos[hit]]]
    img = img.reshape(nv, nu)
    w = WCS(naxis=2)
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    w.wcs.crval = [prod.sp["ra0_deg"], prod.sp["dec0_deg"]]
    w.wcs.crpix = [1, 1]
    w.wcs.cdelt = [-scale_arcsec / 3600.0, scale_arcsec / 3600.0]
    # 切平面中心对应的像素
    w.wcs.crpix = [nu / 2.0 + 0.5 - cu / s, nv / 2.0 + 0.5 - cv / s]
    h = w.to_header()
    h["BUNIT"] = ("ADU", "same normalized unit as the input product")
    h["P5PATH"] = (os.path.basename(path)[:60], "P5 signal extraction path")
    hdu = fits.PrimaryHDU(data=img.astype(np.float32), header=h)
    hdu.writeto(path, overwrite=True)
    return {"file": path, "nx": nu, "ny": nv, "n_valid": int(np.isfinite(img).sum()),
            "scale_arcsec": scale_arcsec}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=DEF_PROD)
    ap.add_argument("--starmask", default=DEF_STARMASK)
    ap.add_argument("--out", required=True)
    ap.add_argument("--fits", default=None)
    ap.add_argument("--no-fits", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    res = {"root": a.root}
    t0 = time.time()

    p = io.load(a.root)
    res["product"] = {"n_frames": p.nframe, "frame_ids": p.frame_ids, "n_union_pixels": int(p.npix),
                      "n_tiles": len(p.tiles), "leaf_nside": io.LEAF_NSIDE,
                      "coverage_hist": {str(int(k)): int(v) for k, v in
                                        zip(*np.unique(p.ncover, return_counts=True))},
                      "valid_per_frame": [int(x) for x in p.valid.sum(axis=1)]}
    res["sample_check"] = p.sample_check()
    edges = M.seam_edges(p)
    res["n_seam_edges"] = int(edges[0].size)

    stacks = {}
    stacks["raw"], _ = M.stack_weighted(p.raw, p)
    stacks["prod"], _ = M.stack_weighted(p.corrected, p)
    base = readouts(p, stacks["prod"], edges)
    base_st = base.pop("st")
    res["baseline_prod"] = base
    raw_r = readouts(p, stacks["raw"], edges)
    raw_st = raw_r.pop("st")
    res["baseline_raw"] = raw_r
    res["baseline_raw"]["consistency"] = M.frame_consistency(p, p.raw)
    res["baseline_prod"]["consistency"] = M.frame_consistency(p, p.corrected)
    res["baseline_prod"]["structure"] = M.structure_metrics(p, stacks["prod"], stacks["raw"])
    res["noise_floor_abs_step"] = M.noise_floor(p, base_st)

    struct = M.structure_mask(p, stacks["raw"])
    res["structure_mask"] = {"n_px": int(struct.sum()), "q": 99.9, "dilate": 2}

    # ---------------- 路径 A
    corrA, infoA = P.path_A_mode_align(p)
    rA = path_report(p, corrA, edges, base_st, stacks["raw"], "A")
    stacks["A"] = rA.pop("stack")
    rA["info"] = infoA
    res["path_A_mode_align"] = rA

    # ---------------- 路径 B
    corrB, infoB = P.path_B_seam_lsq(p, edges[0], edges[1])
    rB = path_report(p, corrB, edges, base_st, stacks["raw"], "B")
    stacks["B"] = rB.pop("stack")
    rB["info"] = infoB
    res["path_B_seam_lsq"] = rB

    # ---------------- 路径 C
    try:
        corrC, infoC = P.path_C_sky_samples(p, struct)
        rC = path_report(p, corrC, edges, base_st, stacks["raw"], "C")
        stacks["C"] = rC.pop("stack")
        rC["info"] = infoC
        res["path_C_sky_samples"] = rC
    except Exception as exc:      # pragma: no cover
        res["path_C_sky_samples"] = {"error": f"{type(exc).__name__}: {exc}"}

    # ---------------- 路径 D（权重）
    res["path_D_weights"] = {}
    for name, w in P.path_D_weights(p).items():
        st_img, _ = M.stack_weighted(p.corrected, p, weights=w)
        r = readouts(p, st_img, edges, base=base_st)
        st = r.pop("st")
        r["consistency"] = M.frame_consistency(p, p.corrected, weights=w)
        r["structure"] = M.structure_metrics(p, st_img, stacks["raw"])
        res["path_D_weights"][name] = r
        if name == "frame_const":
            stacks["D_frame_const"] = st_img

    # ---------------- 归因
    res["diagnostics"] = {
        "multiplicative_pairwise": multiplicative_diag(p),
        "seam_attribution": {
            "raw_abs_step_median": res["baseline_raw"]["seam"]["abs_step_median"],
            "prod_abs_step_median": res["baseline_prod"]["seam"]["abs_step_median"],
            "A_abs_step_median": res["path_A_mode_align"]["seam"]["abs_step_median"],
            "B_abs_step_median": res["path_B_seam_lsq"]["seam"]["abs_step_median"],
            "production_removes_frac": float(1 - res["baseline_prod"]["seam"]["abs_step_median"] /
                                             res["baseline_raw"]["seam"]["abs_step_median"])},
    }
    for tag, root in (("det1", a.starmask + "/out_det1"), ("maskoff", a.starmask + "/out_maskoff")):
        try:
            q = io.load(root)
            st_img, _ = M.stack_weighted(q.corrected, q)
            r = readouts(q, st_img, M.seam_edges(q))
            res["diagnostics"].setdefault("mask_variants", {})[tag] = r["seam"]
        except Exception as exc:
            res["diagnostics"].setdefault("mask_variants", {})[tag] = {"error": str(exc)}
    try:
        qa = io.load(a.starmask + "/out_det1")
        qb = io.load(a.starmask + "/out_maskoff")
        res["diagnostics"]["mask_consistency"] = mask_consistency(qa, qb)
    except Exception as exc:
        res["diagnostics"]["mask_consistency"] = {"error": str(exc)}

    res["elapsed_s"] = time.time() - t0
    with open(os.path.join(a.out, "p5x_readouts.json"), "w") as f:
        json.dump(res, f, indent=1, default=_j, sort_keys=True)

    # ---------------- FITS 成品（只交最终叠好的成品）
    if not a.no_fits and a.fits:
        os.makedirs(a.fits, exist_ok=True)
        fits_info = {}
        for name in ("raw", "prod", "A", "B", "D_frame_const"):
            if name not in stacks:
                continue
            scale = 0.1007 if name == "A" else 0.2014
            try:
                fits_info[name] = write_fits(p, stacks[name], os.path.join(a.fits, f"p5x_{name}.fits"),
                                             scale_arcsec=scale)
            except Exception as exc:
                fits_info[name] = {"error": f"{type(exc).__name__}: {exc}"}
        res["fits"] = fits_info
        with open(os.path.join(a.out, "p5x_readouts.json"), "w") as f:
            json.dump(res, f, indent=1, default=_j, sort_keys=True)
    print("written", os.path.join(a.out, "p5x_readouts.json"), "in %.1fs" % res["elapsed_s"])
    for k in ("baseline_raw", "baseline_prod", "path_A_mode_align", "path_B_seam_lsq", "path_C_sky_samples"):
        if k in res and "seam" in res[k]:
            print("  %-22s rel_median=%.4g  groups=%s" % (
                k, res[k]["seam"]["rel_median_abs"],
                {g: round(v["mean_rel"], 4) for g, v in res[k]["seam_groups"].items() if g != "all"}))


if __name__ == "__main__":
    main()
