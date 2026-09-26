# -*- coding: utf-8 -*-
"""E1: HEALPix equal-area leaf area A_leaf = pi/(3 N^2) and the nside scale constant.

Hypothesis:
  H1: the equal-area chart of every HEALPix base face has constant Jacobian |J| = pi/3
      (equatorial branch and polar branch alike), so one leaf (a 1/N x 1/N square in the
      chart) has exact area A_leaf = pi/(3 N^2) = 4*pi/(12 N^2) sr.
  H2: HEALPIX_SCALE_PER_NSIDE_ARCSEC = sqrt(pi/3)*(180/pi)*3600 = 211076.28514206142 arcsec
      is the exact evaluation of sqrt(A_leaf_unit); the handwritten value 211034.6 differs
      by -1.97e-4 relative and shifts the auto-nside decision inside every octave window.

Method (pure numpy, no repo imports, deterministic -- no RNG needed, seed fixed anyway):
  1. Implement the equal-area chart (Gorski et al. 2005 Sec.5 geometry; repo doc 02 Sec.1.1):
     equatorial branch  z = (2/3)(u+v-1),  phi = (pi/4)(u-v+2(f-4))
     polar branch       s = 2-u-v in (0,1], z = 1-s^2/3, phi = (pi/2)f + pi(1-v)/(2s)
  2. Finite-difference |J| on dense grids of both branches; assert max||J|-pi/3|/ (pi/3).
  3. True leaf area via shoelace in the (phi,z) plane (exact area element dA = dphi dz)
     for N in {16, 64, 256} on the equatorial faces (full square, no folding); compare to
     pi/(3N^2) and to 4*pi/(12 N^2).
  4. Chord-model area via Van Oosterom & Strackee on the 4 corner unit vectors
     (quantifies the model deficit, consumed by E2).
  5. NEGATIVE CONTROL (truth-no-effect => metric zero): the metric
        M = max |A_shoelace / (pi/(3N^2)) - 1|
     must vanish (float noise) for the correct chart; a deliberately perturbed chart with
     Jacobian |J| = (pi/3)*(1+eps), eps=5e-4 (z scaled by (1+eps/2) both directions
     symmetrically so dphi dz picks up (1+eps)) must give M ~= eps != 0.
  6. Scale constant: evaluate C = sqrt(pi/3)*(180/pi)*3600 in double; compare 211034.6;
     demonstrate the auto-nside decision divergence at finest_arcsec = 105527.7 and the
     full octave-window structure; evaluate pixel_resolution_arcsec(256).
  7. P2->P3->P4 control-point assignment interface: inverse equatorial chart
     (phi,z) -> (f,u,v) -> leaf id; round-trip check on a dense grid; leaf-id bijectivity.

Seed: 20050709 (Gorski et al. 2005 submission date); results deterministic regardless.
Run:  python3 e1_leaf_area_and_scale.py
Output: ../results/e1_leaf_area_and_scale.{json,txt}
"""
import json
import math
import numpy as np

SEED = 20050709
np.random.seed(SEED)

PI = math.pi
OUT = {}


def chart_equatorial(f, u, v):
    """Equatorial branch of the equal-area chart. f in 0..11, (u,v) in [0,1]^2."""
    z = (2.0 / 3.0) * (u + v - 1.0)
    phi = (PI / 4.0) * (u - v + 2.0 * (f - 4))
    return z, phi


def chart_polar(f, u, v, south=False):
    """Polar branch. Valid where s = 2-u-v in (0,1] (north faces, u+v>1)."""
    s = 2.0 - u - v
    z = 1.0 - s * s / 3.0
    phi = (PI / 2.0) * f + PI * (1.0 - v) / (2.0 * s)
    if south:
        z = -z
        phi = (PI / 2.0) * f + PI * v / (2.0 * s)
    return z, phi


def jac_num(func, u, v, h=1e-6):
    """Finite-difference Jacobian determinant |d(z,phi)/d(u,v)|."""
    z1, p1 = func(u + h, v)
    z2, p2 = func(u - h, v)
    z3, p3 = func(u, v + h)
    z4, p4 = func(u, v - h)
    dzu = (z1 - z2) / (2 * h)
    dphu = (p1 - p2) / (2 * h)
    dzv = (z3 - z4) / (2 * h)
    dphv = (p3 - p4) / (2 * h)
    return abs(dzu * dphv - dzv * dphu)


