#!/usr/bin/env python3
"""Issue #94 / P0-C2：writer/verify 的 reopen_ok/canonical_match fail-closed 与发布事务
—— 真实 CLI A/B 执行器（fix + hook-only prefix 两个二进制）。

故障钩子（仅测试；生产默认不可达）：
  ACSD_P3_VERIFY_FAULT=pixel_change|wcs_change        writer 写后/自身重开前篡改
  ACSD_P3_VERIFY_FAULT=canonical_change               writer 写 p3_writer.json 后篡改
  ACSD_P3_VERIFY_FAULT=verify_pixel_change|verify_wcs_change  verify 自身重开前篡改
  ACSD_P3_VERIFY_FAULT=identity_drift                 verify 读 COVSTAT 前篡改

判据（fixed）：任何 reopen_ok=0 或 canonical_match=false 最终不得 exit 0 / 不得留
output_phase3.fits / 不得留 p3_writer.json / manifest 必须 incomplete / 无 tmp 残留；
prefix（仅含钩子、不含 fail-closed 消费）必须复现"exit 0 + FITS + complete"伪成功。

用法:
  python3 实验/issue94-verify-failclosed/run_verify_failclosed.py \
      --fixed build/linux-dev/acsd --prefix /workspace/.e94-prefix-build/acsd \
      --tag fixed
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
EVID = os.path.join(HERE, "evidence")

sys.path.insert(0, os.path.join(REPO, "实验", "issue87-stage3-tile-integrity"))
from make_fixtures import build_legal_full, tile_center  # noqa: E402

CASES = [
    ("ok", None),
    ("pixel_change", "pixel_change"),
    ("wcs_change", "wcs_change"),
    ("canonical_change", "canonical_change"),
    ("verify_pixel_change", "verify_pixel_change"),
    ("verify_wcs_change", "verify_wcs_change"),
    ("identity_drift", "identity_drift"),
]


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def astropy_truth(path: str) -> dict:
    import numpy as np
    from astropy.io import fits
    with fits.open(path) as hdul:
        hdu = hdul[0]
        d = np.asarray(hdu.data)
        return {
            "nhdu": len(hdul),
            "primary": hdul[0].name,
            "shape": list(d.shape),
            "bitpix": int(hdu.header.get("BITPIX", 0)),
            "bunit": hdu.header.get("BUNIT"),
            "ctype1": hdu.header.get("CTYPE1"),
            "ctype2": hdu.header.get("CTYPE2"),
            "crval1": hdu.header.get("CRVAL1"),
            "crval2": hdu.header.get("CRVAL2"),
            "covstat": hdu.header.get("COVSTAT"),
            "covverif": hdu.header.get("COVVERIF"),
            "finite_px": int(np.isfinite(d).sum()),
            "nan_px": int(np.isnan(d).sum()),
            "data_sha256": hashlib.sha256(np.ascontiguousarray(d).tobytes()).hexdigest(),
        }


def find_manifest(out_dir: str) -> dict | None:
    if not os.path.isdir(out_dir):
        return None
    cands = [f for f in os.listdir(out_dir)
             if f.startswith("acsd_run_") and f.endswith(".json")]
    if not cands:
        return None
    # 同一 output_dir 多次 run ⇒ 消费方按最新 mtime 选择（E2 报告登记的约定）。
    newest = max(cands, key=lambda f: os.path.getmtime(os.path.join(out_dir, f)))
    m = json.load(open(os.path.join(out_dir, newest), encoding="utf-8"))
    return {"file": newest, "status": m.get("status"),
            "error": (m.get("error") or {}).get("message"),
            "n_manifests": len(cands)}


def run_one(acsd: str, tag: str, name: str, fault: str | None,
            hips: str, ra: float, dec: float, keep: bool = False) -> dict:
    out_dir = os.path.join(WORK, "out", tag, name)
    if os.path.isdir(out_dir) and not keep:
        shutil.rmtree(out_dir)
    cfg = {
        "schema_version": "1",
        "source": {"hips_dir": hips},
        "output_dir": out_dir,
        "center": {"ra_deg": ra, "dec_deg": dec},
        "width_px": 64, "height_px": 64, "scale_deg_per_px": 0.01,
        "sampler": "nearest", "longitude_parity": "east_left",
        "output_mode": "surface_brightness", "sub_block_px": 64,
    }
    cfg_dir = os.path.join(WORK, "configs", tag)
    os.makedirs(cfg_dir, exist_ok=True)
    cfg_path = os.path.join(cfg_dir, name + ".json")
    json.dump(cfg, open(cfg_path, "w", encoding="utf-8"), indent=2)
    env = dict(os.environ)
    env.pop("ACSD_P3_VERIFY_FAULT", None)
    if fault:
        env["ACSD_P3_VERIFY_FAULT"] = fault
    log_dir = os.path.join(EVID, tag)
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, name + ".log")
    with open(log_path, "w", encoding="utf-8") as lf:
        proc = subprocess.run([acsd, "export", "--json", cfg_path, "-y"],
                              stdout=lf, stderr=subprocess.STDOUT, env=env)
    files = sorted(os.listdir(out_dir)) if os.path.isdir(out_dir) else []
    rec = {
        "case": name, "fault": fault, "tag": tag, "exit_code": proc.returncode,
        "artifacts": files,
        "fits_present": "output_phase3.fits" in files,
        "writer_present": "p3_writer.json" in files,
        "verify_present": "p3_verify.json" in files,
        "resampled_present": "p3_resampled.json" in files,
        "tmp_leftovers": [f for f in files if ".tmp" in f],
        "manifest": find_manifest(out_dir),
        "log_path": log_path,
    }
    fits_path = os.path.join(out_dir, "output_phase3.fits")
    if rec["fits_present"]:
        rec["fits"] = astropy_truth(fits_path)
    for key, fn in (("writer", "p3_writer.json"), ("verify", "p3_verify.json")):
        p = os.path.join(out_dir, fn)
        rec[key] = json.load(open(p, encoding="utf-8")) if os.path.isfile(p) else None
    if rec["verify"]:
        rec["reopen_ok"] = rec["verify"].get("reopen_ok")
        rec["canonical_match"] = rec["verify"].get("canonical_match")
        rec["verify_ok"] = rec["verify"].get("verify_ok")
        rec["semantic_code"] = rec["verify"].get("semantic_code")
    return rec


def judge(rec: dict, expect: str) -> tuple[bool, str]:
    reasons = []
    if expect == "ok":
        if rec["exit_code"] != 0:
            reasons.append(f"exit={rec['exit_code']}")
        if not rec["fits_present"]:
            reasons.append("FITS missing")
        if (rec["manifest"] or {}).get("status") != "complete":
            reasons.append("manifest != complete")
        if not rec["writer_present"] or not rec["verify_present"]:
            reasons.append("writer/verify artifact missing")
        if (rec.get("verify") or {}).get("verify_ok") is not True:
            reasons.append("verify_ok != true")
        if rec["tmp_leftovers"]:
            reasons.append(f"tmp {rec['tmp_leftovers']}")
        f = rec.get("fits") or {}
        if f.get("nhdu") != 1 or f.get("bitpix") != -32 or f.get("bunit") != "ADU/sr":
            reasons.append(f"product shape {f.get('nhdu')}/{f.get('bitpix')}/{f.get('bunit')}")
    else:
        # fixed：必须 fail-closed
        if rec["exit_code"] == 0:
            reasons.append("exit==0")
        if rec["fits_present"]:
            reasons.append("FITS present")
        if rec["writer_present"]:
            reasons.append("p3_writer.json present")
        if (rec["manifest"] or {}).get("status") != "incomplete":
            reasons.append("manifest != incomplete")
        if rec["tmp_leftovers"]:
            reasons.append(f"tmp {rec['tmp_leftovers']}")
        if rec.get("verify") and rec["verify"].get("verify_ok") is not False:
            reasons.append("verify.json verify_ok != false")
    return (not reasons), ("ok" if not reasons else "; ".join(reasons))


def judge_prefix_defect(rec: dict) -> tuple[bool, str]:
    """修复前（仅钩子、无 fail-closed 消费）：必须复现 exit0 + FITS + complete 伪成功。"""
    reasons = []
    if rec["exit_code"] != 0:
        reasons.append(f"exit={rec['exit_code']} != 0")
    if not rec["fits_present"]:
        reasons.append("FITS missing")
    if (rec["manifest"] or {}).get("status") != "complete":
        reasons.append("manifest != complete")
    if not rec["writer_present"]:
        reasons.append("p3_writer.json missing")
    return (not reasons), ("ok" if not reasons else "; ".join(reasons))


def judge_prefix_identity(rec: dict) -> tuple[bool, str]:
    """修复前的 E2-R 身份检查已 fail（exit!=0）但**不撤销**已发布产物 ⇒
    失败 run 仍留下可被 consumer 选为成功的 FITS/writer 声明（Issue #94 缺陷面）。"""
    reasons = []
    if rec["exit_code"] == 0:
        reasons.append("exit==0 (identity check should fail pre-fix)")
    if not rec["fits_present"]:
        reasons.append("FITS missing (pre-fix leaves it)")
    if not rec["writer_present"]:
        reasons.append("p3_writer.json missing (pre-fix leaves it)")
    return (not reasons), ("ok" if not reasons else "; ".join(reasons))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--fixed", required=True)
    ap.add_argument("--prefix", required=True)
    ap.add_argument("--tag", default="fixed")
    args = ap.parse_args()
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(EVID, exist_ok=True)
    build_legal_full()
    hips = os.path.join(REPO, "实验", "issue87-stage3-tile-integrity", "work",
                        "fixtures", "legal_full")
    ra, dec = tile_center(0, 4)

    results = []
    for name, fault in CASES:
        for tag, acsd, expect in (("prefix", args.prefix, "prefix"),
                                  (args.tag, args.fixed, "fixed")):
            rec = run_one(acsd, tag, name, fault, hips, ra, dec)
            rec["expect"] = expect
            if name == "ok":
                ok, why = judge(rec, "ok")
            elif tag == "prefix" and name == "identity_drift":
                ok, why = judge_prefix_identity(rec)
            elif tag == "prefix":
                ok, why = judge_prefix_defect(rec)
            else:
                ok, why = judge(rec, "fixed")
            rec["verdict"] = "PASS" if ok else "FAIL"
            rec["verdict_reason"] = why
            results.append(rec)
            print(f"[{tag}] {name:22s} exit={rec['exit_code']} "
                  f"fits={'Y' if rec['fits_present'] else 'n'} "
                  f"writer={'Y' if rec['writer_present'] else 'n'} "
                  f"manifest={(rec['manifest'] or {}).get('status')} -> {rec['verdict']}"
                  + ("" if rec["verdict"] == "PASS" else f" :: {rec['verdict_reason']}"))

    summary = {
        "schema": "acsd.issue94.verify-failclosed/v1",
        "fixed_acsd": os.path.abspath(args.fixed),
        "fixed_acsd_sha256": sha256_file(args.fixed),
        "prefix_acsd": os.path.abspath(args.prefix),
        "prefix_acsd_sha256": sha256_file(args.prefix),
        "n_cases": len(results),
        "n_fail": sum(1 for r in results if r["verdict"] != "PASS"),
        "results": results,
    }
    # 同目录：先成功、后失败 ⇒ 旧 success 不得被新 fail 复用为 complete
    run_one(args.fixed, args.tag, "stale_mix", None, hips, ra, dec)
    stale = run_one(args.fixed, args.tag, "stale_mix", "identity_drift", hips, ra, dec,
                    keep=True)
    stale_ok, stale_why = judge(stale, "fixed")
    summary["stale_mix"] = {
        "exit_code": stale["exit_code"],
        "fits_present": stale["fits_present"],
        "writer_present": stale["writer_present"],
        "manifest": stale["manifest"],
        "tmp_leftovers": stale["tmp_leftovers"],
        "verdict": "PASS" if stale_ok else "FAIL",
        "verdict_reason": stale_why,
    }
    summary["n_fail"] += 0 if stale_ok else 1
    print(f"[{args.tag}] stale_mix (success then fail) -> "
          f"{summary['stale_mix']['verdict']} :: {stale_why}")
    path = os.path.join(EVID, "results.json")
    json.dump(summary, open(path, "w", encoding="utf-8"), indent=2, ensure_ascii=False)
    print(f"wrote {path}: n={summary['n_cases']} fail={summary['n_fail']}")
    return 0 if summary["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
