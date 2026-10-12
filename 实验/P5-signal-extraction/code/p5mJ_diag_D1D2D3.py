"""D1 剖面直打 / D2 判据对已知台阶的响应 / D3 raw_step 诊断量。
判据仍只读导入 sci_c_common.seam_steps（不改判据、不调参）。
"""
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
res = {}
rng = np.random.default_rng(7)
XB, HW, BW, ORD = 1024, 8, 64, 2
def raw_step(mos, xb, half=8):
    L = mos[:, xb - half:xb]; R = mos[:, xb:xb + half]
    Lv = L[np.isfinite(L)]; Rv = R[np.isfinite(R)]
    if Lv.size < 10 or Rv.size < 10: return None, Lv.size, Rv.size
    return float(np.median(Rv) - np.median(Lv)), int(Lv.size), int(Rv.size)
# ============ D2：人造已知台阶（左 200，右 200×1.36 或 200+72），2048×1024 ============
print("=== D2 判据对已知台阶的响应（人造图，边界 x=1024，同 halfwin/base_win/order）===")
D2 = {}
for name, right in (("multiplicative_1.36", 200.0 * 1.36), ("additive_+72", 200.0 + 72.0)):
    img = np.full((1024, 2048), np.nan)
    img[:, :XB] = 200.0; img[:, XB:] = right
    img += rng.normal(0, 1.0, img.shape)          # 轻微噪声
    st = seam_steps(img, boundaries=[XB], halfwin=HW, base_win=BW, order=ORD, min_valid=8, offlocus=True)
    rs, nl, nr = raw_step(img, XB, HW)
    D2[name] = {"rel_step": float(st[0]["rel"]), "step": float(st[0]["step"]), "level": float(st[0]["level"]),
                "excess": None if st[0]["excess"] is None else float(st[0]["excess"]),
                "raw_step": rs, "raw_step_rel": None if rs is None else float(rs / st[0]["level"]),
                "n_left": nl, "n_right": nr, "coverage": 1.0}
    print("  %-18s rel_step %.5f | step %.3f | raw_step %.3f (相对 %.5f) | 有效 L/R %d/%d" % (
        name, D2[name]["rel_step"], D2[name]["step"], rs, D2[name]["raw_step_rel"], nl, nr))
res["D2_synthetic"] = D2
# ============ D1/D3：真实帧窄条（512 宽 × 1024 高，缝在正中 x=256）============
def fmeta(tag, part):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    for e in man["frames"]:
        stem = os.path.basename(e["input_light"]).replace(".fts", "")
        if part in stem: return e["frame_id"], os.path.join(PDIR, tag, f"cleaned_{stem}.fts")
    return None, None
def samp(img, gx, gy):
    ix = np.floor(gx); iy = np.floor(gy)
    ok = (ix >= 0) & (ix < img.shape[1] - 1) & (iy >= 0) & (iy < img.shape[0] - 1)
    ix = np.clip(ix, 0, img.shape[1] - 2).astype(np.int64); iy = np.clip(iy, 0, img.shape[0] - 2).astype(np.int64)
    fx = gx - ix; fy = gy - iy
    v = ((1 - fx) * (1 - fy) * img[iy, ix] + fx * (1 - fy) * img[iy, ix + 1] +
         (1 - fx) * fy * img[iy + 1, ix] + fx * fy * img[iy + 1, ix + 1])
    return np.where(ok & np.isfinite(v), v, np.nan)
fA, pA = fmeta("t2", "M2_T2_flying_dutchman-20251212"); fB, pB = fmeta("t2", "M3_T2_flying_dutchman-20251224")
WA = sip_wcs(os.path.join(PDIR, "t2", fA, "p1_wcs.json")); WB = sip_wcs(os.path.join(PDIR, "t2", fB, "p1_wcs.json"))
n = 4095.0; bnd = []
for t in np.linspace(0, 1, 33): bnd.append((t * n, 0))
for t in np.linspace(0, 1, 33): bnd.append((n, t * n))
for t in np.linspace(1, 0, 33): bnd.append((t * n, n))
for t in np.linspace(1, 0, 33): bnd.append((0, t * n))
bnd = np.array(bnd)
rb, db = WA.all_pix2world(bnd[:, 0], bnd[:, 1], 0); rc, dc = WB.all_pix2world(bnd[:, 0], bnd[:, 1], 0)
ra0, dec0 = float((np.median(rb) + np.median(rc)) / 2), float((np.median(db) + np.median(dc)) / 2)
GD, DEG = 0.967 / 3600.0, 0.967 / 3600.0
WX, HY, XS = 512, 1024, 256                      # 窄条：宽 512（缝在 256），高 1024
gx_ = (np.arange(WX) - XS) * DEG; gy_ = (np.arange(HY) - HY / 2) * DEG
UU, VV = np.meshgrid(gx_, gy_)
RA = ra0 + UU / np.cos(np.radians(dec0)); DEC = dec0 + VV
def inside(ra_, de_):
    poly = Path(np.stack([(ra_ - ra0) / DEG, (de_ - dec0) / DEG], 1))
    return poly.contains_points(np.stack([(RA - ra0) / DEG, (DEC - dec0) / DEG], 1)).reshape(HY, WX)
