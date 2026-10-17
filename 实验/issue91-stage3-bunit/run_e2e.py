#!/usr/bin/env python3
"""Issue #91：真实正式 Stage3 CLI（acsd export）正反例矩阵 + 独立 Astropy 读回 + 数值不变性。

对每个受控变体：
  1) 写一个固定 config（除 hips_dir/output_dir 外逐字节相同）；
  2) 运行**真实 CLI** `acsd export --json <cfg> -y`；
  3) 记录 rc / 退出类别 / semantic_code / 发布状态（manifest.status）/ 是否产出 FITS；
  4) 对成功产品用 Astropy 独立读主头 + 像素（BUNIT/WCS/NaN 语义/data 摘要）。

用法:
  python3 实验/issue91-stage3-bunit/run_e2e.py --acsd build/linux-dev/acsd --tag fixed
  python3 实验/issue91-stage3-bunit/run_e2e.py --acsd /tmp/acsd-issue91-base-build/acsd --tag base
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

import numpy as np
from astropy.io import fits

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
EVID = os.path.join(HERE, "evidence")

# 期望口径（冻结于本 Issue；见 REPORT §判决合同）
EXPECT = {
    "legal_explicit":          dict(kind="ok"),
    "legal_bare_adu_sb":       dict(kind="ok"),
    "legal_sr_power_written":  dict(kind="ok"),
    "legacy_px2_alias":        dict(kind="r4", code="P3-INPUT-NOT-SURFACE-BRIGHTNESS"),
    "legacy_pixel2_alias":     dict(kind="r4", code="P3-INPUT-NOT-SURFACE-BRIGHTNESS"),
    # 进口覆盖：全 12 face 有 tile → 全覆盖；只发 face 4 → 中心跨 face 4/5 边界 ⇒ 混合
    "legal_covered_all_faces": dict(kind="ok", hips="base", ra=10.0, dec=0.0,
                                    require="all_finite"),
    "legal_partial_coverage":  dict(kind="ok", hips="partial", ra=45.0, dec=0.0,
                                    require="mixed_finite_nan"),
    "missing_bunit":           dict(kind="r3", code="P3-INPUT-BUNIT-MISSING"),
    "missing_both":            dict(kind="r3", code="P3-INPUT-BUNIT-MISSING"),
    "empty_bunit":             dict(kind="r3", code="P3-INPUT-BUNIT-EMPTY"),
    "dup_same":                dict(kind="r3", code="P3-INPUT-BUNIT-DUPLICATE"),
    "dup_diff":                dict(kind="r3", code="P3-INPUT-BUNIT-CONFLICT"),
    "case_alias_dup":          dict(kind="r3", code="P3-INPUT-BUNIT-DUPLICATE"),
    "alias_conflict":          dict(kind="r3", code="P3-INPUT-UNIT-DECLARATION-CONFLICT"),
    "illegal_format":          dict(kind="r3", code="P3-INPUT-BUNIT-UNPARSABLE"),
    "legal_but_unknown_vocab": dict(kind="r3", code="P3-INPUT-BUNIT-UNPARSABLE"),
    "applied_relative_unit":   dict(kind="r3", code="P3-INPUT-BUNIT-UNPARSABLE"),
    "bare_adu_no_semantics":   dict(kind="r3", code="P3-INPUT-BUNIT-UNDECIDABLE"),
    "semantics_conflict":      dict(kind="r4", code="P3-INPUT-UNIT-SEMANTICS-CONFLICT"),
    "power_conflict":          dict(kind="r4", code="P3-INPUT-UNIT-SEMANTICS-CONFLICT"),
    "not_surface_brightness":  dict(kind="r4", code="P3-INPUT-NOT-SURFACE-BRIGHTNESS"),
    "bad_pixel_semantics":     dict(kind="r4", code="P3-INPUT-PIXEL-SEMANTICS-UNSUPPORTED"),
}

WCS_KEYS = ["NAXIS1", "NAXIS2", "CTYPE1", "CTYPE2", "CUNIT1", "CUNIT2",
            "CRPIX1", "CRPIX2", "CRVAL1", "CRVAL2",
            "CD1_1", "CD1_2", "CD2_1", "CD2_2", "BUNIT", "BITPIX"]


def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def make_config(hips_dir: str, out_dir: str, ra: float, dec: float) -> dict:
    return {
        "schema_version": "1",
        "source": {"hips_dir": hips_dir},
        "output_dir": out_dir,
        "center": {"ra_deg": ra, "dec_deg": dec},
        "width_px": 64,
        "height_px": 64,
        "scale_deg_per_px": 0.01,
        "rotation_deg": 0.0,
        "crpix_px": [32.5, 32.5],
        "output_mode": "surface_brightness",
    }


def read_fits(path: str) -> dict:
    with fits.open(path) as h:
        hd = h[0]
        data = np.asarray(hd.data)
        return {
            "header": {k: (hd.header[k] if not isinstance(hd.header[k], (np.floating, float))
                           else float(hd.header[k])) for k in WCS_KEYS if k in hd.header},
            "shape": list(data.shape),
            "finite_px": int(np.isfinite(data).sum()),
            "nan_px": int(np.isnan(data).sum()),
            "data_sha256": sha256_bytes(np.ascontiguousarray(data, dtype=data.dtype).tobytes()),
            "data_sum": float(np.nansum(data.astype(np.float64))),
        }


def run_case(acsd: str, tag: str, case: str, spec: dict, base: dict) -> dict:
    hips_key = spec.get("hips", "variant")
    if hips_key == "base":
        hips_dir = base["base_hips"]
    elif hips_key == "partial":
        hips_dir = base["partial_hips"]
    else:
        hips_dir = os.path.join(WORK, "variants", case)
    out_dir = os.path.join(WORK, "out", tag, case)
    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir)
    cfg = make_config(hips_dir, out_dir, spec.get("ra", 10.0), spec.get("dec", 0.0))
    cfg_path = os.path.join(WORK, "configs", tag, case + ".json")
    os.makedirs(os.path.dirname(cfg_path), exist_ok=True)
    with open(cfg_path, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)
        f.write("\n")
    log_path = os.path.join(EVID, tag, case + ".log")
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    with open(log_path, "w", encoding="utf-8") as lf:
        proc = subprocess.run([acsd, "export", "--json", cfg_path, "-y"],
                              stdout=lf, stderr=subprocess.STDOUT)
    rc = proc.returncode
    log_text = open(log_path, encoding="utf-8", errors="replace").read()

    fits_path = os.path.join(out_dir, "output_phase3.fits")
    rec = {
        "case": case,
        "kind_expected": spec["kind"],
        "code_expected": spec.get("code"),
        "hips_dir": hips_dir,
        "config_path": cfg_path,
        "config_sha256": sha256_bytes(open(cfg_path, "rb").read()),
        "log_path": log_path,
        "log_sha256": sha256_bytes(log_text.encode("utf-8")),
        "exit_code": rc,
        "fits_present": os.path.isfile(fits_path),
        "manifest": None,
        "p3_props": None,
        "emit_events": [],
    }
    # 发布状态（唯一完成标记 = acsd_run_*.json manifest.status）
    for fn in sorted(os.listdir(out_dir)) if os.path.isdir(out_dir) else []:
        if fn.startswith("acsd_run_") and fn.endswith(".json"):
            m = json.load(open(os.path.join(out_dir, fn), encoding="utf-8"))
            rec["manifest"] = {"file": fn, "status": m.get("status"),
                               "kind": m.get("kind"), "exit_codes": m.get("exit_codes")}
    props_path = os.path.join(out_dir, "p3_props.json")
    if os.path.isfile(props_path):
        p = json.load(open(props_path, encoding="utf-8"))
        rec["p3_props"] = {"bunit": p.get("bunit"), "bunit_input": p.get("bunit_input"),
                           "pixel_semantics": p.get("pixel_semantics"),
                           "pixel_area_power": p.get("pixel_area_power")}
    # semantic_code（节点 manifest / 最终事件）
    import re
    codes = re.findall(r"P3-INPUT-[A-Z-]+", log_text)
    rec["semantic_codes"] = sorted(set(codes))
    if rec["fits_present"]:
        rec["fits"] = read_fits(fits_path)
    # 判定
    exp = spec["kind"]
    if exp == "ok":
        ok = (rc == 0 and rec["fits_present"] and rec.get("manifest", {}).get("status") == "complete"
              and rec["p3_props"] and rec["p3_props"]["bunit"] == "ADU/sr"
              and rec["fits"]["header"].get("BUNIT") == "ADU/sr")
        if spec.get("require") == "all_finite":
            ok = ok and rec["fits"]["nan_px"] == 0 and rec["fits"]["finite_px"] == 64 * 64
        if spec.get("require") == "mixed_finite_nan":
            ok = ok and rec["fits"]["finite_px"] > 0 and rec["fits"]["nan_px"] > 0
        rec["verdict"] = "PASS" if ok else "FAIL"
    else:
        want = 3 if exp == "r3" else 4
        ok = (rc == want and not rec["fits_present"]
              and rec.get("manifest", {}).get("status") == "incomplete"
              and spec["code"] in rec["semantic_codes"])
        rec["verdict"] = "PASS" if ok else "FAIL"
    return rec


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", required=True)
    ap.add_argument("--tag", required=True)
    ap.add_argument("--only", default="")
    args = ap.parse_args()

    base = json.load(open(os.path.join(WORK, "fixtures.json"), encoding="utf-8"))
    os.makedirs(os.path.join(EVID, args.tag), exist_ok=True)

    results = []
    for case, spec in EXPECT.items():
        if args.only and args.only not in case:
            continue
        rec = run_case(args.acsd, args.tag, case, spec, base)
        results.append(rec)
        print(f"[{args.tag}] {case:26s} exit={rec['exit_code']:>2} "
              f"fits={'Y' if rec['fits_present'] else 'n'} "
              f"manifest={(rec['manifest'] or {}).get('status')} "
              f"codes={','.join(rec['semantic_codes']) or '-'} -> {rec['verdict']}")
    n_fail = sum(1 for r in results if r["verdict"] != "PASS")
    out = {
        "tag": args.tag,
        "acsd": os.path.abspath(args.acsd),
        "acsd_sha256": sha256_bytes(open(args.acsd, "rb").read()),
        "n_cases": len(results),
        "n_fail": n_fail,
        "results": results,
    }
    path = os.path.join(EVID, args.tag, "results.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"[{args.tag}] wrote {path} (fail={n_fail}/{len(results)})")
    return 1 if n_fail else 0


if __name__ == "__main__":
    sys.exit(main())
