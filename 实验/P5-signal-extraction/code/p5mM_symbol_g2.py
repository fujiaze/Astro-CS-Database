"""G1/G2/G3：把帧间乘性标度的符号约定钉死，并用多路独立估计同一件事，全部换算到 m_ab := F_a/F_b。
a = M2_T2_20251212_020002（网格右侧、电平 201.2）；b = M3_T2_20251224_042543（左侧、电平 173.8）。
"""
import json, os, sys
import numpy as np
from astropy.io import fits
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
A = {"tag": "t2", "part": "M2_T2_flying_dutchman-20251212"}
B = {"tag": "t2", "part": "M3_T2_flying_dutchman-20251224"}
def meta(fr):
    man = json.load(open(f"{PDIR}/{fr['tag']}/p1_products.json"))
    fl = json.load(open(f"{PDIR}/{fr['tag']}/p1_flux.json"))
    for e in man["frames"]:
        stem = os.path.basename(e["input_light"]).replace(".fts", "")
        if fr["part"] in stem:
            fr["fid"] = e["frame_id"]; fr["stem"] = stem
            fr["path"] = os.path.join(PDIR, fr["tag"], f"cleaned_{stem}.fts")
            fr["src"] = [r for f in fl["frames"]
                         if f["file"].replace("cleaned_", "").replace(".fts", "") == stem
                         for r in f["results"] if r.get("valid") and r["flux"] > 0]
            return True
    return False
