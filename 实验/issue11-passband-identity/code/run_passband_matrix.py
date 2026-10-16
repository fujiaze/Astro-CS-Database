#!/usr/bin/env python3
"""Issue #11 —— Mosaic 名义通带身份矩阵（真实 CLI E2E）。

对每组输入（真实 Phase1 HiPS 产品的硬链接 overlay，仅名义 obs_filter 不同）跑
`acsd mosaic --json cfg -y`，要求 exit 0 且发布完整产品，抽取：
  · HiPS `signal/properties:obs_filter`
  · `p2_final.json:obs_filter` / `response_distribution`
  · 首张 signal 叶 tile 的 FITS `FILTER` 键（真实写盘）
并把同多重集的不同排列做逐字一致性断言。

用法：
  python3 run_passband_matrix.py --acsd <bin> --out /workspace/issue11-scratch/matrix_post \
      [--cases all] [--keep <case,case>]
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
SCRATCH = "/workspace/issue11-scratch"
SRC = {
    "A": "run/science-v2-impl/e2e/out/ab_off/M42_M1_T2_flying_dutchman-20251212_014241-600S-H-alpha",
    "B": "run/science-v2-impl/e2e/out/ab_off/M42_M1_T2_flying_dutchman-20251216_031003-600S-H-alpha",
    "C": "run/PHOT-FIX/p1_wcs_full/M42_M1_T2_flying_dutchman-20251212_012404-300S-Red",
    # 3 帧用例用同一 M42_M2 面板、同滤镜/同曝光的三个真实帧（保证天空面联合解良态）
    "D": "run/PHOT-FIX/p1_wcs_full/M42_M2_T2_flying_dutchman-20251212_020002-300S-Red",
    "E": "run/PHOT-FIX/p1_wcs_full/M42_M2_T2_flying_dutchman-20251216_033550-300S-Red",
    "F": "run/PHOT-FIX/p1_wcs_full/M42_M2_T2_flying_dutchman-20251224_033141-300S-Red",
}
# case -> list of (frame_key, label)；期望 (obs_filter, response_distribution)
CASES = {
    "empty_halpha":   ([("A", ""), ("B", "Halpha")], "mixed:2_passbands",
                       {"(undeclared)": 1, "Halpha": 1}),
    "halpha_empty":   ([("B", "Halpha"), ("A", "")], "mixed:2_passbands",
                       {"(undeclared)": 1, "Halpha": 1}),
    "empty_empty":    ([("A", ""), ("B", "")], "",
                       {"(undeclared)": 2}),
    "red_red":        ([("A", "Red"), ("B", "Red")], "Red",
                       {"Red": 2}),
    "red_blue":       ([("A", "Red"), ("B", "Blue")], "mixed:2_passbands",
                       {"Blue": 1, "Red": 1}),
    "blue_red":       ([("A", "Blue"), ("B", "Red")], "mixed:2_passbands",
                       {"Blue": 1, "Red": 1}),
    "pem_01":         ([("D", ""), ("E", "Red"), ("F", "Blue")], "mixed:3_passbands",
                       {"(undeclared)": 1, "Blue": 1, "Red": 1}),
    "pem_02":         ([("D", ""), ("F", "Blue"), ("E", "Red")], "mixed:3_passbands",
                       {"(undeclared)": 1, "Blue": 1, "Red": 1}),
    "pem_03":         ([("E", "Red"), ("D", ""), ("F", "Blue")], "mixed:3_passbands",
                       {"(undeclared)": 1, "Blue": 1, "Red": 1}),
    "pem_04":         ([("E", "Red"), ("F", "Blue"), ("D", "")], "mixed:3_passbands",
                       {"(undeclared)": 1, "Blue": 1, "Red": 1}),
    "pem_05":         ([("F", "Blue"), ("D", ""), ("E", "Red")], "mixed:3_passbands",
                       {"(undeclared)": 1, "Blue": 1, "Red": 1}),
    "pem_06":         ([("F", "Blue"), ("E", "Red"), ("D", "")], "mixed:3_passbands",
                       {"(undeclared)": 1, "Blue": 1, "Red": 1}),
}
MK = os.path.join(REPO, "实验/issue11-passband-identity/code/make_labeled_inputs.py")


def basename(key: str) -> str:
    return os.path.basename(SRC[key])


def overlay(tag: str, key: str, label: str) -> str:
    dst = f"{SCRATCH}/ovl_{tag}/{label or 'EMPTY'}_{key}/{basename(key)}"
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    r = subprocess.run(["python3", MK, "--src", os.path.join(REPO, SRC[key]),
                        "--dst", dst, "--label", label],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"overlay failed: {r.stdout} {r.stderr}")
    return dst


def sha_signal(product: str) -> dict:
    d = os.path.join(product, "signal")
    out = {}
    for dp, _dd, fs in os.walk(d):
        for f in sorted(fs):
            p = os.path.join(dp, f)
            h = hashlib.sha256()
            with open(p, "rb") as fh:
                for c in iter(lambda: fh.read(1 << 20), b""):
                    h.update(c)
            out[os.path.relpath(p, d)] = h.hexdigest()
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--cases", default="all")
    ap.add_argument("--keep", default="")
    ap.add_argument("--tag", default="post")
    a = ap.parse_args()
    keep = set(x for x in a.keep.split(",") if x)
    names = list(CASES) if a.cases == "all" else a.cases.split(",")
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(os.path.join(a.out, "logs"), exist_ok=True)
    results = []
    for name in names:
        specs, exp_of, exp_dist = CASES[name]
        paths = [overlay(a.tag, k, lab) for k, lab in specs]
        out_dir = os.path.join(a.out, "run_" + name)
        shutil.rmtree(out_dir, ignore_errors=True)
        cfg = {"schema_version": "1", "hips_paths": paths,
               "output_dir": out_dir, "snr_path": "sparse_reconstruct"}
        cfg_path = os.path.join(a.out, "logs", name + ".cfg.json")
        json.dump(cfg, open(cfg_path, "w"), indent=2)
        t0 = time.time()
        p = subprocess.run([a.acsd, "mosaic", "--json", cfg_path, "-y"],
                           capture_output=True, text=True)
        dt = time.time() - t0
        open(os.path.join(a.out, "logs", name + ".stderr"), "w").write(p.stderr)
        prod = None
        if os.path.isfile(os.path.join(out_dir, "p2_final.json")):
            prod = out_dir  # mosaic: p2_final.json 直接落在 output_dir（不嵌套帧子目录）
        elif os.path.isdir(out_dir):
            for e in os.listdir(out_dir):
                if os.path.isfile(os.path.join(out_dir, e, "p2_final.json")):
                    prod = os.path.join(out_dir, e)
                    break
        rec = {"case": name, "exit": p.returncode, "wall_s": round(dt, 1),
               "labels": [lab for _k, lab in specs], "input_keys": [k for k, _l in specs],
               "expected_obs_filter": exp_of, "expected_response_distribution": exp_dist,
               "output_dir": out_dir, "product": prod}
        if prod and os.path.isfile(os.path.join(prod, "p2_final.json")):
            fin = json.load(open(os.path.join(prod, "p2_final.json")))
            rec["obs_filter_p2_final"] = fin.get("obs_filter")
            rec["response_distribution"] = fin.get("response_distribution")
            props = os.path.join(prod, "signal", "properties")
            of = None
            for line in open(props, encoding="utf-8", errors="replace"):
                if line.startswith("obs_filter="):
                    of = line.split("=", 1)[1].rstrip("\n")
            rec["obs_filter_properties"] = of
            # 首张 signal 叶 tile 的 FITS FILTER 键（真实写盘），astropy 可选
            try:
                from astropy.io import fits
                import glob
                tiles = sorted(glob.glob(os.path.join(prod, "signal", "Norder*", "Dir*", "Npix*.fits")))
                if tiles:
                    h = fits.getheader(tiles[0])
                    rec["tile_fits_FILTER"] = h.get("FILTER")
                    rec["tile_file"] = os.path.relpath(tiles[0], prod)
            except Exception as ex:  # noqa: BLE001
                rec["tile_fits_error"] = str(ex)
            rec["signal_sha256"] = sha_signal(prod)
            rec["n_signal_files"] = len(rec["signal_sha256"])
        # 断言
        ok = p.returncode == 0
        if rec.get("obs_filter_properties") != exp_of:
            ok = False
        if rec.get("obs_filter_p2_final") != exp_of:
            ok = False
        if rec.get("response_distribution") != exp_dist:
            ok = False
        rec["pass"] = ok
        print(f"[{'PASS' if ok else 'FAIL'}] {name:14s} exit={p.returncode} "
              f"obs_filter={rec.get('obs_filter_properties')!r} dist={rec.get('response_distribution')} "
              f"({dt:.0f}s)", flush=True)
        results.append(rec)
        # 保存原始元数据文件（小），供"逐字相同"级别对照（在大输出删除前）
        if prod:
            meta = os.path.join(a.out, "meta")
            os.makedirs(meta, exist_ok=True)
            shutil.copy(os.path.join(prod, "p2_final.json"),
                        os.path.join(meta, name + ".p2_final.json"))
            shutil.copy(os.path.join(prod, "signal", "properties"),
                        os.path.join(meta, name + ".properties"))
        if name not in keep:
            shutil.rmtree(out_dir, ignore_errors=True)
            rec["output_dir_removed"] = True
    json.dump(results, open(os.path.join(a.out, "matrix.json"), "w"),
              indent=2, ensure_ascii=False)
    nfail = sum(1 for r in results if not r["pass"])
    print(f"== FAIL={nfail}/{len(results)} ==")
    return 1 if nfail else 0


if __name__ == "__main__":
    sys.exit(main())
