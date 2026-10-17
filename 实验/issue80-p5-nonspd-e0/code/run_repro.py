#!/usr/bin/env python3
"""#80 P5 E0 — 在冻结输入上重跑生产 mosaic CLI，逐臂记录 exit/stderr/产物哈希。

只运行 `acsd mosaic --json cfg -y`；不修改任何生产源码/算法。
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
OUT_DEFAULT = os.path.join(REPO, "实验/issue80-p5-nonspd-e0/evidence")


def sha256_file(path: str, chunk: int = 1 << 20) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for c in iter(lambda: fh.read(chunk), b""):
            h.update(c)
    return h.hexdigest()


def small_artifacts(out_dir: str) -> dict:
    """只记录小产物哈希（矩阵/元数据层），大 HiPS 只记计数。"""
    keep = {}
    for name in ("p2_final.json", "p2_coverage.json", "p2_sky_plane.bin",
                 "p2_sky_plane.json", "acsd_run_manifest.json"):
        p = os.path.join(out_dir, name)
        if os.path.isfile(p):
            keep[name] = {"sha256": sha256_file(p), "size": os.path.getsize(p)}
    for dp, _dd, fs in os.walk(out_dir):
        for f in sorted(fs):
            if f in ("p2_samples.json", "p2_coverage.json"):
                p = os.path.join(dp, f)
                keep[os.path.relpath(p, out_dir)] = {
                    "sha256": sha256_file(p), "size": os.path.getsize(p)}
    keep["_n_files_total"] = sum(len(fs) for _d, _dd, fs in os.walk(out_dir))
    return keep


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", default=os.path.join(REPO, "build/linux-sci-p5/acsd"))
    ap.add_argument("--index", default=os.path.join(OUT_DEFAULT, "input_index.json"))
    ap.add_argument("--scratch", default="/workspace/issue80-scratch")
    ap.add_argument("--out", default=OUT_DEFAULT)
    ap.add_argument("--arms", default="")
    ap.add_argument("--snr-path", default="sparse_reconstruct")
    a = ap.parse_args()

    index = json.load(open(a.index))
    names = a.arms.split(",") if a.arms else list(index["arms"])
    runs = os.path.join(a.scratch, "runs")
    os.makedirs(runs, exist_ok=True)
    logs = os.path.join(a.out, "cli_logs")
    os.makedirs(logs, exist_ok=True)
    env = dict(os.environ)
    env.setdefault("OMP_NUM_THREADS", "1")  # 数值可复现：单线程装配

    results = []
    for arm in names:
        spec = index["arms"][arm]
        paths = [f["path"] for f in spec["frames"] if "path" in f]
        out_dir = os.path.join(runs, arm)
        shutil.rmtree(out_dir, ignore_errors=True)
        os.makedirs(out_dir, exist_ok=True)
        cfg = {"schema_version": "1", "hips_paths": paths,
               "output_dir": out_dir, "snr_path": a.snr_path}
        cfg_path = os.path.join(logs, arm + ".cfg.json")
        with open(cfg_path, "w") as fh:
            json.dump(cfg, fh, indent=2)
        t0 = time.time()
        p = subprocess.run([a.acsd, "mosaic", "--json", cfg_path, "-y"],
                           capture_output=True, text=True, env=env)
        dt = time.time() - t0
        with open(os.path.join(logs, arm + ".stderr"), "w") as fh:
            fh.write(p.stderr)
        with open(os.path.join(logs, arm + ".stdout"), "w") as fh:
            fh.write(p.stdout)
        rec = {
            "arm": arm,
            "frames": [{"key": f.get("key"), "label": f.get("label"),
                        "dirname": os.path.basename(f.get("path", ""))} for f in spec["frames"]],
            "exit": p.returncode,
            "wall_s": round(dt, 2),
            "output_dir": out_dir,
            "config": cfg,
        }
        # 关键失败字符串
        sserr = p.stderr
        for marker in ("not SPD", "adaptive node spacing exhausted",
                       "reduced normal matrix", "surface build FAILED"):
            rec[f"stderr_has::{marker}"] = marker in sserr
        for line in sserr.splitlines():
            if "sky_plane" in line and ("FAILED" in line or "ok" in line):
                rec["sky_plane_line"] = line.strip()
                break
        for line in sserr.splitlines():
            if line.startswith("stage sample ok"):
                rec["sample_line"] = line.strip()
        rec["artifacts"] = small_artifacts(out_dir)
        results.append(rec)
        print(f"[run] {arm:28s} exit={p.returncode} wall={dt:.1f}s "
              f"{rec.get('sky_plane_line', '')[:90]}", flush=True)

    outp = os.path.join(a.out, "replay_cases.json")
    with open(outp, "w") as fh:
        json.dump(results, fh, indent=1, ensure_ascii=False)
    print(f"[run] wrote {outp}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
