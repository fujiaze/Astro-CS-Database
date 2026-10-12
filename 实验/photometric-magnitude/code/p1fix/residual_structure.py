#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-FIX 诊断 P-A(2)：空间场后残差**还剩多少空间结构**（单帧内，无跨帧耦合）。

原理：残差 e_i = r_i − (location + Σ coef·B̃) 已经是 order=2 多项式拟合后的量。
若 e 在星对上仍有**随间距衰减的空间相关性**，说明多项式没吃完的结构还在（升阶/加项可能有救）；
若 e 的空间相关 ≈ 0（相关函数在任意间距都接近 0），则剩余散度是**逐星随机**的，
**升阶只会过拟合噪声**。

做法（只用该帧自身星点）：把星两两配对，按归一化像面间距分箱，算 e 的 Pearson 相关系数
与结构函数 D(s) = mean[(e_i−e_j)²]，并与"无相关基线" 2·Var(e) 比较。

输出 results/p1fix/residual_structure.json。
"""
from __future__ import annotations
import json, math
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
MAD_SCALE = 0.6744897501960817

BINS = [(0.00, 0.05), (0.05, 0.10), (0.10, 0.20), (0.20, 0.40), (0.40, 0.80), (0.80, 2.00)]


def main():
    res_all = []
    for tag in ("t2", "t3"):
        d = json.loads((SRC / tag / "p1_phot.json").read_text())
        for fk in sorted(d["spatial_gain"]):
            sg = d["spatial_gain"][fk]; pd = d["photscale_detail"][fk]
            s = sg.get("samples", {})
            if not s.get("x"):
                continue
            r = np.array(s["r_log10_finstr_over_fsyn"])
            xs = np.array(s["x"]); ys = np.array(s["y"]); used = np.array(s["used"], bool)
            k_photo = pd.get("k_photo")
            if not (k_photo and k_photo > 0):
                continue
            loc = -math.log10(k_photo)
            norm = sg["norm"]; coef = np.array(sg["coef"]); ctr = np.array(sg["center"])
            xn = (xs - norm["x_ref"]) / norm["x_scale"]; yn = (ys - norm["y_ref"]) / norm["y_scale"]
            B = np.column_stack([xn, yn, xn * xn, xn * yn, yn * yn])
            logm = -((B - ctr) @ coef)
            e = r - (loc - logm)
            m = used
            e = e[m]; xn = xn[m]; yn = yn[m]
            e = e - np.median(e)
            var = float(np.var(e))
            if var <= 0 or len(e) < 50:
                continue
            # 两两配对
            n = len(e)
            iu = np.triu_indices(n, 1)
            de = e[iu[0]] - e[iu[1]]
            dx = xn[iu[0]] - xn[iu[1]]
            dy = yn[iu[0]] - yn[iu[1]]
            sep = np.hypot(dx, dy)
            row = {"tag": tag, "frame": fk, "n": int(n),
                   "sigma_e_dex": math.sqrt(var),
                   "bins": []}
            for lo, hi in BINS:
                sel = (sep >= lo) & (sep < hi)
                k = int(sel.sum())
                if k < 200:
                    row["bins"].append({"lo": lo, "hi": hi, "n_pairs": k})
                    continue
                a = e[iu[0]][sel]; b = e[iu[1]][sel]
                da = a - a.mean(); db = b - b.mean()
                den = math.sqrt(float((da * da).sum()) * float((db * db).sum()))
                rho = float((da * db).sum()) / den if den > 0 else float("nan")
                D = float(np.mean((a - b) ** 2))
                row["bins"].append({"lo": lo, "hi": hi, "n_pairs": k, "rho": rho,
                                    "D_over_2var": D / (2 * var)})
            res_all.append(row)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "residual_structure.json").write_text(json.dumps(res_all, indent=1, ensure_ascii=False))
    # 汇总每档的中位 rho
    print(f"frames={len(res_all)}")
    for bi, (lo, hi) in enumerate(BINS):
        vals = [row["bins"][bi].get("rho") for row in res_all
                if len(row["bins"]) > bi and row["bins"][bi].get("rho") is not None]
        dvals = [row["bins"][bi].get("D_over_2var") for row in res_all
                 if len(row["bins"]) > bi and row["bins"][bi].get("D_over_2var") is not None]
        if vals:
            vals_s = sorted(vals); dv = sorted(dvals)
            print(f"  sep[{lo:.2f},{hi:.2f})  rho median={vals_s[len(vals_s)//2]:+.4f} "
                  f"(min {vals_s[0]:+.4f}, max {vals_s[-1]:+.4f})   D/2var median={dv[len(dv)//2]:.4f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
