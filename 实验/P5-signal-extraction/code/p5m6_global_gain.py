"""全局测光 g_k：P1 的 k_photo 是**绝对锚定**（每帧系到 Gaia 同一测光系）⇒ 逐帧相对增益
g_k = k_photo_k / k_photo_ref 不需要跨帧重叠，6 个拼接分量因此可被一次连起来。

同时用「同一指向组内」的共同星通量比（权威几何 p1_wcs.json samples + 1″匹配）做**独立交叉验证**：
组内比值应等于 k_photo 比值（若一致 ⇒ 测光路线成立）。
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
KAPPA = 3.0

res = {}
kp, sig, stems, tags = [], [], [], []
for tag in ("t2", "t3"):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    phot = json.load(open(f"{PDIR}/{tag}/p1_phot.json"))["photscale_detail"]
    for e in man["frames"]:
        fid = e["frame_id"]
        v = phot.get(fid)
        if not v: continue
        kp.append(v["k_photo"]); sig.append(v["spatial_gain"].get("sigma_spatial_residual_dex"))
        stems.append(fid); tags.append(tag)
kp = np.array(kp); sig = np.array(sig, dtype=float)
nf = kp.size
lg = np.log10(kp) - np.average(np.log10(kp))
cap = KAPPA * sig
nclip = int(np.sum(np.abs(lg) > cap))
lgc = np.clip(lg, -cap, cap)
rms = float(np.sqrt(np.mean(lg ** 2)))
red = float(np.max(np.abs(lg)) / max(rms, 1e-300))
res["global_gain_from_kphoto"] = {
    "n_frames": int(nf), "rank": int(nf), "full_rank": True,
    "note": "逐帧独立（绝对锚定）⇒ 设计阵为单位阵，秩 = 帧数",
    "rms_log10_g": rms, "redline_max_Sg_over_rms_Sg": red,
    "n_clipped": nclip, "clip_frac": nclip / float(nf),
    "median_sigma_resid": float(np.nanmedian(sig)),
    "significant": bool(rms > np.nanmedian(sig) / 3.0),
    "log10_g": lg.tolist(), "log10_g_capped": lgc.tolist(), "g": (10 ** lgc).tolist(),
    "stems": stems, "tags": tags}
print("== 全局测光 g_k（绝对锚定）==")
print("   帧 %d | 秩 %d（=帧数，满秩）" % (nf, nf))
print("   log10 g 差模 RMS %.5f | σ_resid 中位 %.5f | 显著=%s" % (rms, np.nanmedian(sig), res["global_gain_from_kphoto"]["significant"]))
print("   红线 max|S_g|/RMS(S_g) = %.4f | 3σ 截断 %d 帧（%.0f%%）" % (red, nclip, 100 * nclip / nf))
print("   g 范围 %.4f–%.4f (%+.1f%% … %+.1f%%)" % (10 ** lgc.min(), 10 ** lgc.max(), 100 * (10 ** lgc.min() - 1), 100 * (10 ** lgc.max() - 1)))

# 交叉验证：同指向组内共同星比 vs k_photo 比
print("\n== 交叉验证：组内共同星通量比 vs k_photo 比 ==")
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
        f, _, _ = m5.fit_geom(wj)
        xy = np.array([[s["x"], s["y"]] for s in src[stem]])
        ra, dec = f(xy[:, 0], xy[:, 1])
        F.append({"stem": stem, "fid": fid, "xyz": m5.unit(ra, dec),
                  "flux": np.array([s["flux"] for s in src[stem]]),
                  "kp": phot[fid]["k_photo"], "pt": stem.split("_M")[1][0]})
    devs = []
    for a in range(len(F)):
        for b in range(a + 1, len(F)):
            if F[a]["pt"] != F[b]["pt"]: continue
            tr = cKDTree(F[b]["xyz"]); d, idx = tr.query(F[a]["xyz"], k=1)
            c = np.clip(np.sum(F[a]["xyz"] * F[b]["xyz"][idx], axis=1), -1, 1)
            sep = np.degrees(np.arccos(c)) * 3600
            ok = np.flatnonzero(sep < 1.0)
            if ok.size < 50: continue
            r = F[a]["flux"][ok] / F[b]["flux"][idx[ok]]
            med = np.median(r); keep = (r > med / 1.5) & (r < med * 1.5)
            ratio_star = float(np.median(r[keep]))
            ratio_kp = F[a]["kp"] / F[b]["kp"]
            devs.append(np.log10(ratio_star / ratio_kp))
    if devs:
        devs = np.array(devs)
        res.setdefault("cross_check", {})[tag] = {"n_pairs": int(devs.size),
                                                  "median_dex": float(np.median(devs)),
                                                  "p16": float(np.percentile(devs, 16)),
                                                  "p84": float(np.percentile(devs, 84)),
                                                  "rms_dex": float(np.sqrt(np.mean(devs ** 2)))}
        print("   [%s] 对 %d：log10(星比比 / k_photo 比) 中位 %+.5f dex，p16/p84 %+.5f/%+.5f，RMS %.5f dex" % (
            tag, devs.size, np.median(devs), np.percentile(devs, 16), np.percentile(devs, 84),
            np.sqrt(np.mean(devs ** 2))))
    else:
        print("   [%s] 无可比帧对" % tag)
json.dump(res, open(f"{OUT}/p5m6_global_gain.json", "w"), indent=1)
print("\n写入", f"{OUT}/p5m6_global_gain.json")
