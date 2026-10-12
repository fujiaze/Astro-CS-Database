"""几何来源改用该帧 p1_wcs.json 的 samples 表（64 条像素↔天球），不再用帧头 CD/CDELT。

口径源（可引用）：p1_wcs.json 明写 pixel_origin = "0-based array index (index-is-center);
FITS 1-based xp = x + 1" ⇒ 数组 0 基、整数索引即像素中心；头里 CRPIX/CRVAL 为 FITS 1 基。
做法：用 64 条 samples 拟合 (x,y)→(Δra·cosδ, Δdec) 的三次多项式（10 项），报拟合残差（角秒），
再用它换算 p1_flux.json 的星位；验收 = 同指向对 1″ 内共同星数出现量级跳变。
"""
import json, os, sys
import numpy as np
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")


def poly_terms(x, y, x0, y0, s):
    u = (x - x0) / s; v = (y - y0) / s
    return np.stack([np.ones_like(u), u, v, u * u, u * v, v * v,
                     u ** 3, u * u * v, u * v * v, v ** 3], axis=1)


def fit_geom(wj):
    w = json.load(open(wj)); s = w["samples"]
    x = np.array([p["x"] for p in s], float); y = np.array([p["y"] for p in s], float)
    ra = np.array([p["ra"] for p in s], float); dec = np.array([p["dec"] for p in s], float)
    ra0, dec0 = float(np.median(ra)), float(np.median(dec))
    x0, y0, sc = float(np.mean(x)), float(np.mean(y)), 2048.0
    B = poly_terms(x, y, x0, y0, sc)
    tx = (ra - ra0) * np.cos(np.radians(dec)); ty = dec - dec0
    cx, *_ = np.linalg.lstsq(B, tx, rcond=None)
    cy, *_ = np.linalg.lstsq(B, ty, rcond=None)
    rx = B @ cx - tx; ry = B @ cy - ty
    rms = float(np.sqrt(np.mean(rx ** 2 + ry ** 2)) * 3600)
    def f(xx, yy):
        Bq = poly_terms(np.asarray(xx, float), np.asarray(yy, float), x0, y0, sc)
        ra_q = ra0 + (Bq @ cx) / np.cos(np.radians(dec0 + Bq @ cy))
        return ra_q, dec0 + (Bq @ cy)
    return f, rms, {"ra0": ra0, "dec0": dec0, "n_samples": len(s),
                    "ctype": w.get("ctype1"), "sip_order": w.get("sip_order"),
                    "rms_px_json": w.get("rms_px"), "best_inliers": w.get("best_inliers"),
                    "poly_fit_rms_arcsec": rms}


def unit(ra, dec):
    ra, dec = np.radians(ra), np.radians(dec)
    return np.stack([np.cos(dec) * np.cos(ra), np.cos(dec) * np.sin(ra), np.sin(dec)], 1)


