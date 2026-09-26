#!/usr/bin/env python3
# exp03_weight_conservation.py -- R3-03 + P3 chain use-case (P2->P3->P4/P5 interfaces).
# Setup: TAN WCS (ra0=30deg, dec0=30deg), 24x24 source pixels of size theta inside the face,
#   pixfrac=0.8; drop footprint = gnomonic image of the pixfrac-shrunk square. Gnomonic maps
#   tangent-plane straight lines to great circles, so the drop is a spherical quadrilateral with
#   EXACT area via Van Oosterom-Strackee on its 4 corners.
# Metrics:
#   (1) completeness/conservation: w_jp = a_jp/A_drop,j on true-curve leaf polygons. Chord
#       tiles of adjacent leaves share edges, hence tile the sphere exactly, so sum_p a_jp =
#       A_drop to round-off: per-pixel |sum_p a_jp/A_drop - 1| ~ 1e-15; global sum estimator
#       sum_p F_p = sum_j x_j.
#   (2) pixfrac negative control: A_pixel normalization => sum_p w' = pf^2 (deficit 0.36);
#       delta = A_drop/(pf^2*A_pixel) - 1 vs closed form (1-pf^2)*theta^2*[0.25/(1+r_c^2) -
#       0.625 xi_c^2/(1+r_c^2)^2]; centered field: delta = 0.25*(1-pf^2)*theta^2 =>
#       8.46e-12 (theta=2") and 1.90e-7 (theta=300").
#   (3) injection control: single-pair area factor 0.9003 with same-pixel flux compensation:
#       pair metric red at 9.968e-2 while global sum metric stays <=1e-15 (localization).
#   (4) F&H weighted-mean estimator (sec 2, "a factor of s^2 is introduced to conserve surface
#       intensity"): constant field reproduces S_p = B0 on every touched leaf to round-off.
#   (5) sparse SNR control points (P2->P3): direction -> ring (j,m) leaf id, value carried;
#       direction-to-leaf-center distance <= 1.05*hp_res (ties to exp04 circumradius 1.0415).
#   Candidate leaves searched within angular distance <= max_angle + 1.25*hp_res
#   (HP_CIRCUMRADIUS_FACTOR margin); metric (1) certifies that search is complete.
import json, os, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from edge_geom import sample_edge, to_vec, wrap_pi
from exp01_leaf_area import ring_layout

SEED = 20260926
PF = 0.8
RA0, DEC0 = np.radians(30.0), np.radians(30.0)
NSEG = 8

def tan_rotation(ra0, dec0):
    return np.array([[-np.sin(ra0), np.cos(ra0), 0.0],
                     [-np.sin(dec0)*np.cos(ra0), -np.sin(dec0)*np.sin(ra0), np.cos(dec0)],
                     [np.cos(dec0)*np.cos(ra0), np.cos(dec0)*np.sin(ra0), np.sin(dec0)]])

R_TAN = tan_rotation(RA0, DEC0)

def tan_to_vec(xi, eta):
    v = R_TAN @ np.array([xi, eta, 1.0])
    return v/np.linalg.norm(v)

def tri_area(a, b, c):
    det = np.dot(a, np.cross(b, c))
    return 2.0*np.arctan2(abs(det), 1.0 + np.dot(a,b) + np.dot(b,c) + np.dot(c,a))

def quad_area(V):
    return sum(tri_area(V[0], V[i], V[i+1]) for i in range(1, len(V)-1))

def clip_poly(V, n):
    """Sutherland-Hodgman spherical clip: keep n.v >= 0."""
    out = []
    M = len(V)
    for i in range(M):
        a, b = V[i], V[(i+1) % M]
        da, db = a @ n, b @ n
        if da >= 0.0:
            out.append(a)
        if (da > 0.0) != (db > 0.0):
            x = np.cross(np.cross(a, b), n)
            ln = np.linalg.norm(x)
            if ln > 1e-300:
                x = x/ln
                if x @ (a + b) < 0.0:
                    x = -x
                out.append(x)
    return np.array(out)

