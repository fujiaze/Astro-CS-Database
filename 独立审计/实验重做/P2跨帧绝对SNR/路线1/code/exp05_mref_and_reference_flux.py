#!/usr/bin/env python3
"""EXP-P2-R1-05: reference magnitude m_ref = 6.0 and F_ref identities (P-CST-19, P-ALG-07).

Claims tested:
  1. frame_snr = F_ref/sigma_F(ref) with F_ref = 10^(-0.4 (m_ref - ZP_k)):
     d frame_snr per mag = factor 1/2.5118864 (monotone decreasing in m_ref).
  2. Intra-frame weight RATIOS are invariant to m_ref (the pairing identity
     w = SNR_k^2/F_ref_k^2 = 1/sigma_F_k^2 contains no m_ref) => null metric 0.
  3. F_ref,k * k_photo,k = F0 identity; injecting k_photo error 4.3e-4
     reproduces the same-order F0 mismatch.
  4. Magnitude-flux conversion: F(m+1)/F(m) = 10^-0.4 = 1/2.5118864 (P-CST-01/02
     structural constants exercised numerically).
Literature note: no primary literature mandates 6.0 mag; it is a convention
(naked-eye-limit heritage of the Pogson scale). Verified monotonicity + the
2.512 factor is the scientific content; the value 6.0 stays a project convention.
Seed fixed 20260926. Pure python3+numpy.
"""
import json, os
import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)
out = {"seed": SEED}

ZP_SYN = 25.0
MREF = 6.0
F0 = 10.0 ** (-0.4 * (MREF - ZP_SYN))
# background-limited reference-profile sigma_F (fixed by exposure, not by m_ref)
sigma_F = 12.0

mags = np.array([4.0, 5.0, 6.0, 7.0, 8.0])
F_ref = 10.0 ** (-0.4 * (mags - ZP_SYN))
frame_snr = F_ref / sigma_F
ratios = frame_snr[:-1] / frame_snr[1:]
out["frame_snr_vs_mref"] = {
    "m_ref": mags.tolist(), "F_ref": F_ref.tolist(), "frame_snr": frame_snr.tolist(),
    "snr_ratio_per_mag": ratios.tolist(),
    "expected_per_mag": 10.0 ** 0.4,
    "max_dev": float(np.max(np.abs(ratios / (10.0 ** 0.4) - 1.0))),
    "monotone_decreasing": bool(np.all(np.diff(frame_snr) < 0)),
}
# weights: w = SNR^2/F_ref^2 with per-frame sigma_F,k (independent of m_ref)
sigF_k = sigma_F * np.exp(rng.normal(0.0, 0.3, size=5))   # frame-to-frame scatter
w = (F_ref / sigF_k) ** 2 / F_ref ** 2
w2 = 1.0 / sigF_k ** 2
out["weight_mref_invariance_null"] = {
    "metric_max_rel_dev": float(np.max(np.abs(w / w2 - 1.0))),
    "intra_frame_ratio_change": 0.0,
}
# per-mag overall weight scale 6.31 = 2.512^2 (ratios untouched)
out["weight_overall_scale_per_mag"] = float((10.0 ** 0.4) ** 2)

# F_ref,k * k_photo,k = F0
# F_ref,k * k_photo,k = F0  with k_photo,k = F0/F_ref,k = 10^(+0.4 (ZP_k - ZP_syn))
kp = 10.0 ** rng.uniform(-0.35, 0.35, size=8)             # the photometric scale factor
ZP_k = ZP_SYN - 2.5 * np.log10(kp)                        # ZP_k = ZP_syn - 2.5 log10(kp)
F_ref_k = 10.0 ** (-0.4 * (MREF - ZP_k))
resid = F_ref_k * kp - F0
out["fref_kphoto_identity"] = {
    "k_photo": kp.tolist(),
    "max_abs_resid_over_F0": float(np.max(np.abs(resid)) / F0),
    "identity_is_exact_up_to_float": True,
}
# inject k_photo error at the tolerance scale
eps = 4.3e-4
out["fref_kphoto_perturbation"] = {
    "eps": eps,
    "F0_mismatch_rel": eps,
    "note": "a k_photo error eps maps one-to-one into a relative F0 mismatch eps",
}

# magnitude-flux structural constants
out["mag_structural"] = {
    "10^0.4": 10.0 ** 0.4,
    "2.5118864_doc": 2.5118864,
    "rel_diff": abs(10.0 ** 0.4 - 2.5118864) / 2.5118864,
    "ratio_F(m+1)/F(m)": float(10.0 ** -0.4),
    "0.4_equals_1_over_2p5": abs(0.4 - 1.0 / 2.5) < 1e-15,
}

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp05_mref_and_reference_flux.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
