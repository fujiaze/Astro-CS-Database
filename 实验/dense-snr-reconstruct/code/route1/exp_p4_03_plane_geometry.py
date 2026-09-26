#!/usr/bin/env python3
"""EXP-P4-03: Geometry admissibility criterion for the plane variance field
(P-CST-17:  lambda_lo / lambda_hi >= 1/16, i.e. condition number kappa <= 4).

Audit item I5. The plane field  var(x,y) = a + b x + c y  is fitted to control-point
variances; the fit is ill-conditioned when the control-point cloud is near-collinear.
The frozen criterion is a centered-Gram eigenvalue-ratio threshold.

Claims tested:
  T (theory) : For the centered design A = [1, x-xbar, y-ybar], the normal-equation
               matrix is the centered Gram G = A^T A with eigenvalues
               lambda_hi >= lambda_mid >= lambda_lo, and
               kappa_2(A) = sqrt(lambda_hi / lambda_lo).
               Hence lambda_lo/lambda_hi >= 1/16  <=>  kappa_2(A) <= 4:
               the criterion is exactly a "no worse than a 4:1 axis-ratio ellipse"
               geometric admissibility condition on the point cloud.
  E (experiment): control clouds with a collinear fraction swept from 0 to 0.875;
               plane fit coefficient recovery error and prediction RMSE vs the
               eigenvalue ratio, unweighted and relative-error-weighted (Aitken).
Negative controls (non-degeneracy):
    (a) isotropic cloud (ratio ~ 1): the criterion must NOT reject, and the fit must
        be unbiased at the noise level ("healthy configs are never flagged" arm);
    (b) the degradation metric must collapse to zero for a perfectly identified cloud
        at zero noise (true-value-no-effect => metric zero).

Pure python + numpy. No repository imports. Seed hardcoded.
Run:  python3 exp_p4_03_plane_geometry.py
Out:  ../results/exp_p4_03_plane_geometry.json
"""
import json, time
import numpy as np

SEED = 20260926
OUT = "../results/exp_p4_03_plane_geometry.json"

N_PTS = 48
N_TRIALS = 200
NOISE_REL = 0.05      # relative noise on control variances


def cloud(frac_lin, rng, n=N_PTS):
    """Point cloud with a fraction of points exactly on a line y = 0.3 + 0.05 x."""
    k = int(round(frac_lin * n))
    pts = rng.uniform(0.0, 1.0, size=(n, 2))
    t = rng.uniform(0.0, 1.0, size=k)
    pts[:k, 0] = t
    pts[:k, 1] = 0.3 + 0.05 * t
    return pts


def gram_ratio(pts):
    """Centered point-cloud scatter matrix (2x2): G = sum_k p_k p_k^T with
    p_k = (x-xbar, y-ybar). lambda ratio of THIS matrix is the frozen criterion
    (NOISE_MODEL.md S4: 'centered control-point cloud Gram eigenvalue ratio');
    the constant column is not part of the point cloud."""
    P = np.column_stack([pts[:, 0] - pts[:, 0].mean(), pts[:, 1] - pts[:, 1].mean()])
    G = P.T @ P
    ev = np.sort(np.linalg.eigvalsh(G))[::-1]
    return ev[0], ev[1], ev[1] / ev[0]     # lam_hi, lam_lo, ratio


def fit_plane(pts, z, weights=None):
    A = np.column_stack([np.ones(len(pts)), pts[:, 0], pts[:, 1]])
    if weights is None:
        coef, *_ = np.linalg.lstsq(A, z, rcond=None)
    else:
        WA = A * weights[:, None]
        coef, *_ = np.linalg.lstsq(WA, weights * z, rcond=None)
    return coef


def trial(frac_lin, seed, weighted):
    rng = np.random.default_rng(seed)
    pts = cloud(frac_lin, rng)
    lam_hi, lam_lo, ratio = gram_ratio(pts)
    a, b, c = 25.0, 3.0, -1.5            # true plane, ADU^2
    z_true = a + b * pts[:, 0] + c * pts[:, 1]
    z = z_true * (1.0 + rng.normal(0.0, NOISE_REL, size=len(pts)))
    w = None
    if weighted:
        w = (z_true.mean() / z_true) ** 2   # relative-error weights (Aitken-style)
    coef = fit_plane(pts, z, w)
    pred = coef[0] + coef[1] * pts[:, 0] + coef[2] * pts[:, 1]
    return {
        "ratio": float(ratio), "kappa": float(np.sqrt(lam_hi / lam_lo)),
        "b_rel_err": float(abs(coef[1] / b - 1.0)),
        "c_rel_err": float(abs(coef[2] / c - 1.0)),
        "pred_rmse_rel": float(np.sqrt(np.mean(((pred - z_true) / z_true) ** 2))),
        "noise_floor_rel": NOISE_REL / np.sqrt(len(pts)),
    }