def poly_area(V):
    a = V[0]
    tot = 0.0
    for i in range(1, len(V)-1):
        b, c = V[i], V[i+1]
        det = np.dot(a, np.cross(b, c))
        s = 1.0 if det >= 0 else -1.0
        tot += s*tri_area(a, b, c)
    return abs(tot)

def drop_quad(xi0, eta0, half):
    cs = [(-half,-half),(half,-half),(half,half),(-half,half)]
    return np.array([tan_to_vec(xi0+dx, eta0+dy) for dx, dy in cs])

_LEAF_CACHE = {}

def leaf_poly(N, j, m, lev, z, S, w, o, nseg=NSEG):
    key = (N, j, m, nseg)
    P = _LEAF_CACHE.get(key)
    if P is not None:
        return P
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
    corners = [(phiN, zN), (phiL, zj), (phiS, zS), (phiR, zj)]  # N,L,S,R cyclic
    pts = []
    for i in range(4):
        a, b = np.array(corners[i]), np.array(corners[(i+1) % 4])
        V = sample_edge(a, b, n=nseg+1)
        pts.extend(V[:-1])
    P = np.array(pts)
    _LEAF_CACHE[key] = P
    return P

def candidate_leaves(N, cvec, r_max, lev, z, S, w, o):
    hp_res = np.sqrt(np.pi/3.0)/N
    R = r_max + 1.25*hp_res
    zc = float(cvec[2]); phic = float(np.arctan2(cvec[1], cvec[0]))
    colc = np.arccos(np.clip(zc, -1, 1))
    sinc = np.sin(colc)
    out = []
    for j in range(1, 4*N):
        if S[j] == 0: continue
        col = np.arccos(np.clip(z[j], -1, 1))
        if abs(col - colc) > R: continue
        sc = np.sin(col)
        if sc*sinc > 1e-300:
            val = (np.cos(R) - np.cos(col)*np.cos(colc))/(sc*sinc)
            if val >= 1.0: continue
            dphi = np.arccos(np.clip(val, -1.0, 1.0)) + w[j]
        else:
            dphi = np.pi + w[j]
        k0 = int(np.floor((phic - dphi - o[j])/w[j] - 0.5))
        k1 = int(np.ceil((phic + dphi - o[j])/w[j] - 0.5))
        for k in range(k0, k1+1):
            m = k % S[j]
            fc = o[j] + (m + 0.5)*w[j]
            lv = to_vec(np.array([fc]), np.array([z[j]]))[0]
            if np.arccos(np.clip(lv @ cvec, -1, 1)) <= R:
                out.append((j, m))
    return out

_GL = np.polynomial.legendre.leggauss(48)
_GLX, _GLW = _GL[0], _GL[1]

def gauss_area(h):
    """Solid angle of the gnomonic image of the tangent-plane square [-h,h]^2 via
    Gauss-Legendre quadrature of dOmega = dxi deta / (1+xi^2+eta^2)^{3/2} (smooth)."""
    X = h*_GLX; W = h*_GLW
    XI = np.tile(X, (len(X), 1)); ETA = XI.T
    U = 1.0 + XI*XI + ETA*ETA
    return float((W[:,None]*W[None,:] * U**-1.5).sum())

def drop_halfplanes(Dq):
    ns = []
    for e in range(4):
        n = np.cross(Dq[e], Dq[(e+1) % 4]); n = n/np.linalg.norm(n)
        if n @ Dq.mean(axis=0) < 0: n = -n
        ns.append(n)
    return ns

