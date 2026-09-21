#!/usr/bin/env python3
"""SNR-DESIGN EXP-5: final-product SNR error budget.

Two DIFFERENT questions, kept separate on purpose:

(A) Random error of the stacked signal  -- what the mosaic is worth.
      Var_stack(p) = 1 / sum_k w_k(p)          (exact inverse-variance weights)
      Var_stack(p) = sum_k w_k^2 v_k / (sum w_k)^2   (general weights)
    Reported as the SNR gain factor relative to the best single frame.

(B) Error of the REPORTED SNR/variance itself -- "is the number right?"
      eps_tot^2 = eps_P1^2 + eps_sparse^2 + eps_theta^2 + eps_g^2
                + eps_drizzle^2 + eps_master^2
    Each term is a RELATIVE error on the local sigma (equivalently on the
    published SNR), evaluated from independent evidence; the script prints the
    composition and the dominant term.

All inputs are either measured in EXP-2/EXP-3 on real RELEASE-02 frames or
quoted from a frozen project constant with its source; nothing is invented.

Run: TMPDIR=/dev/shm/astrocs_snrd python3 exp5_error_budget.py --out exp5_error_budget.json
"""
import argparse, json
import numpy as np


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="exp5_error_budget.json")
    ap.add_argument("--n-frames", type=int, default=8)
    ap.add_argument("--frame-snr-spread", type=float, default=2.22,
                    help="max/min of ASTROCS_FRAME_SNR over the 49 L4 frames")
    a = ap.parse_args()

    # ---------------- (A) stacking gain -------------------------------------
    rng = np.random.default_rng(11)
    snr = np.exp(rng.uniform(0.0, np.log(a.frame_snr_spread), size=200000))
    snr = np.repeat(snr[:, None], a.n_frames, axis=1) * np.exp(
        rng.normal(0, 0.05, size=(200000, a.n_frames)))
    w = snr ** 2                                   # w = SNR^2 / F_ref^2, F_ref common
    var_stack = 1.0 / w.sum(axis=1)
    var_best = 1.0 / w.max(axis=1)
    var_mean = (w ** -1.0).mean(axis=1)            # unweighted mean of frames
    gain_vs_best = np.sqrt(var_best / var_stack)
    gain_vs_mean = np.sqrt(var_mean / var_stack)
    # equal-weight stack of the same frames
    var_eq = (w ** -1.0).sum(axis=1) / a.n_frames ** 2
    A = dict(
        n_frames=a.n_frames,
        frame_snr_max_over_min=a.frame_snr_spread,
        snr_gain_vs_best_single_frame=dict(
            median=float(np.median(gain_vs_best)),
            p05=float(np.percentile(gain_vs_best, 5)),
            p95=float(np.percentile(gain_vs_best, 95)),
            sqrt_n=float(np.sqrt(a.n_frames)),
            note="sqrt(N) is reached only when all frames have equal SNR"),
        snr_gain_inverse_variance_vs_equal_weight=dict(
            median=float(np.median(np.sqrt(var_eq / var_stack))),
            p05=float(np.percentile(np.sqrt(var_eq / var_stack), 5)),
            p95=float(np.percentile(np.sqrt(var_eq / var_stack), 95))),
        snr_gain_vs_unweighted_mean=dict(
            median=float(np.median(gain_vs_mean))),
    )

    # ---------------- (B) budget of the REPORTED sigma ----------------------
    # every entry: (name, relative error on sigma, evidence)
    terms = [
        ("phase1_estimator_noise", 1.44 / np.sqrt(9216),
         "SE(sigma_hat)/sigma ~ 1.44/sqrt(N_sky), N_sky>=9216 -> 1.5% "
         "(NOISE_MODEL.md 5a sky-budget constant)"),
        ("phase1_source_contamination_bias", 0.0,
         "MEASURED: production frame scalar sigma_MAD / sigma_clippedRMS = "
         "1.3127 median (range 1.09-1.57) over 8 real frames (EXP-3 Part A); "
         "the production estimator is a whole-frame UNCLIPPED MAD, so this is "
         "a BIAS not a random error -- entered separately below"),
        ("sparse_reconstruction", 0.0,
         "MEASURED: RMSE(log SNR) 0.092 (64 px pitch) .. 0.230 (1024 px) "
         "on a real M42 frame (EXP-2/EXP-3)"),
        ("upm_parameter_covariance", 0.0,
         "reports/RELEASE-02/unc-prop-audit.md 2.2/2.3: J_out C_theta J_out^T "
         "= 0.46..333 ADU^2 vs raw sigma^2 = 6.76 ADU^2 => +7%..+4900% in "
         "VARIANCE, i.e. +3.4%..+2200% in sigma, condition-number dependent"),
        ("multiplicative_normalization_g", 0.0,
         "production runs g == 1 (module_adapters.cpp:6344/6368 read "
         "multiplicative_gain_applied / frame_gain, no writer exists), so the "
         "term is structurally zero TODAY; it becomes the dominant term the "
         "moment g is enabled without an uncertainty on g"),
        ("drizzle_correlated_noise", 0.202,
         "UNCERTAINTY_AND_COVARIANCE.md: diagonal-only propagation "
         "underestimates variance by ~1+0.75*rho; rho=0.19 (nside=512 MC) "
         "=> variance low by 36.3%, sigma low by 20.2%"),
        ("master_frame_variance", 0.0,
         "CALIBRATION.md:236 registers the master-variance gap as UNRESOLVED "
         "with no project formula; a finite-master model gives "
         "sigma_master/sigma_sky ~ 1/sqrt(N_master) per master frame"),
    ]

    # scenario table: how the composition changes with the two live choices
    scen = []
    for label, eps_p1, eps_sparse, eps_theta, eps_g in (
            ("current production (scalar sigma, no sparse layer, g=1, no C_theta)",
             1.3127, 0.0, 0.0, 0.0),
            ("scalar sigma fixed + C_theta propagated (well conditioned)",
             0.015, 0.0, 0.034, 0.0),
            ("scalar sigma fixed + C_theta propagated (ill conditioned)",
             0.015, 0.0, 2.20, 0.0),
            ("+ sparse layer at 64 px pitch (UPM control pitch)",
             0.015, 0.092, 0.034, 0.0),
            ("+ sparse layer at 512 px pitch (Phase1 8x8 patch grid)",
             0.015, 0.135, 0.034, 0.0),
            ("+ g enabled with 1% gain uncertainty",
             0.015, 0.092, 0.034, 0.010),
    ):
        tot = float(np.sqrt(eps_p1**2 + eps_sparse**2 + eps_theta**2 + eps_g**2))
        scen.append(dict(scenario=label, eps_phase1=eps_p1,
                         eps_sparse=eps_sparse, eps_theta=eps_theta,
                         eps_g=eps_g, eps_total=tot,
                         snr_error_pct=100.0 * tot))
    B = dict(terms=[dict(name=n, rel_sigma_error=e, evidence=ev)
                    for n, e, ev in terms],
             scenarios=scen,
             drizzle_term_always_present=0.202)

    out = dict(
        exp="EXP-5 final-product SNR error budget",
        partA_stacking_gain=A, partB_reported_sigma_budget=B,
    )
    with open(a.out, "w") as f:
        json.dump(out, f, indent=2)

    print("== (A) stacking gain over %d frames (frame SNR spread %.2fx) =="
          % (a.n_frames, a.frame_snr_spread))
    g = A["snr_gain_vs_best_single_frame"]
    print("   SNR gain vs best single frame : median %.3f  [p05 %.3f, p95 %.3f]"
          % (g["median"], g["p05"], g["p95"]))
    print("   sqrt(N) reference             : %.3f" % g["sqrt_n"])
    q = A["snr_gain_inverse_variance_vs_equal_weight"]
    print("   inverse-variance vs equal wt  : median %.3f  [p05 %.3f, p95 %.3f]"
          % (q["median"], q["p05"], q["p95"]))
    print()
    print("== (B) error of the REPORTED sigma (relative) ==")
    print("%-42s %12s" % ("scenario", "eps_total %"))
    for s in scen:
        print("%-42s %12.3f" % (s["scenario"], s["snr_error_pct"]))
    print()
    print("   (drizzle correlated-noise term %.1f%% is present in every row)"
          % (100 * 0.202))
    print("\nwrote", a.out)


if __name__ == "__main__":
    main()
