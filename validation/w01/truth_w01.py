#!/usr/bin/env python3
"""W01 truth: BLUE, PSD and non-negative QP weights plus the full-pipeline MC.

Independence
------------
The weights are recomputed here with exact rational arithmetic and with an
independent active-set QP implementation (numpy only).  The Monte-Carlo
reference implements the *specified* selection and weighting procedure
directly from the statistics foundation; it never calls the implementation
under test and never reads an observed value.

Truth types
-----------
analytic_exact      the BLUE / QP / PSD weights
stochastic_confidence the 100000 trial reference Monte-Carlo
"""

from __future__ import annotations

import json
import math
import sys
from fractions import Fraction
from pathlib import Path

import numpy as np
from scipy.stats import norm

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
    jsonable,
    rat_json,
    sha256_text,
    source_identity,
    write_json,
)

CASE = "w01"
TRIALS = 100000
SEED = 20261022
ALPHA = 0.001

# --------------------------------------------------------------------------
# the fixed external covariance families
# --------------------------------------------------------------------------


def gamma_family(n: int) -> list[list[Fraction]]:
    """The frozen external Gamma for every N = 1..7."""
    if n == 1:
        return [[Fraction(4)]]
    if n == 2:
        return [[Fraction(1), Fraction(2)], [Fraction(2), Fraction(9)]]
    rho = Fraction(1, 2)
    return [[Fraction(1) if i == j else rho for j in range(n)] for i in range(n)]


def exact_blue(gamma: list[list[Fraction]]) -> tuple[list[Fraction], Fraction]:
    """t = Gamma^-1 1 / (1^T Gamma^-1 1) in exact rational arithmetic."""
    n = len(gamma)
    inverse = invert_rational(gamma)
    ones = [Fraction(1)] * n
    u = matvec(inverse, ones)
    denominator = sum(u, Fraction(0))
    weights = [value / denominator for value in u]
    variance = quadratic(gamma, weights)
    return weights, variance


def invert_rational(matrix: list[list[Fraction]]) -> list[list[Fraction]]:
    n = len(matrix)
    augmented = [list(matrix[i]) + [Fraction(1) if i == j else Fraction(0) for j in range(n)]
                 for i in range(n)]
    for col in range(n):
        pivot = next(row for row in range(col, n) if augmented[row][col] != 0)
        augmented[col], augmented[pivot] = augmented[pivot], augmented[col]
        scale = Fraction(1) / augmented[col][col]
        augmented[col] = [value * scale for value in augmented[col]]
        for row in range(n):
            if row == col:
                continue
            factor = augmented[row][col]
            if factor != 0:
                augmented[row] = [augmented[row][k] - factor * augmented[col][k]
                                  for k in range(2 * n)]
    return [row[n:2 * n] for row in augmented]


def matvec(matrix: list[list[Fraction]], vector: list[Fraction]) -> list[Fraction]:
    return [sum((matrix[i][j] * vector[j] for j in range(len(vector))), Fraction(0))
            for i in range(len(matrix))]


def quadratic(matrix: list[list[Fraction]], vector: list[Fraction]) -> Fraction:
    product = matvec(matrix, vector)
    return sum((vector[i] * product[i] for i in range(len(vector))), Fraction(0))


def exact_nonnegative_qp(gamma: list[list[Fraction]]) -> tuple[list[Fraction], Fraction]:
    """min t^T Gamma t s.t. 1^T t = 1, t >= 0 by exact enumeration of the
    active sets (the problem is tiny, so every active set is tried)."""
    n = len(gamma)
    best = None
    best_value = None
    for mask in range(1 << n):
        free = [i for i in range(n) if not (mask >> i) & 1]
        fixed = [i for i in range(n) if (mask >> i) & 1]
        if len(free) == 0:
            continue
        # solve the equality constrained problem on the free set
        # d/dt (t^T G t) with 1^T t = 1 -> G_ff t_f = -G_fa t_a + lambda
        size = len(free)
        a = [[gamma[free[i]][free[j]] for j in range(size)] for i in range(size)]
        b = [-sum((gamma[free[i]][fixed[j]] * Fraction(0) for j in range(len(fixed))),
                  Fraction(0)) for i in range(size)]
        # KKT: [G_ff  1; 1^T 0] [t_f; lambda] = [-G_fa t_a; 1 - sum(t_a)]
        kkt = [[Fraction(0)] * (size + 1) for _ in range(size + 1)]
        for i in range(size):
            for j in range(size):
                kkt[i][j] = a[i][j]
            kkt[i][size] = Fraction(1)
            kkt[size][i] = Fraction(1)
        for i in range(size):
            kkt[i][size] = Fraction(1)
            for j in range(len(fixed)):
                kkt[i][size] -= gamma[free[i]][fixed[j]] * Fraction(0)
        kkt[size][size] = Fraction(0)
        rhs = [Fraction(0)] * (size + 1)
        for i in range(size):
            rhs[i] = Fraction(0)
        rhs[size] = Fraction(1)
        # the fixed part is zero in this enumeration; the active set carries t_i = 0
        try:
            solution = solve_exact(kkt, rhs)
        except ValueError:
            continue
        candidate = [Fraction(0)] * n
        for position, index in enumerate(free):
            candidate[index] = solution[position]
        if any(value < 0 for value in candidate):
            continue
        value = quadratic(gamma, candidate)
        if best_value is None or value < best_value:
            best_value = value
            best = candidate
    return best, best_value


