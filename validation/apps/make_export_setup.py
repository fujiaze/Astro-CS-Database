#!/usr/bin/env python3
"""Builds the configuration of the export run of the apps validation.

The target WCS is placed on the leaves of the frame product that the round
trip publishes, so that the exported image really covers the signal.
"""
from __future__ import annotations

import hashlib
import json
import math
import sys
from pathlib import Path

J_FACE = [2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4]
L_FACE = [1, 3, 5, 7, 0, 2, 4, 6, 1, 3, 5, 7]


def leaf_direction(order: int, ipix: int) -> tuple[float, float]:
    """The equatorial direction of a leaf, in radians."""
    nside = 1 << order
    face = ipix // (nside * nside)
    in_face = ipix % (nside * nside)
    ix = 0
    iy = 0
    for level in range(order):
        if (in_face >> (2 * level)) & 1:
            ix |= 1 << level
        if (in_face >> (2 * level + 1)) & 1:
            iy |= 1 << level
    s = (ix + 0.5) / nside
    t = (iy + 0.5) / nside
    r = J_FACE[face] - s - t
    d = s - t
    if r < 1.0:
        z = 1.0 - r * r / 3.0
        phi = (math.pi / 4.0) * (L_FACE[face] + d / r)
    elif r <= 3.0:
        z = (2.0 - r) * 2.0 / 3.0
        phi = (math.pi / 4.0) * (L_FACE[face] + d)
    else:
        z = -1.0 + (4.0 - r) ** 2 / 3.0
        phi = (math.pi / 4.0) * (L_FACE[face] + d / (4.0 - r))
    return phi, math.asin(z)


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    product = Path(sys.argv[1]) if len(sys.argv) > 1 else root / ".work/product-roundtrip/frame"
    out = root / "validation/apps/evidence/export"
    out.mkdir(parents=True, exist_ok=True)

    manifest = (product / "manifest.json").read_bytes()
    manifest_hash = "sha256:" + hashlib.sha256(manifest).hexdigest()

    phi, dec = leaf_direction(6, 100)
    ra = phi
    scale = math.radians(0.01)  # one hundredth of a degree per pixel
    scientific = {
        "command": "export",
        "request": {
            "projection": {
                "target": {
                    "model": {
                        "crpix_fits1": [8.5, 8.5],
                        "cd_rad_per_pixel": [-scale, 0.0, 0.0, scale],
                        "c": [
                            math.cos(dec) * math.cos(ra),
                            math.cos(dec) * math.sin(ra),
                            math.sin(dec),
                        ],
                        "e": [
                            -math.sin(ra),
                            math.cos(ra),
                            0.0,
                        ],
                        "north": [
                            -math.sin(dec) * math.cos(ra),
                            -math.sin(dec) * math.sin(ra),
                            math.cos(dec),
                        ],
                        "domain": {"lower": [-0.5, -0.5], "upper": [15.5, 15.5]},
                        "detector_frame": "acsd.export.target",
                    },
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
            }
        },
    }
    (out / "export.scientific.json").write_text(json.dumps(scientific, indent=1) + "\n",
                                                 encoding="utf-8")
    config = {
        "schema": "acsd.config/2",
        "command": "export",
        "inputs": [{"path": str(product), "expected_hash": manifest_hash}],
        "scientific": "export.scientific.json",
        "output": "products/export",
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
    }
    (out / "config.json").write_text(json.dumps(config, indent=1) + "\n", encoding="utf-8")
    print(json.dumps({"config": str(out / "config.json"), "manifest_hash": manifest_hash}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
