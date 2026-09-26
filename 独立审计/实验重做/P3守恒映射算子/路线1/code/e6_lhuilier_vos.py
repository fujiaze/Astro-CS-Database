# -*- coding: utf-8 -*-
"""E6: two independent spherical-triangle area formulas -- VOS x l'Huilier cross-check.

Hypotheses:
  H1: Van Oosterom & Strackee (1983) solid angle  E = |2*atan2(det, 1+a.b+b.c+c.a)| and
      l'Huilier's theorem  tan(E/4) = sqrt(tan(s/2) tan((s-a)/2) tan((s-b)/2) tan((s-c)/2))
      agree to <= ~1e-12 relative over the valid triangle family (random, tiny,
      near-hemisphere) on normalized unit vectors.
  H2 (requirement-A self-check, non-normalized input divergence): scaling all three
      vectors by lambda = 1.064 leaves a properly formulated l'Huilier UNCHANGED (side
      lengths are central angles, homogeneous in the inputs) but shifts VOS measurably
      (~9.6% level on small triangles) => the divergence detects non-normalized input,
      as demanded by repo doc 02 Sec.3.2 requirement A.
  H3 (conditioning boundary, honest): l'Huilier loses relative precision for tiny
      triangles (arccos cancellation in the side lengths) and near the hemisphere
      (tan(s/2) blow-up); VOS stays robust. Quantified, not hidden.

Run: python3 e6_lhuilier_vos.py        (seed fixed, seconds)
Output: ../results/e6_lhuilier_vos.{json,txt}
"""
import json
import math
import numpy as np

PI = math.pi
SEED = 20050709
rng = np.random.default_rng(SEED)
OUT = {"seed": SEED}


def vos_area(a, b, c):
    det = np.sum(a * np.cross(b, c), axis=-1)
    dot = np.sum(a * b, axis=-1) + np.sum(b * c, axis=-1) + np.sum(c * a, axis=-1)
    return np.abs(2.0 * np.arctan2(det, 1.0 + dot))


def lhuilier_area(a, b, c):
    """l'Huilier: side lengths = central angles from NORMALIZED dots (homogeneous in inputs)."""
    na = a / np.linalg.norm(a, axis=-1, keepdims=True)
    nb = b / np.linalg.norm(b, axis=-1, keepdims=True)
    nc = c / np.linalg.norm(c, axis=-1, keepdims=True)
    s_ab = np.arccos(np.clip(np.sum(na * nb, axis=-1), -1, 1))
    s_bc = np.arccos(np.clip(np.sum(nb * nc, axis=-1), -1, 1))
    s_ca = np.arccos(np.clip(np.sum(nc * na, axis=-1), -1, 1))
    s = 0.5 * (s_ab + s_bc + s_ca)
    t = (np.tan(0.5 * s) * np.tan(0.5 * (s - s_ab)) *
         np.tan(0.5 * (s - s_bc)) * np.tan(0.5 * (s - s_ca)))
    t = np.clip(t, 0.0, None)
    return 4.0 * np.arctan(np.sqrt(t))


def sample_triangles(n, mode):
    if mode == "random":
        v = rng.normal(size=(n, 3, 3))
        v /= np.linalg.norm(v, axis=-1, keepdims=True)
        return v
    if mode == "tiny":
        base = rng.normal(size=(n, 3)); base /= np.linalg.norm(base, axis=-1, keepdims=True)
        helper = np.where(np.abs(base[:, 2:3]) < 0.9,
                          np.array([0.0, 0.0, 1.0]), np.array([1.0, 0.0, 0.0]))
        e1 = np.cross(base, helper); e1 /= np.linalg.norm(e1, axis=-1, keepdims=True)
        e2 = np.cross(base, e1)
        r = rng.uniform(1e-6, 1e-3, n)[:, None]
        angs = np.sort(rng.uniform(0, 2 * PI, (n, 3)), axis=1)
        v = np.empty((n, 3, 3))
        for k in range(3):
            v[:, k, :] = base * np.cos(r) + (e1 * np.cos(angs[:, k])[:, None] +
                                             e2 * np.sin(angs[:, k])[:, None]) * np.sin(r)
        return v
    if mode == "near_hemisphere":
        v = rng.normal(size=(n, 3, 3))
        v /= np.linalg.norm(v, axis=-1, keepdims=True)
        nrm = rng.normal(size=(n, 3)); nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
        comp = np.sum(v * nrm[:, None, :], axis=-1, keepdims=True)
        v = v - 0.999 * comp * nrm[:, None, :]
        v /= np.linalg.norm(v, axis=-1, keepdims=True)
        return v
    raise ValueError(mode)


