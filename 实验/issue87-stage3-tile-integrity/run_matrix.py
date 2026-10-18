#!/usr/bin/env python3
"""Issue #87 / E2：Stage3 tile 完整性故障矩阵 —— 真实 CLI 执行器。

同一受控输入矩阵分别用**修复前基线**与**修复后**二进制跑 `acsd export`：
  --expect fixed  : 声明内缺失/损坏/校验失败/IO/身份不足必须非零退出，
                    不产 FITS / p3_resampled.bin / p3_resampled.json / complete
                    manifest；合法稀疏/真实未覆盖仍成功且 NaN 语义不变。
  --expect prefix : 修复前 A/B 证据（同一矩阵应 exit 0 并发布 complete FITS，
                    证明缺陷真实存在；合法用例数值必须与修复后逐位一致）。

用法:
  python3 实验/issue87-stage3-tile-integrity/run_matrix.py \
      --acsd build/linux-dev/acsd --tag fixed --expect fixed
  python3 实验/issue87-stage3-tile-integrity/run_matrix.py \
      --acsd /tmp/acsd-e87-base-build/acsd --tag prefix --expect prefix
"""
from __future__ import annotations

import argparse
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
WORK = os.path.join(HERE, "work")
FIXTURES = os.path.join(WORK, "fixtures.json")
EVID = os.path.join(HERE, "evidence")


def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def read_fits(path: str) -> dict:
    with fits.open(path) as hdul:
        hdu = hdul[0]
        data = np.asarray(hdu.data)
        rec = {
            "nhdu": len(hdul),
            "primary": hdul[0].name,
            "shape": list(data.shape),
            "bitpix": int(hdu.header.get("BITPIX", 0)),
            "bunit": hdu.header.get("BUNIT"),
            "naxis": int(hdu.header.get("NAXIS", 0)),
            "ctype1": hdu.header.get("CTYPE1"),
            "ctype2": hdu.header.get("CTYPE2"),
            "crval1": hdu.header.get("CRVAL1"),
            "crval2": hdu.header.get("CRVAL2"),
            "cd1_1": hdu.header.get("CD1_1"),
            "cd2_2": hdu.header.get("CD2_2"),
            "finite_px": int(np.isfinite(data).sum()),
            "nan_px": int(np.isnan(data).sum()),
            "exact_zero_px": int((data == 0).sum()),
            "sum": float(np.nansum(data.astype(np.float64))),
            "data_sha256": sha256_bytes(np.ascontiguousarray(data).tobytes()),
        }
        return rec


def find_manifest(out_dir: str) -> dict | None:
    if not os.path.isdir(out_dir):
        return None
    for fn in sorted(os.listdir(out_dir)):
        if fn.startswith("acsd_run_") and fn.endswith(".json"):
            p = os.path.join(out_dir, fn)
            try:
                m = json.load(open(p, encoding="utf-8"))
            except Exception:
                continue
            return {"file": fn, "status": m.get("status"),
                    "summary": (m.get("summary") or "")[:2000],
                    "error": (m.get("error") or {}).get("message")}
    return None


