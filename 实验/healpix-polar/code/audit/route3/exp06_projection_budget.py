#!/usr/bin/env python3
# exp06_projection_budget.py -- R3-08/R3-09: TAN projection error budget.
# (a) Planar-vs-solid area of a gnomonic pixel image: the image of a tangent-plane square has
#     great-circle sides, so its solid angle is EXACT via VOS on the 4 corners.
#     ratio - 1 = plane_area/solid_area - 1 = +1.5*rho_c^2 + O(rho^4, theta^2)
#     (plane measure overestimates; coefficient 1.5). Negative control: rho_c -> 0 gives 0.
# (b) Orthographic reconstruction of drop edge half-planes vs true great circles: fitting the
#     drop corners ORTHOGRAPHICALLY (v.e1, v.e2) and building 2D edge lines yields small-circle
#     planes {v: N.v = c}, deviating from the true great circle by ~ h^2/2 over the edge (h =
#     tangent half-side). At the code threshold max_angular_radius_for_tan = 1e-3 rad the
#     deviation is ~5e-7 rad (the "delta < 4e-8" comment at spherical_overlap.cpp:1002 is wrong).
import json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from exp03_weight_conservation import tan_to_vec, quad_area

def part_a(theta_arcsec=10.0):
    th = np.radians(theta_arcsec/3600.0)
    rows = []
    for rho in [0.0, 0.02, 0.05, 0.1, 0.2, 0.4, 0.7, 1.0]:
        xi0, eta0 = rho, 0.0
        h = th/2.0
        Dq = np.array([tan_to_vec(xi0+dx, eta0+dy) for dx, dy in
                       [(-h,-h),(h,-h),(h,h),(-h,h)]])
        solid = quad_area(Dq)
        plane = th*th
        ratio = plane/solid - 1.0
        rows.append(dict(rho_c=rho, ratio_minus_1=ratio, one5rho2=1.5*rho*rho,
                         rel_vs_1p5rho2=(ratio - 1.5*rho*rho)/max(1.5*rho*rho, 1e-300) if rho > 0 else 0.0))
        print("rho=%.2f ratio-1=%+.6e 1.5rho2=%+.6e" % (rho, ratio, 1.5*rho*rho), flush=True)
    return rows

def part_b(half_list=(1e-4, 3e-4, 1e-3, 3e-3, 1e-2), n=65):
    """Max angular deviation between the orthographic small-circle edge plane and the true
    great-circle edge, sampled over the drop square boundary, vs tangent half-side h."""
    rows = []
    xi0 = eta0 = 0.0
    for h in half_list:
        cs = [(-h,-h),(h,-h),(h,h),(-h,h)]
        V = [tan_to_vec(xi0+dx, eta0+dy) for dx, dy in cs]
        dev = 0.0
        for e in range(4):
            v1, v2 = np.array(V[e]), np.array(V[(e+1) % 4])
            n_gc = np.cross(v1, v2); n_gc = n_gc/np.linalg.norm(n_gc)
            if n_gc @ v1 < 0: n_gc = -n_gc   # interior side: n_gc.v >= 0
            # orthographic reconstruction: 2D line through (v1.e1, v1.e2),(v2.e1, v2.e2)
            e1 = np.array([1.0, 0.0, 0.0]); e2 = np.array([0.0, 1.0, 0.0])
            p1 = np.array([v1[0], v1[1]]); p2 = np.array([v2[0], v2[1]])
            d2 = p2 - p1
            n2 = np.array([-d2[1], d2[0]]); n2 = n2/np.linalg.norm(n2)
            c2 = n2 @ p1
            N3 = n2[0]*e1 + n2[1]*e2
            c3 = N3 @ v1
            t = np.linspace(0.0, 1.0, n)
            # sample true great-circle arc between v1 and v2 (slerp)
            om = np.arccos(np.clip(v1 @ v2, -1, 1))
            Sl = (np.sin((1-t)*om)[:,None]*v1 + np.sin(t*om)[:,None]*v2)/np.sin(om)
            # signed angular offset of Sl from the small circle plane, evaluated at the arc
            off = (Sl @ N3 - c3)/np.linalg.norm(N3)
            dev = max(dev, float(np.abs(off).max()))
        rows.append(dict(h=h, max_dev_rad=dev, dev_over_h2=dev/(h*h)))
        print("h=%.0e max_dev=%.6e rad  dev/h^2=%.4f" % (h, dev, dev/(h*h)), flush=True)
    return rows

def main():
    out = dict(exp="exp06_projection_budget")
    out["part_a_ratio"] = part_a()
    out["part_b_ortho"] = part_b()
    a = out["part_a_ratio"]
    mid = [r for r in a if 0.05 <= r["rho_c"] <= 0.7]
    b = out["part_b_ortho"]
    h1e3 = [r for r in b if abs(r["h"] - 1e-3) < 1e-12][0]
    # exact model (measured): ratio - 1 = (1+rho_c^2)^{3/2} - 1 + theta^2/4 + O(rho^2 theta^2);
    # the ledger's +1.5*rho^2 is the second-order expansion of (1+rho_c^2)^{3/2}-1, and the
    # theta^2/4 term is the square's own self-compression (rho=0 row).
    th = np.radians(10.0/3600.0)
    model = [((1.0 + r["rho_c"]**2)**1.5 - 1.0) + th*th/4.0 for r in a]
    exact_ok = all(abs(r["ratio_minus_1"] - m0) < 5e-8 for r, m0 in zip(a, model))
    out["exact_model"] = [float(m0) for m0 in model]
    out["exact_form_check"] = exact_ok
    out["verdict"] = dict(
        planar_overestimates_1p5rho2=bool(exact_ok and all(r["ratio_minus_1"] > 1.5*r["rho_c"]**2 - 1e-12 for r in a if r["rho_c"] > 0)),
        exact_closed_form=bool(exact_ok),
        rho0_zero=bool(abs(a[0]["ratio_minus_1"] - th*th/4.0) < 1e-12),
        ortho_quadratic=bool(all(abs(r["dev_over_h2"] - b[-1]["dev_over_h2"])/(b[-1]["dev_over_h2"]) < 0.2 for r in b)),
        ortho_dev_at_1e3=h1e3["max_dev_rad"],
        comment_4e8_is_wrong=bool(h1e3["max_dev_rad"] > 4e-8))
    print("VERDICT:", json.dumps(out["verdict"]))
    with open("results/exp06_projection_budget.json", "w") as f:
        json.dump(out, f, indent=1)

if __name__ == "__main__":
    main()
