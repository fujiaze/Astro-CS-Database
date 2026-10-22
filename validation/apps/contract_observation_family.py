#!/usr/bin/env python3
"""The observation family of the contract driver: four operations, each with a
request, a response and an exit code.

The fixture is small but real: an eight by eight frame with one leaf, a world
coordinate model, a footprint set, three catalogue stars and the arrays of the
observation.  The evidence is written into
validation/apps/evidence/contract/ as the usual request, response and exit code
triples.
"""
from __future__ import annotations

import json
import math
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "build/science/acsd-contract"
EVIDENCE = ROOT / "validation/apps/evidence/contract"

RESOURCES = {"bytes": 8388608, "work": 1000000, "spill_bytes": 0, "threads": 1,
             "max_depth": 32, "max_precision_bits": 128, "scratch_directory": ".work"}
UNIT = {"scale": "L", "linear_power": 1, "sr_power": -1, "multiplier": 1.0,
        "nm_power": 0, "pixel_power": 0}


def wcs_payload() -> dict:
    ra = math.radians(49.0)
    dec = math.radians(8.0)
    scale = math.radians(0.001)
    return {"crpix_fits1": [4.5, 4.5], "cd_rad_per_pixel": [-scale, 0.0, 0.0, scale],
            "c": [math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec)],
            "e": [-math.sin(ra), math.cos(ra), 0.0],
            "north": [-math.sin(dec) * math.cos(ra), -math.sin(dec) * math.sin(ra), math.cos(dec)],
            "domain": {"lower": [-0.5, -0.5], "upper": [7.5, 7.5]},
            "detector_frame": "acsd.contract.detector"}


def objects() -> dict:
    pixels = [1.0 + 0.01 * i for i in range(64)]
    return {
        "values": {"type": "Array", "dtype": "f64", "shape": [64], "unit": UNIT,
                   "index_id": "", "values": pixels},
        "dq": {"type": "Array", "dtype": "u8", "shape": [64], "unit": UNIT,
               "index_id": "", "values": [0] * 64},
        "leaf_index": {"type": "Index", "data": {
            "size": 1, "kind": "leaf", "order": 0, "namespace_id": "acsd.contract.leaves",
            "ids": {"$ref": "leaf_ids"}, "partitions": [], "sources": []}},
        "leaf_ids": {"type": "Array", "dtype": "u64", "shape": [2], "unit": UNIT,
                     "index_id": "", "values": [0, 0]},
        "native_index": {"type": "Index", "data": {
            "size": 64, "kind": "native", "order": 0,
            "namespace_id": "acsd.contract.detector.pixels",
            "ids": {"$ref": "pixel_ids"}, "partitions": [], "sources": []}},
        "pixel_ids": {"type": "Array", "dtype": "u64", "shape": [64], "unit": UNIT,
                      "index_id": "", "values": list(range(64))},
        "wcs": {"type": "WcsModel", "data": wcs_payload()},
        "footprints": {"type": "FootprintSet", "data": {
            "width": 8, "height": 8, "detector_from_sample": [1.0, 0.0, 0.0, 1.0],
            "offset": [0.0, 0.0], "detector_frame": "acsd.contract.detector",
            "index": {"$ref": "leaf_index"}, "wcs": {"$ref": "wcs"},
            "inline_footprints": [
                {"wcs": {"$ref": "wcs"},
                 "source": {"dataset": "acsd.contract", "exposure": "e1",
                            "detector": "d1", "channel": "c1", "sample": 0},
                 "detector_rings": [{"hole": False, "vertices": [[3.0, 3.0], [5.0, 3.0],
                                                                  [5.0, 5.0], [3.0, 5.0]]}]}]}},
        "observation": {"type": "NativeObservation", "data": {
            "frame": "contract-frame", "physical_collection": True, "unit": UNIT,
            "integrated_values": {"$ref": "values"}, "dq": {"$ref": "dq"},
            "native_index": {"$ref": "native_index"}, "footprints": {"$ref": "footprints"},
            "scale_applications": []}},
        "stars": {"type": "CatalogueStars", "data": {"stars": [
            {"source_id": 1, "ra_rad": math.radians(49.0), "dec_rad": math.radians(8.0),
             "epoch_year": 2016.0, "pmra_cosdec_rad_year": 0.0, "pmdec_rad_year": 0.0,
             "parallax_rad": 0.0, "brightness_mag": 15.0},
            {"source_id": 2, "ra_rad": math.radians(49.0008), "dec_rad": math.radians(8.0004),
             "epoch_year": 2016.0, "pmra_cosdec_rad_year": 0.0, "pmdec_rad_year": 0.0,
             "parallax_rad": 0.0, "brightness_mag": 16.0},
            {"source_id": 3, "ra_rad": math.radians(48.9994), "dec_rad": math.radians(7.9996),
             "epoch_year": 2016.0, "pmra_cosdec_rad_year": 0.0, "pmdec_rad_year": 0.0,
             "parallax_rad": 0.0, "brightness_mag": 17.0}]}},
        "psf": {"type": "EffectivePSF", "data": {
            "family": "gaussian", "parameters": [1.5, 0.0],
            "spatial_domain": {"lower": [-4.0, -4.0], "upper": [4.0, 4.0]},
            "wavelength_nm": {"lower": 400.0, "upper": 700.0}, "tail_mass_upper": 1e-6,
            "achromatic": True, "spatial_basis": {"kind": "constant", "degree": 0}}},
        "prediction": {"type": "SpectralPrediction", "data": {
            "basis_integrals": {"$ref": "prediction_values"},
            "absolute_errors": {"$ref": "prediction_errors"},
            "support_nm": {"lower": 400.0, "upper": 700.0}}},
        "prediction_values": {"type": "Array", "dtype": "f64", "shape": [3], "unit": UNIT,
                              "index_id": "", "values": [1.0, 2.0, 3.0]},
        "prediction_errors": {"type": "Array", "dtype": "f64", "shape": [3], "unit": UNIT,
                               "index_id": "", "values": [0.0, 0.0, 0.0]},
        "measurements": {"type": "SourceMeasurements", "data": {
            "frame": "contract-frame", "stars": [
                {"gaia_source_id": 1, "integrated_flux": 100.0, "definition": "aperture",
                 "window": {"lower": [0.0, 0.0], "upper": [2.0, 2.0]}}]}},
        "solution": {"type": "PhotoSolution", "data": {
            "theta": {"$ref": "theta"}, "application_id": "contract-solution",
            "frames": ["contract-frame"]}},
        "theta": {"type": "Array", "dtype": "f64", "shape": [3], "unit": UNIT,
                  "index_id": "", "values": [1.0, 1.0, 1.0]},
    }


