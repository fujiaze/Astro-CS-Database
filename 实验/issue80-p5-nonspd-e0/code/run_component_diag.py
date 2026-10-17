#!/usr/bin/env python3
"""#80 P5 E0 — 生产只读分量/规范诊断（不改源码，仅用现成诊断环境变量）。

  ACSD_SKY_COMPONENT_DUMP=<path>  → 逐分量 n_samples/n_cells/n_nodes/rank/kappa/identifiable
  ACSD_SKY_P5_FAULT=global_anchor  → 负例注入：退回全局单一参考帧

并对每个臂收集 p2_samples.json 的 frame_ids / 观测缩放统计，作为矩阵 oracle 的输入。
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


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", default=os.path.join(REPO, "build/linux-sci-p5/acsd"))
    ap.add_argument("--index", default=os.path.join(OUT_DEFAULT, "input_index.json"))
    ap.add_argument("--scratch", default="/workspace/issue80-scratch")
    ap.add_argument("--out", default=OUT_DEFAULT)
    ap.add_argument("--arms", default="abc_real,abc_label_swap_all_red,ab_2halpha,"
                                   "def_samefilter_red_3frame")
    ap.add_argument("--fault", default="none",
                    help="none | global_anchor")
    a = ap.parse_args()
    index = json.load(open(a.index))
    tag = "fault_global_anchor" if a.fault == "global_anchor" else "normal"
    base = os.path.join(a.scratch, "runs_diag", tag)
    os.makedirs(base, exist_ok=True)
    logs = os.path.join(a.out, "diag_logs")
    os.makedirs(logs, exist_ok=True)
    env = dict(os.environ)
    env["OMP_NUM_THREADS"] = "1"
    if a.fault != "none":
        env["ACSD_SKY_P5_FAULT"] = a.fault
    out_rec = []
    for arm in a.arms.split(","):
        spec = index["arms"][arm]
        paths = [f["path"] for f in spec["frames"] if "path" in f]
        out_dir = os.path.join(base, arm)
        shutil.rmtree(out_dir, ignore_errors=True)
        os.makedirs(out_dir, exist_ok=True)
        dump = os.path.join(logs, f"{arm}.{tag}.component_dump.json")
        if os.path.exists(dump):
            os.remove(dump)
        env["ACSD_SKY_COMPONENT_DUMP"] = dump
        cfg = {"schema_version": "1", "hips_paths": paths,
               "output_dir": out_dir, "snr_path": "sparse_reconstruct"}
        cfg_path = os.path.join(logs, f"{arm}.{tag}.cfg.json")
        with open(cfg_path, "w") as fh:
            json.dump(cfg, fh, indent=2)
        t0 = time.time()
        p = subprocess.run([a.acsd, "mosaic", "--json", cfg_path, "-y"],
                           capture_output=True, text=True, env=env)
        dt = time.time() - t0
        with open(os.path.join(logs, f"{arm}.{tag}.stderr"), "w") as fh:
            fh.write(p.stderr)
        sky_line = ""
        for line in p.stderr.splitlines():
            if "sky_plane" in line:
                sky_line = line.strip()
                break
        rec = {"arm": arm, "fault": a.fault, "exit": p.returncode,
               "wall_s": round(dt, 2), "sky_plane_line": sky_line,
               "component_dump_written": os.path.isfile(dump)}
        if rec["component_dump_written"]:
            with open(dump) as fh:
                rec["component_dump"] = json.load(fh)
        sp = os.path.join(out_dir, "p2_samples.json")
        if os.path.isfile(sp):
            d = json.load(open(sp))
            rec["frame_ids"] = d["frame_ids"]
            rec["n_obs"] = d["n_obs"]
            import statistics
            per = {}
            for o in d["observations"]:
                per.setdefault(o["frame_id"], []).append(o)
            for fid, rows in per.items():
                cv = [r["control_variance"] for r in rows]
                v = [r["value"] for r in rows]
                per[fid] = {
                    "n": len(rows),
                    "control_variance_min": min(cv), "control_variance_max": max(cv),
                    "control_variance_median": statistics.median(cv),
                    "value_min": min(v), "value_max": max(v),
                    "value_median": statistics.median(v),
                    "weight_median": statistics.median(1.0 / x for x in cv),
                }
            rec["per_frame"] = per
        out_rec.append(rec)
        print(f"[diag:{tag}] {arm:28s} exit={p.returncode} dump={rec['component_dump_written']} "
              f"{sky_line[:80]}", flush=True)
    outp = os.path.join(a.out, f"diag_{tag}.json")
    with open(outp, "w") as fh:
        json.dump(out_rec, fh, indent=1, ensure_ascii=False)
    print("wrote", outp)
    return 0


if __name__ == "__main__":
    sys.exit(main())
