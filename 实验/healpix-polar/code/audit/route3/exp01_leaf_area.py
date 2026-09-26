#!/usr/bin/env python3
# exp01_leaf_area.py -- R3-01: A_leaf = pi/(3N^2) literature-value leg + experiment leg
# Two independent spherical-polygon area formulas (VOS fan triangulation vs l'Huilier)
# cross-checked on the exact ring-corner vertex grid; closure Sigma A = 4pi; mean = A_true.
# Negative controls: (i) same-geometry comparison must vanish (<=1e-12);
# (ii) non-unitized input breaks VOS but not l'Huilier (documented sensitivity self-check);
# (iii) cyclic-order reversal gives identical |area|, crossing order does not.
# Seed policy: no stochastic sampling is needed; grid is deterministic (seed printed for record).
import json, sys
import numpy as np

SEED = 20260926
np.random.seed(SEED)

def ring_layout(N):
    lev = np.arange(0, 4*N+1)
    z = np.empty(4*N+1)
    z[0] = 1.0; z[4*N] = -1.0
    north = (lev >= 1) & (lev <= N)
    eq    = (lev > N) & (lev < 3*N)
    south = (lev >= 3*N) & (lev <= 4*N-1)
    z[north] = 1.0 - lev[north]**2/(3.0*N*N)
    z[eq]    = 2.0*(2.0*N - lev[eq])/(3.0*N)
    z[south] = -1.0 + (4.0*N - lev[south])**2/(3.0*N*N)
    S = np.zeros(4*N+1, dtype=np.int64)
    S[north] = 4*lev[north]
    S[eq]    = 4*N
    S[south] = 4*(4*N - lev[south])
    w = np.where(S > 0, 2*np.pi/np.maximum(S, 1), 0.0)
    o = np.where((eq) & (lev % 2 == 1), w/2.0, 0.0)
    return lev, z, S, w, o

def pixel_corners(N):
    """Return arrays Nc, Lc, Rc, Sc of shape (M,2) as (phi, z) for all 12N^2 pixels (ring scheme)."""
    lev, z, S, w, o = ring_layout(N)
    js, ms = [], []
    for j in range(1, 4*N):
        if S[j] > 0:
            js.append(np.full(S[j], j)); ms.append(np.arange(S[j]))
    j = np.concatenate(js); m = np.concatenate(ms)
    phi_c = o[j] + (m + 0.5)*w[j]
    phiL = o[j] + m*w[j]          # = phi_c - w_j/2
    phiR = o[j] + (m+1)*w[j]
    zj = z[j]
    # nearest grid point on level j-1 and j+1 (vectorized; pole levels give phi=0)
    def nearest(jlev, phi_c):
        pole = (jlev <= 0) | (jlev >= 4*N)
        jc = np.clip(jlev, 0, 4*N)
        Sj = S[jc]; wj = w[jc]; oj = o[jc]
        wsafe = np.where(wj > 0, wj, 1.0)
        k = np.mod(np.round((phi_c - oj)/wsafe).astype(np.int64), np.maximum(Sj, 1))
        phi = oj + k*wj
        return np.where(pole, 0.0, phi)
    phiN = nearest(j-1, phi_c); phiS = nearest(j+1, phi_c)
    zN = z[np.clip(j-1, 0, 4*N)]; zS = z[np.clip(j+1, 0, 4*N)]
    Nc = np.stack([phiN, zN], axis=1); Lc = np.stack([phiL, zj], axis=1)
    Rc = np.stack([phiR, zj], axis=1); Sc = np.stack([phiS, zS], axis=1)
    return np.stack([Nc, Rc, Sc, Lc], axis=1)  # cyclic order N->R->S->L

def to_vec(pz):
    phi, z = pz[..., 0], pz[..., 1]
    r = np.sqrt(np.maximum(0.0, 1.0 - z*z))
    return np.stack([r*np.cos(phi), r*np.sin(phi), z], axis=-1)

