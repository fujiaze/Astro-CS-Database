#!/usr/bin/env python3
"""Mosaic + Export configs for the cross-panel M2+M5 leaf products (issue #126 D).

The target WCS is the M5 crop solution, shifted so that the union of the two
sky footprints sits in a common pixel box at the native 0.967"/px scale; the
SIP polynomial is translation invariant under the CRPIX shift (u = x_fits -
CRPIX is unchanged), so the target map is the certified camera model evaluated
over the union, not a re-fit.
"""
from __future__ import annotations

import hashlib
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wcs_model import WcsModel

ROOT = "/workspace/Astro CS Database"
CP = os.path.join(ROOT, "run/issue126/cross_panel")
SUFFIX = sys.argv[1] if len(sys.argv) > 1 else "north_1024"
OUT = os.path.join(CP, "mosaic_run" + SUFFIX.replace("north_", ""))


def product_info(panel: str):
    directory = os.path.join(CP, f"{panel}_{SUFFIX}", "out")
    names = [n for n in os.listdir(directory) if os.path.isdir(os.path.join(directory, n))]
    assert len(names) == 1, names
    root = os.path.join(directory, names[0])
    manifest = json.load(open(os.path.join(root, "manifest.json")))
    canonical = json.dumps(manifest, separators=(",", ":"), sort_keys=True).encode()
    manifest_hash = "sha256:" + hashlib.sha256(canonical).hexdigest()
    sidecar = json.load(open(os.path.join(root, "frame_science.json")))["data"]
    graph_id = sidecar["identity"]["object_id"]
    estimate_id = sidecar["signal"]["$ref"]
    objects = {}
    for entry in manifest["objects"]:
        objects[entry["hash"]] = json.load(open(os.path.join(root, entry["path"])))["data"]
    estimate = objects[estimate_id]
    response_id = estimate["effective_response"]["$ref"]
    response = objects[response_id]
    return {
        "root": root,
        "manifest_hash": manifest_hash,
        "graph": graph_id,
        "response": response_id,
        "response_actions": [a["$ref"] for a in response.get("spatial_actions", [])],
    }


def newton_inverse(wcs: WcsModel, ra, dec, guess, iterations=30):
    """Solve wcs.forward(x,y) = (ra,dec) by Newton on the unit sphere."""
    x = np.array(guess, dtype=float)
    target = np.array([ra, dec], float)

    def angle_residual(px, py):
        r, d = wcs.forward(np.array([px]), np.array([py]))
        return np.array([(ra - r[0]) * np.cos(np.radians(dec)), dec - d[0]])

    for _ in range(iterations):
        res = angle_residual(x[0], x[1])
        if np.max(np.abs(res)) < 1e-9:
            break
        h = 1e-3
        j = np.zeros((2, 2))
        for k in range(2):
            step = np.zeros(2); step[k] = h
            f1 = angle_residual(x[0] + step[0], x[1] + step[1])
            f0 = angle_residual(x[0] - step[0], x[1] - step[1])
            j[:, k] = (f1 - f0) / (2 * h)
        try:
            x = x + np.linalg.solve(j, -res)
        except np.linalg.LinAlgError:
            break
    return x, np.max(np.abs(angle_residual(x[0], x[1])))


