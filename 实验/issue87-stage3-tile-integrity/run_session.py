#!/usr/bin/env python3
"""Issue #87 / E2：正式 session 入口（p3_session_run）的 tile 故障传播证据。

p3_session_run 不在 CLI 的 phase3 IR 节点链上（见 REPORT「两个生产入口」），
因此经其生产 C API 驱动：编译 session_tile_probe.cpp 并链接当前构建的生产库。

用法:
  python3 实验/issue87-stage3-tile-integrity/run_session.py --build build/linux-dev
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(HERE, "work")
EVID = os.path.join(HERE, "evidence", "session")
PROBE = os.path.join(HERE, "session_tile_probe.cpp")
sys.path.insert(0, HERE)
from make_fixtures import tile_center  # noqa: E402

LIBS = [
    "libacsd_phase3_session.a",
    "lib/algorithms/fits_output/libacsd_p3_fits_output.a",
    "lib/algorithms/resample/libacsd_p3_rsmp.a",
    "lib/algorithms/projection/libacsd_p3_projection_wcs.a",
    "libacsd_hips_properties.a",
    "libacsd_hips.a",
    "libacsd_aio.a",
    "libacsd_common.a",
    "libacsd_cfitsio.a",
]
INCLUDES = ["lib/phase3_session", "lib/include", "lib/include/acsd",
            "lib/algorithms/projection", "lib/algorithms/resample",
            "lib/algorithms/fits_output", "lib/algorithms/shared",
            "lib/algorithms/shared/crypto", "lib/algorithms/shared/healpix",
            "lib/infrastructure/aio/include"]


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", required=True)
    ap.add_argument("--tag", default="fixed")
    args = ap.parse_args()
    build = os.path.abspath(args.build)
    root = os.path.join(WORK, "out_session", args.tag)
    os.makedirs(root, exist_ok=True)
    os.makedirs(EVID, exist_ok=True)
    exe = os.path.join(EVID, "session_tile_probe")
    cmd = ["g++", "-std=c++17", "-O2", "-o", exe, PROBE]
    cmd += ["-I" + os.path.join(REPO, i) for i in INCLUDES]
    cmd += ["-I" + build]
    cmd += [os.path.join(build, l) for l in LIBS]
    cmd += ["-lz", "-lpthread", "-lm"]
    print("+ " + " ".join(cmd))
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        print(f"compile failed rc={rc}", file=sys.stderr)
        return rc

    t4 = tile_center(0, 4)
    t5 = tile_center(0, 5)
    t7 = tile_center(0, 7)
    t5_o1 = tile_center(1, 5)
    scen = [
        dict(case="legal_full", hips="legal_full", ra=t4[0], dec=t4[1],
             w=64, h=64, scale=0.01, expect_status=0, expect_code=None,
             fits=True),
        dict(case="sparse_moc_outside", hips="legal_sparse_moc", ra=t7[0], dec=t7[1],
             w=64, h=64, scale=0.01, expect_status=0, expect_code=None,
             fits=True, expect_nan_all=True),
        dict(case="declared_missing", hips="declared_missing", ra=t5[0], dec=t5[1],
             w=64, h=64, scale=0.01, expect_status=1,
             expect_code="P3-TILE-DECLARED-MISSING", fits=False),
        dict(case="checksum_bad", hips="checksum_bad", ra=t4[0], dec=t4[1],
             w=64, h=64, scale=0.01, expect_status=1,
             expect_code="P3-TILE-CHECKSUM-FAILED", fits=False),
        dict(case="io_error", hips="io_error", ra=t4[0], dec=t4[1],
             w=64, h=64, scale=0.01, expect_status=4,
             expect_code="P3-TILE-IO-ERROR", fits=False),
        dict(case="undetermined_coarse_moc", hips="undetermined_coarse_moc",
             ra=t5_o1[0], dec=t5_o1[1], w=64, h=64, scale=0.01,
             expect_status=1, expect_code="P3-TILE-FOOTPRINT-UNDETERMINED",
             fits=False),
        dict(case="same_bad_multi_worker", hips="same_bad_multi_worker",
             ra=t4[0], dec=t4[1], w=256, h=256, scale=0.01,
             expect_status=1, expect_code="P3-TILE-CHECKSUM-FAILED", fits=False),
    ]
    results = []
    for sc in scen:
        case = sc["case"]
        out_dir = os.path.join(root, case)
        if os.path.isdir(out_dir):
            shutil.rmtree(out_dir)
        os.makedirs(out_dir, exist_ok=True)
        hips = os.path.join(WORK, "fixtures", sc["hips"])
        log_path = os.path.join(EVID, case + ".log")
        with open(log_path, "w", encoding="utf-8") as lf:
            proc = subprocess.run(
                [exe, hips, out_dir, str(sc["ra"]), str(sc["dec"]),
                 str(sc["w"]), str(sc["h"]), str(sc["scale"]), "nearest"],
                stdout=lf, stderr=subprocess.STDOUT)
        line = open(log_path, encoding="utf-8").read().strip().splitlines()[-1]
        try:
            rec = json.loads(line)
        except Exception:
            rec = {"raw": line}
        fits_path = os.path.join(out_dir, "output_phase3.fits")
        rec.update({
            "case": case, "hips_dir": hips, "out_dir": out_dir,
            "exit_code": proc.returncode, "log_path": log_path,
            "fits_present": os.path.isfile(fits_path),
            "expect_status": sc["expect_status"],
            "expect_code": sc["expect_code"],
        })
        reasons = []
        if rec.get("status") != sc["expect_status"]:
            reasons.append(f"status {rec.get('status')} != {sc['expect_status']}")
        if rec["fits_present"] != sc["fits"]:
            reasons.append(f"fits_present {rec['fits_present']} != {sc['fits']}")
        if sc["expect_code"] and sc["expect_code"] not in (rec.get("last_error") or ""):
            reasons.append(f"code {sc['expect_code']} not in last_error")
        if sc.get("expect_nan_all") and rec["fits_present"]:
            import numpy as np
            from astropy.io import fits as afits
            with afits.open(fits_path) as hdul:
                d = np.asarray(hdul[0].data)
                if not np.isnan(d).all():
                    reasons.append("expected all-NaN legal sparse output")
        rec["verdict"] = "PASS" if not reasons else "FAIL"
        rec["verdict_reason"] = "ok" if not reasons else "; ".join(reasons)
        results.append(rec)
        print(f"[session] {case:28s} status={rec.get('status')} "
              f"({rec.get('status_name')}) fits={'Y' if rec['fits_present'] else 'n'} "
              f"code={sc['expect_code'] or '-'} -> {rec['verdict']}"
              + ("" if rec["verdict"] == "PASS" else f" :: {rec['verdict_reason']}"))

    out = {"schema": "acsd.issue87.session/v1",
           "n_cases": len(results),
           "n_fail": sum(1 for r in results if r["verdict"] != "PASS"),
           "results": results}
    path = os.path.join(EVID, "results.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"wrote {path}: n={out['n_cases']} fail={out['n_fail']}")
    return 0 if out["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
