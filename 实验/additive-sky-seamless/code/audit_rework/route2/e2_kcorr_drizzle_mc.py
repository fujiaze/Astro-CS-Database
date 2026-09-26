#!/usr/bin/env python3
"""E2: k_corr of drizzled samples (frozen default 1.4, project MC claim 1.3883 @ pixfrac=0.8).

docs/science/PHASE2_UPM.md §4/§5: k_corr > 1 (domain), N_eff = N_retained / k_corr;
frozen default 1.4 from project MC (pixfrac=0.8, 2000 iters, measured 1.3883).

Design (independent re-implementation; the original MC test's exact geometry is not
fully documented => we test the qualitative claim k_corr > 1 and its magnitude):
  - input pixel grid, iid N(0,1) pixel values (sky-subtracted noise only);
  - output grid oversampled by factor F, drizzle with pixfrac p=0.8
    (shrunken input footprint overlap weights, flux-conserving normalized sum);
  - a "control patch" = F^2 adjacent output pixels = one input pixel of area,
    N_retained = F^2; k_corr_est = Var(median_patch) / ((pi/2) * sigma_bg^2 / N).
NEGATIVE (no-effect => zero): uncorrelated reference branch (median of raw input
pixels, no resampling) must give k_corr_est - 1 == 0 within MC CI.
Standalone: pure python3 + numpy. SEED fixed.
Run: python3 e2_kcorr_drizzle_mc.py
"""
import json, math, os
import numpy as np

SEED = 20250926
PIXFRAC = 0.8
N_ITER = 2000
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e2_kcorr_drizzle_mc.json")

def drizzle_frame(rng, G, F):
    """G x G input pixels (unit size) -> (G*F) x (G*F) output pixels, pixfrac PIXFRAC."""
    d = rng.standard_normal((G, G))
    # shrunken input footprint: side PIXFRAC centered at input centers
    lo = (np.arange(G) + 0.5) - PIXFRAC / 2.0   # lower edge per input index
    hi = (np.arange(G) + 0.5) + PIXFRAC / 2.0
    out = np.zeros((G * F, G * F))
    wsum = np.zeros((G * F, G * F))
    o = (np.arange(G * F) + 0.5) / F            # output centers in input units
    oe = np.arange(G * F + 1) / F               # output edges
    for i in range(G):
        # vertical overlap of input i with every output row
        oy0 = np.clip(oe[:-1], lo[i], hi[i]); oy1 = np.clip(oe[1:], lo[i], hi[i])
        wy = np.maximum(oy1 - oy0, 0.0)          # (G*F,)
        rows = np.nonzero(wy > 0)[0]
        for j in range(G):
            ox0 = np.clip(oe[:-1], lo[j], hi[j]); ox1 = np.clip(oe[1:], lo[j], hi[j])
            wx = np.maximum(ox1 - ox0, 0.0)
            cols = np.nonzero(wx > 0)[0]
            if len(rows) == 0 or len(cols) == 0:
                continue
            w = np.outer(wy[rows], wx[cols])
            out[np.ix_(rows, cols)] += w * d[i, j]
            wsum[np.ix_(rows, cols)] += w
    return out / np.maximum(wsum, 1e-300)

def patch_medians(img, F, n_patch):
    """disjoint patches of F^2 adjacent output pixels (one input-pixel area each)."""
    H = img.shape[0]
    med = np.empty(n_patch)
    k = 0
    for by in range(0, H, F):
        for bx in range(0, H, F):
            if k >= n_patch:
                break
            med[k] = np.median(img[by:by + F, bx:bx + F])
            k += 1
    return med[:k]

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "pixfrac": PIXFRAC, "n_iter": N_ITER, "cases": []}
    for F in (2, 3, 4):
        G = 24
        meds = []
        allsamp = []
        for _ in range(N_ITER):
            img = drizzle_frame(rng, G, F)
            allsamp.append(img)
            meds.append(patch_medians(img, F, (G * G) // 4))
        pool = np.concatenate(meds)
        N = F * F
        s_bg = 1.4826 * np.median(np.abs(np.concatenate([a.ravel() for a in allsamp[::50]]) -
                                         np.median(np.concatenate([a.ravel() for a in allsamp[::50]]))))
        var_med = float(np.var(pool))
        k_est = var_med / ((math.pi / 2.0) * s_bg ** 2 / N)
        # lag-1 correlation of output pixels
        a = allsamp[0]
        dx = np.corrcoef(a[:, :-1].ravel(), a[:, 1:].ravel())[0, 1]
        dy = np.corrcoef(a[:-1, :].ravel(), a[1:, :].ravel())[0, 1]
        # mean-inflation check on the same pooled patches
        res["cases"].append({"F": F, "N_retained": N, "sigma_bg_robust": float(s_bg),
                             "var_median": var_med, "k_corr_est": float(k_est),
                             "lag1_corr": float((dx + dy) / 2.0),
                             "N_eff": N / k_est})
    # NEGATIVE: uncorrelated reference branch (raw input pixels, no resampling)
    R, Nref = 200_000, 25
    x = rng.standard_normal((R, Nref))
    k_ref = float(np.var(np.median(x, axis=1)) / ((math.pi / 2.0) / Nref))
    res["negative_uncorrelated"] = {"branch": "raw input pixels", "N": Nref,
                                    "k_corr_est": k_ref, "k_minus_1": k_ref - 1.0}
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))

if __name__ == "__main__":
    main()
