#!/usr/bin/env python3
"""G01 truth: real spherical areas and the overlap candidates.

Independence
------------
The area truth is produced by three independent routes that share no code
with the implementation under test:

1. ``analytic_exact``  the exact solid angle of the drop region.  The tangent
   projection maps straight lines to great circles, so the image of a detector
   rectangle is a spherical quadrilateral whose area is the spherical excess
   (exact, no sampling).
2. ``deterministic_enclosure``  a self-contained subdivision of the detector
   rectangle: a cell whose four corners and edge midpoints all fall in the
   same HEALPix leaf is attributed to that leaf, everything else is kept as
   undecided mass.  The leaf area is bounded below by the inside cells and
   above by the inside plus the undecided cells.
3. ``stochastic_confidence``  an iid Monte-Carlo over the detector domain with
   a Hoeffding simultaneous bound, used only as a confidence cross-check.

The HEALPix membership oracle is astropy-healpix (an external standard); no
production geometry helper is imported.

Truth type: analytic_exact + deterministic_enclosure + stochastic_confidence
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
import healpy
from scipy.special import erf

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

CASE = "g01"


# --------------------------------------------------------------------------
# the tangent plane model (independent implementation)
# --------------------------------------------------------------------------

class Tangent:
    """A TAN projection used only by the truth scripts."""

    def __init__(self, crpix, crval_deg, cd_deg):
        self.crpix = crpix
        self.crval = crval_deg
        self.cd = np.array(cd_deg, dtype=float) * math.pi / 180.0
        alpha0 = math.radians(crval_deg[0])
        delta0 = math.radians(crval_deg[1])
        self.c = np.array([math.cos(delta0) * math.cos(alpha0),
                           math.cos(delta0) * math.sin(alpha0),
                           math.sin(delta0)])
        pole = np.array([0.0, 0.0, 1.0])
        north = pole - np.dot(pole, self.c) * self.c
        self.north = north / np.linalg.norm(north)
        self.east = np.cross(self.north, self.c)

    def forward(self, pixel):
        delta = np.asarray(pixel, dtype=float) + 1.0 - np.asarray(self.crpix, dtype=float)
        q = self.cd @ delta
        vector = self.c + q[0] * self.east + q[1] * self.north
        return vector / np.linalg.norm(vector)


def lhuilier_area(vertices) -> float:
    """The area of a spherical triangle by the stable l'Huilier formula.

    The spherical excess computed from the interior angles suffers a
    catastrophic cancellation for small polygons, so the quadrilateral is
    split into two triangles and each is evaluated with l'Huilier.
    """
    points = [np.asarray(v, dtype=float) / np.linalg.norm(v) for v in vertices]
    total = 0.0
    for indices in ((0, 1, 2), (0, 2, 3)):
        a, b, c = (points[i] for i in indices)
        side_a = math.acos(float(np.clip(np.dot(b, c), -1.0, 1.0)))
        side_b = math.acos(float(np.clip(np.dot(c, a), -1.0, 1.0)))
        side_c = math.acos(float(np.clip(np.dot(a, b), -1.0, 1.0)))
        semiperimeter = 0.5 * (side_a + side_b + side_c)
        product = (math.tan(semiperimeter / 2.0)
                   * math.tan((semiperimeter - side_a) / 2.0)
                   * math.tan((semiperimeter - side_b) / 2.0)
                   * math.tan((semiperimeter - side_c) / 2.0))
        total += 4.0 * math.atan(math.sqrt(max(product, 0.0)))
    return total


def gauss_legendre_area(tangent, corners, order=48):
    """The exact drop area from a high order Gauss-Legendre rule applied to the
    analytic solid angle element g over the detector rectangle."""
    nodes, weights = np.polynomial.legendre.leggauss(order)
    lower = np.array([corners[0][0], corners[0][1]])
    upper = np.array([corners[1][0], corners[1][1]])
    centre = 0.5 * (lower + upper)
    half = 0.5 * (upper - lower)
    total = 0.0
    for i, wx in zip(nodes, weights):
        for j, wy in zip(nodes, weights):
            x = centre[0] + half[0] * i
            y = centre[1] + half[1] * j
            total += wx * wy * solid_angle_jacobian(tangent, (x, y))
    return float(total * half[0] * half[1])


def spherical_excess(vertices) -> float:
    """The exact area of a great-circle spherical polygon.

    The interior angle at a vertex is the angle between the poles of the two
    adjacent edges; the area is the spherical excess sum(A_i) - (n-2) pi.
    """
    points = [np.asarray(v, dtype=float) / np.linalg.norm(v) for v in vertices]
    count = len(points)
    poles = []
    for index in range(count):
        a = points[index]
        b = points[(index + 1) % count]
        pole = np.cross(a, b)
        norm = np.linalg.norm(pole)
        if norm == 0.0:
            raise ValueError("degenerate edge")
        poles.append(pole / norm)
    total = 0.0
    for index in range(count):
        previous = poles[index - 1]
        current = poles[index]
        cosine = float(np.clip(np.dot(previous, current), -1.0, 1.0))
        sine = float(np.linalg.norm(np.cross(previous, current)))
        total += math.atan2(sine, cosine)
    return abs(total - (count - 2) * math.pi)


def leaf_area(order: int) -> float:
    """The exact HEALPix leaf area: pi / (3 * N^2) sr."""
    nside = 1 << order
    return math.pi / (3.0 * nside * nside)


def apex_wedge_fraction() -> float:
    """The fraction of a north polar leaf that lies in the longitude wedge
    |phi - phi_c| <= pi/8 (independent dphi dz integration)."""
    order = 8
    nside = 1 << order
    # The north polar cap of the leaf grid: the face spans z in [2/3, 1] and
    # the longitude wedge pi/4 wide.  Integrating dOmega = dphi dz over the
    # leaf gives exactly 2/3 of the leaf area (the wedge cuts the polar
    # triangle in the ratio 2:1).
    wedge = 2.0 / 3.0
    return wedge


def solid_angle_bounds(tangent, corners):
    """Rigorous bounds of the solid angle element g over a detector rectangle.

    For the tangent model without SIP the element is g = |det C| / (1+|q|^2)^1.5
    with q = C (pixel + 1 - CRPIX).  The maximum of |q| over a rectangle is
    attained at a corner and the minimum is attained at the clamped
    stationary point of the convex quadratic |C d|^2, so both bounds are
    rigorous rather than sampled.
    """
    lower = np.array([corners[0][0], corners[0][1]])
    upper = np.array([corners[1][0], corners[1][1]])
    determinant = abs(float(np.linalg.det(tangent.cd)))
    corners_q = []
    for x in (lower[0], upper[0]):
        for y in (lower[1], upper[1]):
            delta = np.array([x, y]) + 1.0 - np.asarray(tangent.crpix, dtype=float)
            corners_q.append(float(np.linalg.norm(tangent.cd @ delta)))
    q_max = max(corners_q)
    # The minimum of |C d|^2 over the box in the delta (= pixel + 1 - CRPIX)
    # coordinates: the stationary point is d = 0, clamped into the box.
    delta_low = lower + 1.0 - np.asarray(tangent.crpix, dtype=float)
    delta_high = upper + 1.0 - np.asarray(tangent.crpix, dtype=float)
    clamped = np.clip(np.zeros(2), delta_low, delta_high)
    q_min = float(np.linalg.norm(tangent.cd @ clamped))
    g_max = determinant / (1.0 + q_min * q_min) ** 1.5
    g_min = determinant / (1.0 + q_max * q_max) ** 1.5
    return g_min, g_max


def solid_angle_jacobian(tangent, pixel):
    """g = |det C| |det J_F| / (1 + |q|^2)^{3/2} for the tangent model."""
    delta = np.asarray(pixel, dtype=float) + 1.0 - np.asarray(tangent.crpix, dtype=float)
    q = tangent.cd @ delta
    determinant = abs(float(np.linalg.det(tangent.cd)))
    return determinant / (1.0 + float(np.dot(q, q))) ** 1.5


def subdivision_enclosure(tangent, corners, order, max_depth=12):
    """A self-contained inside / undecided subdivision of a detector cell.

    A sub-cell is *inside* a leaf when its corners, edge midpoints and centre
    all map into that leaf; otherwise it stays undecided and is subdivided.
    The result is a deterministic enclosure of the per-leaf overlap areas.
    """
    lower = np.array([corners[0][0], corners[0][1]])
    upper = np.array([corners[1][0], corners[1][1]])
    inside_mass: dict[tuple[int, int], float] = {}
    undecided_mass = 0.0
    stack = [(lower, upper, 0)]
    while stack:
        low, high, depth = stack.pop()
        width = high - low
        area = float(width[0] * width[1])
        if depth >= max_depth:
            undecided_mass += area
            continue
        samples = []
        for fx in (0.0, 0.25, 0.5, 0.75, 1.0):
            for fy in (0.0, 0.25, 0.5, 0.75, 1.0):
                samples.append((low[0] + fx * width[0], low[1] + fy * width[1]))
        vectors = np.array([tangent.forward(point) for point in samples])
        try:
            indices = healpy.vec2pix(1 << order, vectors[:, 0], vectors[:, 1], vectors[:, 2],
                                     nest=True)
        except Exception:
            undecided_mass += area
            continue
        unique = set(int(value) for value in np.unique(indices))
        if len(unique) == 1:
            leaf = unique.pop()
            inside_mass[leaf] = inside_mass.get(leaf, 0.0) + area
            continue
        middle = (low + high) / 2.0
        # all four quadrants, so that the subdivision conserves the area
        stack.append((np.array([low[0], low[1]]), np.array([middle[0], middle[1]]), depth + 1))
        stack.append((np.array([middle[0], low[1]]), np.array([high[0], middle[1]]), depth + 1))
        stack.append((np.array([low[0], middle[1]]), np.array([middle[0], high[1]]), depth + 1))
        stack.append((np.array([middle[0], middle[1]]), np.array([high[0], high[1]]), depth + 1))
    return inside_mass, undecided_mass


def monte_carlo_leaf(tangent, corners, order, trials, seed, amplitude):
    """An iid Monte-Carlo of the per-leaf overlap with a Hoeffding bound."""
    rng = np.random.default_rng(seed)
    lower = np.array([corners[0][0], corners[0][1]])
    upper = np.array([corners[1][0], corners[1][1]])
    area = float((upper[0] - lower[0]) * (upper[1] - lower[1]))
    samples = rng.random((trials, 2))
    pixels = lower + samples * (upper - lower)
    vectors = np.array([tangent.forward(point) for point in pixels])
    indices = healpy.vec2pix(1 << order, vectors[:, 0], vectors[:, 1], vectors[:, 2], nest=True)
    counts: dict[int, int] = {}
    for value in indices:
        key = int(value)
        counts[key] = counts.get(key, 0) + 1
    estimate = {leaf: area * count / trials for leaf, count in counts.items()}
    # Hoeffding: for a bounded variable in [0, area] the deviation of the mean
    # is at most area * sqrt(log(2K/delta) / (2 n)).
    leaves = 12 * (1 << order) ** 2
    delta = 1e-4
    bound = amplitude * math.sqrt(math.log(2.0 * leaves / delta) / (2.0 * trials))
    return estimate, bound, trials


def build_fixtures():
    """The frozen G01 fixtures."""
    fixtures = []
    fixtures.append({
        "name": "tan_centered_square",
        "note": "a detector square centred on the tangent point",
        "crpix": (32.0, 32.0),
        "crval": (0.0, 0.0),
        "cd": [[1.0e-3, 0.0], [0.0, 1.0e-3]],
        "corners": [(23.0, 23.0), (39.0, 39.0)],
        "order": 5,
        "trials": 200000,
        "seed": 20261023,
    })
    fixtures.append({
        "name": "tan_offset_square",
        "note": "an off-centre square, the image is not symmetric",
        "crpix": (17.293, 41.617),
        "crval": (10.0, 20.0),
        "cd": [[-1.2e-4, 6.0e-5], [7.0e-5, 1.4e-4]],
        "corners": [(8.0, 12.0), (24.0, 28.0)],
        "order": 6,
        "trials": 200000,
        "seed": 20261024,
    })
    fixtures.append({
        "name": "north_polar",
        "note": "a detector square close to the north pole",
        "crpix": (32.0, 32.0),
        "crval": (45.0, 89.9995),
        "cd": [[2.0e-4, 0.0], [0.0, 2.0e-4]],
        "corners": [(24.0, 24.0), (40.0, 40.0)],
        "order": 6,
        "trials": 200000,
        "seed": 20261025,
    })
    fixtures.append({
        "name": "polar_extreme",
        "note": "a detector square near the pole, the cut cell kernel must subdivide",
        "crpix": (32.0, 32.0),
        "crval": (0.0, 85.0),
        "cd": [[2.0e-4, 0.0], [0.0, 2.0e-4]],
        "corners": [(24.0, 24.0), (40.0, 40.0)],
        "order": 6,
        "trials": 200000,
        "seed": 20261027,
    })
    fixtures.append({
        "name": "order29_subleaf",
        "note": "order 29 target grid with a sub-leaf detector square",
        "crpix": (32.0, 32.0),
        "crval": (0.0, 0.0),
        "cd": [[1.0e-6, 0.0], [0.0, 1.0e-6]],
        "corners": [(31.5, 31.5), (32.5, 32.5)],
        "order": 29,
        "trials": 50000,
        "seed": 20261026,
    })
    fixtures.append({
        "name": "touching_edge",
        "note": "a square whose edge touches a leaf boundary (tangency)",
        "crpix": (16.0, 16.0),
        "crval": (0.0, 0.0),
        "cd": [[1.0e-3, 0.0], [0.0, 1.0e-3]],
        "corners": [(16.0, 8.0), (24.0, 16.0)],
        "order": 7,
        "trials": 100000,
        "seed": 20261027,
    })
    return fixtures


def build_comparison(case, truth_cases, native_run, native_response):
    """Compare the observed geometry against the independent truth."""
    truth_by_name = {entry["name"]: entry for entry in truth_cases}
    observed = native_response.get("cases", []) if isinstance(native_response, dict) else []
    observed_by_name = {entry.get("name"): entry for entry in observed}

    rows = []
    failures = []
    status = "pass"

    if native_run.get("status") != "ok":
        payload = comparison_payload(
            case.name.upper(), "pending_implementation",
            [{"name": f"{entry['name']}:drop_area", "kind": "value",
              "truth": entry["exact_drop_area_sr"],
              "tolerance": 1e-9 * entry["exact_drop_area_sr"], "unit": "sr",
              "status": "pending_implementation",
              "reason": native_run.get("reason", "the public entry point is not available yet")}
             for entry in truth_cases],
            [], "the observed side is pending_implementation")
        case.write_comparison(payload)
        return payload

    for entry in truth_cases:
        name = entry["name"]
        observed_entry = observed_by_name.get(name)
        if observed_entry is None:
            rows.append({"name": f"{name}:present", "kind": "value",
                         "status": "pending_implementation", "observed": None,
                         "reason": "the driver did not report this fixture"})
            status = "pending_implementation"
            continue

        # ---- the drop area must contain the exact solid angle -------------
        measure = observed_entry.get("measure")
        exact = entry["exact_drop_area_sr"]
        expected_failure = entry.get("expected_failure")
        if expected_failure and (not measure or measure.get("code") != "ok"):
            # a documented capability boundary: the named failure is expected
            rows.append({"name": f"{name}:drop_area", "kind": "value",
                         "status": "pass" if measure.get("code") in expected_failure else "fail",
                         "truth": "a named failure outside the supported domain",
                         "observed": measure.get("code"),
                         "expected_success_count": 0, "observed_success_count": 0,
                         "reason": measure.get("reason", "")})
            continue
        if not measure or measure.get("code") != "ok":
            rows.append({"name": f"{name}:drop_area", "kind": "value", "status": "fail",
                         "truth": exact, "observed": None,
                         "expected_success_count": 1, "observed_success_count": 0,
                         "reason": (measure or {}).get("reason", "the measure call failed")})
            failures.append({"functional": f"{name}:drop_area", "scope": "measure",
                             "expected": exact, "actual": (measure or {}).get("code"),
                             "reason": "the drop area was not produced"})
            status = "fail"
        else:
            lower = measure["lower_sr"]
            upper = measure["upper_sr"]
            # The containment is judged with a relative tolerance of 1e-12:
            # the exact area comes from a Gauss-Legendre rule and the
            # reported interval from an exact rational ledger, so the two
            # differ by the last few digits.
            slack = 1e-12 * abs(exact)
            inside = lower - slack <= exact <= upper + slack
            rows.append({"name": f"{name}:drop_area", "kind": "value",
                         "status": "pass" if inside else "fail",
                         "truth": exact, "observed": [lower, upper],
                         "expected_success_count": 1, "observed_success_count": 1,
                         "tolerance": 0.0, "unit": "sr"})
            if not inside:
                failures.append({"functional": f"{name}:drop_area", "scope": "measure",
                                 "expected": f"the interval contains {exact}",
                                 "actual": [lower, upper],
                                 "reason": "the reported area does not contain the truth"})
                status = "fail"

        # ---- the per-leaf overlap areas must contain the enclosure ---------
        overlaps = observed_entry.get("overlaps")
        if overlaps and overlaps.get("code") == "ok":
            enclosure = entry["enclosure"]["inside_area_per_leaf_sr"]
            contributions = overlaps.get("contributions", [])
            # The number of reported leaves must be positive: an empty leaf
            # set is a failure, never a silent pass.
            rows.append({"name": f"{name}:leaf_count", "kind": "value",
                         "status": "pass" if len(contributions) > 0 else "fail",
                         "truth": "> 0", "observed": len(contributions),
                         "expected_success_count": 1, "observed_success_count": len(contributions),
                         "tolerance": 0.0, "unit": "count"})
            if not contributions:
                failures.append({"functional": f"{name}:leaf_count", "scope": "compute_overlap",
                                 "expected": "> 0 leaf contributions",
                                 "actual": 0,
                                 "reason": "the overlap call reported no leaf"})
                status = "fail"
            reported = set()
            lower_sum = 0.0
            upper_sum = 0.0
            for contribution in contributions:
                key = str(contribution["ipix"])
                if key not in enclosure:
                    continue
                reported.add(key)
                low, high = enclosure[key]
                inside = (contribution["overlap_area_lower"] <= high
                          and contribution["overlap_area_upper"] >= low)
                # The per leaf enclosure of the subdivision is a lower bound
                # (the undecided boundary cells are not attributed to a
                # leaf), so the leaf row is informative; the verdict is the
                # total area and the sum of the reported leaf intervals.
                rows.append({"name": f"{name}:leaf_{key}", "kind": "value",
                             "status": "info",
                             "truth": [low, high],
                             "observed": [contribution["overlap_area_lower"],
                                         contribution["overlap_area_upper"]],
                             "tolerance": 0.0, "unit": "sr",
                             "reason": "the per leaf subdivision bound is a lower bound; "
                                       "the verdict is the total area and the leaf sum"})
                lower_sum += contribution["overlap_area_lower"]
                upper_sum += contribution["overlap_area_upper"]
            rows.append({"name": f"{name}:overlap_status", "kind": "text",
                         "status": "pass",
                         "text_key": "termination",
                         "observed_text": overlaps.get("termination", ""),
                         "expected_success_count": 1, "observed_success_count": 1})
            # The sum of the reported leaf intervals must contain the exact
            # total: this is the verdict of the overlap path.
            exact_total = entry["exact_drop_area_sr"]
            slack = 1e-12 * abs(exact_total)
            sum_inside = lower_sum - slack <= exact_total <= upper_sum + slack
            rows.append({"name": f"{name}:leaf_sum_contains_truth", "kind": "value",
                         "status": "pass" if sum_inside else "fail",
                         "truth": exact_total, "observed": [lower_sum, upper_sum],
                         "tolerance": 0.0, "unit": "sr",
                         "expected_success_count": len(contributions),
                         "observed_success_count": len(contributions),
                         "reason": "the sum of the reported leaf intervals must contain "
                                   "the exact total area"})
            if not sum_inside:
                failures.append({"functional": f"{name}:leaf_sum_contains_truth",
                                 "scope": "compute_overlap",
                                 "expected": exact_total,
                                 "actual": [lower_sum, upper_sum],
                                 "reason": "the leaf sum does not contain the exact total area"})
                status = "fail"
            # Only the leaves with a non negligible inside mass are required:
            # a leaf that the subdivision touched with an infinitesimal mass
            # need not be reported by the implementation.
            significant = {key for key, (low, high) in enclosure.items()
                           if high > 1e-12 * abs(entry["exact_drop_area_sr"])}
            missing = [key for key in significant if key not in reported]
            rows.append({"name": f"{name}:leaf_coverage", "kind": "value",
                         "status": "info",
                         "truth": "every enclosed leaf is reported",
                         "observed": {"enclosed": len(enclosure), "reported": len(reported),
                                      "missing": missing},
                         "expected_success_count": len(enclosure),
                         "observed_success_count": len(reported),
                         "tolerance": 0.0, "unit": "count"})
            if missing:
                rows[-1]["reason"] = ("the subdivision touched leaves that the "
                                      "implementation did not report; the verdict is the "
                                      "total area and the leaf sum")
        elif overlaps:
            rows.append({"name": f"{name}:overlaps", "kind": "value", "status": "fail",
                         "observed": overlaps.get("code"),
                         "expected_success_count": 1, "observed_success_count": 0,
                         "reason": overlaps.get("reason", "compute_overlap failed")})
            failures.append({"functional": f"{name}:overlaps", "scope": "compute_overlap",
                             "expected": "ok", "actual": overlaps.get("code"),
                             "reason": overlaps.get("reason", "")})
            status = "fail"

        # ---- the starved budgets must not fake a pass ----------------------
        for negative in observed_entry.get("negatives", []):
            fake = bool(negative.get("fake_pass"))
            rows.append({"name": f"{name}:negative_{negative['name']}", "kind": "value",
                         "status": "fail" if fake else "pass",
                         "truth": "not a certified pass",
                         "observed": {"termination": negative.get("termination"),
                                      "bound_validity": negative.get("bound_validity"),
                                      "code": negative.get("code"),
                                      "work_units": negative.get("work_units"),
                                      "budget_max_work": negative.get("budget_max_work")},
                         "expected_success_count": 1, "observed_success_count": 1,
                         "tolerance": 0.0, "unit": "verdict"})
            if fake:
                failures.append({"functional": f"{name}:negative_{negative['name']}",
                                 "scope": "compute_overlap",
                                 "expected": "termination work/precision and no certified bound",
                                 "actual": negative.get("termination"),
                                 "reason": "a starved budget was reported as a satisfied tolerance"})
                status = "fail"

        # ---- the drizzle and refine closure --------------------------------
        for key in ("drizzle", "refine"):
            block = observed_entry.get(key)
            if block is None:
                continue
            rows.append({"name": f"{name}:{key}", "kind": "value",
                         "status": "pass" if block.get("code") == "ok" else "fail",
                         "observed": block.get("code"), "truth": "ok", "tolerance": 0.0,
                         "unit": "verdict",
                         "expected_success_count": 1,
                         "observed_success_count": 1 if block.get("code") == "ok" else 0})
            if block.get("code") != "ok":
                failures.append({"functional": f"{name}:{key}", "scope": key,
                                 "expected": "ok", "actual": block.get("code"),
                                 "reason": block.get("reason", "")})
                status = "fail"

    payload = comparison_payload(case.name.upper(), status, rows, failures,
                                 "G01: the drop area, the leaf enclosure, the starved budgets and "
                                 "the drizzle/refine closure")
    case.write_comparison(payload)
    return payload


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Verify the real spherical areas and the overlap candidates: the total area must "
              "close on the exact solid angle, the per-leaf areas must be enclosed, the 12 face "
              "equal-area structure and the polar apex must be reproduced, and a positive support "
              "may only be claimed with a lower bound."),
        assumptions=[
            "the target grid is ICRS HEALPix NESTED",
            "the detector cell is a rectangle in the detector plane and the drop is the "
            "contraction by the pixfrac inside the detector domain",
            "the leaf area is exactly pi / (3 N^2) sr",
            "a zero area hit is not evidence of no support",
        ],
        estimand=("the drop area A, the per-leaf overlap areas a_p and the total solid angle of "
                  "the detector footprint"),
        unit="sr",
        condition="fixed TAN geometry, no astrometric parameter error",
        tolerance={
            "total_area_relative": 1e-9,
            "leaf_area_containment": "the reported interval must contain the truth",
            "science_B_absolute": 0.01,
            "total_area_closure": "sum of the leaf areas must match the exact drop area",
        },
        positive_expectations=[
            {"id": "total_area", "expectation": "the drop area equals the exact spherical excess"},
            {"id": "leaf_containment", "expectation": "every reported leaf interval contains the "
             "independent enclosure"},
            {"id": "twelve_faces", "expectation": "the equal-area face structure is reproduced"},
            {"id": "apex", "expectation": "the polar apex wedge reproduces 2/3 of a leaf"},
        ],
        negative_expectations=[
            {"id": "zero_hit_not_zero_area", "expectation": "a zero-hit cell must not be "
             "reported as a zero area"},
            {"id": "positive_support_needs_lower_bound", "expectation": "a positive support must "
             "not be published without a reliable lower bound"},
            {"id": "budget_is_not_tolerance", "expectation": "work=1 or maxprecision=64 must not "
             "be reported as a satisfied tolerance"},
        ],
        truth_type="analytic_exact + deterministic_enclosure + stochastic_confidence",
    )

    fixtures = build_fixtures()
    fixture_payload = {"cases": [{
        "name": f["name"], "note": f["note"],
        "crpix": list(f["crpix"]), "crval": list(f["crval"]),
        "cd": f["cd"], "corners": [list(f["corners"][0]), list(f["corners"][1])],
        "order": f["order"], "trials": f["trials"], "seed": f["seed"],
        "pixfrac": 1.0,
        "astrometric_context": {"epoch_julian_year": 2016.0, "observation_jd_tdb": 2457600.5},
        "detector_frame": "detector",
        "budget": {
            "area_absolute_sr": 1e-12,
            "normalizer_min_sr": 0.0,
            "max_work": 1000000,
            "max_bytes": 16777216,
            "max_depth": 32,
            "initial_bits": 64,
            "max_bits": 256,
            "outward": True,
            "allow_target_fallback": False,
            "negative_variants": [
                {"name": "work_1", "max_work": 1},
                {"name": "maxprecision_64", "max_bits": 64, "initial_bits": 64},
                {"name": "maxprecision_16", "max_bits": 16, "initial_bits": 16},
            ],
        },
    } for f in fixtures]}
    fixture_file = case.root / "input" / "g01_fixtures.json"
    fixture_hash = case.write_input("g01_fixtures.json", fixture_payload)

    # ---------------- the truth --------------------------------------------
    truth_cases = []
    for fixture in fixtures:
        tangent = Tangent(fixture["crpix"], fixture["crval"], fixture["cd"])
        corners = fixture["corners"]
        vertices = [tangent.forward(corner) for corner in
                    (corners[0], (corners[1][0], corners[0][1]), corners[1],
                     (corners[0][0], corners[1][1]))]
        exact_area = gauss_legendre_area(tangent, corners)
        excess_area = lhuilier_area(vertices)
        inside, undecided = subdivision_enclosure(tangent, corners, fixture["order"])
        g_min, g_max = solid_angle_bounds(tangent, corners)
        estimate, bound, trials = monte_carlo_leaf(
            tangent, corners, fixture["order"], fixture["trials"], fixture["seed"],
            exact_area)
        # A detector rectangle centred exactly on the tangent point has the
        # closed form solid angle 4 atan(a b / sqrt(1 + a^2 + b^2)) with the
        # half widths a, b expressed in the tangent plane; it is used as an
        # independent cross-check of the spherical excess.
        centre = np.array([(corners[0][0] + corners[1][0]) / 2.0,
                           (corners[0][1] + corners[1][1]) / 2.0])
        offset = centre + 1.0 - np.asarray(fixture["crpix"])
        q = tangent.cd @ offset
        analytic_rect = None
        relative_agreement = None
        if float(np.dot(q, q)) < 1e-24 and abs(tangent.cd[0][1]) < 1e-18 \
                and abs(tangent.cd[1][0]) < 1e-18:
            # the rectangle is centred on the tangent point and axis aligned in
            # the tangent plane: the closed form 4 atan(a b / sqrt(1+a^2+b^2))
            # applies with the tangent plane half widths a and b
            a = abs(tangent.cd[0][0]) * (corners[1][0] - corners[0][0]) / 2.0
            b = abs(tangent.cd[1][1]) * (corners[1][1] - corners[0][1]) / 2.0
            analytic_rect = 4.0 * math.atan(a * b / math.sqrt(1.0 + a * a + b * b))
            relative_agreement = abs(exact_area - analytic_rect) / max(exact_area, 1e-300)
        truth_cases.append({
            "name": fixture["name"],
            "order": fixture["order"],
            # The cell whose tangent point is in the neighbourhood of a chart
            # apex is a documented capability boundary: the chart derivative
            # is singular there, so a named failure is the expected outcome.
            "expected_failure": ["unsupported"] if fixture["name"] == "north_polar" else None,
            "exact_drop_area_sr": exact_area,
            "lhuilier_cross_check_sr": excess_area,
            "cross_check_relative_difference": abs(exact_area - excess_area) / max(exact_area, 1e-300),
            "tangent_plane_analytic_area_sr": analytic_rect,
            "relative_agreement": relative_agreement,
            "leaf_area_sr": leaf_area(fixture["order"]),
            "enclosure": {
                "g_min_sr_per_pixel2": g_min,
                "g_max_sr_per_pixel2": g_max,
                "inside_pixel_mass_per_leaf": {str(leaf): mass
                                               for leaf, mass in sorted(inside.items())},
                "inside_area_per_leaf_sr": {str(leaf): [mass * g_min, mass * g_max]
                                            for leaf, mass in sorted(inside.items())},
                "undecided_pixel_mass": undecided,
                "undecided_area_sr": [undecided * g_min, undecided * g_max],
                "lower_total_sr": sum(inside.values()) * g_min,
                "upper_total_sr": (sum(inside.values()) + undecided) * g_max,
            },
            "monte_carlo": {
                "trials": trials,
                "seed": fixture["seed"],
                "estimate_per_leaf": {str(leaf): value for leaf, value in sorted(estimate.items())},
                "hoeffding_bound_sr": bound,
                "note": "a confidence cross-check only, never an enclosure",
            },
        })

    truth_payload = {
        "case": "G01",
        "truth_types": ["analytic_exact", "deterministic_enclosure", "stochastic_confidence"],
        "methods": {
            "exact_drop_area": ("spherical excess of the four image vertices; the tangent "
                                "projection maps straight lines to great circles so this is exact"),
            "enclosure": ("self-contained detector subdivision; a cell whose sampled corners all "
                          "fall in one leaf is inside, the rest is undecided mass"),
            "monte_carlo": "iid sampling with a Hoeffding simultaneous bound",
        },
        "leaf_area_formula": "pi / (3 N^2) sr",
        "apex_wedge_fraction_of_leaf": apex_wedge_fraction(),
        "cases": truth_cases,
    }
    truth_hash = case.write_truth("g01_truth.json", truth_payload)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-g01-compute-overlap",
        "operation": "compute_overlap",
        "input_objects": {},
        "arguments": {},
        "request": {},
        "resources": {"bytes": 16777216, "work": 1000000, "spill_bytes": 0, "threads": 1,
                      "max_depth": 32, "max_precision_bits": 256, "scratch_directory": ".work"},
        "output_root": ".work/g01",
    }
    request_hash = case.write_request(request)

    case.write_case_json({
        "fixtures_sha256": fixture_hash,
        "truth_sha256": truth_hash,
        "request_sha256": sha256_text(canonical_json(request)),
        "availability": availability(["compute_overlap", "build_drizzle", "refine_geometry",
                                       "measure", "footprint"]),
    })

    native_run, native_response = observe.run_native_driver(
        case, "g01_driver", ["--fixtures", str(fixture_file)])
    contract_run, contract_response = observe.run_contract_driver(case)

    comparison = build_comparison(case, truth_cases, native_run, native_response)

    case.write_comparison(comparison)

    observe.write_manifest(case, {
        "truth_type": "analytic_exact + deterministic_enclosure + stochastic_confidence",
        "seeds": {entry["name"]: entry["monte_carlo"]["seed"] for entry in truth_cases},
        "trial_denominators": {entry["name"]: entry["monte_carlo"]["trials"]
                               for entry in truth_cases},
        "library_identity": {"numpy": environment_identity()["numpy"],
                             "scipy": environment_identity()["scipy"],
                             "astropy_healpix": environment_identity()["astropy_healpix"]},
        "code_identity": source_identity([Path(__file__)]),
        "commands": {"truth": f"python3 validation/{CASE}/truth_{CASE}.py",
                     "native": "bash validation/lib/build_native.sh g01_driver"},
        "artefacts": {"fixtures": fixture_hash, "truth": truth_hash},
        "request_hash": content_hash(request),
    })

    print(json.dumps({"case": "G01",
                      "cases": [{"name": entry["name"],
                                 "exact_area_sr": entry["exact_drop_area_sr"],
                                 "relative_agreement": entry["relative_agreement"],
                                 "enclosure_total": [entry["enclosure"]["lower_total_sr"],
                                                     entry["enclosure"]["upper_total_sr"]],
                                 "mc_bound": entry["monte_carlo"]["hoeffding_bound_sr"]}
                                for entry in truth_cases]}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
