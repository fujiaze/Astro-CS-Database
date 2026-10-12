"""PMM 首选路线：共同星通量比解逐帧相对增益（键归一 + 容差自检 + 红线）。

三件：
  1) σ_resid 用 manifest 的 frame_id（`_` 形式）查表，自检命中数=帧数且无 NaN；
  2) 共同星匹配容差自检 1″/2″/3″/5″，并用「同指向帧对」校准期望共同星数，确定像素口径；
  3) 解 log10 g（规范 Σw log10 g = 0）、报差模 RMS、红线 max|S_g|/RMS(S_g)、秩、显著性；
     不显著则输出 g ≡ 1。
"""
import json, os, sys
import numpy as np
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p5z_run as Z

PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
TOLS = (1.0, 2.0, 3.0, 5.0)


def unit(ra, dec):
    ra, dec = np.radians(ra), np.radians(dec)
    return np.stack([np.cos(dec) * np.cos(ra), np.cos(dec) * np.sin(ra), np.sin(dec)], 1)


def sep_arcsec(a, b, idx):
    d = np.clip(np.sum(a * b[idx], axis=1), -1, 1)
    return np.degrees(np.arccos(d)) * 3600


def main():
    res = {}
    for tag in ("t2", "t3"):
        man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
        phot = json.load(open(f"{PDIR}/{tag}/p1_phot.json"))["photscale_detail"]
        fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
        src = {}
        for fr in fl["frames"]:
            stem = fr["file"].replace("cleaned_", "").replace(".fts", "")
            src[stem] = [r for r in fr["results"] if r.get("valid") and r["flux"] > 0]
        frames = []
        for e in man["frames"]:
            fid = e["frame_id"]                      # `_` 形式（键归一口径）
            stem = os.path.basename(e["input_light"]).replace(".fts", "")
            p = os.path.join(PDIR, tag, f"cleaned_{stem}.fts")
            if not os.path.exists(p) or not src.get(stem):
                continue
            try:
                fw, _ = Z.frame_wcs(p)
            except Exception:
                continue
            sg = phot.get(fid, {}).get("spatial_gain", {}) if isinstance(phot.get(fid), dict) else {}
            frames.append({"fid": fid, "stem": stem, "path": p, "src": src[stem],
                           "sigma": sg.get("sigma_spatial_residual_dex"),
                           "pointing": None})
            m = stem.split("_M")
            frames[-1]["pointing"] = (m[1][0] if len(m) > 1 else "?")
        # 1) 键匹配自检
        hits = sum(1 for f in frames if f["sigma"] is not None)
        nnan = sum(1 for f in frames if f["sigma"] is None or not np.isfinite(f["sigma"]))
        res.setdefault("key_check", {})[tag] = {"n_frames": len(frames), "key_hits": hits,
                                               "n_missing_or_nan": nnan}
        print("[%s] 键自检：帧 %d，命中 %d，缺失/NaN %d" % (tag, len(frames), hits, nnan))
        # 星位
        for f in frames:
            xy = np.array([[s["x"], s["y"]] for s in f["src"]])
            f["flux"] = np.array([s["flux"] for s in f["src"]])
            fw, _ = Z.frame_wcs(f["path"])
            ra, dec = fw.all_pix2world(xy, 0).T
            f["xyz"] = unit(ra, dec)
        # 2) 容差曲线 + 同指向校准
        curve = {t: [] for t in TOLS}
        same_pt = {t: [] for t in TOLS}
        for a in range(len(frames)):
            for b in range(a + 1, len(frames)):
                tr = cKDTree(frames[b]["xyz"])
                d, idx = tr.query(frames[a]["xyz"], k=1)
                d = np.clip(np.sum(frames[a]["xyz"] * frames[b]["xyz"][idx], axis=1), -1, 1)
                sep = np.degrees(np.arccos(d)) * 3600
                for t in TOLS:
                    n = int(np.sum(sep < t))
                    curve[t].append(n)
                    if frames[a]["pointing"] == frames[b]["pointing"]:
                        same_pt[t].append(n)
        res.setdefault("tolerance_curve", {})[tag] = {
            "all_pairs_median": {str(t): int(np.median(curve[t])) for t in TOLS},
            "all_pairs_p90": {str(t): int(np.percentile(curve[t], 90)) for t in TOLS},
            "pairs_ge6": {str(t): int(np.sum(np.array(curve[t]) >= 6)) for t in TOLS},
            "same_pointing_median": {str(t): (int(np.median(same_pt[t])) if same_pt[t] else None) for t in TOLS},
            "n_pairs": len(curve[TOLS[0]]), "n_same_pointing_pairs": len(same_pt[TOLS[0]])}
        print("[%s] 容差曲线 全部对中位 %s | ≥6 共同星的对数 %s | 同指向对中位 %s" % (
            tag, res["tolerance_curve"][tag]["all_pairs_median"],
            res["tolerance_curve"][tag]["pairs_ge6"], res["tolerance_curve"][tag]["same_pointing_median"]))
        # 3) 用 TOL=2″ 解 g
        TOL = 2.0
        pairs = []
        for a in range(len(frames)):
            for b in range(a + 1, len(frames)):
                tr = cKDTree(frames[b]["xyz"]); d, idx = tr.query(frames[a]["xyz"], k=1)
                d = np.clip(np.sum(frames[a]["xyz"] * frames[b]["xyz"][idx], axis=1), -1, 1)
                sep = np.degrees(np.arccos(d)) * 3600
                ok = np.flatnonzero(sep < TOL)
                if ok.size < 20:
                    continue
                fi = frames[a]["flux"][ok]; fj = frames[b]["flux"][idx[ok]]
                ratio = fi / fj; med = np.median(ratio)
                keep = (ratio > med / 1.5) & (ratio < med * 1.5)
                rr = ratio[keep]
                if rr.size < 6:
                    continue
                se = 1.4826 * np.median(np.abs(rr - np.median(rr))) / np.sqrt(rr.size)
                if not np.isfinite(se) or se <= 0:
                    continue
                pairs.append((a, b, float(np.median(rr)), float(se), int(rr.size)))
        n = len(frames)
        if len(pairs) < n:
            print("[%s] 有效对 %d < 帧数 %d ⇒ 无法解" % (tag, len(pairs), n))
            res.setdefault("gain", {})[tag] = {"n_pairs": len(pairs), "solved": False}
            continue
        A = np.zeros((len(pairs), n)); y = np.zeros(len(pairs)); W = np.zeros(len(pairs))
        for t, (a, b, m_, se_, cnt) in enumerate(pairs):
            A[t, b] = 1.0; A[t, a] = -1.0
            y[t] = np.log10(m_); W[t] = 1.0 / max(se_ / m_, 1e-6) ** 2
        Aw = np.vstack([A * np.sqrt(W)[:, None], np.ones((1, n)) / n * 1e-6])
        yw = np.concatenate([y * np.sqrt(W), [0.0]])
        x, *_ = np.linalg.lstsq(Aw, yw, rcond=None)
        rank = int(np.linalg.matrix_rank(Aw))
        lg = x - np.average(x)
        sig = np.array([frames[k]["sigma"] for k in range(n)], dtype=float)
        cap = 3 * sig
        nclip = int(np.sum(np.abs(lg) > cap))
        lg_c = np.clip(lg, -cap, cap)
        rms = float(np.sqrt(np.mean(lg ** 2)))
        sig_med = float(np.nanmedian(sig))
        red = float(np.max(np.abs(lg)) / max(rms, 1e-300))
        sig_flag = bool(rms > sig_med / 3.0)
        out = {"n_pairs": len(pairs), "rank": rank, "n_unknowns": n, "full_rank": rank == n - 1,
               "log10_g": lg.tolist(), "log10_g_capped": lg_c.tolist(), "g": (10 ** lg_c).tolist(),
               "sigma_resid": sig.tolist(), "cap": cap.tolist(), "n_clipped": nclip,
               "rms_log10_g": rms, "median_sigma_resid": sig_med,
               "significant": sig_flag, "redline": red,
               "g_identity": (not sig_flag), "common_stars_median": int(np.median([p[4] for p in pairs]))}
        res.setdefault("gain", {})[tag] = out
        print("[%s] TOL=2″：对 %d | 共同星中位 %d | 秩 %d/%d 满秩=%s" % (
            tag, len(pairs), out["common_stars_median"], rank, n, out["full_rank"]))
        print("      log10 g RMS %.5f（σ_resid 中位 %.5f）显著=%s ⇒ g≡1 ? %s" % (
            rms, sig_med, sig_flag, out["g_identity"]))
        print("      红线 max|S_g|/RMS(S_g) = %.4f | 截断 %d 帧 | g 范围 %.5f–%.5f" % (
            red, nclip, (10 ** lg_c).min(), (10 ** lg_c).max()))
    json.dump(res, open(f"{OUT}/p5m4_star_ratio.json", "w"), indent=1)
    print("写入", f"{OUT}/p5m4_star_ratio.json")


if __name__ == "__main__":
    main()
