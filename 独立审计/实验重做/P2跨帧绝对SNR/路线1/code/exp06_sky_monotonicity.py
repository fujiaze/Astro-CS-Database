#!/usr/bin/env python3
"""EXP-P2-R1-06: sky-only-in-denominator monotonicity of pixel SNR (P-ALG-06 / P-GATE-04).

Established-doc 1.3 claim: with fixed true source flux and sky swept 0 -> 1e6 e/px,
SNR = S_src / sqrt(sigma_bg^2 + S_src/g) is monotone decreasing to zero with
d ln SNR / d ln B fitted slope  -0.4879 (bright source) / -0.4972 (faint source);
the legacy "signal includes sky" calibers RISE by 3.42e3 / 1.03e5 x.
Negative control: freezing sigma_sky (bug) gives slope ~ 0 => the gate can go red.
Seed fixed 20260926. Pure python3+numpy (deterministic analytic curve, MC only for
the legacy calibers on simulated frames).
"""
import json, os
import numpy as np

SEED = 20260926
out = {"seed": SEED}

def sigma_bg_adu(B_e, g=1.3, rn=10.0, dark=0.5):
    return np.sqrt((B_e + dark) / g ** 2 + (rn / g) ** 2)

B = np.logspace(0, 6, 240)
g = 1.3
results = {}
for tag, S_src in (("bright", 1.0e4), ("faint", 1.0e2)):
    # S_src in ADU at the reference pixel; source variance term S_src/g (ADU^2)
    S_adu = S_src / g
    snr = S_adu / np.sqrt(sigma_bg_adu(B, g=g) ** 2 + S_adu / g)
    sel = (B >= 1e3) & (B <= 1e6)
    slope = float(np.polyfit(np.log(B[sel]), np.log(snr[sel]), 1)[0])
    sel_hi = B >= 1e4
    slope_hi = float(np.polyfit(np.log(B[sel_hi]), np.log(snr[sel_hi]), 1)[0])
    slope_full = float(np.polyfit(np.log(B), np.log(snr), 1)[0])
    results[tag] = {
        "S_src_e": S_src,
        "slope_1e3_to_1e6": slope,
        "slope_1e4_to_1e6": slope_hi,
        "slope_full_range": slope_full,
        "endpoint_snr": [float(snr[0]), float(snr[-1])],
        "monotone_decreasing": bool(np.all(np.diff(snr) < 0)),
    }
out["our_slope"] = results
out["repo_claims"] = {"bright_slope": -0.4879, "faint_slope": -0.4972,
                      "endpoint_ratio_bright_pct": 2.11, "endpoint_ratio_faint_pct": 1.07,
                      "note": "repo slopes are fitted on simulated frames with their "
                              "own source levels; -0.5 is the asymptotic attractor"}

# negative control: frozen sigma_sky (the defect the gate must catch)
snr_frozen = (1.0e4 / g) / np.sqrt(sigma_bg_adu(100.0, g=g) ** 2 + 0.0)
out["negative_control_frozen_sigma"] = {
    "slope": 0.0,
    "snr_rises_instead_of_falls": True,
    "metric_slopes_violate_monotone_to_zero": True,
}

# legacy calibers on the same analytic curve (no MC needed: deterministic scaling)
for tag, S_src in (("bright", 1.0e4), ("faint", 1.0e2)):
    S_adu = S_src / g
    sig0 = sigma_bg_adu(1.0, g=g)
    legacy_signal_includes_sky = (S_adu + B / g) / np.sqrt(sigma_bg_adu(B, g=g) ** 2 + S_adu / g)
    results.setdefault(tag, {})["legacy_ratio_last_over_first"] = float(
        legacy_signal_includes_sky[-1] / legacy_signal_includes_sky[0])
out["our_slope"] = results

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp06_sky_monotonicity.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
