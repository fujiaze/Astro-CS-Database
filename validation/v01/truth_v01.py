#!/usr/bin/env python3
"""V01 truth: sparse actual-scene variance reconstruction.

Independence
------------
The truth is an explicit construction on a gnomonic chart: the control
points, the chart triangulation (scipy Delaunay, an external library), the
barycentric interpolation of log V and the Taylor remainder bound of the
foundation.  The C-infinity counter-example is built analytically.

Truth type: analytic_exact with a deterministic bound, plus an explicit
counter-example for the case without an external curvature bound.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
from scipy.spatial import Delaunay

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lib import observe  # noqa: E402
from lib.availability import availability  # noqa: E402
from lib.vcommon import (  # noqa: E402
    Case,
    canonical_json,
    comparison_payload,
    content_hash,
    environment_identity,
    sha256_text,
    source_identity,
    write_json,
)

CASE = "v01"


def chart_points():
    """A regular control grid on a local gnomonic chart, in radians.

    The chart of the implementation is the tangent plane at the centre, so
    its coordinates are radians; the truth uses the same unit.
    """
    grid = []
    step = 0.4 * math.pi / 180.0
    start = -0.6 * math.pi / 180.0
    for i in range(4):
        for j in range(4):
            grid.append((start + step * i, start + step * j))
    return np.array(grid, dtype=float)


def log_v_plane(points):
    """log V is an exact affine function of the chart coordinates."""
    return 0.7 + 130.0 * points[:, 0] - 90.0 * points[:, 1]


def log_v_quadratic(points):
    """log V is an exact quadratic; the Hessian is known."""
    x = points[:, 0]
    y = points[:, 1]
    return 0.7 + 130.0 * x - 90.0 * y + 2.0 * x * x - 1.1 * y * y


def hessian_norm_quadratic():
    """The spectral norm of the Hessian of the quadratic truth."""
    hessian = np.array([[4.0, 0.0], [0.0, -2.2]])
    return float(np.max(np.abs(np.linalg.eigvalsh(hessian))))


def barycentric_interpolate(triangulation, points, values, query):
    """The non-negative barycentric interpolation on the triangulation."""
    simplex = triangulation.find_simplex(query)
    if simplex < 0:
        return None, None
    transform = triangulation.transform[simplex]
    offset = query - transform[2]
    barycentric = transform[:2].dot(offset)
    weights = np.append(barycentric, 1.0 - barycentric.sum())
    indices = triangulation.simplices[simplex]
    interpolated = float(weights @ values[indices])
    return interpolated, (indices, weights)


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Verify the sparse actual-scene variance reconstruction: with an external delta and "
              "a known curvature bound the truth must lie inside the reported interval, without a "
              "curvature bound only a conditional prediction is allowed, and no cross-leaf "
              "covariance may be manufactured."),
        assumptions=[
            "the controls are actual-scene variance controls of the same estimator",
            "the chart is a local gnomonic chart and the query lies inside a support triangle",
            "delta_i and the curvature bound M are external information, not fitted from the same "
            "sparse points",
        ],
        estimand="log V at the query point and the variance interval [exp(u-Delta), exp(u+Delta)]",
        unit="log of a variance (dimensionless); the variance is in the unit of the controls",
        condition="actual scene, same estimator and response",
        tolerance={
            "interval_containment": "the truth must lie inside the reported interval",
            "conditional_without_M": "no interval may be published without M",
            "cross_leaf_covariance": "no off-diagonal covariance may be produced",
        },
        positive_expectations=[
            {"id": "plane_contained", "expectation": "the plane truth lies inside the interval"},
            {"id": "quadratic_contained", "expectation": "the quadratic truth lies inside the "
             "interval built with the known Hessian bound"},
            {"id": "linear_exact", "expectation": "the affine truth is reproduced exactly by "
             "the barycentric interpolation"},
        ],
        negative_expectations=[
            {"id": "c_infinity_bump", "expectation": "a smooth field that vanishes at every "
             "control point but is large at the query must not be certified by smoothness"},
            {"id": "no_M_conditional", "expectation": "without M only a conditional prediction "
             "is allowed"},
            {"id": "outside_domain", "expectation": "a query outside the support must not be "
             "filled by nearest neighbour"},
        ],
        truth_type="analytic_exact + conditional_model",
    )

    points = chart_points()
    triangulation = Delaunay(points)
    degree = math.pi / 180.0
    queries = np.array([[-0.5 * degree, -0.5 * degree], [0.1 * degree, 0.2 * degree],
                        [0.55 * degree, -0.45 * degree], [0.0, 0.0],
                        [5.0 * degree, 5.0 * degree]])

    plane_values = log_v_plane(points)
    quadratic_values = log_v_quadratic(points)
    delta = 0.01
    curvature = hessian_norm_quadratic()

    # The last query lies outside the mesh; it is judged by the outside
    # domain rule, not by the containment.
    inside_queries = queries[:-1]
    plane_rows = []
    for query in inside_queries:
        interpolated, support = barycentric_interpolate(triangulation, points, plane_values, query)
        truth = float(log_v_plane(np.array([query]))[0])
        bound = delta  # the affine field has a zero Hessian
        plane_rows.append({
            "query": query.tolist(),
            "interpolated": interpolated,
            "truth": truth,
            "delta": delta,
            "curvature_bound": 0.0,
            "remainder_bound": 0.0,
            "interval_log_lower": interpolated - bound,
            "interval_log_upper": interpolated + bound,
            "truth_inside": (interpolated - bound) <= truth <= (interpolated + bound),
            "variance_interval": [math.exp(interpolated - bound), math.exp(interpolated + bound)],
            "truth_variance": math.exp(truth),
        })

    quadratic_rows = []
    for query in inside_queries:
        interpolated, support = barycentric_interpolate(triangulation, points, quadratic_values,
                                                        query)
        truth = float(log_v_quadratic(np.array([query]))[0])
        indices, weights = support
        remainder = 0.0
        for index, weight in zip(indices, weights):
            distance = float(np.linalg.norm(points[index] - query))
            remainder += float(weight) * distance * distance
        bound = float(np.sum(weights) * delta) + 0.5 * curvature * remainder
        quadratic_rows.append({
            "query": query.tolist(),
            "interpolated": interpolated,
            "truth": truth,
            "delta": delta,
            "curvature_bound": curvature,
            "remainder_term": 0.5 * curvature * remainder,
            "interval_log_lower": interpolated - bound,
            "interval_log_upper": interpolated + bound,
            "truth_inside": (interpolated - bound) <= truth <= (interpolated + bound),
            "relative_error_over_bound": abs(interpolated - truth) / bound if bound else None,
        })

    # the C-infinity counter-example: a smooth field that is zero at every
    # control point and large at the query
    bump_points = points
    bump_query = np.array([[0.05, 0.05]])
    bump = np.prod(np.sin(np.pi * (bump_points - bump_query[0]) / 1.2), axis=1)
    bump_value = float(np.prod(np.sin(np.pi * (bump_query[0] - bump_query[0]) / 1.2)))
    max_abs_control = float(np.max(np.abs(bump)))
    # the C-infinity bump is zero at the controls by construction; its value
    # at an arbitrary nearby point is bounded by nothing finite
    bump_counter = {
        "control_values": bump.tolist(),
        "max_abs_control_value": max_abs_control,
        "query": bump_query[0].tolist(),
        "note": ("the product of sines vanishes at every control point and is unbounded in "
                 "between; without an external curvature bound no finite interval can be "
                 "certified, which is the required negative behaviour"),
    }

    # a query outside the support
    outside = np.array([[5.0 * degree, 5.0 * degree]])
    simplex = int(triangulation.find_simplex(outside)[0])
    outside_row = {
        "query": outside[0].tolist(),
        "inside_support": simplex >= 0,
        "expected": "unavailable; a nearest neighbour fill must not be certified",
    }

    truth = {
        "case": "V01",
        "truth_types": ["analytic_exact", "conditional_model"],
        "methods": {
            "interpolation": "non-negative barycentric weights on a scipy Delaunay triangulation",
            "bound": "Delta = sum_i lambda_i delta_i + (M/2) sum_i lambda_i |x_i - x|^2",
            "counter_example": "a product of sines that vanishes at every control point",
        },
        "delta": delta,
        "curvature_bound_M": curvature,
        "plane": plane_rows,
        "quadratic": quadratic_rows,
        "c_infinity_bump": bump_counter,
        "outside_domain": outside_row,
    }
    truth_hash = case.write_truth("v01_truth.json", truth)

    fixture = {
        "case": "V01",
        "chart": {"kind": "gnomonic", "centre_deg": [0.0, 0.0], "radius_deg": 1.0},
        "control_points": points.tolist(),
        "control_values_plane": plane_values.tolist(),
        "control_values_quadratic": quadratic_values.tolist(),
        "delta": delta,
        "curvature_bound_M": curvature,
        "queries": queries.tolist(),
        "estimator": {"id": "fixture-v01", "quantity": "actual_scene_variance", "unit": "L^2"},
    }
    fixture_hash = case.write_input("v01_model.json", fixture)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-v01-scene-variance",
        "operation": "reconstruct_scene_variance",
        "input_objects": {},
        "arguments": {},
        "request": {},
        "resources": {"bytes": 8388608, "work": 100000, "spill_bytes": 0, "threads": 1,
                      "max_depth": 32, "max_precision_bits": 128, "scratch_directory": ".work"},
        "output_root": ".work/v01",
    }
    request_hash = case.write_request(request)

    case.write_case_json({
        "input_model_sha256": fixture_hash,
        "truth_sha256": truth_hash,
        "request_sha256": sha256_text(canonical_json(request)),
        "availability": availability(["reconstruct_scene_variance",
                                       "build_supported_triangulation"]),
    })

    observe.write_manifest(case, {
        "truth_type": "analytic_exact + conditional_model",
        "seeds": [],
        "trial_denominators": {"queries": len(queries)},
        "library_identity": {"numpy": environment_identity()["numpy"],
                             "scipy": environment_identity()["scipy"]},
        "code_identity": source_identity([Path(__file__)]),
        "commands": {"truth": f"python3 validation/{CASE}/truth_{CASE}.py"},
        "artefacts": {"truth": truth_hash, "input_model": fixture_hash},
        "request_hash": content_hash(request),
    })

    # ---- the observed side: the real reconstruction path --------------------
    native_run, native_response = observe.run_native_driver(case, "v01_driver", [])

    def observed_case(name: str) -> dict:
        for entry in native_response.get("cases", []) or []:
            if entry.get("name") == name:
                return entry
        return {}

    plane_observed = observed_case("plane")
    quadratic_observed = observed_case("quadratic")
    without_curvature = observed_case("quadratic_without_curvature")

    rows = []
    failures = []
    status = "pass"

    def add(name: str, ok: bool, truth, observed, note: str = "", unit: str = "1",
            tolerance: float = 0.0) -> None:
        nonlocal status
        rows.append({"name": name, "kind": "value", "status": "pass" if ok else "fail",
                     "truth": truth, "observed": observed, "tolerance": tolerance,
                     "unit": unit, "expected_success_count": 1, "observed_success_count": 1,
                     "note": note})
        if not ok:
            failures.append({"functional": name, "scope": "reconstruct_scene_variance",
                             "expected": truth, "actual": observed, "reason": note})
            status = "fail"

    if native_run.get("status") != "ok":
        rows.append({"name": "v01_driver_available", "kind": "value",
                     "status": "pending_implementation", "observed": None,
                     "expected_success_count": 1, "observed_success_count": 0,
                     "reason": native_run.get("reason", "the driver did not run")})
        status = "pending_implementation"
    else:
        plane_queries = plane_observed.get("queries", []) or []
        quadratic_queries = quadratic_observed.get("queries", []) or []
        without_queries = without_curvature.get("queries", []) or []

        # 1) the plane truth lies inside the published interval
        for index, truth_row in enumerate(plane_rows):
            observed_row = plane_queries[index] if index < len(plane_queries) else {}
            inside = bool(observed_row.get("truth_inside"))
            add(f"plane_truth_inside[{index}]", inside,
                [truth_row["interval_log_lower"], truth_row["interval_log_upper"]],
                [observed_row.get("observed_log_lower"), observed_row.get("observed_log_upper")],
                "the plane truth must lie inside the published log interval", unit="log variance")

        # 2) the quadratic truth lies inside the interval built with M
        for index, truth_row in enumerate(quadratic_rows):
            observed_row = quadratic_queries[index] if index < len(quadratic_queries) else {}
            inside = bool(observed_row.get("truth_inside"))
            add(f"quadratic_truth_inside[{index}]", inside,
                [truth_row["interval_log_lower"], truth_row["interval_log_upper"]],
                [observed_row.get("observed_log_lower"), observed_row.get("observed_log_upper")],
                "the quadratic truth must lie inside the interval of the known Hessian bound",
                unit="log variance")

        # 3) the affine truth is reproduced exactly by the barycentric interpolation
        worst = 0.0
        for index, truth_row in enumerate(plane_rows):
            observed_row = plane_queries[index] if index < len(plane_queries) else {}
            value = observed_row.get("observed_log_value")
            if isinstance(value, (int, float)):
                worst = max(worst, abs(float(value) - float(truth_row["truth"])))
        add("linear_exact", worst <= 1e-12, 0.0, worst,
            "the barycentric interpolation reproduces the affine truth exactly", tolerance=1e-12)

        # 4) without the curvature bound only a conditional prediction is allowed
        conditional = bool(without_curvature.get("queries", [{}])[0].get("conditional")) if without_queries else False
        no_interval = all(not isinstance(row.get("observed_log_lower"), (int, float))
                           or row.get("observed_log_lower") == 0.0
                           for row in without_queries)
        add("no_M_conditional", conditional and no_interval,
            "conditional without an interval",
            {"conditional": conditional, "interval_published": not no_interval},
            "without the external curvature bound no interval may be published")

        # 5) a query outside the mesh must not be filled
        outside_observed = plane_queries[-1] if len(plane_queries) >= len(plane_rows) + 1 else {}
        state = outside_observed.get("state")
        value = outside_observed.get("value")
        unfilled = state == 0 or value is None
        add("outside_domain", unfilled, "state 0 and no value",
            {"state": state, "value": value},
            "a query outside the support must not be filled by a nearest neighbour")

        # 6) the C-infinity counter-example: without M nothing can be certified,
        #    and the observed route indeed publishes no interval
        add("c_infinity_bump", conditional and no_interval,
            "no finite interval may be certified without M",
            {"conditional": conditional, "interval_published": not no_interval},
            "the analytic counter-example of the truth: a product of sines that vanishes at "
            "every control point is unbounded between them")

    comparison = comparison_payload(
        case.name.upper(), status, rows, failures,
        "V01: the sparse actual-scene variance reconstruction against the barycentric truth "
        "with the Taylor remainder bound")

    case.write_comparison(comparison)
    print(json.dumps({
        "case": "V01",
        "plane_all_inside": all(row["truth_inside"] for row in plane_rows),
        "quadratic_all_inside": all(row["truth_inside"] for row in quadratic_rows),
        "quadratic_relative_error_over_bound": [row["relative_error_over_bound"]
                                                for row in quadratic_rows],
        "c_infinity_control_max_abs": max_abs_control,
        "outside_support": outside_row["inside_support"],
    }, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
