#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""C10 ms 生产门验证（只读成品 + 报告，不改生产代码）。

验证载体：本脚本（实验域正本；task-5 原脚本 run/TASK5-MS-SEAM/scripts/ms_seam_verify.py
的入仓副本，内容逐位一致，run/ 下原路径只作运行工作区，不入仓）。
/tmp 二进制不作交付证据；本脚本跑完只在运行工作区写 csv/json，无 /tmp 写入。

四组数字：
  G1 回归门：reg vs reg2 成品 FITS 有限像素逐位一致（maxabs==0.0）；
     与 RERUN3 差异只记录（晕停用量级，非 ms 量级，不判门）。
  G2 缝台阶：ms 臂同窗实测（C8 口径：B 路 FITS 直读，L=0-based 1959:2029，
     R=2030:2100，列中位再中位；haloU=1850-1900 / haloD=1900-1950），判 ≤5e-06。
  G3 暗段不动 + 结构区 sup≈0（C9 口径）：暗三段 1500-1600/1600-1700/1700-1800
     ms 相对回归移动量；结构区（M42S 2050-2150）sup≈0（ms 不吃结构）。
  G4 回归对比数：reg-reg2 / reg-ms / reg-RERUN3 有限像素 maxabs + 同窗 5 段 step 表。

生产门（前台口径）：G1 PASS 且 G2 两段 ≤5e-06 且 G3 通过 ⇒ 总判 PASS，否则 FAIL。
ms 启用方式：ACSD_UPM_MS_ENABLE=1（冻结测试覆写通道；model 保持空对象）。

复跑（运行工作区 run/TASK5-MS-SEAM 下执行）：
  python3 run/TASK5-MS-SEAM/scripts/ms_seam_verify.py
  产物：run/TASK5-MS-SEAM/csv/ms_seam_verify.csv + run/TASK5-MS-SEAM/csv/ms_seam_verify.json
  （run/ 不入仓；本脚本为实验域正本。）
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
R = ROOT / "run" / "TASK5-MS-SEAM"
CSV = R / "csv"
REG = R / "out" / "reg" / "p3" / "output_phase3.fits"
REG2 = R / "out" / "reg2" / "p3" / "output_phase3.fits"
MS = R / "out" / "ms" / "p3" / "output_phase3.fits"
RERUN3 = ROOT / "run" / "E2E-HALO-RERUN3" / "out" / "p3" / "output_phase3.fits"

SEGS = [(1500, 1600, "dark-N"), (1600, 1700, "dark"),
        (1700, 1800, "dark-S"), (1850, 1900, "haloU"),
        (1900, 1950, "haloD"), (1950, 2050, "trans"),
        (2050, 2150, "M42S-struct")]
GATE = 5e-06


def seg_step_band(band: np.ndarray) -> float:
    """C8 口径：列中位再中位；L=0-based 1959:2029，R=2030:2100。NaN 感知。"""
    with np.errstate(all="ignore"):
        cL = np.nanmedian(band[:, 1959:2029], axis=0)
        cR = np.nanmedian(band[:, 2030:2100], axis=0)
    return float(np.nanmedian(cR) - np.nanmedian(cL))


def finite_maxabs(a: np.ndarray, b: np.ndarray) -> float:
    m = np.isfinite(a) & np.isfinite(b)
    if not m.any():
        return float("nan")
    return float(np.max(np.abs(a[m] - b[m])))


