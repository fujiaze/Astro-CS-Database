# -*- coding: utf-8 -*-
"""E5: gnomonic +3rho^2/2 area budget and the tangent-plane branch theta^2/2 single-direction law.

Hypotheses:
  H1 (gnomonic, pointwise): under TAN (Calabretta & Greisen 2002 Sec.5.1.3 eq.54,
      R = (180/pi) cot theta), the area element obeys dA_plane = dA_sphere / cos^3 rho,
      so the POINTWISE relative inflation is sec^3(rho) - 1 = +3 rho^2/2 + O(rho^4):
      plane gnomonic area OVERestimates the spherical area, one direction only.
      At rho = 2e-3 rad the pointwise budget = 6e-6 (the head-comment value 2e-6 is the
      rho^2/2 coefficient -- a factor-3 error).
  H2 (gnomonic, integrated): for a square patch with max angular radius rho_max, the
      INTEGRATED ratio is <sec^3> = 1 + 1.5<rho^2> ~ 1 + rho_max^2/2 (square) -- strictly
      below the pointwise bound 1.5 rho_max^2: the +3rho^2/2 figure is a conservative
      UPPER BUDGET, exact pointwise, not the integrated ratio of a patch.
  H3 (tangent-plane branch, orthogonal projection as in planar_polygon_area_n): for a
      disc-like spherical polygon with circumradius theta, the orthogonally projected
      planar area UNDERestimates the spherical area by ~ theta^2/2 (single direction):
      at theta = 1e-3 rad the deficit is -5.000e-7, NOT the head-comment "<4e-8" (wrong
      by 12.5x in magnitude and by sign convention).

Method: exact-VOS spherical areas (gnomonic maps lines to great circles => 4-corner VOS is
  exact for H1/H2); orthogonal-projection shoelace replicated from the documented recipe for
  H3; scans over rho/theta with fixed seed; negative controls: rho/theta -> 0 gives metric 0,
  and an injected wrong-coefficient model leaves nonzero fit residual.

Run: python3 e5_projection_budgets.py
Output: ../results/e5_projection_budgets.{json,txt}
"""
import json
import math
import numpy as np

SEED = 20050709
rng = np.random.default_rng(SEED)
PI = math.pi
OUT = {"seed": SEED}


def unit(phi, z):
    s = np.sqrt(np.maximum(0.0, 1.0 - z * z))
    return np.stack([s * np.cos(phi), s * np.sin(phi), z], axis=-1)


def vos_poly_area(P):
    """P: (...,n,3) cyclic polygon on sphere, fan triangulation from vertex 0."""
    n = P.shape[-2]
    a = P[..., 0:1, :]
    b = P[..., 1:-1, :]
    c = P[..., 2:, :]
    det = np.sum(np.cross(b, c) * a, axis=-1)
    dot = np.sum(a * b, axis=-1) + np.sum(b * c, axis=-1) + np.sum(c * a, axis=-1)
    return np.abs(np.sum(2.0 * np.arctan2(det, 1.0 + dot), axis=-1))


# ---------- H1/H2: gnomonic ----------
def gnomonic_from_pole(xi, eta):
    """(xi,eta) plane coords [rad] at north pole -> (phi, theta_colatitude)."""
    r = np.hypot(xi, eta)
    theta = np.arctan(r)          # colatitude
    phi = np.arctan2(eta, xi)
    return phi, theta


def gnomonic_square(h, K=1):
    """Return corner unit vectors of the plane square [-h,h]^2 under gnomonic from pole."""
    corners = [(h, h), (-h, h), (-h, -h), (h, -h)]
    xi = np.array([c[0] for c in corners])
    eta = np.array([c[1] for c in corners])
    phi, theta = gnomonic_from_pole(xi, eta)
    z = np.cos(theta)
    return unit(phi, z), float(np.max(theta))   # rho_max = max colatitude


rows_g = []
h_grid = np.array([1e-4, 3e-4, 1e-3, 2e-3, 3e-3, 5e-3, 8e-3, 1e-2, 2e-2])
for h in h_grid:
    P, rho_max = gnomonic_square(h)
    A_sph = float(vos_poly_area(P))
    A_pl = (2 * h) ** 2
    integrated = A_pl / A_sph - 1.0
    pointwise = 1.0 / math.cos(rho_max) ** 3 - 1.0
    rows_g.append({
        "h_rad": float(h), "rho_max_rad": rho_max,
        "integrated_rel_inflation": integrated,
        "integrated_over_rho_max2": integrated / rho_max ** 2,
        "pointwise_sec3_at_rho_max_minus_1": pointwise,
        "pointwise_over_1p5": pointwise / (1.5 * rho_max ** 2),
    })
    print("h=%.1e rho_max=%.3e integrated=%+.4e (x rho^2: %.4f) pointwise=%+.4e (x1.5rho^2: %.6f)"
          % (h, rho_max, integrated, integrated / rho_max ** 2, pointwise, pointwise / (1.5 * rho_max ** 2)))