def shoelace(phi, z):
    """Shoelace in (phi,z) plane; exact spherical area for chart-straight edges."""
    phi = np.asarray(phi, float)
    z = np.asarray(z, float)
    return 0.5 * abs(np.dot(phi, np.roll(z, -1)) - np.dot(np.roll(phi, -1), z))


def unit_from_chart(phi, z_sin_dec):
    """(phi, z=sin(dec)) chart coords -> unit vector: n = [sqrt(1-z^2) cos phi, sqrt(1-z^2) sin phi, z]."""
    s = math.sqrt(max(0.0, 1.0 - z_sin_dec * z_sin_dec))
    return np.array([s * math.cos(phi), s * math.sin(phi), z_sin_dec])


def vos_polygon_area(pts):
    """Van Oosterom & Strackee fan-triangulated solid angle of a cyclic polygon (unit vecs)."""
    n = len(pts)
    total = 0.0
    for i in range(1, n - 1):
        a, b, c = pts[0], pts[i], pts[i + 1]
        det = np.dot(a, np.cross(b, c))
        s = np.dot(a, b) + np.dot(b, c) + np.dot(c, a)
        total += 2.0 * math.atan2(det, 1.0 + s)
    return abs(total)


# ---------------- 1. Jacobian identity ----------------
grid = np.linspace(0.01, 0.99, 33)
J_eq = np.array([jac_num(lambda u, v: chart_equatorial(4, u, v), u, v) for u in grid for v in grid])
# polar branch valid region: u+v>1, s in (0,1]
ug, vg = np.meshgrid(grid, grid)
m = (ug + vg > 1.02) & (ug + vg < 1.98)
J_pol = np.array([jac_num(lambda u, v: chart_polar(0, u, v), u, v) for u, v in zip(ug[m], vg[m])])
res_j = {
    "equatorial_max_rel_dev": float(np.max(np.abs(J_eq / (PI / 3) - 1))),
    "polar_max_rel_dev": float(np.max(np.abs(J_pol / (PI / 3) - 1))),
    "n_equatorial_points": int(J_eq.size),
    "n_polar_points": int(J_eq.size and J_pol.size),
}
OUT["jacobian"] = res_j

# ---------------- 2-5. leaf areas, equatorial faces ----------------
leaf_rows = []
for N in (16, 64, 256):
    A_analytic = PI / (3.0 * N * N)
    A_alt = 4.0 * PI / (12.0 * N * N)
    rng = np.random.default_rng(SEED + N)  # reproducible leaf sample
    faces = rng.choice(4, size=4000) + 4   # equatorial faces 4..7
    iu = rng.integers(0, N, size=4000)
    iv = rng.integers(0, N, size=4000)
    dev_true, dev_alt, dev_chord = [], [], []
    for f, i, j in zip(faces, iu, iv):
        u0, v0 = i / N, j / N
        corners_uv = [(u0, v0), (u0 + 1 / N, v0), (u0 + 1 / N, v0 + 1 / N), (u0, v0 + 1 / N)]
        zs, ps = zip(*[chart_equatorial(f, u, v) for u, v in corners_uv])
        a_true = shoelace(ps, zs)               # exact (chart-straight edges)
        pts3 = [unit_from_chart(p, z) for p, z in zip(ps, zs)]
        a_chord = vos_polygon_area(pts3)        # great-circle chord model
        dev_true.append(a_true / A_analytic - 1.0)
        dev_alt.append(a_true / A_alt - 1.0)
        dev_chord.append(a_chord / A_analytic - 1.0)
    leaf_rows.append({
        "nside": N,
        "A_analytic": A_analytic,
        "max|A_true/A_analytic-1|": float(np.max(np.abs(dev_true))),
        "max|A_true/(4pi/12N^2)-1|": float(np.max(np.abs(dev_alt))),
        "chord_model_max_rel_dev": float(np.max(np.abs(dev_chord))),
        "n_leaves": 4000,
    })
