"""The named negative cases of P01 over a published product.

Every case takes a **copy** of the published product, tampers with it in the
way the specification names, and runs the independent closure checker over the
copy.  The expectation is that the checker rejects the tampered product; a
tampered product that still passes is a failure of the product format.

The checker never calls the implementation under test; it reads the product as
a foreign product, exactly as an independent consumer would.
"""

from __future__ import annotations

import json
import shutil
import sys
import tempfile
from pathlib import Path
from typing import Any, Callable, Dict, List

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "e01"))

import closure_check  # noqa: E402


def tamper_missing_block(root: Path) -> None:
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    blocks = manifest.get("blocks", []) or []
    if blocks:
        target = root / blocks[0]["path"]
        if target.exists():
            target.unlink()


def tamper_block_hash(root: Path) -> None:
    """The bytes of a block no longer match the hash of the manifest."""
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    blocks = manifest.get("blocks", []) or []
    if not blocks:
        return
    target = root / blocks[0]["path"]
    payload = bytearray(target.read_bytes())
    if payload:
        payload[0] ^= 0xFF
        target.write_bytes(bytes(payload))


def tamper_manifest_hash(root: Path) -> None:
    """The manifest declares a hash that the object does not have."""
    manifest_path = root / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    objects = manifest.get("objects", []) or []
    if not objects:
        return
    entry = dict(objects[0])
    entry["hash"] = entry["hash"].split(":")[0] + ":0" * 64
    manifest["objects"] = [entry] + list(objects[1:])
    manifest_path.write_text(json.dumps(manifest, indent=1) + "\n", encoding="utf-8")


def tamper_duplicate_exposure(root: Path) -> None:
    """The same exposure is declared twice in the inputs."""
    manifest_path = root / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    inputs = manifest.get("inputs", []) or []
    if inputs:
        manifest["inputs"] = list(inputs) + [inputs[0]]
    else:
        # the export product declares no input; the negative case declares
        # the same one twice
        manifest["inputs"] = ["acsd.export.fixture", "acsd.export.fixture"]
    manifest_path.write_text(json.dumps(manifest, indent=1) + "\n", encoding="utf-8")


def tamper_missing_object(root: Path) -> None:
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    objects = manifest.get("objects", []) or []
    if objects:
        target = root / objects[0]["path"]
        if target.exists():
            target.unlink()


def tamper_incomplete_manifest(root: Path) -> None:
    """The manifest claims to be incomplete: it must not be a published product."""
    manifest_path = root / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["complete"] = False
    manifest_path.write_text(json.dumps(manifest, indent=1) + "\n", encoding="utf-8")


def tamper_unknown_recipe(root: Path) -> None:
    """An algorithm of the manifest is unknown to the reader."""
    manifest_path = root / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    algorithms = manifest.get("algorithm_set", []) or []
    if algorithms:
        entry = dict(algorithms[0])
        entry["name"] = "acsd.unknown_recipe_v9"
        manifest["algorithm_set"] = [entry] + list(algorithms[1:])
    manifest_path.write_text(json.dumps(manifest, indent=1) + "\n", encoding="utf-8")


CASES: List[Dict[str, Any]] = [
    {"id": "missing_block", "expectation": "a missing block must not pass the closure",
     "expected_check": "blocks_present", "tamper": tamper_missing_block},
    {"id": "hash_conflict_block", "expectation": "a block whose bytes no longer match the "
     "manifest hash must not pass the closure",
     "expected_check": "block_hashes", "tamper": tamper_block_hash},
    {"id": "hash_conflict_manifest", "expectation": "a manifest hash that no object has must "
     "not pass the closure",
     "expected_check": "references_resolve", "tamper": tamper_manifest_hash},
    {"id": "duplicate_exposure", "expectation": "a repeated exposure must not pass the closure",
     "expected_check": "inputs_unique", "tamper": tamper_duplicate_exposure},
    {"id": "missing_object", "expectation": "a missing object must not pass the closure",
     "expected_check": "objects_present", "tamper": tamper_missing_object},
    {"id": "incomplete_manifest", "expectation": "a manifest that declares itself incomplete "
     "must not pass as a published product",
     "expected_check": "manifest_complete", "tamper": tamper_incomplete_manifest},
    {"id": "unknown_recipe", "expectation": "an unknown algorithm must not pass the closure",
     "expected_check": None, "tamper": tamper_unknown_recipe,
     # An unknown recipe is an implementation behaviour (the reader has to
     # report unsupported while the signal stays readable), not a property of
     # the closure; it is judged on the contract route, which is not
     # dispatched yet.
     "deferred_to": "the contract route (unsupported while the signal stays readable)"},
]


def run_case(product: Path, case: Dict[str, Any], baseline: Dict[str, Any]) -> Dict[str, Any]:
    """The tampered copy must be rejected by a check the baseline passes.

    The baseline of the product may itself fail (the exported FITS header is
    not FITS standard conforming), so a rejection only counts when it comes
    from a check that the untampered product passes; otherwise the rejection
    would not be attributable to the tampering.
    """
    baseline_failures = {failure["check"] for failure in baseline["failures"]}
    with tempfile.TemporaryDirectory(prefix="acsd-p01-") as directory:
        copy = Path(directory) / "product"
        shutil.copytree(product, copy)
        case["tamper"](copy)
        report = closure_check.check_product(copy)
        new_failures = [failure["check"] for failure in report["failures"]
                        if failure["check"] not in baseline_failures]
        expected = case.get("expected_check")
        deferred = case.get("deferred_to")
        if deferred:
            return {"id": case["id"], "expectation": case["expectation"],
                    "expected_check": expected, "detected_by": new_failures,
                    "baseline_failures": sorted(baseline_failures),
                    "status": "deferred", "deferred_to": deferred}
        detected = bool(new_failures) and (expected is None or expected in new_failures)
        return {"id": case["id"], "expectation": case["expectation"],
                "expected_check": expected,
                "detected_by": new_failures,
                "baseline_failures": sorted(baseline_failures),
                "status": "pass" if detected else "fail"}


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: negative_closure.py <product-directory>")
        return 2
    product = Path(sys.argv[1])
    baseline = closure_check.check_product(product)
    results = [run_case(product, case, baseline) for case in CASES]
    judged = [result for result in results if result["status"] != "deferred"]
    report = {"product": str(product),
              "baseline": {"status": baseline["status"],
                           "failures": [failure["check"] for failure in baseline["failures"]]},
              "cases": results,
              "status": "pass" if all(result["status"] == "pass" for result in judged) else "fail",
              "attempts": len(judged),
              "successes": sum(1 for result in judged if result["status"] == "pass"),
              "deferred": [result["id"] for result in results if result["status"] == "deferred"]}
    print(json.dumps(report, indent=1))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
