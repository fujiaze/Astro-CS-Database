#!/usr/bin/env python3
"""Build the cross-panel M2+M5 Normalize inputs (issue #126 section D).

Two *different* M1..M6 panels with certified Gaia WCS are cropped around the
real sky overlap of their pointings:
  M2 20251212 east edge  x0=3072, 1024x1024
  M5 20251212 west edge  x0=0,    1024x1024
The crops keep the frozen calibration masters (cropped) and the certified
Gaia/SIP solution shifted by the crop origin.  The frames are never modified in
testdata/.
"""
from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

import numpy as np
from astropy.io import fits

ROOT = Path("/workspace/Astro CS Database")
M42 = ROOT / "testdata/M42_T2T3_mosaic_Flying_dutchman"
S1_INPUTS = ROOT / "run/m42-s1/inputs"
OUT = ROOT / "run/issue126/cross_panel"

FRAMES = {
    "M2": {
        "path": M42 / "T2/M2/M42_M2_T2_flying_dutchman-20251212@021829-600S-H-alpha.fts",
        "frame": "M42_M2_T2_flying_dutchman-20251212_021829-600S-H-alpha",
        "wcs": ROOT / "run/issue126/astrometry/wcs_20251212_M2.json",
    },
    "M5": {
        "path": M42 / "T2/M5/M42_M5_T2_flying_dutchman-20251212@041842-600S-H-alpha.fts",
        "frame": "M42_M5_T2_flying_dutchman-20251212_041842-600S-H-alpha",
        "wcs": S1_INPUTS / "gaia/wcs_20251212.json",
    },
}


def build(panel: str, x0: int, y0: int, n: int, name: str) -> Path:
    info = FRAMES[panel]
    dst = OUT / name
    for sub in ("cal", "out", "scratch"):
        (dst / sub).mkdir(parents=True, exist_ok=True)
    with fits.open(info["path"]) as handle:
        header = handle[0].header.copy()
        data = handle[0].data[y0:y0 + n, x0:x0 + n]
    header["CRPIX1"] = float(header["CRPIX1"]) - x0
    header["CRPIX2"] = float(header["CRPIX2"]) - y0
    fits.PrimaryHDU(data.astype(np.uint16), header=header).writeto(dst / "frame.fits",
                                                                   overwrite=True)
    planes = {
        "bias": S1_INPUTS / "calibration/bias_adu.fits",
        "dark": S1_INPUTS / "calibration/dark600_adu.fits",
        "flat": S1_INPUTS / "calibration/flat_ha_normalized.fits",
    }
    for key, source in planes.items():
        with fits.open(source) as handle:
            plane = handle[0].data[y0:y0 + n, x0:x0 + n]
        fits.PrimaryHDU(plane.astype(np.float32)).writeto(dst / "cal" / f"{key}.fits",
                                                          overwrite=True)
    calibration = json.loads((S1_INPUTS / "calibration/calibration.json").read_text())
    calibration["bias_file"] = "bias.fits"
    calibration["dark_file"] = "dark.fits"
    calibration["flat_file"] = "flat.fits"
    (dst / "cal/calibration.json").write_text(json.dumps(calibration, indent=1))
    shutil.copy(S1_INPUTS / "gaia/catalogue.json", dst / "catalogue.json")
    shutil.copy(S1_INPUTS / "response/response_ha.json", dst / "response_ha.json")
    wcs = json.loads(info["wcs"].read_text())
    wcs["crpix_fits1"] = [wcs["crpix_fits1"][0] - x0, wcs["crpix_fits1"][1] - y0]
    wcs["domain"] = {"lower": [-0.5, -0.5], "upper": [n - 0.5, n - 0.5]}
    wcs["detector_frame"] = info["frame"]
    budget = {
        "tolerance": {"absolute": 1e-10, "relative": 1e-10, "absolute_floor": 0.0,
                      "unit": {"scale": "1", "sr_power": 0, "pixel_power": 0, "nm_power": 0,
                               "multiplier": 1.0, "linear_power": 0}},
        "max_iterations": 128,
        "arithmetic": {"mode": "outward", "initial_bits": 128, "max_bits": 256,
                       "allow_target_fallback": False},
    }
    scientific = {
        "command": "normalize",
        "request": {
            "astrometry": {
                "regions": [{"center": {"nx": wcs["c"][0], "ny": wcs["c"][1], "nz": wcs["c"][2]},
                             "radius_rad": 0.02}],
                "scale_rad_pixel": {"lower": 1e-6, "upper": 3e-6},
                "max_candidates": 64, "max_hypotheses": 8, "sip_order": 3,
                "detect_snr": 5.0, "match_radius_pixel": 3.0, "min_matches": 4,
                "quad_descriptor_tolerance": 0.2,
                "budget": {**budget, "max_iterations": 128},
            },
            "psf": {"family": "gaussian", "spatial_degree": 0, "initial_width_pixel": 1.5,
                    "tail_absolute_flux": 1e-4, "window_radius_pixel": 10.0,
                    "budget": {**budget, "max_iterations": 128}},
            "drizzle": {"order": "auto", "pixfrac": 1.0,
                        "budget": {"requested_functionals": [], "per_leaf_absolute_area": [],
                                   "area_absolute_sr": 1e-15, "normalizer_min_sr": 0.0,
                                   "max_work": 64, "max_bytes": 1073741824,
                                   "max_depth": 32,
                                   "arithmetic": {"mode": "outward", "initial_bits": 64,
                                                  "max_bits": 256,
                                                  "allow_target_fallback": False}}},
            "information": [],
            "allow_relative_uncalibrated": True,
        },
        "frame_options": [{"frame": info["frame"], "supplied_wcs": wcs, "regroup": None}],
    }
    (dst / "normalize.scientific.json").write_text(json.dumps(scientific, indent=1))
    config = {
        "schema": "acsd.config/2", "command": "normalize",
        "inputs": [{"path": str(dst / "frame.fits"), "frame_id": info["frame"],
                    "response": "response_ha.json", "calibration": "cal/calibration.json"}],
        "catalogue": "catalogue.json", "scientific": "normalize.scientific.json",
        "output": "out",
        "resources": {"bytes": 12884901888, "work": 1000000000000, "spill_bytes": 8589934592,
                      "threads": 4, "max_depth": 32, "max_precision_bits": 256,
                      "scratch_directory": str(dst / "scratch")},
        "product_profile": "signal_only",
        "publication": {"signal_dtype": "f64", "tile_log2_width": 6, "durable": False},
    }
    (dst / "normalize.config.json").write_text(json.dumps(config, indent=1))
    (dst / "run_identity.json").write_text(json.dumps({
        "panel": panel, "frame": info["frame"], "crop_x0": x0, "crop_y0": y0, "crop_n": n,
        "source": str(info["path"]), "wcs": str(info["wcs"]),
    }, indent=1))
    return dst


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=1024)
    ap.add_argument("--y0-m2", type=int, default=3072)
    ap.add_argument("--y0-m5", type=int, default=4)
    ap.add_argument("--x0-m2", type=int, default=1536)
    ap.add_argument("--x0-m5", type=int, default=1536)
    args = ap.parse_args()
    m2 = build("M2", args.x0_m2, args.y0_m2, args.n, "M2_1024")
    m5 = build("M5", args.x0_m5, args.y0_m5, args.n, "M5_1024")
    print(m2)
    print(m5)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
