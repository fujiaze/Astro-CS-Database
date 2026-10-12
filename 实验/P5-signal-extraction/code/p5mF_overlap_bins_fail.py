"""② 检验：把"散度"对"共同星重叠率"分箱。
重叠率定义（可复核）：rate = n_matches / n_expected，其中 n_expected = A 帧源中落在 B 帧足迹内的数量
（用 B 的 SIP 前向几何的逆：把 A 的源投到 B 像素，判是否在 [0,4095]² 内）。rate→1 表示两帧检出完备性好。
"""
import json, os, sys
import numpy as np
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location("mD", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5mD_sip_kphoto.py"))
mD = importlib.util.module_from_spec(spec)
try: spec.loader.exec_module(mD)
except SystemExit: pass
sip_wcs, unit = mD.sip_wcs, mD.unit
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
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
        wj = os.path.join(PDIR, tag, fid, "p1_wcs.json")
        if not os.path.exists(wj) or not src.get(stem): continue
        r = src[stem]; xy = np.array([[s["x"], s["y"]] for s in r])
        W = sip_wcs(wj); ra, dec = W.all_pix2world(xy[:, 0], xy[:, 1], 0)
        F.append({"fid": fid, "pt": stem.split("_M")[1][0], "xy": xy,
                  "flux": np.array([s["flux"] for s in r]), "xyz": unit(ra, dec),
                  "W": W, "kp": phot[fid]["k_photo"] if fid in phot else np.nan})
    rows = []
    for a in range(len(F)):
        for b in range(a + 1, len(F)):
            A, B = F[a], F[b]
            # A 的源投到 B 像素 → 足迹内数量
            ra, dec = A["W"].all_pix2world(A["xy"][:, 0], A["xy"][:, 1], 0)
            xb, yb = B["W"].all_world2pix(ra, dec, 0)
            inside = (xb >= 0) & (xb <= 4095) & (yb >= 0) & (yb <= 4095)
            n_exp = int(inside.sum())
            if n_exp < 30: continue
            tr = cKDTree(B["xyz"]); d, idx = tr.query(A["xyz"], k=1)
            c = np.clip(np.sum(A["xyz"] * B["xyz"][idx], axis=1), -1, 1)
            sep = np.degrees(np.arccos(c)) * 3600
            ok = np.flatnonzero(sep < 1.0)
            if ok.size < 20: continue
            rr = A["flux"][ok] / B["flux"][idx[ok]]
            med = np.median(rr); keep = (rr > med / 1.5) & (rr < med * 1.5)
            dev = np.log10(float(np.median(rr[keep])) / (A["kp"] / B["kp"]))
            rows.append((float(ok.size / n_exp), float(dev), A["pt"] == B["pt"], int(ok.size), n_exp))
    rate = np.array([r[0] for r in rows]); dev = np.array([r[1] for r in rows]); same = np.array([r[2] for r in rows])
    print("[%s] 对 %d | 重叠率分位 [10,50,90] = %s" % (tag, len(rows), np.round(np.percentile(rate, [10, 50, 90]), 3)))
    for lo, hi in ((0.0, 0.2), (0.2, 0.4), (0.4, 0.6), (0.6, 0.8), (0.8, 1.01)):
        m = (rate >= lo) & (rate < hi)
        if m.sum() == 0: continue
        dd = dev[m]
        print("   重叠率 [%.1f,%.1f)：对 %3d | 中位 %+.4f | RMS %.4f dex" % (
            lo, hi, m.sum(), np.median(dd), np.sqrt(np.mean(dd ** 2))))
    for lbl, m in (("组内", same), ("跨组", ~same)):
        if m.sum() == 0: continue
        print("   %s：对 %3d | 重叠率中位 %.3f | RMS %.4f dex" % (lbl, m.sum(), np.median(rate[m]), np.sqrt(np.mean(dev[m] ** 2))))
    res[tag] = {"n_pairs": len(rows), "rate_p10": float(np.percentile(rate, 10)), "rate_median": float(np.median(rate)),
                "rate_p90": float(np.percentile(rate, 90)),
                "bins": [{"lo": lo, "hi": hi, "n": int(((rate >= lo) & (rate < hi)).sum()),
                          "median": float(np.median(dev[(rate >= lo) & (rate < hi)])) if ((rate >= lo) & (rate < hi)).sum() else None,
                          "rms": float(np.sqrt(np.mean(dev[(rate >= lo) & (rate < hi)] ** 2))) if ((rate >= lo) & (rate < hi)).sum() else None}
                         for lo, hi in ((0.0, 0.2), (0.2, 0.4), (0.4, 0.6), (0.6, 0.8), (0.8, 1.01))],
                "corr_rate_dev": float(np.corrcoef(rate, np.abs(dev))[0, 1]) if len(rows) > 3 else None}
json.dump(res, open(f"{OUT}/p5mF_overlap_bins.json", "w"), indent=1)
print("写入 results/p5w/p5mF_overlap_bins.json")