def main() -> int:
    from astropy.io import fits
    for p in (REG, REG2, MS):
        if not p.exists():
            print(f"SKIP: 缺成品 {p}")
            return 2
    a = fits.getdata(str(REG)).astype(np.float64)
    b = fits.getdata(str(REG2)).astype(np.float64)
    m = fits.getdata(str(MS)).astype(np.float64)
    r3 = fits.getdata(str(RERUN3)).astype(np.float64) if RERUN3.exists() else None

    out: dict = {"gate": GATE, "inputs": {
        "reg": "run/TASK5-MS-SEAM/out/reg/p3/output_phase3.fits",
        "reg2": "run/TASK5-MS-SEAM/out/reg2/p3/output_phase3.fits",
        "ms": "run/TASK5-MS-SEAM/out/ms/p3/output_phase3.fits",
        "rerun3": "run/E2E-HALO-RERUN3/out/p3/output_phase3.fits"}}

    # G1 回归门：同 HEAD 自回归逐位一致
    g1 = {"reg_reg2_maxabs": finite_maxabs(a, b),
          "masks_identical": bool(((np.isfinite(a)) == (np.isfinite(b))).all())}
    g1["pass"] = bool(g1["reg_reg2_maxabs"] == 0.0 and g1["masks_identical"])
    out["G1_self_regression"] = g1

    # G4 回归对比数（含与 RERUN3 记录值）
    g4 = {"reg_ms_maxabs": finite_maxabs(a, m)}
    steps = {}
    for y0, y1, label in SEGS:
        steps[f"{y0}-{y1}"] = {
            "label": label,
            "reg": seg_step_band(a[y0 - 1:y1, :]),
            "reg2": seg_step_band(b[y0 - 1:y1, :]),
            "ms": seg_step_band(m[y0 - 1:y1, :]),
            "rerun3": seg_step_band(r3[y0 - 1:y1, :]) if r3 is not None else None}
    g4["steps"] = steps
    if r3 is not None:
        g4["reg_rerun3_maxabs"] = finite_maxabs(a, r3)
    out["G4_contrast"] = g4

    # G2 缝台阶（ms 臂 haloU/haloD）
    g2 = {}
    for key in ("1850-1900", "1900-1950"):
        v = steps[key]["ms"]
        g2[key] = {"step": v, "pass": bool(abs(v) <= GATE)}
    g2["pass"] = bool(all(g2[k]["pass"] for k in g2 if k != "pass"))
    out["G2_seam"] = g2

    # G3 暗段不动 + 结构区 sup≈0（C9 口径：ms 相对回归移动量；结构区 sup 为 ms−reg 差）
    g3 = {}
    for key in ("1500-1600", "1600-1700", "1700-1800"):
        mv = steps[key]["ms"] - steps[key]["reg"]
        g3[key] = {"move": mv, "pass": bool(abs(mv) <= GATE)}
    sup = steps["2050-2150"]["ms"] - steps["2050-2150"]["reg"]
    g3["M42S-struct-sup"] = {"sup": sup, "pass": bool(abs(sup) <= GATE)}
    g3["pass"] = bool(all(v["pass"] for v in g3.values() if isinstance(v, dict)))
    out["G3_dark_struct"] = g3

    overall = bool(g1["pass"] and g2["pass"] and g3["pass"])
    out["overall"] = "PASS" if overall else "FAIL"

    CSV.mkdir(parents=True, exist_ok=True)
    (CSV / "ms_seam_verify.json").write_text(
        json.dumps(out, indent=1, ensure_ascii=False), encoding="utf-8")
    lines = ["seg,label,reg,reg2,ms,rerun3"]
    for y0, y1, label in SEGS:
        s = steps[f"{y0}-{y1}"]
        r3v = ("None" if s["rerun3"] is None else ("%.6e" % s["rerun3"]))
        lines.append(str(y0) + "-" + str(y1) + "," + label + "," + ("%.6e" % s["reg"]) + "," + ("%.6e" % s["reg2"]) + "," + ("%.6e" % s["ms"]) + "," + r3v)
    (CSV / "ms_seam_verify.csv").write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"G1 self-regression: maxabs={g1['reg_reg2_maxabs']:.3e} "
          f"{'PASS' if g1['pass'] else 'FAIL'}")
    print(f"G4 reg-ms maxabs={g4['reg_ms_maxabs']:.3e} "
          f"reg-rerun3 maxabs={g4.get('reg_rerun3_maxabs', float('nan')):.3e}")
    for key in ("1850-1900", "1900-1950"):
        print(f"G2 {key} haloU/D ms step={g2[key]['step']:.4e} "
              f"{'PASS' if g2[key]['pass'] else 'FAIL'} (gate 5e-06)")
    for key in ("1500-1600", "1600-1700", "1700-1800"):
        print(f"G3 {key} dark move={g3[key]['move']:.3e} "
              f"{'PASS' if g3[key]['pass'] else 'FAIL'}")
    print(f"G3 M42S-struct sup={g3['M42S-struct-sup']['sup']:.3e} "
          f"{'PASS' if g3['M42S-struct-sup']['pass'] else 'FAIL'}")
    print(f"OVERALL {out['overall']}")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())
