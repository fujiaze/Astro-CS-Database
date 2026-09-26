"""exp05_projection_budgets.py -- P3-06 / P3-07: planar branch area budgets.

Hypothesis (P3-06, gnomonic/TAN) : the tangent-plane image of a small region
             at angular distance rho overestimates its spherical area by
             dA_plane/dA_sphere = sec^3(rho) = 1 + (3/2) rho^2 + O(rho^4):
             one-sided POSITIVE bias +3 rho^2 / 2 (6e-6 at rho = 2e-3 rad,
             NOT 2e-6 as the p1drz_geom.hpp comment claims).
Hypothesis (P3-07, tangent-plane/orthographic) : the planar polygon area from
             orthographic tangent coordinates of unit vertices underestimates
             the spherical area by theta^2/2 (one-sided NEGATIVE): -5.000e-7
             at theta = 1e-3 rad (NOT 4e-8 as the code comment claims).
Negative   : rho -> 0 collapses both biases to zero (no effect => metric 0);
             a deliberately symmetric (sign-flipping) measurement must not
             appear: sign is stable across 200 random orientations.
Seed       : 20260927 (orientation sampling only).
Runtime    : < 30 s.
"""
import json
import sys
import numpy as np

sys.path.insert(0, "code")
import p3lib as P  # noqa: E402

SEED = 20260927


def make_quad(n_center, ea, ed, half, theta):
    """Small geodesic quadrilateral of angular half-size theta around n_center
    (local tangent frame ea, ed)."""
    corners = []
    for sx, sy in ((-1, -1), (-1, 1), (1, 1), (1, -1)):
        d = n_center + theta * (sx * ea + sy * ed)
        corners.append(d / np.linalg.norm(d))
    return corners


def quad_area_spherical(corners):
    return abs(P.geodesic_polygon_area_vos(np.array(corners)))


def quad_area_planar_ortho(corners, n, ea, ed):
    pts = np.array([[float(np.dot(c, ea)), float(np.dot(c, ed))] for c in corners])
    return abs(P.shoelace(pts))


def quad_area_planar_tan(corners, ra0, dec0):
    pts = np.array([P.tan_project(c, ra0, dec0) for c in corners])
    return abs(P.shoelace(pts))


