#!/usr/bin/env python3
"""E3: seam gate max|rel_step| <= 1e-2 (docs/science/PHASE2_UPM.md §17).

Tests, on synthetic images with M42-like noise (sigma_pix/bg = 1.0703e-2, §17.4):
  (1) deterministic floor: noise=gradient=0 => rel_step = Delta/(L+Delta/2);
      gate crossing at Delta/L = gate/(1-gate/2) = 1.0050% (closed form check);
  (2) null distribution: amp=0 => max|rel_step| at baseline (NEGATIVE: truth-no-effect);
      pure noise+no gradient+no step => rel_step == 0 exactly per-realization median? no:
      metric is stochastic; the no-effect zero check uses gradient=0,step=0,noise=0 => 0.0;
  (3) detection probability vs Delta/L for n_s affected edges (multiplicity), 30 seeds;
      compare with §17.3 numbers (0.90%->17%, 0.95%->70%, 1.00%->96.7%);
  (4) gradient cancellation (defect D-57 mechanism): opposite gradient can mask a
      deterministically-red step => green;
  (5) sigma(rel_step): measured vs sqrt(2)*sigma/bg analytic vs sqrt(pi/2)*MAD formula.
Standalone: pure python3 + numpy. SEED fixed.
Run: python3 e3_seam_gate_1e-2.py
"""
import json, math, os
import numpy as np

SEED = 20250926
GATE = 1e-2
D = 2            # half-distance in px
N_E = 256        # samples along the edge
SIGMA_OVER_BG = 1.0703e-2
L = 1.0
H, W = 512, 512
X0 = W // 2
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e3_seam_gate_1e-2.json")

def seam_rel_step(img, x0=X0, d=D, n=N_E):
    ys = np.linspace(4, H - 5, n).astype(int)
    seam = img[ys, x0 + d] - img[ys, x0 - d]
    bg = np.median(np.abs(np.concatenate([img[ys, x0 + d], img[ys, x0 - d]])))
    return float(np.median(seam) / bg), seam / bg

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "gate": GATE, "d": D, "N_e": N_E, "sigma_over_bg": SIGMA_OVER_BG}
    # (1) deterministic floor
    floor = []
    for dl in (0.005, 0.009, 0.01005, 0.011, 0.02):
        Delta = dl * L
        img = np.full((H, W), L)
        img[:, X0:] += Delta
        rs, _ = seam_rel_step(img)
        floor.append({"Delta_over_L": dl, "rel_step": rs,
                      "closed_form_Delta/(L+Delta/2)": Delta / (L + Delta / 2.0)})
    res["deterministic_floor"] = floor
    res["deterministic_gate_crossing_Delta_over_L"] = GATE / (1.0 - GATE / 2.0)
    # (5) sigma of the metric under H0 (noise only), 200 realizations
    vals = []
    for _ in range(200):
        img = L + rng.standard_normal((H, W)) * (SIGMA_OVER_BG * L)
        rs, per = seam_rel_step(img)
        vals.append((rs, float(np.mean(np.abs(per)))))
    rs_arr = np.array([v[0] for v in vals])
    # (3) detection probability, n_s affected edges out of 114 applicable (M42-like),
    # each edge an independent realization; gate statistic = max over ALL 114 edges.
    n_edges, n_seeds = 114, 30
    sig = SIGMA_OVER_BG * L
    det = []
    for dl in (0.005, 0.00717, 0.009, 0.0095, 0.010, 0.0105, 0.011, 0.012, 0.015):
        for ns in (1, 2, 4):
            hits = 0
            for s in range(n_seeds):
                r2 = np.random.default_rng(SEED + 1000 * ns + s)
                Delta = dl * L
                red = 0.0
                stat = 0.0
                for e in range(n_edges):
                    if e < ns:
                        left = np.full(N_E, L); right = np.full(N_E, L + Delta)
                        seam = (right + r2.standard_normal(N_E) * sig) - (left + r2.standard_normal(N_E) * sig)
                        bg = np.median(np.abs(np.concatenate([left, right]))) + 0.0
                    else:
                        seam = r2.standard_normal(N_E) * sig * math.sqrt(2.0)
                        bg = L
                    stat = max(stat, abs(np.median(seam) / bg))
                hits += stat > GATE
            det.append({"Delta_over_L": dl, "n_s": ns, "P_detect": hits / n_seeds})
    res["detection"] = det
    # (4) gradient cancellation: step 2% (deterministically red) + opposite gradient
    dl = 0.02
    Delta = dl * L
    g = -Delta / (2 * D)  # 2d*grad = -Delta
    img = np.full((H, W), L) + g * (np.arange(W)[None, :] - X0)
    img[:, X0:] += Delta
    rs_g, _ = seam_rel_step(img)
    img2 = np.full((H, W), L) + g * (np.arange(W)[None, :] - X0)
    rs_pure_g, _ = seam_rel_step(img2)
    res["gradient_cancellation"] = {
        "step_2pct_no_gradient_rel_step": Delta / (L + Delta / 2.0),
        "step_2pct_plus_opposite_gradient_rel_step": rs_g,
        "gradient_only_rel_step": rs_pure_g,
        "gradient_only_predicted_4g/bg": 4 * g / L,
        "verdict_green_despite_step": rs_g <= GATE,
    }
    # (2) negative: zero-effect metric
    img0 = np.full((H, W), L)
    rs0, _ = seam_rel_step(img0)
    res["negative_zero_effect"] = {"rel_step": rs0, "metric_zero": rs0 == 0.0}
    res["null_distribution"] = {
        "sigma_rel_step_measured": float(np.std(rs_arr)),
        "mean_abs_per_edge": float(np.mean([v[1] for v in vals])),
        "analytic_sqrt2_sigma_over_bg": math.sqrt(2.0) * SIGMA_OVER_BG,
        "median_se_coeff_sqrt_pi_over_2": math.sqrt(math.pi / 2.0),
    }
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps({k: res[k] for k in ("deterministic_floor",
        "deterministic_gate_crossing_Delta_over_L", "gradient_cancellation",
        "negative_zero_effect", "null_distribution")}, indent=1))
    print("detection (selected):")
    for row in res["detection"]:
        if row["Delta_over_L"] in (0.009, 0.0095, 0.010, 0.0105):
            print(row)

if __name__ == "__main__":
    main()
