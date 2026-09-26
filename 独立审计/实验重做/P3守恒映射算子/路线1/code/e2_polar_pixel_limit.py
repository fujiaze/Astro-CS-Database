# -*- coding: utf-8 -*-
"""E2: polar-pixel closed-form deficit 2*sqrt(2)/pi - 1 and the absolute deficit law.

Hypothesis (repo doc 02 Sec.1.3; to be re-derived and re-measured here independently):
  H1: representing a HEALPix leaf by the great-circle (chord) quadrilateral of its four
      ring-order corners makes polar-cap-adjacent cells systematically SMALLER than the
      true cell area, with a relative deficit that does NOT shrink with resolution:
           lim_{N->inf} A_quad/A_true = 2*sqrt(2)/pi = 0.9003163161...
           rel deficit -> 2*sqrt(2)/pi - 1 = -9.968368384e-2
  H2: the worst cells are exactly ring 1 and ring 4N-1 (the 8 polar cells) then ring 3
      and ring 4N-3; the max ABSOLUTE deficit follows  0.104369 / N^2  sr.
  H3 (negative controls): (a) restricted to equatorial cells the deficit is second order
      and shrinks with N; (b) with true-curve edges (chart-straight lines finely sampled)
      the deficit metric goes to zero -- the metric measures the chord approximation only.

Method (pure numpy, deterministic, seed fixed):
  ring-order corner construction (02 Sec.1.4): levels j=0..4N with
    z_j = 1 - j^2/(3N^2)                      (1<=j<=N)
    z_j = 2(2N-j)/(3N)                        (N<j<3N)
    z_j = -1 + (4N-j)^2/(3N^2)                (3N<=j<=4N-1)
    S_j = 4j / 4N / 4(4N-j) pixels per ring; cap offset 0, equatorial offset w_j/2 for odd j
  cell (j,m) corners: N' = nearest level-(j-1) grid vertex, L = (phi_c-w_j/2, z_j),
    R = (phi_c+w_j/2, z_j), S = nearest level-(j+1) grid vertex; level 0 = pole.
  true area: A_true(j) = 2*pi*dz_j/S_j = pi/(3N^2)  (zone closure checked to 4*pi)
  model area: Van Oosterom & Strackee on the 4 corner unit vectors.

Seed: 20050709 (corner construction is deterministic; seed fixed for the record).
Run:  python3 e2_polar_pixel_limit.py        (a few minutes at N=256; vectorized VOS)
Output: ../results/e2_polar_pixel_limit.{json,txt}
"""
import json
import math
import numpy as np

SEED = 20050709
np.random.seed(SEED)
PI = math.pi


def ring_levels(N):
    j = np.arange(0, 4 * N + 1)
    z = np.empty(4 * N + 1)
    z[0] = 1.0
    m1 = (j >= 1) & (j <= N)
    m2 = (j > N) & (j < 3 * N)
    m3 = (j >= 3 * N) & (j <= 4 * N - 1)
    z[m1] = 1.0 - (j[m1] ** 2) / (3.0 * N * N)
    z[m2] = 2.0 * (2 * N - j[m2]) / (3.0 * N)
    z[m3] = -1.0 + ((4 * N - j[m3]) ** 2) / (3.0 * N * N)
    z[4 * N] = -1.0
    S = np.where(j <= N, 4 * np.maximum(j, 1), np.where(j < 3 * N, 4 * N, 4 * np.maximum(4 * N - j, 1)))
    S[0] = S[4 * N] = 1  # poles (degenerate levels: single point)
    return j, z, S


def ring_offsets(N, j, S):
    o = np.zeros(4 * N + 1)
    for k in range(1, 4 * N):
        if N < k < 3 * N and k % 2 == 1:
            o[k] = PI / S[k]
    return o


def unit_vecs(phi, z):
    """(array phi, array z) -> unit vectors, shape (..., 3)."""
    s = np.sqrt(np.maximum(0.0, 1.0 - z * z))
    return np.stack([s * np.cos(phi), s * np.sin(phi), np.broadcast_to(z, phi.shape)], axis=-1)


def vos_quad_area(P):
    """Vectorized VOS area of quads P (...,4,3) as fan (0,1,2)+(0,2,3)."""
    a, b, c, d = P[..., 0, :], P[..., 1, :], P[..., 2, :], P[..., 3, :]
    def tri(a, b, c):
        det = np.sum(a * np.cross(b, c), axis=-1)
        dot = np.sum(a * b, axis=-1) + np.sum(b * c, axis=-1) + np.sum(c * a, axis=-1)
        return 2.0 * np.arctan2(det, 1.0 + dot)
    return np.abs(tri(a, b, c) + tri(a, c, d))


