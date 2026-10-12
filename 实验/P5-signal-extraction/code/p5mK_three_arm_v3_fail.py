"""三臂 v3：F0 修 ravel；F1 边界从掩膜推出；S3/F3 硬自检；过后跑三臂（raw_step 主判据）。"""
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
ADD = "/workspace/Astro CS Database/实验/additive-sky-seamless/code"
sp2 = importlib.util.spec_from_file_location("scc", os.path.join(ADD, "sci_c_common.py"))
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

fA, pA, srcA = fmeta("t2", "M2_T2_flying_dutchman-20251212")
fB, pB, srcB = fmeta("t2", "M3_T2_flying_dutchman-20251224")
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
RAr, DECr = RA.ravel(), DEC.ravel()                       # F0：ravel 修好
def inside(ra_, de_):
    poly = Path(np.stack([(ra_ - ra0) / DEG, (de_ - dec0) / DEG], 1))
    return poly.contains_points(np.stack([(RAr - ra0) / DEG, (DECr - dec0) / DEG], 1)).reshape(G, G)
mA = inside(rb, db); mB = inside(rc, dc)
imgA = fits.getdata(pA).astype(np.float64); imgB = fits.getdata(pB).astype(np.float64)
def g2p(W):
    x, y = W.all_world2pix(RAr, DECr, 0, quiet=True); return x.reshape(G, G), y.reshape(G, G)
PA = samp(imgA, *g2p(WA)); PB = samp(imgB, *g2p(WB))
mA = mA & np.isfinite(PA); mB = mB & np.isfinite(PB)
print("网格 %d² | A %.3f B %.3f | 交 %.3f | 交/并 %.3f" % (G, mA.mean(), mB.mean(), (mA & mB).mean(),
      (mA & mB).sum() / max((mA | mB).sum(), 1)))
# ---- F1：边界从掩膜推出：取 min(covA,covB) 最大的列 ----
covA = mA.mean(axis=0); covB = mB.mean(axis=0)
xb = int(np.argmax(np.minimum(covA, covB)))
print("F1 边界 x_b = %d | covA=%.3f covB=%.3f | 该列有效样本 L %d R %d" % (
    xb, covA[xb], covB[xb], int(np.isfinite(PA[:, xb]).sum()), int(np.isfinite(PB[:, xb]).sum())))
b_right = mB[:, xb:].mean() > mB[:, :xb].mean()
side = (np.arange(G)[None, :] > xb) if b_right else (np.arange(G)[None, :] <= xb)
ov = mA & mB
lvA = float(np.nanmedian(PA[ov])); lvB = float(np.nanmedian(PB[ov])); dOff = lvA - lvB
def raw(mos, x=0, hw=HW):
    L = mos[:, x - hw:x]; R = mos[:, x:x + hw]
    Lv = L[np.isfinite(L)]; Rv = R[np.isfinite(R)]
    if Lv.size < 8 or Rv.size < 8: return None, Lv.size, Rv.size
    return float(np.median(Rv) - np.median(Lv)), int(Lv.size), int(Rv.size)
def build(mBf=1.0, dOffv=None, inj=None):
    d = dOff if dOffv is None else dOffv
    PBc = PB * mBf + d
    if inj is not None: PBc = PBc * inj
    out = np.where(mA, PA, np.nan)
    return np.where((mB & side) & np.isfinite(PBc), PBc, out)
mos0 = build()
rs0, nl0, nr0 = raw(mos0, xb)
den = abs(lvA)
rs0_rel = None if rs0 is None else rs0 / den
st0 = seam_steps(mos0, boundaries=[xb], halfwin=HW, base_win=64, order=2, min_valid=8, offlocus=True)
exp_rel = (lvA - lvB) / den          # 未注入时应出现的相对台阶（左半为分母）
print("F1 两侧电平：L(A侧) %.1f | B 帧 %.1f | Δ %.1f ⇒ 期望 raw_step_rel ≈ %+.4f" % (lvA, lvB, dOff, exp_rel))
print("S3 未注入：raw_step %.3f（相对 %+.5f）| 生产 rel_step %+.5f | 有效 L/R %d/%d" % (
    rs0, rs0_rel, st0[0]["rel"], nl0, nr0))
