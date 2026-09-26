# -*- coding: utf-8 -*-
"""exp01_leaf_area.py -- P3-01: A_leaf = pi/(3 N^2) three-leg experiment.

Hypothesis : every HEALPix leaf has area pi/(3 N^2) [sr], as an analytic
             identity of the equal-area chart (|J| = pi/3 per base face).
Method     : (a) numeric integration of the chart Jacobian over whole faces;
             (b) algebraic identity pi/(3N^2) == 4*pi/(12 N^2) == 4pi/npix;
             (c) zero-effect negative control (true value => metric exactly 0)
                 versus a wrong candidate (metric = 0.25).
Seed       : none needed (fully deterministic).
Runtime    : < 5 s.
"""
import json
import sys
import numpy as np

sys.path.insert(0, "code")
import p3lib as P  # noqa: E402


def face_area_numeric(f, n_grid=400):
    """Integrate |J| over one base face by midpoint quadrature in (u, v)."""
    edges = np.linspace(0.0, 1.0, n_grid + 1)
    cu = 0.5 * (edges[:-1] + edges[1:])
    cv = 0.5 * (edges[:-1] + edges[1:])
    U, V = np.meshgrid(cu, cv, indexing="ij")
    J = P.jacobian_chart(f, U, V)
    return float(np.sum(J)) / (n_grid * n_grid)


def main():
    res = {"face_area_numeric": {}, "identity": {}, "negative_control": {}}

    # (a) chart integration closes each face at pi/3
    for f in (0, 4, 8):          # north-polar, equatorial, south face
        A = face_area_numeric(f)
        res["face_area_numeric"][f"face_{f}"] = {
            "area_sr": A, "rel_dev_from_pi_over_3": A / (np.pi / 3.0) - 1.0}

    # (b) algebraic identity and cross-form agreement
    for N in (4, 16, 64, 256, 1024):
        a1 = P.leaf_area_analytic(N)                 # pi/(3 N^2)
        a2 = 4.0 * np.pi / (12.0 * N ** 2)           # 4pi/(12 N^2)
        a3 = 4.0 * np.pi / (12.0 * N * N)            # 4pi/npix (third-party form)
        res["identity"][str(N)] = {
            "A_leaf": a1,
            "abs(a1-a2)": abs(a1 - a2),
            "abs(a1-a3)": abs(a1 - a3),
            "Npix_total_area_rel": (a1 * 12 * N * N) / (4 * np.pi) - 1.0,
        }

    # (c) negative control: metric must vanish for the true value, not for a
    #     wrong candidate. Metric = |A * Npix / (4 pi) - 1|.
    N = 64
    A_true = P.leaf_area_analytic(N)
    A_wrong = np.pi / (4.0 * N ** 2)                 # plausible-looking wrong form
    res["negative_control"] = {
        "metric_true": abs(A_true * 12 * N * N / (4 * np.pi) - 1.0),
        "metric_wrong_candidate": abs(A_wrong * 12 * N * N / (4 * np.pi) - 1.0),
        "wrong_candidate_rel_dev": A_wrong / A_true - 1.0,
    }

    ok = (all(abs(v["rel_dev_from_pi_over_3"]) < 1e-4
              for v in res["face_area_numeric"].values())
          and res["negative_control"]["metric_true"] == 0.0
          and res["negative_control"]["metric_wrong_candidate"] > 0.2)
    res["verdict"] = "PASS" if ok else "FAIL"
    print(json.dumps(res, indent=2))
    with open("results/exp01_leaf_area.json", "w") as fh:
        json.dump(res, fh, indent=2)


if __name__ == "__main__":
    main()