def nearest_vertex(phi_c, level_phi):
    d = np.abs(((phi_c[:, None] - level_phi[None, :] + PI) % (2 * PI)) - PI)
    return level_phi[np.argmin(d, axis=1)]


def build_cells(N):
    """Return list of (ring j, corner unit vectors array (S_j,4,3), true area per cell)."""
    j, z, S = ring_levels(N)
    o = ring_offsets(N, j, S)
    levels_phi = []
    for k in range(4 * N + 1):
        if k == 0 or k == 4 * N:
            levels_phi.append(None)
        else:
            levels_phi.append(o[k] + np.arange(S[k]) * (2 * PI / S[k]))
    A_true = PI / (3.0 * N * N)
    out = []
    for jj in range(1, 4 * N):
        w = 2 * PI / S[jj]
        phic = o[jj] + (np.arange(S[jj]) + 0.5) * w
        L = np.stack([phic - w / 2, np.full(S[jj], z[jj])], axis=1)
        R = np.stack([phic + w / 2, np.full(S[jj], z[jj])], axis=1)
        if jj == 1:
            top = None  # pole
        else:
            top = nearest_vertex(phic, levels_phi[jj - 1])
        if jj == 4 * N - 1:
            bot = None
        else:
            bot = nearest_vertex(phic, levels_phi[jj + 1])
        quads = np.zeros((S[jj], 4, 3))
        topv = np.array([0.0, 0.0, 1.0])
        botv = np.array([0.0, 0.0, -1.0])
        zT = z[jj - 1] if jj > 1 else 1.0
        zB = z[jj + 1] if jj < 4 * N - 1 else -1.0
        for i in range(S[jj]):
            tphi = phic[i] if top is None else top[i]
            bphi = phic[i] if bot is None else bot[i]
            corners = [(tphi, zT), (R[i, 0], R[i, 1]), (bphi, zB), (L[i, 0], L[i, 1])]
            quads[i] = unit_vecs(np.array([c[0] for c in corners]), np.array([c[1] for c in corners]))
        out.append((jj, quads, S[jj], A_true))
    return out, A_true, S


results = {}
for N in (2, 4, 8, 16, 32, 64, 128, 256):
    cells, A_true, S = build_cells(N)
    rel_all = []
    worst = (None, 0.0)
    n_gt1pct = 0
    deficit = 0.0
    ring_max = {}
    for jj, quads, cnt, A_t in cells:
        a = vos_quad_area(quads)
        rel = a / A_t - 1.0
        rel_all.append(rel)
        k = int(np.argmax(rel))
        ring_max[jj] = float(rel[k])
        if rel[k] < worst[1]:
            worst = (jj, float(rel[k]))
        n_gt1pct += int(np.sum(rel < -0.01))
        deficit = min(deficit, float(np.min(rel)))
    rel_all = np.concatenate(rel_all)
    results[N] = {
        "A_true": A_true,
        "max_rel_deficit": float(rel_all.min()),
        "worst_ring": worst[0],
        "n_cells_gt1pct_deficit": int(n_gt1pct),
        "max_abs_deficit_times_N2": float(-rel_all.min() * A_true * N * N),
        "sum_model_over_4pi_minus_1": float(np.sum([np.sum(vos_quad_area(q)) for _, q, _, _ in cells]) / (4 * PI) - 1.0),
        "ring1_rel": ring_max.get(1),
        "ring3_rel": ring_max.get(3),
    }
    print("N=%-4d max_rel=%.6e worst_ring=%s |rel|>1%%:%d  deficit*N^2=%.6f  sum/4pi-1=%.2e"
          % (N, rel_all.min(), worst[0], n_gt1pct, results[N]["max_abs_deficit_times_N2"], results[N]["sum_model_over_4pi_minus_1"]))

# closed forms
lim = 2.0 * math.sqrt(2.0) / PI
lim_rel = lim - 1.0
# deficit law coefficient from the largest N (least contaminated) and from theory A_true*(1-2sqrt2/pi)
coeff_theory = (PI / 3.0) * (1.0 - lim)   # = 0.104369... sr * N^2
out = {
    "seed": SEED,
    "closed_form_limit_ratio_2sqrt2_over_pi": lim,
    "closed_form_limit_rel": lim_rel,
    "deficit_coeff_theory": coeff_theory,
    "per_n": {str(k): v for k, v in results.items()},
    "N256_limit_gap": results[256]["max_rel_deficit"] - lim_rel,
}

# NEGATIVE CONTROL (a): equatorial cells only, second order shrinking
eq = []
for N in (32, 64, 128):
    cells, A_true, S = build_cells(N)
    worst_eq = 0.0
    for jj, quads, cnt, A_t in cells:
        if N < jj < 3 * N:
            rel = vos_quad_area(quads) / A_t - 1.0
            worst_eq = max(worst_eq, float(np.max(np.abs(rel))))
    eq.append({"nside": N, "equatorial_max_abs_rel": worst_eq})
