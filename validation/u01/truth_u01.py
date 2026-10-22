#!/usr/bin/env python3
"""U01 truth: same-R matrix, covariance and signed aperture variance.

Independence
------------
The truth below is recomputed here with exact rational arithmetic
(``fractions.Fraction``).  The numbers quoted in the specification are *not*
read by this script: the matrices are re-entered from the specification text,
the products are recomputed, and the results are compared with the
specification only afterwards as a consistency statement.

The script never imports anything from ``src/`` and never looks at an
observed run.

Truth type: analytic_exact
"""

from __future__ import annotations

import json
import os
import sys
from fractions import Fraction
from pathlib import Path

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
    matmul_rat,
    matvec_rat,
    mat_rat_json,
    quadratic_form_rat,
    rat,
    rat_json,
    sha256_text,
    solve_rat,
    source_identity,
    write_json,
)

CASE = "u01"

# --------------------------------------------------------------------------
# frozen input model (exact rationals)
# --------------------------------------------------------------------------

R = [[Fraction(1, 2), Fraction(1, 2), Fraction(0)],
     [Fraction(0), Fraction(1, 4), Fraction(3, 4)]]
C = [[Fraction(4), Fraction(1), Fraction(0)],
     [Fraction(1), Fraction(9), Fraction(2)],
     [Fraction(0), Fraction(2), Fraction(16)]]
T = [Fraction(1), Fraction(-1, 2)]

# The specification quotes C as the Gram matrix of a 3 x 24 factor.  The
# factor below is rebuilt from the matrix itself (exact Cholesky over the
# rationals is not available because the factor is irrational); instead the
# factor is reconstructed column-wise so that F F^T reproduces C exactly.
def build_factor_f():
    """The 3 x 24 factor of the public example, re-entered row by row.

    Row 0 owns latent columns 0, 2, 3, 4; row 1 owns 0, 1, 5..11; row 2 owns
    column 1 with coefficient 2 and columns 12..23.  The Gram matrix of this
    factor is C by construction; the script verifies that numerically with
    exact integer arithmetic.
    """
    rows = [
        {0: 1, 2: 1, 3: 1, 4: 1},
        {0: 1, 1: 1, 5: 1, 6: 1, 7: 1, 8: 1, 9: 1, 10: 1, 11: 1},
        {1: 2, 12: 1, 13: 1, 14: 1, 15: 1, 16: 1, 17: 1, 18: 1, 19: 1,
         20: 1, 21: 1, 22: 1, 23: 1},
    ]
    latent_columns = 24
    offsets = [0]
    indices = []
    values = []
    for row in rows:
        for column in sorted(row):
            indices.append(column)
            values.append(row[column])
        offsets.append(len(indices))
    return offsets, indices, values, latent_columns


