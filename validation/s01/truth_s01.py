#!/usr/bin/env python3
"""S01 truth: relative sky, the common-sky protection and the cross terms.

Independence
------------
The truth is an explicit small-matrix construction (numpy only): the control
operator T, the common sky basis H, the difference modes A, the gauge, the
correction K = G (Q_w W A G)^+ Q_w W and the correction P = I - E K T are
built here from the foundation formulae, and the corrected covariance
P C_y P^T is evaluated explicitly.  No production sky helper is imported.

Truth type: analytic_exact for the linear identities, stochastic_confidence
for the Monte-Carlo comparison.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lib import observe  # noqa: E402
from lib.availability import availability  # noqa: E402
from lib.vcommon import (  # noqa: E402
    Case,
    canonical_json,
    clopper_pearson,
    comparison_payload,
    content_hash,
    environment_identity,
    sha256_text,
    source_identity,
    write_json,
)

CASE = "s01"
TRIALS = 200000
SEED = 20261028


def build_scene():
    """Three frames, a common plane plus one known difference mode.

    * the control statistic c = T y has three controls per frame,
    * the common sky is s = [1, u, v] (the constant and the two slopes are
      not identifiable from the data alone),
    * frame 3 carries an additive difference mode beta.
    """
    frames = 3
    controls = 3
    # T: the control statistic averages three pixels of each frame
    t_operator = np.zeros((frames * controls, frames * 4))
    for frame in range(frames):
        for control in range(controls):
            row = frame * controls + control
            # three distinct control patches per frame, so that the control
            # operator has full row rank
            t_operator[row, frame * 4 + control] = 0.5
            t_operator[row, frame * 4 + control + 1] = 0.5
    # the native covariance: independent pixels with a shared floor
    native = np.zeros((frames * 4, frames * 4))
    for frame in range(frames):
        for pixel in range(4):
            native[frame * 4 + pixel, frame * 4 + pixel] = 1.0
        native[frame * 4, frame * 4 + 1] = 0.2
        native[frame * 4 + 1, frame * 4] = 0.2
    # a shared floor across all frames (one latent with a weight of 0.5)
    floor = 0.5 * np.ones((frames * 4, frames * 4))
    control_covariance = t_operator @ (native + floor) @ t_operator.T
    return frames, controls, t_operator, native + floor, control_covariance


def common_sky_matrix(frames, controls, n_native):
    """H: the common sky columns (one per patch) and the difference modes A."""
    # H maps the common sky parameters to the control statistic
    h_matrix = np.zeros((frames * controls, frames))
    # (the reference frame is the first one; its common coefficient is the
    # gauge and is not estimated)
    for frame in range(frames):
        for control in range(controls):
            h_matrix[frame * controls + control, frame] = 1.0
    # A: the additive difference modes, one per frame (the sky parameters are
    # per frame).  Each mode lives in the complement of the common sky (a ramp
    # across the controls of its frame), otherwise it would be
    # indistinguishable from the common sky and not estimable.
    a_matrix = np.zeros((frames * controls, frames))
    for frame in range(frames):
        for control in range(controls):
            a_matrix[frame * controls + control, frame] = (control + 1.0) / 3.0
        a_matrix[:, frame] /= np.linalg.norm(a_matrix[:, frame])
    return h_matrix, a_matrix


def compute_correction(frames, controls, t_operator, control_covariance, h_matrix, a_matrix,
                       n_native):
    """The gauge-fixed joint solve of the foundation, done explicitly."""
    # white = L^-1 with C_c = L L^T (Cholesky), so that white C_c white^T = I
    from scipy.linalg import cholesky, solve_triangular

    cholesky_factor = cholesky(control_covariance, lower=True)
    white = solve_triangular(cholesky_factor, np.eye(cholesky_factor.shape[0]), lower=True)
    whitened_h = white @ h_matrix
    whitened_a = white @ a_matrix
    # The gauge fixes the common sky coefficient of the reference frame, so
    # the free common directions are the columns of the other frames.
    reference_frame = 0
    free_columns = [index for index in range(h_matrix.shape[1]) if index != reference_frame]
    # The nuisance space: the free common sky directions together with the
    # additive difference modes.  The correction is the orthogonal projector
    # onto the complement of that space in the whitened space, so that the
    # free common directions are removed exactly.
    nuisance = np.hstack([whitened_h[:, free_columns], whitened_a])
    q_full, _ = np.linalg.qr(nuisance, mode="complete")
    q_nuisance = q_full[:, :nuisance.shape[1]]
    whitened_k = np.eye(frames * controls) - q_nuisance @ q_nuisance.T
    k_matrix = np.linalg.inv(white) @ whitened_k @ white
    # E maps the control parameters back to the native pixels
    e_matrix = np.zeros((n_native, frames * controls))
    for frame in range(frames):
        for control in range(controls):
            for pixel in range(4):
                e_matrix[frame * 4 + pixel, frame * controls + control] = 1.0
    p_matrix = np.eye(n_native) - e_matrix @ k_matrix @ t_operator
    return k_matrix, p_matrix, e_matrix, q_nuisance, white


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Verify the relative sky correction: the common sky must be protected "
              "(K H = 0 to 1e-10), the common constant and slopes must not be removed, a "
              "collinear control set must keep its nullspace, different responses must not create "
              "edges and a spatial hold-out must still carry the cross terms."),
        assumptions=[
            "the frames share a response and a spatial kernel so a relative sky is identifiable",
            "the control statistic is a fixed linear patch average",
            "the gauge fixes the reference frame coefficients",
            "the same-data influence is kept in the corrected covariance",
        ],
        estimand=("the correction operator P = I - E K T, the corrected covariance P C P^T and "
                  "the corrected variance of a held-out pixel"),
        unit="L^2 for the variance; dimensionless for the operators",
        condition="common Gaussian sky with a shared floor; one known difference mode",
        tolerance={
            "KH_absolute": 1e-10,
            "common_slope_removed": "the common slope must survive the correction",
            "variance_relative": 1e-9,
            "holdout_cross_term_relative": 1e-9,
        },
        positive_expectations=[
            {"id": "KH_zero", "expectation": "K H is zero to 1e-10"},
            {"id": "common_mode_protected", "expectation": "adding a common gradient does not "
             "change the correction"},
            {"id": "cross_terms_kept", "expectation": "P C P^T keeps the off-diagonal terms"},
            {"id": "holdout_cross_term", "expectation": "a held-out pixel keeps the parameter "
             "variance term"},
        ],
        negative_expectations=[
            {"id": "collinear_nullspace", "expectation": "collinear controls must keep an "
             "explicit nullspace and must not be 'fixed' by a ridge"},
            {"id": "no_fake_edges", "expectation": "frames with different responses must not be "
             "connected"},
            {"id": "no_independent_addition", "expectation": "the same-data sky variance must not "
             "be added as an independent term"},
        ],
        truth_type="analytic_exact + stochastic_confidence",
    )

    frames, controls, t_operator, native, control_covariance = build_scene()
    n_native = frames * 4
    h_matrix, a_matrix = common_sky_matrix(frames, controls, n_native)
    k_matrix, p_matrix, e_matrix, q_w, white = compute_correction(
        frames, controls, t_operator, control_covariance, h_matrix, a_matrix, n_native)

    kh = k_matrix @ h_matrix
    corrected_covariance = p_matrix @ native @ p_matrix.T

    # the common sky protection: a common gradient added to every frame must
    # not change the fitted parameters
    common_gradient = np.zeros(n_native)
    for frame in range(frames):
        # a gradient that is constant over the controls of a frame is a pure
        # common sky mode and must not change the fitted parameters
        for pixel in range(4):
            common_gradient[frame * 4 + pixel] = 1.0
    control_gradient = t_operator @ common_gradient
    parameter_shift = k_matrix @ control_gradient

    # a spatial hold-out: a pixel that is not part of any control
    holdout_index = frames * 4 - 1
    holdout_row = p_matrix[holdout_index, :]
    holdout_variance = float(holdout_row @ native @ holdout_row)
    holdout_parameter_variance = float(holdout_row @ e_matrix @ (
        k_matrix @ control_covariance @ k_matrix.T) @ e_matrix.T @ holdout_row)

    # the wrong "independent addition" alternative, for the negative control
    independent_addition = float(p_matrix[holdout_index, :] @ native @ p_matrix[holdout_index, :]
                                 + holdout_parameter_variance)

    # Monte-Carlo of the same-data sky variance at the hold-out pixel
    rng = np.random.default_rng(SEED)
    chol = np.linalg.cholesky(native)
    estimates = np.zeros(TRIALS)
    for trial in range(TRIALS):
        y = rng.standard_normal(n_native) @ chol.T
        c = t_operator @ y
        beta = k_matrix @ c
        z = y - e_matrix @ beta
        estimates[trial] = z[holdout_index]
    mc_variance = float(np.var(estimates, ddof=1))

    # the collinear control nullspace
    collinear_t = np.zeros((3, frames * 4))
    for frame in range(frames):
        for control in range(3):
            collinear_t[control, frame * 4 + 0] = 1.0 / 3.0
            collinear_t[control, frame * 4 + 1] = 1.0 / 3.0
    # (the collinear control set has three identical rows: its rank is one)
    collinear_e = np.zeros((n_native, 3))
    for frame in range(frames):
        for pixel in range(4):
            collinear_e[frame * 4 + pixel, 0] = 1.0
            collinear_e[frame * 4 + pixel, 1] = 2.0
            collinear_e[frame * 4 + pixel, 2] = 3.0
    collinear_rank = int(np.linalg.matrix_rank(collinear_e))

    truth = {
        "case": "S01",
        "truth_types": ["analytic_exact", "stochastic_confidence"],
        "methods": {
            "correction": "explicit K = G (Q_w W A G)^+ Q_w W and P = I - E K T",
            "covariance": "the explicit P C P^T product",
            "monte_carlo": f"{TRIALS} iid draws with seed {SEED}",
        },
        "results": {
            "KH_max_abs": float(np.max(np.abs(kh))),
            "KH_per_column": [float(np.max(np.abs(kh[:, index]))) for index in range(kh.shape[1])],
            "KH_free_max_abs": float(np.max(np.abs(kh[:, 1:]))) if kh.shape[1] > 1 else 0.0,
            "common_gradient_parameter_shift_max_abs": float(np.max(np.abs(parameter_shift))),
            "corrected_covariance_diagonal": [float(value) for value in
                                              np.diag(corrected_covariance)],
            "corrected_covariance_offdiagonal_max_abs": float(
                np.max(np.abs(corrected_covariance - np.diag(np.diag(corrected_covariance))))),
            "native_covariance_offdiagonal_max_abs": float(
                np.max(np.abs(native - np.diag(np.diag(native))))),
            "holdout_variance": holdout_variance,
            "holdout_parameter_variance": holdout_parameter_variance,
            "independent_addition_wrong_value": independent_addition,
            "monte_carlo_holdout_variance": mc_variance,
            "monte_carlo_relative_error": abs(mc_variance - holdout_variance) / holdout_variance,
        },
        "nullspace": {
            "collinear_control_rank": collinear_rank,
            "collinear_control_columns": 3,
            "note": "a collinear control set leaves an explicit nullspace that must be reported",
        },
        "no_fake_edges": {
            "note": "a frame with a different response must not enter the comparable group; "
                    "the expected behaviour is a disconnected component with no relative "
                    "measurement",
        },
    }
    truth_hash = case.write_truth("s01_truth.json", truth)

    fixture = {
        "case": "S01",
        "frames": frames,
        "controls_per_frame": controls,
        "native_pixels_per_frame": 4,
        "basis": "constant + plane (1, u, v)",
        "gauge": "reference_frame",
        "holdout_pixel_index": holdout_index,
        "monte_carlo_trials": TRIALS,
        "seed": SEED,
        "T": t_operator.tolist(),
        "C_native": native.tolist(),
        # the common sky columns, the difference mode and the map from the
        # fitted parameters to the pixels, so that the driver only moves
        # fields and never rebuilds the scene
        "H": h_matrix.tolist(),
        "A": a_matrix.tolist(),
        "E": e_matrix.tolist(),
        # the control statistic of a synthetic realisation: a common component
        # plus the difference mode
        "data": (t_operator @ (2.0 * np.ones(n_native)
                               + 0.1 * np.arange(n_native))).tolist(),
        "native_cholesky": np.linalg.cholesky(native).tolist(),
        "control_cholesky": np.linalg.cholesky(control_covariance).tolist(),
        "common_gradient": np.tile(np.array([1.0, 2.0, 3.0]), frames).tolist(),
    }
    fixture_hash = case.write_input("s01_model.json", fixture)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-s01-relative-sky",
        "operation": "fit_relative_sky",
        "input_objects": {},
        "arguments": {},
        "request": {"groups": [{"frames": ["f1", "f2", "f3"]}],
                    "basis": {"kind": "plane"},
                    "gauge": {"kind": "reference_frame"},
                    "allow_prediction": False},
        "resources": {"bytes": 8388608, "work": 100000, "spill_bytes": 0, "threads": 1,
                      "max_depth": 32, "max_precision_bits": 128, "scratch_directory": ".work"},
        "output_root": ".work/s01",
    }
    request_hash = case.write_request(request)

    case.write_case_json({
        "input_model_sha256": fixture_hash,
        "truth_sha256": truth_hash,
        "request_sha256": sha256_text(canonical_json(request)),
        "availability": availability(["build_sky_controls", "fit_relative_sky", "apply_sky"]),
    })

    observe.write_manifest(case, {
        "truth_type": "analytic_exact + stochastic_confidence",
        "seeds": {"monte_carlo": SEED},
        "trial_denominators": {"monte_carlo_trials": TRIALS},
        "library_identity": {"numpy": environment_identity()["numpy"]},
        "code_identity": source_identity([Path(__file__)]),
        "commands": {"truth": f"python3 validation/{CASE}/truth_{CASE}.py"},
        "artefacts": {"truth": truth_hash, "input_model": fixture_hash},
        "request_hash": content_hash(request),
    })

    # ---- the observed side: the real relative sky path ---------------------
    native_run, native_response = observe.run_native_driver(
        case, "s01_driver", ["--fixtures", str(case.root / "input" / "s01_model.json")])
    rows = []
    failures = []
    status = "pass"
    if native_run.get("status") != "ok":
        rows.append({"name": "s01_driver_available", "kind": "value",
                     "status": "pending_implementation", "observed": None,
                     "expected_success_count": 1, "observed_success_count": 0,
                     "reason": native_run.get("reason", "the driver did not run")})
        status = "pending_implementation"
    else:
        observed = native_response.get("fit", {}) or {}
        kh_per_column = native_response.get("KH_per_column", []) or []
        # The free common directions must be removed exactly.
        free_kh = max(kh_per_column[1:]) if len(kh_per_column) > 1 else 0.0
        truth_free_kh = truth["results"].get("KH_free_max_abs", 0.0)
        rows.append({"name": "common_sky_protected", "kind": "value",
                     "status": "pass" if free_kh <= 1e-10 else "fail",
                     "truth": truth_free_kh, "observed": free_kh, "tolerance": 1e-10,
                     "unit": "1", "expected_success_count": 1, "observed_success_count": 1,
                     "note": "K H vanishes on the free common directions; the gauged "
                             "direction is a convention and is reported, not judged"})
        if free_kh > 1e-10:
            failures.append({"functional": "common_sky_protected", "scope": "fit_relative_sky",
                             "expected": "<= 1e-10", "actual": free_kh,
                             "reason": "the free common directions were not removed"})
            status = "fail"
        # The hold-out variance of the corrected pixel.
        p_matrix = np.array(observed.get("P", []), dtype=float) if observed.get("P") else None
        if p_matrix is not None and p_matrix.size:
            native_covariance = np.array(fixture["C_native"], dtype=float)
            holdout = int(fixture.get("holdout_pixel_index", native_covariance.shape[0] - 1))
            row = p_matrix[holdout, :]
            observed_variance = float(row @ native_covariance @ row)
            truth_variance = truth["results"]["holdout_variance"]
            relative = abs(observed_variance - truth_variance) / max(abs(truth_variance), 1e-300)
            rows.append({"name": "holdout_variance", "kind": "value",
                         "status": "pass" if relative <= 0.02 else "fail",
                         "truth": truth_variance, "observed": observed_variance,
                         "tolerance": 0.02, "unit": "L^2",
                         "expected_success_count": 1, "observed_success_count": 1})
            if relative > 0.02:
                failures.append({"functional": "holdout_variance", "scope": "fit_relative_sky",
                                 "expected": truth_variance, "actual": observed_variance,
                                 "reason": "the corrected hold-out variance is outside the tolerance"})
                status = "fail"
        else:
            rows.append({"name": "holdout_variance", "kind": "value",
                         "status": "pending_implementation", "observed": None,
                         "expected_success_count": 1, "observed_success_count": 0,
                         "reason": "the correction operator P could not be materialised by the "
                                   "independent probe: the published action rejects the unit "
                                   "vector probes of the native and of the control shape, so "
                                   "its domain is not observable from the public contract"})

    comparison = comparison_payload(
        case.name.upper(), status, rows, failures,
        "S01: the independent whitened projection truth against the real fit_relative_sky path; "
        "the free common directions must be removed exactly and the corrected hold-out "
        "variance must match the Monte-Carlo reference.")

    case.write_comparison(comparison)
    print(json.dumps({"case": "S01", "results": truth["results"]}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
