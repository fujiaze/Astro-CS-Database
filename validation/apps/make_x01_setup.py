#!/usr/bin/env python3
"""Builds the configuration of the X01 run of the apps validation.

The script reads the header of the X01 frames, writes the catalogue, the
response and the scientific request of the run and the configuration file
itself.  The paths are relative to the directory of the configuration file.
"""
from __future__ import annotations

import json
import math
import struct
import sys
from pathlib import Path


def read_header(path: Path) -> dict:
    cards: dict[str, str] = {}
    with path.open("rb") as stream:
        while True:
            block = stream.read(2880)
            if not block:
                break
            for start in range(0, len(block), 80):
                card = block[start : start + 80].decode("ascii", "replace")
                key = card[:8].strip()
                if key == "END":
                    return cards
                if not key or key in ("COMMENT", "HISTORY"):
                    continue
                value = card[10:].split("/")[0].strip().strip("'").strip()
                cards[key] = value
            break
    return cards


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    frames_dir = root / "validation/x01/input/frames"
    out = root / "validation/apps/evidence/x01"
    out.mkdir(parents=True, exist_ok=True)

    inputs = []
    frame_options = []
    for index in range(8):
        group = "A" if index < 4 else "B"
        name = f"frame_{index:02d}_{group}.fits"
        frame_id = f"frame-{index:02d}"
        inputs.append(
            {
                "path": f"../../../x01/input/frames/{name}",
                "frame_id": frame_id,
                "response": "response.json",
            }
        )
        header = read_header(frames_dir / name)
        if header.get("CTYPE1"):
            crpix1 = float(header.get("CRPIX1", 1.0))
            crpix2 = float(header.get("CRPIX2", 1.0))
            crval1 = float(header.get("CRVAL1", 0.0))
            crval2 = float(header.get("CRVAL2", 0.0))
            cd11 = float(header.get("CD1_1", 0.0))
            cd12 = float(header.get("CD1_2", 0.0))
            cd21 = float(header.get("CD2_1", 0.0))
            cd22 = float(header.get("CD2_2", 0.0))
            frame_options.append(
                {
                    "frame": frame_id,
                    "supplied_wcs": {
                        "crpix_fits1": [crpix1, crpix2],
                        "cd_rad_per_pixel": [
                            math.radians(cd11),
                            math.radians(cd12),
                            math.radians(cd21),
                            math.radians(cd22),
                        ],
                        "c": [
                            math.cos(math.radians(crval2)) * math.cos(math.radians(crval1)),
                            math.cos(math.radians(crval2)) * math.sin(math.radians(crval1)),
                            math.sin(math.radians(crval2)),
                        ],
                        "e": [
                            -math.sin(math.radians(crval1)),
                            math.cos(math.radians(crval1)),
                            0.0,
                        ],
                        "north": [
                            -math.sin(math.radians(crval2)) * math.cos(math.radians(crval1)),
                            -math.sin(math.radians(crval2)) * math.sin(math.radians(crval1)),
                            math.cos(math.radians(crval2)),
                        ],
                        "domain": {"lower": [-0.5, -0.5], "upper": [127.5, 127.5]},
                        "detector_frame": frame_id,
                    },
                }
            )

    # The response: a flat throughput over the support of the case.
    wavelengths = [400.0 + 5.0 * i for i in range(61)]
    throughput = []
    for wavelength in wavelengths:
        line = math.exp(-0.5 * ((wavelength - 656.3) / 1.5) ** 2)
        throughput.append(0.2 + 0.8 * line)
    response = {
        "convention": "photon_throughput",
        "wavelength_nm": wavelengths,
        "throughput": throughput,
        "support_nm": {"lower": 400.0, "upper": 700.0},
        "provenance": "x01:synthetic-response",
        "normalization": "unity",
    }
    (out / "response.json").write_text(json.dumps(response, indent=1) + "\n", encoding="utf-8")

    # The catalogue: one star at the centre of the field of the A group.
    header = read_header(frames_dir / "frame_00_A.fits")
    ra_deg = float(header.get("CRVAL1", 0.0))
    dec_deg = float(header.get("CRVAL2", 0.0))
    catalogue = {
        "source_ids": [1],
        "astrometry": [[math.radians(ra_deg), math.radians(dec_deg), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]],
        "brightness_mag": [6.0],
        "xp_coefficients": [[1.0, 0.0, 0.0, 0.0]],
        "xp_basis": {
            "knots_nm": [400.0, 500.0, 600.0, 700.0],
            "basis_coefficients": [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0],
                                    [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]],
            "degree": 3,
            "coefficient_count": 4,
            "support_nm": {"lower": 400.0, "upper": 700.0},
        },
        "build": {
            "sky_order": 3,
            "max_stars_per_cell": 64,
            "neighbors_per_anchor": 12,
            "quads_per_anchor": 8,
            "quad_baseline_rad": {"lower": 0.0, "upper": 0.0},
            "release": "x01",
            "calibration": "none",
        },
    }
    (out / "catalogue.json").write_text(json.dumps(catalogue, indent=1) + "\n", encoding="utf-8")

    cd11 = float(header.get("CD1_1", -0.001))
    cd22 = float(header.get("CD2_2", 0.001))
    scale = math.hypot(cd11, cd22) * math.pi / 180.0
    scientific = {
        "command": "normalize",
        "request": {
            "astrometry": {
                "regions": [
                    {
                        "center": {
                            "nx": math.cos(math.radians(dec_deg)) * math.cos(math.radians(ra_deg)),
                            "ny": math.cos(math.radians(dec_deg)) * math.sin(math.radians(ra_deg)),
                            "nz": math.sin(math.radians(crval2)),
                        },
                        "radius_rad": 0.05,
                    }
                ],
                "scale_rad_pixel": {"lower": scale * 0.5, "upper": scale * 2.0},
                "max_candidates": 32,
                "max_hypotheses": 4,
                "sip_order": 2,
                "detect_snr": 5.0,
                "match_radius_pixel": 3.0,
                "min_matches": 4,
                "quad_descriptor_tolerance": 0.2,
            },
            "psf": {
                "family": "gaussian",
                "spatial_degree": 0,
                "initial_width_pixel": 2.0,
                "tail_absolute_flux": 1e-6,
                "window_radius_pixel": 8.0,
            },
            "photometry": {
                "application_id": "x01",
                "basis": {"kind": "constant"},
                "gradient_tolerance": 1e-8,
                "step_tolerance": 1e-8,
                "max_outer_iterations": 20,
            },
            "drizzle": {
                "order": 6,
                "pixfrac": 1.0,
                "budget": {
                    "area_absolute_sr": 1e-12,
                    "normalizer_min_sr": 1e-15,
                    "max_work": 2000000,
                    "max_bytes": 268435456,
                    "max_depth": 64,
                },
            },
            "allow_relative_uncalibrated": True,
        },
        "frame_options": frame_options,
    }
    (out / "normalize.scientific.json").write_text(json.dumps(scientific, indent=1) + "\n",
                                                    encoding="utf-8")

    config = {
        "schema": "acsd.config/2",
        "command": "normalize",
        "inputs": inputs,
        "catalogue": "catalogue.json",
        "scientific": "normalize.scientific.json",
        "output": "products",
        "resources": {
            "bytes": 1073741824,
            "work": 10000000,
            "spill_bytes": 0,
            "threads": 4,
            "max_depth": 64,
            "max_precision_bits": 256,
            "scratch_directory": ".work",
        },
        "product_profile": "full",
        "publication": {"signal_dtype": "f64", "tile_log2_width": 6, "durable": True},
    }
    (out / "config.json").write_text(json.dumps(config, indent=1) + "\n", encoding="utf-8")
    print(json.dumps({"config": str(out / "config.json"), "frames": len(inputs)}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
