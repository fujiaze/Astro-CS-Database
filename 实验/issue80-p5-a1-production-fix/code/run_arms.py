#!/usr/bin/env python3
"""#80 P0-A1 — run the frozen M42 arms through the *production* mosaic CLI.

Records exit code, stderr markers and small-artifact hashes.  Never mutates the
frozen inputs (hardlink tree built by the E0 unit); outputs go to a caller-given
scratch root so the E0 evidence stays byte-identical.

Usage:
    python3 run_arms.py --acsd <path> --label prefix \
        --arms abc_real,ab_2halpha --scratch /workspace/issue80-scratch \
        --out 实验/issue80-p5-a1-production-fix/evidence/<label>
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
INDEX = os.path.join(REPO, "实验/issue80-p5-nonspd-e0/evidence/input_index.json")


def sha256_file(path: str, chunk: int = 1 << 20) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for c in iter(lambda: fh.read(chunk), b""):
            h.update(c)
    return h.hexdigest()


def small_artifacts(out_dir: str) -> dict:
    keep: dict = {}
    for name in ("p2_final.json", "p2_coverage.json", "p2_sky_plane.bin",
                 "p2_sky_plane.json", "acsd_run_manifest.json", "p2_samples.json"):
        p = os.path.join(out_dir, name)
        if os.path.isfile(p):
            keep[name] = {"sha256": sha256_file(p), "size": os.path.getsize(p)}
    keep["_n_files_total"] = sum(len(fs) for _d, _dd, fs in os.walk(out_dir))
    return keep


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", required=True)
    ap.add_argument("--label", required=True)
    ap.add_argument("--arms", default="")
    ap.add_argument("--scratch", default="/workspace/issue80-scratch")
    ap.add_argument("--out", default="")
    ap.add_argument("--run-root", default="runs_a1")
    ap.add_argument("--env", default="")   # extra KEY=VAL,KEY=VAL
    a = ap.parse_args()

    idx = json.load(open(INDEX))
    names = a.arms.split(",") if a.arms else list(idx["arms"])
    out = a.out or os.path.join(REPO, "实验/issue80-p5-a1-production-fix/evidence", a.label)
    os.makedirs(out, exist_ok=True)
    run_root = os.path.join(a.scratch, a.run_root, a.label)
    os.makedirs(run_root, exist_ok=True)

    env = dict(os.environ)
    env.setdefault("OMP_NUM_THREADS", "1")
    for kv in filter(None, a.env.split(",")):
        k, _, v = kv.partition("=")
        env[k] = v

    results = []
    for arm in names:
        spec = idx["arms"][arm]
        paths = [f["path"] for f in spec["frames"] if "path" in f]
        out_dir = os.path.join(run_root, arm)
        shutil.rmtree(out_dir, ignore_errors=True)
        os.makedirs(out_dir, exist_ok=True)
        cfg = {"schema_version": "1", "hips_paths": paths,
               "output_dir": out_dir, "snr_path": "sparse_reconstruct"}
        cfg_path = os.path.join(out, arm + ".cfg.json")
        with open(cfg_path, "w") as fh:
            json.dump(cfg, fh, indent=2)
        t0 = time.time()
        p = subprocess.run([a.acsd, "mosaic", "--json", cfg_path, "-y"],
                           capture_output=True, text=True, env=env)
        dt = time.time() - t0
        with open(os.path.join(out, arm + ".stderr"), "w") as fh:
            fh.write(p.stderr)
        with open(os.path.join(out, arm + ".stdout"), "w") as fh:
            fh.write(p.stdout)
        rec = {"arm": arm, "exit": p.returncode, "wall_s": round(dt, 2),
               "output_dir": out_dir, "config": cfg,
               "acsd": a.acsd, "env_extra": a.env}
        for marker in ("not SPD", "adaptive node spacing exhausted",
                       "reduced normal matrix", "surface build FAILED",
                       "identifiability criterion RED", "UNIDENTIFIABLE",
                       "P5 disconnected graph", "P5 per-component"):
            rec["stderr_has::" + marker] = marker in p.stderr
        for line in p.stderr.splitlines():
            if "sky_plane" in line and ("FAILED" in line or "ok" in line):
                rec["sky_plane_line"] = line.strip()
                break
        for line in p.stderr.splitlines():
            if line.startswith("stage sample ok"):
                rec["sample_line"] = line.strip()
        rec["artifacts"] = small_artifacts(out_dir)
        results.append(rec)
        print("[run] %-30s exit=%s wall=%.1fs %s"
              % (arm, p.returncode, dt, rec.get("sky_plane_line", "")[:100]),
              flush=True)

    with open(os.path.join(out, "arms.json"), "w") as fh:
        json.dump(results, fh, indent=1, ensure_ascii=False)
    print("[run] wrote", os.path.join(out, "arms.json"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
