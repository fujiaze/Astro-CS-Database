# -*- coding: utf-8 -*-
"""p3lib.py -- shared independent geometry primitives (route-2 P3 experiments).

Standalone: depends only on numpy. Does NOT import any repository Python module.
All formulas independently re-derived; validated against Gorski et al. 2005
(arXiv:astro-ph/0409513) definitions.

Chart conventions
-----------------
- Unit vectors on S^2 (right-handed, z = north pole).
- HEALPix equal-area chart per base face f in 0..11, (u,v) in [0,1]^2:
  * equatorial band (faces 4-7 whole square; faces 0-3 with u+v <= 1):
      z = (2/3)(u+v-1),  phi = (pi/4)(u-v) + (pi/2)(f-4)
  * north polar branch (faces 0-3, u+v > 1): s = 2-u-v in (0,1]
      z = 1 - s^2/3,  phi = (pi/2) f + pi (1-v)/(2s)
  * south polar branch (faces 8-11): antipodal image of north face f-8
    (area preserving; science quantities here are rotation invariant).
  Jacobian |d(z,phi)/d(u,v)| = pi/3 identically on every branch.
"""
import numpy as np


# ---------------------------------------------------------------- HEALPix chart

def chart_to_vec(f, u, v):
    """Map base-face chart coordinates (f, u, v) to unit vectors; vectorized.

    Exact replica of the production branch structure
    (spherical_overlap.cpp xyf2ang_replica, lines 711-774):
      - equatorial branch (default): z = (2/3)(u+v+zoff),
        phi = (pi/4)(u-v+phioff+2*chp) with per-face offsets
        zoff/phioff/chp = (0,1,f) for f<=3, (-1,0,f-4) for 4<=f<=7,
        (-2,1,f-8) for f>=8;
      - polar branch f<=3 and u+v>1: z = 1-s^2/3, phi = (pi/2)f + pi(1-v)/(2s),
        s = (1-u)+(1-v);
      - polar branch f>=8 and u+v<1: mirrored, z = -(1-s^2/3),
        phi = (pi/2)(f-8) + pi*u/(2s), s = u+v.
    The seam u+v=1 is continuous (z = +-2/3 on both branches).
    """
    f = np.asarray(f)
    u = np.asarray(u, dtype=float)
    v = np.asarray(v, dtype=float)
    uv = u + v

    zoff = np.where(f <= 3, 0.0, np.where(f <= 7, -1.0, -2.0))
    phioff = np.where(f <= 3, 1.0, np.where(f <= 7, 0.0, 1.0))
    chp = np.where(f <= 3, f, np.where(f <= 7, f - 4.0, f - 8.0))
    ze = (2.0 / 3.0) * (uv + zoff)
    pe = (np.pi / 4.0) * (u - v + phioff + 2.0 * chp)

    # north polar branch
    sn = (1.0 - u) + (1.0 - v)
    sn = np.where(sn <= 0, np.nan, sn)
    zn = 1.0 - sn ** 2 / 3.0
    pn = (np.pi / 2.0) * f + np.pi * (1.0 - v) / (2.0 * sn)
    # south polar branch (mirrored)
    ss = u + v
    ss = np.where(ss <= 0, np.nan, ss)
    zsn = -(1.0 - ss ** 2 / 3.0)
    psn = (np.pi / 2.0) * (f - 8) + np.pi * u / (2.0 * ss)

    pol_n = (f <= 3) & (uv > 1)
    pol_s = (f >= 8) & (uv < 1)
    z = np.where(pol_n, zn, np.where(pol_s, zsn, ze))
    phi = np.where(pol_n, pn, np.where(pol_s, psn, pe))

    # Pole itself (s = 0 corner): direction folds; pin (z, phi) = (sign, 0).
    bad = ~np.isfinite(z)
    if np.any(bad):
        z = np.where(bad, np.where(f >= 8, -1.0, 1.0), z)
        phi = np.where(bad, 0.0, phi)

    st = np.sqrt(np.maximum(0.0, 1.0 - z ** 2))
    out = np.empty(np.broadcast(u, v).shape + (3,))
    out[..., 0] = st * np.cos(phi)
    out[..., 1] = st * np.sin(phi)
    out[..., 2] = z
    return out


def jacobian_chart(f, u, v):
    """Analytic Jacobian; equals pi/3 identically on every branch."""
    return np.full_like(np.asarray(u, dtype=float), np.pi / 3.0)


def leaf_area_analytic(nside):
    """A_leaf = pi/(3 N^2) [sr] (Gorski 2005 Sec. 5: Omega_pix = pi/(3 N^2))."""
    return np.pi / (3.0 * np.asarray(nside, dtype=float) ** 2)


def hp_res(nside):
    """Equal-area linear scale sqrt(A_leaf)."""
    return np.sqrt(np.pi / 3.0) / np.asarray(nside, dtype=float)


def pixel_corners(f, i, j, nside):
    """Corner chart coordinates of leaf (i,j) on face f, cycle order."""
    h = 1.0 / nside
    us = np.array([i * h, (i + 1) * h, (i + 1) * h, i * h])
    vs = np.array([j * h, j * h, (j + 1) * h, (j + 1) * h])
    return f, us, vs