OUT["leaf_area"] = leaf_rows

# NEGATIVE CONTROL: perturbed chart (|J| = (pi/3)(1+eps)) must break the metric
eps = 5e-4
def chart_perturbed(u, v):
    z, phi = chart_equatorial(4, u, v)
    return z * (1.0 + eps / 2.0), phi  # dz scaling => |J| scales by (1+eps/2)... use symmetric
# scale z by (1+eps): dphi dz -> (1+eps) dphi dz
def chart_perturbed2(u, v):
    z, phi = chart_equatorial(4, u, v)
    return z * (1.0 + eps), phi

Jp = np.array([jac_num(chart_perturbed2, u, v) for u in grid[:8] for v in grid[:8]])
N = 64
A_analytic = PI / (3.0 * N * N)
devs = []
for i in range(16):
    for j in range(16):
        u0, v0 = i / N, j / N
        corners_uv = [(u0, v0), (u0 + 1 / N, v0), (u0 + 1 / N, v0 + 1 / N), (u0, v0 + 1 / N)]
        zs, ps = zip(*[chart_perturbed2(u, v) for u, v in corners_uv])
        devs.append(shoelace(ps, zs) / A_analytic - 1.0)
OUT["negative_control"] = {
    "eps_injected": eps,
    "jacobian_rel_shift_measured": float(np.max(np.abs(Jp / (PI / 3) - 1))),
    "metric_correct_chart": leaf_rows[1]["max|A_true/A_analytic-1|"],
    "metric_perturbed_chart": float(np.max(np.abs(devs))),
    "verdict": "metric==0 (float noise) for truth; metric~=eps != 0 for perturbed chart"
               if np.max(np.abs(devs)) > 100 * leaf_rows[1]["max|A_true/A_analytic-1|"] else "FAILED"
}

# ---------------- 6. scale constant ----------------
C = math.sqrt(PI / 3.0) * (180.0 / PI) * 3600.0
C_hand = 211034.6
finest = 105527.7
nside_formula = 2 ** math.ceil(math.log2(max(1.0, math.ceil(C / finest))))
nside_hand = 2 ** math.ceil(math.log2(max(1.0, math.ceil(C_hand / finest))))
oct_windows = []
for k in range(4, 21):
    lo, hi = C_hand / 2 ** k, C / 2 ** k
    oct_windows.append({"k": k, "window_arcsec": [lo, hi], "rel_width": (hi - lo) / hi})
OUT["scale_constant"] = {
    "C_evaluated": C,
    "C_reference_16_digits": 211076.28514206142,
    "abs_diff": abs(C - 211076.28514206142),
    "handwritten_value": C_hand,
    "rel_diff_hand": (C_hand - C) / C,
    "demo_finest_arcsec": finest,
    "nside_from_formula": nside_formula,
    "nside_from_handwritten": nside_hand,
    "nside_factor_lost": nside_formula // nside_hand if nside_formula > nside_hand else 1,
    "pixel_resolution_arcsec_256": C / 256.0,
    "octave_windows_example": oct_windows[:3],
}

# ---------------- 7. control-point assignment interface (P2->P3->P4) ----------------
# Chart geometry fact (verified here): the equatorial face chart square maps to a DIAMOND
# in (phi,z): vertices (0,-2/3),(pi/4,0),(0,2/3),(-pi/4,0), area (1/2)(pi/2)(4/3) = pi/3.
# The 4 equatorial diamonds + the 8 polar-face triangles tile the band exactly.
def inverse_face4_diamond(z, phi):
    """(z,phi) inside face 4's diamond -> (u,v); None outside the diamond."""
    if abs(phi) > (PI / 4.0) * (1.0 - 1.5 * abs(z)) + 1e-15:
        return None
    s12 = 1.0 + 1.5 * z                  # u+v
    d12 = 4.0 * phi / PI                 # u-v
    u = 0.5 * (s12 + d12)
    v = 0.5 * (s12 - d12)
    return u, v


def leaf_id(u, v, N):
    return (int(v * N), int(u * N))  # face-local (row v, col u); NESTED bit order left to production