def main():
    t0 = time.time()
    frac_list = [0.0, 0.5, 0.875, 0.9375, 0.9583, 0.9792]
    res = {"seed": SEED, "n_points": N_PTS, "n_trials": N_TRIALS,
           "noise_rel": NOISE_REL, "sweep": []}
    for frac in frac_list:
        agg = {"frac_lin": frac}
        for weighted in (False, True):
            runs = [trial(frac, SEED + 1000 * (int(weighted) + 1) + i, weighted)
                    for i in range(N_TRIALS)]
            key = "weighted" if weighted else "unweighted"
            agg[key] = {
                "ratio_median": float(np.median([r["ratio"] for r in runs])),
                "kappa_median": float(np.median([r["kappa"] for r in runs])),
                "b_rel_err_median": float(np.median([r["b_rel_err"] for r in runs])),
                "b_rel_err_p90": float(np.percentile([r["b_rel_err"] for r in runs], 90)),
                "pred_rmse_rel_median": float(np.median([r["pred_rmse_rel"] for r in runs])),
            }
        res["sweep"].append(agg)

    # ---- theory check: kappa relation on a synthetic family of clouds ----
    chk = []
    for ax_ratio in [1.0, 2.0, 4.0, 8.0, 16.0]:
        rng = np.random.default_rng(SEED + 7)
        u = rng.uniform(-1, 1, 200000)
        v = rng.uniform(-1, 1, 200000)
        pts = np.column_stack([u, v / ax_ratio])
        lam_hi, lam_lo, ratio = gram_ratio(pts)
        chk.append({"axis_ratio": ax_ratio, "gram_ratio": float(ratio),
                    "kappa": float(np.sqrt(lam_hi / lam_lo)),
                    "kappa_matches_axis_ratio": bool(
                        abs(np.sqrt(lam_hi / lam_lo) / ax_ratio - 1.0) < 0.05)})
    res["theory_check"] = chk

    # ---- negative control (b): zero noise, well-conditioned cloud => metric zero ----
    rng = np.random.default_rng(SEED + 9)
    pts = rng.uniform(0, 1, size=(N_PTS, 2))
    a, b, c = 25.0, 3.0, -1.5
    z_true = a + b * pts[:, 0] + c * pts[:, 1]
    coef = fit_plane(pts, z_true)
    res["zero_noise_control"] = {
        "coef_rel_err_max": float(np.max(np.abs(coef - [a, b, c]) / np.abs([a, b, c]))),
        "ratio": float(gram_ratio(pts)[2]),
    }

    # ---- negative control (a): isotropic cloud must not be flagged ----
    iso = [trial(0.0, SEED + 11 + i, False) for i in range(N_TRIALS)]
    res["isotropic_control"] = {
        "min_ratio": float(min(r["ratio"] for r in iso)),
        "all_ratio_ge_1_over_16": bool(min(r["ratio"] for r in iso) >= 1.0 / 16.0),
        "b_rel_err_median": float(np.median([r["b_rel_err"] for r in iso])),
    }

    # ---- empirical degradation onset vs the 1/16 threshold ----
    # Baseline: coefficient scatter of an isotropic cloud is noise-dominated. A sweep
    # point counts as degraded when its median coefficient error exceeds 3x baseline.
    base = res["sweep"][0]["unweighted"]["b_rel_err_median"]
    degraded = [s for s in res["sweep"]
                if s["unweighted"]["b_rel_err_median"] > 3.0 * max(base, 1e-12)]
    first_bad_ratio = degraded[0]["unweighted"]["ratio_median"] if degraded else None
    res["degradation_onset"] = {
        "baseline_b_rel_err_median": float(base),
        "first_degraded_frac_lin": degraded[0]["frac_lin"] if degraded else None,
        "gram_ratio_at_onset": first_bad_ratio,
    }
    # Sufficiency arm: every sweep point whose ratio >= 1/16 stays within 3x baseline.
    ok_pts = [s for s in res["sweep"] if s["unweighted"]["ratio_median"] >= 1.0 / 16.0]
    res["sufficiency"] = {
        "n_points_above_threshold": len(ok_pts),
        "max_b_rel_err_above_threshold":
            float(max((s["unweighted"]["b_rel_err_median"] for s in ok_pts), default=0.0)),
    }

    res["gates"] = {
        "kappa_theory_holds": all(c["kappa_matches_axis_ratio"] for c in chk),
        "isotropic_never_flagged": res["isotropic_control"]["all_ratio_ge_1_over_16"],
        "zero_noise_recovers_exactly": res["zero_noise_control"]["coef_rel_err_max"] < 1e-9,
        "threshold_is_sufficient":
            res["sufficiency"]["max_b_rel_err_above_threshold"] <= 3.0 * max(base, 1e-12),
    }
    res["all_gates_pass"] = bool(all(res["gates"].values()))
    res["runtime_s"] = time.time() - t0
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
