#!/usr/bin/env python3
"""#80 P5 E0 — 受控**光度归一**输入（真实现测数据，仅施加缺失的 P1 光度尺度）。

背景（本 Issue 的实测根因，见 REPORT.md §4）：
  · H-alpha 两帧（run/science-v2-impl/e2e/out/ab_off）的 `p1_stack.json` 记
    `photappl=0, photscal=1.0` ⇒ 光度尺度**未施加**；
  · Red 帧（run/PHOT-FIX/p1_wcs_full）记 `photappl=1, photscal=5.70245780684985e-17`
    ⇒ 已施加。
  两代产品都声明 `BUNIT=ADU/sr` / `surface_brightness`，但其数值尺度相差 ~1.75e16，
  于是 3 帧联合正规方程跨帧权重跨度达 ~1.4e32（> double 的 16 位有效数字）。

本脚本**只做**：把 H-alpha 帧的 signal 乘 c、variance 乘 c²、ivar 除 c²
（c = Red 帧自己的 photscal），即把它换算到 Red 帧的数值单位。这是同一物理模型的
**合法单位换算**，不是 ridge、不是正则化、不改任何模型自由度，也不改变可辨识性。

它**不是**生产修复：真正的修复属于 P1 光度身份合同（跨代产品的 photappl/photscal
必须一致，且必须在跨帧准入时校验），且跨真实通带共用单个 B_ref 仍缺共同响应/SED
模型。本臂只用于**因果验证**"失败源于光度尺度不一致"。

真实观测 FITS 不入 Git；输出落在 scratch。
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys

import numpy as np
from astropy.io import fits

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC = {
    "A_Halpha_600s": "run/science-v2-impl/e2e/out/ab_off/"
                     "M42_M1_T2_flying_dutchman-20251212_014241-600S-H-alpha",
    "B_Halpha_600s": "run/science-v2-impl/e2e/out/ab_off/"
                     "M42_M1_T2_flying_dutchman-20251216_031003-600S-H-alpha",
    "C_Red_300s": "run/PHOT-FIX/p1_wcs_full/"
                  "M42_M1_T2_flying_dutchman-20251212_012404-300S-Red",
}


def scale_tiles(root: str, c: float) -> int:
    """signal *= c；variance *= c^2；ivar /= c^2；support 不变。"""
    n = 0
    plan = {"signal": c, "variance": c * c, "ivar": 1.0 / (c * c)}
    for prod, factor in plan.items():
        for f in sorted(glob.glob(os.path.join(root, prod, "Norder*", "Dir*", "Npix*.fits"))):
            with fits.open(f, mode="update") as h:
                a = h[0].data
                h[0].data = (a.astype("float64") * factor).astype("float32")
                h.flush()
            n += 1
    return n


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--scratch", default="/workspace/issue80-scratch")
    ap.add_argument("--out", default=os.path.join(
        REPO, "实验/issue80-p5-nonspd-e0/evidence"))
    a = ap.parse_args()
    frozen = os.path.join(a.scratch, "frozen", "abc_photscal_harmonized")
    if os.path.isdir(frozen):
        shutil.rmtree(frozen)
    os.makedirs(frozen, exist_ok=True)
    # 先读各帧 photappl/photscal，确定要施加的尺度 c（= 已施加帧的 photscal）
    tree = {}
    c = None
    for key, rel in SRC.items():
        stack = json.load(open(os.path.join(REPO, rel, "p1_stack.json")))
        tree[key] = {"photappl": stack.get("photappl"), "photscal": stack.get("photscal")}
        if stack.get("photappl") and c is None:
            c = float(stack["photscal"])
    if c is None:
        raise SystemExit("no photappl=1 frame to take the scale from")
    # 要改写的帧用**真实拷贝**（绝不硬链接后原地改，否则会波及真本 inode）；
    # 不改写的帧用硬链接（省空间，且不会被写入）。
    applied = {}
    for key, rel in SRC.items():
        src = os.path.join(REPO, rel)
        dst = os.path.join(frozen, os.path.basename(src))
        if tree[key]["photappl"] == 0:
            subprocess.run(["cp", "-a", src, dst], check=True)
            n = scale_tiles(dst, c)
            applied[key] = {"factor": c, "n_tiles": n, "copy": "real"}
        else:
            subprocess.run(["cp", "-al", src, dst], check=True)
            applied[key] = {"factor": 1.0, "n_tiles": 0, "copy": "hardlink"}
    rec = {"c": c, "src": tree, "applied": applied, "dir": frozen}
    outp = os.path.join(a.out, "photscal_overlay.json")
    with open(outp, "w") as fh:
        json.dump(rec, fh, indent=1, ensure_ascii=False)
    print(json.dumps(rec, ensure_ascii=False, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
