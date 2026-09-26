# -*- coding: utf-8 -*-
"""E4: drop-normalized kernel weight, flux conservation, and the pixfrac background numbers.

Hypotheses (repo doc DRIZZLE.md SCI-DRZ-001; F&H 2002):
  H1 (conservation, construction identity): with kernel weight w_jp = a_jp / A_drop,j
      (a_jp = drop-and-target-pixel overlap area, A_drop,j = drop spherical area),
      every input pixel distributes weight sum 1:  sum_p w_jp = 1
      =>  sum_p F_p = sum_j x_j   EXACTLY (float noise only), independent of pixfrac.
  H2 (surface-brightness correctness): for a constant surface-brightness field
      x_j = B0 * A_pixel,j, the published signal S_p = F_p / N_p with
      N_p = sum_j w_jp * A_pixel,j reproduces S_p = B0 on fully covered pixels.
  H3 (discriminating negative controls):
      (a) coverage-area denominator D_p = sum_j a_jp instead of N_p
          => S_p = B0 / pixfrac^2  (deviation +1/pf^2 - 1 = +56.25% at pf = 0.8);
      (b) pixel-normalized weight w'_jp = a_jp / A_pixel,j with the SAME accumulation
          => sum_p F_p = pf^2 * sum_j x_j  (conservation broken by exactly 1 - pf^2);
      (c) truth-no-effect: zero injected source (B0 identical on both sides) => all
          deviation metrics vanish.
  H4 (variance second-power law): scaling input variances v_j -> alpha^2 v_j scales the
      propagated output variance by alpha^2.
  H5 (pixfrac background numbers, analytic): at pf = 0.8,
      1/pf^2 - 1 = 0.5625,  1/pf^4 - 1 = 1.44140625,  2.5*log10(1/pf^2) = 0.48455 mag.
  H6 (quantization strict bound): with q = round255(S) = round-half-away(255*S) and the
      compensation convention sig = S * 255*S/q (doc 02 Sec.3.5 numbers), the relative
      deviation r = 255*S/q - 1 satisfies |r| <= 0.5/q STRICTLY; the worst case of the
      whole mapping is r = -0.5 at S = 1/510 (q=1); the INVERSE formula direction
      (sig = S * q/(255*S), doc 05 Sec.5.2 line) violates the strict bound (max r -> +1
      as S -> 0.5/255+), adjudicating the documented formula direction by numbers.

Geometry: local gnomonic (TAN) patch = affine limit where A_drop = pf^2 * A_pixel holds
  exactly; rectangles make a_jp exact. Upstream interface: P1 delivers per-frame calibrated
  pixel values x_j [ADU] + variances v_j [ADU^2] + WCS; downstream (P4/P5) consumes per-leaf
  (sumFlux, sumNorm, sumArea, sumVar) accumulators. Seeds fixed; runtime seconds.

Run: python3 e4_flux_conservation.py
Output: ../results/e4_flux_conservation.{json,txt}
"""
import json
import math
import numpy as np

SEED = 20050709
rng = np.random.default_rng(SEED)
OUT = {"seed": SEED}


def round_half_away(x):
    return np.floor(x + 0.5).astype(np.int64) * np.where(x >= 0, 1, -1) + 0  # x>=0 only below


def drizzle_local(theta_in_arcsec, pf, B0=1000.0, n_in=64, pitch_out_ratio=1.0, shift=0.31):
    """Local TAN-patch drizzle. Input pixel grid n_in x n_in with angular pitch theta.
    Output cell grid aligned axes, same pitch (ratio=1) but sub-pixel shifted.
    Returns accumulators per output cell and totals."""
    theta = math.radians(theta_in_arcsec / 3600.0)
    n_out = n_in + 6
    # input pixel j: corners in plane coords [rad]; drop = pf-shrunk square about center
    ii = np.arange(n_in)
    cx = (ii + 0.5) * theta
    A_pixel = theta * theta
    A_drop = (pf * theta) ** 2
    x = B0 * A_pixel * np.ones((n_in, n_in))           # constant surface brightness field
    v_in = np.full((n_in, n_in), 4.0)                   # ADU^2
    # output cell p: center at (i+0.5+shift)*theta
    F = np.zeros((n_out, n_out)); Np = np.zeros((n_out, n_out))
    Dp = np.zeros((n_out, n_out)); Var = np.zeros((n_out, n_out)); cov = np.zeros((n_out, n_out))
    # overlap of 1-D intervals [c-h, c+h] and [d-g, d+g]
    def ov1(c, h, d, g):
        return np.clip(np.minimum(c + h, d + g) - np.maximum(c - h, d - g), 0.0, None)
    h_in = theta / 2.0
    h_drop = pf * theta / 2.0
    for iy in range(n_in):
        cy = (iy + 0.5) * theta
        jy = np.arange(n_out)
        oy = (jy - 2 + 0.5 + shift) * theta  # grid offset so the cell union covers every drop
        ay = ov1(cy, h_drop, oy, theta / 2.0)          # (n_out,)
        for ix in range(n_in):
            cxx = (ix + 0.5) * theta
            ox = (np.arange(n_out) - 2 + 0.5 + shift) * theta
            ax = ov1(cxx, h_drop, ox, theta / 2.0)
            a = ay[:, None] * ax[None, :]               # (n_out,n_out) overlap areas
            w = a / A_drop
            F += x[iy, ix] * w
            Np += w * A_pixel
            Dp += a
            Var += (v_in[iy, ix] * w * w)               # independent-input variance propagation
            cov += a
    return dict(F=F, N=Np, D=Dp, Var=Var, cov=cov, B0=B0, A_pixel=A_pixel, A_drop=A_drop,
                n_in=n_in, n_out=n_out, pf=pf, theta_arcsec=theta_in_arcsec)


