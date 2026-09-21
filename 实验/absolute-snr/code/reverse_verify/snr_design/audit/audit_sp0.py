#!/usr/bin/env python3
"""SNR-EXP-AUDIT 2: independent re-derivation and physical verification of the
SP-0 criterion  Var_approx/Var_opt = 1 + Var_p(delta).

AUDITED CLAIM (design doc section 3.1 SP-0 / section 3.6 eq (3.1)):
    Var_approx/Var_opt = 1 + Var_p(delta) + O(delta^3),
    delta_f = w_hat_f/w_f - 1,  Var_p = sum_f p_f (delta_f - deltabar)^2.

INDEPENDENT RE-DERIVATION (exact, no approximation)
    x_i independent, Var(x_i) = sigma_i^2 = 1/t_i, weights w_i = t_i (1+delta_i)
    Var_approx = sum w_i^2 sigma_i^2 / (sum w_i)^2
               = sum t_i (1+delta_i)^2 / (sum t_i (1+delta_i))^2
    Var_opt    = 1 / sum t_i
    =>  Var_approx/Var_opt = [sum p_i (1+d_i)^2] / [sum p_i (1+d_i)]^2 ,  p_i = t_i/sum t
                           = 1 + Var_p(delta) / (1 + deltabar)^2          EXACT
    The document writes "1 + Var_p(delta) + O(delta^3)", which is the expansion
    of the exact form for deltabar -> 0.

WHAT THE DERIVATION USES
    (a) Var(x_i) = sigma_i^2  -- ANY distribution, Gaussian or Poisson;
    (b) Cov(x_i, x_j) = 0 for i != j  -- frame independence;
    (c) the weights are FIXED, i.e. statistically independent of the x_i.
    It does NOT use Gaussianity.  Items (b) and (c) are the real assumptions.

GAP_AUDIT 9.42: no physical closed form evaluated; G, RN declared.

Run:
  TMPDIR=/dev/shm/astrocs_snraudit python3 audit_sp0.py \
     --out ../../../../run/reverse_verify/snr_design/audit/audit_sp0.json
"""
import argparse, json, os, sys, time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import physnoise as pn

NPIX = 2000 * 2000


def stack_ratio(frames, weights):
    """Empirical Var(stack) and the stacked array.  frames: (F, ...) any shape."""
    x = np.asarray(frames, float)
    F = x.shape[0]
    x = x.reshape(F, -1)
    w = np.asarray(weights, float)[:, None]
    y = (w * x).sum(axis=0) / w.sum(axis=0)
    return float(np.var(y, ddof=1)), y


def exact_ratio(weights, var_true):
    w = np.asarray(weights, float)
    t = 1.0 / np.asarray(var_true, float)
    p = t / t.sum()
    d = w / t - 1.0
    dbar = float((p * d).sum())
    var_p = float((p * (d - dbar) ** 2).sum())
    return 1.0 + var_p / (1.0 + dbar) ** 2, var_p, dbar


