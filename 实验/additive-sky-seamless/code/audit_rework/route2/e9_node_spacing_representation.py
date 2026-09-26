#!/usr/bin/env python3
"""E9: node-spacing representation boundary (PHASE2_UPM.md §7a rules 1-3; 11_upm.md §4.2).

Claims audited:
  - "无接缝 <=> 公共面可表示"; node spacing h must be DERIVED from input geometry
    (upper bound = min(overlap-band width, pointing separation)/2), not a constant;
  - when the frame-to-frame sky difference contains a coherent component at scale
    s <~ 2h, the residual seam grows (doc: scale scan 1600 -> 50 px grew seam x5.07);
  - 05_正向规格.md §9's alpha=0.7 / beta=2.0 / h_absolute_min=0.01deg cite
    "11_upm.md §4.2 (节点密度约束)" -- AUDIT: that section exists but contains NO such
    constants (spacing comes from input geometry; fail-closed when geometry missing)
    => hallucinated anchor; alpha/beta are performance heuristics, not science quantities.

EXPERIMENT leg (2D, bilinear node grid, h = 128 px):
  fit the common plane B_ref (bilinear nodes) + per-frame plane delta_k to two frames
  whose sky difference is a coherent sinusoid of wavelength s (amplitude 1), then
  measure the post-correction seam level step across a synthetic frame boundary.
  (a) seam residual vs s for s in {1600, 800, 400, 200, 100, 50} px -> growth ratio;
  (b) NEGATIVE (truth-no-effect => metric zero): difference field exactly representable
      on the node grid (s -> bilinear-exact) => seam residual ~ 0.
Standalone: pure python3 + numpy. SEED fixed.
Run: python3 e9_node_spacing_representation.py
"""
import json, os
import numpy as np

SEED = 20250926
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e9_node_spacing_representation.json")

H_IMG = 512
W_IMG = 1600
X_BND = 800          # synthetic frame boundary column
H_NODE = 128.0       # node spacing (px)
SIGMA = 1e-3         # small noise; residual dominated by representation error

def bilinear_nodes(xs, ys, node_x, node_y):
    ix = np.clip(np.searchsorted(node_x, xs) - 1, 0, len(node_x) - 2)
    iy = np.clip(np.searchsorted(node_y, ys) - 1, 0, len(node_y) - 2)
    tx = (xs - node_x[ix]) / (node_x[ix + 1] - node_x[ix])
    ty = (ys - node_y[iy]) / (node_y[iy + 1] - node_y[iy])
    return ix, iy, tx, ty

def design(theta_shape, xs, ys, node_x, node_y, frame, n_frames):
    """rows: B_ref bilinear (nodes) + per-frame plane (a + b x) delta_k, frame0 gauge 0."""
    nx, ny = len(node_x), len(node_y)
    n_par = nx * ny + 2 * (n_frames - 1)
    ix, iy, tx, ty = bilinear_nodes(xs, ys, node_x, node_y)
    n = len(xs)
    R = np.zeros((n, n_par))
    rows = np.arange(n)
    R[rows, iy * nx + ix] += (1 - tx) * (1 - ty)
    R[rows, iy * nx + ix + 1] += tx * (1 - ty)
    R[rows, (iy + 1) * nx + ix] += (1 - tx) * ty
    R[rows, (iy + 1) * nx + ix + 1] += tx * ty
    if frame > 0:
        base = nx * ny + 2 * (frame - 1)
        R[rows, base] = 1.0
        R[rows, base + 1] = xs / W_IMG
    return R, n_par

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "h_node_px": H_NODE, "cases": []}
    node_x = np.arange(0, W_IMG + 1, H_NODE); node_y = np.arange(0, H_IMG + 1, H_NODE)
    ys, xs = np.meshgrid(np.linspace(2, H_IMG - 3, 64), np.linspace(2, W_IMG - 3, 200))
    ys, xs = ys.ravel(), xs.ravel()
    framesamp = (xs < X_BND).astype(int)  # frame 0 left of boundary, frame 1 right
    for s in (1600.0, 800.0, 400.0, 200.0, 100.0, 50.0):
        diff = np.sin(2 * np.pi * xs / s)          # coherent component, amplitude 1
        y0 = 0.5 * diff * (framesamp == 0) + 0.5 * diff * 0  # frame0 holds +D/2 ... see below
        # frame0 sky: +D/2 on its side; frame1 sky: -D/2 on its side (difference = D)
        y = np.where(framesamp == 0, 0.5 * diff, -0.5 * diff)
        y = y + rng.standard_normal(len(y)) * SIGMA
        R0, n0 = design(None, xs[framesamp == 0], ys[framesamp == 0], node_x, node_y, 0, 2)
        R1, n1 = design(None, xs[framesamp == 1], ys[framesamp == 1], node_x, node_y, 1, 2)
        X = np.zeros((len(y), n0))
        X[framesamp == 0] = R0
        X[framesamp == 1] = R1
        th, *_ = np.linalg.lstsq(X, y, rcond=None)
        pred = X @ th
        resid = y - pred
        # seam metric at the boundary: rel-level-step = mean(residual right) - mean(left)
        seam = float(np.mean(resid[framesamp == 1]) - np.mean(resid[framesamp == 0]))
        res["cases"].append({"wavelength_s_px": s, "seam_residual_RMS": float(np.sqrt(np.mean(resid ** 2))),
                             "level_step_across_boundary": seam})
    rms = [c["seam_residual_RMS"] for c in res["cases"]]
    res["growth_ratio_1600_to_50"] = max(rms) / max(min(rms), 1e-300)
    res["doc_claim_growth_5.07"] = 5.07
    # (b) NEGATIVE: exactly representable difference (constant D = 1 between frames)
    y = np.where(framesamp == 0, 0.5, -0.5) + rng.standard_normal(len(y)) * SIGMA
    X = np.zeros((len(y), n0))
    X[framesamp == 0] = R0
    X[framesamp == 1] = R1
    th, *_ = np.linalg.lstsq(X, y, rcond=None)
    resid = y - X @ th
    seam = float(np.mean(resid[framesamp == 1]) - np.mean(resid[framesamp == 0]))
    res["negative_representable"] = {"level_step": seam, "seam_residual_RMS": float(np.sqrt(np.mean(resid ** 2)))}
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))

if __name__ == "__main__":
    main()
