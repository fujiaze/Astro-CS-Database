#!/usr/bin/env python3
"""Issue #91 / E1-R：p3_op_resample worker sampler 打开失败传播 —— 真实 CLI 正反例。

场景（同一合法 HiPS 输入，仅 config / 注入 env 不同）:
  multi_normal         256×256 sub_block_px=128 → nblocks=4, nw=4，无注入 ⇒ 完整成功
  single_inject_k0     64×64   sub_block_px=64  → nblocks=1, nw=1，注入 worker 0 ⇒ 失败
  multi_inject_k1      256×256 注入 worker 1（主 sampler 与 worker 0 均成功）⇒ 失败
  multi_inject_default 256×256 注入值不带 :k（默认 k=1）⇒ 失败
  multi_inject_k3      256×256 注入最后一个 worker ⇒ 失败

--expect fixed  : 注入用例必须非零退出、无 FITS/bin、manifest=incomplete
--expect prefix : 修复前 A/B（注入管线 + 旧静默 return 语义）⇒ 注入用例**会** exit 0
                 并发布 FITS（缺陷证据；同时记录零填充带状区域）

用法:
  python3 run_e1r.py --acsd build/linux-dev/acsd --tag fixed --expect fixed
  python3 run_e1r.py --acsd /tmp/acsd-e1r-base-build/acsd --tag prefix --expect prefix
"""
from __future__ import annotations

import argparse
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

import numpy as np
from astropy.io import fits

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
# 复用 E1 的同一合法 HiPS 受控输入（Issue #91 证据单元）
HIPS = os.path.join(REPO, "实验", "issue91-stage3-bunit", "work", "variants", "legal_explicit")
WORK = os.path.join(HERE, "work")
EVID = os.path.join(HERE, "evidence")

SCENARIOS = [
    dict(case="multi_normal", w=256, h=256, sb=128, fault=None,
         expect="ok", require_bands=True),
    dict(case="single_inject_k0", w=64, h=64, sb=64, fault="resample_worker_open_fail:0",
         expect="fail", worker=0),
    dict(case="multi_inject_k1", w=256, h=256, sb=128, fault="resample_worker_open_fail:1",
         expect="fail", worker=1),
    dict(case="multi_inject_default", w=256, h=256, sb=128, fault="resample_worker_open_fail",
         expect="fail", worker=1),
    dict(case="multi_inject_k3", w=256, h=256, sb=128, fault="resample_worker_open_fail:3",
         expect="fail", worker=3),
]


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def config(case: str, w: int, h: int, sb: int, tag: str) -> dict:
    return {
        "schema_version": "1",
        "source": {"hips_dir": HIPS},
        "output_dir": os.path.join(WORK, "out", tag, case),
        "center": {"ra_deg": 10.0, "dec_deg": 0.0},
        "width_px": w,
        "height_px": h,
        "scale_deg_per_px": 0.01,
        "rotation_deg": 0.0,
        "crpix_px": [w / 2.0 + 0.5, h / 2.0 + 0.5],
        "output_mode": "surface_brightness",
        "sub_block_px": sb,
    }