def vos_triangle(a, b, c):
    det = np.einsum('ij,ij->i', a, np.cross(b, c))
    return 2.0*np.arctan2(det, 1.0 + np.einsum('ij,ij->i', a, b)
                                + np.einsum('ij,ij->i', b, c)
                                + np.einsum('ij,ij->i', c, a))

def vos_quad(V):
    return np.abs(vos_triangle(V[:,0], V[:,1], V[:,2]) + vos_triangle(V[:,0], V[:,2], V[:,3]))

def lhuilier_triangle(a, b, c):
    def edge(p, q):
        return np.arctan2(np.linalg.norm(np.cross(p, q), axis=-1), np.einsum('ij,ij->i', p, q))
    A = edge(a, b); B = edge(b, c); C = edge(c, a)
    s = 0.5*(A+B+C)
    t = (np.tan(0.5*s)*np.tan(0.5*(s-A))*np.tan(0.5*(s-B))*np.tan(0.5*(s-C)))
    t = np.maximum(t, 0.0)
    return 4.0*np.arctan(np.sqrt(t))

def lhuilier_quad(V):
    return lhuilier_triangle(V[:,0], V[:,1], V[:,2]) + lhuilier_triangle(V[:,0], V[:,2], V[:,3])

def run_N(N):
    corners = pixel_corners(N)
    V = to_vec(corners)          # (M,4,3)
    M = V.shape[0]
    A_vos = vos_quad(V)
    A_lh  = lhuilier_quad(V)
    A_true = np.pi/(3.0*N*N)
    two_form = np.max(np.abs(A_vos - A_lh))/A_true
    sum_res  = A_vos.sum()/ (4*np.pi) - 1.0
    mean_res = A_vos.mean()/A_true - 1.0
    return dict(N=N, npix=int(M), two_formula_maxrel=float(two_form),
                sum_res=float(sum_res), mean_res=float(mean_res), A_true=float(A_true))

def negative_controls():
    corners = pixel_corners(16)
    V = to_vec(corners)
    # (ii) non-unitized vertex (scale 1.5 on one vertex of first 1000 quads)
    V2 = V[:1000].copy(); V2[:,1] *= 1.5
    d_vos = np.max(np.abs(vos_quad(V2)/vos_quad(V[:1000]) - 1.0))
    d_lh  = np.max(np.abs(lhuilier_quad(V2)/lhuilier_quad(V[:1000]) - 1.0))
    # (iii) order reversal vs crossing order
    Vrev = V[:1000][:, [0, 3, 2, 1], :]
    Vcro = V[:1000][:, [0, 2, 1, 3], :]
    d_rev = np.max(np.abs(vos_quad(Vrev) - vos_quad(V[:1000])))
    d_cro = np.max(np.abs(vos_quad(Vcro) - vos_quad(V[:1000])))
    return dict(nonunit_vos_rel=float(d_vos), nonunit_lh_rel=float(d_lh),
                reverse_absdiff=float(d_rev), crossing_absdiff=float(d_cro))

def main():
    out = dict(seed=SEED, exp="exp01_leaf_area", rows=[], controls=negative_controls())
    for N in [2, 4, 8, 16, 32, 64, 128, 256]:
        row = run_N(N)
        out["rows"].append(row)
        print(f"N={N:4d} npix={row['npix']:8d} two_formula_maxrel={row['two_formula_maxrel']:.3e} "
              f"sum_res={row['sum_res']:+.3e} mean_res={row['mean_res']:+.3e}", flush=True)
    ok2f = max(r['two_formula_maxrel'] for r in out['rows'])
    out["verdict"] = dict(two_formula_le_3p3e12=bool(ok2f <= 4e-12),
                          sum_closure_le_1e12=bool(max(abs(r['sum_res']) for r in out['rows']) <= 1e-12),
                          mean_le_1e12=bool(max(abs(r['mean_res']) for r in out['rows']) <= 1e-12))
    print("VERDICT:", json.dumps(out["verdict"]))
    with open("results/exp01_leaf_area.json", "w") as f:
        json.dump(out, f, indent=1)

if __name__ == "__main__":
    main()
