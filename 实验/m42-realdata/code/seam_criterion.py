# 实验/m42-realdata/code/seam_criterion.py
# -*- coding: utf-8 -*-
"""**权威接缝判据本体**的逐字副本（用于真实导出图复算）。

来源（唯一权威）：实验/additive-sky-seamless/code/sci_c_common.py:356-432
  - _step_at(m, xb, halfwin, base_win, order, min_valid)          [sci_c_common.py:356-374]
  - seam_steps(mosaic, boundaries, halfwin, base_win, order, ...)  [sci_c_common.py:377-411]
  - BOUNDARIES                                                     [sci_c_common.py:166]

判据定义（非退化，保留背景）：
  1. profile m(x) = nanmedian_y mosaic(x, .)（对星点稳健）；
  2. 在每个边界两侧 base_win 内（挖掉 ±(halfwin+2)）拟合 order 阶多项式基线；
  3. step = median(残差 右 halfwin 列) - median(残差 左 halfwin 列)；
  4. rel  = step / median(m 于窗口内)；
  5. excess = step(x_b) - median(step(x_b+delta), delta in offsets)：用同一条带内
     偏离边界的对照线扣掉"真结构"造成的系统台阶（off-locus 对照）。

**本文件不新定义任何判据**：函数体与 sci_c_common.py 逐字一致，并由
c3_seam_additive.py 的 SELFTEST 判据对随机输入做逐位比对（rtol=0）。
"""
from __future__ import annotations

import numpy as np

BOUNDARIES = [128, 192, 256, 320, 384, 448]   # sci_c_common.py:166（合成 512 图）


def _step_at(m, xb, halfwin, base_win, order, min_valid):
    m = np.asarray(m)
    nx = m.size
    lo, hi = max(0, xb - base_win), min(nx, xb + base_win)
    xs = np.arange(lo, hi)
    good = np.isfinite(m[lo:hi])
    excl = (xs >= xb - halfwin - 2) & (xs <= xb + halfwin - 2)
    fitm = good & (~excl)
    if fitm.sum() < order + 2:
        return np.nan, np.nan
    coef = np.polyfit(xs[fitm], m[lo:hi][fitm], order)
    res = m[lo:hi] - np.polyval(coef, xs)
    L = res[(xs >= xb - halfwin) & (xs <= xb - 1)]
    R = res[(xs >= xb) & (xs <= xb + halfwin - 1)]
    L = L[np.isfinite(L)]
    R = R[np.isfinite(R)]
    if L.size < min_valid or R.size < min_valid:
        return np.nan, float(np.nanmedian(m[lo:hi]))
    return float(np.median(R) - np.median(L)), float(np.nanmedian(m[lo:hi]))


def seam_steps(mosaic, boundaries=None, halfwin=8, base_win=64, order=2,
               min_valid=8, offlocus=True,
               offsets=(-96, -64, -32, 32, 64, 96)):
    """**非退化接缝判据**：保留背景，度量覆盖子集突变处的背景电平跳变。"""
    if boundaries is None:
        boundaries = BOUNDARIES
    m = np.nanmedian(mosaic, axis=0)
    out = []
    for xb in boundaries:
        step, lvl = _step_at(m, xb, halfwin, base_win, order, min_valid)
        exc = np.nan
        if offlocus and np.isfinite(step):
            ctrl = []
            for d in offsets:
                xc = xb + d
                if 0 <= xc < m.size:
                    s2, _ = _step_at(m, xc, halfwin, base_win, order, min_valid)
                    if np.isfinite(s2):
                        ctrl.append(s2)
            if ctrl:
                exc = float(step - np.median(ctrl))
        out.append(dict(x=xb, step=step, excess=exc,
                        rel=step / lvl if (lvl and np.isfinite(step)) else np.nan,
                        nvalid=int(2 * halfwin), level=lvl))
    return out


def steps_on_profile(m, boundaries, halfwin=8, base_win=64, order=2, min_valid=8,
                     offsets=(-96, -64, -32, 32, 64, 96)):
    """把**同一判据本体**（_step_at + off-locus 对照）套在一条给定的一维剖面上。

    用于"逐行块一致性"检查：真接缝在各行块上给出同号同量级的 excess，
    天体结构（星云纤维）不会。
    """
    m = np.asarray(m)
    out = []
    for xb in boundaries:
        step, lvl = _step_at(m, xb, halfwin, base_win, order, min_valid)
        exc = np.nan
        if np.isfinite(step):
            ctrl = []
            for d in offsets:
                xc = xb + d
                if 0 <= xc < m.size:
                    s2, _ = _step_at(m, xc, halfwin, base_win, order, min_valid)
                    if np.isfinite(s2):
                        ctrl.append(s2)
            if ctrl:
                exc = float(step - np.median(ctrl))
        out.append(dict(x=int(xb), step=step, excess=exc, level=lvl))
    return out


def seam_steps_axis(mosaic, boundaries, axis=0, **kw):
    """把同一判据套在另一轴上（转置后调用 seam_steps，判据本体不变）。"""
    a = mosaic if axis == 0 else np.asarray(mosaic).T
    return seam_steps(a, boundaries=boundaries, **kw)
