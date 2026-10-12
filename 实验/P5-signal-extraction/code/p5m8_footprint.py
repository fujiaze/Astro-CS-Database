"""天区 footprint 重叠几何直查（权威几何）：p1_wcs.json 的 samples → 边界多边形 → 公共网格交叠。

判据：跨组对重叠比例显著 >0（如 >5%）⇒ 上一轮"6 分量=拼接无重叠"作废。
"""
import json, os, sys
import numpy as np
from matplotlib.path import Path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location("m5", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5m5_star_ratio_sip.py"))
m5 = importlib.util.module_from_spec(spec); spec.loader.exec_module(m5)
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")

def gnom(ra0, dec0, ra, dec):
    ra, dec, ra0, dec0 = map(np.radians, (ra, dec, ra0, dec0))
    c = np.sin(dec0) * np.sin(dec) + np.cos(dec0) * np.cos(dec) * np.cos(ra - ra0)
    return np.cos(dec) * np.sin(ra - ra0) / c, (np.cos(dec0) * np.sin(dec) - np.sin(dec0) * np.cos(dec) * np.cos(ra - ra0)) / c

frames = []
for tag in ("t2", "t3"):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    for e in man["frames"]:
        fid = e["frame_id"]; stem = os.path.basename(e["input_light"]).replace(".fts", "")
        wj = os.path.join(PDIR, tag, fid, "p1_wcs.json")
        if not os.path.exists(wj): continue
        f, rms, meta = m5.fit_geom(wj)
        n = 4095.0
        bnd = []
        for t in np.linspace(0, 1, 33):          # 修正：按「下→右→上→左」遍历，简单多边形
            bnd.append((t * n, 0))
        for t in np.linspace(0, 1, 33):
            bnd.append((n, t * n))
        for t in np.linspace(1, 0, 33):
            bnd.append((t * n, n))
        for t in np.linspace(1, 0, 33):
            bnd.append((0, t * n))
        bnd = np.array(bnd)
        ra, dec = f(bnd[:, 0], bnd[:, 1])
        cx = float(np.median(ra)); cy = float(np.median(dec))
        frames.append({"tag": tag, "fid": fid, "pt": stem.split("_M")[1][0],
                       "ra": ra, "dec": dec, "c": (cx, cy), "stem": stem})
print("帧 %d（有 p1_wcs.json）" % len(frames))
ra_all = np.concatenate([f["ra"] for f in frames]); dec_all = np.concatenate([f["dec"] for f in frames])
ra0, dec0 = float(np.median(ra_all)), float(np.median(dec_all))
S = 0.005                                    # 18″/格
lim = 1.6
xs = np.arange(-lim, lim + S, S); ys = np.arange(-lim, lim + S, S)
X, Y = np.meshgrid(xs, ys)
pts = np.stack([X.ravel(), Y.ravel()], 1)
print("公共切平面网格 %d×%d，步长 %.3f°（%.0f″），中心 (%.4f, %.4f)" % (
    X.shape[1], X.shape[0], S, S * 3600, ra0, dec0))
for f in frames:
    u, v = gnom(ra0, dec0, f["ra"], f["dec"])
    u, v = np.degrees(u), np.degrees(v)      # 修正：切平面坐标统一到「度」
    f["mask"] = Path(np.stack([u, v], 1)).contains_points(pts)
    f["area"] = int(f["mask"].sum())
    uu = u[:-1]; vv = v[:-1]                    # 手性：有符号面积（>0 逆时针，<0 顺时针）
    f["signed_area"] = float(0.5 * np.sum(uu * np.roll(vv, -1) - np.roll(uu, -1) * vv))
    f["handedness"] = "CCW" if f["signed_area"] > 0 else "CW"
print("手性：%s | 各指向组手性 %s" % (
    {h: sum(1 for f in frames if f["handedness"] == h) for h in ("CCW", "CW")},
    {pt: sorted({f["handedness"] for f in frames if f["pt"] == pt}) for pt in sorted({f["pt"] for f in frames})}))
print("单帧面积中位 %d 格 ⇒ %.3f deg²（真值应≈1.21）" % (
    int(np.median([f["area"] for f in frames])),
    float(np.median([f["area"] for f in frames])) * S * S))
n = len(frames)
OV = np.zeros((n, n))
for a in range(n):
    for b in range(a + 1, n):
        inter = int(np.logical_and(frames[a]["mask"], frames[b]["mask"]).sum())
        OV[a, b] = OV[b, a] = inter / max(frames[a]["area"], 1)
same, cross = [], []
for a in range(n):
    for b in range(a + 1, n):
        (same if frames[a]["pt"] == frames[b]["pt"] else cross).append((a, b, OV[a, b]))
same = np.array([c for _, _, c in same]); cross = np.array([c for _, _, c in cross])
res = {"grid": {"step_deg": S, "n": int(X.shape[0] * X.shape[1]), "center": [ra0, dec0]},
       "frame_area_median_deg2": float(np.median([f["area"] for f in frames])) * S * S,
       "within_group": {"n_pairs": int(same.size), "median": float(np.median(same)),
                        "p10": float(np.percentile(same, 10)), "p90": float(np.percentile(same, 90))},
       "cross_group": {"n_pairs": int(cross.size), "median": float(np.median(cross)),
                       "max": float(cross.max()), "n_gt_5pct": int((cross > 0.05).sum()),
                       "n_gt_20pct": int((cross > 0.20).sum())},
       "cross_gt5_examples": [{"a": frames[a]["fid"], "b": frames[b]["fid"],
                               "a_pt": frames[a]["pt"], "b_pt": frames[b]["pt"], "overlap": float(c)}
                              for a, b, c in sorted([t for t in [(a, b, OV[a, b]) for a in range(n) for b in range(a + 1, n) if frames[a]["pt"] != frames[b]["pt"]]], key=lambda t: -t[2])[:8]],
       "overlap_matrix_top": {frames[a]["fid"]: {frames[b]["fid"]: round(float(OV[a, b]), 3)
                              for b in range(n) if OV[a, b] > 0.05 and a != b}
                              for a in range(n)},
       "frame_pointing_of_fid": {f["fid"]: f["pt"] for f in frames},
       "handedness": {f["fid"]: f["handedness"] for f in frames},
       "handedness_counts": {h: sum(1 for f in frames if f["handedness"] == h) for h in ("CCW", "CW")},
       "handedness_by_pointing": {pt: sorted({f["handedness"] for f in frames if f["pt"] == pt})
                                  for pt in sorted({f["pt"] for f in frames})},
       "signed_area_deg2": {f["fid"]: round(f["signed_area"], 4) for f in frames}}
print("组内对 %d：重叠中位 %.3f（p10 %.3f, p90 %.3f）" % (same.size, np.median(same), np.percentile(same, 10), np.percentile(same, 90)))
print("跨组对 %d：重叠中位 %.3f，最大 %.3f，>5%% 的对 %d，>20%% 的对 %d" % (
    cross.size, np.median(cross), cross.max(), int((cross > 0.05).sum()), int((cross > 0.20).sum())))
print("跨组重叠前 6 例：")
for e in res["cross_gt5_examples"][:6]:
    print("   %s(%s) × %s(%s) 重叠 %.1f%%" % (e["a"][:34], e["a_pt"], e["b"][:34], e["b_pt"], 100 * e["overlap"]))
json.dump(res, open(f"{OUT}/p5m8_footprint.json", "w"), indent=1)
print("写入", f"{OUT}/p5m8_footprint.json")