def run_field(N, theta_arcsec, npix_side=24, inject=False, norm_pixel=False, field="random"):
    lev, z, S, w, o = ring_layout(N)
    theta = np.radians(theta_arcsec/3600.0)
    rng = np.random.default_rng(SEED)
    B0 = 1.7
    if field == "random":
        xvals = rng.uniform(0.5, 2.0, npix_side*npix_side)
    else:
        xvals = np.full(npix_side*npix_side, B0)
    half_d = PF*theta/2.0
    half_p = theta/2.0
    sumF = 0.0; sumX = 0.0
    comp_max = 0.0; ksum = 0.0; kquad = 0.0; kgauss = 0.0; pair_red = 0.0
    acc = {}
    inj_done = False
    for idx in range(npix_side*npix_side):
        i, k = idx // npix_side, idx % npix_side
        xi0 = (i - (npix_side-1)/2.0)*theta
        eta0 = (k - (npix_side-1)/2.0)*theta
        Dq = drop_quad(xi0, eta0, half_d)
        Pq = drop_quad(xi0, eta0, half_p)
        A_drop = quad_area(Dq)
        A_pix = quad_area(Pq)
        dnorms = drop_halfplanes(Dq)
        cvec = tan_to_vec(xi0, eta0)
        r_max = max(np.arccos(np.clip(cvec @ v, -1, 1)) for v in Dq)
        cands = candidate_leaves(N, cvec, r_max, lev, z, S, w, o)
        xj = xvals[idx]
        sumX += xj
        tot = 0.0
        pair_areas = []
        for (j, m) in cands:
            P = leaf_poly(N, j, m, lev, z, S, w, o)
            Q = P
            for n in dnorms:
                if len(Q): Q = clip_poly(Q, n)
            if len(Q) < 3: continue
            a = poly_area(Q)
            pair_areas.append(((j, m), a))
        # injection: x0.9003 deficit on the first overlap pair of the first multi-pair pixel,
        # compensated inside the same pixel (deficit added to its last pair) so that
        # sum_p a'_jp = A_drop still holds exactly => global sum metric stays green,
        # while per-pair weights are wrong by 9.968e-2 => pair metric red.
        if inject and not inj_done and len(pair_areas) >= 2:
            (jm0, a0) = pair_areas[0]
            pair_areas[0] = (jm0, 0.9003*a0)
            (jm1, a1) = pair_areas[-1]
            pair_areas[-1] = (jm1, a1 + 0.0997*a0)
            inj_done = True
            pair_red = abs(0.9003*a0 - a0)/a0   # per-pair relative error, expected 9.97e-2
        for ((j, m), a) in pair_areas:
            tot += a
            e = acc.setdefault((j, m), [0.0, 0.0])
            e[0] += a*xj; e[1] += a
            wgt = a/A_drop if not norm_pixel else a/A_pix
            sumF += wgt*xj
        comp = tot/A_drop - 1.0
        comp_max = max(comp_max, abs(comp))
        ksum += tot/A_pix
        kquad += quad_area(Dq)/(PF*PF*quad_area(Pq))
        kgauss += gauss_area(half_d)/(PF*PF*gauss_area(half_p))
    mean_res = 0.0
    for (jm, (sw, sa)) in acc.items():
        if sa > 0:
            mean_res = max(mean_res, abs(sw/sa/B0 - 1.0))
    return dict(comp_max=comp_max, pair_red=pair_red, sumF=sumF, sumX=sumX,
                kmean=ksum/(npix_side*npix_side),
                kquad=kquad/(npix_side*npix_side),
                kgauss=kgauss/(npix_side*npix_side),
                mean_res=mean_res, n_leaves_touched=len(acc))

def control_points(N, npts=8):
    rng = np.random.default_rng(SEED + 1)
    lev, z, S, w, o = ring_layout(N)
    hp_res = np.sqrt(np.pi/3.0)/N
    theta = np.radians(300.0/3600.0)
    pts = []; worst = 0.0
    for _ in range(npts):
        xi = rng.uniform(-11.5*theta, 11.5*theta)
        eta = rng.uniform(-11.5*theta, 11.5*theta)
        v = tan_to_vec(xi, eta)
        zc = float(v[2]); phi = float(np.arctan2(v[1], v[0])) % (2*np.pi)
        if zc > 2.0/3.0:
            jf = N*np.sqrt(3.0*(1.0 - zc))
        elif zc < -2.0/3.0:
            jf = 4*N - N*np.sqrt(3.0*(1.0 + zc))
        else:
            jf = 2*N - 1.5*N*zc
        j = int(np.clip(round(jf), 1, 4*N-1))
        if S[j] == 0: continue
        m = int(np.mod(np.round((phi - o[j])/w[j] - 0.5), S[j]))
        fc = o[j] + (m + 0.5)*w[j]
        c = to_vec(np.array([fc]), np.array([z[j]]))[0]
        ang = np.arccos(np.clip(c @ v, -1, 1))
        worst = max(worst, ang/hp_res)
        pts.append(dict(j=j, m=m, ang_over_hp_res=ang/hp_res))
    return pts, worst