def gram_of_factor(offsets, indices, values, rows_count):
    """Exact integer Gram matrix of the sparse factor."""
    gram = [[0] * rows_count for _ in range(rows_count)]
    for i in range(rows_count):
        for j in range(rows_count):
            total = 0
            for k in range(offsets[i], offsets[i + 1]):
                for l in range(offsets[j], offsets[j + 1]):
                    if indices[k] == indices[l]:
                        total += values[k] * values[l]
            gram[i][j] = total
    return gram


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Exercise the public matrix path: storage.copy / make_action / make_covariance / "
              "apply / transpose_apply / covariance_apply / quadratic_form on an explicit "
              "rational matrix, and falsify the 'split a shared source into independent "
              "pieces' shortcut."),
        assumptions=[
            "the native covariance is exactly C (L^2) and the row operator is exactly R (sr^-1)",
            "the aperture functional is the signed vector t = [1, -1/2] on the leaf domain",
            "the leaf covariance is R C R^T, computed by the same single R that acts on the signal",
            "the shared latent is one random variable; splitting it changes the covariance",
        ],
        estimand=("leaf covariance matrix R C R^T and the signed aperture variance "
                  "t^T (R C R^T) t"),
        unit="leaf variance (L^2 sr^-2); aperture variance (L^2 sr^-2)",
        condition="fixed linear model, one shared Gaussian latent, no model bias",
        tolerance={
            "leaf_covariance_absolute": "1e-12 (L^2 sr^-2)",
            "aperture_variance_absolute": "1e-12 (L^2 sr^-2)",
            "false_independence_separation_relative": 0.01,
            "shared_latent_variance_absolute": "1e-12",
        },
        positive_expectations=[
            {"id": "leaf_covariance", "expectation": "R C R^T equals [[15/4, 2], [2, 165/16]]"},
            {"id": "aperture_variance", "expectation": "t^T R C R^T t equals 277/64"},
            {"id": "native_route_agreement",
             "expectation": "quadratic_form(C_native, R^T t) equals the leaf-domain value"},
        ],
        negative_expectations=[
            {"id": "split_shared_source",
             "expectation": ("treating the shared latent as independent must NOT reproduce the "
                             "truth; the false value must differ by more than the tolerance")},
            {"id": "no_zero_fill",
             "expectation": "an output array must not decode to zeros"},
        ],
        truth_type="analytic_exact",
    )

    # ---------------- truth -------------------------------------------------
    r_transpose = [[R[0][0], R[1][0]], [R[0][1], R[1][1]], [R[0][2], R[1][2]]]
    h = matmul_rat(matmul_rat(R, C), r_transpose)
    u = matvec_rat(r_transpose, T)
    aperture = quadratic_form_rat(h, T)
    native_aperture = quadratic_form_rat(C, u)

    # false independence: drop the cross terms of the native covariance
    c_diagonal = [[C[i][j] if i == j else Fraction(0) for j in range(3)] for i in range(3)]
    false_independence_native = quadratic_form_rat(c_diagonal, u)
    h_diagonal = [[h[i][j] if i == j else Fraction(0) for j in range(2)] for i in range(2)]
    false_independence_leaf = quadratic_form_rat(h_diagonal, T)

    offsets, indices, values, latent_columns = build_factor_f()
    gram = gram_of_factor(offsets, indices, values, 3)
    gram_matches = all(gram[i][j] == C[i][j] for i in range(3) for j in range(3))

    # shared-latent fixture: one latent z with Var = 4 seen by two samples
    shared_true = Fraction(0)          # Var(z - z) with one shared z
    shared_false = Fraction(8)         # Var(z1) + Var(z2) with independent z1, z2
    shared_variance = Fraction(4)

    truth = {
        "case": "U01",
        "truth_type": "analytic_exact",
        "method": ("exact rational arithmetic (fractions.Fraction); every product is recomputed "
                   "here from the re-entered input matrices"),
        "input_model": {
            "R_sr_inv": mat_rat_json(R),
            "C_l2": mat_rat_json(C),
            "t": [rat_json(value) for value in T],
            "R_transpose_times_t": [rat_json(value) for value in u],
        },
        "results": {
            "leaf_covariance_RCRt": mat_rat_json(h),
            "aperture_variance_t_RCRt_t": rat_json(aperture),
            "aperture_variance_native_route": rat_json(native_aperture),
            "false_independence_native_variance": rat_json(false_independence_native),
            "false_independence_leaf_variance": rat_json(false_independence_leaf),
            "shared_latent_true_variance": rat_json(shared_true),
            "shared_latent_false_independence_variance": rat_json(shared_false),
            "shared_latent_variance": rat_json(shared_variance),
        },
        "factor_check": {
            "latent_columns": latent_columns,
            "offsets": offsets,
            "indices": indices,
            "values": values,
            "gram_matrix": gram,
            "gram_equals_C": gram_matches,
        },
        "separation": {
            "native_false_minus_true": rat_json(false_independence_native - aperture),
            "leaf_false_minus_true": rat_json(false_independence_leaf - aperture),
            "shared_false_minus_true": rat_json(shared_false - shared_true),
        },
    }

    # cross-check against the numbers quoted in the specification, stated as a
    # consistency statement only (the truth above does not depend on it)
    quoted = {
        "leaf_covariance": [[Fraction(15, 4), Fraction(2)], [Fraction(2), Fraction(165, 16)]],
        "aperture_variance": Fraction(277, 64),
    }
    truth["specification_consistency"] = {
        "leaf_covariance_matches": h == quoted["leaf_covariance"],
        "aperture_variance_matches": aperture == quoted["aperture_variance"],
        "note": ("the quoted values are only compared after the independent recomputation; they "
                 "are never an input to it"),
    }

    truth_path = case.write_truth("u01_truth.json", truth)

    # ---------------- frozen input ----------------------------------------
    input_model = {
        "case": "U01",
        "note": "the exact numeric model handed to the implementation under test",
        "R": {
            "shape": [2, 3],
            "unit": {"scale": "fixture:L", "linear_power": 1, "sr_power": -1, "pixel_power": 0,
                     "nm_power": 0, "multiplier": 1},
            "values": [[float(value) for value in row] for row in R],
            "exact": mat_rat_json(R),
        },
        "C": {
            "shape": [3, 3],
            "unit": {"scale": "fixture:L", "linear_power": 2, "sr_power": 0, "pixel_power": 0,
                     "nm_power": 0, "multiplier": 1},
            "values": [[float(value) for value in row] for row in C],
            "exact": mat_rat_json(C),
        },
        "t": {"shape": [2], "values": [float(value) for value in T],
              "exact": [rat_json(value) for value in T]},
        "F": {
            "shape": [3, 24],
            "offsets": offsets,
            "indices": indices,
            "values": values,
            "unit": {"scale": "fixture:L", "linear_power": 1, "sr_power": 0, "pixel_power": 0,
                     "nm_power": 0, "multiplier": 1},
        },
        "shared_latent_fixture": {
            "description": "two native samples share one latent z with Var(z) = 4",
            "latent_variance": rat_json(shared_variance),
            "aperture": [1, -1],
        },
    }
    input_path = case.write_input("u01_model.json", input_model)

    # ---------------- frozen contract request ------------------------------
    request = build_contract_request()
    request_path = case.write_request(request)

    case.write_case_json({
        "input_model_sha256": sha256_text(canonical_json(input_model)),
        "truth_sha256": sha256_text(canonical_json(truth)),
        "request_sha256": sha256_text(canonical_json(request)),
        "observed_route": ["in_memory_public_api (validation/native/u01_driver.cpp)",
                           "acsd-contract process driver"],
    })

    manifest = {
        "case": "U01",
        "truth_type": "analytic_exact",
        "seeds": [],
        "trial_denominators": {"mc_trials": 0},
        "code_identity": source_identity([Path(__file__)]),
        "library_identity": environment_identity(),
        "dependencies": {
            "python": "fractions (standard library) only; no production module is imported",
            "native_driver": "validation/native/u01_driver.cpp (includes only <acsd/contracts.hpp>)",
            "production_library": "src/model, src/numerics, src/operators, src/uncertainty",
        },
        "commands": {
            "truth": f"python3 validation/{CASE}/truth_{CASE}.py",
            "native_driver": "bash validation/lib/build_native.sh u01_driver && "
                             f"validation/build/u01_driver --output validation/{CASE}/observed/native_u01.json",
            "contract_driver": f"acsd-contract --request validation/{CASE}/request.json "
                               f"--response validation/{CASE}/observed/contract_u01.json",
        },
        "artefacts": {
            "truth": truth_path,
            "input_model": input_path,
            "request": request_path,
        },
        "request_hash": content_hash(request),
    }
    case.write_manifest(manifest)

    # ---------------- observed -------------------------------------------
    native_run, native_response = observe.run_native_driver(case, "u01_driver")
    contract_run, contract_response = observe.run_contract_driver(case)

    observed = observe.observed_values(native_response)
    functionals = [
        {"name": "apply_R_ones", "truth": 1.0, "tolerance": 1e-12, "unit": "L/sr"},
        {"name": "apply_R_ones_second", "truth": 1.0, "tolerance": 1e-12, "unit": "L/sr"},
        {"name": "transpose_R_t_0", "truth": 0.5, "tolerance": 1e-12, "unit": "L"},
        {"name": "transpose_R_t_1", "truth": 0.375, "tolerance": 1e-12, "unit": "L"},
        {"name": "transpose_R_t_2", "truth": -0.375, "tolerance": 1e-12, "unit": "L"},
        {"name": "leaf_quadratic_form", "truth": 4.328125, "tolerance": 1e-12,
         "unit": "L^2 sr^-2"},
        {"name": "native_quadratic_form", "truth": 4.328125, "tolerance": 1e-12,
         "unit": "L^2"},
        {"name": "false_independence_quadratic_form", "truth": 4.515625, "tolerance": 1e-12,
         "unit": "L^2"},
        {"name": "covariance_column_1_0", "truth": 3.75, "tolerance": 1e-12,
         "unit": "L^2 sr^-2"},
        {"name": "covariance_column_1_1", "truth": 2.0, "tolerance": 1e-12, "unit": "L^2 sr^-2"},
        {"name": "covariance_column_2_0", "truth": 2.0, "tolerance": 1e-12, "unit": "L^2 sr^-2"},
        {"name": "covariance_column_2_1", "truth": 10.3125, "tolerance": 1e-12,
         "unit": "L^2 sr^-2"},
        # the false-independence shortcut must be separated from the truth
        {"name": "split_shared_source", "kind": "differ", "first": "leaf_quadratic_form",
         "second": "false_independence_quadratic_form", "tolerance": 1e-6},
    ]
    observed_status = "ok" if native_run.get("status") == "ok" else "pending_implementation"
    comparison = observe.compare(case, functionals, observed,
                                  observe.observed_texts(native_response), observed_status,
                                  native_run.get("reason", ""),
                                  notes=("U01: the leaf route, the native route and the "
                                         "false-independence control are compared with the "
                                         "independent exact rational truth"))

    # the preserved failed attempts (not overwritten by the current run)
    attempts = case.root / "observed" / "attempts"
    attempts.mkdir(parents=True, exist_ok=True)
    preserved = [
        {
            "attempt": 1,
            "date": "before commit bb90d6663 (encoder byte count)",
            "status": "fail",
            "scope": "public API storage.copy / write_f64 payload size",
            "expected": "an output array of N f64 carries N*8 bytes",
            "actual": "the public encoder emitted 16 bytes per element (bytes=32 for 2 "
                      "elements); storage.copy rejected the payload with invalid_request",
            "evidence": "validation/native/u01_driver.cpp encoder payload probe",
            "reported_to": "lead",
            "fixed_in": "bb90d6663",
        },
        {
            "attempt": 2,
            "date": "before commit 17d6258fe (covariance linear helper)",
            "status": "fail",
            "scope": "uncertainty::apply_linear / apply_linear_transposed read back",
            "expected": "quadratic_form on the leaf covariance returns 277/64",
            "actual": "invalid_request: the helper read the action output with the input "
                      "length, so a dimension changing action overran or truncated",
            "evidence": "validation/native/u01_driver.cpp leaf route probe",
            "reported_to": "lead",
            "fixed_in": "17d6258fe",
        },
    ]
    for attempt in preserved:
        write_json(attempts / f"attempt_{attempt['attempt']:02d}.json", attempt)

    print(json.dumps({
        "case": "U01",
        "leaf_covariance": [[rat_str(value) for value in row] for row in h],
        "aperture_variance": rat_str(aperture),
        "false_independence_native": rat_str(false_independence_native),
        "spec_consistent": truth["specification_consistency"],
        "comparison_status": comparison["status"],
    }, indent=2))
    return 0


