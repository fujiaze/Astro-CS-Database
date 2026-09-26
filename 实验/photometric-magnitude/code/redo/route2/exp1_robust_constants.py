#!/usr/bin/env python3
"""exp1_robust_constants.py -- P1 route2 experiment 1: robust statistics constants.

Items covered:
  S1  Tukey biweight c=4.685 (95% Gaussian asymptotic efficiency)   [01/C11]
  S2  MAD->sigma constant 0.6744897501960817 = Phi^-1(3/4),
      and 1/0.6744897501960817 = 1.482602218505602
  S3  IRLS convergence tol 1e-6 / max iterations 50
  S10 finite-sample bias of MAD-type scale (Croux & Rousseeuw 1992 table)

All seeds are fixed. Pure python + numpy. No repo imports.
Run:  python3 exp1_robust_constants.py    (writes ../results/exp1_robust_constants.json)
"""
import json
import math
import os

import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)

CODE_MAD_SCALE = 0.6744897501960817     # star_matcher.cpp:21 _MAD_SCALE
CODE_1_OVER    = 1.482602218505602      # 1/MAD_SCALE, NOISE_ESTIMATION.md:213
CODE_TUKEY_C   = 4.685                  # star_matcher.cpp:23 _TUKEY_C
CODE_TOL       = 1e-6                   # star_matcher.cpp:27 _IRLS_CONVERGE
CODE_MAX_ITER  = 50                     # star_matcher.cpp:25 _IRLS_MAX_ITER

def phi(x):
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))

def phi_inv(p):
    lo, hi = -10.0, 10.0
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if phi(mid) < p:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)

q34 = phi_inv(0.75)
rel_err_q34 = abs(q34 - CODE_MAD_SCALE) / CODE_MAD_SCALE
inv_q34 = 1.0 / q34
rel_err_inv = abs(inv_q34 - CODE_1_OVER) / CODE_1_OVER
rel_err_trunc = abs(1.4826 - CODE_1_OVER) / CODE_1_OVER

_gx = np.linspace(-10.0, 10.0, 2000001)
_QX = _gx
_QW = np.full_like(_gx, (_gx[1] - _gx[0]))  # trapezoid: scaled later via dot without halved ends
_QW[0] *= 0.5
_QW[-1] *= 0.5

def tukey_are(c, n_quad=None):
    x, w = _QX, _QW
    pdf = np.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)
    psi = x * (1.0 - (x / c) ** 2) ** 2 * (np.abs(x) < c)
    dpsix = 1.0 - (x / c) ** 2
    dpsi = dpsix * (dpsix - 4.0 * (x / c) ** 2) * (np.abs(x) < c)
    a1 = float(np.sum(w * dpsi * pdf))
    a2 = float(np.sum(w * psi * psi * pdf))
    return (a1 * a1) / a2, a1, a2

are_4685, a1_4685, a2_4685 = tukey_are(CODE_TUKEY_C)

lo, hi = 2.0, 8.0
for _ in range(200):
    mid = 0.5 * (lo + hi)
    if tukey_are(mid)[0] < 0.95:
        lo = mid
    else:
        hi = mid
c_095 = 0.5 * (lo + hi)

eff_at_4685065 = tukey_are(4.685065)[0]
eff_at_3882662 = tukey_are(3.882662)[0]
eff_at_5182361 = tukey_are(5.182361)[0]

ptail = 2.0 * (1.0 - phi(CODE_TUKEY_C))

def identity_are():
    x, w = _QX, _QW
    pdf = np.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)
    a1 = float(np.sum(w * 1.0 * pdf))
    a2 = float(np.sum(w * x * x * pdf))
    return (a1 * a1) / a2

eff_identity = identity_are()

print("S1/S2 analytic parts done", flush=True)

# ---- S10: finite-sample bias of MAD-type scale, per-row MAD ----
def mad_scale_rows(xs):
    med = np.median(xs, axis=1, keepdims=True)
    return np.median(np.abs(xs - med), axis=1) / CODE_MAD_SCALE

ns = [3, 4, 5, 9, 12, 20, 50, 200]
reps_map = {3: 4000000, 4: 4000000, 5: 4000000, 9: 2000000, 12: 2000000,
            20: 1000000, 50: 400000, 200: 200000}
