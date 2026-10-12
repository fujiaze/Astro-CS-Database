"""接缝三臂实证（真实帧）：构造真实接缝，比较 仅加性 / 仅乘性 / 加性+乘性。

判据复用（只读导入）：实验/additive-sky-seamless/code/sci_c_common.py::seam_steps(
    mosaic, boundaries, halfwin=8, base_win=64, order=2, min_valid=8, offlocus=True)
几何：帧头 SIP + p1_wcs.json 的 wcs.sip（禁用 samples）。乘性因子：加权比值中位。
"""
import json, os, sys, importlib.util
import numpy as np
from astropy.io import fits
from matplotlib.path import Path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
spec = importlib.util.spec_from_file_location("mD", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5mD_sip_kphoto.py"))
mD = importlib.util.module_from_spec(spec)
try: spec.loader.exec_module(mD)
except SystemExit: pass
sip_wcs, unit = mD.sip_wcs, mD.unit
ADD = "/workspace/Astro CS Database/实验/additive-sky-seamless/code"
spec2 = importlib.util.spec_from_file_location("sci_c_common", os.path.join(ADD, "sci_c_common.py"))
scc = importlib.util.module_from_spec(spec2); spec2.loader.exec_module(scc)
seam_steps = scc.seam_steps
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
G = 1024; PIX = 0.967
DEG = PIX / 3600.0

def fmeta(tag, part):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
    src = {fr["file"].replace("cleaned_", "").replace(".fts", ""):
           [r for r in fr["results"] if r.get("valid") and r["flux"] > 0] for fr in fl["frames"]}
    for e in man["frames"]:
        stem = os.path.basename(e["input_light"]).replace(".fts", "")
        if part in stem:
            p = os.path.join(PDIR, tag, f"cleaned_{stem}.fts")
            if os.path.exists(p) and src.get(stem):
                return e["frame_id"], p, src[stem]
    return None, None, None

def samp(img, gx, gy):
    ix = np.floor(gx); iy = np.floor(gy)
    ok = (ix >= 0) & (ix < img.shape[1] - 1) & (iy >= 0) & (iy < img.shape[0] - 1)
    ix = np.clip(ix, 0, img.shape[1] - 2).astype(np.int64); iy = np.clip(iy, 0, img.shape[0] - 2).astype(np.int64)
    fx = gx - ix; fy = gy - iy
    v = ((1 - fx) * (1 - fy) * img[iy, ix] + fx * (1 - fy) * img[iy, ix + 1] +
         (1 - fx) * fy * img[iy + 1, ix] + fx * fy * img[iy + 1, ix + 1])
    return np.where(ok & np.isfinite(v), v, np.nan)

# —— 选帧：相邻指向（M2 t2 20251212 × M3 t2 20251224），重叠是真实条带 ——
fA, pA, srcA = fmeta("t2", "M2_T2_flying_dutchman-20251212")
fB, pB, srcB = fmeta("t2", "M3_T2_flying_dutchman-20251224")
print("帧 A:", fA, "\n帧 B:", fB)
WA, WB = sip_wcs(os.path.join(PDIR, "t2", fA, "p1_wcs.json")), sip_wcs(os.path.join(PDIR, "t2", fB, "p1_wcs.json"))
def srcs(src):
    xy = np.array([[s["x"], s["y"]] for s in src]); f = np.array([s["flux"] for s in src])
    ra, dec = WA.all_pix2world(xy[:, 0], xy[:, 1], 0) if src is srcA else WB.all_pix2world(xy[:, 0], xy[:, 1], 0)
    return xy, f, ra, dec
xyA, fAflux, raA, decA = srcs(srcA); xyB, fBflux, raB, decB = srcs(srcB)
ra0, dec0 = float(np.median(np.concatenate([raA, raB]))), float(np.median(np.concatenate([decA, decB])))
def gxy(W, ra, dec):
    Bq = np.array([[1, 0, 0]]); # 直接反解：用牛顿不必要——用切平面近似 + SIP 前向迭代一次
    return None
# 网格坐标 → 帧像素：用 SIP 前向的逆需迭代；这里改用「帧像素→天球→网格」建掩膜，采样用前向反查表
gg = (np.arange(G) - G / 2) * DEG
UU, VV = np.meshgrid(gg, gg)
# 建立「网格 → 帧像素」：先在帧像素上撒稠密网格，求其 (ra,dec) → 网格坐标，再栅格化成映射表
def build_map(W, imgshape):
    xs = np.arange(0, imgshape[1], 8.0); ys = np.arange(0, imgshape[0], 8.0)
    XS, YS = np.meshgrid(xs, ys); XS = XS.ravel(); YS = YS.ravel()
    ra, dec = W.all_pix2world(XS, YS, 0)
    u = (ra - ra0) / DEG; v = (dec - dec0) / DEG
    gx = u + G / 2; gy = v + G / 2
    return XS, YS, gx, gy
mgA = build_map(WA, fits.getdata(pA).shape); mgB = build_map(WB, fits.getdata(pB).shape)
def grid_to_frame(mg, img, thresh=6.0):
    """把帧像素 → 网格坐标的散点表做最近邻栅格化，得到网格上每点的帧像素坐标与掩膜。"""
    XS, YS, gx, gy = mg
    gxi = np.round(gx).astype(int); gyi = np.round(gy).astype(int)
    ok = (gxi >= 0) & (gxi < G) & (gyi >= 0) & (gyi < G)
    Mx = np.full((G, G), np.nan); My = np.full((G, G), np.nan)
    Mx[gyi[ok], gxi[ok]] = XS[ok]; My[gyi[ok], gxi[ok]] = YS[ok]
    # 空洞用形态学填充：用 3×3 中值迭代
    from scipy.ndimage import generic_filter
    for _ in range(3):
        nanm = ~np.isfinite(Mx)
        if not nanm.any(): break
        Mx = np.where(nanm, generic_filter(np.nan_to_num(Mx), np.nanmedian, size=3), Mx)
        My = np.where(nanm, generic_filter(np.nan_to_num(My), np.nanmedian, size=3), My)
    mask = np.isfinite(Mx)
    return Mx, My, mask
MxA, MyA, maskA = grid_to_frame(mgA, None)
MxB, MyB, maskB = grid_to_frame(mgB, None)
imgA = fits.getdata(pA).astype(np.float64); imgB = fits.getdata(pB).astype(np.float64)
PA = samp(imgA, MxA, MyA); PB = samp(imgB, MxB, MyB)
print("网格 %d² | A 覆盖 %d | B 覆盖 %d | 两者重叠 %d" % (G, maskA.sum(), maskB.sum(), (maskA & maskB).sum()))
ov = maskA & maskB
ys, xs = np.nonzero(ov)
x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
print("重叠 bbox x[%d,%d] y[%d,%d]（%d×%d）" % (x0, x1, y0, y1, x1 - x0 + 1, y1 - y0 + 1))
# 乘性因子（加权比值中位，用重叠区共同星）
xgA, ygA = (raA - ra0) / DEG + G / 2, (decA - dec0) / DEG + G / 2
xgB, ygB = (raB - ra0) / DEG + G / 2, (decB - dec0) / DEG + G / 2
from scipy.spatial import cKDTree
tB = cKDTree(np.stack([xgB, ygB], 1)); d, idx = tB.query(np.stack([xgA, ygA], 1), k=1)
inA = maskA[yy, xx] if False else None
sel = d < 1.0 * (PIX and 1.0)
ratios = fAflux[sel] / fBflux[idx[sel]]
med = float(np.median(ratios))
keep = (ratios > med / 1.5) & (ratios < med * 1.5)
mfac = float(np.median(ratios[keep]))
se = float(1.4826 * np.median(np.abs(ratios[keep] - mfac)) / np.sqrt(keep.sum()))
print("共同星 %d（±1 格）| 乘性因子 m = %.5f（SE %.5f, 3σ下限 %.5f）" % (sel.sum(), mfac, se, 3 * se))

def mosaic_for(x_seam, dB, mB, injected=None):
    """左半 A、右半 B；dB 加性偏置、mB 乘性因子作用在 B 上；injected 为注入的额外乘性失配。"""
    PBc = PB * mB + dB
    if injected: PBc = PBc * injected
    out = np.where(maskA, PA, np.nan)
    right = maskB & (np.arange(G)[None, :] > x_seam)
    out = np.where(right, np.where(np.isfinite(PBc), PBc, out), out)
    return out

# 两帧在重叠区的加性偏置（天光电平差）
dA = np.nanmedian(PA[ov]); dB0 = np.nanmedian(PB[ov])
seed = (dA - dB0)
print("重叠区中位电平：A %.3f | B %.3f ⇒ 加性偏置差 %.3f" % (dA, dB0, seed))
res = {"frames": [fA, fB], "grid": G, "pix_arcsec": PIX, "overlap_bbox": [int(x0), int(x1), int(y0), int(y1)],
       "m_factor": mfac, "m_se": se, "three_sigma_limit": 3 * se, "level_offset": float(seed)}
seams = [int(x0 + f * (x1 - x0)) for f in (0.35, 0.45, 0.5, 0.55, 0.65)]
arms = {"additive_only": dict(mB=1.0, dB=seed, injected=None),
        "multiplicative_only": dict(mB=mfac, dB=0.0, injected=None),
        "both": dict(mB=mfac, dB=seed, injected=None),
        "INJ1.56_additive_only": dict(mB=1.0, dB=seed, injected=1.56),
        "INJ1.56_multiplicative_only": dict(mB=mfac, dB=0.0, injected=1.56),
        "INJ1.56_both": dict(mB=mfac, dB=seed, injected=1.56)}
res["arms"] = {}
for name, kw in arms.items():
    mos = mosaic_for(seams[2], kw["mB"], kw["dB"], kw["injected"])
    st = seam_steps(mos, boundaries=seams, halfwin=8, base_win=64, order=2, min_valid=8, offlocus=True)
    rels = [s["rel"] for s in st if np.isfinite(s["rel"])]
    cov = [float(np.mean(np.isfinite(mos[:, s["x"]]))) for s in st]
    res["arms"][name] = {"rel": [None if not np.isfinite(s["rel"]) else float(s["rel"]) for s in st],
                         "rel_median": float(np.median(rels)) if rels else None,
                         "rel_abs_median": float(np.median(np.abs(rels))) if rels else None,
                         "excess": [None if not np.isfinite(s["excess"]) else float(s["excess"]) for s in st],
                         "coverage": cov, "n_measurable": len(rels)}
    print("  %-30s rel中位 %s | |rel|中位 %s | 可测边 %d/%d | 覆盖 %s" % (
        name, ("%.5f" % np.median(rels)) if rels else "—", ("%.5f" % np.median(np.abs(rels))) if rels else "—",
        len(rels), len(st), np.round(cov, 2).tolist()))
# 单标量残余：重叠条带分块，逐块星比相对全局标量
nb = 4
sub = []
bx = np.linspace(x0, x1 + 1, nb + 1); by = np.linspace(y0, y1 + 1, nb + 1)
for i in range(nb):
    for j in range(nb):
        s = sel & (xgA >= bx[i]) & (xgA < bx[i + 1]) & (ygA >= by[j]) & (ygA < by[j + 1])
        if s.sum() < 10: continue
        r = fAflux[s] / fBflux[idx[s]]
        k = (r > med / 1.5) & (r < med * 1.5)
        if k.sum() < 8: continue
        sub.append(float(np.median(r[k]) / mfac))
sub = np.array(sub)
res["subblock"] = {"n": int(sub.size), "median": float(np.median(sub)) if sub.size else None,
                   "rms": float(np.sqrt(np.mean((sub - 1) ** 2))) if sub.size else None,
                   "p16": float(np.percentile(sub, 16)) if sub.size else None,
                   "p84": float(np.percentile(sub, 84)) if sub.size else None}
print("单标量残余（%d 子块）：中位偏离 %+.5f | RMS %.5f（=%.1f%%）" % (
    sub.size, (np.median(sub) - 1) if sub.size else 0, res["subblock"]["rms"] or 0, 100 * (res["subblock"]["rms"] or 0)))
json.dump(res, open(f"{OUT}/p5mH_seam_three_arm.json", "w"), indent=1)
print("写入 results/p5w/p5mH_seam_three_arm.json")
