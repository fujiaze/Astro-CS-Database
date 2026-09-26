#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run_extra.py - 补充实验组(在 run_scan.py 之后):
  G0b 修复自检臂: 恒等几何 phase=(0.5,0.5) (pixfrac=1 时 drop 与输出像素逐位对齐,
      M=I), k_corr 必须恒 1(任何 N) —— 装备有效性判据。
  G3b iid 高斯有限 N 参考臂: 同一恒等几何下扫 N, 给出 k_gauss(N) ——
      把『估计器有限 N 偏置(MAD 有限样本偏差 + 中位数方差渐近式偏差)』从
      k_shape 中分离: 正本公式 N=5 时渐近式低估 8.5%(PHASE2_UPM.md SS5)。
  G7  F&H 式(8) R 的算子级数值复核(正本单帧几何, 与闭式(10) 量级对照)。
"""
from __future__ import annotations

import json
import os
import sys
from datetime import datetime, timezone

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mc_kcorr as m  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "results")
os.makedirs(OUT, exist_ok=True)

SEED_G0B = 994000
SEED_G3B = 995000


def dump(name, obj):
    obj["written_utc"] = datetime.now(timezone.utc).isoformat()
    with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=1)
    print(f"[written] {name}")


def g0b():
    op = m.build_operator(src_scale=300.0, out_scale=300.0, pixfrac=1.0, phase=(0.5, 0.5))
    ident = bool(np.allclose(op.M, np.eye(op.M.shape[0])))
    cx, cy = (m.W_SRC / 2 * m.SRC_SCALE_ARCSEC, m.H_SRC / 2 * m.SRC_SCALE_ARCSEC)
    rows = []
    for n in (5, 25, 289):
        if n > op.M.shape[0]:
            continue
        patch = m.patch_nearest(op, n, cx, cy)
        r = m.run_config(op, patch, nmc=4000, sigma=m.SIGMA_SRC, seed0=SEED_G0B + n,
                         iid_seed=990001, with_diag=False)
        rows.append(dict(N=r["N"], k_corr=r["k_corr"], k_se=r["k_corr_mc_se"],
                         k_shape=r["k_shape"]))
    out = dict(group="G0b_sanity_fixed", identity_M=bool(ident), meta=op.meta, rows=rows,
               gate_pass=bool(ident and all(abs(r["k_corr"] - 1.0) < 0.06 for r in rows)))
    dump("g0b_sanity_fixed.json", out)
    return out


def g3b():
    op = m.build_operator(src_scale=300.0, out_scale=300.0, pixfrac=1.0, phase=(0.5, 0.5))
    cx, cy = (m.W_SRC / 2 * m.SRC_SCALE_ARCSEC, m.H_SRC / 2 * m.SRC_SCALE_ARCSEC)
    rows = []
    for n in (5, 9, 17, 25, 49, 81, 121, 169, 225, 289, 400):
        if n > op.M.shape[0]:
            continue
        patch = m.patch_nearest(op, n, cx, cy)
        V = m.mc_medians(op.M, patch, 4000, m.SIGMA_SRC, SEED_G3B + n, op.M.shape[1])
        st = m.median_stats(V)
        rows.append(dict(N=st["N"], k_corr=st["k_corr"], sigma_bg=st["sigma_bg"]))
        print(f"G3b iid-gauss N={st['N']} k={st['k_corr']:.4f}", flush=True)
    out = dict(group="G3b_iid_gauss_reference", rows=rows)
    dump("g3b_gauss_ref.json", out)
    return out


def g7():
    op = m.build_operator(phase=(0.3, 0.55))   # 正本几何(单帧)
    r = m.fh_ratio(op, m.SIGMA_SRC)
    out = dict(group="G7_fh_ratio_canonical", **r)
    dump("g7_fh_ratio.json", out)
    return out


if __name__ == "__main__":
    g0b()
    g3b()
    g7()
    print("[done]")
