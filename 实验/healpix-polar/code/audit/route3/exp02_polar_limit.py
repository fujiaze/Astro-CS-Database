#!/usr/bin/env python3
# exp02_polar_limit.py -- R3-02: closed-form limit of polar-touching pixel model area
#   A_quad/A_true -> 2*sqrt(2)/pi = 0.9003163161  => rel error 2*sqrt(2)/pi - 1 = -9.968368384e-2
#   absolute deficit law: max deficit = A_true * (1 - 2*sqrt2/pi) = 0.104369/N^2
#   worst pixels = the 8 polar-touching (ring 1 & ring 4N-1) + 8 next (ring 3 & 4N-3) => 16 with |rel|>1%
# Negative control: equatorial-band pixels of the same model must sit at ~1e-8 level, i.e. the
# metric is zero where the effect is absent (localization, non-degenerate metric).
import json
import numpy as np
from exp01_leaf_area import pixel_corners, to_vec, vos_quad, ring_layout

LIMIT = 2.0*np.sqrt(2.0)/np.pi - 1.0          # -0.099683683838434
DEFICIT_C = (np.pi/3.0)*(1.0 - 2.0*np.sqrt(2.0)/np.pi)  # coefficient of 1/N^2

def run_N(N):
    corners = pixel_corners(N)          # (M,4,2) ordered N,R,S,L as (phi,z)
    V = to_vec(corners)
    A_quad = vos_quad(V)
    A_true = np.pi/(3.0*N*N)
    rel = A_quad/A_true - 1.0
    lev, z, S, w, o = ring_layout(N)
    js = np.concatenate([np.full(S[j], j) for j in range(1, 4*N) if S[j] > 0])
    polar = (js <= N) | (js >= 3*N)
    belt = ~polar
    worst = rel.min()
    n_over1pct = int((np.abs(rel) > 0.01).sum())
    cap_edge_max = np.abs(rel[polar]).max()
    belt_max = np.abs(rel[belt]).max()
    deficit = A_true*abs(worst)
    # worst pixel location
    iw = int(rel.argmin())
    return dict(N=N, npix=int(len(rel)), worst_rel=float(worst),
                limit=LIMIT, worst_minus_limit=float(worst - LIMIT),
                deficit=float(deficit), deficit_times_N2=float(deficit*N*N),
                deficit_law_pred=float(DEFICIT_C),
                n_abs_rel_gt_1pct=n_over1pct,
                cap_max_abs_rel=float(cap_edge_max), belt_max_abs_rel=float(belt_max),
                sum_res=float(A_quad.sum()/(4*np.pi) - 1.0), worst_ring=int(js[iw]))

def main():
    out = dict(exp="exp02_polar_limit", limit_rel=LIMIT, deficit_coeff=DEFICIT_C, rows=[])
    for N in [2, 4, 8, 16, 32, 64, 128, 256]:
        row = run_N(N)
        out["rows"].append(row)
        print(f"N={N:4d} worst={row['worst_rel']:+.6e} (limit {LIMIT:+.6e}) "
              f"deficit*N^2={row['deficit_times_N2']:.6f} (law {DEFICIT_C:.6f}) "
              f"n>1%={row['n_abs_rel_gt_1pct']:3d} belt_max={row['belt_max_abs_rel']:.2e} "
              f"worst_ring={row['worst_ring']}", flush=True)
    # convergence is O(1/N^2): check successive-doubling ratio ~4 and absolute tail values
    d = [abs(r["worst_minus_limit"]) for r in out["rows"][-4:]]   # N = 32,64,128,256
    ratios = [d[i]/d[i+1] for i in range(len(d)-1)]
    b = [r["belt_max_abs_rel"]*r["N"]**2 for r in out["rows"][-4:]]
    out["verdict"] = dict(
        limit_ratios_halving4=[float(x) for x in ratios],
        limit_abs_at_256=float(d[-1]),
        limit_converged=bool(d[-1] < 5e-6 and all(3.0 < x < 5.5 for x in ratios)),
        deficit_relerr=[float(r["deficit_times_N2"]/DEFICIT_C - 1) for r in out["rows"][-4:]],
        deficit_law=bool(abs(out["rows"][-1]["deficit_times_N2"]/DEFICIT_C - 1) < 5e-5),
        count16=bool(all(r["n_abs_rel_gt_1pct"] == 16 for r in out["rows"][-4:])),
        belt_o1_over_N2=[float(x) for x in b])
    print("VERDICT:", json.dumps(out["verdict"]))
    with open("results/exp02_polar_limit.json", "w") as f:
        json.dump(out, f, indent=1)

if __name__ == "__main__":
    main()
