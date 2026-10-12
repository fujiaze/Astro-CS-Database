"""同口径测光交叉验证：在共同星位置上用**同一孔径/环带**逐帧测光，重做组内比值对 k_photo 比。

与 §14 的差别：不再用 p1_flux.json 的逐帧 PSF 通量（口径/PSF 不一致），而是在帧影像上
按各帧自身像素做**同一物理口径**（r=4 px ≈ 3.9″，环带 8–12 px 减天光）的孔径测光。
验收：log10(星比比 / k_photo比) 的 RMS 应落到 ≲0.02 dex。
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
RAP, RIN, ROUT = 4, 8, 12
yy, xx = np.mgrid[-ROUT:ROUT + 1, -ROUT:ROUT + 1]
rr = np.hypot(xx, yy)
AP = rr <= RAP
ANN = (rr >= RIN) & (rr <= ROUT)
_cache = {}

def aper(img, xs, ys):
    out = np.full(xs.size, np.nan)
    H, W = img.shape
    for i, (x, y) in enumerate(zip(xs, ys)):
        xi, yi = int(round(x)), int(round(y))
        if xi < ROUT or yi < ROUT or xi > W - ROUT - 1 or yi > H - ROUT - 1:
            continue
        st = img[yi - ROUT:yi + ROUT + 1, xi - ROUT:xi + ROUT + 1]
        if not np.all(np.isfinite(st)):
            continue
        sky = np.median(st[ANN]); out[i] = np.sum(st[AP] - sky)
    return out

def load(path):
    if path not in _cache:
        _cache[path] = fits.getdata(path).astype(np.float64)
    return _cache[path]

res = {"aperture": {"r_ap_px": RAP, "r_in_px": RIN, "r_out_px": ROUT, "arcsec_per_px": 0.967}}
for tag in ("t2", "t3"):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    phot = json.load(open(f"{PDIR}/{tag}/p1_phot.json"))["photscale_detail"]
    fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
    src = {fr["file"].replace("cleaned_", "").replace(".fts", ""):
           [r for r in fr["results"] if r.get("valid") and r["flux"] > 0] for fr in fl["frames"]}
    F = []
    for e in man["frames"]:
        fid = e["frame_id"]; stem = os.path.basename(e["input_light"]).replace(".fts", "")
        wj = os.path.join(PDIR, tag, fid, "p1_wcs.json"); p = os.path.join(PDIR, tag, f"cleaned_{stem}.fts")
        if not os.path.exists(wj) or not src.get(stem) or not os.path.exists(p): continue
        f, _, _ = m5.fit_geom(wj)
        xy = np.array([[s["x"], s["y"]] for s in src[stem]])
        ra, dec = f(xy[:, 0], xy[:, 1])
        F.append({"fid": fid, "path": p, "xy": xy, "xyz": m5.unit(ra, dec),
                  "kp": phot[fid]["k_photo"], "pt": stem.split("_M")[1][0]})
    devs = []; det = []
    for a in range(len(F)):
        for b in range(a + 1, len(F)):
            if F[a]["pt"] != F[b]["pt"]: continue
            tr = cKDTree(F[b]["xyz"]); d, idx = tr.query(F[a]["xyz"], k=1)
            c = np.clip(np.sum(F[a]["xyz"] * F[b]["xyz"][idx], axis=1), -1, 1)
            sep = np.degrees(np.arccos(c)) * 3600
            ok = np.flatnonzero(sep < 1.0)
            if ok.size < 50: continue
            ia, ib = load(F[a]["path"]), load(F[b]["path"])
            fa = aper(ia, F[a]["xy"][ok, 0], F[a]["xy"][ok, 1])
            fb = aper(ib, F[b]["xy"][idx[ok], 0], F[b]["xy"][idx[ok], 1])
            m = np.isfinite(fa) & np.isfinite(fb) & (fa > 0) & (fb > 0)
            if m.sum() < 30: continue
            ratio = fa[m] / fb[m]
            med = np.median(ratio); keep = (ratio > med / 1.5) & (ratio < med * 1.5)
            r_star = float(np.median(ratio[keep]))
            r_kp = F[a]["kp"] / F[b]["kp"]
            devs.append(np.log10(r_star / r_kp))
            det.append({"a": F[a]["fid"], "b": F[b]["fid"], "n": int(m.sum()),
                        "n_kept": int(keep.sum()), "star_ratio": r_star, "kphoto_ratio": float(r_kp),
                        "dev_dex": float(np.log10(r_star / r_kp))})
            del ia, ib
    _cache.clear()
    devs = np.array(devs)
    res[tag] = {"n_pairs": int(devs.size), "median_dex": float(np.median(devs)) if devs.size else None,
                "rms_dex": float(np.sqrt(np.mean(devs ** 2))) if devs.size else None,
                "p16": float(np.percentile(devs, 16)) if devs.size else None,
                "p84": float(np.percentile(devs, 84)) if devs.size else None,
                "pass_0p02": bool(devs.size and np.sqrt(np.mean(devs ** 2)) <= 0.02),
                "pairs": det}
    print("[%s] 同口径测光：对 %d | 中位偏差 %+.5f dex | RMS %.5f dex | p16/p84 %+.5f/%+.5f | 验收(≤0.02) %s" % (
        tag, devs.size, np.median(devs) if devs.size else float("nan"),
        np.sqrt(np.mean(devs ** 2)) if devs.size else float("nan"),
        np.percentile(devs, 16) if devs.size else float("nan"),
        np.percentile(devs, 84) if devs.size else float("nan"),
        "通过" if (devs.size and np.sqrt(np.mean(devs ** 2)) <= 0.02) else "**未通过**"))
json.dump(res, open(f"{OUT}/p5m7_samecal_phot.json", "w"), indent=1)
print("写入", f"{OUT}/p5m7_samecal_phot.json")
