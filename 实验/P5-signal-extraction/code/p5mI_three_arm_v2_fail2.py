"""接缝三臂 v2：四步修法（SIP AP/BP 或牛顿迭代求逆；关键字实参；S1/S2 硬自检）。
判据只读导入 sci_c_common.seam_steps。
"""
import json, os, sys, importlib.util
import numpy as np
from astropy.io import fits
from matplotlib.path import Path
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
spec = importlib.util.spec_from_file_location("mD", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5mD_sip_kphoto.py"))
mD = importlib.util.module_from_spec(spec)
try: spec.loader.exec_module(mD)
except SystemExit: pass
sip_wcs = mD.sip_wcs
ADD = "/workspace/Astro CS Database/实验/additive-sky-seamless/code"
sp2 = importlib.util.spec_from_file_location("scc", os.path.join(ADD, "sci_c_common.py"))
scc = importlib.util.module_from_spec(sp2); sp2.loader.exec_module(scc)
seam_steps = scc.seam_steps
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
PIX = 0.967; DEG = PIX / 3600.0; G = 2048

def fmeta(tag, part):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
    src = {fr["file"].replace("cleaned_", "").replace(".fts", ""):
           [r for r in fr["results"] if r.get("valid") and r["flux"] > 0] for fr in fl["frames"]}
    for e in man["frames"]:
        stem = os.path.basename(e["input_light"]).replace(".fts", "")
        if part in stem:
            return e["frame_id"], os.path.join(PDIR, tag, f"cleaned_{stem}.fts"), src.get(stem)
    return None, None, None

def samp(img, gx, gy):
    ix = np.floor(gx); iy = np.floor(gy)
    ok = (ix >= 0) & (ix < img.shape[1] - 1) & (iy >= 0) & (iy < img.shape[0] - 1)
    ix = np.clip(ix, 0, img.shape[1] - 2).astype(np.int64); iy = np.clip(iy, 0, img.shape[0] - 2).astype(np.int64)
    fx = gx - ix; fy = gy - iy
    v = ((1 - fx) * (1 - fy) * img[iy, ix] + fx * (1 - fy) * img[iy, ix + 1] +
         (1 - fx) * fy * img[iy + 1, ix] + fx * fy * img[iy + 1, ix + 1])
    return np.where(ok & np.isfinite(v), v, np.nan)

fA, pA, srcA = fmeta("t2", "M2_T2_flying_dutchman-20251212")
fB, pB, srcB = fmeta("t2", "M3_T2_flying_dutchman-20251224")
WA = sip_wcs(os.path.join(PDIR, "t2", fA, "p1_wcs.json")); WB = sip_wcs(os.path.join(PDIR, "t2", fB, "p1_wcs.json"))
imgA = fits.getdata(pA).astype(np.float64); imgB = fits.getdata(pB).astype(np.float64)
n = 4095.0; bnd = []
for t in np.linspace(0, 1, 65): bnd.append((t * n, 0))
for t in np.linspace(0, 1, 65): bnd.append((n, t * n))
for t in np.linspace(1, 0, 65): bnd.append((t * n, n))
for t in np.linspace(1, 0, 65): bnd.append((0, t * n))
bnd = np.array(bnd)
rb, db = WA.all_pix2world(bnd[:, 0], bnd[:, 1], 0); rc, dc = WB.all_pix2world(bnd[:, 0], bnd[:, 1], 0)
uA, vA = float(np.median(rb)), float(np.median(db)); uB, vB = float(np.median(rc)), float(np.median(dc))
ra0, dec0 = (uA + uB) / 2.0, (vA + vB) / 2.0
g = (np.arange(G) - G / 2) * DEG
UU, VV = np.meshgrid(g, g)
RA = ra0 + UU / np.cos(np.radians(dec0)); DEC = dec0 + VV
# S1：掩膜用「帧边界多边形」，求逆用 astropy（含 SIP 前向的迭代），quiet 抑制框外不收敛
def inside(W, ra_poly, dec_poly, ra, dec):
    poly = Path(np.stack([(ra_poly - ra0) / DEG, (dec_poly - dec0) / DEG], 1))
    return poly.contains_points(np.stack([(ra - ra0) / DEG, (dec - dec0) / DEG], 1))
mA = inside(WA, rb, db, RA.ravel(), DEC.ravel()).reshape(G, G)
mB = inside(WB, rc, dc, RA.ravel(), DEC.ravel()).reshape(G, G)
un = mA | mB; inter = mA & mB
cov_union = float(un.mean()); ratio = float(inter.sum() / max(un.sum(), 1))
print("S1 掩膜自检：并集覆盖 %.3f | 交集/并集 %.3f | A %d B %d" % (cov_union, ratio, mA.sum(), mB.sum()))
# S1 判据（改正）：网格横跨两帧 ⇒ 并集必然接近 1，故判「非退化」= 各帧自身未铺满 + 交集占比在 15%–40%
S1 = {"union_coverage": cov_union, "inter_over_union": ratio,
      "A_frac": float(mA.mean()), "B_frac": float(mB.mean()),
      "nondegenerate": bool(mA.mean() < 0.999 and mB.mean() < 0.999 and 0.10 <= ratio <= 0.45)}
if not S1["nondegenerate"]:
    print("*** S1 失败：掩膜全 True ⇒ 判构造失败，停止 ***"); sys.exit(0)
def g2p(W, ra, dec):
    try:
        return W.all_world2pix(ra.ravel(), dec.ravel(), 0, quiet=True)
    except Exception as e:
        print("  求逆异常:", type(e).__name__, str(e)[:80]); return np.full(ra.size, np.nan), np.full(ra.size, np.nan)
gxA, gyA = g2p(WA, RA, DEC); gxB, gyB = g2p(WB, RA, DEC)
PA = samp(imgA, gxA.reshape(G, G), gyA.reshape(G, G)); PB = samp(imgB, gxB.reshape(G, G), gyB.reshape(G, G))
print("   采样有效：A %.3f | B %.3f（网格内）" % (np.mean(np.isfinite(PA)), np.mean(np.isfinite(PB))))
mA = mA & np.isfinite(PA); mB = mB & np.isfinite(PB)
print("   掩膜∩有效：A %d | B %d | 交 %d" % (mA.sum(), mB.sum(), (mA & mB).sum()))
# 交叉标定：重叠区乘性因子（加权比值中位）+ 加性偏置
ov = mA & mB
xgiA = (np.array([]),)
def srcmatch():
    xyA = np.array([[s["x"], s["y"]] for s in srcA]); xyB = np.array([[s["x"], s["y"]] for s in srcB])
    raA, decA = WA.all_pix2world(xyA[:, 0], xyA[:, 1], 0); raB, decB = WB.all_pix2world(xyB[:, 0], xyB[:, 1], 0)
    gA = np.stack([(raA - ra0) / DEG + G / 2, (decA - dec0) / DEG + G / 2], 1)
    gB = np.stack([(raB - ra0) / DEG + G / 2, (decB - dec0) / DEG + G / 2], 1)
    t = cKDTree(gB); d, idx = t.query(gA, k=1)
    s = (d < 1.5) & (gA[:, 0] >= 0) & (gA[:, 0] < G) & (gA[:, 1] >= 0) & (gA[:, 1] < G)
    return gA[s], np.array([x["flux"] for x in srcA])[s], idx[s], np.array([x["flux"] for x in srcB])[idx[s]]
gA, fA_f, idxm, fB_f = srcmatch()
rat = fA_f / fB_f; med = float(np.median(rat)); k = (rat > med / 1.5) & (rat < med * 1.5)
mfac = float(np.median(rat[k])); se = float(1.4826 * np.median(np.abs(rat[k] - mfac)) / np.sqrt(k.sum()))
lvA = float(np.nanmedian(PA[ov])); lvB = float(np.nanmedian(PB[ov])); dOff = lvA - lvB
print("   共同星 %d | m = %.5f (SE %.5f, 3σ %.5f) | 电平 A %.1f B %.1f Δ %.1f" % (k.sum(), mfac, se, 3 * se, lvA, lvB, dOff))
seams = [int(G * f) for f in (0.40, 0.45, 0.50, 0.55, 0.60)]
def build(x_seam, mB_fac, dOff_v, inj=None):
    PBc = PB * mB_fac + dOff_v
    if inj is not None: PBc = PBc * inj
    out = np.where(mA, PA, np.nan)
    right = mB & (np.arange(G)[None, :] > x_seam)
    return np.where(right & np.isfinite(PBc), PBc, out)
def measure(mos):
    st = seam_steps(mos, boundaries=seams, halfwin=8, base_win=64, order=2, min_valid=8, offlocus=True)
    rel = np.array([s["rel"] for s in st]); cov = np.array([float(np.mean(np.isfinite(mos[:, s["x"]]))) for s in st])
    ok = np.isfinite(rel) & (cov > 0.5)
    return {"rel": [None if not np.isfinite(v) else float(v) for v in rel], "coverage": cov.tolist(),
            "measurable": int(ok.sum()), "rel_median": float(np.median(rel[ok])) if ok.any() else None,
            "rel_abs_median": float(np.median(np.abs(rel[ok]))) if ok.any() else None,
            "rel_abs_p16": float(np.percentile(np.abs(rel[ok]), 16)) if ok.any() else None,
            "rel_abs_p84": float(np.percentile(np.abs(rel[ok]), 84)) if ok.any() else None}
# S2：注入自检
mos0 = build(seams[2], mB_fac=mfac, dOff_v=dOff); r0 = measure(mos0)
mosI = build(seams[2], mB_fac=mfac, dOff_v=dOff, inj=1.56); rI = measure(mosI)
print("S2 注入自检：无注入 |rel| 中位 %s | 注入 1.56× 后 %s（应 ≈0.359）" % (r0["rel_abs_median"], rI["rel_abs_median"]))
S2 = {"rel_no_inj": r0["rel_abs_median"], "rel_inj156": rI["rel_abs_median"],
      "expected": 0.359, "pass": bool(rI["rel_abs_median"] is not None and abs(rI["rel_abs_median"] - 0.359) < 0.15)}
print("   S2 判定：%s" % ("通过" if S2["pass"] else "**未通过**"))
res = {"frames": [fA, fB], "G": G, "pix_arcsec": PIX, "S1": S1, "S2": S2,
       "m": mfac, "m_se": se, "three_sigma": 3 * se, "level_offset": dOff, "n_stars": int(k.sum())}
if S2["pass"]:
    arms = {}
    for nm, kw in {"additive_only": dict(mB_fac=1.0, dOff_v=dOff),
                   "multiplicative_only": dict(mB_fac=mfac, dOff_v=0.0),
                   "both": dict(mB_fac=mfac, dOff_v=dOff),
                   "inj1.56_additive_only": dict(mB_fac=1.0, dOff_v=dOff, inj=1.56),
                   "inj1.56_multiplicative_only": dict(mB_fac=mfac, dOff_v=0.0, inj=1.56),
                   "inj1.56_both": dict(mB_fac=mfac, dOff_v=dOff, inj=1.56),
                   "inj1.03_additive_only": dict(mB_fac=1.0, dOff_v=dOff, inj=1.03),
                   "inj1.03_multiplicative_only": dict(mB_fac=mfac, dOff_v=0.0, inj=1.03),
                   "inj1.03_both": dict(mB_fac=mfac, dOff_v=dOff, inj=1.03)}.items():
        rr = measure(build(seams[2], **kw)); arms[nm] = rr
        print("   %-28s |rel|中位 %s | 可测边 %d/5 | 覆盖 %s" % (
            nm, None if rr["rel_abs_median"] is None else "%.5f" % rr["rel_abs_median"], rr["measurable"],
            np.round(rr["coverage"], 2).tolist()))
    res["arms"] = arms
    # 单标量残余 + 阶数扫描
    if gA.size:
        u = (gA[:, 0] - G / 2) * PIX; v = (gA[:, 1] - G / 2) * PIX
        inov = (gA[:, 0] >= 0) & (gA[:, 0] < G) & (gA[:, 1] >= 0) & (gA[:, 1] < G)
        lr = np.log10(fA_f / fB_f)
        res["order_scan"] = {}
        for order in (0, 1, 2, 3):
            cols = []
            for i in range(order + 1):
                for j in range(order + 1 - i):
                    cols.append((u ** i) * (v ** j))
            Bm = np.stack(cols, 1)
            coef, *_ = np.linalg.lstsq(Bm, lr, rcond=None)
            rr = lr - Bm @ coef
            res["order_scan"][order] = {"n_coef": len(cols), "resid_rms_dex": float(np.sqrt(np.mean(rr ** 2))),
                                        "resid_rms_frac": float(np.sqrt(np.mean(rr ** 2)) * np.log(10))}
            print("   阶 %d（%d 系数）：基外残余 RMS %.5f dex（%.2f%%）" % (
                order, len(cols), res["order_scan"][order]["resid_rms_dex"], 100 * res["order_scan"][order]["resid_rms_frac"]))
        nb = 4; sub = []
        tx, ty = (gA[:, 0].min(), gA[:, 0].max()), (gA[:, 1].min(), gA[:, 1].max())
        bx = np.linspace(tx[0], tx[1] + 1, nb + 1); by = np.linspace(ty[0], ty[1] + 1, nb + 1)
        for i in range(nb):
            for j in range(nb):
                s = (gA[:, 0] >= bx[i]) & (gA[:, 0] < bx[i + 1]) & (gA[:, 1] >= by[j]) & (gA[:, 1] < by[j + 1])
                if s.sum() < 8: continue
                rr = fA_f[s] / fB_f[s]; kk = (rr > med / 1.5) & (rr < med * 1.5)
                if kk.sum() < 6: continue
                sub.append(float(np.median(rr[kk]) / mfac - 1))
        sub = np.array(sub)
        res["subblock_single_scalar"] = {"n": int(sub.size), "rms": float(np.sqrt(np.mean(sub ** 2))) if sub.size else None,
                                         "median": float(np.median(sub)) if sub.size else None,
                                         "p16": float(np.percentile(sub, 16)) if sub.size else None,
                                         "p84": float(np.percentile(sub, 84)) if sub.size else None}
        print("   单标量残余：%d 子块 | 中位 %s | RMS %s" % (
            sub.size, None if not sub.size else "%+.5f" % np.median(sub),
            None if not sub.size else "%.5f" % np.sqrt(np.mean(sub ** 2))))
json.dump(res, open(f"{OUT}/p5mI_three_arm_v2.json", "w"), indent=1)
print("写入 results/p5w/p5mI_three_arm_v2.json")