res = {}
for mode, n in (("random", 200000), ("tiny", 50000), ("near_hemisphere", 50000)):
    v = sample_triangles(n, mode)
    a, b, c = v[:, 0, :], v[:, 1, :], v[:, 2, :]
    A_vos = vos_area(a, b, c)
    A_lh = lhuilier_area(a, b, c)
    ok = (A_vos > 0) & np.isfinite(A_vos) & np.isfinite(A_lh) & (A_vos < PI * 0.999) & (A_lh < PI * 0.999)
    rel = np.abs(A_lh[ok] - A_vos[ok]) / A_vos[ok]
    res[mode] = {
        "n": int(n), "n_valid": int(ok.sum()),
        "max_rel_diff": float(rel.max()) if rel.size else None,
        "median_rel_diff": float(np.median(rel)) if rel.size else None,
    }
    print("%-15s n=%d valid=%d max_rel=%.3e median=%.3e"
          % (mode, n, ok.sum(), res[mode]["max_rel_diff"], res[mode]["median_rel_diff"]))

# H2: non-normalized input divergence
v = sample_triangles(50000, "random")
a, b, c = v[:, 0, :] * 1.064, v[:, 1, :] * 1.064, v[:, 2, :] * 1.064
A_vos_scaled = vos_area(a, b, c)
A_lh_scaled = lhuilier_area(a, b, c)
A_vos_ref = vos_area(v[:, 0, :], v[:, 1, :], v[:, 2, :])
A_lh_ref = lhuilier_area(v[:, 0, :], v[:, 1, :], v[:, 2, :])
vos_shift = np.abs(A_vos_scaled - A_vos_ref) / A_vos_ref
lh_shift = np.abs(A_lh_scaled - A_lh_ref) / A_lh_ref
small = A_vos_ref < 0.05   # small-triangle domain: theory shift ~ lambda^3*4/(1+3 lambda^2) - 1
OUT["non_normalized_divergence"] = {
    "lambda": 1.064,
    "n_small_domain": int(small.sum()),
    "vos_rel_shift_median_small_domain": float(np.median(vos_shift[small])),
    "vos_rel_shift_max": float(vos_shift.max()),
    "lhuilier_rel_shift_max": float(lh_shift.max()),
    "expected_small_triangle_shift": (1.064 ** 3) * 4.0 / (1.0 + 3 * 1.064 ** 2) - 1.0,
    "verdict": ("divergence DETECTS non-normalized input (VOS shifts on small triangles, "
                "l'Huilier invariant)")
               if float(lh_shift.max()) < 1e-12 and float(np.median(vos_shift[small])) > 0.05 else "FAILED",
}
print("non-normalized lambda=1.064: VOS shift (small-tri domain) median=%.4f ; l'Huilier shift max=%.2e"
      % (OUT["non_normalized_divergence"]["vos_rel_shift_median_small_domain"],
         OUT["non_normalized_divergence"]["lhuilier_rel_shift_max"]))

# H3: hemisphere domain census
mx = np.zeros(50000)
for i in range(3):
    for j in range(i + 1, 3):
        mx = np.maximum(mx, np.arccos(np.clip(np.sum(v[:, i, :] * v[:, j, :], axis=-1), -1, 1)))
hem = mx >= PI / 2 - 1e-12
A = vos_area(v[:, 0, :], v[:, 1, :], v[:, 2, :])
OUT["hemisphere_domain"] = {
    "n": 50000,
    "n_with_side_ge_half_circle": int(hem.sum()),
    "vos_finite_on_those": int(np.sum(np.isfinite(A) & hem)),
    "note": "documented behavior: max_ang >= pi/2 - 1e-12 must fail explicitly (NaN) per 05 Sec.3.5",
}

OUT["triangle_cross_check"] = res

with open("../results/e6_lhuilier_vos.json", "w") as fh:
    json.dump(OUT, fh, indent=1)

L = ["E6 VOS x l'Huilier cross-check (seed=%d)" % SEED, ""]
for mode, r in res.items():
    L.append("  %-15s n=%-7d valid=%-7d max_rel=%.3e median_rel=%.3e"
             % (mode, r["n"], r["n_valid"], r["max_rel_diff"], r["median_rel_diff"]))
nd = OUT["non_normalized_divergence"]
L.append("")
L.append("[NEGATIVE CONTROL non-normalized] lambda=1.064: VOS shift (small-tri, n=%d) median=%.4f (theory %.4f) ; l'Huilier shift max=%.2e"
         % (nd["n_small_domain"], nd["vos_rel_shift_median_small_domain"],
            nd["expected_small_triangle_shift"], nd["lhuilier_rel_shift_max"]))
L.append("  verdict: %s" % nd["verdict"])
hd = OUT["hemisphere_domain"]
L.append("[DOMAIN] triangles with side >= pi/2-1e-12: %d/%d (documented explicit-fail domain)" % (hd["n_with_side_ge_half_circle"], hd["n"]))
L.append("[HONEST BOUNDARY] l'Huilier conditioning: arccos cancellation (tiny triangles) and")
L.append("  tan(s/2) blow-up (near-hemisphere) degrade its relative accuracy; VOS (atan2 form) robust.")
txt = "\n".join(L)
with open("../results/e6_lhuilier_vos.txt", "w") as fh:
    fh.write(txt + "\n")
print(txt)
