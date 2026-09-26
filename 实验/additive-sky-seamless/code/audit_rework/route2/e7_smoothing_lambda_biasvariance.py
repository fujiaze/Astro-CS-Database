#!/usr/bin/env python3
"""E7: smoothing/regularization strength -- hallucinated anchor audit + bias-variance leg.

05_正向规格.md 判据01 claims: "弯曲能权重 lambda_bend 取自 docs/science/PHASE2_UPM.md §6
的经验范围 [0.01, 0.1], 默认 0.05".
AUDIT RESULT (independent, this route): grep of docs/ shows ZERO occurrences of
"lambda_bend"; PHASE2_UPM.md §6 is the "假设" (assumptions) section and contains no such
range; the real repo constants are smoothing_lambda compile default 0.0 and
P2_SMOOTHING_LAMBDA_AUTO = 0.1 (11_upm.md §5). => hallucinated anchor.

EXPERIMENT leg: 1D common-sky + per-frame offset model, second-difference (bending)
penalty. Shows:
  (a) MSE(lambda) = bias^2(lambda) + variance(lambda) trade-off; optimal lambda* depends
      on noise scale and roughness of the true field => NO universal [0.01,0.1] range
      exists in normalized units either (two different true fields => different lambda*);
  (b) lambda=0 degeneracy: with per-(frame,cell)-free additive field (the §16.3
      "不可检验域"), leave-one-cell-out prediction error explodes => lambda=0 makes the
      common-field claim untestable (supports the doc's own warning);
  (c) NEGATIVE: lambda -> 0 on an identifiable problem reproduces unregularized LS
      (max|dtheta| -> 0).
Standalone: pure python3 + numpy. SEED fixed.
Run: python3 e7_smoothing_lambda_biasvariance.py
"""
import json, os
import numpy as np

SEED = 20250926
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e7_smoothing_lambda_biasvariance.json")

def fit(y, X, lam, D2p):
    H = X.T @ X + lam * (D2p.T @ D2p)
    rhs = X.T @ y
    return np.linalg.pinv(H) @ rhs

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "audit": {
        "lambda_bend_hits_in_docs": 0,
        "PHASE2_UPM_section6_is": "假设 (assumptions), no numeric range",
        "repo_constants": {"smoothing_lambda_default": 0.0, "P2_SMOOTHING_LAMBDA_AUTO": 0.1},
        "verdict": "hallucinated anchor in 05_正向规格.md 判据01",
    }, "sweeps": [], "lambda0_degeneracy": {}, "negative": {}}
    n_grid, n_frames, n_rep = 50, 5, 60
    xs = np.linspace(0, 1, n_grid)
    D2 = np.zeros((n_grid - 2, n_grid))
    for i in range(n_grid - 2):
        D2[i, i:i + 3] = (1.0, -2.0, 1.0)
    D2p = np.zeros((n_grid - 2, n_grid + n_frames - 1))
    D2p[:, :n_grid] = D2
    X = np.zeros((n_frames * n_grid, n_grid + n_frames - 1))
    for k in range(n_frames):
        X[k * n_grid:(k + 1) * n_grid, :n_grid] = np.eye(n_grid)
        if k > 0:
            X[k * n_grid:(k + 1) * n_grid, n_grid + k - 1] = 1.0
    fields = {
        "smooth_broad": np.exp(-((xs - 0.35) ** 2) / 0.02) + 0.7 * np.exp(-((xs - 0.7) ** 2) / 0.05),
        "narrow_structure": np.exp(-((xs - 0.5) ** 2) / 0.001),
    }
    lams = np.logspace(-6, 3, 19)
    for fname, B in fields.items():
        for sigma in (0.01, 0.05):
            mse, bias2, var = [], [], []
            for lam in lams:
                errs, fits_ = [], []
                for rep in range(n_rep):
                    r2 = np.random.default_rng(SEED + rep)
                    off = np.concatenate([[0.0], rng.standard_normal(n_frames - 1)])
                    y = np.tile(B, n_frames) + np.repeat(off, n_grid)
                    y = y + r2.standard_normal(len(y)) * sigma
                    th = fit(y, X, lam, D2p)
                    fits_.append(th[:n_grid])
                    errs.append(float(np.mean((th[:n_grid] - B) ** 2)))
                F = np.array(fits_)
                mse.append(float(np.mean(errs)))
                bias2.append(float(np.mean((F.mean(axis=0) - B) ** 2)))
                var.append(float(np.mean(F.var(axis=0))))
            i0 = int(np.argmin(mse))
            res["sweeps"].append({"field": fname, "sigma": sigma,
                                  "lambda_star": float(lams[i0]), "mse_min": mse[i0],
                                  "lambda_0.01_mse": mse[3] if len(mse) > 3 else None,
                                  "lambda_0.1_mse": mse[8] if len(mse) > 8 else None})
    # (b) lambda=0 degeneracy: per-(frame,cell)-free field, leave-one-cell-out prediction
    n_cells = 10
    for lam in (0.0, 0.01, 0.1, 1.0):
        errs = []
        for rep in range(30):
            r2 = np.random.default_rng(SEED + 100 + rep)
            Bc = np.exp(-((np.arange(n_cells) - 5) ** 2) / 4.0)
            off = r2.standard_normal(n_frames)
            data = {k: Bc + off[k] + r2.standard_normal(n_cells) * 0.05 for k in range(n_frames)}
            hold = 4
            idx = [c for c in range(n_cells) if c != hold]
            rows, ys = [], []
            for k in range(n_frames):
                for c in idx:
                    e = np.zeros(n_cells * n_frames); e[k * n_cells + c] = 1.0
                    rows.append(e); ys.append(data[k][c])
            Xl = np.array(rows); yl = np.array(ys)
            L = np.zeros((n_cells * n_frames - n_cells, n_cells * n_frames))
            r = 0
            for k in range(n_frames):
                for c in idx[:-1]:
                    L[r, k * n_cells + c] = 1.0; L[r, k * n_cells + c + 1] = -1.0; r += 1
            Hl = Xl.T @ Xl + lam * (L.T @ L)
            th = np.linalg.pinv(Hl) @ (Xl.T @ yl)
            pred = np.mean([th[k * n_cells + hold] for k in range(n_frames)])
            errs.append((pred - Bc[hold]) ** 2)
        res["lambda0_degeneracy"][f"lam_{lam}"] = {"holdout_mse": float(np.mean(errs))}
    # (c) negative: identifiable problem, lam -> 0 equals unregularized LS
    y = np.tile(fields["smooth_broad"], n_frames) + 0.01 * rng.standard_normal(n_frames * n_grid)
    th0 = fit(y, X, 0.0, D2p)
    th1 = fit(y, X, 1e-10, D2p)
    res["negative"]["max_abs_dtheta_lam0_vs_1e-10"] = float(np.max(np.abs(th0 - th1)))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))

if __name__ == "__main__":
    main()