mA = inside(rb, db); mB = inside(rc, dc)
imgA = fits.getdata(pA).astype(np.float64); imgB = fits.getdata(pB).astype(np.float64)
def g2p(W):
    x, y = W.all_world2pix(RA.ravel(), DEC.ravel(), 0, quiet=True); return x.reshape(HY, WX), y.reshape(HY, WX)
PA = samp(imgA, *g2p(WA)); PB = samp(imgB, *g2p(WB))
mA = mA & np.isfinite(PA); mB = mB & np.isfinite(PB)
print("\n=== D1/D3 真实窄条 %d×%d（缝 x=%d）：A %.3f B %.3f 交 %.3f | 交/并 %.3f" % (
    HY, WX, XS, mA.mean(), mB.mean(), (mA & mB).mean(), (mA & mB).sum() / max((mA | mB).sum(), 1)))
b_right = mB[:, XS:].mean() > mB[:, :XS].mean()
print("   B 在网格的 %s 侧" % ("右" if b_right else "左"))
side = (np.arange(WX)[None, :] > XS) if b_right else (np.arange(WX)[None, :] <= XS)
lvA = float(np.nanmedian(PA[mA & mB])); lvB = float(np.nanmedian(PB[mA & mB]))
dOff = lvA - lvB
def build(inj=None):
    PBc = PB + dOff
    if inj is not None: PBc = PBc * inj
    out = np.where(mA, PA, np.nan)
    return np.where((mB & side) & np.isfinite(PBc), PBc, out)
mos0, mosI = build(), build(1.56)
def prof(mos):
    return np.array([np.nanmedian(mos[:, x]) for x in range(XS - 6, XS + 7)])
p0, pI = prof(mos0), prof(mosI)
cols = list(range(XS - 6, XS + 7))
print("   D1 逐列剖面 nanmedian(axis=0)：")
print("      列号   " + " ".join("%8d" % c for c in cols))
print("      无注入 " + " ".join("%8.2f" % v for v in p0))
print("      注入1.56 " + " ".join("%8.2f" % v for v in pI))
same = bool(np.allclose(p0, pI, equal_nan=True, rtol=0, atol=1e-9))
print("   D1 判读：两遍%s" % ("**逐列完全重合** ⇒ 图里没有台阶" if same else "在边界处不重合 ⇒ 台阶存在"))
st0 = seam_steps(mos0, boundaries=[XS], halfwin=HW, base_win=BW, order=ORD, min_valid=8, offlocus=True)
stI = seam_steps(mosI, boundaries=[XS], halfwin=HW, base_win=BW, order=ORD, min_valid=8, offlocus=True)
rs0, nl0, nr0 = raw_step(mos0, XS, HW); rsI, nlI, nrI = raw_step(mosI, XS, HW)
res["D1_real"] = {"frame": [fA, fB], "strip": [HY, WX], "seam_x": XS, "A_frac": float(mA.mean()), "B_frac": float(mB.mean()),
                  "B_side": "right" if b_right else "left", "level_A": lvA, "level_B": lvB, "offset": dOff,
                  "cols": cols, "prof_no_inj": p0.tolist(), "prof_inj156": pI.tolist(), "identical": same,
                  "rel_step_no_inj": float(st0[0]["rel"]) if st0[0]["rel"] == st0[0]["rel"] else None,
                  "rel_step_inj156": float(stI[0]["rel"]) if stI[0]["rel"] == stI[0]["rel"] else None,
                  "coverage_no_inj": float(np.mean(np.isfinite(mos0[:, XS]))), "cov_right_point": float(np.mean(np.isfinite(mos0[:, XS + 1]))),
                  "raw_step_no_inj": rs0, "raw_step_rel_no_inj": None if rs0 is None else float(rs0 / max(abs(lvA), 1e-9)),
                  "raw_step_inj156": rsI, "raw_step_rel_inj156": None if rsI is None else float(rsI / max(abs(lvA), 1e-9)),
                  "n_left": nl0, "n_right": nr0}
print("   D3 raw_step（窄带 ±%d 列，不做基线扣除）：无注入 %.4f（相对 %.5f）| 注入1.56 %.4f（相对 %.5f）" % (
    HW, rs0, res["D1_real"]["raw_step_rel_no_inj"], rsI, res["D1_real"]["raw_step_rel_inj156"]))
print("   生产判据 rel_step：无注入 %s | 注入1.56 %s | 沿边覆盖率 %.3f" % (
    res["D1_real"]["rel_step_no_inj"], res["D1_real"]["rel_step_inj156"], res["D1_real"]["coverage_no_inj"]))
json.dump(res, open(f"{OUT}/p5mJ_diag.json", "w"), indent=1)
print("写入 results/p5w/p5mJ_diag.json")
