#!/usr/bin/env python3
"""SNR-DESIGN EXP-1: imperfect-weight efficiency penalty + residual-maker variance.

Two independent, falsifiable claims used by reports/RELEASE-02/snr-propagation-design.md:

(C1) WEIGHT-EFFICIENCY PENALTY.
     For an inverse-variance combination F_hat = sum_i w_i x_i / sum_i w_i with
     x_i = F + n_i, Var(n_i) = sigma_i^2 = 1/t_i, using *approximate* weights
     w_i = t_i (1 + delta_i) instead of the exact t_i costs, to leading order,

         Var_approx / Var_opt = 1 + Var_p(delta) + O(delta^3),
         Var_p(delta) = sum_i p_i (delta_i - deltabar)^2,  p_i = t_i / sum_j t_j.

     => A *common-mode* weight error (delta_i identical for all frames) is FREE.
        Only the frame-to-frame SCATTER of the relative weight error costs.
     This is the quantitative justification for accepting a sparse/approximate
     SNR field: the penalty is second order in the relative SNR error.

(C2) RESIDUAL-MAKER VARIANCE.
     For corrected = (I - H) y = P y with Cov(y) = diag(sigma^2),
         Var(corrected_i) = sum_j P_ij^2 sigma_j^2   (exact, diagonal input)
     The naive form  sigma_i^2 + Var(g_hat_i)  (treating the subtracted estimate
     as an independent random variable) omits the cross term -2 sum_j H_ij ... and
     for the sample-mean residual maker with N=8 overestimates by exactly 8/7.

Run:  TMPDIR=/dev/shm/astrocs_snrd python3 exp1_weight_penalty.py --out exp1_weight_penalty.json
"""
import argparse, json, os
import numpy as np

RNG = np.random.default_rng(20260919)


def mc_penalty(n_frames=8, n_real=200000, sigma_spread=0.5, err_scale=0.05,
               common_mode=0.0):
    """Monte-Carlo Var_approx / Var_opt for lognormal true sigmas and
    weight errors delta = err_scale*N(0,1) + common_mode*N(0,1)."""
    sig = np.exp(RNG.normal(0.0, sigma_spread, size=(n_real, n_frames)))
    t = 1.0 / sig**2
    p = t / t.sum(axis=1, keepdims=True)
    delta = err_scale * RNG.normal(size=(n_real, n_frames))
    if common_mode:
        delta = delta + common_mode * RNG.normal(size=(n_real, 1))
    w = t * (1.0 + delta)
    var_approx = (w**2 * sig**2).sum(axis=1) / w.sum(axis=1) ** 2
    var_opt = 1.0 / t.sum(axis=1)
    ratio_mc = var_approx / var_opt
    dbar = (p * delta).sum(axis=1)
    var_p = (p * (delta - dbar[:, None]) ** 2).sum(axis=1)
    ratio_pred = 1.0 + var_p / (1.0 + dbar) ** 2
    return ratio_mc, ratio_pred, var_p


