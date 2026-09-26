#!/usr/bin/env python3
"""EXP-P2-R1-08: profile half-window sensitivity of SNR_F (P-CST-23, P-ALG-03 window rule).

Window rule (snr_science.cpp:99-103, verified):  h = clip(ceil(12*FWHM), 30, 256).
Claim to test: at profile FWHM 60/120/260 px the SNR bias of the rule window
against a huge window is +4e-6 / +2.4e-4 / +1.33e-2 (positive = SNR overstated,
the conservative side is the LARGE window).
Mechanism: sigma_F = sigma_sky / sqrt(sum P_i^2) with P normalized IN window;
narrower window -> larger sum P^2 -> smaller sigma_F -> larger SNR.
Negative control: window = 8*FWHM vs 16*FWHM => metric 0.
Seed fixed 20260926 (deterministic; no MC needed). Pure python3+numpy.
"""
import json, os
import numpy as np

SEED = 20260926
K4 = 1.2303076525901024          # Moffat4 FWHM/sigma (verified in exp02)
out = {"seed": SEED, "profile": "Moffat4 (1+r^2/2s^2)^-4, sigma = FWHM/K4"}

def sum_p2(fwhm, half):
    s = fwhm / K4
    h = int(np.ceil(half))
    r = np.arange(-h, h + 1)
    X, Y = np.meshgrid(r, r)
    P = (1.0 + (X ** 2 + Y ** 2) / (2 * s ** 2)) ** -4.0
    P /= P.sum()
    return float((P ** 2).sum())

rows = []
for fwhm in (30.0, 60.0, 120.0, 260.0):
    h_rule = min(256.0, max(30.0, float(np.ceil(12.0 * fwhm))))
    p2_rule = sum_p2(fwhm, h_rule)
    p2_big = sum_p2(fwhm, 8 * fwhm)
    # SNR(h)/SNR(big) - 1 = sqrt(p2_rule/p2_big) - 1
    snr_bias = float(np.sqrt(p2_rule / p2_big) - 1.0)
    rows.append({"fwhm_px": fwhm, "h_rule": h_rule, "snr_bias": snr_bias})
out["rows"] = rows

# variant: the quoted number is the Moffat4 PARAMETER s (not FWHM = 1.2303 s)
rows_v = []
for s_q in (30.0, 60.0, 120.0, 260.0):
    h_rule = min(256.0, max(30.0, float(np.ceil(12.0 * s_q))))
    def sum_p2_s(s_px, half):
        h = int(np.ceil(half))
        rr = np.arange(-h, h + 1)
        XX, YY = np.meshgrid(rr, rr)
        PP = (1.0 + (XX ** 2 + YY ** 2) / (2 * s_px ** 2)) ** -4.0
        return float(((PP / PP.sum()) ** 2).sum())
    p2_rule = sum_p2_s(s_q, h_rule)
    p2_big = sum_p2_s(s_q, 8 * s_q)
    rows_v.append({"s_px": s_q, "h_rule": h_rule,
                   "snr_bias": float(np.sqrt(p2_rule / p2_big) - 1.0)})
out["rows_variant_sigma_equals_quote"] = rows_v
out["repo_claims"] = [{"fwhm": 60, "bias": 4e-6}, {"fwhm": 120, "bias": 2.4e-4},
                      {"fwhm": 260, "bias": 1.33e-2}]
out["abs_diffs"] = [abs(rows[i]["snr_bias"] - c["bias"])
                    for i, c in zip((1, 2, 3), out["repo_claims"])]

# negative control: 8*FWHM vs 16*FWHM
d = []
for fwhm in (60.0, 260.0):
    b1 = np.sqrt(sum_p2(fwhm, 8 * fwhm) / sum_p2(fwhm, 16 * fwhm)) - 1.0
    d.append(abs(float(b1)))
out["null_bigwindow_metric"] = {"max_abs": float(max(d))}

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp08_profile_window_sensitivity.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
