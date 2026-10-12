"""乘性残留诊断：从已保存的逐帧粗格均值解逐帧、逐半径带的相对增益 s_k 及其不确定度，
并判定主导层（P1 归一 / 平场残差 / 天光电平相关）。

原理：在重叠粗格上，先按半径带去均值（去掉该带的加性偏置），再解零截距稳健比值
  s_ij = Σ x_i x_j / Σ x_j²      （x 为该带内的去均值格值）
⇒ 加性项被抵消，剩下的斜率就是两帧的**相对增益**；再按 log s_ij = log g_i − log g_j
（规范 Σ log g = 0，按 1/se² 加权）解出逐帧 g_k。
半径依赖（同一帧各带的 g 是否一致）判平场残差；两臂对照（cleaned 含 P1 电平、photoapplied 已归一）
判 P1 层；g−1 与帧天光电平的相关判「电平相关残差」。
"""
import json, os, sys
import numpy as np

OUT = os.path.dirname(os.path.abspath(__file__)) + "/../results/p5w"
CELL = 32
CD = 0.0005
NXC, NYC = 4352 // CELL, 6208 // CELL
NB = 6                      # 半径带数


def cell_radius():
    cy, cx = np.mgrid[0:NYC, 0:NXC]
    du = (cx.ravel() - (NXC - 1) / 2.0) * CELL * CD
    dv = (cy.ravel() - (NYC - 1) / 2.0) * CELL * CD
    return np.hypot(du, dv)


def solve_gains(cm, w, bands, min_cells=20):
    """解逐帧、逐带的相对增益。返回 g[nf, nb]、se[nf, nb]。"""
    nf, nc = cm.shape
    R = cell_radius()
    band_id = np.digitize(R, bands[1:-1])
    rows, cols, vals, rhs, ws = [], [], [], [], []
    pair_se = {}
    r = 0
    for b in range(len(bands) - 1):
        m_b = band_id == b
        for i in range(nf):
            for j in range(i + 1, nf):
                m = m_b & (w[i] > 0) & (w[j] > 0) & np.isfinite(cm[i]) & np.isfinite(cm[j])
                n = int(m.sum())
                if n < min_cells:
                    continue
                xi = cm[i][m] - np.mean(cm[i][m]); xj = cm[j][m] - np.mean(cm[j][m])
                den = float(np.sum(xj ** 2))
                if den <= 0:
                    continue
                s = float(np.sum(xi * xj) / den)
                if not np.isfinite(s) or s <= 0:
                    continue
                resid = xi - s * xj
                se = float(np.sqrt(np.sum(resid ** 2) / max(n - 1, 1) / den))
                pair_se[(b, i, j)] = (s, se, n)
                # log s = log g_i - log g_j
                rows.append(r); cols.append((b, i)); vals.append(1.0); ws.append(1.0 / max(se / s, 1e-6) ** 2)
                rows.append(r); cols.append((b, j)); vals.append(-1.0); ws.append(1.0 / max(se / s, 1e-6) ** 2)
                rhs.append(np.log(s)); r += 1
    # 组装（带 × 帧）未知量
    rid = []
    for b in range(len(bands) - 1):
        for i in range(nf):
            rid.append((b, i))
    idx = {k: t for t, k in enumerate(rid)}
    A = np.zeros((r, len(rid))); y = np.zeros(r)
    for t in range(r):
        A[t, idx[cols[2 * t]]] = vals[2 * t]; A[t, idx[cols[2 * t + 1]]] = vals[2 * t + 1]; y[t] = rhs[t]
    # 规范：每带 Σ_i log g_i = 0
    G = np.zeros((len(bands) - 1, len(rid)))
    for b in range(len(bands) - 1):
        for i in range(nf):
            G[b, idx[(b, i)]] = 1.0
    A = np.vstack([A, G * 1e-3]); y = np.concatenate([y, np.zeros(len(bands) - 1)])
    x, *_ = np.linalg.lstsq(A, y, rcond=None)
    g = np.zeros((nf, len(bands) - 1)); 
    for b in range(len(bands) - 1):
        for i in range(nf):
            g[i, b] = np.exp(x[idx[(b, i)]])
    # 不确定度：用残差做一次线性传播（近似）
    res = A @ x - y
    dof = max(r - len(rid), 1)
    s2 = float(np.sum(res ** 2) / dof)
    cov = s2 * np.linalg.pinv(A.T @ A)
    se = np.sqrt(np.maximum(np.diag(cov), 0))
    se_g = np.zeros_like(g)
    for b in range(len(bands) - 1):
        for i in range(nf):
            se_g[i, b] = g[i, b] * se[idx[(b, i)]]
    return g, se_g, pair_se


def main():
    nf = 43
    res = {}
    for arm in ("cleaned", "photoapplied"):
        cs = np.load(f"{OUT}/stage1_{arm}_cellsum.npy"); cw = np.load(f"{OUT}/stage1_{arm}_cellw.npy")
        cm = np.where(cw > 0, cs / np.maximum(cw, 1e-300), np.nan)
        R = cell_radius(); rmax = float(R.max())
        bands = np.linspace(0, rmax, NB + 1)
        g, seg, ps = solve_gains(cm, cw, bands)
        med = np.array([np.nanmedian(cm[i]) if np.isfinite(cm[i]).any() else np.nan for i in range(nf)])
        gm = np.median(g, axis=1); 
        entry = {"band_edges_deg": bands.tolist(), "g_median_all": gm.tolist(),
                 "g_per_band": g.tolist(), "se_per_band": seg.tolist(),
                 "frame_median_level": med.tolist(),
                 "g_spread_per_frame": (np.max(g, axis=1) - np.min(g, axis=1)).tolist(),
                 "g_radial_variation_median": float(np.median(np.max(g, axis=1) - np.min(g, axis=1)))}
        # 电平相关：g−1 与帧天光电平的相关
        ok = np.isfinite(gm) & np.isfinite(med)
        if ok.sum() > 3:
            entry["corr_g_minus1_vs_level"] = float(np.corrcoef(gm[ok] - 1, med[ok])[0, 1])
            A = np.vstack([med[ok], np.ones(ok.sum())]).T
            slope = np.linalg.lstsq(A, gm[ok] - 1, rcond=None)[0]
            entry["fit_g_minus1_on_level"] = {"slope_per_ADU": float(slope[0]), "intercept": float(slope[1])}
        res[arm] = entry
        print("== %s ==  帧相对增益 g_k：中位 %.5f  范围 %.5f–%.5f  (se 中位 %.1e)" % (
            arm, np.median(gm), gm.min(), gm.max(), np.median(seg)))
        print("   逐帧 g 的带间极差 中位 %.5f（半径依赖）| g−1 与帧天光电平相关 %.3f" % (
            entry["g_radial_variation_median"], entry.get("corr_g_minus1_vs_level", float("nan"))))
        top = np.argsort(-np.abs(gm - 1))[:6]
        print("   |g−1| 最大的 6 帧:", [(int(k), round(float(gm[k]), 4)) for k in top])
    json.dump(res, open(f"{OUT}/p5m_multiplicative_diag.json", "w"), indent=1)
    print("写入", f"{OUT}/p5m_multiplicative_diag.json")


if __name__ == "__main__":
    main()
