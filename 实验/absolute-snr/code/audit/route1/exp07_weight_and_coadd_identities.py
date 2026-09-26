#!/usr/bin/env python3
"""EXP-P2-R1-07: float64 identities behind the unique weight conversion (P-ALG-10, P-CST-13).

Claims:
  w = SNR_k^2 / F_ref^2 = 1/sigma_F_k^2  (the ONLY sanctioned snr->ivar conversion)
  SNR_combined^2 = sum_k SNR_k^2         (optimal coadd of identical-flux frames)
Registered gate value: deviation ~ 2.22e-16 (1 ulp); V-4 archived measurement 3.01e-16.
Negative control: dropping the F_ref^2 normalization (w' = SNR^2/F_ref) must show
an O(1) deviation => the identity gate is non-vacuous.
Seed fixed 20260926. Pure python3+numpy.
"""
import json, os
import numpy as np

SEED = 20260926
rng = np.random.default_rng(SEED)
out = {"seed": SEED, "machine_eps": float(np.finfo(float).eps)}

n = 200000
F_ref = 10.0 ** rng.uniform(1.0, 5.0, size=n)
sigma_F = 10.0 ** rng.uniform(-1.0, 2.0, size=n)
snr = F_ref / sigma_F                      # 1 rounding
w_ident = snr ** 2 / F_ref ** 2            # 3 more roundings
w_direct = 1.0 / sigma_F ** 2
dev = np.abs(w_ident - w_direct) / w_direct
out["identity_w_snr2_over_fref2"] = {
    "max_rel_dev": float(dev.max()),
    "p99_rel_dev": float(np.quantile(dev, 0.99)),
    "ulp_bound_2p22e_16": 2.220446049250313e-16,
    "archived_v4_max_dev": 3.01e-16,
    "within_4_ulp": bool(dev.max() <= 4 * 2.220446049250313e-16),
}
# negative control
w_bad = snr ** 2 / F_ref
dev_bad = np.abs(w_bad - w_direct) / w_direct
out["negative_control_missing_fref2"] = {"max_rel_dev": float(dev_bad.max()),
                                         "o1_detected": bool(dev_bad.max() > 0.1)}

# SNR_combined^2 = sum SNR_k^2
K = 8
nk = 20000
snr_k = 10.0 ** rng.uniform(-1.0, 1.0, size=(nk, K))
# unit common flux: sigma_k = 1/SNR_k ; Var(F_comb) = 1/sum(1/sigma_k^2) = 1/sum(SNR_k^2)
var_comb = 1.0 / (snr_k ** 2).sum(axis=1)
snr_comb = np.sqrt(1.0 / var_comb)
resid = np.abs(snr_comb ** 2 - (snr_k ** 2).sum(axis=1)) / (snr_k ** 2).sum(axis=1)
out["identity_snr_comb_squared"] = {
    "K": K, "max_rel_resid": float(resid.max()),
    "p99_rel_resid": float(np.quantile(resid, 0.99)),
    "within_4_ulp": bool(resid.max() <= 4 * 2.220446049250313e-16),
}

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "exp07_weight_and_coadd_identities.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
print(json.dumps(out, indent=1))
