"""跨组系统偏移扫描：对跨组帧对（含组内对照），统计 (Δra·cosδ, Δdec) 偏移直方图，
找 1″ 内匹配数的峰值偏移；峰值明显非零 ⇒ 坐实"组相关系统偏移"。
"""
import json, os, sys
import numpy as np
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location("m5", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5m5_star_ratio_sip.py"))
m5 = importlib.util.module_from_spec(spec); spec.loader.exec_module(m5)
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
R = 60.0 / 3600.0            # 搜索半径 60″
BIN = 1.0 / 3600.0           # 直方图 1″ 格

F = {}
for tag in ("t2", "t3"):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
    src = {fr["file"].replace("cleaned_", "").replace(".fts", ""):
           [r for r in fr["results"] if r.get("valid") and r["flux"] > 0] for fr in fl["frames"]}
    for e in man["frames"]:
        fid = e["frame_id"]; stem = os.path.basename(e["input_light"]).replace(".fts", "")
        wj = os.path.join(PDIR, tag, fid, "p1_wcs.json")
        if not os.path.exists(wj) or not src.get(stem): continue
        f, rms, _ = m5.fit_geom(wj)
        xy = np.array([[s["x"], s["y"]] for s in src[stem]])
        ra, dec = f(xy[:, 0], xy[:, 1])
        F[fid] = {"ra": ra, "dec": dec, "pt": stem.split("_M")[1][0], "tag": tag,
                  "flux": np.array([s["flux"] for s in src[stem]])}

def scan(fa, fb):
    A, B = F[fa], F[fb]
    ra0 = float(np.median(A["ra"])); dec0 = float(np.median(A["dec"]))
    ua = (A["ra"] - ra0) * np.cos(np.radians(A["dec"])); va = A["dec"] - dec0
    ub = (B["ra"] - ra0) * np.cos(np.radians(B["dec"])); vb = B["dec"] - dec0
    tb = cKDTree(np.stack([ub, vb], 1))
    idx = tb.query_ball_point(np.stack([ua, va], 1), r=R)
    nb = 2001
    H = np.zeros((nb, nb), dtype=np.int32)
    for i, js in enumerate(idx):
        for j in js:
            di = int(round((ub[j] - ua[i]) / BIN)) + nb // 2
            dj = int(round((vb[j] - va[i]) / BIN)) + nb // 2
            if 0 <= di < nb and 0 <= dj < nb:
                H[di, dj] += 1
    k = np.unravel_index(np.argmax(H), H.shape)
    peak = (float((k[0] - nb // 2) * BIN), float((k[1] - nb // 2) * BIN))
    zero = int(H[nb // 2, nb // 2])
    # 峰附近 1″ 内的总匹配数
    nm = int(H[max(0, k[0] - 0):k[0] + 1, k[1]:k[1] + 1].sum())
    return {"peak_offset_arcsec": [round(peak[0] * 3600, 1), round(peak[1] * 3600, 1)],
            "peak_count": int(H[k]), "count_at_zero": zero,
            "n_sources_a": int(A["ra"].size), "n_sources_b": int(B["ra"].size)}

cases = [("M42_M2_T2_flying_dutchman-20251212_012404-300S-Red", "M42_M3_T2_flying_dutchman-20251224_042543-300S-Red", "跨组 15%"),
         ("M42_M2_T2_flying_dutchman-20251212_012404-300S-Red", "M42_M3_T2_flying_dutchman-20251216_043245-300S-Red", "跨组"),
         ("M42_M5_T2_flying_dutchman-20260108_030531-300S-Red", "M42_M6_T2_flying_dutchman-20251224_061438-300S-Red", "跨组"),
         ("M42_M1_T2_flying_dutchman-20251212_012404-300S-Red", "M42_M4_T2_flying_dutchman-20251216_050117-300S-Red", "跨组"),
         ("M42_M1_T2_flying_dutchman-20251212_012404-300S-Red", "M42_M1_T2_flying_dutchman-20251224_031410-300S-Red", "组内对照"),
         ("M42_M3_T2_flying_dutchman-20251216_043245-300S-Red", "M42_M3_T2_flying_dutchman-20251224_042543-300S-Red", "组内对照")]
res = {}
print("帧表 %d 帧" % len(F))
for a, b, lbl in cases:
    a2 = a if a in F else a + ".fts"
    b2 = b if b in F else b + ".fts"
    ka = [k for k in F if k.startswith(a[:38])]
    kb = [k for k in F if k.startswith(b[:38])]
    if not ka or not kb:
        print("  [%s] 帧不在表内: %s / %s" % (lbl, a[:30], b[:30])); continue
    r = scan(ka[0], kb[0]); r["kind"] = lbl; r["frames"] = [ka[0], kb[0]]
    res[f"{ka[0][:30]}|{kb[0][:30]}"] = r
    print("  [%s] %s × %s" % (lbl, ka[0][:30], kb[0][:30]))
    print("      峰值偏移 (Δra·cosδ, Δdec) = %s ″，峰计数 %d | **零偏移处计数 %d** | 源数 %d/%d" % (
        r["peak_offset_arcsec"], r["peak_count"], r["count_at_zero"], r["n_sources_a"], r["n_sources_b"]))
json.dump(res, open(f"{OUT}/p5m9_offset_scan.json", "w"), indent=1)
print("写入", f"{OUT}/p5m9_offset_scan.json")
