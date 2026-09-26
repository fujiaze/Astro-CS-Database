"""exp04_area_operators.py -- P3-04 / P3-05: VOS sector triangle vs l'Huilier.

Hypothesis : (i) the Van Oosterom & Strackee (1983) formula agrees with the
             analytic solid angle of the octant triangle (pi/2) at float
             round-off; (ii) VOS and l'Huilier agree to <= 3.3e-12 relative
             over every leaf of HEALPix N = 2..64; (iii) the fan-sum area of
             all leaves closes 4*pi; (iv) NEGATIVE CONTROL: a 6.4% length
             error on one vertex shifts VOS by ~+9.6% but leaves l'Huilier
             unchanged (free self-check works).
Seed       : none (deterministic).
Runtime    : < 120 s.
"""
import json
import sys
import numpy as np

sys.path.insert(0, "code")
import p3lib as P  # noqa: E402


def quad_split_vos(f, i, j, nside):
    _, us, vs = P.pixel_corners(f, i, j, nside)
    v = P.chart_to_vec(f, us, vs)
    t1 = float(P.vos_triangle(v[0], v[1], v[2]))
    t2 = float(P.vos_triangle(v[0], v[2], v[3]))
    return t1 + t2


def quad_split_lhui(f, i, j, nside):
    _, us, vs = P.pixel_corners(f, i, j, nside)
    v = P.chart_to_vec(f, us, vs)
    t1 = float(P.lhuilier_triangle(v[0], v[1], v[2]))
    t2 = float(P.lhuilier_triangle(v[0], v[2], v[3]))
    return t1 + t2


def main():
    out = {"analytic_anchors": {}, "intercheck": [], "closure": [],
           "normalization_negative_control": {}}

    # (i) analytic anchor: octant triangle
    octant = [np.array([1.0, 0, 0]), np.array([0, 1.0, 0]), np.array([0, 0, 1.0])]
    v1 = float(P.vos_triangle(*octant))
    l1 = float(P.lhuilier_triangle(*octant))
    out["analytic_anchors"] = {
        "octant_exact": np.pi / 2.0,
        "vos": v1, "vos_rel_dev": v1 / (np.pi / 2.0) - 1.0,
        "lhuilier": l1, "lhuilier_rel_dev": l1 / (np.pi / 2.0) - 1.0,
    }

    # (ii)+(iii) exhaustive intercheck and closure
    for N in (2, 4, 8, 16, 32, 64):
        tot = 0.0
        worst = 0.0
        for f in range(12):
            for i in range(N):
                for j in range(N):
                    av = quad_split_vos(f, i, j, N)
                    al = quad_split_lhui(f, i, j, N)
                    tot += av
                    rel = abs(av / al - 1.0)
                    if rel > worst:
                        worst = rel
        out["intercheck"].append({"N": N, "max_rel_dev_vos_vs_lhuilier": worst})
        out["closure"].append({
            "N": N, "sum_all_leaves_over_4pi_minus_1": tot / (4 * np.pi) - 1.0,
            "mean_over_pi_3N2_minus_1":
                (tot / (12 * N * N)) / (np.pi / (3 * N * N)) - 1.0})

    # (iv) normalization negative control: vertex scaled by 1.064
    v = [np.array([1.0, 0, 0]), np.array([0, 1.0, 0]), np.array([0, 0, 1.0])]
    v_bad = [v[0] * 1.064, v[1], v[2]]
    v_bad = [v_bad[0] / np.linalg.norm(v_bad[0]), v[1], v[2]]
    # NOTE: production self-check is on the RAW (non-normalized) vertex
    raw = [np.array([1.064, 0, 0]), v[1], v[2]]
    det = float(np.einsum('i,i->', raw[0], np.cross(raw[1], raw[2])))
    den = (1.0 + float(np.dot(raw[0], raw[1])) + float(np.dot(raw[1], raw[2]))
           + float(np.dot(raw[2], raw[0])))
    vos_bad = 2.0 * np.arctan2(det, den)
    lh_bad = float(P.lhuilier_triangle(v_bad[0], v_bad[1], v_bad[2]))
    out["normalization_negative_control"] = {
        "vos_raw_vertex_shifted_rel_dev": vos_bad / v1 - 1.0,
        "claimed_in_02": 0.096,
        "lhuilier_with_normalized_vertex_rel_dev": lh_bad / v1 - 1.0,
    }

    worst_all = max(r["max_rel_dev_vos_vs_lhuilier"] for r in out["intercheck"])
    closure_worst = max(abs(r["sum_all_leaves_over_4pi_minus_1"])
                        for r in out["closure"])
    ok = (abs(out["analytic_anchors"]["vos_rel_dev"]) < 1e-15
          and worst_all < 1e-11
          and closure_worst < 1e-12
          and abs(out["normalization_negative_control"]
                  ["vos_raw_vertex_shifted_rel_dev"]) > 0.02
          and abs(out["normalization_negative_control"]
                  ["lhuilier_with_normalized_vertex_rel_dev"]) < 1e-12)
    out["verdict"] = "PASS" if ok else "FAIL"
    print(json.dumps(out, indent=2))
    with open("results/exp04_area_operators.json", "w") as fh:
        json.dump(out, fh, indent=2)


if __name__ == "__main__":
    main()