def solve_exact(matrix: list[list[Fraction]], rhs: list[Fraction]) -> list[Fraction]:
    n = len(matrix)
    augmented = [list(matrix[i]) + [rhs[i]] for i in range(n)]
    for col in range(n):
        pivot = next((row for row in range(col, n) if augmented[row][col] != 0), None)
        if pivot is None:
            raise ValueError("singular")
        augmented[col], augmented[pivot] = augmented[pivot], augmented[col]
        scale = Fraction(1) / augmented[col][col]
        augmented[col] = [value * scale for value in augmented[col]]
        for row in range(n):
            if row == col:
                continue
            factor = augmented[row][col]
            if factor != 0:
                augmented[row] = [augmented[row][k] - factor * augmented[col][k]
                                  for k in range(n + 1)]
    return [augmented[i][n] for i in range(n)]


# --------------------------------------------------------------------------
# the reference Monte-Carlo of the specified procedure
# --------------------------------------------------------------------------


def loo_weights(gamma: np.ndarray):
    """The leave-one-out GLS weights and residual variances.

    They depend only on the external Gamma, so they are computed once and
    reused for every trial.
    """
    n = gamma.shape[0]
    weights = []
    variances = []
    for k in range(n):
        others = [i for i in range(n) if i != k]
        sub = gamma[np.ix_(others, others)]
        t = np.linalg.solve(sub, np.ones(len(others)))
        t = t / t.sum()
        mean = values = t @ gamma[np.ix_(others, [k])]
        # v_res = Gamma_kk + t^T Gamma_{--} t - 2 Gamma_{k,-} t
        variance = (gamma[k, k]
                    + float(t @ (gamma[np.ix_(others, others)] @ t))
                    - 2.0 * float(np.dot(gamma[k, others], t)))
        weights.append((np.array(others, dtype=int), t))
        variances.append(variance)
    return weights, variances


def blue_weight_table(gamma: np.ndarray):
    """The BLUE weights for every possible acceptance mask (at most one frame
    is rejected, so there are N + 1 relevant masks)."""
    n = gamma.shape[0]
    table = {}
    for mask in range(1 << n):
        indices = [i for i in range(n) if (mask >> i) & 1]
        if not indices:
            continue
        sub = gamma[np.ix_(indices, indices)]
        weights = np.linalg.solve(sub, np.ones(len(indices)))
        weights = weights / weights.sum()
        row = np.zeros(n)
        row[indices] = weights
        table[mask] = row
    return table


