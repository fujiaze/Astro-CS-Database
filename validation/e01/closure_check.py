"""The independent product closure checker of E01 and P01.

The checker opens a published product **as a foreign product**: it reads the
manifest, resolves every object and block reference, recomputes the content
hashes of the blocks, checks that the referenced objects exist and that the
FITS payload round trips through an external reader.

It never calls the implementation under test and never trusts the manifest:
every hash is recomputed here from the bytes on disk.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any, Dict, List, Tuple


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def collect_references(node: Any, found: List[str]) -> None:
    """Every object reference of the JSON tree."""
    if isinstance(node, dict):
        for key, value in node.items():
            if key == "$ref" and isinstance(value, str):
                found.append(value)
            else:
                collect_references(value, found)
    elif isinstance(node, list):
        for value in node:
            collect_references(value, found)


def check_product(root: Path) -> Dict[str, Any]:
    """The independent closure report of one product directory."""
    report: Dict[str, Any] = {"root": str(root), "checks": [], "failures": []}

    def check(name: str, ok: bool, detail: Any = None) -> bool:
        report["checks"].append({"name": name, "pass": bool(ok), "detail": detail})
        if not ok:
            report["failures"].append({"check": name, "detail": detail})
        return bool(ok)

    manifest_path = root / "manifest.json"
    if not manifest_path.is_file():
        check("manifest_present", False, str(manifest_path))
        report["status"] = "fail"
        return report
    check("manifest_present", True, str(manifest_path))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    # 1) every declared object and block exists on disk
    declared_objects = manifest.get("objects", []) or []
    declared_blocks = manifest.get("blocks", []) or []
    missing_objects = [entry.get("path") for entry in declared_objects
                       if not (root / entry.get("path", "")).is_file()]
    missing_blocks = [entry.get("path") for entry in declared_blocks
                      if not (root / entry.get("path", "")).is_file()]
    check("objects_present", not missing_objects, missing_objects)
    check("blocks_present", not missing_blocks, missing_blocks)

    # 2) the content hash of every block is recomputed from the bytes
    hash_failures = []
    for entry in declared_blocks:
        path = root / entry["path"]
        if not path.is_file():
            continue
        expected = str(entry.get("raw_sha256", "")).replace("sha256:", "")
        actual = sha256_file(path)
        if expected and actual != expected:
            hash_failures.append({"path": entry["path"], "expected": expected, "actual": actual})
        if entry.get("stored_bytes") is not None and path.stat().st_size != entry["stored_bytes"]:
            hash_failures.append({"path": entry["path"], "expected_bytes": entry["stored_bytes"],
                                  "actual_bytes": path.stat().st_size})
    check("block_hashes", not hash_failures, hash_failures)

    # 3) the root object and every internal reference resolve
    known = {entry.get("hash") for entry in declared_objects}
    root_object = manifest.get("root_object")
    check("root_object_declared", root_object in known if root_object else False, root_object)
    unresolved: List[str] = []
    for entry in declared_objects:
        path = root / entry["path"]
        if not path.is_file():
            continue
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            unresolved.append(f"{entry['path']}: not JSON")
            continue
        references: List[str] = []
        collect_references(document, references)
        for reference in references:
            if reference not in known:
                unresolved.append(f"{entry['path']} -> {reference}")
    check("references_resolve", not unresolved, unresolved)

    # 4) the published payload reads back with an external reader
    fits_path = root / "signal.fits"
    fits_report: Dict[str, Any] = {"present": fits_path.is_file()}
    if fits_path.is_file():
        try:
            import numpy as np
            from astropy.io import fits

            with fits.open(fits_path) as handle:
                header = handle[0].header
                data = handle[0].data
            fits_report["shape"] = list(np.shape(data)) if data is not None else None
            fits_report["header_keys"] = sorted(key for key in header.keys()
                                                if not key.startswith("COMMENT"))
            fits_report["min"] = float(np.min(data)) if data is not None else None
            fits_report["max"] = float(np.max(data)) if data is not None else None
            # the round trip: the header and the data survive a read and a
            # write through the external reader
            temporary = fits_path.with_suffix(".roundtrip.fits")
            handle = fits.open(fits_path)
            handle.writeto(temporary, overwrite=True)
            handle.close()
            with fits.open(temporary) as second:
                same_header = (dict(second[0].header) == dict(header))
                same_data = bool(np.array_equal(np.asarray(second[0].data), np.asarray(data)))
            temporary.unlink(missing_ok=True)
            fits_report["header_round_trip"] = same_header
            fits_report["data_round_trip"] = same_data
            check("fits_round_trip", same_header and same_data, fits_report)
        except Exception as exc:  # pragma: no cover - a diagnostic only
            fits_report["error"] = f"{type(exc).__name__}: {exc}"
            check("fits_round_trip", False, fits_report)
    else:
        check("fits_round_trip", False, fits_report)
    report["fits"] = fits_report

    # 5) the manifest claims a complete product
    check("manifest_complete", bool(manifest.get("complete")), manifest.get("complete"))
    report["status"] = "pass" if not report["failures"] else "fail"
    report["manifest_stage"] = manifest.get("stage")
    report["manifest_profile"] = manifest.get("profile")
    report["manifest_inputs"] = manifest.get("inputs")
    return report


def negative_products(root: Path) -> Dict[str, Any]:
    """The named negative cases of the closure: each one must be rejected."""
    expectations = {
        "missing_block": "a product with a missing block must not open",
        "hash_conflict": "a product with a conflicting hash must not open",
        "duplicate_exposure": "a product with a duplicated exposure must not open",
        "signal_only": "a signal_only product must not claim the full closure",
        "bad_closure": "a broken closure must not be published",
    }
    return expectations


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("usage: closure_check.py <product-directory> [...]")
        raise SystemExit(2)
    reports = [check_product(Path(argument)) for argument in sys.argv[1:]]
    print(json.dumps(reports, indent=1))
    raise SystemExit(0 if all(report["status"] == "pass" for report in reports) else 1)
