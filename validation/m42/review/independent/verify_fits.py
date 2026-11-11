#!/usr/bin/env python3
"""Independent audit script (read-only): deliverable FITS + astrometry.

Run from the repo root:  python3 run/m42-s1/review/independent/verify_fits.py
It never writes outside stdout.
"""
import csv
import json

import numpy as np
from astropy.io import fits
from astropy.wcs import WCS

ROOT = "/workspace/Astro CS Database"
DELIV = f"{ROOT}/validation/m42/deliverable/M42-S1_Halpha_256_calibrated.fits"
RAW = (f"{ROOT}/testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M5/"
       "M42_M5_T2_flying_dutchman-20251212@041842-600S-H-alpha.fts")
CAL = f"{ROOT}/run/m42-s1/inputs/calibration"
X0, Y0, N = 1792, 384, 256


def check_fits():
    with fits.open(DELIV) as h:
        d = h[0].data
        hdr = h[0].header
    print("== deliverable FITS ==")
    print(" shape", d.shape, "dtype", d.dtype, "BUNIT", hdr["BUNIT"], "BITPIX", hdr["BITPIX"])
    nan = np.isnan(d)
    print(" NaN", int(nan.sum()), f"{nan.mean():.6f}")
    with fits.open(RAW) as h:
        r = np.array(h[0].data[Y0:Y0 + N, X0:X0 + N], dtype=np.float64)
    print(" NaN mask == raw>=65535 :", np.array_equal(nan, r >= 65535.0),
          " (n_sat", int((r >= 65535.0).sum()), ")")
    # independent calibration
    bias = np.array(fits.getdata(f"{CAL}/bias_adu.fits"), dtype=np.float64)
    dark = np.array(fits.getdata(f"{CAL}/dark600_adu.fits"), dtype=np.float64)
    flat = np.array(fits.getdata(f"{CAL}/flat_ha_normalized.fits"), dtype=np.float64)
    fn = np.median(flat)
    f = flat[Y0:Y0 + N, X0:X0 + N] / fn
    b = bias[Y0:Y0 + N, X0:X0 + N]
    dk = dark[Y0:Y0 + N, X0:X0 + N]
    x = (r - b - (dk - b)) / f  # beta = 1
    m = np.isfinite(d)
    diff = np.abs(d[m] - x[m])
    print(f" independent calibration: flat_norm={fn!r} max|diff|={diff.max():.3e} "
          f"median|diff|={np.median(diff):.3e} n={m.sum()}")
    return hdr, d


def check_wcs(hdr):
    print("\n== WCS ==")
    d = json.load(open(f"{ROOT}/run/m42-s1/inputs/gaia/wcs_20251212.json"))
    v = json.load(open(f"{ROOT}/run/m42-s1/review/astrometry/verify_20251212.json"))
    crpix = np.array(d["crpix_fits1"])
    cd = np.array(d["cd_rad_per_pixel"]).reshape(2, 2)
    c, e, north = np.array(d["c"]), np.array(d["e"]), np.array(d["north"])
    A = {(t["x_power"], t["y_power"]): t["coefficient"] for t in d["sip_a"]["terms"]}
    B = {(t["x_power"], t["y_power"]): t["coefficient"] for t in d["sip_b"]["terms"]}

    def world(x0, y0):  # full-frame 0-based
        u, w = x0 + 1 - crpix[0], y0 + 1 - crpix[1]
        uu = u + sum(a * u ** i * w ** j for (i, j), a in A.items())
        ww = w + sum(b * u ** i * w ** j for (i, j), b in B.items())
        de, dn = cd[0, 0] * uu + cd[0, 1] * ww, cd[1, 0] * uu + cd[1, 1] * ww
        p = c + de * e + dn * north
        p /= np.linalg.norm(p)
        return np.degrees(np.arctan2(p[1], p[0])), np.degrees(np.arcsin(p[2]))

    worst = 0.0
    for g in v["checks"]["grid"]:
        ra, dec = world(g["x"], g["y"])
        cd_ = np.cos(np.radians(g["dec_deg"]))
        worst = max(worst, np.hypot((ra - g["ra_deg"]) * cd_, dec - g["dec_deg"]) * 3600)
    print(f" wcs JSON decode vs verify grid: max {worst:.3e} arcsec")
    print(" crop CRPIX = full CRPIX - (1792,384):",
          tuple(np.round(crpix - [X0, Y0], 6)), "-> header",
          (hdr["CRPIX1"], hdr["CRPIX2"]))
    w = WCS(hdr)
    ra, dec = world(X0, Y0)  # full-frame 0-based corner = crop FITS (1,1)
    got = w.pixel_to_world_values(0, 0)  # astropy 0-based
    cd_ = np.cos(np.radians(dec))
    print(f" astropy(crop 0,0) vs JSON{tuple(np.round([X0, Y0], 0))}: "
          f"{np.hypot((got[0]-ra)*cd_, got[1]-dec)*3600:.3e} arcsec")
    # inverse SIP absent (AP/BP=0): test the actual forward->inverse round trip
    worst = 0.0
    for x, y in [(0, 0), (255, 0), (0, 255), (255, 255), (128, 128), (200, 50)]:
        ra2, dec2 = w.pixel_to_world_values(x, y)
        bx, by = w.world_to_pixel_values(ra2, dec2)
        worst = max(worst, float(np.hypot(bx - x, by - y)))
    print(f" AP/BP={w.sip.ap_order}/{w.sip.bp_order}; pixel->world->pixel "
          f"max err={worst:.4f} px over 6 points")