def main():
    res = {"geometry_source": "p1_wcs.json samples -> cubic polynomial", "tolerance_curve": {}, "gain": {}}
    for tag in ("t2", "t3"):
        man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
        phot = json.load(open(f"{PDIR}/{tag}/p1_phot.json"))["photscale_detail"]
        fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
        src = {fr["file"].replace("cleaned_", "").replace(".fts", ""):
               [r for r in fr["results"] if r.get("valid") and r["flux"] > 0] for fr in fl["frames"]}
        frames = []
        for e in man["frames"]:
            fid = e["frame_id"]; stem = os.path.basename(e["input_light"]).replace(".fts", "")
            wj = os.path.join(PDIR, tag, fid, "p1_wcs.json")
            if not os.path.exists(wj) or not src.get(stem):
                continue
            try:
                f, rms, meta = fit_geom(wj)
            except Exception as ex:
                print("  跳过（无 p1_wcs.json）:", stem[:40], type(ex).__name__); continue
            xy = np.array([[s["x"], s["y"]] for s in src[stem]])
            ra, dec = f(xy[:, 0], xy[:, 1])
            sg = phot.get(fid, {}).get("spatial_gain", {}) if isinstance(phot.get(fid), dict) else {}
            frames.append({"fid": fid, "stem": stem, "xyz": unit(ra, dec),
                           "flux": np.array([s["flux"] for s in src[stem]]),
                           "sigma": sg.get("sigma_spatial_residual_dex"),
                           "pointing": (stem.split("_M")[1][0] if "_M" in stem else "?"),
                           "geom_rms_arcsec": rms, "meta": meta})
        print("[%s] 帧 %d | 多项式拟合残差中位 %.4f″ | 例：%s" % (
            tag, len(frames), np.median([f["geom_rms_arcsec"] for f in frames]),
            json.dumps(frames[0]["meta"], ensure_ascii=False)[:120]))
        TOLS = (0.5, 1.0, 2.0, 3.0, 5.0)
        curve = {t: [] for t in TOLS}; samept = {t: [] for t in TOLS}
        for a in range(len(frames)):
            for b in range(a + 1, len(frames)):
                tr = cKDTree(frames[b]["xyz"]); d, idx = tr.query(frames[a]["xyz"], k=1)
                c = np.clip(np.sum(frames[a]["xyz"] * frames[b]["xyz"][idx], axis=1), -1, 1)
                sep = np.degrees(np.arccos(c)) * 3600
                for t in TOLS:
                    n = int(np.sum(sep < t)); curve[t].append(n)
                    if frames[a]["pointing"] == frames[b]["pointing"]:
                        samept[t].append(n)
        res["tolerance_curve"][tag] = {"all_median": {str(t): int(np.median(curve[t])) for t in TOLS},
                                       "same_pointing_median": {str(t): (int(np.median(samept[t])) if samept[t] else None) for t in TOLS},
                                       "n_pairs": len(curve[1.0]), "n_same_pointing": len(samept[1.0])}
        print("[%s] 1″共同星：全部对中位 %d | 同指向对中位 %d（验收判据：应为数百）" % (
            tag, int(np.median(curve[1.0])),
            int(np.median(samept[1.0])) if samept[1.0] else -1))
        print("     容差曲线 全部对中位 %s | 同指向对中位 %s" % (
            res["tolerance_curve"][tag]["all_median"], res["tolerance_curve"][tag]["same_pointing_median"]))
        # 解 g_k（TOL=1.0″）
        TOL = 1.0; pairs = []
        for a in range(len(frames)):
            for b in range(a + 1, len(frames)):
                tr = cKDTree(frames[b]["xyz"]); d, idx = tr.query(frames[a]["xyz"], k=1)
                c = np.clip(np.sum(frames[a]["xyz"] * frames[b]["xyz"][idx], axis=1), -1, 1)
                sep = np.degrees(np.arccos(c)) * 3600
                ok = np.flatnonzero(sep < TOL)
                if ok.size < 20: continue
                ratio = frames[a]["flux"][ok] / frames[b]["flux"][idx[ok]]
                med = np.median(ratio); keep = (ratio > med / 1.5) & (ratio < med * 1.5)
                rr = ratio[keep]
                if rr.size < 10: continue
                se = 1.4826 * np.median(np.abs(rr - np.median(rr))) / np.sqrt(rr.size)
                pairs.append((a, b, float(np.median(rr)), float(se), int(rr.size)))
        n = len(frames)
        if len(pairs) < n:
            print("[%s] 有效对 %d < 帧数 %d ⇒ 仍无法解" % (tag, len(pairs), n))
            res["gain"][tag] = {"n_pairs": len(pairs), "solved": False}
            continue
        A = np.zeros((len(pairs), n)); y = np.zeros(len(pairs)); W = np.zeros(len(pairs))
        for t, (a, b, m_, se_, cnt) in enumerate(pairs):
            A[t, b] = 1.0; A[t, a] = -1.0; y[t] = np.log10(m_); W[t] = 1.0 / max(se_ / m_, 1e-6) ** 2
        Aw = np.vstack([A * np.sqrt(W)[:, None], np.ones((1, n)) / n * 1e-6])
        yw = np.concatenate([y * np.sqrt(W), [0.0]])
        x, *_ = np.linalg.lstsq(Aw, yw, rcond=None)
        rank = int(np.linalg.matrix_rank(Aw)); lg = x - np.average(x)
        sig = np.array([frames[k]["sigma"] for k in range(n)], float)
        cap = 3 * sig; nclip = int(np.sum(np.abs(lg) > cap)); lg_c = np.clip(lg, -cap, cap)
        rms = float(np.sqrt(np.mean(lg ** 2))); sig_med = float(np.nanmedian(sig))
        red = float(np.max(np.abs(lg)) / max(rms, 1e-300)); sig_flag = bool(rms > sig_med / 3)
        res["gain"][tag] = {"n_pairs": len(pairs), "rank": rank, "n_unknowns": n, "full_rank": rank == n - 1,
                            "common_stars_median": int(np.median([p[4] for p in pairs])),
                            "log10_g": lg.tolist(), "log10_g_capped": lg_c.tolist(), "g": (10 ** lg_c).tolist(),
                            "n_clipped": nclip, "rms_log10_g": rms, "median_sigma_resid": sig_med,
                            "significant": sig_flag, "g_identity": (not sig_flag), "redline": red}
        print("[%s] 解：对 %d | 共同星中位 %d | 秩 %d/%d 满秩=%s" % (
            tag, len(pairs), int(np.median([p[4] for p in pairs])), rank, n, rank == n - 1))
        print("     log10 g RMS %.5f（σ_resid 中位 %.5f）显著=%s ⇒ g≡1 ? %s | 红线 %.4f | 截断 %d" % (
            rms, sig_med, sig_flag, not sig_flag, red, nclip))
    json.dump(res, open(f"{OUT}/p5m5_star_ratio_sip.json", "w"), indent=1)
    print("写入", f"{OUT}/p5m5_star_ratio_sip.json")


if __name__ == "__main__":
    main()