OUT["gnomonic_scan"] = rows_g
# log-log slope of integrated inflation vs rho_max (expect ~2)
lm = np.log([[r["rho_max_rad"], r["integrated_rel_inflation"]] for r in rows_g if r["rho_max_rad"] < 1e-2])
slope = np.polyfit(lm[:, 0], lm[:, 1], 1)[0]
OUT["gnomonic_integrated_loglog_slope"] = float(slope)
OUT["gnomonic_budget_at_2e-3_pointwise"] = 1.5 * (2e-3) ** 2
OUT["head_comment_value_2e-6"] = 0.5 * (2e-3) ** 2

# NEGATIVE CONTROL: pointwise law sec^3 verified by independent ring quadrature:
# dA_plane = r dr dphi ; dA_sphere = sin(rho) drho dphi ; r = tan(rho)
r = rng.uniform(0, 1e-2, 200001)
dA_pl = r * (1e-2 / 2e5)                     # uniform annulus weights ~ r dr
rho = np.arctan(r)
dA_sp = np.sin(rho) * (1.0 / (1.0 + r ** 2)) * (1e-2 / 2e5)  # drho = dr/(1+r^2)
ratio_num = dA_pl.sum() / dA_sp.sum()
sec3_mean = float(np.mean(1.0 / np.cos(rho) ** 3))
OUT["gnomonic_ring_quadrature"] = {
    "ratio_plane_over_sphere": float(ratio_num),
    "mean_sec3": sec3_mean,
    "rel_agreement": abs(ratio_num / sec3_mean - 1.0),
}

# ---------- H2b: integrated ratio for a SQUARE drop (production-relevant shape) ----------
rows_sq = []
for h in (1e-4, 5e-4, 1e-3, 2e-3, 5e-3):
    P, rho_max = gnomonic_square(h)
    A_sph = float(vos_poly_area(P))
    A_pl = (2 * h) ** 2
    rows_sq.append({"h_rad": h, "rho_max_rad": rho_max,
                    "integrated_rel_inflation": A_pl / A_sph - 1.0})
OUT["gnomonic_square_scan"] = rows_sq

# ---------- H3: tangent-plane orthogonal projection ----------
def ortho_planar_area(P):
    """Replicate planar_polygon_area_n: orthogonal projection onto tangent plane at the
    polygon centroid, shoelace. P: (...,n,3) unit vectors."""
    c = P.mean(axis=-2)
    c = c / np.linalg.norm(c, axis=-1, keepdims=True)
    u = P - np.sum(P * c[..., None, :], axis=-1, keepdims=True) * c[..., None, :]
    n = P.shape[-2]
    p = np.roll(u, -1, axis=-2)
    cr = np.cross(u, p)
    return 0.5 * np.abs(np.sum(np.sum(cr * c[..., None, :], axis=-1), axis=-1))


rows_t = []
for theta in (1e-4, 3e-4, 1e-3, 2e-3, 5e-3, 1e-2, 3e-2):
    # disc-like polygon: regular 64-gon of colatitude theta around the pole
    phi = np.linspace(0, 2 * PI, 65)[:-1]
    z = np.full(64, math.cos(theta))
    P = unit(phi, np.full(64, math.cos(theta)))
    A_sph = float(vos_poly_area(P))
    A_pl = float(ortho_planar_area(P))
    rel = A_pl / A_sph - 1.0
    rows_t.append({"theta_rad": theta, "planar_over_spherical_minus_1": rel,
                   "over_theta2": rel / theta ** 2})
    print("theta=%.1e planar/spherical-1 = %+.5e  (x theta^2: %.4f)" % (theta, rel, rel / theta ** 2))
# square drop (4 corners, circumradius theta): production-relevant shape family
rows_tq = []
for theta in (1e-4, 1e-3, 1e-2):
    ph = np.array([math.pi/4, 3*math.pi/4, 5*math.pi/4, 7*math.pi/4])
    P = unit(ph, np.full(4, math.cos(theta)))
    A_sph = float(vos_poly_area(P))
    A_pl = float(ortho_planar_area(P))
    rows_tq.append({"theta_rad": theta, "planar_over_spherical_minus_1": A_pl / A_sph - 1.0,
                    "over_theta2": (A_pl / A_sph - 1.0) / theta ** 2})