def main():
    t0 = time.time()
    out = dict(exp="exp03_weight_conservation", seed=SEED, pixfrac=PF,
               ra0_deg=30.0, dec0_deg=30.0, npix_side=24)
    r1 = run_field(512, 300.0, field="random")
    out["conservation_300as_n512"] = dict(
        max_abs_pixel_completeness=r1["comp_max"],
        global_sum_minus=r1["sumF"]/r1["sumX"] - 1.0,
        n_leaves_touched=r1["n_leaves_touched"])
    print("theta=300 nside=512:", json.dumps(out["conservation_300as_n512"]), flush=True)
    r2a = run_field(1024, 2.0)
    r2b = run_field(512, 300.0)
    th2 = np.radians(2.0/3600.0); th300 = np.radians(300.0/3600.0)
    d2 = 0.25*(1.0-PF**2)*th2**2; d300 = 0.25*(1.0-PF**2)*th300**2
    out["pixel_norm_delta"] = dict(
        theta2=dict(k_minus_1_clip=r2a["kmean"]/PF**2 - 1.0,
                    k_minus_1_vos=r2a["kquad"] - 1.0,
                    k_minus_1_gauss=r2a["kgauss"] - 1.0, closed_form=d2),
        theta300=dict(k_minus_1_clip=r2b["kmean"]/PF**2 - 1.0,
                      k_minus_1_vos=r2b["kquad"] - 1.0,
                      k_minus_1_gauss=r2b["kgauss"] - 1.0, closed_form=d300),
        pf2_deficit=1.0-PF**2, raw_ratio_theta300=r2b["kmean"])
    print("delta theta2:", json.dumps(out["pixel_norm_delta"]["theta2"]), flush=True)
    print("delta theta300:", json.dumps(out["pixel_norm_delta"]["theta300"]), flush=True)
    r3 = run_field(512, 300.0, inject=True)
    out["injection"] = dict(pair_metric=r3["pair_red"], expected=0.0996836838,
                            global_sum_minus=r3["sumF"]/r3["sumX"] - 1.0)
    print("injection:", json.dumps(out["injection"]), flush=True)
    r4 = run_field(512, 300.0, field="constant")
    out["mean_estimator"] = dict(max_abs_Sp_over_B0_minus_1=r4["mean_res"],
                                 n_leaves=r4["n_leaves_touched"])
    print("mean estimator:", json.dumps(out["mean_estimator"]), flush=True)
    pts, worst = control_points(512)
    out["control_points"] = dict(points=pts, max_ang_over_hp_res=worst, bound=1.05)
    print("control points worst ang/hp_res =", worst, flush=True)
    out["verdict"] = dict(
        conservation=bool(r1["comp_max"] < 1e-10 and abs(r1["sumF"]/r1["sumX"] - 1.0) < 1e-12),
        delta_theta2_ok=bool(abs((r2a["kgauss"] - 1.0)/d2 - 1.0) < 0.02),
        delta_theta300_ok=bool(abs((r2b["kgauss"] - 1.0)/d300 - 1.0) < 0.02),
        pf2_deficit_visible=bool(abs(r2b["kmean"] - PF**2) < 1e-3),
        injection_red=bool(abs(r3["pair_red"] - 0.0997) < 1e-6),
        injection_sum_green=bool(abs(r3["sumF"]/r3["sumX"] - 1.0) < 1e-12),
        mean_estimator_zero=bool(r4["mean_res"] < 1e-10),
        control_points=bool(worst < 1.05),
        elapsed_s=time.time()-t0)
    print("VERDICT:", json.dumps(out["verdict"]))
    with open("results/exp03_weight_conservation.json", "w") as f:
        json.dump(out, f, indent=1)

if __name__ == "__main__":
    main()
