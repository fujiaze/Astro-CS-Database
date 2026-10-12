"""N1–N3：高重叠箱（共同星重叠率≥0.8）上的差模/共模分离、空间结构、PMM 式线性拟合。
几何：帧头 SIP + p1_wcs.json 的 sip；配对机器沿用本单元既有实现。PMM 只读参考其 LinearFitData 思路。
"""
import json, os, sys
import numpy as np
from scipy.spatial import cKDTree
from matplotlib.path import Path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location("mD", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5mD_sip_kphoto.py"))
mD = importlib.util.module_from_spec(spec)
try: spec.loader.exec_module(mD)
except SystemExit: pass
sip_wcs, unit = mD.sip_wcs, mD.unit
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
TOL, WIN, RATE_MIN, MINM = 1.0, 1.5, 0.8, 20
NBOOT = 2000


def pmm_fit(fa, fb):
    """PMM 思路：F_b = m*F_a + b，通量窗 ±WIN + 按垂距剔离群（只读参考其做法，本实现自写）。"""
    A = np.stack([fa, np.ones_like(fa)], 1)
    s, *_ = np.linalg.lstsq(A, fb, rcond=None); m0 = s[0]
    keep = (fb / np.maximum(fa, 1e-30) > m0 / WIN) & (fb / np.maximum(fa, 1e-30) < m0 * WIN)
    fa, fb = fa[keep], fb[keep]
    for _ in range(5):
        if fa.size < MINM: break
        A = np.stack([fa, np.ones_like(fa)], 1)
        s, *_ = np.linalg.lstsq(A, fb, rcond=None); m, b = s
        perp = np.abs(m * fa - fb + b) / np.sqrt(m * m + 1)
        nrem = max(1, int(round(0.1 * fa.size)))
        k = np.argsort(-perp)[:nrem]
        fa = np.delete(fa, k); fb = np.delete(fb, k)
    if fa.size < 4: return None
    A = np.stack([fa, np.ones_like(fa)], 1)
    s, *_ = np.linalg.lstsq(A, fb, rcond=None)
    resid = fb - A @ s
    den = float(np.sum(fa ** 2))
    se = float(np.sqrt(np.sum(resid ** 2) / max(fa.size - 2, 1) / den)) if den > 0 else np.nan
    return float(s[0]), float(s[1]), se, int(fa.size)


res = {}
for tag in ("t2", "t3"):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
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
        F.append({"fid": fid, "pt": stem.split("_M")[1][0], "xy": xy, "W": W,
                  "ra": ra, "dec": dec, "flux": np.array([s["flux"] for s in r]), "xyz": unit(ra, dec)})
    ratios, pmms, pts, se3, comode = [], [], [], [], {f["fid"]: [] for f in F}
    for a in range(len(F)):
        for b in range(a + 1, len(F)):
            A, B = F[a], F[b]
            n = 4095.0; bb = []
            for t in np.linspace(0, 1, 21): bb.append((t * n, 0))
            for t in np.linspace(0, 1, 21): bb.append((n, t * n))
            for t in np.linspace(1, 0, 21): bb.append((t * n, n))
            for t in np.linspace(1, 0, 21): bb.append((0, t * n))
            bb = np.array(bb); rbp, dbp = B["W"].all_pix2world(bb[:, 0], bb[:, 1], 0)
            ra0, dec0 = float(np.median(rbp)), float(np.median(dbp))
            poly = Path(np.stack([(rbp - ra0) * np.cos(np.radians(dbp)), dbp - dec0], 1))
            ins = poly.contains_points(np.stack([(A["ra"] - ra0) * np.cos(np.radians(A["dec"])), A["dec"] - dec0], 1))
            n_exp = int(ins.sum())
            if n_exp < 30: continue
            tr = cKDTree(B["xyz"]); d, idx = tr.query(A["xyz"], k=1)
            c = np.clip(np.sum(A["xyz"] * B["xyz"][idx], axis=1), -1, 1)
            sep = np.degrees(np.arccos(c)) * 3600
            ok = np.flatnonzero(sep < TOL)
            if ok.size < MINM: continue
            rate = ok.size / n_exp
            if rate < RATE_MIN: continue
            fa = A["flux"][ok]; fb = B["flux"][idx[ok]]
            ratio = fa / fb
            med = float(np.median(ratio))
            se = float(1.4826 * np.median(np.abs(ratio - med)) / np.sqrt(ratio.size))
            ratios.append({"a": A["fid"][:28], "b": B["fid"][:28], "same": A["pt"] == B["pt"],
                           "n": int(ratio.size), "rate": rate, "median": med, "se": se,
                           "rms": float(np.sqrt(np.mean((ratio - med) ** 2)))})
            se3.append(3 * se)
            # 共模：几何均值相对各帧标度
            gm = np.sqrt(fa * fb)
            comode[A["fid"]].append(np.median(fa / gm)); comode[B["fid"]].append(np.median(fb / gm))
            p = pmm_fit(fa, fb)
            if p: pmms.append({"a": A["fid"][:28], "b": B["fid"][:28], "m": p[0], "b": p[1], "se": p[2], "n": p[3]})
            # N2 空间结构：log10(ratio) 对切平面坐标的低阶面
            u = (A["ra"][ok] - ra0) * np.cos(np.radians(A["dec"][ok])) * 3600
            v = (A["dec"][ok] - dec0) * 3600
            lr = np.log10(ratio)
            order = 1 if ratio.size < 80 else 2
            cols = [np.ones_like(u), u, v] + ([u * u, u * v, v * v] if order == 2 else [])
            Bm = np.stack(cols, 1)
            coef, *_ = np.linalg.lstsq(Bm, lr, rcond=None)
            fit = Bm @ coef; resid = lr - fit
            # 面峰谷（在样本点上的极差）
            pts.append({"a": A["fid"][:28], "b": B["fid"][:28], "order": order, "n": int(ratio.size),
                        "ptp_dex": float(np.max(fit) - np.min(fit)), "resid_rms_dex": float(np.sqrt(np.mean(resid ** 2))),
                        "resid_rms_out": float(np.sqrt(np.mean((lr - np.mean(lr)) ** 2)))})
    rat = np.array([r["median"] for r in ratios]); se_all = np.array(se3)
    bs = np.array([np.median(np.random.default_rng(1).choice(rat, rat.size, replace=True)) for _ in range(NBOOT)]) if rat.size else np.array([])
    res[tag] = {
        "n_pairs": len(ratios),
        "diff_mode": {"median": float(np.median(rat)), "median_ci95": [float(np.percentile(bs, 2.5)), float(np.percentile(bs, 97.5))] if bs.size else None,
                      "rms": float(np.sqrt(np.mean((rat - 1) ** 2))), "p16": float(np.percentile(rat, 16)), "p84": float(np.percentile(rat, 84)),
                      "min": float(rat.min()), "max": float(rat.max())},
        "detect_limit_3sigma_median": float(np.median(se_all)), "detect_limit_3sigma_p16": float(np.percentile(se_all, 16)),
        "vs_synth_5p89e-4": float(np.median(np.abs(rat - 1)) / 5.89e-4),
        "pmm": {"n": len(pmms), "m_median": float(np.median([p["m"] for p in pmms])), "m_rms": float(np.sqrt(np.mean((np.array([p["m"] for p in pmms]) - 1) ** 2))),
                "m_p16": float(np.percentile([p["m"] for p in pmms], 16)), "m_p84": float(np.percentile([p["m"] for p in pmms], 84))} if pmms else None,
        "spatial": {"n": len(pts), "order1": sum(1 for p in pts if p["order"] == 1), "order2": sum(1 for p in pts if p["order"] == 2),
                    "ptp_dex_median": float(np.median([p["ptp_dex"] for p in pts])), "ptp_dex_p90": float(np.percentile([p["ptp_dex"] for p in pts], 90)),
                    "resid_rms_dex_median": float(np.median([p["resid_rms_dex"] for p in pts])),
                    "raw_rms_dex_median": float(np.median([p["resid_rms_out"] for p in pts]))} if pts else None,
        "common_mode": {fid: {"n": len(v), "dev_from_1_median": float(np.median(v) - 1)} for fid, v in comode.items() if v},
        "pairs": ratios[:6], "pmm_pairs": pmms[:4], "spatial_pairs": pts[:4]}
    print("[%s] 高重叠对 %d | 差模中位 %.5f [%.5f,%.5f] | 偏离 RMS %.5f | p16/p84 %.5f/%.5f" % (
        tag, len(rat), np.median(rat), np.percentile(bs, 2.5), np.percentile(bs, 97.5),
        np.sqrt(np.mean((rat - 1) ** 2)), np.percentile(rat, 16), np.percentile(rat, 84)))
    print("   3σ 检测限中位 %.5f | 与合成 5.89e-4 之比 %.1f×" % (np.median(se_all), np.median(np.abs(rat - 1)) / 5.89e-4))
    if pmms:
        mm = np.array([p["m"] for p in pmms])
        print("   PMM 线性拟合 m：中位 %.5f | RMS(偏离 1) %.5f | p16/p84 %.5f/%.5f" % (
            np.median(mm), np.sqrt(np.mean((mm - 1) ** 2)), np.percentile(mm, 16), np.percentile(mm, 84)))
    if pts:
        print("   空间面：阶1 %d/阶2 %d | 面峰谷中位 %.4f dex (p90 %.4f) | 基外残余 RMS 中位 %.4f dex | 原始 RMS %.4f" % (
            sum(1 for p in pts if p["order"] == 1), sum(1 for p in pts if p["order"] == 2),
            np.median([p["ptp_dex"] for p in pts]), np.percentile([p["ptp_dex"] for p in pts], 90),
            np.median([p["resid_rms_dex"] for p in pts]), np.median([p["resid_rms_out"] for p in pts])))
json.dump(res, open(f"{OUT}/p5mG_mult_main.json", "w"), indent=1)
print("写入 results/p5w/p5mG_mult_main.json")