def run_one(acsd: str, tag: str, sc: dict, cfg_dir: str, mode: str) -> dict:
    cfg = config(sc["case"], sc["w"], sc["h"], sc["sb"], tag)
    os.makedirs(cfg_dir, exist_ok=True)
    cfg_path = os.path.join(cfg_dir, sc["case"] + ".json")
    with open(cfg_path, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)
        f.write("\n")
    out = cfg["output_dir"]
    if os.path.isdir(out):
        shutil.rmtree(out)
    env = dict(os.environ)
    env.pop("ACSD_RT001_FAULT", None)
    if sc["fault"]:
        env["ACSD_RT001_FAULT"] = sc["fault"]
    log_path = os.path.join(EVID, tag, sc["case"] + ".log")
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    with open(log_path, "w", encoding="utf-8") as lf:
        proc = subprocess.run([acsd, "export", "--json", cfg_path, "-y"],
                              stdout=lf, stderr=subprocess.STDOUT, env=env)
    log = open(log_path, encoding="utf-8", errors="replace").read()
    rec = {
        "case": sc["case"], "fault": sc["fault"], "expect": sc["expect"],
        "config": cfg, "config_sha256": sha256_file(cfg_path),
        "log_path": log_path, "log_sha256": hashlib.sha256(log.encode()).hexdigest(),
        "exit_code": proc.returncode,
        "codes": sorted(set(re.findall(r"P3-RESAMPLE-WORKER-OPEN-[A-Z]+", log))),
        "input_hips_dir": HIPS,
        "input_properties_sha256": sha256_file(os.path.join(HIPS, "signal", "properties")),
    }
    # 产物与发布状态
    files = sorted(os.listdir(out)) if os.path.isdir(out) else []
    rec["artifacts"] = files
    rec["fits_present"] = "output_phase3.fits" in files
    rec["resampled_bin_present"] = "p3_resampled.bin" in files
    rec["resampled_json_present"] = "p3_resampled.json" in files
    rec["tmp_leftovers"] = [f for f in files if f.endswith(".tmp") or ".tmp" in f]
    rec["staging_dirs"] = [f for f in files if "staging" in f.lower()]
    rec["manifest"] = None
    for fn in files:
        if fn.startswith("acsd_run_") and fn.endswith(".json"):
            m = json.load(open(os.path.join(out, fn), encoding="utf-8"))
            rec["manifest"] = {"file": fn, "status": m.get("status"),
                               "summary": (m.get("summary") or "")[:400]}
    # 失败/成功的节点产物
    for key, fn in (("props", "p3_props.json"), ("wcs", "p3_wcs.json"),
                    ("resampled", "p3_resampled.json"), ("writer", "p3_writer.json"),
                    ("verify", "p3_verify.json")):
        p = os.path.join(out, fn)
        rec[key] = json.load(open(p, encoding="utf-8")) if os.path.isfile(p) else None
    if rec["resampled"] and isinstance(rec["resampled"], dict):
        rec["worker_bands"] = rec["resampled"].get("worker_bands")
    if rec["writer"] and isinstance(rec["writer"], dict):
        es = rec["writer"].get("export_stream") or {}
        rec["writer_sub_blocks"] = es.get("sub_blocks")
    if rec["fits_present"]:
        with fits.open(os.path.join(out, "output_phase3.fits")) as hd:
            d = np.asarray(hd[0].data)
            zero_rows = sorted({int(y) for y in np.nonzero((d == 0).all(axis=1))[0]}) \
                if d.ndim == 2 else []
            rec["fits"] = {
                "shape": list(d.shape),
                "bitpix": hd[0].header.get("BITPIX"),
                "bunit": hd[0].header.get("BUNIT"),
                "finite_px": int(np.isfinite(d).sum()),
                "nan_px": int(np.isnan(d).sum()),
                "exact_zero_px": int((d == 0).sum()),
                "all_zero_rows": zero_rows,
                "all_zero_row_count": len(zero_rows),
                "data_sha256": hashlib.sha256(d.tobytes()).hexdigest(),
                "sum": float(np.nansum(d.astype(np.float64))),
            }
    # 判定
    if sc["expect"] == "ok":
        wb = rec.get("worker_bands") or {}
        bands_ok = True
        if wb:
            bands_ok = (wb.get("planned") == wb.get("executed") == wb.get("sub_blocks") == 4)
        elif mode == "fixed":
            bands_ok = False   # 修复后必须把 work unit 计数随产物落盘（可独立核对）
        ok = (rec["exit_code"] == 0 and rec["fits_present"]
              and (rec["manifest"] or {}).get("status") == "complete"
              and bands_ok and rec.get("writer_sub_blocks") == 4
              and (rec.get("fits") or {}).get("nan_px") == 0
              and (rec.get("fits") or {}).get("exact_zero_px") == 0)
        rec["check"] = f"bands={wb} writer_sub_blocks={rec.get('writer_sub_blocks')}"
    else:
        if mode == "prefix":
            # 修复前 A/B：缺陷本体 = 注入后仍 exit 0 且发布 FITS（未传播失败）
            ok = (rec["exit_code"] == 0 and rec["fits_present"]
                  and (rec["manifest"] or {}).get("status") == "complete")
            rec["defect_reproduced"] = ok
        else:
            ok = (rec["exit_code"] != 0 and not rec["fits_present"]
                  and not rec["resampled_bin_present"] and not rec["resampled_json_present"]
                  and (rec["manifest"] or {}).get("status") == "incomplete"
                  and not rec["tmp_leftovers"] and not rec["staging_dirs"]
                  and rec["codes"] == ["P3-RESAMPLE-WORKER-OPEN-INPUT"]
                  and f"worker {sc['worker']}" in (rec["manifest"] or {}).get("summary", ""))
    rec["verdict"] = "PASS" if ok else "FAIL"
    return rec


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", required=True)
    ap.add_argument("--tag", required=True)
    ap.add_argument("--expect", choices=["fixed", "prefix"], required=True)
    args = ap.parse_args()
    os.makedirs(os.path.join(EVID, args.tag), exist_ok=True)
    cfg_dir = os.path.join(WORK, "configs", args.tag)

    results = []
    for sc in SCENARIOS:
        rec = run_one(args.acsd, args.tag, sc, cfg_dir, args.expect)
        results.append(rec)
        print(f"[{args.tag}] {sc['case']:22s} exit={rec['exit_code']} "
              f"fits={'Y' if rec['fits_present'] else 'n'} "
              f"bin={'Y' if rec['resampled_bin_present'] else 'n'} "
              f"manifest={(rec['manifest'] or {}).get('status')} "
              f"codes={','.join(rec['codes']) or '-'} -> {rec['verdict']}")

    n_fail = sum(1 for r in results if r["verdict"] != "PASS")
    out = {"tag": args.tag, "expect": args.expect, "acsd": os.path.abspath(args.acsd),
           "acsd_sha256": sha256_file(args.acsd), "n_fail": n_fail, "results": results}
    path = os.path.join(EVID, args.tag, "results.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"[{args.tag}] wrote {path} (fail={n_fail}/{len(results)})")
    return 1 if n_fail else 0


if __name__ == "__main__":
    sys.exit(main())
