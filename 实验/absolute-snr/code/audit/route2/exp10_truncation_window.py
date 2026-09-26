#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-10: P-CST-23 (profile truncation half-window min(256, max(30, ceil(12*FWHM))))

A literature leg  : NOISE_MODEL.md section 9a sigma_i^2 composition;
                    PSF_SIGNAL_WEIGHT (A_NEA = 1/sum P^2); matched-filter
                    (optimal) extraction theory.
B experiment leg  : analytic sensitivity curve of the truncation window
                    (P-GATE-13 style), closed-form 2-D integrals.
  Profiles (both normalised to unit total mass on the infinite plane):
    Gaussian          P(r)   = (2 pi sigma^2)^{-1} exp(-r^2/(2 sigma^2))
      encircled mass   f_W   = 1 - exp(-h^2/(2 sigma^2))
      sum P^2 (all)          = 1/(4 pi sigma^2)
      sum P^2 (window)       = (1/(4 pi sigma^2)) (1 - exp(-h^2/sigma^2))
    Moffat beta       P(r)   = (beta-1)/(pi alpha^2) (1+r^2/alpha^2)^{-beta}
      encircled mass   f_W   = 1 - (1 + h^2/alpha^2)^{1-beta}
      sum P^2 (all)          = (beta-1)^2 / (pi alpha^2 (2 beta - 1))
      sum P^2 (window)       = sum_all * (1 - (1+h^2/alpha^2)^{1-2 beta})
  Optimal extraction with truncated normalised weights p = P/sum_W P:
    S1 consistent truncation: reported flux f_W F, SNR bias =
         f_W^2 sqrt(sum_all P^2 / sum_W P^2) - 1                      (<= 0)
    S2 flux exact (P1 photometry), truncated weights: SNR bias =
         sqrt(sum_all P^2 / sum_W P^2) - 1                            (<= 0)
  Checked: V-7 ("sum P^2 stable from 5th digit for half 12 -> 60 FWHM units")
  and the 05 series +4e-6 / +2.4e-4 / +1.33% (NOT reproduced under S1 or S2;
  sign and magnitude differ -> anchor UNRESOLVED, our curve replaces it).
C seed             : SEED = 20260926 (deterministic analytic evaluation).
D outputs          : results/exp10_truncation_window.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np
from math import sqrt, ceil, log, exp, pi

SEED = 20260926
OUT = "results/exp10_truncation_window.json"

def gauss_terms(fwhm, h):
    sig = fwhm / (2.0 * sqrt(2.0 * log(2.0)))
    f_w = 1.0 - exp(-h * h / (2.0 * sig * sig))
    s2_all = 1.0 / (4.0 * pi * sig * sig)
    s2_w = s2_all * (1.0 - exp(-h * h / (sig * sig)))
    return f_w, s2_all, s2_w

def moffat_terms(fwhm, beta, h):
    alpha = fwhm / (2.0 * sqrt(2.0 ** (1.0 / beta) - 1.0))
    u = 1.0 + h * h / (alpha * alpha)
    f_w = 1.0 - u ** (1.0 - beta)
    s2_all = (beta - 1.0) ** 2 / (pi * alpha * alpha * (2.0 * beta - 1.0))
    s2_w = s2_all * (1.0 - u ** (1.0 - 2.0 * beta))
    return f_w, s2_all, s2_w

def main():
    res = {"seed": SEED}
    rows = []
    cases = (("gauss3", 3.0, gauss_terms), ("gauss10", 10.0, gauss_terms),
             ("moffat4_3", 3.0, lambda f, h: moffat_terms(f, 4.0, h)),
             ("moffat4_10", 10.0, lambda f, h: moffat_terms(f, 4.0, h)),
             ("moffat4_30", 30.0, lambda f, h: moffat_terms(f, 4.0, h)),
             ("moffat4_60", 60.0, lambda f, h: moffat_terms(f, 4.0, h)),
             # repo-typical Moffat beta=2.5, FWHM series of the 05 claimed values
             ("moffat2p5_60", 60.0, lambda f, h: moffat_terms(f, 2.5, h)),
             ("moffat2p5_120", 120.0, lambda f, h: moffat_terms(f, 2.5, h)),
             ("moffat2p5_260", 260.0, lambda f, h: moffat_terms(f, 2.5, h)))
    BIG = 1e6  # effectively infinite window
    for name, fwhm, terms in cases:
        _, s2_all, _ = terms(fwhm, BIG)
        h_prod = min(256, max(30, int(ceil(12.0 * fwhm))))
        for label, half in (("rule_30_12_256", h_prod),
                            ("uncapped_12fwhm", min(int(ceil(12.0 * fwhm)), 20000)),
                            ("h30", 30), ("h60", 60), ("h120", 120), ("h256", 256)):
            f_w, _, s2_w = terms(fwhm, half)
            rows.append({
                "profile": name, "fwhm": fwhm, "half_px": half,
                "encircled_fraction": float(f_w),
                "sumP2_ratio_w_over_all": float(s2_w / s2_all),
                "snr_bias_S1_consistent": float(f_w ** 2 * sqrt(s2_all / s2_w) - 1.0),
                "snr_bias_S2_flux_exact": float(sqrt(s2_all / s2_w) - 1.0)})
    res["sensitivity_curve"] = rows

    # V-7 check (Gaussian, FWHM=3): sum P^2 ratio for half 36 -> 60 -> 180 -> 720
    v7 = {}
    for half in (36, 60, 180, 720):
        _, _, s2_w = gauss_terms(3.0, half)
        v7[f"half{half}_px"] = float(s2_w / s2_all)
    res["v7_check_gauss_fwhm3"] = v7

    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