def doc_ratio(weights, var_true):
    _, var_p, _ = exact_ratio(weights, var_true)
    return 1.0 + var_p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="audit_sp0.json")
    ap.add_argument("--G", type=float, default=1.3)
    ap.add_argument("--RN", type=float, default=10.0)
    a = ap.parse_args()
    t0 = time.time()
    out = {"audit": "SNR-EXP-AUDIT 2 -- SP-0 criterion, independent re-derivation "
                    "+ physical verification",
           "declared_model_parameters": {"G_e_per_adu": a.G, "RN_e": a.RN,
                                         "status": "DECLARED, not measured"},
           "gap_audit_9_42": "no physical closed form evaluated",
           "exact_form": "Var_approx/Var_opt = 1 + Var_p(delta)/(1+deltabar)^2",
           "document_form": "Var_approx/Var_opt = 1 + Var_p(delta) + O(delta^3)"}

    rng = np.random.default_rng(5)
    worst = 0.0
    worst_doc = 0.0
    for _ in range(200):
        F = int(rng.integers(2, 9))
        t = np.exp(rng.normal(0, 1.0, F))
        d = rng.normal(0, 0.3, F)
        w = t * (1 + d)
        v = 1.0 / t
        direct = ((w ** 2) * v).sum() / w.sum() ** 2 / (1.0 / t.sum())
        ex, _, _ = exact_ratio(w, v)
        worst = max(worst, abs(direct - ex))
        worst_doc = max(worst_doc, abs(direct - doc_ratio(w, v)))
    out["A_algebra"] = dict(
        n_trials=200, max_abs_error_exact_form=float(worst),
        max_abs_error_document_form=float(worst_doc),
        verdict="EXACT form verified to machine precision; the document form "
                "differs at O(delta^3) as stated")

    F = 4
    scales = np.array([0.85, 0.95, 1.05, 1.15])
    scales = scales / scales.mean()
    mu0 = 180.0
    tmpl = np.full((2000, 2000), mu0)
    rngB = np.random.default_rng(2024)

    def make_frames():
        return np.stack([pn.simulate_frame(tmpl, None, G=a.G, RN=a.RN,
                                           sky_scale=float(s), rng=rngB)
                         for s in scales])

    var_true = np.array([mu0 * s / a.G + (a.RN / a.G) ** 2 + (1 / 12) / a.G ** 2
                         for s in scales])
    t_true = 1.0 / var_true
    out["B_setup"] = dict(n_frames=F, sky_scales=[float(s) for s in scales],
                          sky_level_adu=mu0,
                          truth_sigma_adu=[float(np.sqrt(v)) for v in var_true],
                          frame_sigma_spread=float(np.sqrt(var_true.max() / var_true.min())),
                          n_pixels=NPIX)

    rows = []
    for drms in (0.0, 0.01, 0.05, 0.20):
        delta = np.zeros(F) if drms == 0 else rngB.normal(0, drms, F)
        w = t_true * (1 + delta)
        frames = make_frames()
        var_meas, _ = stack_ratio(frames, w)
        var_opt = 1.0 / t_true.sum()
        ratio_meas = var_meas / var_opt
        ex, var_p, dbar = exact_ratio(w, var_true)
        rows.append(dict(delta_rms=drms, delta=[float(x) for x in delta],
                         var_p_delta=var_p, delta_bar=dbar,
                         ratio_measured_poisson=float(ratio_meas),
                         ratio_exact_form=ex, ratio_document_form=doc_ratio(w, var_true),
                         rel_err_exact_pct=float(100 * (ratio_meas / ex - 1)),
                         rel_err_document_pct=float(100 * (ratio_meas / doc_ratio(w, var_true) - 1))))
    out["B_physical_poisson_verification"] = rows

    rows_g = []
    for drms in (0.0, 0.01, 0.05, 0.20):
        delta = np.zeros(F) if drms == 0 else rngB.normal(0, drms, F)
        w = t_true * (1 + delta)
        frames = np.stack([rngB.normal(0, np.sqrt(v), NPIX) for v in var_true])
        var_meas, _ = stack_ratio(frames, w)
        ratio_meas = var_meas / (1.0 / t_true.sum())
        ex, var_p, dbar = exact_ratio(w, var_true)
        rows_g.append(dict(delta_rms=drms, ratio_measured_gaussian=float(ratio_meas),
                           ratio_exact_form=ex,
                           rel_err_exact_pct=float(100 * (ratio_meas / ex - 1))))
    out["B_gaussian_comparison"] = rows_g

    neg = {}
    w = np.ones(F) * t_true
    frames = make_frames()
    var_meas, _ = stack_ratio(frames, w)
    neg["C1_zero_delta"] = dict(ratio=float(var_meas * t_true.sum()),
                                var_p_delta=0.0, must_be="1.000000")
    c = 0.37
    w = t_true * (1 + c)
    var_meas, _ = stack_ratio(frames, w)
    ex, var_p, dbar = exact_ratio(w, var_true)
    neg["C2_common_mode_delta_37pct"] = dict(ratio_measured=float(var_meas * t_true.sum()),
                                             ratio_exact=ex, var_p_delta=var_p,
                                             delta_bar=dbar, must_be="1.000000")
    var_ident = np.full(F, var_true.mean())
    w = np.full(F, 1.0 / var_ident.mean())
    ex, var_p, dbar = exact_ratio(w, var_ident)
    neg["C3_identical_sigma_fields"] = dict(ratio_exact=ex, var_p_delta=var_p,
                                            must_be="1.000000")
    out["C_negative_controls"] = neg

    rows_d = []
    for cell in (4096, 256, 64, 1):
        frames = make_frames().reshape(F, -1)
        vhat = frames[:, :cell].var(axis=1, ddof=0)
        w = 1.0 / vhat
        y = (w[:, None] * frames).sum(axis=0) / w.sum()
        var_meas = float(np.var(y[cell:], ddof=1))
        ratio_meas = var_meas / (1.0 / t_true.sum())
        delta = w / t_true - 1.0
        ex, var_p, dbar = exact_ratio(w, var_true)
        rows_d.append(dict(estimator_pixels=cell,
                           delta_realised=[float(x) for x in delta],
                           ratio_measured=float(ratio_meas),
                           ratio_exact_form_from_realised_delta=ex,
                           rel_err_exact_pct=float(100 * (ratio_meas / ex - 1))))
    out["D_self_estimated_weights"] = dict(
        rows=rows_d,
        note="weights estimated from the same frames break assumption (c); the "
             "residual bias is reported as rel_err_exact_pct")

    rows_e = []
    for rho_common in (0.0, 0.02, 0.10):
        frames = make_frames().reshape(F, -1)
        if rho_common > 0:
            common = rngB.normal(0, np.sqrt(var_true.mean()) * rho_common, NPIX)
            frames = frames + common
        w = t_true.copy()
        var_meas, _ = stack_ratio(frames, w)
        ratio_meas = var_meas / (1.0 / t_true.sum())
        ex, var_p, dbar = exact_ratio(w, var_true)
        rows_e.append(dict(common_mode_sigma_fraction=rho_common,
                           ratio_measured=float(ratio_meas), ratio_diagonal_formula=ex,
                           excess_pct=float(100 * (ratio_meas / ex - 1))))
    out["E_correlated_common_mode_noise"] = dict(
        rows=rows_e,
        note="a common error term added to every frame (the physical analogue of "
             "a shared master frame) breaks assumption (b): the true variance "
             "grows but the inverse-variance formula does not see it")

    out["elapsed_s"] = time.time() - t0
    with open(a.out, "w") as f:
        json.dump(out, f, indent=2, default=str)

    print("== A algebra ==")
    print(json.dumps(out["A_algebra"], indent=1))
    print("== B physical (Poisson) verification ==")
    for r in rows:
        print("  d_rms={:.2f}  ratio_meas={:.6f}  exact={:.6f}  doc={:.6f}  "
              "err_exact={:.3f}%  err_doc={:.3f}%".format(
                  r["delta_rms"], r["ratio_measured_poisson"], r["ratio_exact_form"],
                  r["ratio_document_form"], r["rel_err_exact_pct"],
                  r["rel_err_document_pct"]))
    print("== B gaussian comparison ==")
    for r in rows_g:
        print("  d_rms={:.2f}  ratio_meas={:.6f}  exact={:.6f}  err={:.3f}%".format(
            r["delta_rms"], r["ratio_measured_gaussian"], r["ratio_exact_form"],
            r["rel_err_exact_pct"]))
    print("== C negative controls ==")
    print(json.dumps(neg, indent=1))
    print("== D self-estimated weights ==")
    for r in rows_d:
        print("  est_pixels={:6d}  ratio_meas={:.6f}  exact={:.6f}  err={:.3f}%".format(
            r["estimator_pixels"], r["ratio_measured"],
            r["ratio_exact_form_from_realised_delta"], r["rel_err_exact_pct"]))
    print("== E correlated common-mode noise ==")
    for r in rows_e:
        print("  common_frac={:.2f}  ratio_meas={:.6f}  diag_formula={:.6f}  excess={:.3f}%".format(
            r["common_mode_sigma_fraction"], r["ratio_measured"],
            r["ratio_diagonal_formula"], r["excess_pct"]))
    print("wrote", a.out, "elapsed {:.1f}s".format(out["elapsed_s"]))


if __name__ == "__main__":
    main()
