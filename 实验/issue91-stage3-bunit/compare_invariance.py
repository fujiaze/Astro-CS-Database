#!/usr/bin/env python3
"""Issue #91：修复前后科学数值不变性对拍（独立 Astropy 读回）。

判据（固定输入、固定算法参数、相同有效单位身份）:
  * signal DATA 逐位相同（bitwise）并报告 max|Δ|；
  * 主头 WCS 关键字（NAXIS/CTYPE/CUNIT/CRPIX/CRVAL/CD/BITPIX）逐位相同；
  * 输出尺寸、有效（有限）像元数、NaN 语义相同。

用法: python3 实验/issue91-stage3-bunit/compare_invariance.py
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np
from astropy.io import fits

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
EVID = os.path.join(HERE, "evidence")

COMPARE_KEYS = ["NAXIS1", "NAXIS2", "BITPIX", "CTYPE1", "CTYPE2", "CUNIT1", "CUNIT2",
                "CRPIX1", "CRPIX2", "CRVAL1", "CRVAL2",
                "CD1_1", "CD1_2", "CD2_1", "CD2_2"]


def load(tag: str, case: str):
    return os.path.join(WORK, "out", tag, case, "output_phase3.fits")


def main() -> int:
    base = json.load(open(os.path.join(EVID, "base", "results.json"), encoding="utf-8"))
    fixed = json.load(open(os.path.join(EVID, "fixed", "results.json"), encoding="utf-8"))
    bmap = {r["case"]: r for r in base["results"]}
    fmap = {r["case"]: r for r in fixed["results"]}

    rows = []
    all_ok = True
    for case, fr in fmap.items():
        br = bmap.get(case)
        if br is None or not fr["fits_present"] or not br["fits_present"]:
            continue
        pb, pf = load("base", case), load("fixed", case)
        with fits.open(pb) as hb, fits.open(pf) as hf:
            db, df = np.asarray(hb[0].data), np.asarray(hf[0].data)
            hkb = {k: hb[0].header[k] for k in COMPARE_KEYS if k in hb[0].header}
            hkf = {k: hf[0].header[k] for k in COMPARE_KEYS if k in hf[0].header}
            bitwise = (db.tobytes() == df.tobytes())
            if db.shape == df.shape:
                both = np.isfinite(db) & np.isfinite(df)
                maxdiff = float(np.max(np.abs(db[both] - df[both]))) if both.any() else 0.0
                nan_same = bool(np.array_equal(np.isnan(db), np.isnan(df)))
            else:
                maxdiff, nan_same = float("nan"), False
            wcs_same = (hkb == hkf)
        ok = bitwise and maxdiff == 0.0 and nan_same and wcs_same
        all_ok = all_ok and ok
        rows.append({
            "case": case,
            "shape": list(db.shape),
            "bitwise_identical": bitwise,
            "max_abs_diff": maxdiff,
            "nan_mask_identical": nan_same,
            "wcs_header_identical": wcs_same,
            "finite_px": int(np.isfinite(db).sum()),
            "nan_px": int(np.isnan(db).sum()),
            "verdict": "PASS" if ok else "FAIL",
        })
        print(f"{case:26s} bitwise={bitwise} max|d|={maxdiff} nan_same={nan_same} "
              f"wcs_same={wcs_same} finite={rows[-1]['finite_px']} nan={rows[-1]['nan_px']} "
              f"-> {rows[-1]['verdict']}")

    out = {"comparison": "fixed(vs)base", "cases": rows,
           "all_pass": bool(all_ok and rows)}
    path = os.path.join(EVID, "invariance.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"wrote {path} all_pass={out['all_pass']} n={len(rows)}")
    return 0 if out["all_pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
