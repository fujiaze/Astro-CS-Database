"""三臂 v4：门基线改为「原始拼接图」（不做任何校正），期望由数据导出；门过后跑三臂 + 单标量阶数 + 小注入。"""
import json, os, sys, importlib.util
import numpy as np
from astropy.io import fits
from matplotlib.path import Path
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sp = importlib.util.spec_from_file_location("mD", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5mD_sip_kphoto.py"))
mD = importlib.util.module_from_spec(sp)
try: sp.loader.exec_module(mD)
except SystemExit: pass
sip_wcs = mD.sip_wcs
sp2 = importlib.util.spec_from_file_location("scc", "/workspace/Astro CS Database/实验/additive-sky-seamless/code/sci_c_common.py")
scc = importlib.util.module_from_spec(sp2); sp2.loader.exec_module(scc)
seam_steps = scc.seam_steps
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
PIX = 0.967; DEG = PIX / 3600.0; G = 1024; HW = 8
def fmeta(tag, part):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
    src = {fr["file"].replace("cleaned_", "").replace(".fts", ""):
           [r for r in fr["results"] if r.get("valid") and r["flux"] > 0] for fr in fl["frames"]}
    for e in man["frames"]:
        stem = os.path.basename(e["input_light"]).replace(".fts", "")
        if part in stem: return e["frame_id"], os.path.join(PDIR, tag, f"cleaned_{stem}.fts"), src.get(stem)
    return None, None, None
def samp(img, gx, gy):
    ix = np.floor(gx); iy = np.floor(gy)
    ok = (ix >= 0) & (ix < img.shape[1] - 1) & (iy >= 0) & (iy < img.shape[0] - 1)
    ix = np.clip(ix, 0, img.shape[1] - 2).astype(np.int64); iy = np.clip(iy, 0, img.shape[0] - 2).astype(np.int64)
    fx = gx - ix; fy = gy - iy
    v = ((1 - fx) * (1 - fy) * img[iy, ix] + fx * (1 - fy) * img[iy, ix + 1] +
         (1 - fx) * fy * img[iy + 1, ix] + fx * fy * img[iy + 1, ix + 1])
    return np.where(ok & np.isfinite(v), v, np.nan)
fA, pA, srcA = fmeta("t2", "M2_T2_flying_dutchman-20251212"); fB, pB, srcB = fmeta("t2", "M3_T2_flying_dutchman-20251224")
WA = sip_wcs(os.path.join(PDIR, "t2", fA, "p1_wcs.json")); WB = sip_wcs(os.path.join(PDIR, "t2", fB, "p1_wcs.json"))
n = 4095.0; bnd = []
for t in np.linspace(0, 1, 41): bnd.append((t * n, 0))
for t in np.linspace(0, 1, 41): bnd.append((n, t * n))
for t in np.linspace(1, 0, 41): bnd.append((t * n, n))
for t in np.linspace(1, 0, 41): bnd.append((0, t * n))
bnd = np.array(bnd)
rb, db = WA.all_pix2world(bnd[:, 0], bnd[:, 1], 0); rc, dc = WB.all_pix2world(bnd[:, 0], bnd[:, 1], 0)
ra0, dec0 = float((np.median(rb) + np.median(rc)) / 2), float((np.median(db) + np.median(dc)) / 2)
g = (np.arange(G) - G / 2) * DEG
RA, DEC = np.meshgrid(ra0 + g / np.cos(np.radians(dec0)), dec0 + g)
RAr, DECr = RA.ravel(), DEC.ravel()
def inside(ra_, de_):
    poly = Path(np.stack([(ra_ - ra0) / DEG, (de_ - dec0) / DEG], 1))
    return poly.contains_points(np.stack([(RAr - ra0) / DEG, (DECr - dec0) / DEG], 1)).reshape(G, G)
mA, mB = inside(rb, db), inside(rc, dc)
imgA = fits.getdata(pA).astype(np.float64); imgB = fits.getdata(pB).astype(np.float64)
def g2p(W):
    x, y = W.all_world2pix(RAr, DECr, 0, quiet=True); return x.reshape(G, G), y.reshape(G, G)
PA = samp(imgA, *g2p(WA)); PB = samp(imgB, *g2p(WB))
mA = mA & np.isfinite(PA); mB = mB & np.isfinite(PB)
covA, covB = mA.mean(axis=0), mB.mean(axis=0)
xb = int(np.argmax(np.minimum(covA, covB)))
b_right = mB[:, xb:].mean() > mB[:, :xb].mean()
side = (np.arange(G)[None, :] > xb) if b_right else (np.arange(G)[None, :] <= xb)
ov = mA & mB
lvA = float(np.nanmedian(PA[ov])); lvB = float(np.nanmedian(PB[ov]))
def build(mBf=1.0, dOffv=0.0, inj=None, cut=None):
    xc = xb if cut is None else cut
    PBc = PB * mBf + dOffv
    if inj is not None: PBc = PBc * inj
    sd = (np.arange(G)[None, :] > xc) if b_right else (np.arange(G)[None, :] <= xc)
    out = np.where(mA, PA, np.nan)
    return np.where((mB & sd) & np.isfinite(PBc), PBc, out)
def raw(mos, x=None, hw=HW):
    x = xb if x is None else x
    L = mos[:, x - hw:x]; R = mos[:, x:x + hw]
    Lv = L[np.isfinite(L)]; Rv = R[np.isfinite(R)]
    if Lv.size < 8 or Rv.size < 8: return None, None, Lv.size, Rv.size
    return float(np.median(Rv) - np.median(Lv)), float(np.median(Lv)), int(Lv.size), int(Rv.size)
def measure(mos, x=None):
    x = xb if x is None else x
    rs, lvl, nl, nr = raw(mos, x)
    st = seam_steps(mos, boundaries=[x], halfwin=HW, base_win=64, order=2, min_valid=8, offlocus=True)
    cov = float(np.mean(np.isfinite(mos[:, x])))
    return {"raw_step": rs, "raw_step_rel": None if (rs is None or not lvl) else rs / abs(lvl),
            "rel_step": float(st[0]["rel"]) if st[0]["rel"] == st[0]["rel"] else None,
            "n_left": nl, "n_right": nr, "coverage": cov}
# ---- 门：原始拼接图（不做任何校正）----
mos_raw = build(mBf=1.0, dOffv=0.0)
m0 = measure(mos_raw)
exp_rel = (lvA - lvB) / abs(lvB) if not b_right else (lvA - lvB) / abs(lvB)
# 数据导出的期望：左半=右侧帧(或B) 的实际中位
Lm = np.nanmedian(mos_raw[:, :xb]); Rm = np.nanmedian(mos_raw[:, xb:])
exp_data = (Rm - Lm) / abs(Lm)
mos_inj = build(mBf=1.0, dOffv=0.0, inj=1.56); mI = measure(mos_inj)
Blvl = lvB
exp_shift = -(1.56 - 1.0) * Blvl / abs(Lm)
S3 = bool(m0["raw_step_rel"] is not None and np.sign(m0["raw_step_rel"]) == np.sign(exp_data)
          and abs(m0["raw_step_rel"]) >= 0.5 * abs(exp_data))
F3 = bool(mI["raw_step_rel"] is not None and m0["raw_step_rel"] is not None
          and 0.5 <= (mI["raw_step_rel"] - m0["raw_step_rel"]) / exp_shift <= 1.5)
print("F1 边界 x_b=%d | covA=%.3f covB=%.3f | B 在%s | 电平 A %.1f B %.1f" % (xb, covA[xb], covB[xb], "右" if b_right else "左", lvA, lvB))
print("S3 原始图：raw_step %s（相对 %s）| 数据导出的期望 %+.5f | rel_step %s | pass=%s" % (
    m0["raw_step"], None if m0["raw_step_rel"] is None else "%+.5f" % m0["raw_step_rel"], exp_data, m0["rel_step"], S3))
print("F3 注入1.56：raw_step_rel %s | 位移 %s（期望 %+.5f）| rel_step %s | pass=%s" % (
    None if mI["raw_step_rel"] is None else "%+.5f" % mI["raw_step_rel"],
    None if (mI["raw_step_rel"] is None or m0["raw_step_rel"] is None) else "%+.5f" % (mI["raw_step_rel"] - m0["raw_step_rel"]),
    exp_shift, mI["rel_step"], F3))
res = {"frames": [fA, fB], "G": G, "x_b": xb, "B_side": "right" if b_right else "left",
       "level_A": lvA, "level_B": lvB, "exp_data": exp_data, "exp_shift": exp_shift,
       "S3": {**m0, "exp": exp_data, "pass": S3}, "F3": {**mI, "exp_shift": exp_shift, "pass": F3}}
# ---- 乘性因子（加权比值中位，共同星）----
xyA = np.array([[s["x"], s["y"]] for s in srcA]); xyB = np.array([[s["x"], s["y"]] for s in srcB])
raA, decA = WA.all_pix2world(xyA[:, 0], xyA[:, 1], 0); raB, decB = WB.all_pix2world(xyB[:, 0], xyB[:, 1], 0)
gA = np.stack([(raA - ra0) / DEG + G / 2, (decA - dec0) / DEG + G / 2], 1)
gB = np.stack([(raB - ra0) / DEG + G / 2, (decB - dec0) / DEG + G / 2], 1)
fA_f = np.array([s["flux"] for s in srcA]); fB_f = np.array([s["flux"] for s in srcB])
t = cKDTree(gB); d, idx = t.query(gA, k=1)
s_ = (d < 1.5) & (gA[:, 0] >= 0) & (gA[:, 0] < G) & (gA[:, 1] >= 0) & (gA[:, 1] < G)
rat = fA_f[s_] / fB_f[idx[s_]]; med = float(np.median(rat)); k1 = (rat > med / 1.5) & (rat < med * 1.5)
mfac = float(np.median(rat[k1])); se_m = float(1.4826 * np.median(np.abs(rat[k1] - mfac)) / np.sqrt(k1.sum()))
res["mult"] = {"n_stars": int(s_.sum()), "m": mfac, "se": se_m, "three_sigma": 3 * se_m}
print("共同星 %d | m(F_A/F_B) = %.5f (SE %.5f, 3σ %.5f)" % (s_.sum(), mfac, se_m, 3 * se_m))
if S3 and F3:
    arms = {"raw": dict(mBf=1.0, dOffv=0.0),
            "additive_only": dict(mBf=1.0, dOffv=lvA - lvB),
            "multiplicative_only": dict(mBf=mfac, dOffv=0.0),
            "both": dict(mBf=mfac, dOffv=lvA - lvB * mfac),
            "inj1.56_additive_only": dict(mBf=1.0, dOffv=lvA - lvB, inj=1.56),
            "inj1.56_multiplicative_only": dict(mBf=mfac, dOffv=0.0, inj=1.56),
            "inj1.56_both": dict(mBf=mfac, dOffv=lvA - lvB * mfac, inj=1.56)}
    for injv in (1.02, 1.05):
        arms["inj%.2f_both" % injv] = dict(mBf=mfac, dOffv=lvA - lvB * mfac, inj=injv)
    res["arms"] = {}
    for nm, kw in arms.items():
        mm = measure(build(**kw)); res["arms"][nm] = mm
        print("   %-28s raw_step_rel %s | rel_step %s | L/R %d/%d | 覆盖 %.2f" % (
            nm, None if mm["raw_step_rel"] is None else "%+.5f" % mm["raw_step_rel"],
            None if mm["rel_step"] is None else "%+.5f" % mm["rel_step"], mm["n_left"], mm["n_right"], mm["coverage"]))
    # 单标量够不够：共同星比值对重叠条带的空间变化 + 阶数扫描
    u = (gA[s_, 0] - G / 2) * PIX; v = (gA[s_, 1] - G / 2) * PIX; lr = np.log10(rat)
    kk = (rat > med / 1.5) & (rat < med * 1.5)
    res["order_scan"] = {}
    for order in (0, 1, 2, 3):
        cols = [np.ones_like(u)]
        for i in range(1, order + 1):
            for j in range(0, i + 1):
                cols.append((u ** (i - j)) * (v ** j))
        Bm = np.stack(cols, 1)
        coef, *_ = np.linalg.lstsq(Bm, lr, rcond=None)
        rr = lr - Bm @ coef
        ptp = float(np.max(Bm @ coef) - np.min(Bm @ coef))
        res["order_scan"][order] = {"n_coef": int(Bm.shape[1]), "resid_rms_dex": float(np.sqrt(np.mean(rr ** 2))),
                                    "resid_rms_frac": float(np.sqrt(np.mean(rr ** 2)) * np.log(10)), "ptp_dex": ptp}
        print("   阶 %d（%d 系数）：面峰谷 %.4f dex | 基外残余 RMS %.4f dex（%.2f%%）" % (
            order, Bm.shape[1], ptp, res["order_scan"][order]["resid_rms_dex"], 100 * res["order_scan"][order]["resid_rms_frac"]))
    nb = 4; sub = []
    bx = np.linspace(gA[s_, 0].min(), gA[s_, 0].max() + 1, nb + 1); by = np.linspace(gA[s_, 1].min(), gA[s_, 1].max() + 1, nb + 1)
    for i in range(nb):
        for j in range(nb):
            ss = (gA[s_, 0] >= bx[i]) & (gA[s_, 0] < bx[i + 1]) & (gA[s_, 1] >= by[j]) & (gA[s_, 1] < by[j + 1])
            if ss.sum() < 6: continue
            rr = rat[ss]; k2 = (rr > med / 1.5) & (rr < med * 1.5)
            if k2.sum() < 5: continue
            sub.append(float(np.median(rr[k2]) / mfac - 1))
    sub = np.array(sub)
    res["subblock_single_scalar"] = {"n": int(sub.size), "rms": float(np.sqrt(np.mean(sub ** 2))) if sub.size else None,
                                     "median": float(np.median(sub)) if sub.size else None, "frac": sub.tolist()}
    print("单标量残余：%d 子块 | RMS %s" % (sub.size, None if not sub.size else "%.5f" % np.sqrt(np.mean(sub ** 2))))
json.dump(res, open(f"{OUT}/p5mL_three_arm_v4.json", "w"), indent=1)
print("写入 results/p5w/p5mL_three_arm_v4.json")
