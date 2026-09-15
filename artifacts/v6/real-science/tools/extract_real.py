#!/usr/bin/env python3
"""REAL-SCIENCE-001 — 真实数据抽取器 (M42 / 银心 / LDN43)

只读取冻结 testdata 中的真实 FITS 帧（只读），用真实像素构造五口径比较所需的
逐帧逐星可观测量：PSF profile P、像素方差 sigma2（由真实背景 RMS 测得）、
背景扣除后的数据 d（ADU）、曝光时间、共同星集。

不编造任何数值：所有像素/噪声/曝光均直接来自真实 FITS。
输出: run/v6/real-science/inputs/<dataset>.json

用法: python3 extract_real.py --out <dir> [--frames K]
"""
import argparse, glob, hashlib, json, os
import numpy as np
from astropy.io import fits
from astropy.wcs import WCS
from astropy.stats import sigma_clipped_stats
from scipy import ndimage

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

DATASETS = {
    "M42": {
        "dir": "testdata/M42_T2T3_mosaic_Flying_dutchman/T3/M1",
        "glob": "*-300S-Red.fts", "telescope": "T3", "object": "M42",
    },
    "M42_Halpha": {
        "dir": "testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1",
        "glob": "*-H-alpha.fts", "telescope": "T2", "object": "M42",
        "pick": [0, 1, 2, 5, 6],  # 混曝光：4x600s + 1x300s（真实不同曝光）
    },
    "Galaxy_Center": {
        "dir": "testdata/Galaxy_Center_T4/lights/panel1",
        "glob": "*-180S-Red.fts", "telescope": "T4", "object": "Galaxy_Center",
    },
    "LDN43": {
        "dir": "testdata/LDN43_T2素材_flying_dutchman/lights",
        "glob": "*Lum.fts", "telescope": "T2", "object": "LDN43",
    },
    "NGC1727": {
        "dir": "testdata/NGC1727_T2_flying_dutchman",
        "glob": "*Red.fts", "telescope": "T2", "object": "NGC1727",
    },
    "NGC247": {
        "dir": "testdata/NGC247_T2_flying_dutchman",
        "glob": "*Lum.fts", "telescope": "T2", "object": "NGC247",
    },
    "NGC55": {
        "dir": "testdata/NGC55_T3_flying_dutchman",
        "glob": "*Lum.fts", "telescope": "T3", "object": "NGC55",
    },
    "NGC83": {
        "dir": "testdata/NGC83_cluster_T3_Flying_Dutchman",
        "glob": "*Green.fts", "telescope": "T3", "object": "NGC83_cluster",
    },
    "Victory_Nebula": {
        "dir": "testdata/Victory_Nebula_T4_Flying_Dutchman",
        "glob": "*Lum.fts", "telescope": "T4", "object": "Victory_Nebula",
    },
}

STAMP = 9
ANN_IN, ANN_OUT = 8, 14
EDGE = 40
MAX_STARS = 8
MIN_STARS = 3
SAT = 60000.0


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def detect_stars(data, bg, rms, limit=400):
    sm = ndimage.gaussian_filter(data.astype(np.float64), 2.0)
    thr = bg + 25.0 * rms
    mx = ndimage.maximum_filter(sm, size=15)
    cand = np.argwhere((sm == mx) & (sm > thr))
    if cand.size == 0:
        return []
    h, w = data.shape
    keep = []
    for (y, x) in cand:
        if x < EDGE or y < EDGE or x > w - EDGE or y > h - EDGE:
            continue
        if data[y - 3:y + 4, x - 3:x + 4].max() >= SAT:
            continue
        keep.append((float(sm[y, x]), int(y), int(x)))
    keep.sort(reverse=True)
    sel = []
    for (v, y, x) in keep:
        if all((y - yy) ** 2 + (x - xx) ** 2 >= 30 ** 2 for (_, yy, xx) in sel):
            sel.append((v, y, x))
        if len(sel) >= limit:
            break
    return sel


