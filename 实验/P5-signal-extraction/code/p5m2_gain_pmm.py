"""PMM 式乘性差模解：从重叠样本对（规则分格中位对）解逐帧相对增益 g_k。

与已证伪的粗格比值法的区别（为什么这次可能有效）：
  粗格比值法用「去均值后的零截距比值」s=Σxy/Σy²，是方差比估计量，对「同一格内两帧覆盖子区域不同」
  极敏感（结构梯度直接变成假增益）；本脚本按 PMM 的做法拟合**带自由截距的直线** y = m·x + b
  （y=帧 l 的格中位、x=帧 k 的格中位），把逐帧加性偏置与格内采样噪声交给截距与残差承担，
  斜率才是相对增益。规范 Σ_k w_k log10 g_k = 0（公共增益按构造为 1）。
"""
import json, os, sys
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
REMOVE_FRAC = 0.10          # PMM 的 outlierRemovalPercent 同量级
MIN_CELLS = 30
KAPPA = 3.0                 # 幅度上界 |log10 g| <= KAPPA * sigma_resid


def robust_line(x, y, rounds=6):
    """带自由截距的稳健直线拟合（按垂距逐轮剔最差点），返回 (m, b, se_m, n)。"""
    m, n = len(x), 0
    keep = np.ones(m, dtype=bool)
    for _ in range(rounds):
        X, Y = x[keep], y[keep]
        if X.size < MIN_CELLS:
            break
        A = np.stack([X, np.ones_like(X)], axis=1)
        sol, *_ = np.linalg.lstsq(A, Y, rcond=None)
        slope, inter = sol
        resid = Y - (slope * X + inter)
        den = float(np.sum(X ** 2))
        if den <= 0:
            break
        se = float(np.sqrt(np.sum(resid ** 2) / max(X.size - 2, 1) / den))
        perp = np.abs(slope * X - Y + inter) / np.sqrt(slope ** 2 + 1)
        nrem = max(1, int(round(REMOVE_FRAC * keep.sum())))
        worst = np.argsort(-perp)[:nrem]
        idx = np.flatnonzero(keep)
        keep[idx[worst]] = False
        n = X.size
    X, Y = x[keep], y[keep]
    A = np.stack([X, np.ones_like(X)], axis=1)
    sol, *_ = np.linalg.lstsq(A, Y, rcond=None)
    slope, inter = sol
    resid = Y - (slope * X + inter)
    den = float(np.sum(X ** 2))
    se = float(np.sqrt(np.sum(resid ** 2) / max(X.size - 2, 1) / den)) if den > 0 else np.nan
    return float(slope), float(inter), se, int(X.size)