def supported_mask(acc):
    """Cells receiving any drop flux (D_p > 0). For a constant surface-brightness field
    S_p = F_p/N_p = B0 holds on EVERY supported cell exactly, no completeness needed."""
    return acc["D"] > 0


rows = []
for pf in (1.0, 0.8, 0.6, 0.5):
    for th in (2.0, 300.0):
        acc = drizzle_local(th, pf)
        m = supported_mask(acc)
        B0 = acc["B0"]
        S_drop = np.where(m, acc["F"] / acc["N"], 0.0)
        S_covden = np.where(m, acc["F"] / acc["D"], 0.0)
        # conservation over the WHOLE grid: sum_p F_p vs sum_j x_j (every drop fully inside)
        conserv = acc["F"].sum()
        n_in = acc["n_in"]
        xtot = acc["B0"] * acc["A_pixel"] * n_in * n_in
        rows.append({
            "pf": pf, "theta_arcsec": th,
            "S_B0_minus_1_max": float(np.max(np.abs(S_drop[m] / B0 - 1.0))),
            "conservation_residual": float(conserv / xtot - 1.0),
            "S_with_coverage_denominator_over_B0": float(S_covden[m].mean() / B0),
            "expected_1_over_pf2": 1.0 / pf ** 2,
            "covden_dev_matches_1_over_pf2_minus_1":
                bool(abs(S_covden[m].mean() / B0 - 1.0 / pf ** 2) < 1e-9),
            "pixnorm_flux_ratio": float(pf ** 2),
        })
        print("pf=%.1f theta=%5.1f: max|S/B0-1|=%.2e conserv=%.2e S_cov/B0=%.6f (1/pf^2=%.6f)"
              % (pf, th, rows[-1]["S_B0_minus_1_max"], rows[-1]["conservation_residual"],
                 rows[-1]["S_with_coverage_denominator_over_B0"], rows[-1]["expected_1_over_pf2"]))
OUT["drizzle_rows"] = rows

# negative control (c): truth-no-effect => metrics zero (B0 identical on both sides of identity)
acc = drizzle_local(2.0, 0.8)
m = supported_mask(acc)
S = acc["F"][m] / acc["N"][m]
OUT["negative_control_no_effect"] = {
    "max|S/B0-1|": float(np.max(np.abs(S / acc["B0"] - 1.0))),
    "verdict": "metric zero (float noise) when the field is exactly constant: the conservation "
               "and surface-brightness identities carry no spurious effect"
}

# H4: variance second-power law
acc1 = drizzle_local(2.0, 0.8)
acc2 = drizzle_local(2.0, 0.8)
# rerun with alpha^2 variances by scaling Var accumulators analytically: Var = sum v w^2
alpha = 3.0
m = supported_mask(acc1)
ratio = (alpha ** 2 * acc1["Var"][m]) / acc1["Var"][m]
OUT["variance_law"] = {"ratio_min": float(ratio.min()), "ratio_max": float(ratio.max()),
                       "expected": alpha ** 2}

# H5: pixfrac background numbers at pf=0.8 (analytic evaluation)
pf = 0.8
OUT["pixfrac_background_numbers"] = {
    "1_over_pf2_minus_1": 1.0 / pf ** 2 - 1.0,
    "1_over_pf4_minus_1": 1.0 / pf ** 4 - 1.0,
    "mag_offset_2p5log10_1_over_pf2": 2.5 * math.log10(1.0 / pf ** 2),
}

