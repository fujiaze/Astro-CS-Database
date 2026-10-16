#!/usr/bin/env python3
"""Issue #11 —— 负例（缺少 obs_filter key）+ Phase3/Export 消费面检查。

1. 负例：真实 Phase1 产品 overlay 删掉 `signal/properties` 的 `obs_filter` 行 ⇒
   coverage `inspect_frame` 应 fail-closed（"missing obs_filter property"），
   CLI 非 0（input），不发布产品；与"显式空串 = 已声明 unknown"必须区分。
2. Export：对完整 mosaic 产品跑真实 `acsd export`，检查
   `p3_props.json` 与 `output_phase3.fits` 是否携带/读取名义通带身份
   （obs_filter / response_distribution / FILTER），并与产品的 properties 对照。

用法：
  python3 run_negative_and_export.py --acsd <bin> --product <complete_mosaic_product> \
      --work /workspace/issue11-scratch/neg_export
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
MK = os.path.join(REPO, "实验/issue11-passband-identity/code/make_labeled_inputs.py")
SRC_A = "run/science-v2-impl/e2e/out/ab_off/M42_M1_T2_flying_dutchman-20251212_014241-600S-H-alpha"


def sh(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", required=True)
    ap.add_argument("--product", required=True, help="complete mosaic product dir")
    ap.add_argument("--work", default="/workspace/issue11-scratch/neg_export")
    ap.add_argument("--json", default=None)
    a = ap.parse_args()
    out = {}
    os.makedirs(a.work, exist_ok=True)

    # ── 1. 负例：缺少 obs_filter key ────────────────────────────────────────
    base = os.path.basename(SRC_A)
    dst = os.path.join(a.work, "missing_key", base)
    shutil.rmtree(os.path.join(a.work, "missing_key"), ignore_errors=True)
    r = sh(["python3", MK, "--src", os.path.join(REPO, SRC_A), "--dst", dst, "--drop-key"])
    assert r.returncode == 0, r.stderr
    props = open(os.path.join(dst, "signal", "properties")).read()
    out["missing_key_has_obs_filter_line"] = any(
        l.startswith("obs_filter=") for l in props.splitlines())
    cfg = {"schema_version": "1", "hips_paths": [dst],
           "output_dir": os.path.join(a.work, "missing_key_out"),
           "snr_path": "sparse_reconstruct"}
    cfg_path = os.path.join(a.work, "missing_key.cfg.json")
    json.dump(cfg, open(cfg_path, "w"), indent=2)
    p = sh([a.acsd, "mosaic", "--json", cfg_path, "-y"])
    open(os.path.join(a.work, "missing_key.stderr"), "w").write(p.stderr)
    out["missing_key_exit"] = p.returncode
    out["missing_key_named_error"] = (
        "missing obs_filter property" in p.stderr or "missing obs_filter" in p.stderr)
    out["missing_key_product_published"] = any(
        e.startswith("M42") for e in os.listdir(cfg["output_dir"])
        if os.path.isdir(cfg["output_dir"]))
    ok_neg = (p.returncode != 0 and out["missing_key_named_error"]
              and not out["missing_key_product_published"]
              and not out["missing_key_has_obs_filter_line"])
    print(f"[{'PASS' if ok_neg else 'FAIL'}] negative missing obs_filter key: exit={p.returncode} "
          f"named_error={out['missing_key_named_error']} product={out['missing_key_product_published']}")
    out["negative_pass"] = ok_neg

    # ── 2. Export 消费面 ────────────────────────────────────────────────────
    prod = a.product.rstrip("/")
    props = {}
    for line in open(os.path.join(prod, "signal", "properties"), encoding="utf-8", errors="replace"):
        if "=" in line:
            k, v = line.rstrip("\n").split("=", 1)
            props[k] = v
    fin = json.load(open(os.path.join(prod, "p2_final.json")))
    out["product_obs_filter_properties"] = props.get("obs_filter")
    out["product_obs_filter_p2_final"] = fin.get("obs_filter")
    out["product_response_distribution"] = fin.get("response_distribution")
    # export 配置（中心取产品 signal 的首个叶 tile 的 TAN 产品几何不可得，用既有 export.json 中心）
    exp_dir = os.path.join(a.work, "export")
    shutil.rmtree(exp_dir, ignore_errors=True)
    exp_cfg = {
        "schema_version": "1",
        "source": {"hips_dir": prod},
        "output_dir": exp_dir,
        "center": {"ra_deg": 83.2835420429, "dec_deg": -6.3744273786},
        "width_px": 512, "height_px": 512, "scale_deg_per_px": 0.0012,
        "rotation_deg": 0.0, "crpix_px": [256.5, 256.5],
        "output_mode": "surface_brightness",
    }
    exp_cfg_path = os.path.join(a.work, "export.cfg.json")
    json.dump(exp_cfg, open(exp_cfg_path, "w"), indent=2)
    pe = sh([a.acsd, "export", "--json", exp_cfg_path, "-y"])
    open(os.path.join(a.work, "export.stderr"), "w").write(pe.stderr)
    out["export_exit"] = pe.returncode
    p3p = os.path.join(exp_dir, "p3_props.json")
    if os.path.isfile(p3p):
        d3 = json.load(open(p3p))
        out["p3_props_keys"] = sorted(d3.keys())
        out["p3_props_filter_keys"] = {k: v for k, v in d3.items()
                                       if "filt" in k.lower() or "resp" in k.lower()}
    fits_path = os.path.join(exp_dir, "output_phase3.fits")
    out["export_fits_exists"] = os.path.isfile(fits_path)
    if out["export_fits_exists"]:
        try:
            from astropy.io import fits
            h = fits.getheader(fits_path)
            out["export_fits_filter_keys"] = {k: h[k] for k in h
                                              if "FILT" in k.upper() or "RESP" in k.upper()}
            out["export_fits_nkeys"] = len(h)
        except Exception as ex:  # noqa: BLE001
            out["export_fits_error"] = str(ex)
    out["export_reads_nominal_identity"] = bool(
        out.get("p3_props_filter_keys") or out.get("export_fits_filter_keys"))
    print(f"[INFO] export exit={out['export_exit']} "
          f"p3_props_filter_keys={out.get('p3_props_filter_keys')} "
          f"fits_filter_keys={out.get('export_fits_filter_keys')} "
          f"-> export_reads_nominal_identity={out['export_reads_nominal_identity']}")

    if a.json:
        json.dump(out, open(a.json, "w"), indent=2, ensure_ascii=False)
        print("json:", a.json)
    return 0 if ok_neg else 1


if __name__ == "__main__":
    sys.exit(main())