S3 = bool(rs0_rel is not None and np.sign(rs0_rel) == np.sign(exp_rel) and abs(rs0_rel) >= 0.75 * abs(exp_rel))
print("   S3 判定：%s" % ("通过" if S3 else "**未通过**（未注入的台阶归零 ⇒ 边界不是 A|B 突变处）"))
mosI = build(inj=1.56); rsI, nlI, nrI = raw(mosI, xb)
rsI_rel = None if rsI is None else rsI / den
stI = seam_steps(mosI, boundaries=[xb], halfwin=HW, base_win=64, order=2, min_valid=8, offlocus=True)
shift = None if (rs0_rel is None or rsI_rel is None) else rsI_rel - rs0_rel
print("F3 注入 1.56×：raw_step %.3f（相对 %+.5f）| 位移 %+.5f（期望 ≈ +0.34）| 生产 rel_step %+.5f" % (
    rsI, rsI_rel, shift, stI[0]["rel"]))
S2b = bool(shift is not None and 0.20 <= shift <= 0.45)
print("   F3 判定：%s" % ("通过" if S2b else "**未通过**"))
res = {"frames": [fA, fB], "G": G, "x_b": xb, "covA_xb": float(covA[xb]), "covB_xb": float(covB[xb]),
       "B_side": "right" if b_right else "left", "level_A": lvA, "level_B": lvB, "offset": dOff,
       "expected_rel": exp_rel, "S3": {"raw_step": rs0, "raw_step_rel": rs0_rel, "rel_step": float(st0[0]["rel"]),
       "n_left": nl0, "n_right": nr0, "coverage": float(np.mean(np.isfinite(mos0[:, xb]))), "pass": S3},
       "F3": {"raw_step": rsI, "raw_step_rel": rsI_rel, "shift": shift, "rel_step": float(stI[0]["rel"]), "pass": S2b}}
if S3 and S2b:
    arms = {}
    for nm, kw in {"additive_only": dict(mBf=1.0, dOffv=dOff), "multiplicative_only": dict(mBf=None if False else 0.0, dOffv=0.0),
                   "both": dict(mBf=1.0, dOffv=dOff),
                   "inj1.56_additive_only": dict(mBf=1.0, dOffv=dOff, inj=1.56),
                   "inj1.56_multiplicative_only": dict(mBf=1.0, dOffv=0.0, inj=1.56),
                   "inj1.56_both": dict(mBf=1.0, dOffv=dOff, inj=1.56),
                   "inj1.03_additive_only": dict(mBf=1.0, dOffv=dOff, inj=1.03),
                   "inj1.03_multiplicative_only": dict(mBf=1.0, dOffv=0.0, inj=1.03),
                   "inj1.03_both": dict(mBf=1.0, dOffv=dOff, inj=1.03)}.items():
        rr = build(**kw); r_, nl, nr = raw(rr, xb)
        st = seam_steps(rr, boundaries=[xb], halfwin=HW, base_win=64, order=2, min_valid=8, offlocus=True)
        arms[nm] = {"raw_step": r_, "raw_step_rel": None if r_ is None else r_ / den, "rel_step": float(st[0]["rel"]),
                    "n_left": nl, "n_right": nr, "coverage": float(np.mean(np.isfinite(rr[:, xb])))}
        print("   %-30s raw_step_rel %s | rel_step %+.5f | 覆盖 %.2f" % (
            nm, None if arms[nm]["raw_step_rel"] is None else "%+.5f" % arms[nm]["raw_step_rel"],
            arms[nm]["rel_step"], arms[nm]["coverage"]))
    res["arms"] = arms
json.dump(res, open(f"{OUT}/p5mK_three_arm_v3.json", "w"), indent=1)
print("写入 results/p5w/p5mK_three_arm_v3.json")