def check_astro():
    print("\n== astrometry residuals ==")
    cat = {r["source_id"]: r for r in csv.DictReader(
        open(f"{ROOT}/run/m42-s1/inputs/gaia/stars.csv"))}
    fit = json.load(open(f"{ROOT}/run/m42-s1/inputs/gaia/fit_20251212.json"))
    print(" catalogue rows", len(cat), "-> fit n_gaia_total", fit["n_gaia_total"],
          "n_gaia_in_frame", fit["n_gaia_in_frame"])
    print(" fit.json train/holdout n/rms:",
          (fit["matched_train_unclipped"]["n"], fit["matched_train_unclipped"]["rms_px"]),
          (fit["matched_holdout"]["n"], fit["matched_holdout"]["rms_px"]))
    res = list(csv.DictReader(open(f"{ROOT}/run/m42-s1/inputs/gaia/residuals_20251212.csv")))
    for split in ("train", "holdout"):
        a = np.array([float(r["r_px"]) for r in res if r["split"] == split])
        print(f" csv {split}: n={len(a)} rms={np.sqrt(np.mean(a**2)):.6f} "
              f"median={np.median(a):.4f} frac>0.5px={np.mean(a>0.5):.4f} max={a.max():.4f}")
    print(" arcsec = px *", fit["detector_pixel_scale_arcsec"])


def check_gaia_peaks():
    print("\n== Gaia positions on pixels (deliverable crop) ==")
    cat = {r["source_id"]: r for r in csv.DictReader(
        open(f"{ROOT}/run/m42-s1/inputs/gaia/stars.csv"))}
    d = json.load(open(f"{ROOT}/run/m42-s1/inputs/gaia/wcs_20251212.json"))
    crpix = np.array(d["crpix_fits1"])
    cd = np.array(d["cd_rad_per_pixel"]).reshape(2, 2)
    c, e, north = np.array(d["c"]), np.array(d["e"]), np.array(d["north"])
    A = {(t["x_power"], t["y_power"]): t["coefficient"] for t in d["sip_a"]["terms"]}
    B = {(t["x_power"], t["y_power"]): t["coefficient"] for t in d["sip_b"]["terms"]}
    dt = fit_dt = 2025.94573544883 - 2016.0
    del fit_dt

    def pix(ra, dec):
        x = np.array([crpix[0], crpix[1]])
        for _ in range(60):
            u, w = x[0] + 1 - crpix[0], x[1] + 1 - crpix[1]
            uu = u + sum(a * u ** i * w ** j for (i, j), a in A.items())
            ww = w + sum(b * u ** i * w ** j for (i, j), b in B.items())
            de, dn = cd[0, 0] * uu + cd[0, 1] * ww, cd[1, 0] * uu + cd[1, 1] * ww
            p = c + de * e + dn * north
            p /= np.linalg.norm(p)
            r0, d0 = np.degrees(np.arctan2(p[1], p[0])), np.degrees(np.arcsin(p[2]))
            cosd = np.cos(np.radians(dec))
            resid = np.array([(r0 - ra) * cosd, d0 - dec])
            if np.hypot(*resid) * 3600 < 1e-9:
                break
            J = np.zeros((2, 2))
            for k in range(2):
                xp = x.copy(); xp[k] += 0.25
                u, w = xp[0] + 1 - crpix[0], xp[1] + 1 - crpix[1]
                uu = u + sum(a * u ** i * w ** j for (i, j), a in A.items())
                ww = w + sum(b * u ** i * w ** j for (i, j), b in B.items())
                de, dn = cd[0, 0] * uu + cd[0, 1] * ww, cd[1, 0] * uu + cd[1, 1] * ww
                p = c + de * e + dn * north; p /= np.linalg.norm(p)
                r1, d1 = np.degrees(np.arctan2(p[1], p[0])), np.degrees(np.arcsin(p[2]))
                J[:, k] = [(r1 - r0) * cosd / 0.25, (d1 - d0) / 0.25]
            x = x - np.linalg.solve(J, resid)
        return x

    with fits.open(RAW) as h:
        raw = np.array(h[0].data[Y0:Y0 + N, X0:X0 + N], dtype=np.float64)
    inside, localmax, sat = 0, 0, 0
    for s in cat.values():
        ra = float(s["ra_deg"]) + float(s["pmra_cosdec_mas_yr"]) * dt / 3.6e6 / np.cos(np.radians(float(s["dec_deg"])))
        dec = float(s["dec_deg"]) + float(s["pmdec_mas_yr"]) * dt / 3.6e6
        x, y = pix(ra, dec)
        cx, cy = x - X0, y - Y0
        if not (3 <= cx < N - 3 and 3 <= cy < N - 3):
            continue
        inside += 1
        xi, yi = int(round(cx)), int(round(cy))
        sub = raw[yi - 2:yi + 3, xi - 2:xi + 3]
        if raw[yi, xi] >= sub.max() - 1e-9:
            localmax += 1
        if raw[yi, xi] >= 65535.0:
            sat += 1
    print(f" projected inside crop: {inside}; on saturated pixel: {sat}; "
          f"local 5x5 maximum: {localmax}")


if __name__ == "__main__":
    hdr, _ = check_fits()
    check_wcs(hdr)
    check_astro()
    check_gaia_peaks()
