#!/usr/bin/env python3
"""Runs the contract driver chain of the geometry operations.

The chain is: make_wcs -> make_footprint_provider -> compute_overlap, where
every step takes the object of the previous step from the object table of the
response.  The program checks that the objects are published and that the
overlap of a detector footprint against the leaves of an order is resolved.
"""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "build/science/acsd-contract"
RESOURCES = {
    "bytes": 8388608,
    "work": 100000,
    "spill_bytes": 0,
    "threads": 1,
    "max_depth": 32,
    "max_precision_bits": 128,
    "scratch_directory": ".work",
}


def run(envelope: dict, workdir: Path, name: str = "") -> tuple[int, dict]:
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
    if name:
        # The evidence of the step: the request, the response and the exit code.
        evidence = ROOT / "validation/apps/evidence/contract"
        evidence.mkdir(parents=True, exist_ok=True)
        (evidence / f"{name}.request.json").write_text(
            json.dumps(envelope, indent=1) + "\n", encoding="utf-8")
        (evidence / f"{name}.response.json").write_text(
            json.dumps(body, indent=1) + "\n", encoding="utf-8")
        (evidence / f"{name}.exit.txt").write_text(f"{process.returncode}\n", encoding="utf-8")
    return process.returncode, body


def main() -> int:
    failures = []
    with tempfile.TemporaryDirectory() as temporary:
        workdir = Path(temporary)

        # 1) make_wcs
        code, body = run(
            {
                "protocol": "acsd.contract",
                "major": 1,
                "request_id": "chain-1-make-wcs",
                "operation": "make_wcs",
                "arguments": {
                    "header": [
                        {"key": "CTYPE1", "value": "RA---TAN"},
                        {"key": "CTYPE2", "value": "DEC--TAN"},
                        {"key": "CRPIX1", "value": 4.5},
                        {"key": "CRPIX2", "value": 4.5},
                        {"key": "CRVAL1", "value": 120.0},
                        {"key": "CRVAL2", "value": 30.0},
                        {"key": "CD1_1", "value": -0.001},
                        {"key": "CD2_2", "value": 0.001},
                        {"key": "RADESYS", "value": "ICRS"},
                    ],
                    "domain": {"lower": [-0.5, -0.5], "upper": [7.5, 7.5]},
                    "detector_frame": "detector",
                },
                "request": {},
                "input_objects": {},
                "output_root": str(workdir),
                "resources": RESOURCES,
            },
            workdir,
            "make_wcs",
        )
        print(f"[make_wcs] exit={code} code={body.get('result', {}).get('code')}")
        if code != 0 or body["result"]["code"] != "ok":
            failures.append("make_wcs did not succeed")
        objects = body.get("objects", {})
        print(f"[make_wcs] the published objects: {list(objects)}")
        wcs_payload = None
        for payload in objects.values():
            data = payload.get("data", payload)
            if "crpix_fits1" in data:
                wcs_payload = data
        if wcs_payload is None:
            failures.append("make_wcs did not publish the world coordinate model")
            print(json.dumps(body, indent=1)[:800])
            return report(failures)
        wcs_value = body["result"]["value"]
        wcs_hash = wcs_value["$ref"] if isinstance(wcs_value, dict) else wcs_value
        # The objects are referenced by their identity: the input object of the
        # next request carries the identity as its name.
        print(f"[make_wcs] the identity of the model: {wcs_hash}")

        # 2) make_footprint_provider, taking the model of the response.
        code, body = run(
            {
                "protocol": "acsd.contract",
                "major": 1,
                "request_id": "chain-2-provider",
                "operation": "make_footprint_provider",
                "arguments": {"wcs": {"$ref": wcs_hash}},
                "request": {},
                "input_objects": {wcs_hash: {"type": "WcsModel", "data": wcs_payload}},
                "output_root": str(workdir),
                "resources": RESOURCES,
            },
            workdir,
            "make_footprint_provider",
        )
        print(f"[make_footprint_provider] exit={code} code={body.get('result', {}).get('code')}")
        if code != 0 or body["result"]["code"] != "ok":
            failures.append("make_footprint_provider did not succeed")
        provider_objects = body.get("objects", {})
        provider_payload = None
        for payload in provider_objects.values():
            data = payload.get("data", payload)
            if data.get("kind") == "canonical_tan_sip":
                provider_payload = data
        if provider_payload is None:
            failures.append("make_footprint_provider did not publish the provider")
        else:
            provider_value = body["result"]["value"]
            provider_hash = (provider_value["$ref"] if isinstance(provider_value, dict)
                             else provider_value)
            print(f"[make_footprint_provider] the provider is published: {provider_payload}")

        # 3) compute_overlap of a detector footprint against the leaves.
        code, body = run(
            {
                "protocol": "acsd.contract",
                "major": 1,
                "request_id": "chain-3-overlap",
                "operation": "compute_overlap",
                "arguments": {
                    "provider": {"$ref": provider_hash},
                    "source": {"$ref": "source"},
                },
                "request": {
                    "order": 4,
                    "pixfrac": 1.0,
                    "area_absolute_sr": 1e-12,
                    "normalizer_min_sr": 1e-15,
                    "max_work": 2000000,
                    "max_bytes": 268435456,
                    "max_depth": 64,
                },
                "input_objects": {
                    provider_hash: {"type": "FootprintProvider", "data": provider_payload},
                    "source": {
                        "type": "DetectorFootprint",
                        "data": {
                            "wcs": {"$ref": wcs_hash},
                            "detector_rings": [
                                {
                                    "hole": False,
                                    "vertices": [
                                        {"x": -0.5, "y": -0.5},
                                        {"x": 7.5, "y": -0.5},
                                        {"x": 7.5, "y": 7.5},
                                        {"x": -0.5, "y": 7.5},
                                    ],
                                }
                            ],
                            "source": {
                                "dataset": "acsd.chain",
                                "exposure": "e0",
                                "detector": "d0",
                                "channel": "adu",
                                "sample": 0,
                            },
                            "contraction_center": {"x": 3.5, "y": 3.5},
                        },
                    },
                    wcs_hash: {"type": "WcsModel", "data": wcs_payload},
                },
                "output_root": str(workdir),
                "resources": RESOURCES,
            },
            workdir,
            "compute_overlap",
        )
        print(f"[compute_overlap] exit={code} code={body.get('result', {}).get('code')}")
        if code != 0 or body["result"]["code"] != "ok":
            failures.append("compute_overlap did not succeed")
            print(json.dumps(body, indent=1)[:1200])
        else:
            overlaps = None
            for payload in body.get("objects", {}).values():
                data = payload.get("data", payload)
                if "contributions" in data:
                    overlaps = data
            if overlaps is None:
                failures.append("compute_overlap did not publish the contributions")
            else:
                contributions = overlaps["contributions"]
                print(f"[compute_overlap] the contributions: {len(contributions)}")
                for contribution in contributions[:4]:
                    print(f"   leaf {contribution['leaf']} overlap "
                          f"{contribution['overlap_area_sr']}")
                if not contributions:
                    failures.append("the overlap has no contribution")

    return report(failures)


def report(failures: list[str]) -> int:
    if failures:
        for failure in failures:
            print(f"[FAIL] {failure}")
        return 1
    print("[PASS] the contract chain make_wcs -> make_footprint_provider -> compute_overlap works")
    return 0


if __name__ == "__main__":
    sys.exit(main())
