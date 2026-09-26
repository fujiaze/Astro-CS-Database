#!/usr/bin/env python3
# exp08_scale_constant.py -- R3-11: HEALPIX_SCALE_PER_NSIDE_ARCSEC.
# C = sqrt(pi/3)*(180/pi)*3600 = 211076.28514206142 arcsec (exact identity with
#   sqrt(4*pi/(12*N^2)) converted per N: verify bit-level equality of the two formulas).
# The propagated wrong constant 211034.6 differs by -1.97e-4 relative; demonstrate the
#   nside decision window where the wrong constant flips the chosen nside by a factor 2
#   (rule: smallest power-of-two nside with hp_res(nside) <= finest).
import json
import numpy as np

def main():
    C = np.sqrt(np.pi/3.0)*(180.0/np.pi)*3600.0
    out = dict(exp="exp08_scale_constant")
    out["C_repr"] = repr(C)
    # per-N identity of the two formulas (arcsec)
    rows = []
    max_ulp = 0
    for N in (1, 2, 4, 16, 64, 256, 512, 1024):
        v1 = C/N
        v2 = np.sqrt(4.0*np.pi/(12.0*N*N))*(180.0/np.pi)*3600.0
        ulp = abs(np.float64(v1).view(np.int64) - np.float64(v2).view(np.int64))
        max_ulp = max(max_ulp, int(ulp))
        rows.append(dict(N=N, v1=float(v1), v2=float(v2), ulp_diff=int(ulp)))
    out["per_N_identity"] = rows
    wrong = 211034.6
    rel = (wrong - C)/C
    out["wrong_constant"] = dict(value=wrong, rel_diff=rel)
    # decision window: smallest power-of-two nside with hp_res <= finest
    def choose(C, finest):
        n = 1
        while C/n > finest:
            n *= 2
        return n
    demo = dict()
    finest = 105527.7
    demo["finest"] = finest
    demo["ratio_true"] = C/finest
    demo["ratio_wrong"] = wrong/finest
    demo["nside_true"] = choose(C, finest)
    demo["nside_wrong"] = choose(wrong, finest)
    # window where they disagree (k=1 boundary: nside 2 vs 4)
    lo, hi = wrong/2.0, C/2.0
    demo["flip_window_arcsec"] = [lo, hi]
    demo["flip_window_width_arcsec"] = hi - lo
    out["decision_demo"] = demo
    print("C =", repr(C))
    print("wrong rel diff =", rel)
    print("demo:", json.dumps(demo))
    out["verdict"] = dict(
        identity_bitwise=bool(max_ulp == 0),
        rel_diff_about_minus_2e4=bool(abs(rel + 1.97e-4) < 1e-6),
        decision_flip=bool(demo["nside_true"] == 4 and demo["nside_wrong"] == 2))
    print("VERDICT:", json.dumps(out["verdict"]))
    with open("results/exp08_scale_constant.json", "w") as f:
        json.dump(out, f, indent=1)

if __name__ == "__main__":
    main()