def main():
    args_size = 1024 if SUFFIX == "north_1024" else 512
    info = {p: product_info(p) for p in ("M2", "M5")}
    wcs2 = WcsModel(json.load(open(os.path.join(CP, f"M2_{SUFFIX}/normalize.scientific.json")))["frame_options"][0]["supplied_wcs"])
    wcs5 = WcsModel(json.load(open(os.path.join(CP, f"M5_{SUFFIX}/normalize.scientific.json")))["frame_options"][0]["supplied_wcs"])
    corners = []
    for wcs in (wcs2, wcs5):
        ra, dec = wcs.corner_sky(0, 0, args_size - 1, args_size - 1)
        corners.extend(zip(ra, dec))
    # Sky corners of both crops in the M5 crop pixel frame.
    pixels = []
    for ra, dec in corners:
        x, err = newton_inverse(wcs5, ra, dec, guess=(512.0, 512.0))
        pixels.append((x[0], x[1], err))
        assert err < 1e-6, (ra, dec, err)
    xs = [p[0] for p in pixels]; ys = [p[1] for p in pixels]
    x_min = float(np.floor(min(xs))) - 40.0
    x_max = float(np.ceil(max(xs))) + 40.0
    y_min = float(np.floor(min(ys))) - 40.0
    y_max = float(np.ceil(max(ys))) + 40.0
    width = int(x_max - x_min)
    height = int(y_max - y_min)
    print(f"union in M5-crop pixels: x {x_min:.1f}..{x_max:.1f} y {y_min:.1f}..{y_max:.1f} -> {width}x{height}")
    target_doc = json.loads(json.dumps(wcs5.doc))
    target_doc["crpix_fits1"] = [wcs5.crpix[0] - x_min, wcs5.crpix[1] - y_min]
    target_doc["domain"] = {"lower": [-0.5, -0.5], "upper": [width - 0.5, height - 0.5]}
    target_doc["detector_frame"] = "acsd.target"
    target = {"model": target_doc, "width": width, "height": height}

    os.makedirs(OUT, exist_ok=True)
    mosaic = {
        "command": "mosaic",
        "request": {
            "sky": {
                "groups": [{
                    "frames": [info[p]["graph"] for p in ("M2", "M5")],
                    "response_proof": info["M2"]["response"],
                    "spatial_kernel_proof": info["M2"]["response"],
                    "assumption_kind": "same_filter_relative_response_shape_different_kernel_conditional",
                    "fixed_objects": [info[p]["response"] for p in ("M2", "M5")],
                }],
                "basis": {"kind": "constant", "degree": 0,
                          "normalization_domain": {"lower": [0.0, 0.0], "upper": [1.0, 1.0]},
                          "center": [0.0, 0.0], "scale": [1.0, 1.0], "weighted_mean": None},
                "gauge": {"kind": "reference_frame", "reference": info["M2"]["graph"], "weights": None},
                "T": None, "H": None, "control_noise": None,
                "solve": {"relative_residual": 1e-8, "absolute_residual": 1e-12,
                          "rank_relative": 1e-10, "max_iterations": 64,
                          "dense_max_bytes": 16777216, "huber": False, "huber_delta": 0.0},
                "allow_prediction": False,
            },
            "coaddition": {
                "target": {"id": "m42-cross-panel", "quantity": "brightness",
                           "unit": {"scale": "1", "sr_power": 0, "pixel_power": 0, "nm_power": 0,
                                    "multiplier": 1.0, "linear_power": 0}},
                "mixed_observation": False,
                "weights": "automatic",
                "rejection": {"kind": "dq_only", "alpha": 0.0,
                              "external_fixed_covariance": None, "common_mean_proof": ""},
                "quantities": [],
                "solve": {"relative_residual": 1e-8, "absolute_residual": 1e-12,
                          "rank_relative": 1e-10, "max_iterations": 64,
                          "dense_max_bytes": 16777216, "huber": False, "huber_delta": 0.0},
                "groups": [],
            },
        },
    }
    json.dump(mosaic, open(os.path.join(OUT, "mosaic.scientific.json"), "w"), indent=1)
    export = {
        "command": "export",
        "request": {
            "projection": {
                "target": target,
                "quantity": "supported_mean",
                "support_policy": "full_leaf_approx",
                "assume_uniform_uncovered": False,
                "output_dtype": "f64",
                "budget": {"requested_functionals": [], "per_leaf_absolute_area": [],
                           "area_absolute_sr": 1e-15, "normalizer_min_sr": 0.0,
                           "max_work": 1000000000000, "max_bytes": 1073741824,
                           "max_depth": 32,
                           "arithmetic": {"mode": "outward", "initial_bits": 64, "max_bits": 256,
                                          "allow_target_fallback": False}},
            }
        },
    }
    json.dump(export, open(os.path.join(OUT, "export.scientific.json"), "w"), indent=1)
    resources = {"bytes": 12884901888, "work": 1000000000000, "spill_bytes": 8589934592,
                 "threads": 4, "max_depth": 32, "max_precision_bits": 256,
                 "scratch_directory": os.path.join(OUT, "scratch")}
    mosaic_config = {
        "schema": "acsd.config/2", "command": "mosaic",
        "inputs": [{"path": info[p]["root"], "expected_hash": info[p]["manifest_hash"]}
                   for p in ("M2", "M5")],
        "scientific": "mosaic.scientific.json",
        "output": os.path.join(OUT, "mosaic"),
        "resources": resources, "product_profile": "signal_only",
        "publication": {"signal_dtype": "f64", "tile_log2_width": 6, "durable": False},
    }
    json.dump(mosaic_config, open(os.path.join(OUT, "mosaic.config.json"), "w"), indent=1)
    export_config = {
        "schema": "acsd.config/2", "command": "export",
        "inputs": [{"path": os.path.join(OUT, "mosaic"), "expected_hash": "sha256:" + "0" * 64}],
        "scientific": "export.scientific.json",
        "output": os.path.join(OUT, "export"),
        "resources": resources, "product_profile": "signal_only",
        "publication": {"signal_dtype": "f64", "tile_log2_width": 6, "durable": False},
    }
    json.dump(export_config, open(os.path.join(OUT, "export.config.json"), "w"), indent=1)
    print(json.dumps({p: info[p]["graph"] for p in info}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
