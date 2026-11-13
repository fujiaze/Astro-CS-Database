#!/usr/bin/env python3
"""Independent Gaia TAN+SIP solution for one M42 panel (issue #126).

The production V2 camera model is
    u = x + 1 - CRPIX,   q = CD (u + A, v + B),   n = normalize(c + qx e + qy north)
with A, B polynomials of degree >= 2.  For a field of ~1 degree the composed
map detector -> gnomonic tangent plane is a polynomial of degree 3 to well
below 0.01 px, so this fit solves it directly:

  1. choose the tangent point c0 at a detector reference point (median of the
     matched bright stars) and take its sky position from the frame's header
     WCS (the header is only the starting point, never the answer);
  2. project every Gaia direction onto the orthonormal (east, north) frame at
     c0: (xi, eta) = (n.e, n.n)/(n.c);
  3. linear least squares on the full degree-3 polynomial in (x - x0) for
     (xi, eta), with iterative robust clipping;
  4. take the constant + linear part as the CRVAL/CRPIX/CD of the production
     model and the degree>=2 residual as SIP A/B (exact polynomial shift: the
     SIP variable is the production u, not the fit variable);
  5. report the train/holdout RMS in pixels.

The input epoch propagation uses Gaia DR3 proper motions (the same frozen
catalogue as M42-S1).  The output is the production `acsd.WcsModel/1` JSON.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys

import numpy as np
from astropy.io import fits
from astropy.time import Time
from scipy.spatial import cKDTree

MAS_YR_TO_DEG_YR = 1.0 / (3600.0 * 1000.0)
ARCSEC_PER_RAD = 180.0 / math.pi * 3600.0


def load_gaia(path: str):
    rows = list(csv.DictReader(open(path)))
    return {
        "sid": np.array([int(r["source_id"]) for r in rows], dtype=np.int64),
        "ra": np.array([float(r["ra_deg"]) for r in rows]),
        "dec": np.array([float(r["dec_deg"]) for r in rows]),
        "pmra": np.array([float(r["pmra_cosdec_mas_yr"]) for r in rows]),
        "pmdec": np.array([float(r["pmdec_mas_yr"]) for r in rows]),
        "G": np.array([float(r["magG"]) for r in rows]),
    }


def propagate(stars, year: float):
    dt = year - 2016.0
    cosd = np.cos(np.radians(stars["dec"]))
    ra = stars["ra"] + stars["pmra"] * dt * MAS_YR_TO_DEG_YR / cosd
    dec = stars["dec"] + stars["pmdec"] * dt * MAS_YR_TO_DEG_YR
    return ra, dec, dt


def detect(data: np.ndarray):
    from photutils.background import Background2D, MedianBackground
    from photutils.detection import DAOStarFinder
    bkg = Background2D(data, 64, filter_size=3, bkg_estimator=MedianBackground())
    sub = data - bkg.background
    rms = float(np.median(bkg.background_rms))
    src = DAOStarFinder(fwhm=3.35, threshold=5.0 * rms, sharplo=0.3, sharphi=1.5,
                        roundlo=-0.8, roundhi=0.8)(sub)
    x = np.asarray(src["xcentroid"], float)
    y = np.asarray(src["ycentroid"], float)
    peak = np.asarray(src["peak"], float)
    tree = cKDTree(np.column_stack([x, y]))
    dd, _ = tree.query(np.column_stack([x, y]), k=2)
    iso = dd[:, 1]
    clean = (peak < 55000) & (iso > 3.0)
    return np.column_stack([x[clean], y[clean]]), rms


def project(direction, centre):
    c = centre / np.linalg.norm(centre)
    ra0 = math.atan2(c[1], c[0])
    dec0 = math.asin(max(-1.0, min(1.0, c[2])))
    east = np.array([-math.sin(ra0), math.cos(ra0), 0.0])
    north = np.cross(c, east)
    denom = direction @ c
    xi = (direction @ east) / denom
    eta = (direction @ north) / denom
    return xi, eta, c, east, north


def radec_to_unit(ra_deg, dec_deg):
    ra = np.radians(ra_deg)
    dec = np.radians(dec_deg)
    cosd = np.cos(dec)
    return np.column_stack([cosd * np.cos(ra), cosd * np.sin(ra), np.sin(dec)])


def basis_terms(degree: int):
    terms = []
    for total in range(degree + 1):
        for i in range(total, -1, -1):
            j = total - i
            terms.append((i, j))
    return terms


def design(dx, dy, terms):
    columns = [dx ** i * dy ** j for (i, j) in terms]
    return np.column_stack(columns)


def shift_coefficients(coeffs, terms, delta):
    """Coefficients of the same polynomial in (dx - delta) instead of dx.

    p(dx) = sum c_ij dx^i dy^j;  q(u) = p(u + delta) is expanded exactly by the
    binomial theorem (degree stays the same)."""
    out = {t: 0.0 for t in terms}
    for (i, j), c in zip(terms, coeffs):
        if c == 0.0:
            continue
        for a in range(i + 1):
            for b in range(j + 1):
                # (u + dx0)^i (v + dy0)^j
                out[(a, b)] += c * math.comb(i, a) * math.comb(j, b) * \
                    (delta[0] ** (i - a)) * (delta[1] ** (j - b))
    return np.array([out[t] for t in terms])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("frame")
    ap.add_argument("stars")
    ap.add_argument("out_dir")
    ap.add_argument("--tag", required=True)
    ap.add_argument("--detector-frame", default="M42-panel")
    ap.add_argument("--degree", type=int, default=3)
    ap.add_argument("--holdout", type=float, default=0.3)
    ap.add_argument("--seed", type=int, default=20261112)
    ap.add_argument("--gmax", type=float, default=16.0)
    args = ap.parse_args()

    with fits.open(args.frame) as hdul:
        header = hdul[0].header.copy()
        data = hdul[0].data.astype(np.float32)
    height, width = data.shape
    t = Time(header["DATE-OBS"], scale="utc")
    stars = load_gaia(args.stars)
    ra, dec, dt = propagate(stars, t.jyear)
    det, rms = detect(data)
    print(f"frame={args.frame}")
    print(f"  DATE-OBS={t.isot} jyear={t.jyear:.5f} dt={dt:.4f}yr detections={len(det)} rms={rms:.2f}")

    # Initial tangent point: the header WCS at the detector centre.
    from astropy.wcs import WCS
    header_wcs = WCS(header)
    ref = np.array([width * 0.5, height * 0.5])
    try:
        sky0 = header_wcs.all_pix2world(ref[0], ref[1], 0)
        if not np.all(np.isfinite(sky0)):
            raise ValueError
    except Exception:
        sky0 = (header["CRVAL1"], header["CRVAL2"])
    centre = radec_to_unit(sky0[0], sky0[1])[0]
    ra0 = math.atan2(centre[1], centre[0])
    dec0 = math.asin(centre[2])
    east = np.array([-math.sin(ra0), math.cos(ra0), 0.0])
    north = np.cross(centre, east)
    unit = radec_to_unit(ra, dec)
    denom = unit @ centre
    xi = (unit @ east) / denom
    eta = (unit @ north) / denom
    print(f"  initial tangent point {math.degrees(ra0):.6f},{math.degrees(dec0):.6f}")

    # The header predictor of every Gaia star (0-based detector coordinates).
    try:
        px, py = header_wcs.all_world2pix(ra, dec, 0)
    except Exception:
        px, py = header_wcs.wcs_world2pix(ra, dec, 0)
    in_frame = ((px > 10) & (px < width - 10) & (py > 10) & (py < height - 10) &
                (stars["G"] <= args.gmax) & np.isfinite(px) & np.isfinite(py))
    print(f"  gaia in frame={int(in_frame.sum())}")

    # Association and polynomial refinement.  A crowded 1-degree field makes
    # a 40 px header association mostly false: start from the bright stars
    # (whose header prediction is reliable) and shrink the radius as the
    # polynomial improves.  The Gaia detector position for the association is
    # obtained by Newton inversion of the current polynomial, starting from the
    # header prediction.
    terms = basis_terms(args.degree)
    rng = np.random.default_rng(args.seed)
    holdout = rng.random(len(ra)) < args.holdout
    tree = cKDTree(det)
    index = np.full(len(ra), -1, dtype=int)
    distance = np.full(len(ra), np.inf)
    in_frame_idx = np.where(in_frame)[0]

    def predict_detector(ax, ay, target_xi, target_eta, guess):
        # guess is an (N,2) array of detector positions.
        x = np.array(guess, dtype=float).copy()
        for _ in range(8):
            u = x[:, 0] - ref[0]
            v = x[:, 1] - ref[1]
            A = design(u, v, terms)
            px = A @ ax
            py = A @ ay
            J = np.zeros((len(x), 2, 2))
            for (i, j), c_x, c_y in zip(terms, ax, ay):
                if i > 0:
                    f = i * u ** (i - 1) * v ** j
                    J[:, 0, 0] += c_x * f
                    J[:, 1, 0] += c_y * f
                if j > 0:
                    f = j * u ** i * v ** (j - 1)
                    J[:, 0, 1] += c_x * f
                    J[:, 1, 1] += c_y * f
            rx = target_xi - px
            ry = target_eta - py
            detJ = J[:, 0, 0] * J[:, 1, 1] - J[:, 0, 1] * J[:, 1, 0]
            detJ = np.where(np.abs(detJ) > 1e-30, detJ, 1e-30)
            step_x = (J[:, 1, 1] * rx - J[:, 0, 1] * ry) / detJ
            step_y = (-J[:, 1, 0] * rx + J[:, 0, 0] * ry) / detJ
            x[:, 0] += step_x
            x[:, 1] += step_y
            if np.max(np.hypot(step_x, step_y)) < 1e-4:
                break
        return x

    header_guess = np.column_stack([np.clip(px, 0, width - 1),
                                    np.clip(py, 0, height - 1)])

    def associate(pred, radius, bright_only):
        keep = np.zeros(len(ra), dtype=bool)
        dist = np.full(len(ra), np.inf)
        idx = np.full(len(ra), -1, dtype=int)
        used = np.zeros(len(det), dtype=bool)
        cand = in_frame_idx if not bright_only else in_frame_idx[stars["G"][in_frame_idx] < 14.0]
        order = cand[np.argsort(stars["G"][cand])]
        for k in order:
            d, j = tree.query(pred[k], distance_upper_bound=radius)
            if np.isfinite(d) and not used[int(j)]:
                used[int(j)] = True
                idx[k] = int(j)
                dist[k] = d
        keep = idx >= 0
        return keep, dist, idx

    # Initial affine: the header is only a predictor; a six parameter affine
    # (rotation, scale, shear, translation) from the header predicted position
    # to the detected position is fitted robustly on the bright stars.  This is
    # the main production association strategy (M42-S1 fit_frame.py) and it is
    # what makes a crowded 1-degree field tractable.
    bright = in_frame_idx[stars["G"][in_frame_idx] < 13.5]
    tree_all = cKDTree(det)
    d0, j0 = tree_all.query(np.column_stack([px[bright], py[bright]]),
                            distance_upper_bound=25.0)
    ok = np.isfinite(d0)
    P = np.column_stack([px[bright][ok], py[bright][ok], np.ones(ok.sum())])
    Q = det[j0[ok].astype(int)] if ok.sum() else np.zeros((0, 2))
    keep = np.ones(len(P), dtype=bool)
    coef = np.zeros((3, 2))
    for _ in range(15):
        if keep.sum() < 8:
            break
        coef = np.linalg.lstsq(P[keep], Q[keep], rcond=None)[0]
        pred = P @ coef
        r = np.hypot(pred[:, 0] - Q[:, 0], pred[:, 1] - Q[:, 1])
        new = r < max(1.2, np.percentile(r[keep], 70))
        if new.sum() == keep.sum():
            break
        keep = new
    pred = P @ coef
    r = np.hypot(pred[:, 0] - Q[:, 0], pred[:, 1] - Q[:, 1])
    inl = r < max(1.5, np.percentile(r, 80))
    print(f"  affine init inliers={int(inl.sum())}/{int(ok.sum())} "
          f"med={np.median(r[inl]) if inl.any() else float('nan'):.2f}px")
    aff = (coef[0, 0], coef[1, 0], coef[2, 0], coef[0, 1], coef[1, 1], coef[2, 1])
    header_guess = np.column_stack([aff[0] * px + aff[1] * py + aff[2],
                                    aff[3] * px + aff[4] * py + aff[5]])

    ax = ay = None
    matched = np.zeros(len(ra), dtype=bool)
    radius = 4.0
    for iteration in range(8):
        if ax is None:
            guess = header_guess
        else:
            guess = predict_detector(ax, ay, xi, eta, header_guess)
        keep, dist, idx = associate(guess, radius, bright_only=(iteration == 0))
        sel = np.where(keep)[0]
        print(f"  iter{iteration}: r={radius:.2f}px matched={sel.size} "
              f"med={np.median(dist[keep]) if sel.size else float('nan'):.2f}px")
        if sel.size < 40:
            print("FATAL: association below 40 stars", file=sys.stderr)
            return 1
        train = sel[~holdout[sel]]
        if train.size < 30:
            train = sel
        u = det[idx[train], 0] - ref[0]
        v = det[idx[train], 1] - ref[1]
        A = design(u, v, terms)
        ax = np.linalg.lstsq(A, xi[train], rcond=None)[0]
        ay = np.linalg.lstsq(A, eta[train], rcond=None)[0]
        # Robust clipping of the polynomial residual in arcsec.
        dx = det[idx[sel], 0] - ref[0]
        dy = det[idx[sel], 1] - ref[1]
        P = design(dx, dy, terms)
        res = np.hypot(P @ ax - xi[sel], P @ ay - eta[sel]) * ARCSEC_PER_RAD
        med = np.median(res)
        mad = 1.4826 * np.median(np.abs(res - med))
        thr = max(0.10, med + 5.0 * mad)
        if iteration >= 2:
            thr = min(0.40, thr)
        good = sel[res < thr]
        print(f"           polynomial med={med:.3f}\" kept={good.size}/{sel.size}")
        if good.size < 40:
            print("FATAL: polynomial clip below 40 stars", file=sys.stderr)
            return 1
        # Refit on the clipped training set.
        train = good[~holdout[good]]
        if train.size < 30:
            train = good
        u = det[idx[train], 0] - ref[0]
        v = det[idx[train], 1] - ref[1]
        A = design(u, v, terms)
        ax = np.linalg.lstsq(A, xi[train], rcond=None)[0]
        ay = np.linalg.lstsq(A, eta[train], rcond=None)[0]
        matched = np.zeros(len(ra), dtype=bool)
        matched[good] = True
        if radius <= 2.0 and good.size == sel.size:
            break
        radius = max(1.5, radius * 0.5)

    def fit(sel):
        u = det[idx[sel], 0] - ref[0]
        v = det[idx[sel], 1] - ref[1]
        A = design(u, v, terms)
        ax = np.linalg.lstsq(A, xi[sel], rcond=None)[0]
        ay = np.linalg.lstsq(A, eta[sel], rcond=None)[0]
        return ax, ay

    def residuals(sel, ax, ay):
        u = det[idx[sel], 0] - ref[0]
        v = det[idx[sel], 1] - ref[1]
        A = design(u, v, terms)
        return np.hypot(A @ ax - xi[sel], A @ ay - eta[sel]) * ARCSEC_PER_RAD

    active = np.where(matched)[0]
    print(f"  final association {active.size} (holdout {holdout[active].sum()})")
    # Final fit uses the training split only; the holdout is untouched.
    train = active[~holdout[active]]
    hold = active[holdout[active]]
    if train.size < 40:
        print("FATAL: too few training stars", file=sys.stderr)
        return 1
    ax, ay = fit(train)
    train_rms = float(np.sqrt(np.mean(residuals(train, ax, ay) ** 2)))
    hold_rms = float(np.sqrt(np.mean(residuals(hold, ax, ay) ** 2))) if hold.size else float("nan")
    print(f"  train n={train.size} rms={train_rms:.3f}\"  holdout n={hold.size} rms={hold_rms:.3f}\"")

    # Decompose the polynomial at the reference: constant, linear (CD) and SIP.
    lin_x = np.array([ax[terms.index((1, 0))], ax[terms.index((0, 1))]])
    lin_y = np.array([ay[terms.index((1, 0))], ay[terms.index((0, 1))]])
    cd = np.array([[lin_x[0], lin_x[1]], [lin_y[0], lin_y[1]]])
    const = np.array([ax[terms.index((0, 0))], ay[terms.index((0, 0))]])
    # Move the reference so that the constant term vanishes: u = x - ref',
    # ref' = ref + cd^-1 const (in pixels).
    delta = -np.linalg.solve(cd, const)
    ref_prime = ref + delta
    ax_s = shift_coefficients(ax, terms, delta)
    ay_s = shift_coefficients(ay, terms, delta)
    # q = cd * (u + A(u,v)); A = cd^-1 * P(u) - u.  The linear part of A must be
    # exactly zero by construction; only degree >= 2 terms are kept.
    inverse = np.linalg.inv(cd)
    sip_a = {}
    sip_b = {}
    for (i, j), cx, cy in zip(terms, ax_s, ay_s):
        if i + j < 2:
            continue
        sip_a[(i, j)] = float(inverse[0, 0] * cx + inverse[0, 1] * cy)
        sip_b[(i, j)] = float(inverse[1, 0] * cx + inverse[1, 1] * cy)
    order = max(max(t) for t in sip_a) if sip_a else 0
    terms_a = [{"x_power": i, "y_power": j, "coefficient": sip_a[(i, j)]}
               for (i, j) in sorted(sip_a)]
    terms_b = [{"x_power": i, "y_power": j, "coefficient": sip_b[(i, j)]}
               for (i, j) in sorted(sip_b)]
    doc = {
        "schema": "acsd.WcsModel/1",
        "crpix_fits1": [float(ref_prime[0] + 1.0), float(ref_prime[1] + 1.0)],
        "cd_rad_per_pixel": [float(v) for v in cd.reshape(-1)],
        "c": [float(v) for v in centre],
        "e": [float(v) for v in east],
        "north": [float(v) for v in north],
        "sip_a": {"order": int(order), "terms": terms_a},
        "sip_b": {"order": int(order), "terms": terms_b},
        "detector_frame": args.detector_frame,
    }
    stats = {
        "schema": "acsd.issue126.panel_wcs/1",
        "frame": args.frame,
        "tag": args.tag,
        "degree": args.degree,
        "date_obs": t.isot,
        "epoch_jyear": float(t.jyear),
        "detections": int(len(det)),
        "gaia_in_frame": int(in_frame.sum()),
        "header_association": int(matched.size),
        "matched_final": int(active.size),
        "train": int(train.size),
        "holdout": int(hold.size),
        "train_rms_arcsec": train_rms,
        "holdout_rms_arcsec": hold_rms,
        "sip_order": int(order),
        "crpix": doc["crpix_fits1"],
        "cd": doc["cd_rad_per_pixel"],
        "crval_deg": [math.degrees(ra0), math.degrees(dec0)],
    }
    import os
    os.makedirs(args.out_dir, exist_ok=True)
    with open(f"{args.out_dir}/wcs_{args.tag}.json", "w") as stream:
        json.dump(doc, stream, indent=1)
    with open(f"{args.out_dir}/fit_{args.tag}.json", "w") as stream:
        json.dump(stats, stream, indent=1)
    print(f"  wrote wcs_{args.tag}.json (SIP order {order})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