meta(A); meta(B)
print("a = %s (%s)\nb = %s (%s)" % (A["fid"], A["tag"], B["fid"], B["tag"]))
res = {"frames": {"a": A["fid"], "b": B["fid"]}, "convention": "m_ab := median(F_a/F_b)"}
# ---- 1. 逐星比值中位（原样 / 扣本帧天光）----
kA = {s["id"]: s for s in A["src"]} if "id" in A["src"][0] else None
print("p1_flux 字段:", sorted(A["src"][0].keys()))
# 用位置匹配（不做重投影；两帧源表在各自像素域，这里用天球不可得 ⇒ 用几何：改用 p1_wcs SIP 投天球）
import importlib.util
sp = importlib.util.spec_from_file_location("mD", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5mD_sip_kphoto.py"))
mD = importlib.util.module_from_spec(sp)
try: sp.loader.exec_module(mD)
except SystemExit: pass
WA = mD.sip_wcs(os.path.join(PDIR, "t2", A["fid"], "p1_wcs.json")); WB = mD.sip_wcs(os.path.join(PDIR, "t2", B["fid"], "p1_wcs.json"))
def sky(fr, W):
    xy = np.array([[s["x"], s["y"]] for s in fr["src"]]); ra, dec = W.all_pix2world(xy[:, 0], xy[:, 1], 0)
    return xy, ra, dec
xyA, raA, decA = sky(A, WA); xyB, raB, decB = sky(B, WB)
from scipy.spatial import cKDTree
uA = np.stack([np.cos(np.radians(decA)) * np.cos(np.radians(raA)), np.cos(np.radians(decA)) * np.sin(np.radians(raA)), np.sin(np.radians(decA))], 1)
uB = np.stack([np.cos(np.radians(decB)) * np.cos(np.radians(raB)), np.cos(np.radians(decB)) * np.sin(np.radians(raB)), np.sin(np.radians(decB))], 1)
t = cKDTree(uB); d, idx = t.query(uA, k=1)
sep = np.degrees(np.arccos(np.clip(np.sum(uA * uB[idx], 1), -1, 1))) * 3600
ok = sep < 1.0
fA = np.array([s["flux"] for s in A["src"]])[ok]; fB = np.array([s["flux"] for s in B["src"]])[idx[ok]]
bgA = np.array([s.get("background", 0.0) for s in A["src"]])[ok]; bgB = np.array([s.get("background", 0.0) for s in B["src"]])[idx[ok]]
rat = fA / fB; med = float(np.median(rat)); k1 = (rat > med / 1.5) & (rat < med * 1.5)
m1 = float(np.median(rat[k1])); se1 = float(1.4826 * np.median(np.abs(rat[k1] - m1)) / np.sqrt(k1.sum()))
rb = (fA - bgA) / (fB - bgB); mb = float(np.median(rb[k1])) if np.all(fB[k1] - bgB[k1] > 0) else None
seb = float(1.4826 * np.median(np.abs(rb[k1] - np.median(rb[k1]))) / np.sqrt(k1.sum()))
print("G2-1 逐星比值：共同星 %d | m_ab(原样) = %.5f ± %.5f | m_ab(扣本帧天光) = %s ± %.5f | 天光中位 bga/bgb = %.1f/%.1f" % (
    k1.sum(), m1, se1, None if mb is None else "%.5f" % mb, seb, np.median(bgA), np.median(bgB)))
res["G2_1_star_ratio"] = {"n": int(k1.sum()), "m_raw": m1, "se_raw": se1, "m_bgsub": mb, "se_bgsub": seb,
                          "bg_a_median": float(np.median(bgA)), "bg_b_median": float(np.median(bgB))}
# ---- 2/3. 重叠区电平比与总通量比（用 stage1 格均值，无需重投影）----
cs = np.load(f"{OUT}/stage1_cleaned_cellsum.npy"); cw = np.load(f"{OUT}/stage1_cleaned_cellw.npy")
order = []
for tag in ("t2", "t3"):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    for e in man["frames"]:
        stem = os.path.basename(e["input_light"]).replace(".fts", "")
        if os.path.exists(os.path.join(PDIR, tag, f"cleaned_{stem}.fts")) and os.path.exists(os.path.join(PDIR, tag, e["frame_id"], "p1_wcs.json")):
            order.append(stem)
ia = [i for i, s in enumerate(order) if A["part"] in s]; ib = [i for i, s in enumerate(order) if B["part"] in s]
print("格数据帧序定位：a=idx%s b=idx%s（共 %d 帧）" % (ia, ib, len(order)))
lvl = {}
if ia and ib:
    ca = np.where(cw[ia[0]] > 0, cs[ia[0]] / np.maximum(cw[ia[0]], 1e-300), np.nan)
    cb = np.where(cw[ib[0]] > 0, cs[ib[0]] / np.maximum(cw[ib[0]], 1e-300), np.nan)
    m = np.isfinite(ca) & np.isfinite(cb)
    L_ab = float(np.median(ca[m]) / np.median(cb[m]))
    T_ab = float(np.nansum(cs[ia[0]][m]) / np.nansum(cs[ib[0]][m]))
    print("G2-2 重叠格电平比 L_ab = median(A)/median(B) = %.5f（共同格 %d，A %.2f / B %.2f）" % (
        L_ab, m.sum(), np.median(ca[m]), np.median(cb[m])))
    print("G2-3 重叠格总通量比 T_ab = ΣA/ΣB = %.5f" % T_ab)
    res["G2_2_level_ratio"] = {"L_ab": L_ab, "n_cells": int(m.sum()), "A": float(np.median(ca[m])), "B": float(np.median(cb[m]))}
    res["G2_3_total_flux_ratio"] = {"T_ab": T_ab}
# ---- 4. k_photo 比 ----
kp = {}
for tag in ("t2", "t3"):
    d = json.load(open(f"{PDIR}/{tag}/p1_phot.json"))["photscale_detail"]
    for k, v in d.items(): kp[k] = v["k_photo"]
KA = kp.get(A["fid"]); KB = kp.get(B["fid"])
print("G2-4 产物帧级标度比 k_a/k_b = %.5f / %.5f = %.5f" % (KA, KB, KA / KB))
res["G2_4_kphoto_ratio"] = {"k_a": KA, "k_b": KB, "ratio": KA / KB}
# ---- 5. 头 ZMAG 之差 ----
def zmag(p):
    h = fits.getheader(p)
    for k in ("ZMAG", "MAGZERO", "ZP", "MAGZP"):
        if k in h: return k, float(h[k])
    return None, None
zA, vA = zmag(A["path"]); zB, vB = zmag(B["path"])
if vA is not None and vB is not None:
    ratio = 10 ** (-0.4 * (vA - vB))
    print("G2-5 头 %s: a %.4f / b %.4f ⇒ 通量比 10^(-0.4Δ) = %.5f" % (zA, vA, vB, ratio))
    res["G2_5_zmag"] = {"key": zA, "z_a": vA, "z_b": vB, "flux_ratio": ratio}
else:
    print("G2-5 头 ZMAG 缺失（a:%s b:%s）⇒ 该项未做" % (zA, zB))
    res["G2_5_zmag"] = None
# ---- G3 判定 ----
rows = {"1_逐星(原样)": m1, "1_逐星(扣天光)": mb, "2_重叠格电平比": res.get("G2_2_level_ratio", {}).get("L_ab"),
        "3_重叠格总通量比": res.get("G2_3_total_flux_ratio", {}).get("T_ab"),
        "4_k_photo 比": KA / KB if KA and KB else None,
        "5_ZMAG 通量比": (res["G2_5_zmag"] or {}).get("flux_ratio")}
vals = [v for v in rows.values() if v]
print("\nG2 汇总（同一方向 m_ab = F_a/F_b）：")
for k, v in rows.items(): print("   %-16s %s" % (k, "未做" if v is None else "%.5f" % v))
cross1 = [v for k, v in rows.items() if not k.startswith("1") and v]
print("   跨路（2/3/4/5）中位 %.5f | 与逐星比值之比 %.3f" % (
    np.median(cross1) if cross1 else np.nan, (np.median(cross1) / m1) if cross1 else np.nan))
res["G3"] = {"table": {k: v for k, v in rows.items()}, "cross_median": float(np.median(cross1)) if cross1 else None,
             "star_over_cross": float(np.median(cross1) / m1) if cross1 else None,
             "direction_flip": bool(cross1 and (np.median(cross1) - 1) * (m1 - 1) < 0),
             "verdict": ("方向相反（跨 1）⇒ 需按可信估计量重施" if (cross1 and (np.median(cross1) - 1) * (m1 - 1) < 0)
                         else "方向一致、数值不一致 ⇒ 估计量不一致（非方向错）")}
print("   G3 判定：%s" % res["G3"]["verdict"])
json.dump(res, open(f"{OUT}/p5mM_symbol_g2.json", "w"), indent=1)
print("写入 results/p5w/p5mM_symbol_g2.json")
