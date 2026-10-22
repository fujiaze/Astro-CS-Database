#!/usr/bin/env python3
"""Runs the contract chain of the product family.

The chain is open_product -> build_export -> export_signal on the product that
the round trip publishes.  Every step takes the objects of the previous step
from the object table of the response.  The request, the response and the exit
code of every step are written to validation/apps/evidence/contract/.
"""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "build/science/acsd-contract"
EVIDENCE = ROOT / "validation/apps/evidence/contract"
RESOURCES = {
    "bytes": 8388608,
    "work": 100000,
    "spill_bytes": 0,
    "threads": 1,
    "max_depth": 32,
    "max_precision_bits": 128,
    "scratch_directory": ".work",
}
WAVELENGTHS = [400.0 + 5.0 * i for i in range(61)]
THROUGHPUT = [0.5 + 0.5 * (w - 400.0) / 300.0 for w in WAVELENGTHS]


def run(envelope: dict, workdir: Path, name: str) -> tuple[int, dict]:
    request = workdir / "request.json"
    response = workdir / "response.json"
    request.write_text(json.dumps(envelope), encoding="utf-8")
    process = subprocess.run(
        [str(DRIVER), "--request", str(request), "--response", str(response)],
        capture_output=True,
        text=True,
    )
    if not response.is_file():
        return process.returncode, {"error": process.stderr.strip()}
    body = json.loads(response.read_text(encoding="utf-8"))
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / f"{name}.request.json").write_text(json.dumps(envelope, indent=1) + "\n",
                                                   encoding="utf-8")
    (EVIDENCE / f"{name}.response.json").write_text(json.dumps(body, indent=1) + "\n",
                                                     encoding="utf-8")
    (EVIDENCE / f"{name}.exit.txt").write_text(f"{process.returncode}\n", encoding="utf-8")
    return process.returncode, body


def target_wcs() -> dict:
    """The world coordinate model of the export target.

    The target is centred on the leaf that the round trip fixture publishes
    (the order 6 leaf 100), with one arcminute per pixel.
    """
    import math

    ra = math.radians(49.21875)
    dec = math.radians(8.989299345163)
    scale = math.radians(0.01)
    return {
        "crpix_fits1": [8.5, 8.5],
        "cd_rad_per_pixel": [-scale, 0.0, 0.0, scale],
        "c": [math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec)],
        "e": [-math.sin(ra), math.cos(ra), 0.0],
        "north": [-math.sin(dec) * math.cos(ra), -math.sin(dec) * math.sin(ra), math.cos(dec)],
        "domain": {"lower": [-0.5, -0.5], "upper": [15.5, 15.5]},
        "detector_frame": "acsd.export.target",
    }


def main() -> int:
    failures = []
    product = ROOT / ".work/product-roundtrip/frame"
    if not (product / "manifest.json").is_file():
        print("[FAIL] the round trip product is missing: run validation/apps/run_product_roundtrip.sh")
        return 1
    import hashlib

    manifest_hash = "sha256:" + hashlib.sha256((product / "manifest.json").read_bytes()).hexdigest()
    location = {"directory": str(product), "manifest_hash": manifest_hash}

    with tempfile.TemporaryDirectory() as temporary:
        workdir = Path(temporary)

        # 1) open_product
        code, body = run(
            {
                "protocol": "acsd.contract",
                "major": 1,
                "request_id": "product-1-open",
                "operation": "open_product",
                "arguments": {"location": location},
                "request": {},
                "input_objects": {},
                "output_root": str(workdir),
                "resources": RESOURCES,
            },
            workdir,
            "open_product",
        )
        print(f"[open_product] exit={code} code={body.get('result', {}).get('code')}")
        if code != 0 or body["result"]["code"] != "ok":
            failures.append("open_product did not succeed")
            print(json.dumps(body, indent=1)[:800])
        objects = body.get("objects", {})
        print(f"[open_product] the published objects: {list(objects)}")
        estimate_payload = None
        support_payload = None
        for payload in objects.values():
            data = payload.get("data", payload)
            if "estimator" in data and "signal" in data:
                estimate_payload = data
            if "kind" in data and "area_lower_sr" in data:
                support_payload = data
        if estimate_payload is None:
            failures.append("open_product did not publish the signal estimate")
        if support_payload is None:
            failures.append("open_product did not publish the support map")

        # 2) build_export
        code, body = run(
            {
                "protocol": "acsd.contract",
                "major": 1,
                "request_id": "product-2-build",
                "operation": "build_export",
                "arguments": {"product": location, "request": {"$ref": "export_request"}},
                "request": {},
                "input_objects": {
                    "export_request": {
                        "type": "ExportRequest",
                        "data": {
                            "target": {
                                "model": {"$ref": "wcs"},
                                "width": 16,
                                "height": 16,
                            },
                            "quantity": "supported_mean",
                            "support_policy": "full_leaf_approx",
                            "assume_uniform_uncovered": False,
                            "output_dtype": "f64",
                            "budget": {
                                "area_absolute_sr": 1e-12,
                                "normalizer_min_sr": 1e-15,
                                "max_work": 2000000,
                                "max_bytes": 268435456,
                                "max_depth": 64,
                            },
                        },
                    },
                    "wcs": {"type": "WcsModel", "data": target_wcs()},
                },
                "output_root": str(workdir),
                "resources": RESOURCES,
            },
            workdir,
            "build_export",
        )
        print(f"[build_export] exit={code} code={body.get('result', {}).get('code')}")
        if code != 0 or body["result"]["code"] != "ok":
            failures.append("build_export did not succeed")
            print(json.dumps(body, indent=1)[:1200])
        else:
            print(f"[build_export] the result: {json.dumps(body['result']['value'])[:200]}")

    if failures:
        for failure in failures:
            print(f"[FAIL] {failure}")
        return 1
    print("[PASS] the contract chain open_product -> build_export works")
    return 0


if __name__ == "__main__":
    sys.exit(main())