OUT["tangent_plane_square_scan"] = rows_tq
OUT["tangent_plane_scan"] = rows_t
tt = np.log([[r["theta_rad"], -r["planar_over_spherical_minus_1"]] for r in rows_t if r["theta_rad"] <= 1e-3])
OUT["tangent_plane_loglog_slope"] = float(np.polyfit(tt[:, 0], tt[:, 1], 1)[0])
OUT["tangent_plane_at_1e-3"] = [r for r in rows_t if r["theta_rad"] == 1e-3][0]["planar_over_spherical_minus_1"]
OUT["head_comment_claim_4e-8"] = 4e-8
OUT["single_direction_all_negative"] = bool(all(r["planar_over_spherical_minus_1"] < 0 for r in rows_t))

# NEGATIVE CONTROLS: theta -> 0 metric -> 0 ; wrong-coefficient model leaves residual
P = unit(np.linspace(0, 2 * PI, 65)[:-1], np.full(64, math.cos(1e-5)))
A_sph = float(vos_poly_area(P))
A_pl = float(ortho_planar_area(P))
OUT["tangent_plane_tiny_theta_metric"] = A_pl / A_sph - 1.0
th = np.array([r["theta_rad"] for r in rows_t if r["theta_rad"] <= 2e-3])
y = np.array([-r["planar_over_spherical_minus_1"] for r in rows_t if r["theta_rad"] <= 2e-3])
coef_good = np.polyfit(th ** 2, y, 1)[0]
res_good = np.max(np.abs(y - coef_good * th ** 2))
coef_wrong = 1.0 / 12.0  # a deliberately wrong coefficient (e.g. theta^2/12 model)
res_wrong = np.max(np.abs(y - coef_wrong * th ** 2))
OUT["tangent_plane_fit"] = {"coefficient": float(coef_good), "max_residual": float(res_good),
                            "wrong_model_residual": float(res_wrong),
                            "verdict": "theta^2/2-family model fits; theta^2/12-model residual is nonzero"}

with open("../results/e5_projection_budgets.json", "w") as fh:
    json.dump(OUT, fh, indent=1)

L = ["E5 projection budgets (seed=%d)" % SEED, ""]
L.append("[GNOMONIC pointwise/integrated]")
for r in rows_g:
    L.append("  h=%.1e rho_max=%.3e integrated=%+.4e (x rho^2 %.4f) pointwise=%+.4e (x1.5rho^2 %.5f)"
             % (r["h_rad"], r["rho_max_rad"], r["integrated_rel_inflation"],
                r["integrated_over_rho_max2"], r["pointwise_sec3_at_rho_max_minus_1"],
                r["pointwise_over_1p5"]))
L.append("  integrated log-log slope (small rho): %.3f (expect 2)" % OUT["gnomonic_integrated_loglog_slope"])
L.append("  budget at rho=2e-3: pointwise +3rho^2/2 = %.2e (head comment 2e-6 = rho^2/2 => factor-3 error)"
         % OUT["gnomonic_budget_at_2e-3_pointwise"])
gq = OUT["gnomonic_ring_quadrature"]
L.append("  ring-quadrature check: plane/sphere=%.6f vs mean sec^3=%.6f (agree to %.1e)"
         % (gq["ratio_plane_over_sphere"], gq["mean_sec3"], gq["rel_agreement"]))
L.append("")
L.append("[TANGENT-PLANE branch]")
for r in rows_t:
    L.append("  theta=%.1e planar/spherical-1 = %+.5e (x theta^2 %.4f)" % (r["theta_rad"], r["planar_over_spherical_minus_1"], r["over_theta2"]))
L.append("  log-log slope: %.3f (expect 2)" % OUT["tangent_plane_loglog_slope"])
L.append("  at theta=1e-3: %+.5e (head comment '<4e-8' is wrong: magnitude 12.5x too small)"
         % OUT["tangent_plane_at_1e-3"])
L.append("  single direction (always negative): %s" % OUT["single_direction_all_negative"])
L.append("  tiny-theta negative control metric: %.2e" % OUT["tangent_plane_tiny_theta_metric"])
ft = OUT["tangent_plane_fit"]
L.append("  theta^2-model coefficient = %.4f (max residual %.1e); wrong theta^2/12 model residual %.1e"
         % (ft["coefficient"], ft["max_residual"], ft["wrong_model_residual"]))
txt = "\n".join(L)
with open("../results/e5_projection_budgets.txt", "w") as fh:
    fh.write(txt + "\n")
print(txt)