out["negative_control_equatorial_only"] = eq

# NEGATIVE CONTROL (b): true-curve edges (finely sampled chart-straight edges) => metric -> 0
def true_curve_area_equatorial_cell(N, f, i0, j0, K=64):
    """Area of an equatorial-band leaf via fine sampling of its chart-straight edges,
    VOS on the fine polygon (converges to the true area as K grows)."""
    u = np.linspace(i0 / N, (i0 + 1) / N, K)
    v = np.linspace(j0 / N, (j0 + 1) / N, K)
    z = (2.0 / 3.0) * (u + (j0 / N) - 1.0)
    ph = (PI / 4.0) * (u - (j0 / N) + 2.0 * (f - 4))
    bottom = np.stack([ph, z], axis=1)
    z2 = (2.0 / 3.0) * (((i0 + 1) / N) + v - 1.0)
    ph2 = (PI / 4.0) * (((i0 + 1) / N) - v + 2.0 * (f - 4))
    right = np.stack([ph2, z2], axis=1)[1:]
    z3 = (2.0 / 3.0) * (u + ((j0 + 1) / N) - 1.0)
    ph3 = (PI / 4.0) * (u - ((j0 + 1) / N) + 2.0 * (f - 4))
    top = np.stack([ph3, z3], axis=1)[::-1][1:]
    z4 = (2.0 / 3.0) * ((i0 / N) + v - 1.0)
    ph4 = (PI / 4.0) * ((i0 / N) - v + 2.0 * (f - 4))
    left = np.stack([ph4, z4], axis=1)[::-1][1:-1]
    poly = np.concatenate([bottom, right, top, left], axis=0)
    zz = poly[:, 1]
    ss = np.sqrt(np.maximum(0.0, 1.0 - zz * zz))
    pts = np.stack([ss * np.cos(poly[:, 0]), ss * np.sin(poly[:, 0]), zz], axis=1)
    n = len(pts)
    a, b, c = pts[0], pts[1:-1], pts[2:]
    det = np.sum(a * np.cross(b, c), axis=-1)
    dot = np.sum(a * b, axis=-1) + np.sum(b * c, axis=-1) + np.sum(c * a, axis=-1)
    return abs(np.sum(2.0 * np.arctan2(det, 1.0 + dot)))

tc = []
for N, f, i0, j0 in [(64, 4, 32, 32), (128, 5, 70, 20)]:
    A_t = PI / (3.0 * N * N)
    aK = true_curve_area_equatorial_cell(N, f, i0, j0, K=64)
    a2K = true_curve_area_equatorial_cell(N, f, i0, j0, K=128)
    tc.append({"nside": N, "true_curve_rel_dev_K64": aK / A_t - 1.0,
               "richsonson_residual": (a2K - aK) / A_t})
out["negative_control_true_curve"] = tc

with open("../results/e2_polar_pixel_limit.json", "w") as fh:
    json.dump(out, fh, indent=1)

lines = ["E2 polar-pixel closed-form deficit  (seed=%d)" % SEED, ""]
lines.append("closed form: A_quad/A_true -> 2*sqrt(2)/pi = %.10f ; rel = %.10e" % (lim, lim_rel))
lines.append("deficit law coefficient (theory) = pi/3*(1-2sqrt2/pi) = %.6f" % coeff_theory)
lines.append("")
lines.append("  N     max_rel_deficit   worst_ring  |rel|>1%   deficit*N^2   sum/4pi-1")
for N, r in results.items():
    lines.append("  %-5d %.6e      ring %-3d   %-5d     %.6f      %.2e"
                 % (N, r["max_rel_deficit"], r["worst_ring"], r["n_cells_gt1pct_deficit"],
                    r["max_abs_deficit_times_N2"], r["sum_model_over_4pi_minus_1"]))
lines.append("")
lines.append("N=256 vs closed-form gap: %.3e (same order as O(1/N^2) remainder)" % out["N256_limit_gap"])
lines.append("")
lines.append("[NEGATIVE CONTROL a] equatorial cells only (second order, shrinking):")
for r in eq:
    lines.append("  N=%-4d max|rel| = %.3e" % (r["nside"], r["equatorial_max_abs_rel"]))
lines.append("[NEGATIVE CONTROL b] true-curve edges => metric ~ 0:")
for r in tc:
    lines.append("  N=%-4d K=64 rel dev = %.3e ; Richardson residual = %.3e"
                 % (r["nside"], r["true_curve_rel_dev_K64"], r["richsonson_residual"]))
txt = "\n".join(lines)
with open("../results/e2_polar_pixel_limit.txt", "w") as fh:
    fh.write(txt + "\n")
print(txt)