def main():
    res = {}
    for arm in ("cleaned", "photoapplied"):
        cs = np.load(f"{OUT}/stage1_{arm}_cellsum.npy"); cw = np.load(f"{OUT}/stage1_{arm}_cellw.npy")
        cm = np.where(cw > 0, cs / np.maximum(cw, 1e-300), np.nan)
        nf = cm.shape[0]
        pairs = []
        for i in range(nf):
            for j in range(i + 1, nf):
                m = np.isfinite(cm[i]) & np.isfinite(cm[j]) & (cw[i] > 0) & (cw[j] > 0)
                if m.sum() < MIN_CELLS:
                    continue
                m_, b_, se_, n_ = robust_line(cm[i][m], cm[j][m])
                if np.isfinite(m_) and 0.05 < m_ < 20:
                    pairs.append((i, j, m_, se_, n_))
        # 规范：Σ_k w_k log10 g_k = 0；log m_ij = log g_j - log g_i（cm_j ≈ m·cm_i ⇒ m = g_j/g_i）
        rows, cols, vals, rhs, ws = [], [], [], [], []
        wsum = np.array([np.nansum(cw[k]) for k in range(nf)])
        for t, (i, j, m_, se_, n_) in enumerate(pairs):
            se_log = max(se_ / m_, 1e-6)
            rows.append(t); cols.append(j); vals.append(1.0); ws.append(1.0 / se_log ** 2)
            rows.append(t); cols.append(i); vals.append(-1.0); ws.append(1.0 / se_log ** 2)
            rhs.append(np.log10(m_))
        A = np.zeros((len(pairs), nf)); y = np.zeros(len(pairs)); W = np.zeros(len(pairs))
        for t in range(len(pairs)):
            A[t, cols[2 * t]] = 1.0; A[t, cols[2 * t + 1]] = -1.0; y[t] = rhs[t]; W[t] = ws[2 * t]
        G = (wsum / wsum.sum())[None, :]
        Aw = np.vstack([A * np.sqrt(W)[:, None], G * 1e-6])
        yw = np.concatenate([y * np.sqrt(W), [0.0]])
        x, *_ = np.linalg.lstsq(Aw, yw, rcond=None)
        rank = int(np.linalg.matrix_rank(Aw))
        lg = x - np.average(x, weights=wsum)                      # 差模（加权均值为零）
        # 幅度上界（σ_resid 取该帧 P1 产物的场后空间残差）
        sig = np.full(nf, np.nan)
        for tag in ("t2", "t3"):
            d = json.load(open(f"{PDIR}/{tag}/p1_phot.json"))
            keys = list(d["photscale_detail"].keys())
            for t, k in enumerate(keys):
                pass
        # 帧顺序与 stage1 一致：按清单 input_light 基名
        order = []
        for tag in ("t2", "t3"):
            man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
            for e in man["frames"]:
                stem = os.path.basename(e["input_light"]).replace(".fts", "")
                d = json.load(open(f"{PDIR}/{tag}/p1_phot.json"))
                v = d["photscale_detail"].get(stem)
                order.append(np.nan if v is None else v["spatial_gain"].get("sigma_spatial_residual_dex"))
        order = [o for o in order if o is not None]
        sig = np.array([o if o is not None else np.nan for o in order][:nf], dtype=float)
        if sig.size < nf:
            sig = np.full(nf, np.nanmedian(sig) if np.isfinite(sig).any() else 0.02)
        cap = KAPPA * sig
        lg_raw = lg.copy()
        nclip = int(np.sum(np.abs(lg) > cap))
        lg = np.clip(lg, -cap, cap)
        g = 10 ** lg
        Sg = lg                                        # 差模向量即"公共增益残差"的内容
        redline = float(np.max(np.abs(Sg)) / max(np.sqrt(np.mean(Sg ** 2)), 1e-300))
        rms = float(np.sqrt(np.mean(lg ** 2)))
        sig_med = float(np.nanmedian(sig))
        res[arm] = {"n_pairs": len(pairs), "rank": rank, "n_unknowns": nf,
                    "log10_g_before_cap": lg_raw.tolist(), "log10_g_after_cap": lg.tolist(),
                    "g": g.tolist(), "sigma_resid_used": sig.tolist(), "cap": cap.tolist(),
                    "n_clipped": nclip, "clip_frac": nclip / float(nf),
                    "rms_log10_g": rms, "median_sigma_resid": sig_med,
                    "significant": bool(rms > sig_med / 3.0),
                    "redline_max_Sg_over_rms_Sg": redline,
                    "degenerate_identity": bool(redline <= 1e-12),
                    "pair_slope_stats": {"median": float(np.median([p[2] for p in pairs])),
                                         "p05": float(np.percentile([p[2] for p in pairs], 5)),
                                         "p95": float(np.percentile([p[2] for p in pairs], 95))}}
        print("== %s ==  对 %d  秩 %d/%d" % (arm, len(pairs), rank, nf))
        print("   log10 g 差模: RMS %.4f  (该帧 σ_resid 中位 %.4f) ⇒ 显著=%s" % (
            rms, sig_med, res[arm]["significant"]))
        print("   幅度上界 |log10 g|≤%.4f：截断 %d/%d 帧（%.0f%%）" % (
            float(np.median(cap)), nclip, nf, 100 * nclip / nf))
        print("   红线 max|S_g|/RMS(S_g) = %.4f  ⇒ 恒等退化为 %s" % (redline, res[arm]["degenerate_identity"]))
        print("   对斜率分布 中位 %.4f  [p05 %.4f, p95 %.4f]" % (
            res[arm]["pair_slope_stats"]["median"], res[arm]["pair_slope_stats"]["p05"],
            res[arm]["pair_slope_stats"]["p95"]))
        print("   g_k 范围 %.4f–%.4f" % (g.min(), g.max()))
    json.dump(res, open(f"{OUT}/p5m2_gain_pmm.json", "w"), indent=1)
    print("写入", f"{OUT}/p5m2_gain_pmm.json")


if __name__ == "__main__":
    main()
