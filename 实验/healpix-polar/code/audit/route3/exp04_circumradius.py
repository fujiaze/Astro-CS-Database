#!/usr/bin/env python3
# exp04_circumradius.py -- R3-05: HP_CIRCUMRADIUS_FACTOR = 1.25 experiment leg
# Enumerate all 12N^2 leaves (N=4..64), sample each leaf's 4 true-curve boundary edges densely,
# measure max angular distance from the leaf center (phi_c, z_j) to the boundary, in hp_res units.
# Negative control: an equal-area lat-lon grid under the same metric (must be clearly worse and
# latitude-dependent, showing the metric discriminates and HEALPix uniformity is the special claim).
import json, sys, os
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from edge_geom import sample_edge, to_vec, wrap_pi
from exp01_leaf_area import pixel_corners, ring_layout

def leaf_boundary_samples(N, j, m, lev, z, S, w, o, n=65):
    phi_c = o[j] + (m + 0.5)*w[j]
    phiL = o[j] + m*w[j]; phiR = o[j] + (m+1)*w[j]; zj = z[j]
    def nearest(jlev, phic):
        pole = (jlev <= 0) | (jlev >= 4*N)
        jc = np.clip(jlev, 0, 4*N)
        Sj, wj, oj = S[jc], w[jc], o[jc]
        wsafe = np.where(wj > 0, wj, 1.0)
        k = np.mod(np.round((phic - oj)/wsafe).astype(np.int64), np.maximum(Sj, 1))
        return np.where(pole, 0.0, oj + k*wj)
    phiN = nearest(j-1, phi_c); phiS = nearest(j+1, phi_c)
    zN = z[np.clip(j-1, 0, 4*N)]; zS = z[np.clip(j+1, 0, 4*N)]
    corners = [(phiN, zN), (phiL, zj), (phiS, zS), (phiR, zj)]  # N,L,S,R
    pts = []
    for i in range(4):
        a, b = corners[i], corners[(i+1) % 4]
        V = sample_edge(np.array(a), np.array(b), n=n)
        pts.append(V)
    return np.concatenate(pts, axis=0), (phi_c, zj)

def scan_N(N, n=65):
    lev, z, S, w, o = ring_layout(N)
    hp_res = np.sqrt(np.pi/3.0)/N
    worst = (0.0, None)
    ratios = []
    for j in range(1, 4*N):
        if S[j] == 0: continue
        for m in range(S[j]):
            pts, c = leaf_boundary_samples(N, j, m, lev, z, S, w, o, n=n)
            cv = to_vec(np.array([c[0]]), np.array([c[1]]))[0]
            d = np.arccos(np.clip(pts @ cv, -1, 1)).max()/hp_res
            ratios.append(d)
            if d > worst[0]: worst = (d, (j, m, c[1]))
    return max(ratios), min(ratios), worst

def latlon_negative(delta_deg=2.0, n=65):
    dl = np.radians(delta_deg)
    worst = 0.0; worst_dec = None
    for dec_deg in np.arange(0.0, 89.0, 0.25):
        dec = np.radians(dec_deg)
        area = (2*dl*np.cos(dec))*(2*dl)
        res = np.sqrt(area)
        cvec = to_vec(np.array([0.0]), np.array([np.sin(dec)]))[0]
        # rectangle boundary: meridians phi=+-dl/2, parallels dec+-dl/2 (in declination)
        pts = []
        phi_edge = np.linspace(-dl/2, dl/2, n)
        for d_ in (dec - dl/2, dec + dl/2):
            pts.append(to_vec(phi_edge, np.full(n, np.sin(d_))))
        d_edge = np.linspace(dec - dl/2, dec + dl/2, n)
        for p_ in (-dl/2, dl/2):
            pts.append(to_vec(np.full(n, p_), np.sin(d_edge)))
        P = np.concatenate(pts, axis=0)
        dmax = np.arccos(np.clip(P @ cvec, -1, 1)).max()/res
        if dmax > worst: worst, worst_dec = dmax, dec_deg
    return worst, worst_dec

def main():
    out = dict(exp="exp04_circumradius", rows=[], negctl={})
    for N in [4, 8, 16, 32, 64]:
        mx, mn, worst = scan_N(N)
        out["rows"].append(dict(N=N, max_ratio=float(mx), min_ratio=float(mn),
                                worst_pixel=worst[1], worst_center_z=float(worst[1][2])))
        print(f"N={N:3d} max_ratio={mx:.6f} min_ratio={mn:.6f} worst_z={worst[1][2]:+.4f}", flush=True)
    w, wd = latlon_negative()
    out["negctl"] = dict(grid="lat-lon 2deg", max_ratio=float(w), worst_dec_deg=float(wd))
    print("NEGCTL lat-lon:", out["negctl"])
    r = [r["max_ratio"] for r in out["rows"]]
    out["verdict"] = dict(
        monotone_rising=bool(all(r[i] < r[i+1] for i in range(len(r)-1))),
        max_le_125=bool(max(r) <= 1.25),
        headroom=float(1.25 - max(r)),
        negctl_worse=bool(w > max(r)))
    print("VERDICT:", json.dumps(out["verdict"]))
    with open("results/exp04_circumradius.json", "w") as f:
        json.dump(out, f, indent=1)

if __name__ == "__main__":
    main()
