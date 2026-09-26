#!/usr/bin/env python3
# exp05_sagitta_subdiv.py -- R3-06: leaf-edge sagitta vs great-circle chords; frozen budget
#   adaptive_subdiv_threshold = 1e-6*hp_res, adaptive_max_depth = 8.
# Measures TWO curve conventions for cap edges:
#   chart-curve: z = sgn - c/phi_t^2 (Gorski 2005 sec 5.3: polar-cap boundaries cos(theta)=a+b/phi^2)
#   phiz-straight: straight segment in (phi,z) (the 02-ledger line-58 blanket claim)
# Belt edges: both conventions coincide (z=a+b*phi is straight in (phi,z)).
# Claims tested: sagitta >> 1e-6*hp_res budget; scale invariance in N; ~4x shrink per subdivision;
#   depth needed ~ 8.15; depth-8 residual ~ 1.2e-6*hp_res; meridian edges give exactly 0 (negative control).
import json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from edge_geom import sample_edge, to_vec, wrap_pi
from exp01_leaf_area import ring_layout

SEED = 20260926

def edges_of_pixel(N, j, m, lev, z, S, w, o):
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
    corners = [(phiN, zN), (phiL, zj), (phiS, zS), (phiR, zj)]
    return [(corners[i], corners[(i+1) % 4]) for i in range(4)]

def plane_dev(V, v1, v2):
    nrm = np.cross(v1, v2); ln = np.linalg.norm(nrm)
    if ln < 1e-300: return 0.0
    nrm = nrm/ln
    return float(np.arcsin(np.clip(np.abs(V @ nrm).max(), 0.0, 1.0)))

def sag_pair(a, b, hp_res, n=33):
    """(sag_chart, sag_phiz) in hp_res units for one edge (phi unwrapped; pole phi fixed)."""
    a2 = (float(a[0]), float(a[1])); b2 = (float(b[0]), float(b[1]))
    if abs(abs(a2[1])-1.0) < 1e-15 and abs(abs(b2[1])-1.0) >= 1e-15:
        a2 = (b2[0], a2[1])
    if abs(abs(b2[1])-1.0) < 1e-15 and abs(abs(a2[1])-1.0) >= 1e-15:
        b2 = (a2[0], b2[1])
    b2 = (a2[0] + wrap_pi(b2[0]-a2[0]), b2[1])
    Vc = sample_edge(np.array(a2), np.array(b2), n=n)
    v1, v2 = Vc[0], Vc[-1]
    t = np.linspace(0.0, 1.0, n)
    phiz = np.stack([a2[0] + t*(b2[0]-a2[0]), a2[1] + t*(b2[1]-a2[1])], axis=1)
    Vz = to_vec(phiz[:,0], phiz[:,1])
    return plane_dev(Vc, v1, v2)/hp_res, plane_dev(Vz, v1, v2)/hp_res

def is_meridian(a, b):
    return (abs(abs(a[1])-1.0) < 1e-15 or abs(abs(b[1])-1.0) < 1e-15
            or abs(wrap_pi(a[0]-b[0])) < 1e-12)

def classify_stats(N, n=33):
    lev, z, S, w, o = ring_layout(N)
    hp_res = np.sqrt(np.pi/3.0)/N
    cap_c = []; cap_z = []; belt_c = []; belt_z = []; mer = []
    worst = (0.0, None)
    for j in range(1, 4*N):
        if S[j] == 0: continue
        for m in range(S[j]):
            for (a, b) in edges_of_pixel(N, j, m, lev, z, S, w, o):
                sc, sz = sag_pair(a, b, hp_res, n=n)
                if is_meridian(a, b):
                    mer.append(max(sc, sz))
                elif min(abs(a[1]), abs(b[1])) >= 2.0/3.0 - 1e-12:
                    cap_c.append(sc); cap_z.append(sz)
                    if sc > worst[0]: worst = (sc, (j, m, a, b))
                else:
                    belt_c.append(sc); belt_z.append(sz)
    return dict(cap_chart=cap_c, cap_phiz=cap_z, belt_chart=belt_c, belt_phiz=belt_z,
                meridian=mer), worst

def sampled_cap_max(N, n_pix, n=33):
    rng = np.random.default_rng(SEED + N)
    lev, z, S, w, o = ring_layout(N)
    hp_res = np.sqrt(np.pi/3.0)/N
    mx = 0.0
    for _ in range(n_pix):
        j = int(rng.integers(1, N+1))
        m = int(rng.integers(0, S[j]))
        for (a, b) in edges_of_pixel(N, j, m, lev, z, S, w, o):
            if is_meridian(a, b): continue
            if min(abs(a[1]), abs(b[1])) < 2.0/3.0 - 1e-12: continue
            sc, sz = sag_pair(a, b, hp_res, n=n)
            mx = max(mx, sc)
    return mx

