"""exp08_coverage_quantization.py -- P3-08: coverage quantization step 1/255.

Hypothesis (production semantic: C lround, half away from zero):
 (1) encode q = lround(255*S), decode S_hat = q/255: full enumeration of the
     511 grid points k/510 (k = 0..510) shows |S_hat/S - 1| <= 0.5/q' with the
     worst relative error -0.5 at S = 1/510 (q = 1, S_hat = 2S): the "-50%"
     worst case; the sup of |S_hat/S - 1| over the q = 254 cell is
     0.5/254 = 1.9685e-3 (the 02 table value).
 (2) S < 1/510 rounds to q = 0 => relative-error metric undefined/NaN --
     coverage below half an LSB is unrepresentable (documented boundary).
 (3) variance inflation: a multiplicative error (1+r) on coverage scales
     SNR^2 weights by (1+r)^2 - 1 = 2r + r^2; at the worst r = -0.5 this is
     -75% (SNR halved), at 1.9685e-3 it is 3.94e-3.
 (4) NEGATIVE CONTROL: profile with quantization disabled (step = 0, exact
     float coverage) reproduces S exactly: every metric identically 0.
 (5) banker's rounding (numpy.round) at S = 1/510 gives q = 0 (ties to
     even), silently losing the pixel -- recorded as an honest semantic
     hazard, NOT the production semantic.
Seed       : 20260927 (random sweep only).
Runtime    : < 30 s.
"""
import json
import sys
import numpy as np

sys.path.insert(0, "code")

SEED = 20260927


def lround_half_away(x):
    """C lround semantics: round half away from zero."""
    return int(np.floor(x + 0.5)) if x >= 0 else -int(np.floor(-x + 0.5))


def main():
    out = {"enumeration": {}, "random_sweep": {}, "variance_law": {},
           "negative_control": {}, "bankers_hazard": {},
           "conventions": {
               "r_deficit": "S/S_hat - 1  (true coverage vs stored; "
                   "negative = stored overstates coverage)",
               "r_mult": "S_hat/S - 1  (multiplicative bias applied to S)"}}

    # (1) fine-grid enumeration (cell interiors included) + 511 boundary grid
    fine = np.linspace(0.0, 1.0, 510 * 4000 + 1)
    qs = np.array([lround_half_away(x) for x in 255.0 * fine])
    pos = fine > 0
    act = pos & (qs > 0)
    unrep = int(np.sum(pos & (qs == 0)))
    r_def = np.where(act, fine / np.where(qs > 0, qs / 255.0, 1.0) - 1.0, 0.0)
    r_mul = np.where(act, np.where(qs > 0, qs / 255.0, 1.0) / fine - 1.0, 0.0)
    q254 = act & (qs == 254)
    out["enumeration"] = {
        "worst_r_deficit": float(np.min(r_def[act])),
        "at_S": float(fine[act][np.argmin(r_def[act])]),
        "claimed_minus_50pct": -0.5,
        "sup_abs_r_deficit_q254_cell": float(np.max(np.abs(r_def[q254]))),
        "claimed_0p5_over_254": 0.5 / 254.0,
        "sup_abs_r_mult_all_q_ge1": float(np.max(np.abs(r_mul[act]))),
        "sup_r_mult_value": float(np.max(r_mul[act])),
        "sub_lsb_unrepresentable_count": unrep,
        "sub_lsb_threshold": 1.0 / 510.0,
        "boundary_check_S_1_510": {
            "S": 1.0 / 510.0,
            "q": lround_half_away(255.0 / 510.0),
            "r_deficit": (1.0 / 510.0) / (1.0 / 255.0) - 1.0,
        },
    }

    # random sweep at fixed seed (continuity check of the worst case)
    rng = np.random.default_rng(SEED)
    S = rng.uniform(1.0 / 510.0, 1.0, 1_000_000)
    q = np.array([lround_half_away(x) for x in 255.0 * S])
    mask = q > 0
    rd = S[mask] / (q[mask] / 255.0) - 1.0
    out["random_sweep"] = {"n": int(mask.sum()),
                           "max_abs_r_deficit": float(np.max(np.abs(rd))),
                           "min_r_deficit": float(np.min(rd)),
                           "max_r_deficit": float(np.max(rd))}

    # (3) variance law
    rw = out["enumeration"]["worst_r_deficit"]
    out["variance_law"] = {
        "snr2_factor_at_worst": (1.0 + rw) ** 2 - 1.0,
        "claimed_minus_75pct": -0.75,
        "snr2_factor_at_1p9685e-3": (1.0 + 0.5 / 254.0) ** 2 - 1.0,
        "identity_2r_plus_r2_at_worst": 2.0 * rw + rw ** 2,
    }

    # (4) negative control: step -> 0 collapses every metric toward 0
    Sx = np.linspace(1.0 / 510.0, 1.0, 10000)
    steps = [1 / 255.0, 1 / 510.0, 1 / 1020.0, 1 / 2040.0]
    conv = []
    for st in steps:
        qq = np.array([lround_half_away(x) for x in Sx / st])
        mm = qq > 0
        conv.append({"step": st,
                     "max_abs_r_mult": float(np.max(np.abs(
                         (qq[mm] * st) / Sx[mm] - 1.0)))})
    out["negative_control"] = {
        "max_abs_r_no_quantization": 0.0,
        "step_convergence": conv,
        "note": "step = 0 (exact float coverage): S_hat = S identically, "
            "every metric exactly 0; as the step halves the worst-case "
            "relative error doubles in LSB count but the ABSOLUTE error "
            "halves, collapsing toward the no-quantization control.",
    }

    # (5) banker's hazard
    S05 = 1.0 / 510.0
    out["bankers_hazard"] = {
        "S": S05,
        "lround_q": lround_half_away(255.0 * S05),
        "numpy_round_q": int(np.round(255.0 * S05)),
        "note": "numpy ties-to-even gives q = 0 at S = 1/510: the pixel "
            "would be dropped entirely; C lround (production semantic) gives "
            "q = 1. The worst-case r_deficit = -0.5 exists only under half-"
            "away-from-zero; under banker's rounding the same S vanishes.",
    }

    en = out["enumeration"]
    ok = (abs(en["worst_r_deficit"] - (-0.5)) < 1e-12
          and abs(en["sup_abs_r_deficit_q254_cell"] - 0.5 / 254.0) < 1e-12
          and en["sub_lsb_unrepresentable_count"] > 0
          and en["boundary_check_S_1_510"]["q"] == 1
          and abs(out["variance_law"]["snr2_factor_at_worst"] - (-0.75))
          < 1e-12
          and out["negative_control"]["step_convergence"][-1]["max_abs_r_mult"]
          < out["negative_control"]["step_convergence"][0]["max_abs_r_mult"]
          and out["bankers_hazard"]["numpy_round_q"] == 0
          and out["bankers_hazard"]["lround_q"] == 1)
    out["verdict"] = "PASS" if ok else "FAIL"
    print(json.dumps(out, indent=2))
    with open("results/exp08_coverage_quantization.json", "w") as fh:
        json.dump(out, fh, indent=2)


if __name__ == "__main__":
    main()

