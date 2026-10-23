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
import subprocess
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

    # ---- the observed side: the run matrix over the command line -----------
    import run_matrix

    matrix_path = case.root / "observed" / "run_matrix.json"
    if not matrix_path.is_file():
        subprocess.run([sys.executable, str(case.root / "run_matrix.py")],
                       check=False, capture_output=True, timeout=3600)
    rows = []
    failures = []
    status = "pass"
    if not matrix_path.is_file():
        rows.append({"name": "run_matrix_available", "kind": "value",
                     "status": "pending_implementation", "observed": None,
                     "expected_success_count": 1, "observed_success_count": 0,
                     "reason": "the run matrix did not produce a report"})
        status = "pending_implementation"
    else:
        matrix = json.loads(matrix_path.read_text(encoding="utf-8"))

        # 1) the payload must not depend on the resource limits
        comparisons = matrix.get("comparisons", [])
        identical = all(entry.get("identical_payload") for entry in comparisons)
        rows.append({"name": "content_hash_identical_across_the_matrix", "kind": "value",
                     "status": "pass" if comparisons and identical else "fail",
                     "truth": "identical", "observed": [entry.get("signal_sha256")
                                                        for entry in comparisons],
                     "tolerance": 0.0, "unit": "hash",
                     "expected_success_count": len(comparisons),
                     "observed_success_count": sum(1 for entry in comparisons
                                                  if entry.get("identical_payload"))})
        if not comparisons or not identical:
            failures.append({"functional": "content_hash_identical_across_the_matrix",
                             "scope": "run_matrix",
                             "expected": "identical payload content",
                             "actual": [entry.get("signal_sha256") for entry in comparisons],
                             "reason": "the payload depends on the resource limits"})
            status = "fail"

        # 2) the work budget must be named, not silently change the result
        work_runs = [run for run in matrix.get("runs", []) if run["id"] == "work_1"]
        if work_runs:
            run = work_runs[0]
            finished = run.get("run_finished") or {}
            others = [entry for entry in matrix.get("runs", []) if entry["id"] != "work_1"]
            reference = others[0]["keys"]["signal_sha256"] if others else None
            named = finished.get("code") not in (None, "ok")
            # The red line is that a starved budget never publishes a
            # *different* payload.  Publishing nothing satisfies it, and it is
            # the better outcome: the run reports the exhaustion and leaves no
            # product behind.  Publishing the same payload is also admissible.
            # Only a published payload that differs from the reference fails.
            signal = run["keys"]["signal_sha256"]
            same_payload = signal is None or signal == reference
            rows.append({"name": "work_budget_is_named", "kind": "value",
                         "status": "pass" if (named and same_payload) else "fail",
                         "truth": "the budget exhaustion is named and no different payload is published",
                         "observed": {"code": finished.get("code"),
                                      "signal_sha256": run["keys"]["signal_sha256"],
                                      "reference_sha256": reference},
                         "expected_success_count": 1, "observed_success_count": 1})
            if not (named and same_payload):
                failures.append({"functional": "work_budget_is_named", "scope": "run_matrix",
                                 "expected": "a named work exhaustion and an unchanged payload",
                                 "actual": {"code": finished.get("code"),
                                            "signal_sha256": run["keys"]["signal_sha256"],
                                            "reference_sha256": reference},
                                 "reason": "the work budget of one unit was exceeded and the run "
                                           "either reported success or published a different "
                                           "payload"})
                status = "fail"

        # 3) a write that cannot be completed must be named and must not publish
        write_failure = matrix.get("write_failure", {})
        published = write_failure.get("published", 0)
        code = (write_failure.get("run_finished") or {}).get("code")
        rows.append({"name": "write_failure_is_named", "kind": "value",
                     "status": "pass" if code not in (None, "ok") and published == 0 else "fail",
                     "truth": "a named failure without a publication",
                     "observed": {"code": code, "published": published,
                                  "exit_code": write_failure.get("exit_code")},
                     "expected_success_count": 1, "observed_success_count": 1})
        if code in (None, "ok") or published != 0:
            failures.append({"functional": "write_failure_is_named", "scope": "run_matrix",
                             "expected": "a named failure without a publication",
                             "actual": write_failure, "reason": "the write failure was not named"})
            status = "fail"

        # 4) a cancelled run must be named and must not publish
        cancellation = matrix.get("cancellation", {})
        finished = cancellation.get("run_finished") or {}
        frames = ((cancellation.get("summary") or {}).get("frames") or [])
        reason = frames[0].get("reason") if frames else None
        cancelled = (cancellation.get("summary") or {}).get("cancelled", 0)
        published = (cancellation.get("summary") or {}).get("published", 0)
        named = "cancel" in str(reason).lower() or "cancel" in str(
            (cancellation.get("summary") or {}).get("frames")).lower()
        rows.append({"name": "cancellation_is_named", "kind": "value",
                     "status": "pass" if named and published == 0 else "fail",
                     "truth": "a named cancellation without a publication",
                     "observed": {"reason": reason, "cancelled": cancelled,
                                  "published": published,
                                  "exit_code": cancellation.get("exit_code")},
                     "expected_success_count": 1, "observed_success_count": 1})
        if not named or published != 0:
            failures.append({"functional": "cancellation_is_named", "scope": "run_matrix",
                             "expected": "a named cancellation without a publication",
                             "actual": cancellation, "reason": "the cancellation was not named"})
            status = "fail"
        rows.append({"name": "cancelled_counter", "kind": "value",
                     "status": "pass" if cancelled >= 1 else "fail",
                     "truth": "the summary counts the cancelled run",
                     "observed": cancelled, "expected_success_count": 1,
                     "observed_success_count": 1,
                     "reason": "the frame is counted as failed although the reason names the "
                               "cancellation"})
        if cancelled < 1:
            failures.append({"functional": "cancelled_counter", "scope": "run_matrix",
                             "expected": 1, "actual": cancelled,
                             "reason": "the summary does not count the cancelled run"})
            status = "fail"

        # 5) the resource ledger: the work of the geometry path is metered
        ledger_path = case.root.parent / "g01" / "observed" / "native_g01.json"
        if ledger_path.is_file():
            ledger = json.loads(ledger_path.read_text(encoding="utf-8"))

            def collect(node: object) -> list:
                found: list = []
                if isinstance(node, dict):
                    for key, value in node.items():
                        if key == "work_units" and isinstance(value, (int, float)):
                            found.append(float(value))
                        else:
                            found.extend(collect(value))
                elif isinstance(node, list):
                    for value in node:
                        found.extend(collect(value))
                return found

            work_units = max(collect(ledger), default=0.0)
            rows.append({"name": "resource_ledger_work_metered", "kind": "value",
                         "status": "pass" if work_units > 0 else "fail",
                         "truth": "> 0", "observed": work_units, "tolerance": 0.0,
                         "unit": "work units", "expected_success_count": 1,
                         "observed_success_count": 1,
                         "note": "the geometry path of the native route; the command line "
                                 "event protocol does not publish the ledger"})
            if work_units <= 0:
                failures.append({"functional": "resource_ledger_work_metered",
                                 "scope": "resource_ledger",
                                 "expected": "> 0", "actual": work_units,
                                 "reason": "the work of the geometry path is not metered"})
                status = "fail"

        # 6) the cache switch is not reachable from the configuration
        rows.append({"name": "cache_switch_reachable", "kind": "value",
                     "status": "pending_implementation", "observed": None,
                     "expected_success_count": 1, "observed_success_count": 0,
                     "reason": matrix.get("cache_option")})
        if status == "pass":
            status = "pending_implementation"

    comparison = comparison_payload(
        case.name.upper(), status, rows, failures,
        "R01: the run matrix over the resource limits and the two named failure paths "
        "(a write that cannot be completed and a cancelled run)")

    case.write_comparison(comparison)
    print(json.dumps({"case": "R01", "runs": [entry["name"] for entry in RUN_MATRIX]}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