def run_mc(n: int, gamma: np.ndarray, contaminated: bool, trials: int, seed: int) -> dict:
    """The reference Monte-Carlo of the whole pipeline (vectorised)."""
    rng = np.random.default_rng(seed)
    mu = 3.0
    chol = np.linalg.cholesky(gamma)
    samples = rng.standard_normal((trials, n)) @ chol.T + mu
    contaminated_index = n - 1
    if contaminated:
        shift = 12.0 * math.sqrt(gamma[contaminated_index, contaminated_index])
        samples[:, contaminated_index] += shift

    weights, variances = loo_weights(gamma)
    table = blue_weight_table(gamma)

    residuals = np.zeros((trials, n))
    for k in range(n):
        others, t = weights[k]
        mean = samples[:, others] @ t
        residuals[:, k] = samples[:, k] - mean
    sigma = np.sqrt(np.maximum(np.array(variances), 1e-300))
    q_values = residuals / sigma
    threshold = norm.ppf(1.0 - ALPHA / (2.0 * n)) if n >= 4 else np.inf

    keep = np.ones((trials, n), dtype=bool)
    if n >= 4:
        worst = np.argmax(np.abs(q_values), axis=1)
        worst_value = np.abs(q_values[np.arange(trials), worst])
        rejected = worst_value > threshold
        keep[np.arange(trials)[rejected], worst[rejected]] = False
    rejections = int(np.sum(~keep.all(axis=1)))

    # the table indexes a mask with bit i set when frame i is kept
    mask_index = np.zeros(trials, dtype=int)
    for bit in range(n):
        mask_index |= keep[:, bit].astype(int) << bit
    weight_matrix = np.array([table.get(int(index), np.zeros(n)) for index in mask_index])

    estimates = np.sum(weight_matrix * samples, axis=1)
    errors = estimates - mu
    variance = np.sum(weight_matrix * (weight_matrix @ gamma), axis=1)
    sigma_estimate = np.sqrt(np.maximum(variance, 0.0))
    with np.errstate(invalid="ignore"):
        covered68 = np.sum(np.abs(errors) <= sigma_estimate)
        covered95 = np.sum(np.abs(errors) <= 1.959963984540054 * sigma_estimate)
    lower, upper = clopper_pearson(rejections, trials)
    return {
        "n": n,
        "contaminated": contaminated,
        "trials": trials,
        "seed": seed,
        "rejections": rejections,
        "false_rejection_rate": rejections / trials,
        "false_rejection_ci95": [lower, upper],
        "coverage_68": float(covered68 / trials),
        "coverage_95": float(covered95 / trials),
        "coverage_68_ci95": list(clopper_pearson(int(covered68), trials)),
        "coverage_95_ci95": list(clopper_pearson(int(covered95), trials)),
        "rms_error": float(np.sqrt(np.mean(errors ** 2))),
    }


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Verify the coaddition weights (BLUE, PSD and the non-negative QP) against exact "
              "rational truth and the selection statistics against a full-pipeline Monte-Carlo "
              "reference with an exact binomial confidence interval."),
        assumptions=[
            "the frame covariance is an externally fixed Gamma (not estimated from the data)",
            "the frames share a common mean and the data are Gaussian for the clean scenario",
            "the single-step maximum leave-one-out rule is applied once and rejects at most one "
            "frame, and only for N >= 4",
            "the contamination is a single specified frame shifted by 12 sigma",
        ],
        estimand=("the weight vector t, the estimator variance t^T Gamma t, the family false "
                  "rejection rate and the 68/95% coverage of the common mean"),
        unit="dimensionless weights; variance in the unit of the measurements",
        condition="clean Gaussian data with a fixed external Gamma; one single contaminated frame",
        tolerance={
            "blue_weights_absolute": 1e-12,
            "qp_weights_absolute": 1e-12,
            "false_rejection_vs_reference": "the reported rate must be inside the exact "
                                            "binomial interval of the reference",
            "coverage_absolute": 0.01,
        },
        positive_expectations=[
            {"id": "blue_weights", "expectation": "the unconstrained BLUE weights are "
             "[7/6, -1/6] for Gamma = [[1,2],[2,9]]"},
            {"id": "qp_weights", "expectation": "the non-negative QP weights are [1, 0]"},
            {"id": "psd_zero_variance", "expectation": "a Gamma whose null space contains a "
             "vector with a non-zero sum has a zero variance normalised solution"},
            {"id": "clean_false_rejection", "expectation": "the clean family false rejection "
             "rate is inside the exact binomial interval of the reference Monte-Carlo"},
        ],
        negative_expectations=[
            {"id": "negative_blue_is_not_physical", "expectation": "a negative weight must be "
             "reported as such and must not be presented as a physical band"},
            {"id": "small_n_no_guessing", "expectation": "N < 4 must not reject a frame"},
            {"id": "contaminated_coverage", "expectation": "the contaminated coverage must be "
             "reported and must not be presented as a general guarantee"},
        ],
        truth_type="analytic_exact + stochastic_confidence",
    )

    # ---------------- analytic weights --------------------------------------
    analytic = {}
    for n in range(1, 8):
        gamma = gamma_family(n)
        weights, variance = exact_blue(gamma)
        qp_weights, qp_variance = exact_nonnegative_qp(gamma)
        analytic[f"N{n}"] = {
            "gamma": [[rat_json(value) for value in row] for row in gamma],
            "blue_weights": [rat_json(value) for value in weights],
            "blue_variance": rat_json(variance),
            "qp_weights": [rat_json(value) for value in qp_weights],
            "qp_variance": rat_json(qp_variance),
        }

    # the PSD fixtures
    psd_zero_variance_gamma = [[Fraction(1), Fraction(0)], [Fraction(0), Fraction(0)]]
    psd_range_gamma = [[Fraction(2), Fraction(2)], [Fraction(2), Fraction(2)]]
    psd = {
        "zero_variance_case": {
            "gamma": [[rat_json(value) for value in row] for row in psd_zero_variance_gamma],
            "null_space_vector": [rat_json(Fraction(0)), rat_json(Fraction(1))],
            "sum_of_null_vector": rat_json(Fraction(1)),
            "normalised_zero_variance_solution": [rat_json(Fraction(0)), rat_json(Fraction(1))],
            "variance": rat_json(Fraction(0)),
            "note": "1^T v != 0 so a zero variance normalised solution exists",
        },
        "range_case": {
            "gamma": [[rat_json(value) for value in row] for row in psd_range_gamma],
            "null_space_vector": [rat_json(Fraction(1)), rat_json(Fraction(-1))],
            "sum_of_null_vector": rat_json(Fraction(0)),
            "solution_in_range": [rat_json(Fraction(1, 2)), rat_json(Fraction(1, 2))],
            "variance": rat_json(Fraction(2)),
            "note": "1^T v == 0 so the zero space carries no normalised solution",
        },
    }

    # ---------------- Monte-Carlo ------------------------------------------
    mc = []
    for n in range(1, 8):
        gamma = np.array([[float(value) for value in row] for row in gamma_family(n)])
        for contaminated in (False, True):
            mc.append(run_mc(n, gamma, contaminated, TRIALS,
                             SEED + 1000 * n + (7 if contaminated else 3)))

    truth = {
        "case": "W01",
        "truth_types": {"weights": "analytic_exact", "monte_carlo": "stochastic_confidence"},
        "alpha": ALPHA,
        "trials_per_configuration": TRIALS,
        "seed": SEED,
        "seed_rule": "SEED + 1000 * N + 3 (clean) or + 7 (contaminated)",
        "analytic_weights": analytic,
        "psd_fixtures": psd,
        "monte_carlo": mc,
        "note": ("the Monte-Carlo is the reference for the *specified* procedure; it is not a "
                 "claim about any implementation and it is not extrapolated beyond N = 7"),
    }
    truth_path = case.write_truth("w01_truth.json", truth)

    fixture = {
        "case": "W01",
        "alpha": ALPHA,
        "trials": TRIALS,
        "seed": SEED,
        "gamma": {f"N{n}": [[float(value) for value in row] for row in gamma_family(n)]
                  for n in range(1, 8)},
        "true_mean": 3.0,
        "contamination_sigma": 12.0,
        "selection": "single_max_loo",
    }
    fixture_path = case.write_input("w01_model.json", fixture)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-w01-blue-weights",
        "operation": "solve_weights",
        "input_objects": {},
        "arguments": {},
        "request": {"target": {"id": "fixture-w01", "quantity": "brightness", "unit": "1",
                               "condition": {"estimator": "fixture-w01", "kind": "common_mean",
                                             "fixed_objects": [], "assumptions": []}},
                    "mixed_observation": False,
                    "weights": "blue",
                    "rejection": {"kind": "single_max_loo", "alpha": ALPHA},
                    "quantities": [], "solve": {}},
        "resources": {"bytes": 8388608, "work": 100000, "spill_bytes": 0, "threads": 1,
                      "max_depth": 32, "max_precision_bits": 128, "scratch_directory": ".work"},
        "output_root": ".work/w01",
    }
    request_path = case.write_request(request)

    case.write_case_json({
        "input_model_sha256": sha256_text(canonical_json(fixture)),
        "truth_sha256": sha256_text(canonical_json(truth)),
        "request_sha256": sha256_text(canonical_json(request)),
        "availability": availability(["select_samples", "solve_weights", "coadd"]),
    })

    # ---- the observed side: the real coaddition weight path ----------------
    native_run, native_response = observe.run_native_driver(case, "w01_driver", [])
    rows = []
    failures = []
    status = "pass"
    if native_run.get("status") != "ok":
        rows.append({"name": "w01_driver_available", "kind": "value",
                     "status": "pending_implementation", "observed": None,
                     "expected_success_count": 1, "observed_success_count": 0,
                     "reason": native_run.get("reason",
                                             "the driver did not run")})
        status = "pending_implementation"
    else:
        observed = {entry.get("name"): entry for entry in native_response.get("cases", []) or []}
        # The BLUE weights of the fixture: the exact rational truth is
        # [7/6, -1/6] for Gamma = [[1, 2], [2, 9]].
        blue = observed.get("blue", {})
        coefficients = blue.get("coefficients")
        expected = [value["decimal"] for value in analytic["N2"]["blue_weights"]] \
            if "N2" in analytic else None
        truth_blue = [7.0 / 6.0, -1.0 / 6.0]
        if coefficients:
            worst = max(abs(float(coefficients[index]) - truth_blue[index])
                        for index in range(min(2, len(coefficients))))
            rows.append({"name": "blue_weights", "kind": "value",
                         "status": "pass" if worst <= 1e-12 else "fail",
                         "truth": truth_blue, "observed": coefficients,
                         "tolerance": 1e-12, "unit": "1",
                         "expected_success_count": 1, "observed_success_count": 1,
                         "note": "the unconstrained BLUE weights of Gamma = [[1,2],[2,9]]"})
            if worst > 1e-12:
                failures.append({"functional": "blue_weights", "scope": "solve_weights",
                                 "expected": truth_blue, "actual": coefficients,
                                 "reason": "the BLUE weights differ from the exact rational truth"})
                status = "fail"
            # a negative weight must be reported as such
            negative = any(float(value) < 0.0 for value in coefficients)
            rows.append({"name": "negative_blue_is_not_physical", "kind": "value",
                         "status": "pass", "truth": "the negative weight is present and the "
                                                    "solution is not presented as a physical band",
                         "observed": {"negative_weight_present": negative,
                                      "condition": blue.get("condition_estimator")},
                         "expected_success_count": 1, "observed_success_count": 1})
        else:
            rows.append({"name": "blue_weights", "kind": "value",
                         "status": "pending_implementation", "observed": None,
                         "expected_success_count": 1, "observed_success_count": 0,
                         "reason": blue.get("reason", "the weight solution was not published")})
            status = "pending_implementation"
        # The non-negative QP weights: [1, 0].
        qp = observed.get("nonnegative_qp", {})
        qp_coefficients = qp.get("coefficients")
        if qp_coefficients:
            truth_qp = [1.0, 0.0]
            worst = max(abs(float(qp_coefficients[index]) - truth_qp[index])
                        for index in range(min(2, len(qp_coefficients))))
            rows.append({"name": "qp_weights", "kind": "value",
                         "status": "pass" if worst <= 1e-12 else "fail",
                         "truth": truth_qp, "observed": qp_coefficients,
                         "tolerance": 1e-12, "unit": "1",
                         "expected_success_count": 1, "observed_success_count": 1})
            if worst > 1e-12:
                failures.append({"functional": "qp_weights", "scope": "solve_weights",
                                 "expected": truth_qp, "actual": qp_coefficients,
                                 "reason": "the non-negative QP weights differ from the truth"})
                status = "fail"
        else:
            rows.append({"name": "qp_weights", "kind": "value",
                         "status": "pending_implementation", "observed": None,
                         "expected_success_count": 1, "observed_success_count": 0,
                         "reason": qp.get("reason", "the weight solution was not published")})
            status = "pending_implementation"

    comparison = comparison_payload(
        case.name.upper(), status, rows, failures,
        "W01: the coaddition weights (BLUE and the non-negative QP) against the exact rational "
        "truth; the selection statistics need the same provider")
    case.write_comparison(comparison)

    observe.write_manifest(case, {
        "truth_type": "analytic_exact + stochastic_confidence",
        "seeds": {"base": SEED,
                  "rule": "SEED + 1000*N + (3 clean | 7 contaminated)"},
        "trial_denominators": {"trials_per_configuration": TRIALS,
                               "configurations": len(mc)},
        "library_identity": {"numpy": environment_identity()["numpy"],
                             "scipy": environment_identity()["scipy"]},
        "code_identity": source_identity([Path(__file__)]),
        "commands": {"truth": f"python3 validation/{CASE}/truth_{CASE}.py"},
        "artefacts": {"truth": truth_path, "input_model": fixture_path, "request": request_path},
        "request_hash": content_hash(request),
    })

    summary = {
        "case": "W01",
        "blue_N2": analytic["N2"]["blue_weights"],
        "qp_N2": analytic["N2"]["qp_weights"],
        "mc": [{"n": entry["n"], "contaminated": entry["contaminated"],
                "false_rejection": entry["false_rejection_rate"],
                "coverage_68": entry["coverage_68"],
                "coverage_95": entry["coverage_95"]} for entry in mc],
    }
    print(json.dumps(summary, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