def rat_str(value: Fraction) -> str:
    from lib.vcommon import rat_str as _rat_str
    return _rat_str(value)


def build_contract_request() -> dict:
    """The frozen contract-wire request for the leaf-domain quadratic form."""
    unit_dim = {"scale": "1", "linear_power": 0, "sr_power": 0, "pixel_power": 0,
                "nm_power": 0, "multiplier": 1}
    unit_signal = {"scale": "fixture:L", "linear_power": 1, "sr_power": 0, "pixel_power": 0,
                   "nm_power": 0, "multiplier": 1}
    unit_brightness = {"scale": "fixture:L", "linear_power": 1, "sr_power": -1, "pixel_power": 0,
                       "nm_power": 0, "multiplier": 1}
    unit_variance_brightness = {"scale": "fixture:L", "linear_power": 2, "sr_power": -2,
                                 "pixel_power": 0, "nm_power": 0, "multiplier": 1}

    def index(name, size, kind):
        return {"type": "Index", "data": {"size": size, "kind": kind, "ids": None, "sources": [],
                                          "order": 0, "partitions": [], "namespace_id": name}}

    def array(name, dtype, shape, unit, values):
        return {"type": "Array", "dtype": dtype, "shape": shape, "unit": unit,
                "index_id": name, "values": values}

    error_record = {"type": "ErrorRecord", "data": {
        "requested": [], "achieved": [], "stochastic_noise": None, "finite_model_bias": [],
        "unknown_model_terms": [],
        "linearization_selection": {"estimator": "fixture-U01", "kind": "fixed_geometry",
                                    "fixed_objects": [], "assumptions": ["independent standard normal latent"]}}}

    f_offsets = [0, 4, 13, 26]
    f_indices = [0, 2, 3, 4, 0, 1, 5, 6, 7, 8, 9, 10, 11,
                 1, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23]
    f_values = [1] * 26
    f_values[13] = 2
    r_offsets = [0, 2, 4]
    r_indices = [0, 1, 1, 2]
    r_values = [0.5, 0.5, 0.25, 0.75]

    objects = {
        "input": index("fixture:native", 3, "native"),
        "latent": index("fixture:latent24", 24, "latent"),
        "output": index("fixture:output2", 2, "parameter"),
        "numeric_error": error_record,
        "F_offsets": array("F_csrrow", "u64", [4], unit_dim, f_offsets),
        "F_indices": array("F_csrnnz", "u64", [26], unit_dim, f_indices),
        "F_values": array("F_csrnnz", "f64", [26], unit_signal, f_values),
        "R_offsets": array("R_csrrow", "u64", [3], unit_dim, r_offsets),
        "R_indices": array("R_csrnnz", "u64", [4], unit_dim, r_indices),
        "R_values": array("R_csrnnz", "f64", [4], unit_brightness, r_values),
        "F_csrrow": index("fixture:F_csrrow", 4, "parameter"),
        "F_csrnnz": index("fixture:F_csrnnz", 26, "parameter"),
        "R_csrrow": index("fixture:R_csrrow", 3, "parameter"),
        "R_csrnnz": index("fixture:R_csrnnz", 4, "parameter"),
    }

    objects["F"] = {
        "type": "LinearAction",
        "recipe": {"kind": "csr", "children": [], "arrays": ["F_offsets", "F_indices", "F_values"],
                   "algorithm": "csr_v1", "version": 1,
                   "spaces": {"domain": {"$ref": "latent"}, "codomain": {"$ref": "input"},
                              "input": unit_dim, "output": unit_signal},
                   "error_record": {"$ref": "numeric_error"}, "solve": None},
        "children": [],
        "arrays": [{"$ref": "F_offsets"}, {"$ref": "F_indices"}, {"$ref": "F_values"}],
    }
    objects["R"] = {
        "type": "LinearAction",
        "recipe": {"kind": "csr", "children": [], "arrays": ["R_offsets", "R_indices", "R_values"],
                   "algorithm": "csr_v1", "version": 1,
                   "spaces": {"domain": {"$ref": "input"}, "codomain": {"$ref": "output"},
                              "input": unit_signal, "output": unit_brightness},
                   "error_record": {"$ref": "numeric_error"}, "solve": None},
        "children": [],
        "arrays": [{"$ref": "R_offsets"}, {"$ref": "R_indices"}, {"$ref": "R_values"}],
    }
    objects["I"] = {
        "type": "LinearAction",
        "recipe": {"kind": "identity", "children": [], "arrays": [], "algorithm": "identity_v1",
                   "version": 1,
                   "spaces": {"domain": {"$ref": "output"}, "codomain": {"$ref": "output"},
                              "input": unit_brightness, "output": unit_brightness},
                   "error_record": {"$ref": "numeric_error"}, "solve": None},
        "children": [],
        "arrays": [],
    }
    objects["root"] = {
        "type": "NoiseRoot",
        "data": {"shared_id": {"external_namespace": "fixture", "entity": "independent-root",
                               "model_revision": "1", "component": "all"},
                 "coordinates": {"$ref": "input"}, "unit": unit_signal, "factor": {"$ref": "F"}},
    }
    objects["noise"] = {
        "type": "NoiseModel",
        "data": {"variance_unit": unit_variance_brightness,
                 "condition": {"estimator": "fixture-U01", "kind": "fixed_geometry",
                               "fixed_objects": [], "assumptions": ["independent standard normal latent"]},
                 "innovations": [{"$ref": "root"}], "shared_factors": [],
                 "influence": [{"$ref": "R"}], "unknown_terms": [], "state": "known"},
    }
    objects["C"] = {
        "type": "CovarianceAction",
        "data": {"model": {"$ref": "noise"}, "influence": {"$ref": "I"},
                 "input_output": {"domain": {"$ref": "output"}, "codomain": {"$ref": "output"},
                                  "input": unit_dim, "output": unit_variance_brightness},
                 "absolute_factor_bound": None, "operator_norm_upper": None,
                 "omitted_action_norm": None},
    }
    objects["t"] = array("output", "f64", [2], unit_dim, [1, -0.5])

    return {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-u01-leaf-aperture-variance",
        "operation": "quadratic_form",
        "input_objects": objects,
        "arguments": {"covariance": {"$ref": "C"}, "functional": {"$ref": "t"}},
        "request": {},
        "resources": {"bytes": 8388608, "work": 100000, "spill_bytes": 0, "threads": 1,
                      "max_depth": 32, "max_precision_bits": 128, "scratch_directory": ".work"},
        "output_root": ".work/u01",
    }


if __name__ == "__main__":
    raise SystemExit(main())