def run(operation: str, arguments: dict, request: dict) -> tuple[int, dict]:
    body = {"protocol": "acsd.contract", "major": 1, "request_id": f"contract-{operation}",
            "operation": operation, "arguments": arguments, "request": request,
            "input_objects": objects(), "output_root": tempfile.mkdtemp(),
            "resources": RESOURCES}
    with tempfile.TemporaryDirectory() as directory:
        request_path = Path(directory) / "request.json"
        response_path = Path(directory) / "response.json"
        request_path.write_text(json.dumps(body, indent=1))
        process = subprocess.run(
            [str(DRIVER), "--request", str(request_path), "--response", str(response_path)],
            capture_output=True, text=True)
        response = json.loads(response_path.read_text()) if response_path.is_file() else {}
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / f"{operation}.request.json").write_text(json.dumps(body, indent=1) + "\n")
    (EVIDENCE / f"{operation}.response.json").write_text(json.dumps(response, indent=1) + "\n")
    (EVIDENCE / f"{operation}.exit.txt").write_text(f"{process.returncode}\n")
    return process.returncode, response


def main() -> int:
    cases = [
        ("fit_psf",
         {"observation": {"$ref": "observation"}, "wcs": {"$ref": "wcs"},
          "stars": {"$ref": "stars"}},
         {"family": "gaussian", "spatial_degree": 0, "initial_width_pixel": 1.5,
          "tail_absolute_flux": 1e-4, "window_radius_pixel": 4.0}),
        ("measure_science_sources",
         {"observation": {"$ref": "observation"}, "wcs": {"$ref": "wcs"},
          "stars": {"$ref": "stars"}, "psf": {"$ref": "psf"}},
         {"family": "gaussian", "spatial_degree": 0, "initial_width_pixel": 1.5,
          "tail_absolute_flux": 1e-4, "window_radius_pixel": 4.0}),
        ("fit_photometry",
         {"measurements": {"$ref": "measurements"}, "stars": {"$ref": "stars"},
          "predictions": {"$ref": "prediction"}},
         {"solve": {"relative_residual": 1e-10, "absolute_residual": 1e-12,
                    "rank_relative": 1e-10},
          "max_outer_iterations": 32, "gradient_tolerance": 1e-10,
          "step_tolerance": 1e-10, "likelihood": "joint_flux_gaussian"}),
        ("apply_scale",
         {"solution": {"$ref": "solution"}, "observation": {"$ref": "observation"}},
         {"frame": "contract-frame"}),
    ]
    failures = []
    for operation, arguments, request in cases:
        code, response = run(operation, arguments, request)
        result = response.get("result", {})
        print(f"[{operation}] exit={code} code={result.get('code')} "
              f"value={json.dumps(result.get('value'))[:80]}")
        for issue in result.get("assessment", {}).get("issues", []) or []:
            print(f"    issue: {issue.get('code')}: {issue.get('reason')}")
        if code != 0:
            failures.append(f"{operation} exited {code}")
        if result.get("code") != "ok":
            failures.append(f"{operation} answered {result.get('code')}")
    if failures:
        for failure in failures:
            print(f"[FAIL] {failure}")
        return 1
    print("[PASS] the four operations of the observation family answer with ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