def residual_maker(n_frames=8, n_pix=400000, sigma=1.0):
    """corrected_i = y_i - mean_{j!=?} ; use the full sample-mean residual maker
    P = I - (1/N) 11^T (a *shared* mean, i.e. self-inclusive), which is the
    'residual maker' whose naive variance is wrong."""
    y = RNG.normal(0.0, sigma, size=(n_pix, n_frames))
    ybar = y.mean(axis=1, keepdims=True)
    corr = y - ybar                       # = P y, P = I - 11^T/N
    var_exact = corr.var(axis=0, ddof=0)  # MC of sum_j P_ij^2 sigma_j^2
    P = np.eye(n_frames) - np.ones((n_frames, n_frames)) / n_frames
    var_formula = (P**2).sum(axis=1) * sigma**2          # exact diagonal form
    var_naive = np.full(n_frames, sigma**2 + sigma**2 / n_frames)  # sigma^2 + Var(ybar)
    return dict(
        var_exact_mc=var_exact.tolist(),
        var_formula_diag=var_formula.tolist(),
        var_naive=var_naive.tolist(),
        ratio_naive_over_exact=(var_naive / var_formula).tolist(),
        ratio_naive_over_exact_scalar=float(var_naive[0] / var_formula[0]),
        # exact: sum_j P_ij^2 = 1 - 1/N ; naive: 1 + 1/N  =>  (N+1)/(N-1)
        predicted_ratio=float((n_frames + 1) / (n_frames - 1)),
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="exp1_weight_penalty.json")
    a = ap.parse_args()

    res = {"exp": "EXP-1 imperfect-weight penalty + residual-maker variance"}

    # --- C1: scan of the relative weight-error scatter ------------------------
    scan = []
    for err in (0.0, 0.005, 0.01, 0.02, 0.05, 0.10, 0.20):
        rmc, rpred, varp = mc_penalty(err_scale=err)
        scan.append(dict(rel_weight_err_rms=err,
                         var_p_mean=float(varp.mean()),
                         ratio_mc_mean=float(rmc.mean()),
                         ratio_pred_mean=float(rpred.mean()),
                         ratio_mc_p95=float(np.percentile(rmc, 95)),
                         sigma_penalty_pct=float(100.0 * (np.sqrt(rmc.mean()) - 1.0))))
    res["C1_penalty_scan"] = scan

    # --- C1 negative control: common-mode error is free -----------------------
    rmc_c, rpred_c, varp_c = mc_penalty(err_scale=0.0, common_mode=0.20)
    rmc_i, rpred_i, varp_i = mc_penalty(err_scale=0.20, common_mode=0.0)
    res["C1_common_mode"] = dict(
        common_only_ratio_mean=float(rmc_c.mean()),
        independent_ratio_mean=float(rmc_i.mean()),
        common_only_var_p=float(varp_c.mean()),
        independent_var_p=float(varp_i.mean()),
    )

    # --- C1 sanity: 1/sigma^2 really is optimal (negative example must go red)
    sig = np.exp(RNG.normal(0.0, 0.5, size=(200000, 8)))
    t = 1.0 / sig**2
    p = t / t.sum(axis=1, keepdims=True)
    var_opt = 1.0 / t.sum(axis=1)
    var_eq = (sig**2).sum(axis=1) / 8**2
    var_bad = ((1.0 / (sig**2 * 0.0 + 1.0)) ** 2 * sig**2).sum(axis=1) \
        / (1.0 / (sig**2 * 0.0 + 1.0)).sum(axis=1) ** 2
    res["C1_baselines"] = dict(
        var_opt_mean=float(var_opt.mean()),
        var_equal_weight_mean=float(var_eq.mean()),
        var_unit_weight_mean=float(var_bad.mean()),
        equal_over_opt=float(var_eq.mean() / var_opt.mean()),
        unit_over_opt=float(var_bad.mean() / var_opt.mean()),
    )

    # --- C2: residual-maker variance -----------------------------------------
    for nf in (4, 8, 16):
        res[f"C2_residual_maker_N{nf}"] = residual_maker(n_frames=nf)

    with open(a.out, "w") as f:
        json.dump(res, f, indent=2, sort_keys=True)

    print("== C1 penalty scan ==")
    print(f"{'d_w_rms':>8} {'Var_p':>10} {'ratio_MC':>10} {'ratio_pred':>11} {'sig_pen%':>9}")
    for s in scan:
        print(f"{s['rel_weight_err_rms']:8.3f} {s['var_p_mean']:10.5f} "
              f"{s['ratio_mc_mean']:10.5f} {s['ratio_pred_mean']:11.5f} "
              f"{s['sigma_penalty_pct']:9.3f}")
    print("\n== C1 common-mode (must be FREE) vs independent ==")
    print(json.dumps(res["C1_common_mode"], indent=2))
    print("\n== C1 baselines ==")
    print(json.dumps(res["C1_baselines"], indent=2))
    print("\n== C2 residual maker (naive/ exact) ==")
    for nf in (4, 8, 16):
        d = res[f"C2_residual_maker_N{nf}"]
        print(f"  N={nf:2d}: ratio_naive_over_exact={d['ratio_naive_over_exact_scalar']:.6f} "
              f"predicted N/(N-1)={d['predicted_ratio']:.6f}")
    print("\nwrote", a.out)


if __name__ == "__main__":
    main()
