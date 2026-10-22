#!/usr/bin/env python3
"""P01 truth: the product closure rules.

Independence
------------
The truth is the closure contract itself: a product is consumable only if its
manifest, object graph and block hashes verify, and the named corruptions must
be refused with the declared codes.  The checker in this file is an
independent implementation of the closure rules; it never calls the
production product reader.

Truth type: conditional_model (the verdict is conditional on the published
product being present) with deterministic negative cases.
"""

from __future__ import annotations

import hashlib
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
    sha256_file,
    sha256_text,
    source_identity,
    write_json,
)

CASE = "p01"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def check_closure(root: Path):
    """An independent closure check of a published product directory."""
    manifest_path = root / "manifest.json"
    if not manifest_path.is_file():
        return {"status": "missing", "reason": "no manifest.json"}
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    problems = []
    if manifest.get("format") != "acsd.product":
        problems.append("unexpected product format")
    for entry in manifest.get("objects", []):
        path = root / entry["path"]
        if not path.is_file():
            problems.append(f"missing object {entry['path']}")
            continue
        digest = sha256_file(path)
        if digest != entry["hash"].split(":")[-1]:
            problems.append(f"object hash mismatch {entry['path']}")
    for block in manifest.get("blocks", []):
        path = root / block["path"]
        if not path.is_file():
            problems.append(f"missing block {block['path']}")
            continue
        if block.get("codec") == "raw":
            raw = path.read_bytes()
            if len(raw) != block["raw_bytes"]:
                problems.append(f"block byte count mismatch {block['path']}")
            if "sha256:" + sha256_bytes(raw) != block["raw_sha256"]:
                problems.append(f"block hash mismatch {block['path']}")
    if not manifest.get("complete", False):
        problems.append("the product is not marked complete")
    return {
        "status": "ok" if not problems else "corrupt",
        "problems": problems,
        "manifest": manifest,
    }


def main() -> int:
    case = Case(
        name=CASE,
        goal=("Verify the product closure: a full profile product must be re-openable without the "
              "original FITS, a signal-only product must report an unknown error and equal "
              "weights, and a corrupt closure (missing block, hash conflict, duplicate exposure, "
              "unknown recipe) must not be published."),
        assumptions=[
            "the product format is acsd.product major 2",
            "the manifest is published last and is the only completion identity",
            "the full profile is self contained and the signal_only profile declares the missing "
            "capabilities",
        ],
        estimand=("the product closure verdict, the declared capability set and the science "
                  "content hash"),
        unit="verdict and capability set; the content hash is a sha256 identity",
        condition="the published product directory",
        tolerance={"content_hash": "identical across the resource variants",
                   "closure": "no missing or mismatching object and no missing block"},
        positive_expectations=[
            {"id": "full_reopen", "expectation": "the full product can be opened and refined "
             "without the original FITS"},
            {"id": "signal_only_equal", "expectation": "the signal_only product uses equal "
             "weights and reports an unknown error"},
            {"id": "mismatch_detected", "expectation": "a hash conflict is detected"},
        ],
        negative_expectations=[
            {"id": "missing_block", "expectation": "a missing block must give "
             "missing_dependency and must not publish"},
            {"id": "hash_conflict", "expectation": "a hash conflict must give hash_mismatch"},
            {"id": "duplicate_exposure", "expectation": "a repeated exposure must give "
             "duplicate_source and must not be read twice"},
            {"id": "unknown_recipe", "expectation": "an unknown action recipe must give "
             "unsupported while the signal stays readable"},
            {"id": "no_partial_publication", "expectation": "a cancelled or failed run must not "
             "publish a completion manifest"},
        ],
        truth_type="conditional_model",
    )

    expectations = {
        "closure_checks": {
            "missing_manifest": {"expected_code": "missing_dependency"},
            "missing_object": {"expected_code": "missing_dependency"},
            "missing_block": {"expected_code": "missing_dependency"},
            "block_hash_mismatch": {"expected_code": "hash_mismatch"},
            "object_hash_mismatch": {"expected_code": "hash_mismatch"},
            "unknown_recipe": {"expected_code": "unsupported"},
            "duplicate_exposure": {"expected_code": "duplicate_source"},
            "incomplete_manifest": {"expected_code": "corrupt_product"},
        },
        "profiles": {
            "full": {"capabilities": ["mosaic", "export", "refine", "noise", "response"],
                     "external_inputs": False},
            "signal_only": {"capabilities": ["mosaic", "export"],
                            "external_inputs": False,
                            "noise": "unknown", "weights": "equal"},
        },
        "shared_root": {
            "note": "the same shared root and the same sky must be used across tiles; a new R "
                    "must not be mixed with an old sky",
        },
    }

    truth = {
        "case": "P01",
        "truth_type": "conditional_model",
        "methods": {
            "closure": "an independent manifest / object / block verification",
            "corruptions": "named deterministic negative configurations",
        },
        "expectations": expectations,
        "note": ("the verdict is conditional on a published product; the case is pending until "
                 "the product writer is available"),
    }
    truth_hash = case.write_truth("p01_truth.json", truth)

    fixture = {
        "case": "P01",
        "source": "the U01 fixture extended to a cross tile shared root and sky",
        "tiles": ["tile-a", "tile-b"],
        "exposures": ["exp-1", "exp-2"],
        "profiles": ["full", "signal_only"],
        "corruptions": ["missing_block", "hash_conflict", "duplicate_exposure",
                        "unknown_recipe"],
    }
    fixture_hash = case.write_input("p01_model.json", fixture)

    request = {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": "val-p01-publish-open",
        "operation": "open_product",
        "input_objects": {},
        "arguments": {},
        "request": {},
        "resources": {"bytes": 8388608, "work": 100000, "spill_bytes": 0, "threads": 1,
                      "max_depth": 32, "max_precision_bits": 128, "scratch_directory": ".work"},
        "output_root": ".work/p01",
    }
    request_hash = case.write_request(request)

    case.write_case_json({
        "input_model_sha256": fixture_hash,
        "truth_sha256": truth_hash,
        "request_sha256": sha256_text(canonical_json(request)),
        "availability": availability(["publish_product", "open_product", "mosaic",
                                       "export_image", "build_export"]),
    })

    observe.write_manifest(case, {
        "truth_type": "conditional_model",
        "seeds": [],
        "trial_denominators": {},
        "library_identity": {"python": sys.version.split()[0]},
        "code_identity": source_identity([Path(__file__)]),
        "commands": {"truth": f"python3 validation/{CASE}/truth_{CASE}.py"},
        "artefacts": {"truth": truth_hash, "input_model": fixture_hash},
        "request_hash": content_hash(request),
    })

    comparison = comparison_payload(
        case.name.upper(), "pending_implementation",
        [{"name": "closure_verdict", "kind": "value", "truth": "ok", "tolerance": 0.0,
          "unit": "verdict", "status": "pending_implementation",
          "reason": "no published product exists yet; the product writer is not available"}],
        [],
        "The independent closure checker is ready; the observed side is pending_implementation.")

    print(json.dumps({"case": "P01", "expectations": list(expectations["closure_checks"])},
                     indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