mad_bias = {}
for n in ns:
    reps = reps_map[n]
    acc = 0.0
    acc2 = 0.0
    remaining = reps
    while remaining > 0:
        b = min(remaining, 200000)
        xs = rng.standard_normal((b, n))
        s = mad_scale_rows(xs)
        acc += float(s.sum()); acc2 += float((s * s).sum())
        remaining -= b
    mean_s = acc / reps
    var_s = acc2 / reps - mean_s ** 2
    mad_bias[str(n)] = {"mean_s_over_sigma": mean_s, "bias_factor": 1.0 / mean_s,
                        "sd_s": math.sqrt(max(var_s, 0.0)), "reps": reps}
    print("S10 n=%d mean_s=%.6f bias=%.4f" % (n, mean_s, 1.0 / mean_s), flush=True)

lit_bn = {3: 1.495, 4: 1.363, 5: 1.206, 9: 1.107}
for n, b in lit_bn.items():
    mad_bias[str(n)]["lit_bias_factor"] = b
    mad_bias[str(n)]["rel_dev_vs_lit"] = abs(mad_bias[str(n)]["bias_factor"] - b) / b

# ---- S3: IRLS tol / max-iter sensitivity ----
def irls_tukey(r, tol=CODE_TOL, max_iter=CODE_MAX_ITER, c=CODE_TUKEY_C):
    location = float(np.median(r))
    mad = float(np.median(np.abs(r - location)))
    S = mad / CODE_MAD_SCALE if mad > 0 else 0.0
    iters = 0
    if S > 0:
        prev = location
        for it in range(max_iter):
            iters = it + 1
            u = (r - location) / (c * S)
            w = np.where(np.abs(u) < 1.0, (1.0 - u * u) ** 2, 0.0)
            sw = w.sum()
            if sw <= 0:
                break
            new = float((w * r).sum() / sw)
            location = new
            if abs(new - prev) < tol:
                break
            prev = new
    return location, S, iters

def run_irls_suite(contam_frac, out_shift, n=200, reps=2000):
    res = {}
    for tol in (1e-2, 1e-6, 1e-9, 1e-12):
        locs = []
        iters_max = 0
        hits50 = 0
        for k in range(reps):
            r = rng.standard_normal(n) * 0.05
            m = rng.random(n) < contam_frac
            r[m] += out_shift
            loc, S, it = irls_tukey(r, tol=tol)
            locs.append(loc)
            iters_max = max(iters_max, it)
            if it >= CODE_MAX_ITER:
                hits50 += 1
        res["1e-%d" % round(-math.log10(tol))] = {"median_location": float(np.median(locs)),
                                                  "max_iters_seen": int(iters_max),
                                                  "n_hits_50_cap": int(hits50)}
        print("S3 tol=%g done" % tol, flush=True)
    return res

suite_clean = run_irls_suite(0.0, 0.0, n=200, reps=2000)
suite_dirty = run_irls_suite(0.20, 0.8, n=200, reps=2000)

clean_span = abs(suite_clean["1e-2"]["median_location"] - suite_clean["1e-12"]["median_location"])
dirty_span = abs(suite_dirty["1e-2"]["median_location"] - suite_dirty["1e-12"]["median_location"])

out = {
    "seed": SEED,
    "S2_phi_inv_3_4": {
        "bisection_value": q34,
        "code_literal": CODE_MAD_SCALE,
        "rel_err": rel_err_q34,
        "inv_value": inv_q34,
        "code_inv_literal": CODE_1_OVER,
        "inv_rel_err": rel_err_inv,
        "trunc_14826_rel_err": rel_err_trunc,
    },
    "S1_tukey_are": {
        "are_at_c_4685": are_4685,
        "c_solved_for_eff_095": c_095,
        "statsmodels_095_c": 4.685065,
        "eff_at_statsmodels_095_c": eff_at_4685065,
        "eff_at_statsmodels_090_c_3_882662": eff_at_3882662,
        "eff_at_statsmodels_010_c_5_182361": eff_at_5182361,
        "p_tail_gt_4685": ptail,
        "eff_identity_psi_x": eff_identity,
        "identity_deviation_from_1": abs(eff_identity - 1.0),
    },
    "S10_mad_finite_sample_bias": mad_bias,
    "S3_irls_tol_suite_clean_field": suite_clean,
    "S3_irls_tol_suite_dirty_field": suite_dirty,
    "S3_summary": {
        "clean_field_median_location_span_1e2_vs_1e12": clean_span,
        "dirty_field_median_location_span_1e2_vs_1e12": dirty_span,
        "negative_control_fired": bool(dirty_span > 10.0 * max(clean_span, 1e-12)),
    },
}

here = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(here, "..", "results"), exist_ok=True)
path = os.path.join(here, "..", "results", "exp1_robust_constants.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)
print("WROTE", path, flush=True)

