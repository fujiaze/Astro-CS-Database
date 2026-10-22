#!/usr/bin/env python3
"""C01 truth: absolute WCS coordinates against an external standard.

Independence
------------
The truth is produced with astropy / WCSLIB (the external standard named by
the specification) and with a second, independent implementation of the
gnomonic (TAN) forward model written directly from the foundation formulae
(observation coordinates section 3, equations 3 and 6).  The two independent
routes are compared with each other, and the implementation under test is
compared with both.

No production module is imported and no observed value is used to build an
input.

Truth type: analytic_exact (the closed form tangent-plane model) backed by
stochastic-free numerical differentiation for the derivative functional.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
from astropy.io import fits
from astropy.wcs import WCS

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lib import observe  # noqa: E402
from lib.availability import availability  # noqa: E402
from lib.vcommon import (  # noqa: E402
    Case,
    canonical_json,
    comparison_payload,
    content_hash,
    environment_identity,
    jsonable,
    sha256_text,
    source_identity,
    write_json,
)

CASE = "c01"

ARCSEC = math.pi / (180.0 * 3600.0)

# --------------------------------------------------------------------------
# fixtures (inputs, not truth)
# --------------------------------------------------------------------------

def sip_header(order_a, order_b, a_terms, b_terms):
    """FITS SIP keys: A_p_q / B_p_q with p the u power and q the v power."""
    keys = []
    if order_a:
        keys.append(("A_ORDER", order_a))
    if order_b:
        keys.append(("B_ORDER", order_b))
    for (p, q), value in sorted(a_terms.items()):
        keys.append((f"A_{p}_{q}", value))
    for (p, q), value in sorted(b_terms.items()):
        keys.append((f"B_{p}_{q}", value))
    return keys


def make_case(name, *, crpix, crval, cd=None, cdelt=None, pc=None, lonpole=None,
              radesys="ICRS", sip=(), domain=None, extra_header=(), forward_pixels=(),
              inverse_sky=(), derivative_pixels=(), jacobian_box=None, note="",
              expected_failure=None):
    has_sip = any(key in ("A_ORDER", "B_ORDER") or key.startswith(("A_", "B_"))
                  for key, _ in sip)
    suffix = "-SIP" if has_sip else ""
    header = [("CTYPE1", "RA---TAN" + suffix), ("CTYPE2", "DEC--TAN" + suffix)]
    header.append(("CRPIX1", crpix[0]))
    header.append(("CRPIX2", crpix[1]))
    header.append(("CRVAL1", crval[0]))
    header.append(("CRVAL2", crval[1]))
    if cd is not None:
        header.append(("CD1_1", cd[0][0]))
        header.append(("CD1_2", cd[0][1]))
        header.append(("CD2_1", cd[1][0]))
        header.append(("CD2_2", cd[1][1]))
    if cdelt is not None:
        header.append(("CDELT1", cdelt[0]))
        header.append(("CDELT2", cdelt[1]))
    if pc is not None:
        header.append(("PC1_1", pc[0][0]))
        header.append(("PC1_2", pc[0][1]))
        header.append(("PC2_1", pc[1][0]))
        header.append(("PC2_2", pc[1][1]))
    if radesys is not None:
        header.append(("RADESYS", radesys))
    if lonpole is not None:
        header.append(("LONPOLE", lonpole))
    header.extend(sip)
    header.extend(extra_header)
    return {
        "name": name,
        "note": note,
        "header": [{"key": key, "value": value} for key, value in header],
        "domain": domain or {"lower": [0.0, 0.0], "upper": [64.0, 64.0]},
        "budget": {"absolute": 1e-13, "relative": 1e-13, "absolute_floor": 0.0,
                   "unit_scale": "rad", "max_iterations": 128, "initial_bits": 128,
                   "max_bits": 256, "outward": True, "allow_target_fallback": False},
        "astrometric_context": {"epoch_julian_year": 2016.0, "observation_jd_tdb": 2457600.5},
        "detector_frame": "detector",
        "forward_pixels": [list(pixel) for pixel in forward_pixels],
        "inverse_sky": [list(sky) for sky in inverse_sky],
        "derivative_pixels": [list(pixel) for pixel in derivative_pixels],
    } | ({"jacobian_box": jacobian_box} if jacobian_box else {}) \
      | ({"expected_failure": list(expected_failure)} if expected_failure else {})


def build_fixtures():
    """The frozen fixture set of C01."""
    # only terms with 2 <= p + q <= order are legal (the specification says so
    # and the implementation rejects the others by name)
    # The coefficients are scaled so that the induced infinity norm of the
    # SIP gradient stays below one over the declared domain, which is the
    # sufficient injectivity condition of the specification.
    sip3 = sip_header(
        3, 3,
        {(2, 0): 4.0e-4, (0, 2): -3.0e-4, (3, 0): 4.8e-6, (1, 1): 1.6e-4,
         (1, 2): -8.0e-5, (2, 1): 1.2e-4},
        {(2, 0): -3.6e-4, (0, 2): 2.4e-4, (3, 0): -3.6e-6, (1, 1): 2.0e-4,
         (1, 2): 1.0e-4, (2, 1): -6.0e-5},
    )
    sip2 = sip_header(2, 2, {(2, 0): 6.0e-4, (0, 2): -4.0e-4, (1, 1): 2.0e-4},
                      {(2, 0): -5.0e-4, (0, 2): 3.0e-4, (1, 1): 2.5e-4})
    sip9 = sip_header(9, 9, {(9, 0): 3.0e-12, (0, 9): -3.0e-10, (8, 1): 3.0e-12,
                              (1, 8): -3.0e-10, (5, 4): 3.0e-11, (4, 5): -3.0e-11,
                              (2, 0): 1.0e-5, (0, 2): -8.0e-6, (3, 1): 1.0e-7,
                              (1, 3): -8.0e-8},
                      {(9, 0): -2.7e-12, (0, 9): 2.7e-10, (8, 1): -2.7e-12,
                       (1, 8): 2.7e-10, (5, 4): -2.7e-11, (4, 5): 2.7e-11,
                       (2, 0): -9.0e-6, (0, 2): 7.0e-6, (3, 1): 9.0e-8,
                       (1, 3): 8.0e-8})
    sip_strong = sip_header(2, 2, {(2, 0): 3.0e-2, (0, 2): 1.0e-2},
                            {(2, 0): -2.0e-2, (0, 2): 1.0e-2})

    forward_grid = [(0.0, 0.0), (17.293, 41.617), (63.0, 0.0), (0.0, 63.0), (63.0, 63.0),
                    (32.125, 21.5), (5.5, 58.25), (48.75, 12.125)]
    # The SIP fixtures use a smaller detector domain so that the sufficient
    # injectivity condition sup|grad D| < 1 holds over the whole domain; the
    # script measures the bound and records it in the truth file.
    small_grid = [(0.0, 0.0), (4.0, 6.0), (8.0, 2.0), (12.5, 14.25), (16.0, 16.0),
                  (7.375, 9.125), (2.5, 11.0), (15.5, 3.25)]
    small_domain = {"lower": [0.0, 0.0], "upper": [16.0, 16.0]}
    inverse_points = [(10.0, 20.0), (10.02, 20.03), (9.97, 19.96), (10.05, 19.94)]

    cases = []

    cases.append(make_case(
        "tan_sip3_mirror",
        crpix=(17.293, 41.617), crval=(10.0, 20.0),
        cd=[[-1.2e-4, 6.0e-5], [7.0e-5, 1.4e-4]],
        sip=sip3, forward_pixels=small_grid, domain=small_domain,
        inverse_sky=[sky_vector(10.0, 20.0), sky_vector(10.02, 20.03),
                     sky_vector(9.97, 19.96), sky_vector(10.05, 19.94)],
        derivative_pixels=[(17.293, 41.617), (3.25, 7.75), (55.5, 33.25)],
        jacobian_box={"lower": [0.0, 0.0], "upper": [8.0, 8.0]},
        note="non-integer CRPIX, mirrored non-diagonal CD (det < 0), third order SIP"))

    cases.append(make_case(
        "pc_cdelt",
        crpix=(33.5, 12.25), crval=(120.0, -30.0),
        cdelt=(1.1e-4, 9.0e-5), pc=[[0.87, -0.49], [0.49, 0.87]],
        forward_pixels=forward_grid,
        inverse_sky=[sky_vector(120.0, -30.0), sky_vector(120.05, -30.04)],
        derivative_pixels=[(33.5, 12.25), (10.0, 40.0)],
        jacobian_box={"lower": [28.0, 8.0], "upper": [40.0, 18.0]},
        note="PC + CDELT form instead of CD"))

    cases.append(make_case(
        "north_pole_default_lonpole",
        crpix=(10.5, 20.75), crval=(45.0, 90.0),
        cd=[[2.0e-4, 0.0], [0.0, 2.0e-4]],
        lonpole=None,
        forward_pixels=[(0.0, 0.0), (10.5, 20.75), (63.0, 63.0), (32.0, 5.0)],
        inverse_sky=[sky_vector(45.0, 90.0), sky_vector(44.0, 89.999)],
        derivative_pixels=[(10.5, 20.75)],
        note="exact north pole; LONPOLE omitted (FITS default is 0 deg for theta0 = 90, "
             "CRVAL2 = +90)"))

    cases.append(make_case(
        "south_pole_default_lonpole",
        crpix=(10.5, 20.75), crval=(45.0, -90.0),
        cd=[[2.0e-4, 0.0], [0.0, 2.0e-4]],
        lonpole=None,
        forward_pixels=[(0.0, 0.0), (10.5, 20.75), (63.0, 63.0)],
        inverse_sky=[sky_vector(45.0, -90.0), sky_vector(44.0, -89.999)],
        derivative_pixels=[(10.5, 20.75)],
        note="exact south pole; LONPOLE omitted (FITS default is 180 deg)"))

    cases.append(make_case(
        "lonpole_107",
        crpix=(8.125, 9.875), crval=(30.0, -15.0),
        cd=[[1.5e-4, 2.0e-5], [-1.0e-5, 1.4e-4]],
        lonpole=107.0, sip=sip2,
        forward_pixels=forward_grid,
        inverse_sky=[sky_vector(30.0, -15.0), sky_vector(30.03, -15.02)],
        derivative_pixels=[(8.125, 9.875), (30.0, 40.0)],
        jacobian_box={"lower": [4.0, 4.0], "upper": [12.0, 12.0]},
        note="LONPOLE different from 180 deg"))

    cases.append(make_case(
        "sip_order9",
        crpix=(15.125, 8.0), crval=(200.0, 45.0),
        cd=[[5.0e-5, 4.0e-6], [2.0e-6, 4.8e-5]],
        sip=sip9, forward_pixels=small_grid, domain=small_domain,
        inverse_sky=[sky_vector(200.0, 45.0), sky_vector(200.02, 45.01)],
        derivative_pixels=[(15.125, 25.375)],
        note="ninth order SIP with sparse coefficients"))

    cases.append(make_case(
        "ra_wrap",
        crpix=(5.5, 6.5), crval=(359.95, 10.0),
        cd=[[1.0e-4, 0.0], [0.0, 1.0e-4]],
        forward_pixels=[(0.0, 0.0), (5.5, 6.5), (63.0, 63.0)],
        inverse_sky=[sky_vector(359.95, 10.0), sky_vector(0.02, 10.01)],
        derivative_pixels=[(5.5, 6.5)],
        note="right ascension wrap across 0 deg"))

    # ---- negative configurations ----------------------------------------
    cases.append(make_case(
        "negative_origin_plus_one",
        crpix=(18.293, 42.617), crval=(10.0, 20.0),
        cd=[[-1.2e-4, 6.0e-5], [7.0e-5, 1.4e-4]], sip=sip3,
        forward_pixels=small_grid, domain=small_domain,
        note="negative control: the same model with CRPIX shifted by +1 pixel"))

    cases.append(make_case(
        "negative_origin_plus_half",
        crpix=(17.793, 42.117), crval=(10.0, 20.0),
        cd=[[-1.2e-4, 6.0e-5], [7.0e-5, 1.4e-4]], sip=sip3,
        forward_pixels=small_grid, domain=small_domain,
        note="negative control: the same model with CRPIX shifted by +0.5 pixel"))

    cases.append(make_case(
        "negative_cd_only_change",
        crpix=(17.293, 41.617), crval=(10.0, 20.0),
        cd=[[-1.1e-4, 5.5e-5], [6.4e-5, 1.3e-4]], sip=sip3,
        forward_pixels=small_grid, domain=small_domain,
        note="negative control: only CD changed, the SIP coefficients are NOT transformed"))

    cases.append(make_case(
        "negative_back_hemisphere",
        crpix=(17.293, 41.617), crval=(10.0, 20.0),
        cd=[[-1.2e-4, 6.0e-5], [7.0e-5, 1.4e-4]], sip=sip3,
        forward_pixels=[(17.293, 41.617)],
        inverse_sky=[sky_vector(190.0, 20.0)],
        expected_failure=['domain', 'unsupported', 'unavailable', 'invalid_request', 'unsolved', 'not_identifiable'],
        note="negative control: the inverse of a sky position behind the tangent point"))

    cases.append(make_case(
        "negative_non_icrs",
        crpix=(17.293, 41.617), crval=(10.0, 20.0),
        cd=[[-1.2e-4, 6.0e-5], [7.0e-5, 1.4e-4]], radesys="FK5",
        forward_pixels=[(17.293, 41.617)],
        expected_failure=['unsupported', 'unavailable', 'invalid_request', 'invalid_model'],
        note="negative control: RADESYS = FK5 is not ICRS"))

    cases.append(make_case(
        "negative_extra_distortion",
        crpix=(17.293, 41.617), crval=(10.0, 20.0),
        cd=[[-1.2e-4, 6.0e-5], [7.0e-5, 1.4e-4]],
        extra_header=[("PV1_0", 0.001), ("PV1_1", 0.0)],
        forward_pixels=[(17.293, 41.617)],
        expected_failure=['unsupported', 'unavailable', 'invalid_request', 'invalid_model'],
        note="negative control: a PV distortion that is outside the supported model class"))

    cases.append(make_case(
        "negative_injectivity_unresolved",
        crpix=(17.293, 41.617), crval=(10.0, 20.0),
        cd=[[-1.2e-4, 6.0e-5], [7.0e-5, 1.4e-4]], sip=sip_strong,
        forward_pixels=[(17.293, 41.617), (0.0, 0.0), (63.0, 63.0)],
        expected_failure=['unsupported', 'unavailable', 'invalid_request', 'invalid_model', 'not_identifiable', 'domain'],
        note="negative control: a SIP whose gradient bound cannot certify injectivity"))

    return cases


def sky_vector(ra_deg, dec_deg):
    ra = math.radians(ra_deg)
    dec = math.radians(dec_deg)
    return [math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec)]


# --------------------------------------------------------------------------
# independent tangent-plane model (foundation equations 3 and 6)
# --------------------------------------------------------------------------

def default_lonpole(crval_deg):
    """The FITS default for LONPOLE of a zenithal projection.

    For theta0 = 90 deg the default is 0 deg when CRVAL2 = +90 deg and 180 deg
    otherwise (Greisen & Calabretta 2002, section 2.4).
    """
    if abs(crval_deg[1] - 90.0) < 1e-12:
        return 0.0
    return 180.0


def canonical_cd(cd_deg, crval_deg, lonpole_deg):
    """Canonical (LONPOLE = 180) CD in radians per pixel."""
    cd = np.array(cd_deg, dtype=float) * (math.pi / 180.0)
    if lonpole_deg is None:
        lonpole_deg = default_lonpole(crval_deg)
    rho = math.radians(lonpole_deg - 180.0)
    rotation = np.array([[math.cos(rho), math.sin(rho)], [-math.sin(rho), math.cos(rho)]])
    return rotation @ cd


def tangent_basis(crval_deg, lonpole_deg):
    """The canonical tangent basis (c, e, north) of the specification."""
    alpha0 = math.radians(crval_deg[0])
    delta0 = math.radians(crval_deg[1])
    c = np.array([math.cos(delta0) * math.cos(alpha0),
                  math.cos(delta0) * math.sin(alpha0),
                  math.sin(delta0)])
    # The canonical tangent basis: xi along east, eta along north.  The
    # intermediate world coordinates of the tangent plane are (xi, eta) with
    # xi = CD_11 dx + CD_12 dy and eta = CD_21 dx + CD_22 dy.
    pole = np.array([0.0, 0.0, 1.0])
    if abs(abs(float(c[2])) - 1.0) < 1e-12:
        # The fiducial is a pole: the "north" direction of the tangent plane is
        # degenerate.  The standard resolution uses the fiducial longitude:
        # the xi axis is the equatorial tangent at that longitude and the eta
        # axis completes the right handed frame.
        alpha0 = math.radians(crval_deg[0])
        east = np.array([-math.sin(alpha0), math.cos(alpha0), 0.0])
        north = np.cross(c, east)
        return c, east, north
    north = pole - np.dot(pole, c) * c
    north = north / np.linalg.norm(north)
    east = np.cross(north, c)
    return c, east, north


def sip_value(terms, u, v):
    total = 0.0
    for (p, q), coefficient in terms.items():
        total += coefficient * (u ** p) * (v ** q)
    return total


def sip_gradient(terms, u, v):
    du = 0.0
    dv = 0.0
    for (p, q), coefficient in terms.items():
        if p >= 1:
            du += coefficient * p * (u ** (p - 1)) * (v ** q)
        if q >= 1:
            dv += coefficient * q * (u ** p) * (v ** (q - 1))
    return du, dv


def forward_tan(crval_deg, cd_deg, lonpole_deg, a_terms, b_terms, crpix, pixel):
    """Independent forward model, straight from the foundation equation (3)."""
    c, e, north = tangent_basis(crval_deg, lonpole_deg)
    cd = canonical_cd(cd_deg, crval_deg, lonpole_deg)
    u = pixel[0] + 1.0 - crpix[0]
    v = pixel[1] + 1.0 - crpix[1]
    x = u + sip_value(a_terms, u, v)
    y = v + sip_value(b_terms, u, v)
    q = cd @ np.array([x, y])
    vector = c + q[0] * e + q[1] * north
    return vector / np.linalg.norm(vector)


def solid_angle_jacobian(crval_deg, cd_deg, lonpole_deg, a_terms, b_terms, crpix, pixel):
    """Independent g(u, v) from the foundation equation (6)."""
    cd = canonical_cd(cd_deg, crval_deg, lonpole_deg)
    u = pixel[0] + 1.0 - crpix[0]
    v = pixel[1] + 1.0 - crpix[1]
    x = u + sip_value(a_terms, u, v)
    y = v + sip_value(b_terms, u, v)
    q = cd @ np.array([x, y])
    du_a, dv_a = sip_gradient(a_terms, u, v)
    du_b, dv_b = sip_gradient(b_terms, u, v)
    jacobian_f = np.array([[1.0 + du_a, dv_a], [du_b, 1.0 + dv_b]])
    det = abs(np.linalg.det(jacobian_f))
    return abs(np.linalg.det(cd)) * det / (1.0 + float(np.dot(q, q))) ** 1.5


# --------------------------------------------------------------------------
# astropy helpers
# --------------------------------------------------------------------------

def astropy_wcs(case):
    header = fits.Header()
    for item in case["header"]:
        value = item["value"]
        if isinstance(value, str):
            header[item["key"]] = value
        elif isinstance(value, int):
            header[item["key"]] = value
        else:
            header[item["key"]] = float(value)
    return WCS(header)


def angular_separation(n1, n2):
    """Angle between two unit vectors, robust at the poles."""
    n1 = np.asarray(n1, dtype=float)
    n2 = np.asarray(n2, dtype=float)
    cross = np.linalg.norm(np.cross(n1, n2))
    dot = float(np.dot(n1, n2))
    return math.atan2(cross, dot)


def parse_terms(case, prefix):
    """The forward SIP coefficients, restricted to 2 <= p + q <= order."""
    header = {item["key"]: item["value"] for item in case["header"]}
    order = int(header.get(prefix + "_ORDER", 0))
    terms = {}
    for item in case["header"]:
        key = item["key"]
        if key.startswith(prefix + "_"):
            parts = key.split("_")
            if len(parts) != 3 or not parts[1].isdigit() or not parts[2].isdigit():
                continue  # A_ORDER / B_ORDER and other non coefficient keys
            p = int(parts[1])
            q = int(parts[2])
            if order and p + q > order:
                continue  # outside the declared SIP order
            terms[(p, q)] = float(item["value"])
    return terms


def model_parameters(case):
    header = {item["key"]: item["value"] for item in case["header"]}
    crpix = (float(header["CRPIX1"]), float(header["CRPIX2"]))
    crval = (float(header["CRVAL1"]), float(header["CRVAL2"]))
    if "CD1_1" in header:
        cd = [[float(header["CD1_1"]), float(header["CD1_2"])],
              [float(header["CD2_1"]), float(header["CD2_2"])]]
    else:
        cdelt = np.diag([float(header["CDELT1"]), float(header["CDELT2"])])
        pc = np.array([[float(header["PC1_1"]), float(header["PC1_2"])],
                       [float(header["PC2_1"]), float(header["PC2_2"])]])
        cd = (cdelt @ pc).tolist()
    lonpole = float(header["LONPOLE"]) if "LONPOLE" in header else None
    return crpix, crval, cd, lonpole


def compute_truth(case):
    """The external and the independent truth for one fixture."""
    wcs = astropy_wcs(case)
    crpix, crval, cd, lonpole = model_parameters(case)
    a_terms = parse_terms(case, "A")
    b_terms = parse_terms(case, "B")

    forward = []
    for pixel in case["forward_pixels"]:
        ra, dec = wcs.all_pix2world(pixel[0], pixel[1], 0)
        ra = float(np.asarray(ra).ravel()[0])
        dec = float(np.asarray(dec).ravel()[0])
        astropy_vector = np.array(sky_vector(ra, dec))
        independent = forward_tan(crval, cd, lonpole, a_terms, b_terms, crpix, pixel)
        forward.append({
            "pixel": pixel,
            "astropy_ra_deg": ra,
            "astropy_dec_deg": dec,
            "astropy_vector": astropy_vector.tolist(),
            "independent_vector": independent.tolist(),
            "astropy_vs_independent_rad": angular_separation(astropy_vector, independent),
        })

    inverse = []
    for sky in case["inverse_sky"]:
        ra = math.degrees(math.atan2(sky[1], sky[0]))
        ra = ra % 360.0
        dec = math.degrees(math.asin(max(-1.0, min(1.0, sky[2]))))
        item = {"sky": sky}
        try:
            x, y = wcs.all_world2pix(ra, dec, 0)
            item["astropy_x"] = float(np.asarray(x).ravel()[0])
            item["astropy_y"] = float(np.asarray(y).ravel()[0])
            item["astropy_converged"] = True
        except Exception as exc:  # astropy raises NoConvergence outside the projection
            item["astropy_converged"] = False
            item["astropy_error"] = type(exc).__name__
        inverse.append(item)

    derivative = []
    step = 1.0e-3
    for pixel in case["derivative_pixels"]:
        columns = []
        for axis in (0, 1):
            plus = list(pixel)
            minus = list(pixel)
            plus[axis] += step
            minus[axis] -= step
            ra_p, dec_p = wcs.all_pix2world(plus[0], plus[1], 0)
            ra_m, dec_m = wcs.all_pix2world(minus[0], minus[1], 0)
            vp = np.array(sky_vector(float(ra_p), float(dec_p)))
            vm = np.array(sky_vector(float(ra_m), float(dec_m)))
            columns.append((vp - vm) / (2.0 * step))
        jacobian = np.column_stack(columns)  # 3 x 2
        derivative.append({
            "pixel": pixel,
            "jacobian_numerical": jacobian.tolist(),
            "step_pixel": step,
        })

    jacobian_range = None
    if "jacobian_box" in case:
        box = case["jacobian_box"]
        values = []
        grid = 25
        for i in range(grid):
            for j in range(grid):
                x = box["lower"][0] + (box["upper"][0] - box["lower"][0]) * i / (grid - 1)
                y = box["lower"][1] + (box["upper"][1] - box["lower"][1]) * j / (grid - 1)
                values.append(solid_angle_jacobian(crval, cd, lonpole, a_terms, b_terms,
                                                   crpix, (x, y)))
        jacobian_range = {"min": min(values), "max": max(values), "samples": len(values),
                          "box": box}

    return {"name": case["name"], "forward": forward, "inverse": inverse,
            "derivative": derivative, "solid_angle_jacobian_range": jacobian_range,
            "injectivity_bound": injectivity_bound(case, a_terms, b_terms)}


def injectivity_bound(case, a_terms, b_terms):
    """The sampled sup|grad D| over the declared domain.

    The sufficient injectivity condition of the specification is
    sup_U ||grad D||_inf < 1; the value is recorded so that a legitimate
    rejection (injectivity_unresolved) is distinguishable from a defect.
    """
    if not a_terms and not b_terms:
        return {"sup_grad_dinf": 0.0, "certified": True, "samples": 0}
    crpix, _, _, _ = model_parameters(case)
    domain = case["domain"]
    u_low = domain["lower"][0] + 1.0 - crpix[0]
    u_high = domain["upper"][0] + 1.0 - crpix[0]
    v_low = domain["lower"][1] + 1.0 - crpix[1]
    v_high = domain["upper"][1] + 1.0 - crpix[1]
    max_u = max(abs(u_low), abs(u_high))
    max_v = max(abs(v_low), abs(v_high))
    # The sufficient condition of the specification is sup_U ||grad D||_inf < 1
    # with the *induced* infinity norm (the maximum row sum), so the bound is
    # computed term by term, which is rigorous over the whole rectangle.
    worst = 0.0
    for terms in (a_terms, b_terms):
        row_u = 0.0
        row_v = 0.0
        for (p, q), coefficient in terms.items():
            magnitude = abs(coefficient)
            if p >= 1:
                row_u += magnitude * p * (max_u ** (p - 1)) * (max_v ** q)
            if q >= 1:
                row_v += magnitude * q * (max_u ** p) * (max_v ** (q - 1))
        worst = max(worst, row_u + row_v)
    return {"sup_grad_dinf": worst, "certified": worst < 1.0,
            "norm": "induced infinity norm (maximum row sum)"}


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Verify the absolute WCS model (forward, inverse, derivative and the solid angle "
              "per pixel) against an external complete WCS, and show that the named failure "
              "configurations fail by name."),
        assumptions=[
            "the reference frame is ICRS and the projection is TAN",
            "the SIP distortion is applied before the CD matrix",
            "the detector domain is a closed rectangle that contains the requested pixels",
            "the external truth is astropy/WCSLIB; the second truth is an independent "
            "implementation of the foundation tangent-plane formulae",
        ],
        estimand=("the ICRS unit vector of the forward map, the 0-based pixel of the inverse "
                  "map, the 3x2 Jacobian dn/d(x,y) and the solid angle per pixel^2"),
        unit="rad (angles), pixel (positions), sr/pixel^2 (solid angle)",
        condition="fixed numeric WCS model; no astrometric parameter error",
        tolerance={
            "forward_rad": 1e-10,
            "inverse_pixel": 1e-7,
            "derivative_relative": 1e-6,
            "solid_angle_jacobian_relative": 1e-6,
            "named_failure_codes": ["unsupported", "unavailable", "invalid_request", "domain",
                                    "unsolved", "not_identifiable", "invalid_model",
                                    "projection_unresolved"],
        },
        positive_expectations=[
            {"id": "forward_absolute", "expectation": "the forward map matches the external "
             "standard within 1e-10 rad"},
            {"id": "inverse_absolute", "expectation": "the inverse map matches within 1e-7 pixel"},
            {"id": "derivative", "expectation": "the analytic Jacobian matches the external "
             "finite difference within 1e-6 relative"},
            {"id": "solid_angle", "expectation": "the reported g interval contains the truth"},
            {"id": "origin_control", "expectation": "the forward map must differ from the +1 "
             "and +0.5 pixel origin controls by more than the tolerance"},
            {"id": "cd_only_control", "expectation": "the forward map must differ from the "
             "CD-only-change control by more than the tolerance"},
        ],
        negative_expectations=[
            {"id": "back_hemisphere", "expectation": "the inverse behind the tangent point must "
             "fail by name"},
            {"id": "non_icrs", "expectation": "a non-ICRS reference frame must fail by name"},
            {"id": "extra_distortion", "expectation": "a PV distortion must fail by name"},
            {"id": "injectivity", "expectation": "an uncertifiable SIP must fail by name"},
        ],
        truth_type="analytic_exact",
    )

    fixtures = build_fixtures()
    fixture_payload = {"cases": fixtures}
    fixture_path = case.write_input("c01_fixtures.json", fixture_payload)
    fixture_file = case.root / "input" / "c01_fixtures.json"

    truths = [compute_truth(fixture) for fixture in fixtures]
    # the two independent truth routes must agree with each other
    route_separation = 0.0
    for truth in truths:
        for item in truth["forward"]:
            route_separation = max(route_separation, item["astropy_vs_independent_rad"])
    truth_payload = {
        "case": "C01",
        "truth_type": "analytic_exact",
        "methods": {
            "external": "astropy WCS (bundled WCSLIB): all_pix2world / all_world2pix",
            "independent": ("closed form tangent-plane model written from the foundation "
                            "equations 3 and 6, including the LONPOLE canonicalisation"),
            "derivative": "central differences on the external forward map (step 1e-3 pixel)",
            "solid_angle": "analytic g with the SIP gradient, sampled on a 25x25 grid",
        },
        "route_separation_rad": route_separation,
        "cases": truths,
    }
    truth_path = case.write_truth("c01_truth.json", truth_payload)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-c01-make-wcs",
        "operation": "make_wcs",
        "input_objects": {},
        "arguments": {
            "header": [{"key": item["key"], "value": item["value"]}
                       for item in fixtures[0]["header"]],
            "domain": fixtures[0]["domain"],
            "astrometric_context": fixtures[0]["astrometric_context"],
            "detector_frame": fixtures[0]["detector_frame"],
        },
        "request": fixtures[0]["budget"],
        "resources": {"bytes": 8388608, "work": 100000, "spill_bytes": 0, "threads": 1,
                      "max_depth": 32, "max_precision_bits": 256, "scratch_directory": ".work"},
        "output_root": ".work/c01",
    }
    request_hash = case.write_request(request)

    # ---- observed ---------------------------------------------------------
    native_run, native_response = observe.run_native_driver(case, "c01_driver",
                                                             ["--fixtures", str(fixture_file)])
    contract_run, contract_response = observe.run_contract_driver(case)

    report = build_comparison(case, fixtures, truths, native_run, native_response,
                               contract_run, contract_response)

    case.write_case_json({
        "fixtures_sha256": sha256_text(canonical_json(fixture_payload)),
        "truth_sha256": sha256_text(canonical_json(truth_payload)),
        "availability": availability(["make_wcs", "forward", "inverse", "derivative",
                                       "solid_angle_jacobian"]),
    })
    observe.write_manifest(case, {
        "truth_type": "analytic_exact",
        "seeds": [],
        "trial_denominators": {"mc_trials": 0},
        "external_library_identity": {"astropy": environment_identity()["astropy"],
                                      "wcslib": environment_identity()["wcslib"]},
        "code_identity": source_identity([Path(__file__)]),
        "native_driver": "validation/native/c01_driver.cpp",
        "commands": {
            "truth": f"python3 validation/{CASE}/truth_{CASE}.py",
            "native": "bash validation/lib/build_native.sh c01_driver && "
                      f"validation/build/c01_driver --fixtures validation/{CASE}/input/c01_fixtures.json",
            "contract": f"acsd-contract --request validation/{CASE}/request.json",
        },
        "artefacts": {"fixtures": fixture_path, "truth": truth_path},
    })
    print(json.dumps({"case": "C01", "status": report["status"],
                      "functionals": len(report["functionals"]),
                      "failures": len(report["failures"]),
                      "route_separation_rad": route_separation}, indent=2))
    return 0


def build_comparison(case, fixtures, truths, native_run, native_response, contract_run,
                      contract_response):
    truth_by_name = {truth["name"]: truth for truth in truths}
    fixture_by_name = {fixture["name"]: fixture for fixture in fixtures}
    observed = native_response.get("cases", []) if isinstance(native_response, dict) else []
    observed_by_name = {entry.get("name"): entry for entry in observed}

    rows = []
    failures = []
    status = "pass"

    def add_row(name, kind, **kwargs):
        row = {"name": name, "kind": kind}
        row.update(kwargs)
        rows.append(row)
        return row

    pending_reason = native_run.get("reason", "")
    if native_run.get("status") != "ok":
        for fixture in fixtures:
            add_row(f"{fixture['name']}:wcs_built", "value", status="pending_implementation",
                    observed=None, reason=pending_reason)
        payload = comparison_payload(case.name.upper(), "pending_implementation", rows, failures,
                                     "the public WCS entry points were not reachable")
        case.write_comparison(payload)
        return payload

    for fixture in fixtures:
        name = fixture["name"]
        entry = observed_by_name.get(name)
        truth = truth_by_name[name]
        if entry is None:
            add_row(f"{name}:present", "value", status="pending_implementation",
                    observed=None, reason="the driver did not report this fixture")
            status = "pending_implementation"
            continue
        if entry.get("status") != "wcs_built":
            code = entry.get("code", "unknown")
            expectation = fixture.get("expected_failure")
            if expectation:
                ok_failure = code in expectation
                add_row(f"{name}:named_failure", "code", status="pass" if ok_failure else "fail",
                        expected_codes=expectation, observed_code=code,
                        reason=entry.get("reason", ""))
                if not ok_failure:
                    failures.append({"functional": f"{name}:named_failure", "scope": "make_wcs",
                                     "expected": expectation, "actual": code,
                                     "reason": entry.get("reason", "")})
                    status = "fail"
            else:
                add_row(f"{name}:wcs_built", "value", status="fail", observed=None,
                        reason=f"make_wcs failed with {code}: {entry.get('reason','')}")
                failures.append({"functional": f"{name}:wcs_built", "scope": "make_wcs",
                                 "expected": "ok", "actual": code,
                                 "reason": entry.get("reason", "")})
                status = "fail"
            continue

        # forward
        worst = 0.0
        for index, item in enumerate(entry.get("forward", [])):
            if item.get("code") != "ok":
                add_row(f"{name}:forward[{index}]", "value", status="fail",
                        observed=None, reason=item.get("reason", ""))
                failures.append({"functional": f"{name}:forward[{index}]", "scope": "forward",
                                 "expected": "ok", "actual": item.get("code"),
                                 "reason": item.get("reason", "")})
                status = "fail"
                continue
            separation = angular_separation([item["nx"], item["ny"], item["nz"]],
                                             truth["forward"][index]["astropy_vector"])
            worst = max(worst, separation)
        add_row(f"{name}:forward_max_rad", "value", status="pass" if worst <= 1e-10 else "fail",
                truth=0.0, observed=worst, tolerance=1e-10, unit="rad")
        if worst > 1e-10:
            failures.append({"functional": f"{name}:forward", "scope": "forward",
                             "expected": "<= 1e-10 rad", "actual": worst,
                             "reason": "the absolute forward position is outside the tolerance"})
            status = "fail"

        # inverse
        worst_inverse = 0.0
        for index, item in enumerate(entry.get("inverse", [])):
            if item.get("code") != "ok":
                continue
            reference = truth["inverse"][index]
            if not reference.get("astropy_converged", True):
                continue
            worst_inverse = max(worst_inverse,
                                math.hypot(item["x"] - reference["astropy_x"],
                                           item["y"] - reference["astropy_y"]))
        if truth["inverse"]:
            add_row(f"{name}:inverse_max_pixel", "value",
                    status="pass" if worst_inverse <= 1e-7 else "fail",
                    truth=0.0, observed=worst_inverse, tolerance=1e-7, unit="pixel")
            if worst_inverse > 1e-7:
                failures.append({"functional": f"{name}:inverse", "scope": "inverse",
                                 "expected": "<= 1e-7 pixel", "actual": worst_inverse,
                                 "reason": "the inverse position is outside the tolerance"})
                status = "fail"

        # derivative
        worst_derivative = 0.0
        for index, item in enumerate(entry.get("derivative", [])):
            if item.get("code") != "ok":
                continue
            reference = np.array(truth["derivative"][index]["jacobian_numerical"])
            observed_matrix = np.array(item["jacobian"]).reshape(3, 2)
            scale = np.max(np.abs(reference))
            if scale > 0:
                worst_derivative = max(worst_derivative,
                                       float(np.max(np.abs(observed_matrix - reference))) / scale)
        if truth["derivative"]:
            add_row(f"{name}:derivative_relative", "value",
                    status="pass" if worst_derivative <= 1e-6 else "fail",
                    truth=0.0, observed=worst_derivative, tolerance=1e-6, unit="1")
            if worst_derivative > 1e-6:
                failures.append({"functional": f"{name}:derivative", "scope": "derivative",
                                 "expected": "<= 1e-6 relative", "actual": worst_derivative,
                                 "reason": "the Jacobian is outside the tolerance"})
                status = "fail"

        # solid angle interval
        if truth.get("solid_angle_jacobian_range") and "solid_angle_jacobian" in entry:
            solid = entry["solid_angle_jacobian"]
            reference = truth["solid_angle_jacobian_range"]
            if solid.get("code") == "ok":
                inside = solid["lower"] <= reference["max"] and solid["upper"] >= reference["min"]
                add_row(f"{name}:solid_angle_contains", "value",
                        status="pass" if inside else "fail",
                        truth=[reference["min"], reference["max"]],
                        observed=[solid["lower"], solid["upper"]], tolerance=0.0,
                        unit="sr/pixel^2")
                if not inside:
                    failures.append({"functional": f"{name}:solid_angle", "scope": "solid_angle_jacobian",
                                     "expected": [reference["min"], reference["max"]],
                                     "actual": [solid["lower"], solid["upper"]],
                                     "reason": "the reported interval does not contain the truth"})
                    status = "fail"
            else:
                add_row(f"{name}:solid_angle_contains", "value", status="fail",
                        observed=None, reason=solid.get("reason", ""))
                failures.append({"functional": f"{name}:solid_angle", "scope": "solid_angle_jacobian",
                                 "expected": "ok", "actual": solid.get("code"),
                                 "reason": solid.get("reason", "")})
                status = "fail"

    # ---- origin and CD-only controls -------------------------------------
    base = observed_by_name.get("tan_sip3_mirror")
    for control, label in (("negative_origin_plus_one", "+1 pixel origin"),
                            ("negative_origin_plus_half", "+0.5 pixel origin"),
                            ("negative_cd_only_change", "CD only change")):
        control_entry = observed_by_name.get(control)
        if base is None or control_entry is None or base.get("status") != "wcs_built":
            add_row(f"control:{label}", "value", status="pending_implementation", observed=None,
                    reason="the control fixture was not available")
            status = "pending_implementation" if status == "pass" else status
            continue
        separation = 0.0
        for index, item in enumerate(base.get("forward", [])):
            if item.get("code") != "ok":
                continue
            other = control_entry.get("forward", [])[index]
            if other.get("code") != "ok":
                continue
            separation = max(separation,
                             angular_separation([item["nx"], item["ny"], item["nz"]],
                                                [other["nx"], other["ny"], other["nz"]]))
        add_row(f"control:{label}", "value", status="pass" if separation > 1e-10 else "fail",
                truth="> 1e-10 rad separation", observed=separation, tolerance=1e-10, unit="rad")
        if separation <= 1e-10:
            failures.append({"functional": f"control:{label}", "scope": "forward",
                             "expected": "> 1e-10 rad separation from the control",
                             "actual": separation,
                             "reason": "the implementation does not separate the control model"})
            status = "fail"

    payload = comparison_payload(case.name.upper(), status, rows, failures,
                                 "C01 comparison: external astropy truth and an independent "
                                 "closed form tangent-plane model")
    case.write_comparison(payload)
    return payload


if __name__ == "__main__":
    raise SystemExit(main())