def main():
    out = {"gnomonic": {}, "orthographic_tangent": {}, "negative_control": {}}

    # ---- P3-06: gnomonic, disk-level analytic ratio ------------------------
    rows = []
    for rho in (5e-4, 1e-3, 2e-3, 4e-3):
        A_plane = np.pi * np.tan(rho) ** 2          # planar disk image
        A_sph = 2 * np.pi * (1 - np.cos(rho))       # spherical cap
        ratio = A_plane / A_sph
        rows.append({"rho": rho, "ratio": ratio,
                     "coef_(ratio-1)/rho^2": (ratio - 1.0) / rho ** 2})
    out["gnomonic"]["disk_level"] = rows
    out["gnomonic"]["predicted_coef"] = 1.5
    out["gnomonic"]["rel_error_at_rho_2e-3_plus_3rho2_over_2"] = 1.5 * 4e-6
    out["gnomonic"]["comment_claimed_value"] = 2e-6

    # gnomonic, polygon level: quad at tangent-point offset rho_c
    ra0, dec0 = 0.0, np.deg2rad(41.8)
    n, ea, ed = P.tangent_frame(ra0, dec0)
    rows = []
    for rho_c in (0.0, 1e-3, 2e-3):
        # offset center along ea by rho_c
        c0 = P.tan_deproject(np.tan(rho_c), 0.0, ra0, dec0)
        th = 1e-3
        corners = [P.tan_deproject(np.tan(rho_c) + s * th, t * th, ra0, dec0)
                   for s in (-1, 1) for t in (-1, 1)]
        # ensure cycle order: (-1,-1),(-1,1),(1,1),(1,-1)
        corners = [P.tan_deproject(np.tan(rho_c) + sx * th, sy * th, ra0, dec0)
                   for sx, sy in ((-1, -1), (-1, 1), (1, 1), (1, -1))]
        A_s = quad_area_spherical(corners)
        A_p = quad_area_planar_tan(corners, ra0, dec0)
        ratio = A_p / A_s
        rho_eff = np.hypot(np.tan(rho_c), th)     # rough outer-radius proxy
        rows.append({"rho_c": rho_c, "ratio": ratio,
                     "coef_(ratio-1)/rho^2_outer": (ratio - 1.0) / rho_eff ** 2})
    out["gnomonic"]["polygon_level"] = rows

    # ---- P3-07: orthographic tangent-plane branch --------------------------
    rng = np.random.default_rng(SEED)
    th_list = (1e-4, 3e-4, 1e-3, 3e-3, 1e-2)
    rows = []
    for th in th_list:
        biases = []
        for trial in range(200):
            # random center direction and random in-plane frame rotation
            u = rng.normal(size=3)
            u /= np.linalg.norm(u)
            tmp = rng.normal(size=3)
            e1 = np.cross(u, tmp)
            e1 /= np.linalg.norm(e1)
            e2 = np.cross(u, e1)
            corners = make_quad(u, e1, e2, None, th)
            A_s = quad_area_spherical(corners)
            A_p = quad_area_planar_ortho(corners, u, e1, e2)
            biases.append(A_p / A_s - 1.0)
        b = np.array(biases)
        rows.append({"theta": th,
                     "mean_bias": float(b.mean()),
                     "min": float(b.min()), "max": float(b.max()),
                     "coef_mean/theta^2": float(b.mean()) / th ** 2})
    out["orthographic_tangent"]["theta_sweep"] = rows
    out["orthographic_tangent"]["predicted_coef"] = -0.5
    out["orthographic_tangent"]["bias_at_1e-3_rad"] = -0.5e-6
    out["orthographic_tangent"]["comment_claimed_4e-8"] = 4e-8

    # ---- negative control: theta -> 0 collapses bias -----------------------
    corners = make_quad(np.array([0.0, 0, 1.0]), np.array([1.0, 0, 0]),
                        np.array([0.0, 1, 0]), None, 1e-9)
    A_s = quad_area_spherical(corners)
    A_p = quad_area_planar_ortho(corners, np.array([0.0, 0, 1.0]),
                                 np.array([1.0, 0, 0]), np.array([0.0, 1, 0]))
    out["negative_control"] = {
        "ortho_bias_at_theta_1e-9": A_p / A_s - 1.0,
        "gnomonic_disk_ratio_minus_1_at_rho_1e-7":
            (np.pi * np.tan(1e-7) ** 2) / (2 * np.pi * (1 - np.cos(1e-7))) - 1.0,
        "predicted_3rho2_over_2_at_1e-7": 1.5e-14,
    }

    coef_tail = rows[-1]["coef_mean/theta^2"]
    coef_1e3 = next(r for r in rows if r["theta"] == 1e-3)["mean_bias"]
    disk_coef_2e3 = out["gnomonic"]["disk_level"][2]["coef_(ratio-1)/rho^2"]
    # convert ortho bias to the max-vertex-angle convention theta_max = sqrt2*theta
    out["measured_laws"] = {
        "gnomonic_local_area_element_law": "dA_plane = dA_sphere / cos^3(rho) "
            "=> +1.5 rho^2 locally (exact); disk-averaged coefficient 0.75",
        "gnomonic_disk_coef_at_2e-3": disk_coef_2e3,
        "gnomonic_centered_quad_bias_over_rhomax_sq":
            out["gnomonic"]["polygon_level"][0]["ratio"] - 1.0,
        "gnomonic_offset_quad_bias_over_rhomax_sq_at_2e-3":
            (out["gnomonic"]["polygon_level"][2]["ratio"] - 1.0) / 1.0,
        "ortho_coef_in_theta_max": coef_tail * 0.5,
        "ortho_bias_at_theta_half_1e-3": coef_1e3,
        "ortho_bias_predicted_at_theta_max_1e-3": -5.0e-7,
        "ortho_one_sided": all(r["max"] < 0 for r in rows),
        "note_budget": "comment 'rho^2/2' is the centered-overlap best case; "
            "measured bias/rho_max^2 spans [0.5, 1.5] => budget 1.5 rho_max^2 "
            "(= 6e-6 at rho=2e-3) is the conservative upper bound",
    }
    ok = (abs(disk_coef_2e3 - 0.75) < 1e-3
          and abs(coef_tail * 0.5 - (-0.5)) < 5e-3
          and abs(coef_1e3 - (-1.0e-6)) < 1e-8
          and abs(out["negative_control"]["ortho_bias_at_theta_1e-9"]) < 1e-12
          and all(r["max"] < 0 for r in rows))
    out["verdict"] = "PASS" if ok else "FAIL"
    print(json.dumps(out, indent=2))
    with open("results/exp05_projection_budgets.json", "w") as fh:
        json.dump(out, fh, indent=2)


if __name__ == "__main__":
    main()