def run_one(acsd: str, tag: str, sc: dict, repeat_idx: int,
            expect_mode: str = "fixed") -> dict:
    case = sc["case"] if repeat_idx == 0 else f"{sc['case']}_r{repeat_idx}"
    cfg_dir = os.path.join(WORK, "configs", tag)
    os.makedirs(cfg_dir, exist_ok=True)
    out_dir = os.path.join(WORK, "out", tag, case)
    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir)
    cfg = {
        "schema_version": "1",
        "source": {"hips_dir": sc["hips_dir"]},
        "output_dir": out_dir,
        **sc["config"],
    }
    cfg_path = os.path.join(cfg_dir, case + ".json")
    with open(cfg_path, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)
        f.write("\n")

    env = dict(os.environ)
    env.pop("ACSD_RT001_FAULT", None)
    log_dir = os.path.join(EVID, tag)
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, case + ".log")
    with open(log_path, "w", encoding="utf-8") as lf:
        proc = subprocess.run([acsd, "export", "--json", cfg_path, "-y"],
                              stdout=lf, stderr=subprocess.STDOUT, env=env)
    log = open(log_path, encoding="utf-8", errors="replace").read()

    files = sorted(os.listdir(out_dir)) if os.path.isdir(out_dir) else []
    rec = {
        "case": sc["case"],
        "repeat": repeat_idx,
        "expect": sc["expect"],
        "note": sc.get("note", ""),
        "config": cfg,
        "config_sha256": sha256_file(cfg_path),
        "log_path": log_path,
        "log_sha256": sha256_bytes(log.encode()),
        "exit_code": proc.returncode,
        "artifacts": files,
        "fits_present": "output_phase3.fits" in files,
        "resampled_bin_present": "p3_resampled.bin" in files,
        "resampled_json_present": "p3_resampled.json" in files,
        "writer_json_present": "p3_writer.json" in files,
        "verify_json_present": "p3_verify.json" in files,
        "tile_failures_present": "p3_tile_failures.json" in files,
        "tmp_leftovers": [f for f in files if ".tmp" in f],
        "staging_dirs": [f for f in files if "staging" in f.lower()],
        "manifest": find_manifest(out_dir),
        "input_hashes": sc.get("input_hashes"),
    }
    # 机器码（日志 + manifest + 失败 JSON）
    rec["codes"] = sorted(set(re.findall(r"P3-TILE-[A-Z-]+", log)))
    if rec["manifest"]:
        rec["codes"] = sorted(set(rec["codes"]) |
                              set(re.findall(r"P3-TILE-[A-Z-]+",
                                             rec["manifest"].get("summary") or "")))
    for key, fn in (("resampled", "p3_resampled.json"),
                    ("writer", "p3_writer.json"),
                    ("verify", "p3_verify.json"),
                    ("tile_failures", "p3_tile_failures.json")):
        p = os.path.join(out_dir, fn)
        rec[key] = json.load(open(p, encoding="utf-8")) if os.path.isfile(p) else None

    if rec["fits_present"]:
        rec["fits"] = read_fits(os.path.join(out_dir, "output_phase3.fits"))
    if rec["tile_failures"]:
        tf = rec["tile_failures"]
        rec["failed_tiles"] = [int(x["tile_ipix"]) for x in tf.get("tile_errors", [])]
        rec["failed_codes"] = [x["machine_code"] for x in tf.get("tile_errors", [])]
        rec["failed_by_code"] = tf.get("tile_error_by_code")
        rec["tile_cache"] = tf.get("tile_cache")
        # 排序/去重合同
        rec["failed_tiles_sorted"] = rec["failed_tiles"] == sorted(rec["failed_tiles"])
        rec["failed_tiles_unique"] = len(rec["failed_tiles"]) == len(set(rec["failed_tiles"]))
        rec["tile_error_count"] = int(tf.get("tile_error_count", 0))
        rec["workers_seen"] = sorted(
            {(int(x["worker_min"]), int(x["worker_max"]), int(x["worker_count"]))
             for x in tf.get("tile_errors", [])})
    if rec["resampled"]:
        rec["tile_integrity"] = rec["resampled"].get("tile_integrity")
        rec["tile_cache"] = rec["resampled"].get("tile_cache")
        rec["worker_bands"] = rec["resampled"].get("worker_bands")
    if rec["verify"]:
        rec["coverage_stats"] = rec["verify"].get("coverage_stats")
        rec["reopen_ok"] = rec["verify"].get("reopen_ok")

    # ── 判定 ────────────────────────────────────────────────────────────────
    ok, why = judge(sc, rec, expect_mode)
    rec["verdict"] = "PASS" if ok else "FAIL"
    rec["verdict_reason"] = why
    return rec


