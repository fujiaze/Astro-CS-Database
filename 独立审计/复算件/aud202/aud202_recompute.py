#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-202 independent recomputation of the cross-frame absolute SNR chain.

Fully standalone: does NOT import or read any module from the repository's
experiment code (实验/absolute-snr/code).  Every formula below is re-derived
from the primary literature (Horne 1986 PASP 98 609 optimal extraction;
standard CCD equation) and implemented from scratch with numpy.

Runs in pure Python -- no compiled artefacts required.
"""
from __future__ import annotations

import json
import math
import os
import sys

import numpy as np

OUT = os.path.dirname(os.path.abspath(__file__))
REPO = r"F:\Astro dev\Astro CS Normalization Database"

RNG = np.random.default_rng(20260925)   # own seed, independent of the repo's 20260921


# ----------------------------------------------------------------------
# 0.  Moffat beta=4 profile, discretely normalised on a pixel grid
#     (repo's own declaration: I(r) = (1 + r^2/(2 sigma^2))^-4, sum P_i = 1)
# ----------------------------------------------------------------------
def moffat4_profile(sigma_px: float, half: int):
    j, i = np.mgrid[-half:half + 1, -half:half + 1]
    r2 = i.astype(float) ** 2 + j.astype(float) ** 2
    v = (1.0 + r2 / (2.0 * sigma_px * sigma_px)) ** (-4.0)
    P = v / v.sum()
    return P


def sum_p2_of(P):
    return float((P.ravel() ** 2).sum())


# ----------------------------------------------------------------------
# 1.  Horne 1986 optimal extraction, derived independently.
#     Model:  d_i = F*P_i + b_i + n_i,  Var(n_i) = sigma_i^2
#     Weighted LS with only the flux free:
#        F_hat = sum(P_i d_i / s_i^2) / sum(P_i^2 / s_i^2)
#        Var(F_hat) = 1 / sum(P_i^2 / s_i^2)
#     CCD equation in the ADU domain (g = e-/ADU):
#        sigma_i^2 = sigma_sky^2 + (RN/g)^2 + F_ADU*P_i/g
# ----------------------------------------------------------------------
def sigma_f_horne(F_adu, sigma_sky_adu, g, rn_e, P, double_count_rn=False):
    """Return sigma_F [ADU].  double_count_rn=True mimics the legacy defect:
    sigma_sky is an *empirical total rms* (already contains RN) yet (RN/g)^2
    is added again."""
    Pi = P.ravel()
    rn_term = (rn_e / g) ** 2 if (g > 0 and rn_e > 0) else 0.0
    var = sigma_sky_adu ** 2 + rn_term
    if double_count_rn:
        var = var + rn_term           # <-- the defect
    if g > 0:
        var = var + np.maximum(F_adu * Pi, 0.0) / g
    den = float((Pi * Pi / var).sum())
    return float(math.sqrt(1.0 / den))


def mc_sigma_f(F_e, sigma_px, sky_e, dark_e, rn_e, g, n_mc, half=24):
    """Forward physical simulation in the electron domain; return empirical
    scatter of the optimal-extraction flux estimate (ADU) and the true F (ADU)."""
    P = moffat4_profile(sigma_px, half)
    Pi = P.ravel()
    lam = F_e * Pi + sky_e + dark_e
    e = RNG.poisson(lam, size=(n_mc, Pi.size)).astype(float)
    if rn_e > 0:
        e = e + RNG.normal(0.0, rn_e, size=(n_mc, Pi.size))
    adu = e / g
    # per-frame sky level estimated from the stamp itself: the mean of the
    # profile-free part is not available, so use the known sky+dark expectation
    # in *electrons* converted to ADU -- this is the oracle-background arm,
    # matching the repo's "known sky" setting.
    b_adu = (sky_e + dark_e) / g
    # sigma_sky per frame: empirical total rms of the background-only pixels.
    # Use the analytic total (sky+dark shot + RN) so that the only difference
    # between arms is the estimator, not the input.
    sig_sky_adu = math.sqrt((sky_e + dark_e + rn_e ** 2) / g ** 2)
    F_hat = []
    for k in range(n_mc):
        var = sig_sky_adu ** 2 + np.maximum(F_e * Pi, 0.0) / g ** 2  # ADU^2
        # electrons -> ADU^2 Poisson variance: (F_e*P_i)/g^2  (== F_adu*P_i/g)
        w = 1.0 / var
        num = float((Pi * (adu[k] - b_adu) * w).sum())
        den = float((Pi * Pi * w).sum())
        F_hat.append(num / den)
    F_hat = np.asarray(F_hat)
    return float(F_hat.std(ddof=1)), F_e / g, float(F_hat.mean())


# ----------------------------------------------------------------------
# R1  sigma_F definition vs Monte-Carlo truth (Horne / CCD equation)
# ----------------------------------------------------------------------
def r1():
    base = dict(F_e=1000.0, sigma_px=1.5, sky_e=100.0, dark_e=0.5, rn_e=10.0, g=1.3)
    alt = dict(F_e=417.0, sigma_px=2.3, sky_e=39.0, dark_e=7.0, rn_e=3.4, g=2.7)
    out = {}
    for name, prm in (("base_point_same_as_repo", base), ("independent_alt_point", alt)):
        P = moffat4_profile(prm["sigma_px"], 24)
        sig_mc, F_true, mean_hat = mc_sigma_f(prm["F_e"], prm["sigma_px"], prm["sky_e"],
                                              prm["dark_e"], prm["rn_e"], prm["g"], 4000)
        # SHOT-ONLY sky rms (dark + sky), so that (RN/g)^2 is added exactly once.
        sig_sky_shot = math.sqrt((prm["sky_e"] + prm["dark_e"]) / prm["g"] ** 2)
        sig_def = sigma_f_horne(F_true, sig_sky_shot, prm["g"], prm["rn_e"], P,
                                double_count_rn=False)
        sig_dc = sigma_f_horne(F_true, sig_sky_shot, prm["g"], prm["rn_e"], P,
                               double_count_rn=True)
        se = sig_mc / math.sqrt(2 * (4000 - 1))
        out[name] = dict(
            params=prm, F_true_adu=F_true, mean_F_hat=mean_hat,
            sigma_f_def=sig_def, sigma_f_mc=sig_mc,
            z_def_vs_mc=(sig_def - sig_mc) / se,
            sigma_f_double_counted=sig_dc,
            double_count_bias_pct=100.0 * (sig_dc / sig_def - 1.0),
            bias_overestimate_pct=100.0 * (sig_def / sig_mc - 1.0))
    return out


# ----------------------------------------------------------------------
# R2  frame SNR monotonicity in sky brightness + asymptotic slope
# ----------------------------------------------------------------------
def r2():
    res = {}
    for tag, F_e, sig in (("bright_F2e5", 2.0e5, 1.6), ("faint_F300", 300.0, 1.6)):
        P = moffat4_profile(sig, 24)
        g, rn, dark = 1.3, 10.0, 0.5
        B = np.concatenate([[0.0], np.logspace(0, 8, 25)])
        i1e6 = int(np.argmin(np.abs(B - 1e6)))
        snr = []
        for b in B:
            sig_sky = math.sqrt((b + dark) / g ** 2)   # SHOT-only sky rms; RN added once inside
            sF = sigma_f_horne(F_e / g, sig_sky, g, rn, P, double_count_rn=False)
            snr.append((F_e / g) / sF)
        snr = np.asarray(snr)
        # monotone decreasing?
        mono = bool(np.all(np.diff(snr) < 0))
        # log-log slope in the sky-dominated decade
        m = B > 1e5
        A = np.vstack([np.log10(B[m]), np.ones(m.sum())]).T
        slope = float(np.linalg.lstsq(A, np.log10(snr[m]), rcond=None)[0][0])
        res[tag] = dict(monotone_decreasing=mono,
                        ratio_snr_at_1e6_vs_0=float(snr[i1e6] / snr[0]),
                        loglog_slope_sky_dominated=slope,
                        snr_first=float(snr[0]), snr_last=float(snr[-1]))
        # the "signal includes sky" traditional calibre for contrast
        trad = []
        for b in B:
            sig_sky = math.sqrt((b + dark) / g ** 2)
            sF = sigma_f_horne(F_e / g, sig_sky, g, rn, P)
            trad.append((F_e / g + b / g) / sF)
        res[tag]["traditional_ratio_at_1e6_over_at_0"] = float(np.asarray(trad)[i1e6] / np.asarray(trad)[0])
    return res


# ----------------------------------------------------------------------
# R6  audit of the published analytic inflation formula for the no-gain arm
#     07_noise_snr.md 4.2a branch 3:  SNR_rep/SNR_true = sqrt(1 + F/sigma_bg^2)
# ----------------------------------------------------------------------
def r6():
    g, rn, sky_e, dark_e = 1.3, 10.0, 100.0, 0.5
    sig_b2 = (sky_e + dark_e + rn ** 2) / g ** 2      # background variance, ADU^2
    rows = []
    for F_e in (10.0, 100.0, 1000.0, 1e4, 1e5, 1e6):
        F_adu = F_e / g
        best = None
        for sig_px in (1.0, 1.5, 2.0, 3.0):
            P = moffat4_profile(sig_px, 30)
            sp2 = sum_p2_of(P)
            sF_true = sigma_f_horne(F_adu, math.sqrt((sky_e + dark_e) / g ** 2), g, rn, P)
            sF_nogain = math.sqrt(sig_b2 / sp2)
            exact = sF_nogain / sF_true                 # = SNR_true/SNR_rep ... see note
            rep_over_true = sF_true / sF_nogain         # inflation of reported SNR
            doc = math.sqrt(1.0 + F_adu / sig_b2)       # published formula, verbatim
            dim_free = math.sqrt(1.0 + F_adu * sp2 / sig_b2)   # dimensionally consistent
            rows.append(dict(F_e=F_e, sigma_px=sig_px, sum_p2=sp2,
                             snr_rep_over_snr_true=rep_over_true,
                             doc_formula_sqrt_1_plus_F_over_sigb2=doc,
                             dimfree_formula_sqrt_1_plus_F_sumP2_over_sigb2=dim_free,
                             doc_over_exact=doc / rep_over_true,
                             dimfree_over_exact=dim_free / rep_over_true))
    return dict(rows=rows,
                units_note="F [ADU] / sigma_bg^2 [ADU^2] is ADU^-1 => not dimensionless",
                doc_quoted_points=dict(phi_0p5_claim_pct=41.0, phi_0p95_claim_pct=347.0))


# ----------------------------------------------------------------------
# R3  pairing identity w = SNR^2/F_ref^2 == 1/sigma_F^2
# ----------------------------------------------------------------------
def r3():
    P = moffat4_profile(1.5, 24)
    worst = 0.0
    rows = []
    for g, rn, sky, F in ((1.3, 10.0, 100.0, 1000.0), (2.7, 3.4, 39.0, 417.0),
                          (0.6, 60.0, 1e5, 3e4), (4.0, 0.0, 1e-2, 12.0)):
        sig_sky = math.sqrt((sky + rn ** 2) / g ** 2)
        sF = sigma_f_horne(F / g, sig_sky, g, rn, P)
        m_ref = 6.0
        zp = 24.5
        F_ref = 10 ** (-0.4 * (m_ref - zp)) * 1e-17
        snr = F_ref / sF
        w_pair = snr ** 2 / F_ref ** 2
        w_true = 1.0 / sF ** 2
        rel = abs(w_pair - w_true) / w_true
        worst = max(worst, rel)
        rows.append(dict(g=g, rn=rn, sky=sky, F=F, rel=rel))
    return dict(worst_relative_identity_error=worst, rows=rows,
                note="identity is algebraically exact when SNR is defined as F_ref/sigma_F")


# ----------------------------------------------------------------------
# R4  what the PRODUCTION path actually yields (gain unknown -> sky-limited,
#     sigma_sky = PSF-fit residual), vs the true total-variance weight.
# ----------------------------------------------------------------------
def r4():
    """Quantify (a) the SNR upper-bound inflation of the no-gain path and
    (b) the weight error when the background ivar is used as the stacking
    weight instead of the source-including total variance."""
    g, rn, sky_e, dark_e = 1.3, 10.0, 100.0, 0.5
    out = {}
    for F_e in (10.0, 100.0, 1000.0, 3e4, 1e6):
        sig = 1.5
        P = moffat4_profile(sig, 24)
        Pi = P.ravel()
        F_adu = F_e / g
        sig_total = math.sqrt((sky_e + dark_e + rn ** 2) / g ** 2)   # empirical TOTAL rms (prod. input)
        sig_shot = math.sqrt((sky_e + dark_e) / g ** 2)              # shot-only, RN added once below
        sF_true = sigma_f_horne(F_adu, sig_shot, g, rn, P)
        sF_nogain = sig_total / math.sqrt(sum_p2_of(P))        # production arm, gain<=0
        # background-only per-pixel ivar (what the Phase1 variance product holds)
        var_bg = sig_total ** 2 * np.ones_like(Pi)
        var_tot = sig_shot ** 2 + rn ** 2 / g ** 2 + np.maximum(F_adu * Pi, 0.0) / g
        # weight ratio at the brightest pixel vs at a background pixel
        out[F_e] = dict(
            snr_upper_bound_over_true=sF_true / sF_nogain,
            snr_pct_inflated=100.0 * (sF_true / sF_nogain - 1.0),
            max_over_weight_bg_vs_total=float((1 / var_bg).max() / (1 / var_tot).max()),
            centre_pixel_weight_overestimate_vs_total=float(
                (1 / var_bg)[Pi.argmax()] / (1 / var_tot)[Pi.argmax()]),
            peak_Pi=float(Pi.max()))
    return out


# ----------------------------------------------------------------------
# R5  degeneracy self-test of the archived gates: do they stay green when the
#     physical effect they claim to detect is removed?
# ----------------------------------------------------------------------
def r5():
    """Re-run the B1 monotonicity/slope measurement on an input set with the
    sky-shot-noise effect REMOVED (sigma_sky held constant while sky rises).
    A non-degenerate gate must report the defect."""
    P = moffat4_profile(1.6, 24)
    g, rn, dark = 1.3, 10.0, 0.5
    B = np.logspace(0, 8, 25)
    F_adu = 300.0 / g
    # defective arm: sigma_sky frozen at its B=1 value -> no sky shot noise
    frozen = math.sqrt((1.0 + dark + rn ** 2) / g ** 2)
    snr_frozen = np.array([F_adu / sigma_f_horne(F_adu, frozen, g, rn, P) for _ in B])
    slope = float(np.linalg.lstsq(np.vstack([np.log10(B[15:]), np.ones(len(B[15:]))]).T,
                                 np.log10(snr_frozen[15:]), rcond=None)[0][0])
    return dict(frozen_sky_monotone_decreasing=bool(np.all(np.diff(snr_frozen) < 0)),
                frozen_snr_constant_to_1e7=float(snr_frozen[-8] / snr_frozen[0]),
                loglog_slope_frozen_arm=slope,
                expected_slope_if_effect_present=-0.5)


def main():
    rep = {"R1_sigma_f_vs_mc": r1(), "R2_sky_monotonicity": r2(),
           "R3_pairing_identity": r3(), "R4_production_paths": r4(),
           "R5_degeneracy_selftest": r5(), "R6_doc_analytic_formula": r6()}
    path = os.path.join(OUT, "aud202_recompute.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(rep, f, indent=2, ensure_ascii=False)
    json.dump(rep, sys.stdout, indent=2, ensure_ascii=False)
    print("\nwrote", path)


if __name__ == "__main__":
    main()
