#!/usr/bin/env python3
"""R01 truth: the resource and reproducibility ledger.

Independence
------------
The expectation of R01 is a property, not a number: the science content hash
must not depend on the thread count, the memory budget, the cache setting,
the write failure or the cancellation, and the completion order of the frame
and target blocks must not change the result.  The harness below records the
run matrix and the comparison keys; it never touches the production code.

Truth type: conditional_model (the verdict is conditional on the runs) with
an empirical resource ledger.
"""

from __future__ import annotations

import json
import sys
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
    sha256_text,
    source_identity,
    write_json,
)

CASE = "r01"

RUN_MATRIX = [
    {"name": "threads_1", "threads": 1, "memory_bytes": 256 * 1024 * 1024, "cache": True,
     "write_failure": False, "cancel": False, "completion_order": "sequential"},
    {"name": "threads_4", "threads": 4, "memory_bytes": 256 * 1024 * 1024, "cache": True,
     "write_failure": False, "cancel": False, "completion_order": "sequential"},
    {"name": "memory_32MiB", "threads": 1, "memory_bytes": 32 * 1024 * 1024, "cache": True,
     "write_failure": False, "cancel": False, "completion_order": "sequential"},
    {"name": "cache_off", "threads": 1, "memory_bytes": 256 * 1024 * 1024, "cache": False,
     "write_failure": False, "cancel": False, "completion_order": "sequential"},
    {"name": "write_failure", "threads": 1, "memory_bytes": 256 * 1024 * 1024, "cache": True,
     "write_failure": True, "cancel": False, "completion_order": "sequential"},
    {"name": "cancelled", "threads": 1, "memory_bytes": 256 * 1024 * 1024, "cache": True,
     "write_failure": False, "cancel": True, "completion_order": "sequential"},
    {"name": "out_of_order_frames", "threads": 4, "memory_bytes": 256 * 1024 * 1024,
     "cache": True, "write_failure": False, "cancel": False,
     "completion_order": "frames_reversed"},
    {"name": "out_of_order_targets", "threads": 4, "memory_bytes": 256 * 1024 * 1024,
     "cache": True, "write_failure": False, "cancel": False,
     "completion_order": "targets_reversed"},
]

COMPARISON_KEYS = [
    "science_content_hash",
    "error_record_state",
    "support_state",
    "numerical_bounds",
]


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Verify that the scientific result does not depend on the execution resources and "
              "that the resource ledger is honest: the same science input must give the same "
              "content hash for one and four threads, for a 32 MiB and a 256 MiB budget, with the "
              "cache on and off and with the frame and target blocks completing in a different "
              "order."),
        assumptions=[
            "the same build, the same arithmetic and the same configuration are used",
            "progress, timing, memory and the run identifier are not part of the science content "
            "hash",
            "a write failure and a cancellation must not publish a completion identity",
            "a new geometry revision must not be mixed with an old sky",
        ],
        estimand=("the science content hash, the error and support states, the numerical bounds "
                  "and the peak resource usage"),
        unit="hash identity; bytes and seconds for the resource ledger",
        condition="the same scientific input with different execution resources",
        tolerance={"content_hash": "identical",
                   "error_and_support": "identical states",
                   "numerical_bounds": "identical intervals",
                   "peak_payload": "must not exceed the declared budget"},
        positive_expectations=[
            {"id": "content_hash_stable", "expectation": "the content hash is identical over "
             "the whole run matrix"},
            {"id": "spill_or_fail", "expectation": "with 32 MiB the run either spills inside the "
             "declared budget or fails with resource_exhausted"},
            {"id": "no_mixed_versions", "expectation": "a new R is never combined with an old "
             "sky"},
        ],
        negative_expectations=[
            {"id": "cancel_no_publication", "expectation": "a cancelled run publishes nothing"},
            {"id": "write_failure_no_publication", "expectation": "a failed write publishes "
             "nothing"},
            {"id": "ledger_honest", "expectation": "the reported peak usage does not exceed the "
             "measured peak"},
        ],
        truth_type="conditional_model + empirical",
    )

    truth = {
        "case": "R01",
        "truth_types": ["conditional_model", "empirical"],
        "run_matrix": RUN_MATRIX,
        "comparison_keys": COMPARISON_KEYS,
        "ledger": {
            "fields": ["wall_seconds", "peak_rss_bytes", "peak_payload_bytes", "spill_bytes",
                       "work_units", "read_bytes", "written_bytes"],
            "note": "the measured peak is the reference; the reported ledger must not exceed it "
                    "by more than the declared runtime margin",
        },
        "expectations": {
            "identical_across": ["threads_1", "threads_4", "memory_32MiB", "cache_off",
                                  "out_of_order_frames", "out_of_order_targets"],
            "must_fail_or_spill": ["memory_32MiB"],
            "must_not_publish": ["write_failure", "cancelled"],
        },
    }
    truth_hash = case.write_truth("r01_truth.json", truth)

    fixture = {
        "case": "R01",
        "science_input": "the U01 fixture, run through the full pipeline",
        "run_matrix": RUN_MATRIX,
        "scratch_directory": ".work/r01",
    }
    fixture_hash = case.write_input("r01_model.json", fixture)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-r01-resource-matrix",
        "operation": "normalize",
        "input_objects": {},
        "arguments": {},
        "request": {},
        "resources": {"bytes": 268435456, "work": 10000000, "spill_bytes": 8589934592,
                      "threads": 4, "max_depth": 64, "max_precision_bits": 256,
                      "scratch_directory": ".work/r01"},
        "output_root": ".work/r01",
    }
    request_hash = case.write_request(request)

    case.write_case_json({
        "input_model_sha256": fixture_hash,
        "truth_sha256": truth_hash,
        "request_sha256": sha256_text(canonical_json(request)),
        "availability": availability(["normalize", "mosaic", "export_image"]),
    })

    observe.write_manifest(case, {
        "truth_type": "conditional_model + empirical",
        "seeds": [],
        "trial_denominators": {"runs": len(RUN_MATRIX)},
        "library_identity": {"python": sys.version.split()[0]},
        "code_identity": source_identity([Path(__file__)]),
        "commands": {"truth": f"python3 validation/{CASE}/truth_{CASE}.py"},
        "artefacts": {"truth": truth_hash, "input_model": fixture_hash},
        "request_hash": content_hash(request),
    })

    comparison = comparison_payload(
        case.name.upper(), "pending_implementation",
        [{"name": "content_hash_identical_across_the_matrix", "kind": "value",
          "truth": "identical", "tolerance": 0.0, "unit": "hash",
          "status": "pending_implementation",
          "reason": "the three command entry points are not available yet"}],
        [],
        "The run matrix and the comparison keys are frozen; the observed side is "
        "pending_implementation.")

    print(json.dumps({"case": "R01", "runs": [entry["name"] for entry in RUN_MATRIX]}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
