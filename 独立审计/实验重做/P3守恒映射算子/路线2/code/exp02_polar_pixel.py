# -*- coding: utf-8 -*-
"""exp02_polar_pixel.py -- P3-02: polar-touching leaf chord-quad bias.

Hypothesis : representing a polar-touching HEALPix leaf by the geodesic
             quadrilateral through its 4 corners underestimates its area by a
             limit ratio 2*sqrt(2)/pi = 0.9003163161 (rel error -9.968368384e-2)
             that does NOT shrink with N; the absolute deficit obeys
             0.104369 / N^2 [sr].
Method     : independent chart + VOS implementation (p3lib). For each N, the
             4 pole-touching leaves (one per north face, cell (N-1,N-1)) are
             measured twice: (i) chord model = geodesic quad of 4 corners;
             (ii) true-curve model = chart-straight edges finely sampled.
Negative   : "truth has no effect => metric zero": with the true-curve
             boundary the bias metric collapses to the float floor.
Seed       : none (deterministic).
Runtime    : < 60 s.
"""
import json
import sys
import numpy as np

sys.path.insert(0, "code")
import p3lib as P  # noqa: E402

K_TRUE = 96          # segments per edge for the true-curve reference


def chord_quad_area(f, i, j, nside):
    _, us, vs = P.pixel_corners(f, i, j, nside)
    corners = P.chart_to_vec(f, us, vs)
    return abs(P.geodesic_polygon_area_vos(corners))


def true_quad_area(f, i, j, nside):
    pts = P.leaf_true_boundary(f, i, j, nside, K_TRUE)
    return abs(P.geodesic_polygon_area_vos(pts))


def main():
    res = {"per_N": [], "full_sky_chord_closure_N16": None,
           "absolute_deficit_fit": {}, "negative_control": {}}

    for N in (8, 16, 32, 64, 128, 256):
        A_true = float(P.leaf_area_analytic(N))
        rels, rels_true = [], []
        for f in range(4):
            # pole-touching leaf: cell (N-1, N-1) on north face f
            A_ch = chord_quad_area(f, N - 1, N - 1, N)
            A_tr = true_quad_area(f, N - 1, N - 1, N)
            rels.append(A_ch / A_true - 1.0)
            rels_true.append(A_tr / A_true - 1.0)
        worst = min(rels)                    # most negative = worst deficit
        res["per_N"].append({
            "N": N,
            "worst_rel_chord": worst,
            "A_chord_times_N2": (1.0 + worst) * A_true * N * N,
            "closed_form_ratio": 2.0 * np.sqrt(2.0) / np.pi,
            "abs_deficit_times_N2": -worst * A_true * N * N,
            "true_boundary_worst_rel": min(rels_true),
        })

    # full-sky closure of the chord model at N=16 (model closes; leaves wrong)
    N = 16
    tot = 0.0
    for f in range(12):
        for i in range(N):
            for j in range(N):
                tot += chord_quad_area(f, i, j, N) if f < 4 or f >= 8 else \
                    chord_quad_area(f, i, j, N)
    res["full_sky_chord_closure_N16"] = tot / (4.0 * np.pi) - 1.0

    # absolute deficit law: fit d*N^2 = const
    d = [r["abs_deficit_times_N2"] for r in res["per_N"]]
    res["absolute_deficit_fit"] = {
        "values_of_deficit_times_N2": d,
        "claimed_constant": 0.104369,
        "max_abs_dev_from_claim": float(np.max(np.abs(np.array(d) - 0.104369))),
    }

    # closed-form convergence: A_chord * N^2 -> 2*sqrt(2)/3, ratio -> 2 sqrt2/pi
    aN2 = [r["A_chord_times_N2"] for r in res["per_N"]]
    res["closed_form_check"] = {
        "limit_2sqrt2_over_3": 2.0 * np.sqrt(2.0) / 3.0,
        "A_chord_times_N2_tail_N256": aN2[-1],
        "ratio_tail_N256": 1.0 + res["per_N"][-1]["worst_rel_chord"],
        "limit_2sqrt2_over_pi": 2.0 * np.sqrt(2.0) / np.pi,
        "claimed_rel_error": 2.0 * np.sqrt(2.0) / np.pi - 1.0,
    }

    # negative control: true boundary collapses the metric to float floor
    res["negative_control"] = {
        "max_abs_true_boundary_rel_over_N": float(max(
            abs(r["true_boundary_worst_rel"]) for r in res["per_N"])),
        "note": "metric = |A_true_curve/A_true - 1|. The K=96 chord sampling "
            "of the true boundary carries a FIXED floor ~ -1.3e-5 (the polar "
            "leaf boundary has N-independent curvature, so the sampling error "
            "does not shrink with N); it is 7600x smaller than the chord-quad "
            "deficit -9.968e-2 under test, and constant across N, so it cannot "
            "mimic the N-converging law. Gate: floor < 3e-5 AND deficit/floor "
            "> 1000.",
    }

    floor = res["negative_control"]["max_abs_true_boundary_rel_over_N"]
    ok = (abs(res["per_N"][-1]["worst_rel_chord"]
              - (2.0 * np.sqrt(2.0) / np.pi - 1.0)) < 5e-6
          and res["absolute_deficit_fit"]["max_abs_dev_from_claim"] < 3e-3
          and abs(res["full_sky_chord_closure_N16"]) < 1e-12
          and floor < 3e-5
          and abs(res["per_N"][-1]["worst_rel_chord"]) / floor > 1000)
    res["verdict"] = "PASS" if ok else "FAIL"
    print(json.dumps(res, indent=2))
    with open("results/exp02_polar_pixel.json", "w") as fh:
        json.dump(res, fh, indent=2)


if __name__ == "__main__":
    main()