def judge(sc: dict, rec: dict, expect_mode: str) -> tuple[bool, str]:
    oracle = sc.get("oracle") or {}
    if sc["expect"] == "fail":
        if expect_mode == "prefix":
            # 修复前 A/B：缺陷 = 仍 exit 0 并发布 complete FITS
            ok = (rec["exit_code"] == 0 and rec["fits_present"]
                  and (rec["manifest"] or {}).get("status") == "complete")
            return ok, ("defect reproduced: exit0+FITS+complete" if ok
                        else "prefix did NOT reproduce defect")
        # 修复后：必须 fail-closed
        reasons = []
        if rec["exit_code"] == 0:
            reasons.append("exit_code==0")
        if rec["fits_present"]:
            reasons.append("output_phase3.fits present")
        if rec["resampled_bin_present"]:
            reasons.append("p3_resampled.bin present")
        if rec["resampled_json_present"]:
            reasons.append("p3_resampled.json present")
        if (rec["manifest"] or {}).get("status") != "incomplete":
            reasons.append("manifest != incomplete")
        if not rec["tile_failures_present"]:
            reasons.append("p3_tile_failures.json missing")
        if rec["tmp_leftovers"]:
            reasons.append(f"tmp leftovers {rec['tmp_leftovers']}")
        if rec["staging_dirs"]:
            reasons.append(f"staging dirs {rec['staging_dirs']}")
        # 错误码/ tile 集合
        exp_code = sc.get("expect_code")
        if exp_code and exp_code not in (rec.get("codes") or []):
            reasons.append(f"code {exp_code} not in {rec.get('codes')}")
        exp_tiles = sc.get("expect_tiles")
        if exp_tiles is not None:
            got = set(rec.get("failed_tiles") or [])
            if set(exp_tiles) != got:
                reasons.append(f"failed tiles {sorted(got)} != {sorted(exp_tiles)}")
            if not rec.get("failed_tiles_sorted", False):
                reasons.append("failed tiles not sorted")
            if not rec.get("failed_tiles_unique", False):
                reasons.append("failed tiles not unique")
        exp_codes = sc.get("expect_codes")
        if exp_codes:
            got_codes = set(rec.get("failed_codes") or [])
            if set(exp_codes) != got_codes:
                reasons.append(f"failed codes {sorted(got_codes)} != {sorted(exp_codes)}")
        # 退出码域: IO_ERROR→7; 其余输入完整性→3
        if exp_code == "P3-TILE-IO-ERROR":
            if rec["exit_code"] != 7:
                reasons.append(f"IO exit {rec['exit_code']} != 7")
        else:
            if rec["exit_code"] != 3:
                reasons.append(f"input exit {rec['exit_code']} != 3")
        return (not reasons), ("ok" if not reasons else "; ".join(reasons))

    # 成功用例
    reasons = []
    if rec["exit_code"] != 0:
        reasons.append(f"exit_code={rec['exit_code']}")
    if not rec["fits_present"]:
        reasons.append("output_phase3.fits missing")
    if (rec["manifest"] or {}).get("status") != "complete":
        reasons.append("manifest != complete")
    f = rec.get("fits") or {}
    if f.get("nhdu") != 1:
        reasons.append(f"nhdu={f.get('nhdu')} != 1 (单 PRIMARY signal HDU)")
    if f.get("naxis") != 2:
        reasons.append(f"NAXIS={f.get('naxis')} != 2")
    if f.get("bitpix") != -32:
        reasons.append(f"BITPIX={f.get('bitpix')} != -32")
    if f.get("bunit") != "ADU/sr":
        reasons.append(f"BUNIT={f.get('bunit')} != ADU/sr")
    want_shape = [sc["config"]["height_px"], sc["config"]["width_px"]]
    if f.get("shape") != want_shape:
        reasons.append(f"shape={f.get('shape')} != {want_shape}")
    if f.get("ctype1") != "RA---TAN" or f.get("ctype2") != "DEC--TAN":
        reasons.append(f"CTYPE={f.get('ctype1')},{f.get('ctype2')}")

    kind = oracle.get("kind")
    if kind == "constant":
        expected = float(oracle["value"])
        finite = int(f.get("finite_px", 0))
        if finite != int(f.get("shape")[0]) * int(f.get("shape")[1]):
            reasons.append(f"finite_px={finite} < full frame")
        if f.get("nan_px"):
            reasons.append(f"nan_px={f['nan_px']} != 0")
        # 精确常量（nearest 单 tile；float32 常量可精确表示）
        p = os.path.join(rec["config"]["output_dir"], "output_phase3.fits")
        if os.path.isfile(p):
            with fits.open(p) as hdul:
                d = np.asarray(hdul[0].data)
                bad = int((d[np.isfinite(d)] != np.float32(expected)).sum())
                if bad:
                    reasons.append(f"{bad} pixels != constant {expected}")
    elif kind == "nan_all":
        if f.get("nan_px") != int(f.get("shape")[0]) * int(f.get("shape")[1]):
            reasons.append(f"nan_px={f.get('nan_px')} != full frame")
        if f.get("finite_px"):
            reasons.append(f"finite_px={f['finite_px']} != 0 (合法无覆盖应为 NaN)")
        cov = rec.get("coverage_stats") or {}
        if cov.get("covered_px") not in (0, None):
            reasons.append(f"coverage.covered_px={cov.get('covered_px')} != 0")
    elif kind == "real":
        # 只要成功 + 单 HDU + BUNIT 正确；数值逐位由 compare 脚本对 A/B 判
        pass
    elif kind == "real_partial":
        if not (int(f.get("nan_px", 0)) > 0 and int(f.get("finite_px", 0)) > 0):
            reasons.append(
                f"partial coverage expected both NaN and finite, got "
                f"nan={f.get('nan_px')} finite={f.get('finite_px')}")
    return (not reasons), ("ok" if not reasons else "; ".join(reasons))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", required=True)
    ap.add_argument("--tag", required=True)
    ap.add_argument("--expect", choices=["fixed", "prefix"], required=True)
    args = ap.parse_args()
    os.makedirs(os.path.join(EVID, args.tag), exist_ok=True)

    doc = json.load(open(FIXTURES, encoding="utf-8"))
    results = []
    for sc in doc["scenarios"]:
        reps = sc.get("repeat", 1)
        for r in range(reps):
            rec = run_one(args.acsd, args.tag, sc, r, args.expect)
            results.append(rec)
            print(f"[{args.tag}] {rec['case']:34s} r{r} exit={rec['exit_code']} "
                  f"fits={'Y' if rec['fits_present'] else 'n'} "
                  f"manifest={(rec['manifest'] or {}).get('status')} "
                  f"codes={','.join(rec.get('codes') or []) or '-'} "
                  f"tiles={rec.get('failed_tiles')} -> {rec['verdict']}"
                  + ("" if rec["verdict"] == "PASS" else f" :: {rec['verdict_reason']}"))

    summary = {
        "tag": args.tag,
        "expect": args.expect,
        "acsd": os.path.abspath(args.acsd),
        "acsd_sha256": sha256_file(args.acsd),
        "n_cases": len(results),
        "n_fail": sum(1 for r in results if r["verdict"] != "PASS"),
        "cases": sorted({r["case"] for r in results}),
    }
    out = {"schema": "acsd.issue87.matrix/v1", **summary, "results": results}
    path = os.path.join(EVID, args.tag, "results.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"wrote {path}: n={summary['n_cases']} fail={summary['n_fail']}")
    return 0 if summary["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
