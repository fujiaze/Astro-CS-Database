# -*- coding: utf-8 -*-
"""E3: HP_CIRCUMRADIUS_FACTOR = 1.25 -- exhaustive scan of the HEALPix cell circumradius.

Hypothesis:
  H1: the ratio r_circ/hp_res (cell center -> farthest boundary point over the equal-area
      scale hp_res = sqrt(pi/3)/N), maximized over all 12 N^2 cells, is bounded by ~1.0442
      for every nside (worst near |z| = 2/3, i.e. dec ~ +-41.8 deg, NOT at the poles);
  H2: the ratio increases monotonically with N towards its limit (0.994 @ N=4 .. 1.0415+);
  H3 (discriminating contrast): the same metric on an equal-step lon-lat grid is far larger
      (~1.67), i.e. the 1.25 factor is specific to the HEALPix cell shape.
  Classification: 1.25 is a STRUCTURAL safety buffer (candidate-completeness soft upper
  bound), not a science quantity: it does not enter any published signal/variance value;
  the zero-miss guarantee is enforced by the candidate oracle. Exemption reason recorded.

Method: ring-order construction of cell corners as in E2; boundary sampled densely
  (corners + 8 points per edge); vectorized; N in {4,...,64}. Pure numpy, deterministic.

Run: python3 e3_circumradius_scan.py
Output: ../results/e3_circumradius_scan.{json,txt}
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
    S[0] = S[4 * N] = 1
    return j, z, S


def unit(phi, z):
    s = np.sqrt(np.maximum(0.0, 1.0 - z * z))
    return np.stack([s * np.cos(phi), s * np.sin(phi), z], axis=-1)


def ang_between(a, b):
    d = np.clip(np.sum(a * b, axis=-1), -1.0, 1.0)
    return np.arccos(d)


def scan(N):
    j, z, S = ring_levels(N)
    hp_res = math.sqrt(PI / 3.0) / N
    worst = 0.0
    worst_dec = None
    for jj in range(1, 4 * N):
        w = 2 * PI / S[jj]
        o = 0.0 if (jj <= N or jj >= 3 * N or jj % 2 == 0) else PI / S[jj]
        phic = o + (np.arange(S[jj]) + 0.5) * w
        # center of cell (approximate: mean of corners on the sphere)
        zc = 0.5 * (z[jj - 1] + z[jj + 1])  # center = midpoint of adjacent ring levels
        # boundary: corners + dense edge sampling
        t = np.linspace(0.0, 1.0, 9)
        # top corner: nearest level-(jj-1) vertex
        if jj == 1:
            tphi = phic
            tz = np.full(S[jj], 1.0)
        else:
            lv = o if jj - 1 <= N or (jj - 1 >= 3 * N) else (PI / S[jj - 1] if (jj - 1) % 2 == 1 else 0.0)
            lphi = lv + np.arange(S[jj - 1]) * (2 * PI / S[jj - 1])
            d = np.abs(((phic[:, None] - lphi[None, :] + PI) % (2 * PI)) - PI)
            tphi = lphi[np.argmin(d, axis=1)]
            tz = np.full(S[jj], z[jj - 1])
        if jj == 4 * N - 1:
            bphi = phic
            bz = np.full(S[jj], -1.0)
        else:
            lv2 = 0.0 if (jj + 1 <= N or (jj + 1 >= 3 * N) or (jj + 1) % 2 == 0) else PI / S[jj + 1]
            lphi2 = lv2 + np.arange(S[jj + 1]) * (2 * PI / S[jj + 1])
            d2 = np.abs(((phic[:, None] - lphi2[None, :] + PI) % (2 * PI)) - PI)
            bphi = lphi2[np.argmin(d2, axis=1)]
            bz = np.full(S[jj], z[jj + 1])
        # four edges sampled: top->R, R->bottom, bottom->L, L->top
        def edge(p1, z1, p2, z2):
            # interpolate in (phi,z) along the chart-straight edge; handle wrap via nearest path
            dphi = (p2 - p1 + PI) % (2 * PI) - PI
            tt = t[None, 1:-1]
            ph = (p1[:, None] + tt * dphi[:, None]) % (2 * PI)
            zz = z1[:, None] + tt * (z2 - z1)[:, None]
            return ph, zz
        segs = []
        segs.append(edge(tphi, tz, phic + w / 2, np.full(S[jj], z[jj])))
        segs.append(edge(phic + w / 2, np.full(S[jj], z[jj]), bphi, bz))
        segs.append(edge(bphi, bz, phic - w / 2, np.full(S[jj], z[jj])))
        segs.append(edge(phic - w / 2, np.full(S[jj], z[jj]), tphi, tz))
        # per-cell boundary set: corners (S,4,2) then the 4 sampled edges (S,7,2) each,
        # concatenated along the point axis so row k holds ONLY cell k's boundary points
        corners = np.stack([
            np.stack([phic + w / 2, np.full(S[jj], z[jj])], axis=1),
            np.stack([bphi, bz], axis=1),
            np.stack([phic - w / 2, np.full(S[jj], z[jj])], axis=1),
            np.stack([tphi, tz], axis=1),
        ], axis=1)                                               # (S,4,2)
        parts = [corners] + [np.stack([ph, zz], axis=2) for ph, zz in segs]
        P = np.concatenate(parts, axis=1)                        # (S,4+4*7,2)
        C = unit(phic, np.full(S[jj], zc))[:, None, :]           # (S,1,3)
        B = unit(P[..., 0].ravel(), P[..., 1].ravel()).reshape(S[jj], -1, 3)
        d = ang_between(C, B).max(axis=1) / hp_res
        k = int(np.argmax(d))
        if d[k] > worst:
            worst = float(d[k])
            worst_dec = math.degrees(math.asin(zc))
    return worst, worst_dec


def lonlat_scan(N):
    """Contrast metric on an equal-step lon-lat grid (dalpha x ddelta cells)."""
    dd = PI / N
    da = PI / N
    worst = 0.0
    for i in range(N):
        dec = -PI / 2 + (i + 0.5) * dd
        zc = math.sin(dec)
        hp = math.sqrt(dd * da)  # equal-area scale of the cell
        corners = []
        for dde in (-dd / 2, dd / 2):
            for dda in (-da / 2, da / 2):
                corners.append(unit(np.array([dda]), np.array([math.asin(max(-1, min(1, zc + dde)))])))
        C = unit(np.array([0.0]), np.array([zc]))
        d = max(ang_between(C, p) for p in corners) / hp
        worst = max(worst, float(d))
    return worst


out = {"seed": SEED, "per_n": {}, "lonlat_contrast": {}}
for N in (4, 8, 16, 32, 64):
    w, dec = scan(N)
    out["per_n"][N] = {"max_circumradius_over_hp_res": w, "worst_dec_deg": dec}
    ll = lonlat_scan(N) if N <= 16 else None
    if ll:
        out["lonlat_contrast"][N] = ll
    print("N=%-3d max r_circ/hp_res = %.5f  (dec=%.2f deg)   lonlat contrast=%s"
          % (N, w, dec, ("%.4f" % ll) if ll else "-"))

vals = [out["per_n"][n]["max_circumradius_over_hp_res"] for n in sorted(out["per_n"])]
out["monotone_increasing"] = all(vals[i] <= vals[i + 1] + 1e-12 for i in range(len(vals) - 1))
out["max_measured"] = max(vals)
out["headroom_1p25_vs_measured"] = 1.25 / out["max_measured"] - 1.0
out["classification"] = ("structural safety buffer (exempt from three-leg requirement): soft upper "
                         "bound for candidate completeness; does not enter published signal/variance; "
                         "zero-miss enforced by candidate oracle")

with open("../results/e3_circumradius_scan.json", "w") as fh:
    json.dump(out, fh, indent=1)
lines = ["E3 HP_CIRCUMRADIUS_FACTOR scan (seed=%d)" % SEED, ""]
for N in sorted(out["per_n"], key=int):
    r = out["per_n"][N]
    lines.append("  N=%-3d max r_circ/hp_res = %.5f @ dec=%.2f deg" % (int(N), r["max_circumradius_over_hp_res"], r["worst_dec_deg"]))
lines.append("  monotone increasing: %s" % out["monotone_increasing"])
lines.append("  max measured = %.5f ; 1.25 headroom = %+.1f%%" % (out["max_measured"], 100 * out["headroom_1p25_vs_measured"]))
for N, v in out["lonlat_contrast"].items():
    lines.append("  lon-lat grid contrast N=%s: %.4f" % (N, v))
lines.append("  classification: " + out["classification"])
txt = "\n".join(lines)
with open("../results/e3_circumradius_scan.txt", "w") as fh:
    fh.write(txt + "\n")
print(txt)
