#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-04: P-CST-05 (Moffat4 FWHM<->sigma factor 1.230310)

A literature leg  : Moffat, A. F. J. 1969, A&A 3, 455 (Moffat profile).
B experiment leg  : closed-form derivation + numerical verification.
  2-D Moffat  I(r) = (beta-1)/(pi alpha^2) * (1 + r^2/alpha^2)^(-beta).
  Normalisation: total = 1.  Second moment (finite iff beta > 2):
      <r^2> = alpha^2 / (beta - 2)   =>  sigma = alpha / sqrt(beta - 2).
  FWHM: I(FWHM/2)/I(0) = 1/2  =>  FWHM = 2 alpha sqrt(2^(1/beta) - 1).
  For beta = 4:  FWHM/sigma = 2 sqrt(2^(1/4) - 1) * sqrt(2).
  Numerical: radial quadrature of the second moment (substitution u = r/alpha,
  geometric-grid trapezoid to large radius) and a bisection for FWHM.
  Applicability-domain negative control: beta = 2 exactly -> second-moment
  integral diverges logarithmically (truncated integral grows without bound).
C seed             : no randomness (deterministic quadrature); SEED recorded as
                     20260926 for harness consistency (unused).
D outputs          : results/exp04_moffat4_sigma.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np
from math import sqrt

OUT = "results/exp04_moffat4_sigma.json"
SEED = 20260926
BETA = 4.0

def moffat_second_moment_numeric(beta, alpha=1.0, n=2_000_000, rmax=None):
    """<r^2> = 2(beta-1) int_0^inf r^3 (1+r^2/a^2)^(-beta) dr / a^2... in units of alpha."""
    if rmax is None:
        rmax = alpha * 1e5
    # substitute t = r^2:  <r^2> = (beta-1) * int_0^inf t (1+t/a^2)^(-beta) dt
    # integrate in log space for the heavy tail
    t = np.exp(np.linspace(np.log(1e-12 * alpha ** 2), np.log(rmax ** 2), n))
    dt = np.diff(t, prepend=t[0])
    integrand = t * (1.0 + t / alpha ** 2) ** (-beta)
    return (beta - 1.0) * float(np.sum(integrand * dt))

def moffat_fwhm_numeric(beta, alpha=1.0):
    # I(r)/I(0) = (1 + r^2/a^2)^(-beta) = 1/2
    return 2.0 * alpha * sqrt(2.0 ** (1.0 / beta) - 1.0)

def main():
    res = {"seed": SEED, "beta": BETA}
    # closed form
    sig_over_alpha = 1.0 / sqrt(BETA - 2.0)
    fwhm_over_alpha = moffat_fwhm_numeric(BETA)
    ratio_closed = fwhm_over_alpha / sig_over_alpha
    reg = 1.230310
    res["closed_form_fwhm_over_sigma"] = float(ratio_closed)
    res["registered"] = reg
    res["rel_diff_registered_vs_closed"] = float((reg - ratio_closed) / ratio_closed)
    res["components"] = {
        "fwhm_over_alpha": float(fwhm_over_alpha),
        "sigma_over_alpha_closed": float(sig_over_alpha),
    }
    # numeric second moment (alpha = 1)
    m2 = moffat_second_moment_numeric(BETA)
    sigma_num = sqrt(m2)
    res["numeric_second_moment"] = m2
    res["numeric_sigma_over_alpha"] = float(sigma_num)
    res["numeric_fwhm_over_sigma"] = float(fwhm_over_alpha / sigma_num)
    res["numeric_vs_registered_rel"] = float((reg - fwhm_over_alpha / sigma_num) /
                                             (fwhm_over_alpha / sigma_num))
    # applicability-domain negative control: beta = 2 divergence
    growth = []
    for rmax in (1e2, 1e3, 1e4, 1e5, 1e6):
        growth.append({"rmax_over_alpha": rmax,
                       "truncated_second_moment": moffat_second_moment_numeric(2.0, rmax=rmax)})
    res["beta2_divergence_negative_control"] = growth
    # ratio of the two profile blocks (P-CST-04 vs P-CST-05), for the record
    res["gaussian_over_moffat4_ratio"] = float(2.3548200450309493 / ratio_closed)
    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