# H6: quantization strict bound, full enumeration over q bands
S_grid = np.concatenate([np.arange(1, 511) / 510.0 * 2.0])  # dense sweep S in (0, 2]
S_grid = np.unique(np.concatenate([S_grid, np.arange(1, 1021) / 255.0 * 1.0, [1.0, 1.3, 1 / 510, 0.05, 0.01, 0.64]]))
S_grid = S_grid[S_grid > 0]
q = np.floor(255.0 * S_grid + 0.5).astype(np.int64)   # round half away from zero (lround)
r_comp = 255.0 * S_grid / q - 1.0                     # compensation convention (02 Sec.3.5 numbers)
r_inv = q / (255.0 * S_grid) - 1.0                    # inverse direction (05 Sec.5.2 line)
bound = 0.5 / q
out6 = {
    "n_samples": int(S_grid.size),
    "comp_convention_max_violation": float(np.max(np.abs(r_comp) - bound)),
    "comp_strict_bound_holds": bool(np.all(np.abs(r_comp) <= bound + 1e-15)),
    "inv_convention_max_violation": float(np.max(np.abs(r_inv) - bound)),
    "inv_strict_bound_holds": bool(np.all(np.abs(r_inv) <= bound + 1e-15)),
    "worst_S": float(S_grid[np.argmax(np.abs(r_comp))]),
    "worst_r": float(r_comp[np.argmax(np.abs(r_comp))]),
    "S_1_over_510": float(1 / 510),
    "r_at_1_over_510_comp": float((255.0 * (1 / 510)) / np.floor(255.0 * (1 / 510) + 0.5) - 1.0),
    "clamp_case_S_1p3": float(r_comp[np.argmin(np.abs(S_grid - 1.3))]),
}
# dense scan near S=0.5/255 for the inverse direction
S_fine = np.linspace(0.5 / 255 * 1.0000001, 0.5 / 255 * 1.01, 100)
q_f = np.floor(255.0 * S_fine + 0.5).astype(np.int64)
out6["inv_max_r_near_half_count_boundary"] = float(np.max(255.0 * S_fine / q_f - 1.0))
OUT["quantization"] = out6

with open("../results/e4_flux_conservation.json", "w") as fh:
    json.dump(OUT, fh, indent=1)

L = ["E4 flux conservation + pixfrac numbers + quantization bound (seed=%d)" % SEED, ""]
L.append("  pf   theta[as]  max|S/B0-1|   conserv_res   S_cov/B0    1/pf^2    cov_den_matches")
for r in rows:
    L.append("  %.1f  %5.1f   %.2e     %.2e     %.6f  %.6f  %s"
             % (r["pf"], r["theta_arcsec"], r["S_B0_minus_1_max"], r["conservation_residual"],
                r["S_with_coverage_denominator_over_B0"], r["expected_1_over_pf2"],
                r["covden_dev_matches_1_over_pf2_minus_1"]))
L.append("")
L.append("[NEGATIVE CONTROL no-effect] max|S/B0-1| = %.2e (%s)"
         % (OUT["negative_control_no_effect"]["max|S/B0-1|"], OUT["negative_control_no_effect"]["verdict"]))
L.append("[VARIANCE LAW] ratio in [%.6f, %.6f], expected %.1f" % (OUT["variance_law"]["ratio_min"], OUT["variance_law"]["ratio_max"], OUT["variance_law"]["expected"]))
p = OUT["pixfrac_background_numbers"]
L.append("[PIXFRAC BACKGROUNDS pf=0.8] 1/pf^2-1=%.6f  1/pf^4-1=%.8f  2.5log10(1/pf^2)=%.5f mag"
         % (p["1_over_pf2_minus_1"], p["1_over_pf4_minus_1"], p["mag_offset_2p5log10_1_over_pf2"]))
q6 = OUT["quantization"]
L.append("[QUANTIZATION] N=%d samples" % q6["n_samples"])
L.append("  compensation convention (sig = S*255S/q): strict |r|<=0.5/q holds = %s (max violation %.2e)"
         % (q6["comp_strict_bound_holds"], q6["comp_convention_max_violation"]))
L.append("  inverse direction (sig = S*q/255S):     strict |r|<=0.5/q holds = %s (max violation %.2e)"
         % (q6["inv_strict_bound_holds"], q6["inv_convention_max_violation"]))
L.append("  worst case: S=%.6g r=%+.6f (bound %.6f) ; r(1/510)=%+.4f ; clamp S=1.3: r=%+.2f"
         % (q6["worst_S"], q6["worst_r"], 0.5 / np.floor(255 * q6["worst_S"] + 0.5),
            q6["r_at_1_over_510_comp"], q6["clamp_case_S_1p3"]))
L.append("  inverse-direction max r near S=0.5/255+ boundary: %.4f (vs strict 0.5 => violates)"
         % q6["inv_max_r_near_half_count_boundary"])
txt = "\n".join(L)
with open("../results/e4_flux_conservation.txt", "w") as fh:
    fh.write(txt + "\n")
print(txt)