def subdivision_study(N, worst, max_depth=10):
    hp_res = np.sqrt(np.pi/3.0)/N
    (s0, (j, m, a, b)) = worst
    out = []
    for d in range(0, max_depth+1):
        M = 2**d
        P = sample_edge(np.array(a), np.array(b), n=M+1)
        seg_max = 0.0
        for i in range(M):
            q1, q2 = P[i], P[i+1]
            p1 = (np.arctan2(q1[1], q1[0]), q1[2])
            p2 = (np.arctan2(q2[1], q2[0]), q2[2])
            V = sample_edge(np.array(p1), np.array(p2), n=9)
            nrm = np.cross(q1, q2); ln = np.linalg.norm(nrm)
            if ln < 1e-300: continue
            nrm = nrm/ln
            dev = np.abs(V @ nrm).max()
            seg_max = max(seg_max, float(np.arcsin(np.clip(dev, 0, 1))/hp_res))
        out.append(dict(depth=d, n_seg=M, max_sag_hp_res=seg_max))
    return out

def random_leaf_fraction(N, n_leaves, n=17):
    rng = np.random.default_rng(SEED)
    lev, z, S, w, o = ring_layout(N)
    hp_res = np.sqrt(np.pi/3.0)/N
    over_c = 0; over_z = 0; tot = 0
    for _ in range(n_leaves):
        j = int(rng.integers(1, 4*N))
        if S[j] == 0: continue
        m = int(rng.integers(0, S[j]))
        for (a, b) in edges_of_pixel(N, j, m, lev, z, S, w, o):
            sc, sz = sag_pair(a, b, hp_res, n=n)
            tot += 1
            if sc > 1e-6: over_c += 1
            if sz > 1e-6: over_z += 1
    return over_c, over_z, tot

def main():
    out = dict(exp="exp05_sagitta_subdiv", seed=SEED)
    stats, worst = classify_stats(32)
    row = dict(N=32,
               cap_chart_max=float(max(stats["cap_chart"])),
               cap_phiz_max=float(max(stats["cap_phiz"])),
               belt_chart_max=float(max(stats["belt_chart"])),
               belt_phiz_max=float(max(stats["belt_phiz"])),
               belt_chart_median=float(np.median(stats["belt_chart"])),
               meridian_max=float(max(stats["meridian"])) if stats["meridian"] else 0.0)
    out["N32"] = row
    print("N=32:", json.dumps(row), flush=True)
    sc256 = sampled_cap_max(256, 2000)
    sc1024 = sampled_cap_max(1024, 2000)
    out["cap_scale"] = dict(N32=row["cap_chart_max"], N256=sc256, N1024=sc1024, note="chart-curve convention")
    print("cap_max chart-curve: N32=%.6f N256=%.6f N1024=%.6f" % (row["cap_chart_max"], sc256, sc1024), flush=True)
    sub = subdivision_study(32, worst)
    out["subdivision"] = sub
    ratios = [sub[i]["max_sag_hp_res"]/sub[i+1]["max_sag_hp_res"] for i in range(len(sub)-1)]
    sag0 = sub[0]["max_sag_hp_res"]
    d_need = float(np.log(sag0/1e-6)/np.log(4.0))
    resid8 = sub[8]["max_sag_hp_res"]
    print("subdivision ratios:", ["%.3f" % x for x in ratios], flush=True)
    print("sag0=%.6e depth_needed=%.3f residual_at_depth8=%.6e" % (sag0, d_need, resid8), flush=True)
    oc, oz, tot = random_leaf_fraction(1024, 60000)
    out["fraction"] = dict(N1024_total=tot, frac_over_chart=oc/tot, frac_over_phiz=oz/tot)
    print("frac>1e-6 @N=1024 random 60000 leaves: chart=%.4f phiz=%.4f" % (oc/tot, oz/tot), flush=True)
    out["verdict"] = dict(
        cap_max_scale_invariant=bool(max(row["cap_chart_max"], sc256, sc1024)/min(row["cap_chart_max"], sc256, sc1024) < 1.05),
        subdivision_ratio4=bool(all(3.3 < x < 4.7 for x in ratios)),
        depth_needed=d_need, residual_at_depth8=resid8,
        depth8_over_budget=bool(resid8 > 1e-6),
        meridian_zero=bool(row["meridian_max"] < 1e-15),
        frac_over_chart=oc/tot, frac_over_phiz=oz/tot)
    print("VERDICT:", json.dumps(out["verdict"]))
    with open("results/exp05_sagitta_subdiv.json", "w") as f:
        json.dump(out, f, indent=1)

if __name__ == "__main__":
    main()