def sample_stamp(data, xf, yf, s=STAMP):
    half = s // 2
    yy, xx = np.mgrid[-half:half + 1, -half:half + 1]
    coords = np.array([yf + yy.ravel(), xf + xx.ravel()])
    vals = ndimage.map_coordinates(data.astype(np.float64), coords, order=1,
                                   mode="constant", cval=np.nan)
    return vals.reshape(s, s)


def annulus_std(data, xf, yf):
    half = ANN_OUT
    yy, xx = np.mgrid[-half:half + 1, -half:half + 1]
    rr = np.sqrt(yy ** 2 + xx ** 2)
    m = (rr >= ANN_IN) & (rr <= ANN_OUT)
    vals = sample_stamp(data, xf, yf, 2 * half + 1)[m]
    vals = vals[np.isfinite(vals)]
    if vals.size < 20:
        return None
    med, mean, std = sigma_clipped_stats(vals, sigma=3.0, maxiters=5)
    return float(med), float(std)


def process(dataset, cfg, nframes, outdir):
    found = sorted(glob.glob(os.path.join(REPO, cfg["dir"], "**", cfg["glob"]), recursive=True))
    if "pick" in cfg:
        files = [found[i] for i in cfg["pick"] if i < len(found)]
    else:
        files = found[:nframes]
    if not files:
        raise SystemExit("no frames for " + dataset)
    print("[%s] %d frames" % (dataset, len(files)))

    frames = []
    recs = []
    for i, f in enumerate(files):
        with fits.open(f, memmap=False) as hdul:
            data = hdul[0].data.astype(np.float32)
            hdr = hdul[0].header.copy()
        frames.append((data, hdr))
        recs.append({"index": i, "path": os.path.relpath(f, REPO), "sha256": sha256_file(f),
                     "exptime": float(hdr.get("EXPTIME", 0.0)),
                     "filter": str(hdr.get("FILTER", "")), "date_obs": str(hdr.get("DATE-OBS", "")),
                     "object": str(hdr.get("OBJECT", "")), "naxis1": int(hdr["NAXIS1"]),
                     "naxis2": int(hdr["NAXIS2"])})
        print("   frame %d %s exptime=%s" % (i, os.path.basename(f), recs[-1]["exptime"]))

    ref_data, ref_hdr = frames[0]
    h, w = ref_data.shape
    cy0, cx0 = max(0, h // 2 - 1024), max(0, w // 2 - 1024)
    gmed, gmean, grms = sigma_clipped_stats(
        ref_data[cy0:cy0 + 2048, cx0:cx0 + 2048].astype(np.float64), sigma=3.0, maxiters=5)
    print("   global bg median=%.3f rms=%.3f ADU" % (gmed, grms))

    cands = detect_stars(ref_data, gmed, grms)
    print("   detected %d candidate stars" % len(cands))
    if len(cands) < MIN_STARS:
        raise SystemExit("insufficient stars in reference frame")

    wcs_ref = WCS(ref_hdr)
    star_sky = []
    for (v, y, x) in cands:
        ra, dec = wcs_ref.all_pix2world([[x, y]], 0)[0]
        star_sky.append({"peak": v, "x": int(x), "y": int(y), "ra": float(ra), "dec": float(dec)})

    frames_out = []
    for i, (data, hdr) in enumerate(frames):
        wcs = WCS(hdr)
        fstars = []
        for sidx, s in enumerate(star_sky):
            xy = wcs.all_world2pix([[s["ra"], s["dec"]]], 0)[0]
            xf, yf = float(xy[0]), float(xy[1])
            if not (EDGE <= xf <= w - EDGE and EDGE <= yf <= h - EDGE):
                continue
            st = sample_stamp(data, xf, yf)
            if not np.all(np.isfinite(st)) or st.max() >= SAT:
                continue
            ann = annulus_std(data, xf, yf)
            if ann is None:
                continue
            bg, rms = ann
            d = st - bg
            if d.sum() <= 0:
                continue
            Praw = d / d.sum()
            # 非负 PSF 估计子：裁剪负噪声样本后重归一（P>=0, Sum P=1；冻结门要求）。
            nneg = int((Praw < 0).sum())
            pmin_raw = float(Praw.min())
            P = np.clip(Praw, 0.0, None)
            P = P / P.sum()
            fstars.append({
                "star_id": "S%03d" % sidx, "ra": s["ra"], "dec": s["dec"],
                "x": xf, "y": yf, "peak": s["peak"],
                "d": [float(v) for v in d.ravel()],
                "P": [float(v) for v in P.ravel()],
                "P_raw_min": pmin_raw, "P_raw_negative_pixels": nneg,
                "sigma2": [float(rms * rms)] * (STAMP * STAMP),
                "bg": float(bg), "bg_rms": float(rms), "sum_d": float(d.sum()),
            })
        frames_out.append({"frame_index": i, "frame": recs[i],
                           "global_bg_median": float(gmed), "global_bg_rms": float(grms),
                           "n_matched": len(fstars), "stars": fstars})
        print("   frame %d: %d candidates matched" % (i, len(fstars)))

    common = None
    for fo in frames_out:
        ids = set(s["star_id"] for s in fo["stars"])
        common = ids if common is None else (common & ids)
    common = sorted(common)
    if len(common) < MIN_STARS:
        raise SystemExit("common star set < %d for %s (got %d)" % (MIN_STARS, dataset, len(common)))
    by = {fo["frame_index"]: {s["star_id"]: s for s in fo["stars"]} for fo in frames_out}
    rank = []
    for cid in common:
        snrs = [by[fo["frame_index"]][cid]["sum_d"] /
                (by[fo["frame_index"]][cid]["bg_rms"] * np.sqrt(STAMP * STAMP))
                for fo in frames_out]
        rank.append((min(snrs), cid))
    rank.sort(reverse=True)
    selected = sorted(cid for _, cid in rank[:MAX_STARS])
    print("   common=%d selected=%d: %s" % (len(common), len(selected), selected))

    out = {
        "dataset": dataset, "telescope": cfg["telescope"], "object": cfg["object"],
        "stamp_size": STAMP, "annulus_px": [ANN_IN, ANN_OUT], "edge_px": EDGE,
        "global_bg_median": float(gmed), "global_bg_rms": float(grms),
        "n_candidates_detected": len(cands), "n_common_all_frames": len(common),
        "n_common_stars": len(selected), "common_star_ids": selected,
        "selection_rule": "valid (in-bounds, unsaturated, positive flux) in ALL frames; "
                          "ranked by min over frames of aperture SNR; top %d kept" % MAX_STARS,
        "psf_estimator": "empirical stamp profile P=d/Sum(d), non-negative clipped (P>=0) and "
                         "renormalized to Sum P=1; per-star raw min/negative counts retained",
        "frames": [],
    }
    for fo in frames_out:
        ordered = [by[fo["frame_index"]][c] for c in selected]
        out["frames"].append({"frame_index": fo["frame_index"], "frame": fo["frame"],
                              "global_bg_median": fo["global_bg_median"],
                              "global_bg_rms": fo["global_bg_rms"], "stars": ordered})
    os.makedirs(outdir, exist_ok=True)
    dest = os.path.join(outdir, dataset + ".json")
    with open(dest, "w") as f:
        json.dump(out, f)
    print("[%s] wrote %s (%d bytes)" % (dataset, dest, os.path.getsize(dest)))
    return dest


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--frames", type=int, default=5)
    ap.add_argument("--datasets", default=",".join(DATASETS.keys()))
    a = ap.parse_args()
    for name in a.datasets.split(","):
        process(name, DATASETS[name], a.frames, a.out)


if __name__ == "__main__":
    main()
