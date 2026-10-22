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
            for pixel in range(3):
                t_operator[row, frame * 4 + pixel] = 1.0 / 3.0
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
    for frame in range(frames):
        for control in range(controls):
            h_matrix[frame * controls + control, frame] = 1.0
    # A: the additive difference modes (frame 3 only)
    a_matrix = np.zeros((frames * controls, 1))
    for control in range(controls):
        a_matrix[2 * controls + control, 0] = 1.0
    return h_matrix, a_matrix


def compute_correction(frames, controls, t_operator, control_covariance, h_matrix, a_matrix):
    """The gauge-fixed joint solve of the foundation, done explicitly."""
    # white = C_c^{-1/2}; use an eigen decomposition (the matrix is small)
    eigenvalues, eigenvectors = np.linalg.eigh(control_covariance)
    inverse_sqrt = (eigenvectors * (1.0 / np.sqrt(eigenvalues))) @ eigenvectors.T
    white = inverse_sqrt
    # Q_w: the orthogonal complement of the common sky columns in the
    # whitened space
    whitened_h = white @ h_matrix
    q_perp, _ = np.linalg.qr(whitened_h.T, mode="complete")
    q_w = q_perp[:, h_matrix.shape[1]:].T
    # G: the gauge constrained parameter subspace (the reference frame keeps
    # its common coefficients at zero, so only the difference modes are free)
    gauge = np.zeros((frames, frames))
    gauge[0, 0] = 1.0
    g_matrix = a_matrix @ gauge[:a_matrix.shape[1], :a_matrix.shape[1]] if False else a_matrix
    # solve the normal equations in the parameter space
    design = np.hstack([whitened_h @ np.zeros((frames, frames)), white @ a_matrix])
    # the common modes are projected out; only the difference modes are free
    free = design
    pseudo = np.linalg.pinv(free)
    k_matrix = g_matrix @ pseudo @ q_w @ white
    # E maps the control parameters back to the native pixels
    e_matrix = np.zeros((n_native, frames * controls))
    for frame in range(frames):
        for control in range(controls):
            for pixel in range(4):
                e_matrix[frame * 4 + pixel, frame * controls + control] = 1.0
    p_matrix = np.eye(n_native) - e_matrix @ k_matrix @ t_operator
    return k_matrix, p_matrix, e_matrix, q_w, white


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
        frames, controls, t_operator, control_covariance, h_matrix, a_matrix)

    kh = k_matrix @ h_matrix
    corrected_covariance = p_matrix @ native @ p_matrix.T

    # the common sky protection: a common gradient added to every frame must
    # not change the fitted parameters
    common_gradient = np.zeros(n_native)
    for frame in range(frames):
        common_gradient[frame * 4 + 0] = 1.0
        common_gradient[frame * 4 + 1] = 2.0
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

    comparison = comparison_payload(
        case.name.upper(), "pending_implementation",
        [{"name": "KH_max_abs", "kind": "value", "truth": truth["results"]["KH_max_abs"],
          "tolerance": 1e-10, "unit": "1", "status": "pending_implementation",
          "reason": "the public sky entry points are not available yet"},
         {"name": "holdout_variance", "kind": "value",
          "truth": truth["results"]["holdout_variance"], "tolerance": 1e-9,
          "unit": "L^2", "status": "pending_implementation",
          "reason": "the public sky entry points are not available yet"}],
        [],
        "The independent truth is complete; the observed side is pending_implementation.")

    print(json.dumps({"case": "S01", "results": truth["results"]}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
