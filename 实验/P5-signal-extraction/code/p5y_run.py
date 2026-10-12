"""P5 信号提取（网格合规版）驱动：生产约定的平面网格上出成品并重测全部判据。

  python3 p5y_run.py --root <产物目录> --out <结果目录> --fits <FITS 目录> [--thumbs <目录>]
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time

import numpy as np
from astropy.io import fits

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p5x_io as io            # noqa: E402
import p5x_metrics as M        # noqa: E402
import p5x_paths as P          # noqa: E402
import p5y_plane as G          # noqa: E402

DEF_PROD = "/workspace/Astro CS Database/run/LAMBDA-ROLLBACK/base/mosaic"
REF_FITS = "/workspace/Astro CS Database/run/E2E-M42-P5-EXPORT/out/full_p3_whole/output_phase3.fits"


def _j(x):
    if isinstance(x, np.floating):
        return float(x)
    if isinstance(x, np.integer):
        return int(x)
    if isinstance(x, np.ndarray):
        return x.tolist()
    raise TypeError(str(type(x)))


def write_fits(img, w, path, bunit="ADU"):
    h = w.to_header()
    h["BUNIT"] = (bunit, "same normalized unit as the input product")
    h["P5PATH"] = (os.path.basename(path)[:60], "P5 signal extraction path")
    fits.PrimaryHDU(data=img.astype(np.float32), header=h).writeto(path, overwrite=True)
    return {"file": path, "shape": [int(img.shape[1]), int(img.shape[0])],
            "nan_frac": float(np.mean(~np.isfinite(img))),
            "cdelt_deg": abs(float(h["CDELT1"])) if "CDELT1" in h else abs(float(h["CD1_1"])),
            "ctype": [str(h["CTYPE1"]), str(h["CTYPE2"])],
            "crval": [float(h["CRVAL1"]), float(h["CRVAL2"])],
            "crpix": [float(h["CRPIX1"]), float(h["CRPIX2"])]}


def thumb(img, path, title, pct=(1, 99.5)):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    a = np.array(img, dtype=float)
    fin = np.isfinite(a)
    v0, v1 = np.nanpercentile(a[fin], pct) if fin.any() else (0, 1)
    fig, ax = plt.subplots(figsize=(5.2, 5.2), dpi=110)
    ax.imshow(a, origin="lower", cmap="gray", vmin=v0, vmax=v1,
              interpolation="nearest", extent=(0, a.shape[1], 0, a.shape[0]))
    ax.set_title(title, fontsize=9)
    ax.set_xlabel("x [px, 1.8\"/px]"); ax.set_ylabel("y [px]")
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=DEF_PROD)
    ap.add_argument("--out", required=True)
    ap.add_argument("--fits", required=True)
    ap.add_argument("--thumbs", default=None)
    ap.add_argument("--ref", default=REF_FITS)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.fits, exist_ok=True)
    if a.thumbs:
        os.makedirs(a.thumbs, exist_ok=True)
    t0 = time.time()
    res = {}

    # ---------------- 生产参照网格
    ref = fits.open(a.ref)
    refd = ref[0].data
    refh = ref[0].header
    res["reference_product"] = {
        "file": a.ref, "shape": [int(refh["NAXIS1"]), int(refh["NAXIS2"])],
        "nan_frac": float(np.mean(~np.isfinite(refd))),
        "crval": [float(refh["CRVAL1"]), float(refh["CRVAL2"])],
        "crpix": [float(refh["CRPIX1"]), float(refh["CRPIX2"])],
        "cd": [float(refh["CD1_1"]), float(refh["CD1_2"]), float(refh["CD2_1"]), float(refh["CD2_2"])],
        "cdelt_arcsec": abs(float(refh["CD1_1"])) * 3600.0,
        "ctype": [str(refh["CTYPE1"]), str(refh["CTYPE2"])],
        "bunit": str(refh.get("BUNIT")), "sampler": str(refh.get("SAMPLER"))}

    # ---------------- 输入与网格
    p = io.load(a.root)
    res["input"] = {"root": a.root, "n_frames": p.nframe, "frame_ids": p.frame_ids,
                    "n_union_leaf_pixels": int(p.npix), "leaf_nside": io.LEAF_NSIDE,
                    "leaf_scale_arcsec": 0.1007,
                    "input_hips_paths": json.load(open(os.path.join(a.root, "p2_coverage.json"))).get("hips_paths"),
                    "input_manifest_hash": p.samples.get("input_manifest_hash"),
                    "sky_center_deg": [p.sp["ra0_deg"], p.sp["dec0_deg"]],
                    "coverage_hist": {str(int(k)): int(v) for k, v in
                                      zip(*np.unique(p.ncover, return_counts=True))}}
    w, nx, ny = G.make_grid(p, margin=8)
    res["output_grid"] = {"nx": nx, "ny": ny, "cd_deg": [[-G.CD_DEG, 0.0], [0.0, G.CD_DEG]],
                          "cdelt_arcsec": G.CD_DEG * 3600.0,
                          "crval": [p.sp["ra0_deg"], p.sp["dec0_deg"]],
                          "crpix": [float(w.wcs.crpix[0]), float(w.wcs.crpix[1])],
                          "ctype": ["RA---TAN", "DEC--TAN"], "sampler": "area_bin_from_leaf"}

    # ---------------- 各路径的逐帧修正（在叶像素原生网格上算）
    corrA, infoA = P.path_A_mode_align(p)
    edges_leaf = M.seam_edges(p)
    corrB, infoB = P.path_B_seam_lsq(p, edges_leaf[0], edges_leaf[1])
    try:
        struct = M.structure_mask(p, M.stack_weighted(p.raw, p)[0])
        corrC, infoC = P.path_C_sky_samples(p, struct)
    except Exception as exc:
        corrC, infoC = None, {"error": f"{type(exc).__name__}: {exc}"}

    variants = {"raw": p.raw, "prod": p.corrected, "A": corrA, "B": corrB}
    if corrC is not None:
        variants["C"] = corrC

    # ---------------- 面元重采样 + 判据
    out = {}
    edge_cache = None
    for name, arr in variants.items():
        t1 = time.time()
        r = G.rebin_sums(p, w, nx, ny, arr)
        img = r["stack"].reshape(ny, nx)
        ncov = r["ncover"].reshape(ny, nx)
        bits = r["bits"].reshape(ny, nx)
        if edge_cache is None:
            edge_cache = G.plane_seam_edges(bits, ncov)
        p_, q_ = edge_cache
        bg = G.plane_bg(img)
        st = G.plane_seam(img, p_, q_, bg)
        rec = {"seam": G.plane_summary(st), "seam_groups": G.plane_seam_groups(bits, st),
               "consistency": G.plane_consistency(r["frames"], r["q1"], r["q2"], ncov),
               "n_covered_px": int(np.sum(ncov >= 1)), "nan_frac": float(np.mean(ncov == 0)),
               "build_s": time.time() - t1}
        out[name] = rec
        out[name]["_stack"] = img
        if name == "prod":
            out["_prod_img"] = img
        if name == "raw":
            out["_raw_img"] = img

    # 相对基准（raw 与 prod）的读数
    for name in [k for k in variants]:
        img = out[name]["_stack"]
        bg = G.plane_bg(img)
        p_, q_ = edge_cache
        st = G.plane_seam(img, p_, q_, bg)
        out[name]["_st"] = st
    raw_st = out["raw"]["_st"]
    prod_st = out["prod"]["_st"]
    for name in [k for k in variants]:
        rec = out[name]
        rec["edges_vs_raw"] = G.plane_compare(raw_st, rec["_st"])
        rec["edges_vs_prod"] = G.plane_compare(prod_st, rec["_st"])
        rec["structure_vs_raw"] = G.plane_structure(rec["_stack"], out["raw"]["_stack"], ncov)
        del rec["_st"]
    del out["_raw_img"], out["_prod_img"]

    # 逐帧系统电平（有符号平均，平面网格）
    res["paths"] = {k: {kk: vv for kk, vv in v.items() if kk != "_stack"} for k, v in out.items()}
    res["path_info"] = {"A": infoA, "B": infoB, "C": infoC}

    # ---------------- FITS + 缩略图
    fits_info = {}
    for name in variants:
        img = out[name]["_stack"]
        fits_info[name] = write_fits(img, w, os.path.join(a.fits, f"p5y_{name}.fits"))
    res["fits"] = fits_info
    if a.thumbs:
        th = {}
        th["reference_production"] = thumb(refd, os.path.join(a.thumbs, "ref_production.png"),
                                           "production export (M42 field)\nNaN %.1f%%" % (100 * res["reference_product"]["nan_frac"]))
        for name in variants:
            th[name] = thumb(out[name]["_stack"], os.path.join(a.thumbs, f"p5y_{name}.png"),
                             f"p5y {name}  NaN {100*out[name]['nan_frac']:.1f}%")
        res["thumbs"] = th

    res["elapsed_s"] = time.time() - t0
    with open(os.path.join(a.out, "p5y_readouts.json"), "w") as f:
        json.dump(res, f, indent=1, default=_j, sort_keys=True)
    print("grid %dx%d  cdelt=%.4f\"  %s" % (nx, ny, G.CD_DEG * 3600,
                                            "%.1f%% NaN" % (100 * np.mean(out["prod"]["nan_frac"] == 1))))
    for name in variants:
        r = res["paths"][name]
        print("  %-6s NaN=%5.1f%%  接缝 rel 中位=%.4g  逐帧系统电平 %s" % (
            name, 100 * r["nan_frac"], r["seam"]["rel_median_abs"],
            {g: round(v["mean_rel"], 4) for g, v in r["seam_groups"].items() if g != "all"}))
    print("written", os.path.join(a.out, "p5y_readouts.json"), "in %.1fs" % res["elapsed_s"])


if __name__ == "__main__":
    main()
