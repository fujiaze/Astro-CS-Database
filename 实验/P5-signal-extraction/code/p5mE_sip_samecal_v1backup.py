"""SIP 几何下、同一批星对上的两种测光并列：逐星通量(p1_flux) vs 同口径孔径(本脚本现测)。
判读（写死）：同口径散度降到中位 SE 的 ~1 倍 ⇒ 元凶①（逐星通量帧间不可比）；否则转②/③。
"""
import json, os, sys
import numpy as np
from astropy.io import fits
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location("mD", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5mD_sip_kphoto.py"))
mD = importlib.util.module_from_spec(spec)
try:
    spec.loader.exec_module(mD)          # 复用 sip_wcs / unit 定义
except SystemExit:
    pass
sip_wcs, unit = mD.sip_wcs, mD.unit
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
RAP, RIN, ROUT = 4, 8, 12
yy, xx = np.mgrid[-ROUT:ROUT + 1, -ROUT:ROUT + 1]
rr = np.hypot(xx, yy); APM = rr <= RAP; ANN = (rr >= RIN) & (rr <= ROUT)
_cache = {}

def aper(img, xs, ys):
    out = np.full(xs.size, np.nan); H, W = img.shape
    for i, (x, y) in enumerate(zip(xs, ys)):
        xi, yi = int(round(x)), int(round(y))
        if xi < ROUT or yi < ROUT or xi > W - ROUT - 1 or yi > H - ROUT - 1: continue
        st = img[yi - ROUT:yi + ROUT + 1, xi - ROUT:xi + ROUT + 1]
        if not np.all(np.isfinite(st)): continue
        out[i] = np.sum(st[APM] - np.median(st[ANN]))
    return out

def load(p):
    if p not in _cache: _cache[p] = fits.getdata(p).astype(np.float64)
    return _cache[p]

NBOOT = 2000
def stats(dev, seed=0):
    if dev.size == 0: return None
    rng = np.random.default_rng(seed)
    bs = np.array([np.sqrt(np.mean(rng.choice(dev, dev.size, replace=True) ** 2)) for _ in range(NBOOT)])
    ms = np.array([np.median(rng.choice(dev, dev.size, replace=True)) for _ in range(NBOOT)])
    return {"n": int(dev.size), "median": float(np.median(dev)), "median_ci95": [float(np.percentile(ms, 2.5)), float(np.percentile(ms, 97.5))],
            "rms": float(np.sqrt(np.mean(dev ** 2))), "rms_ci95": [float(np.percentile(bs, 2.5)), float(np.percentile(bs, 97.5))],
            "se_median": float(1.4826 * np.median(np.abs(dev - np.median(dev))) / max(np.sqrt(dev.size), 1))}

res = {}
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
        if not os.path.exists(wj) or not os.path.exists(p) or not src.get(stem): continue
        r = src[stem]; xy = np.array([[s["x"], s["y"]] for s in r])
        W = sip_wcs(wj); ra, dec = W.all_pix2world(xy[:, 0], xy[:, 1], 0)
        F.append({"fid": fid, "path": p, "pt": stem.split("_M")[1][0], "xy": xy,
                  "flux": np.array([s["flux"] for s in r]), "xyz": unit(ra, dec),
                  "kp": phot[fid]["k_photo"] if fid in phot else np.nan})
    print("[%s] 帧 %d（已枚举）" % (tag, len(F)))
    dev_f_in, dev_a_in, dev_f_cr, dev_a_cr = [], [], [], []
    for a in range(len(F)):
        for b in range(a + 1, len(F)):
            same = F[a]["pt"] == F[b]["pt"]
            tr = cKDTree(F[b]["xyz"]); d, idx = tr.query(F[a]["xyz"], k=1)
            c = np.clip(np.sum(F[a]["xyz"] * F[b]["xyz"][idx], axis=1), -1, 1)
            sep = np.degrees(np.arccos(c)) * 3600
            ok = np.flatnonzero(sep < 1.0)
            if ok.size < 20: continue
            ra_ = F[a]["flux"][ok] / F[b]["flux"][idx[ok]]
            try:
                Am = aper(load(F[a]["path"]), F[a]["xy"][ok, 0], F[a]["xy"][ok, 1])
                Bm = aper(load(F[b]["path"]), F[b]["xy"][idx[ok], 0], F[b]["xy"][idx[ok], 1])
            except Exception as ex:
                print("   孔径测光失败:", type(ex).__name__, str(ex)[:60]); continue
            mm = np.isfinite(Am) & np.isfinite(Bm) & (Am > 0) & (Bm > 0)
            if mm.sum() < 20: continue
            ra_ = ra_[mm]
            ar_ = Am[mm] / Bm[mm]
            rkp = F[a]["kp"] / F[b]["kp"]
            df = np.log10(float(np.median(ra_)) / rkp)
            da = np.log10(float(np.median(ar_)) / rkp)
            if same: dev_f_in.append(df); dev_a_in.append(da)
            else: dev_f_cr.append(df); dev_a_cr.append(da)
            if same: _cache.clear()          # 控内存：仅保留当前对
    for lbl, dv in (("组内·逐星通量", dev_f_in), ("组内·同口径孔径", dev_a_in),
                    ("跨组·逐星通量", dev_f_cr), ("跨组·同口径孔径", dev_a_cr)):
        s = stats(np.array(dv))
        key = lbl
        res.setdefault(tag, {})[key] = s
        if s is None: print("   %-16s 无样本" % lbl); continue
        print("   %-16s 对 %3d | 中位 %+.4f [%.4f,%.4f] | RMS %.4f [%.4f,%.4f] | SE %.4f | RMS/SE %.1f" % (
            lbl, s["n"], s["median"], s["median_ci95"][0], s["median_ci95"][1],
            s["rms"], s["rms_ci95"][0], s["rms_ci95"][1], s["se_median"], s["rms"] / max(s["se_median"], 1e-9)))
json.dump(res, open(f"{OUT}/p5mE_sip_samecal.json", "w"), indent=1)
print("写入 results/p5w/p5mE_sip_samecal.json")
