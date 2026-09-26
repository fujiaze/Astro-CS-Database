#!/usr/bin/env python3
# exp07_quantization.py -- R3-10: 1/255 storage quantization ledger.
# Convention (fixed after the direction audit): the ledger r is r_sup = S/(q/255) - 1, i.e.
#   true value over quantized support; q = floor(255*S + 0.5) (half-away rounding for S>=0).
# Checks: exact bound |r| <= 0.5/q; worst -0.5 at S=1/510; ledger values -1.9231e-2 @0.05,
#   +1.2270e-3 @0.64; r == 0 bit-exact at S=1 (negative control: no effect when representable);
#   S < 1/510 uncovered (q = 0 -> division undefined, must be special-cased); clamp S=1.3;
#   variance bias of quantized signal ~ (1+r_sig)^2 - 1 ~ 2*r_sig.
import json
import numpy as np

SEED = 20260926

def q255(S):
    return np.floor(255.0*np.asarray(S, dtype=float) + 0.5)

def r_sup(S):
    S = np.asarray(S, dtype=float)
    q = q255(S)
    out = np.full(S.shape, np.nan)
    m = q > 0
    out[m] = S[m]/(q[m]/255.0) - 1.0
    return out

def main():
    out = dict(exp="exp07_quantization", seed=SEED, convention="r_sup = S/(q/255) - 1, q = floor(255S+0.5)")
    S = np.arange(0, 511)/510.0
    S[0] = 1e-9  # stand-in for the smallest positive datum (q=0 case kept via mask)
    r = r_sup(S)
    q = q255(S)
    m = q > 0
    viol = np.abs(r[m]) - 0.5/q[m]
    bound_ok = bool(np.all(viol <= 1e-15))
    bound_max_viol = float(viol.max())
    iw = int(np.nanargmax(np.abs(r)))
    worst = dict(S=float(S[iw]), r=float(r[iw]), q=int(q[iw]))
    pts = {}
    for s0 in (0.05, 0.64, 1.0, 1.3, 1/510.0):
        rr = r_sup(np.array([s0]))[0]
        pts[repr(s0)] = None if np.isnan(rr) else float(rr)
    # fine sweep for the bound
    Sf = np.linspace(1/510.0, 2.0, 2000001)
    rf = r_sup(Sf); qf = q255(Sf)
    violf = np.abs(rf) - 0.5/qf
    bound_fine = bool(np.all(violf <= 1e-15))
    bound_max_viol = max(bound_max_viol, float(violf.max()))
    # variance bias experiment: multiplicative fluctuation around S0, then quantize
    rng = np.random.default_rng(SEED)
    var_rows = []
    for S0, sig in ((0.05, 0.25), (0.64, 0.25), (0.3, 0.3)):
        n = 400000
        Ss = S0*(1.0 + sig*rng.standard_normal(n))
        Sq = q255(Ss)/255.0
        vr = float(Sq.var()/Ss.var())
        r_sig = float(r_sup(np.array([S0]))[0])
        pred = (1.0 + r_sig)**2 - 1.0
        var_rows.append(dict(S0=S0, sigma=sig, var_ratio_minus_1=vr - 1.0,
                             pred_2rsig_plus=pred, r_sig=r_sig))
        print("S0=%.3f var_ratio-1=%+.6f pred(1+rsig)^2-1=%+.6f" % (S0, vr-1.0, pred), flush=True)
    out["bound_511"] = bound_ok
    out["bound_fine_2e6"] = bound_fine
    out["bound_max_violation"] = bound_max_viol
    out["worst"] = worst
    out["ledger_points"] = pts
    out["variance"] = var_rows
    out["clamp_case"] = dict(S=1.3, q=int(q255(np.array([1.3]))[0]),
                             r_sup_unclamped=float(r_sup(np.array([1.3]))[0]),
                             r_if_stored_clamped_to_1=0.3)
    out["underflow_case"] = dict(note="S < 1/510 gives q=0: r_sup undefined (division by zero);",
                                 q_at_S_half_510=int(q255(np.array([1/1020.0]))[0]))
    out["verdict"] = dict(
        bound_exact=bound_ok and bound_fine,
        bound_max_violation=bound_max_viol,
        worst_is_minus_half_at_1_510=bool(worst["S"] == 1/510.0 and worst["r"] == -0.5),
        ledger_005=bool(abs(pts[repr(0.05)] + 1.9231e-2) < 1e-6),
        ledger_064=bool(abs(pts[repr(0.64)] - 1.2270e-3) < 1e-6),
        one_exact=bool(pts[repr(1.0)] == 0.0),
        var_bias_tracks_2rsig=bool(all(abs(v["var_ratio_minus_1"] - v["pred_2rsig_plus"]) < 0.05 for v in var_rows)))
    print("worst:", worst, "points:", pts)
    print("VERDICT:", json.dumps(out["verdict"]))
    with open("results/exp07_quantization.json", "w") as f:
        json.dump(out, f, indent=1)

if __name__ == "__main__":
    main()
