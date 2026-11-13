#!/usr/bin/env python3
"""Export each cross-panel Normalize product on the common target WCS.

The per-frame exports are the audit material of the cross-panel seam: they use
the same target model as the mosaic export, so every pixel is the same sky
sample and the two panels can be compared without any resampling difference.
"""
from __future__ import annotations

import hashlib
import json
import os
import sys

CP = "/workspace/Astro CS Database/run/issue126/cross_panel"
SUFFIX = sys.argv[1] if len(sys.argv) > 1 else "north_1024"
OUT = os.path.join(CP, "mosaic_run" + SUFFIX.replace("north_", ""))


def product_info(panel: str):
    directory = os.path.join(CP, f"{panel}_{SUFFIX}", "out")
    names = [n for n in os.listdir(directory) if os.path.isdir(os.path.join(directory, n))]
    root = os.path.join(directory, names[0])
    manifest = json.load(open(os.path.join(root, "manifest.json")))
    canonical = json.dumps(manifest, separators=(",", ":"), sort_keys=True).encode()
    return root, "sha256:" + hashlib.sha256(canonical).hexdigest()


def main():
    export = json.load(open(os.path.join(OUT, "export.scientific.json")))
    resources = {"bytes": 12884901888, "work": 1000000000000, "spill_bytes": 8589934592,
                 "threads": 4, "max_depth": 32, "max_precision_bits": 256,
                 "scratch_directory": os.path.join(OUT, "scratch_frame_export")}
    for panel in ("M2", "M5"):
        root, manifest_hash = product_info(panel)
        directory = os.path.join(OUT, f"export_{panel}")
        os.makedirs(directory, exist_ok=True)
        json.dump(export, open(os.path.join(directory, "export.scientific.json"), "w"), indent=1)
        config = {
            "schema": "acsd.config/2", "command": "export",
            "inputs": [{"path": root, "expected_hash": manifest_hash}],
            "scientific": "export.scientific.json", "output": "out",
            "resources": resources, "product_profile": "signal_only",
            "publication": {"signal_dtype": "f64", "tile_log2_width": 6, "durable": False},
        }
        json.dump(config, open(os.path.join(directory, "export.config.json"), "w"), indent=1)
        print(directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