def sample_edge(f, u0, v0, u1, v1, k):
    """Sample k+1 points along a chart-straight (true-curve) leaf edge."""
    t = np.linspace(0.0, 1.0, k + 1)
    return f, u0 + (u1 - u0) * t, v0 + (v1 - v0) * t


def leaf_true_boundary(f, i, j, nside, k):
    """Boundary polygon points (unit vectors), chart-straight edges."""
    f4, us, vs = pixel_corners(f, i, j, nside)
    pts = []
    for m in range(4):
        u0, v0 = us[m], vs[m]
        u1, v1 = us[(m + 1) % 4], vs[(m + 1) % 4]
        _, uu, vv = sample_edge(f4, u0, v0, u1, v1, k)
        pts.append(chart_to_vec(f4, uu, vv))
    return np.concatenate(pts, axis=0)


# --------------------------------------------------------- solid-angle operators

def vos_triangle(a, b, c):
    """Van Oosterom & Strackee (1983): Omega = 2 atan2(|det|, 1+ab+bc+ca).

    fabs on the determinant matches the production/test-side form
    (p1drz_geom.hpp geom_solid_angle_tri) and makes each fan triangle positive.
    """
    det = np.einsum('...i,...i->...', a, np.cross(b, c))
    denom = 1.0 + np.einsum('...i,...i->...', a, b) \
                 + np.einsum('...i,...i->...', b, c) \
                 + np.einsum('...i,...i->...', c, a)
    return 2.0 * np.arctan2(np.abs(det), denom)


def lhuilier_triangle(a, b, c):
    """Spherical excess via l'Huilier's formula; unit-vector inputs."""
    sa = np.arccos(np.clip(np.einsum('...i,...i->...', b, c), -1, 1))
    sb = np.arccos(np.clip(np.einsum('...i,...i->...', a, c), -1, 1))
    sc = np.arccos(np.clip(np.einsum('...i,...i->...', a, b), -1, 1))
    s = 0.5 * (sa + sb + sc)
    t = (np.tan(0.5 * s) * np.tan(0.5 * (s - sa))
         * np.tan(0.5 * (s - sb)) * np.tan(0.5 * (s - sc)))
    return 4.0 * np.arctan(np.sqrt(np.maximum(t, 0.0)))


def geodesic_polygon_area_vos(pts):
    """Signed-fan VOS area of a geodesic polygon (unit vectors, cyclic)."""
    n = pts.shape[0]
    total = 0.0
    for m in range(1, n - 1):
        total += float(vos_triangle(pts[0], pts[m], pts[m + 1]))
    return total


# ------------------------------------------------------- spherical S-H clipping

def clip_halfspace(poly, nrm):
    """Clip geodesic polygon by half-space x.n >= 0 (module generalization)."""
    out = []
    m = len(poly)
    for idx in range(m):
        cur = poly[idx]
        prv = poly[idx - 1]
        sc = float(np.dot(cur, nrm))
        sp = float(np.dot(prv, nrm))
        if sc >= 0.0:
            if sp < 0.0:
                t = sp / (sp - sc)
                out.append(prv + t * (cur - prv))
            out.append(cur)
        elif sp >= 0.0:
            t = sp / (sp - sc)
            out.append(prv + t * (cur - prv))
    return np.array(out) if out else np.zeros((0, 3))


def normalize(v):
    return v / np.linalg.norm(v, axis=-1, keepdims=True)


# ------------------------------------------------------------- TAN projection

def tangent_frame(ra0, dec0):
    n = np.array([np.cos(dec0) * np.cos(ra0), np.cos(dec0) * np.sin(ra0),
                  np.sin(dec0)])
    ea = np.array([-np.sin(ra0), np.cos(ra0), 0.0])
    ed = np.cross(n, ea)
    return n, ea, ed


def tan_project(vec, ra0, dec0):
    """Unit vector -> (xi, eta) [rad]; gnomonic, WCS Paper II eq. (54)."""
    n, ea, ed = tangent_frame(ra0, dec0)
    w = float(np.dot(vec, n))
    return float(np.dot(vec, ea)) / w, float(np.dot(vec, ed)) / w


def tan_deproject(xi, eta, ra0, dec0):
    """(xi, eta) [rad] -> unit vector (gnomonic deprojection)."""
    n, ea, ed = tangent_frame(ra0, dec0)
    p = n + xi * ea + eta * ed
    return p / np.linalg.norm(p)


def ra_dec_to_vec(ra, dec):
    return np.array([np.cos(dec) * np.cos(ra), np.cos(dec) * np.sin(ra),
                     np.sin(dec)])


# --------------------------------------------------------------- planar helpers

def shoelace(pts2):
    """Signed planar polygon area.

    Vertices are translated to their centroid first: the area is translation
    invariant, and the translation removes the O(coord^2) catastrophic
    cancellation for tiny polygons with O(1) coordinates (drop footprints in
    the chart plane).
    """
    a = np.asarray(pts2, dtype=float)
    a = a - a.mean(axis=0)
    x, y = a[:, 0], a[:, 1]
    return 0.5 * float(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1)))


def ortho_tangent_coords(vec, n, ea, ed):
    """Orthographic (tangent-plane) coordinates of a unit vertex."""
    return float(np.dot(vec, ea)), float(np.dot(vec, ed))


def round_half_away(x):
    """C lround() semantics: round half away from zero (nonneg inputs)."""
    return np.floor(np.asarray(x, dtype=float) + 0.5).astype(np.int64)
