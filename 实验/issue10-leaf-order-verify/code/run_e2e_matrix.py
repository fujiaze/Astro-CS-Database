#!/usr/bin/env python3
"""Issue #10 —— 真实 Normalize E2E 矩阵（生产 CLI `acsd normalize --json cfg -y`）。

构型：
  · auto  : drizzle.nside=0（auto，复现历史 K=18 构型）—— post 与 pre-#10 各跑一次；
  · k9    : drizzle.nside=512  → K=9,  Norder0；
  · k11   : drizzle.nside=2048 → K=11, Norder2；
  · k13数学例：8192 仅为公式单测（不为它分配 8192 帧），见 verify_leaf_order 的数学例。

用法：
    python3 run_e2e_matrix.py --acsd-post <bin> --acsd-pre10 <bin> \
        [--cases auto,k9,k11] [--base /tmp/issue10/run]
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
LIGHT = ("testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M4/"
         "M42_M4_T2_flying_dutchman-20251212@033644-600S-H-alpha.fts")
MASTERS = "testdata/T2 calibration files"
GAIA = "gaia/GaiaDR3SP"


def make_cfg(base: str, case: str, nside: int) -> str:
    out = os.path.join(base, case, "out")
    work = os.path.join(base, case, "work")
    cfg = {
        "schema_version": "1",
        "blocks": [{
            "name": "issue10_" + case,
            "input_lights": [os.path.join(REPO, LIGHT)],
            "master_bias": os.path.join(REPO, MASTERS, "masterBias_BIN-1_4096x4096.xisf"),
            "master_dark": os.path.join(REPO, MASTERS,
                                        "masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf"),
            "master_flat": os.path.join(REPO, MASTERS,
                                        "masterFlat_BIN-1_4096x4096_FILTER-H-alpha_mono.xisf"),
            "output_dir": out,
            "work_dir": work,
            "drizzle": {"nested": 1, "nside": nside, "pixfrac": 1.0, "precision_mode": 1},
            "filter_passband": "Baader 7nm H-alpha",
            "wcs": {"init_source": "header_pointing", "gaia_data_dir": os.path.join(REPO, GAIA)},
            "dark_optimization": True,
            "master_flat_normalize": "median",
            "master_units": {"bias": "normalized", "dark": "normalized", "flat": "normalized"},
            "master_scale": {"bias": 65535, "dark": 65535, "flat": 65535},
            "snr": {"gain_e_per_adu": 1.0, "read_noise_e": 9.0,
                    "aperture_radius_px": 4, "n_sky": 20,
                    "reference": {"kind": "explicit_linear_flux",
                                  "reference_id": "user:test-ref-star", "flux": 1000.0,
                                  "flux_unit": "adu", "normalization": "native_calibrated"}},
        }],
    }
    os.makedirs(os.path.join(base, case), exist_ok=True)
    p = os.path.join(base, case, "config.json")
    json.dump(cfg, open(p, "w"), indent=2)
    return p


def run_case(binary: str, cfg: str, base: str, case: str) -> dict:
    logs = os.path.join(base, case)
    t0 = time.time()
    p = subprocess.run([binary, "normalize", "--json", cfg, "-y"],
                       capture_output=True, text=True)
    dt = time.time() - t0
    open(os.path.join(logs, case + ".stderr"), "w").write(p.stderr)
    open(os.path.join(logs, case + ".stdout"), "w").write(p.stdout)
    return {"case": case, "binary": binary, "exit": p.returncode,
            "wall_s": round(dt, 2), "config": cfg}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd-post", default="/tmp/issue10/bin/acsd_post")
    ap.add_argument("--acsd-pre10", default=None)
    ap.add_argument("--cases", default="auto,k9,k11")
    ap.add_argument("--base", default="/tmp/issue10/run")
    a = ap.parse_args()
    nsides = {"auto": 0, "k9": 512, "k11": 2048}
    results = []
    plan = []
    for c in a.cases.split(","):
        plan.append((c, a.acsd_post))
    if a.acsd_pre10:
        plan.append(("auto_pre10", a.acsd_pre10))
    for case, binary in plan:
        key = "auto" if case.startswith("auto") else case
        cfg = make_cfg(a.base, case, nsides[key])
        print(f"running {case} with {binary} ...", flush=True)
        r = run_case(binary, cfg, a.base, case)
        print("  ->", r, flush=True)
        results.append(r)
    json.dump(results, open(os.path.join(a.base, "matrix.json"), "w"), indent=2)
    return 0 if all(r["exit"] == 0 for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