rt_err = 0.0
ids = set()
n_in = 0
Ng = 128
for z in np.linspace(-0.659, 0.659, 61):
    for phi in np.linspace(-0.78, 0.78, 121):
        uv = inverse_face4_diamond(z, phi)
        if uv is None:
            continue
        u, v = uv
        if not (0.0 <= u <= 1.0 and 0.0 <= v <= 1.0):
            continue
        n_in += 1
        zz, pp = chart_equatorial(4, u, v)
        rt_err = max(rt_err, abs(zz - z), abs(pp - phi))
        ids.add(leaf_id(u, v, Ng))
OUT["control_point_assignment"] = {
    "roundtrip_max_abs_err": float(rt_err),
    "sample_points_in_face4_diamond": n_in,
    "distinct_leaf_ids_128grid": len(ids),
    "interface_note": "control point (phi,z) -> (u,v) -> leaf id: P2 frame-domain absolute-SNR value "
                      "rides this chart assignment onto the sphere; P4 consumes (leaf_id, SNR_value) "
                      "pairs. Verified on the equatorial branch incl. diamond membership; full-sky "
                      "polar caps and NESTED bit interleaving stay with the production ang2pix.",
    "honest_boundary": "polar-branch inverse and NESTED ordering not exercised in this standalone check"
}

with open("../results/e1_leaf_area_and_scale.json", "w") as fh:
    json.dump(OUT, fh, indent=1)

lines = ["E1 leaf area + nside scale constant  (seed=%d, deterministic)" % SEED, ""]
lines.append("[Jacobian identity |J|=pi/3]")
lines.append("  equatorial branch: max rel dev = %.3e over %d pts" % (res_j["equatorial_max_rel_dev"], res_j["n_equatorial_points"]))
lines.append("  polar branch:      max rel dev = %.3e over %d pts" % (res_j["polar_max_rel_dev"], res_j["n_polar_points"]))
lines.append("")
lines.append("[Leaf area A_leaf = pi/(3N^2)]")
lines.append("  nside   max|A_true/A_an -1|   max|A_true/(4pi/12N^2)-1|   chord_model_max_dev")
for r in leaf_rows:
    lines.append("  %5d   %.3e              %.3e                    %.3e"
                 % (r["nside"], r["max|A_true/A_analytic-1|"], r["max|A_true/(4pi/12N^2)-1|"], r["chord_model_max_rel_dev"]))
nc = OUT["negative_control"]
lines.append("")
lines.append("[NEGATIVE CONTROL] eps=%.1e injected into chart Jacobian" % nc["eps_injected"])
lines.append("  correct chart metric = %.3e ; perturbed chart metric = %.3e ; verdict: %s"
             % (nc["metric_correct_chart"], nc["metric_perturbed_chart"], nc["verdict"]))
sc = OUT["scale_constant"]
lines.append("")
lines.append("[Scale constant]")
lines.append("  C = sqrt(pi/3)*(180/pi)*3600 = %.14f (ref 211076.28514206142, |diff|=%.2e)" % (sc["C_evaluated"], sc["abs_diff"]))
lines.append("  handwritten 211034.6 rel diff = %+.4e" % sc["rel_diff_hand"])
lines.append("  demo finest=%.1f arcsec: nside formula=%d vs handwritten=%d (factor lost=%d)"
             % (sc["demo_finest_arcsec"], sc["nside_from_formula"], sc["nside_from_handwritten"], sc["nside_factor_lost"]))
lines.append("  pixel_resolution_arcsec(256) = %.6f arcsec" % sc["pixel_resolution_arcsec_256"])
cp = OUT["control_point_assignment"]
lines.append("")
lines.append("[Control-point assignment (P2->P3->P4 interface)]")
lines.append("  roundtrip max err = %.3e over %d in-diamond points ; %d distinct leaf ids on 128-grid"
             % (cp["roundtrip_max_abs_err"], cp["sample_points_in_face4_diamond"], cp["distinct_leaf_ids_128grid"]))
with open("../results/e1_leaf_area_and_scale.txt", "w") as fh:
    fh.write("\n".join(lines) + "\n")
print("\n".join(lines))
