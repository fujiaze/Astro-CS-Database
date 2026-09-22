#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 重建算子库：稀疏控制点网格 -> 稠密场。

统一签名（所有算子）：
    op(ctrl, D, shape, **kw) -> dense(float64, shape)
        ctrl  : (ny, nx) 控制点值，控制点位于各自 cell 中心，cell 边长 D px；
        shape : (H, W) 输出像素尺寸；
        NaN 控制点由 nan_policy 处理（见 prefill_nan），RBF/IDW 类按各自语义丢弃。

算子族（>=5 类，含天文主流实现语义）：
  A 最近邻      nn
  B 局部多项式  bilinear / bicubic_cc
  C 样条        spline_natural（自然边界可分离三次样条，SExtractor 家族核心）
                sextractor_spline（median3 + spline_natural，SExtractor 2.28.2 语义）
  D 平滑样条    tps / tps_smooth（薄板样条，smoothing=0 / >0）
  E RBF/GPR     rbf_mq / rbf_gauss / gpr_rbf / gpr_matern32 / gpr_exp
  F 天文实现    photutils_zoom3 / photutils_zoom1（photutils 2.2.0 Background2D 默认语义）
                idw（photutils ShepardIDWInterpolator 语义：kNN + 1/(d^power+reg)）

外部实现出处（只读核验，不复制 GPL 代码；见 docs/EXP-04-RECONSTRUCTION.md §7）：
  - SExtractor 2.28.2 src/back.c:1044 makebackspline（自然边界，y 向二阶导）
    src/back.c:1092 subbackline / :1241 backrmsline（natural 可分离双三次样条插值，
    src/back.c:1061/1070 "natural" 边界条件）；src/back.c:745 filterback 逐 mesh 中值滤波
    （:827，fthresh=prefs.backfthresh 默认 0.0，src/preflist.h:272）；
    src/back.c:846 backsig=fqmedian(sigma2,np)（帧级 σ = 跨 mesh 中位数）。
    注：源码里另有 back()（src/back.c:990，双线性），唯一调用点 src/analyse.c:72，
    只为单个天体记录中心处背景电平，**不参与逐像素稠密化**；Bertin & Arnouts 1996
    论文 p.394 写的是 bilinear，与 2.28.2 源码不一致（见报告 §7）。
  - photutils 2.2.0 background/interpolators.py:61 BkgZoomInterpolator(order=3,
    mode='reflect', clip=True) / :147 BkgIDWInterpolator(n_neighbors=10, power=1.0)；
    background/background_2d.py:216-221 filter_size=(3,3) 默认 + zoom 插值默认；
    utils/interpolation.py:12 ShepardIDWInterpolator（w=1/(d^power+reg)）。
"""
from __future__ import annotations

import numpy as np
from scipy import ndimage
from scipy.interpolate import RBFInterpolator
from scipy.linalg import solve_banded
from scipy.spatial import cKDTree

# ---------------------------------------------------------------------------
# 控制点 <-> 像素 坐标约定（与既有 bilinear_regular_grid_v1 一致）
# ---------------------------------------------------------------------------
def _axis_positions(n_out: int, n_ctrl: int, D: int):
    """输出像素中心 -> 控制点索引坐标（控制点在 cell 中心，即 (i+0.5)*D-0.5）。"""
    t = (np.arange(n_out) + 0.5) / D - 0.5
    return np.clip(t, 0.0, n_ctrl - 1.0)


def prefill_nan(ctrl, policy="nearest_valid"):
    """NaN 控制点处理策略（显式登记，禁止静默）。

    nearest_valid : SExtractor 2.28.2 src/back.c:769-796 语义——取最近有效 mesh，
                    等距时取平均；全部无效时返回 NaN 由调用方 fail-closed。
    frame_median  : 以有限控制点的中位数填充（保守兜底，会在无效区制造假平坦）。
    keep          : 保留 NaN（RBF/IDW 类自行丢弃，见各算子）。
    """
    c = np.asarray(ctrl, dtype=np.float64)
    bad = ~np.isfinite(c)
    if not bad.any():
        return c.copy(), 0
    if policy == "keep":
        return c.copy(), int(bad.sum())
    if policy == "frame_median":
        out = c.copy()
        out[bad] = np.median(c[~bad]) if (~bad).any() else np.nan
        return out, int(bad.sum())
    if policy != "nearest_valid":
        raise ValueError("unknown nan_policy: %r" % (policy,))
    ny, nx = c.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    gy = yy[~bad].astype(np.float64)
    gx = xx[~bad].astype(np.float64)
    gv = c[~bad]
    if gv.size == 0:
        return c.copy(), int(bad.sum())
    tree = cKDTree(np.column_stack([gy, gx]))
    by, bx = yy[bad].astype(np.float64), xx[bad].astype(np.float64)
    d, idx = tree.query(np.column_stack([by, bx]), k=min(4, gv.size))
    d = np.atleast_2d(d)
    idx = np.atleast_2d(idx)
    # 等距并列取平均（SExtractor 语义：d2==d2min 时累加平均）
    dmin = d[:, :1]
    tie = np.isclose(d, dmin, rtol=0.0, atol=1e-12)
    vals = np.where(tie, gv[idx], 0.0)
    cnt = tie.sum(axis=1)
    out = c.copy()
    out[bad] = vals.sum(axis=1) / cnt
    return out, int(bad.sum())


# ---------------------------------------------------------------------------
# A 最近邻
# ---------------------------------------------------------------------------
def op_nn(ctrl, D, shape, nan_policy="nearest_valid", **_):
    c, nbad = prefill_nan(ctrl, nan_policy)
    ny, nx = c.shape
    iy = np.clip(((np.arange(shape[0]) + 0.5) / D).astype(int), 0, ny - 1)
    ix = np.clip(((np.arange(shape[1]) + 0.5) / D).astype(int), 0, nx - 1)
    return c[np.ix_(iy, ix)], nbad


# ---------------------------------------------------------------------------
# B 局部多项式：双线性 / 三次卷积
# ---------------------------------------------------------------------------
def op_bilinear(ctrl, D, shape, nan_policy="nearest_valid", **_):
    """双线性（= 既有 bilinear_regular_grid_v1，边界 clamp）。"""
    c, nbad = prefill_nan(ctrl, nan_policy)
    ny, nx = c.shape
    ys = _axis_positions(shape[0], ny, D)
    xs = _axis_positions(shape[1], nx, D)
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    y1 = np.minimum(y0 + 1, ny - 1); x1 = np.minimum(x0 + 1, nx - 1)
    wy = (ys - y0)[:, None]; wx = (xs - x0)[None, :]
    top = c[y0][:, x0] * (1 - wx) + c[y0][:, x1] * wx
    bot = c[y1][:, x0] * (1 - wx) + c[y1][:, x1] * wx
    return top * (1 - wy) + bot * wy, nbad


def op_bilinear_prod(ctrl, D, shape, nan_policy="nearest_valid", **_):
    """**生产现行算子语义**（逐行镜像，用于证明本单元 op_bilinear 是它的忠实等价）：

    出处 lib/algorithms/integration/v6/src/weight_chain.cpp:136 reconstruct_sparse_snr：
      gx = (x - x0)/dx；越出 [0, nx-1] ⇒ out_of_domain=true + fail-closed（**不外推、
      不回退帧级**，weight_chain.h:106、:249-256）；域内先 clamp 到 [0, nx-1] 再
      eval_bilinear（索引夹取 [0, n-2]）。控制点坐标 = x0+i·dx, y0+j·dy（**不是 cell 中心**）。
    因此本函数与本单元 op_bilinear 在节点位置一致时**逐位等价**（见 selftest 的 S6）。
    """
    c, nbad = prefill_nan(ctrl, nan_policy)
    ny, nx = c.shape
    cy = (np.arange(shape[0]) + 0.5) / D - 0.5
    cx = (np.arange(shape[1]) + 0.5) / D - 0.5
    cy = np.clip(cy, 0.0, ny - 1.0)          # 生产：域外 fail-closed，域内 clamp
    cx = np.clip(cx, 0.0, nx - 1.0)
    j0 = np.clip(np.floor(cy).astype(int), 0, ny - 2)
    i0 = np.clip(np.floor(cx).astype(int), 0, nx - 2)
    fy = cy - j0; fx = cx - i0
    a = c[np.ix_(j0, i0)]; b = c[np.ix_(j0, i0 + 1)]
    cc = c[np.ix_(j0 + 1, i0)]; d = c[np.ix_(j0 + 1, i0 + 1)]
    out = ((1 - fx)[None, :] * (1 - fy)[:, None] * a + fx[None, :] * (1 - fy)[:, None] * b
           + (1 - fx)[None, :] * fy[:, None] * cc + fx[None, :] * fy[:, None] * d)
    return out, nbad


def _cc_weights(t, a=-0.5):
    """Keys 三次卷积权重（4 抽头），t 为区间内小数部分。"""
    t2, t3 = t * t, t * t * t
    w = np.empty(t.shape + (4,), dtype=np.float64)
    # 标准 Keys 三次卷积权重（a=-0.5 时为 Catmull-Rom）：
    w[..., 0] = a * t3 - 2 * a * t2 + a * t
    w[..., 1] = (a + 2) * t3 - (a + 3) * t2 + 1
    w[..., 2] = -(a + 2) * t3 + (2 * a + 3) * t2 - a * t
    w[..., 3] = -a * t3 + a * t2
    return w


def op_bicubic_cc(ctrl, D, shape, nan_policy="nearest_valid", a=-0.5, **_):
    """三次卷积（Catmull-Rom 类，a=-0.5），4x4 支撑，边界 clamp。"""
    c, nbad = prefill_nan(ctrl, nan_policy)
    ny, nx = c.shape

    def _interp_axis(vals, pos, n):
        i = np.floor(pos).astype(int)
        t = pos - i
        idx = np.stack([np.clip(i - 1 + k, 0, n - 1) for k in range(4)], axis=-1)
        w = _cc_weights(t, a)
        taken = np.take(vals, idx, axis=0)          # (..., 4, n_rest)
        return np.einsum("...kn,...k->...n", taken, w)

    ys = _axis_positions(shape[0], ny, D)
    xs = _axis_positions(shape[1], nx, D)
    tmp = _interp_axis(c, ys, ny)                 # (H, nx)
    out = _interp_axis(tmp.T, xs, nx).T           # (H, W)
    return out, nbad


# ---------------------------------------------------------------------------
# C 自然边界可分离三次样条（SExtractor 家族）
# ---------------------------------------------------------------------------
_NAT_AB = None


def _nat_ab(n: int):
    """自然边界三次样条（节点等距，h=1）的常系数三对角矩阵（带宽形式）。"""
    global _NAT_AB
    if _NAT_AB is not None and _NAT_AB[0] == n:
        return _NAT_AB[1]
    if n < 3:
        ab = None
    else:
        ab = np.zeros((3, n - 2))
        ab[0, 1:] = 1.0     # 上对角
        ab[1, :] = 4.0      # 主对角
        ab[2, :-1] = 1.0    # 下对角
    _NAT_AB = (n, ab)
    return ab


def _nat_second_deriv(y, axis=0):
    """沿 axis 的自然边界二阶导 M（节点等距 h=1）。y 形状 (n, m) 时沿 axis=0。"""
    y = np.moveaxis(np.asarray(y, dtype=np.float64), axis, 0)
    n = y.shape[0]
    if n < 3:
        return np.zeros_like(y)
    rhs = 6.0 * (y[2:] - 2.0 * y[1:-1] + y[:-2])
    M = np.zeros_like(y)
    M[1:-1] = solve_banded((1, 1), _nat_ab(n), rhs)
    return np.moveaxis(M, 0, axis)


def _nat_eval(y, M, pos, axis=0):
    """自然边界三次样条求值：pos 已 clamp 到 [0, n-1]。"""
    y = np.moveaxis(np.asarray(y, np.float64), axis, 0)
    M = np.moveaxis(np.asarray(M, np.float64), axis, 0)
    n = y.shape[0]
    if n == 1:
        return np.moveaxis(np.repeat(y, len(pos), axis=0), 0, axis)
    i = np.clip(np.floor(pos).astype(int), 0, n - 2)
    h = np.clip(pos - i, 0.0, 1.0)
    h = h.reshape(h.shape + (1,) * (y.ndim - 1))     # 广播到其余轴
    y0, y1 = y[i], y[i + 1]
    M0, M1 = M[i], M[i + 1]
    b = (y1 - y0) - (2.0 * M0 + M1) / 6.0
    out = y0 + b * h + M0 * (h * h) / 2.0 + (M1 - M0) * (h ** 3) / 6.0
    return np.moveaxis(out, 0, axis)


def op_spline_natural(ctrl, D, shape, nan_policy="nearest_valid", **_):
    """可分离自然边界三次样条（SExtractor 2.28.2 同族：y 向 natural BC 后 x 向 natural BC）。"""
    c, nbad = prefill_nan(ctrl, nan_policy)
    ny, nx = c.shape
    ys = _axis_positions(shape[0], ny, D)
    xs = _axis_positions(shape[1], nx, D)
    My = _nat_second_deriv(c, axis=0)
    tmp = _nat_eval(c, My, ys, axis=0)                 # (H, nx)
    Mx = _nat_second_deriv(tmp, axis=1)
    out = _nat_eval(tmp, Mx, xs, axis=1)               # (H, W)
    return out, nbad


def _median3_mesh(c, threshold=None, mode="nearest"):
    """控制点网格 3x3 中值滤波。

    threshold=None -> 无条件替换（photutils 2.2.0 filter_threshold=None 语义）；
    threshold=f    -> 仅当 |med - val| >= f 才替换（SExtractor 2.28.2 filterback 语义，
                      f=prefs.backfthresh，默认 0.0 = 实际无条件）。
    """
    med = ndimage.generic_filter(c, np.nanmedian, size=3, mode=mode)
    if threshold is None:
        return med
    return np.where(np.abs(med - c) >= threshold, med, c)


def op_sextractor_spline(ctrl, D, shape, nan_policy="nearest_valid", fthresh=0.0, **_):
    """SExtractor 2.28.2 语义：bad mesh 最近有效填充 -> mesh 中值滤波 -> 自然双三次样条。

    fthresh = prefs.backfthresh，默认 0.0（src/preflist.h:272 "*BACK_FILTTHRESH 0.0"），
    条件 |med−val| >= fthresh 在 0.0 下等价于无条件替换 ⇒ 滤波实际恒生效。
    """

    c, nbad = prefill_nan(ctrl, nan_policy)
    c = _median3_mesh(c, threshold=fthresh, mode="nearest")
    out, _ = op_spline_natural(c, D, shape, nan_policy="keep")
    return out, nbad


def _clip_to_ctrl_range(out, ctrl):
    """把重建场钳到**有效控制值的值域**（photutils clip=True 语义，interpolators.py:111-114）。

    这是把"光滑插值类在病态控制网格上失控（E 达 761–7.8e5）"压回有界的候选工程手段；
    本单元用 _clip 变体做**对照实验**验证该说法（results/exp04_e9_clip.json）。
    """
    lo = float(np.nanmin(ctrl))
    hi = float(np.nanmax(ctrl))
    return np.clip(out, lo, hi)


def op_sextractor_spline_clip(ctrl, D, shape, nan_policy="nearest_valid", fthresh=0.0, **_):
    """§4 推荐配置的**完整**语义：中值滤波 + 自然边界双三次样条 + 值域钳制。

    与 op_sextractor_spline 的唯一差别是最后一步钳制；两者的 E/超调对照见
    results/exp04_e9_clip.json（用于回答"推荐对象是否等于被测量对象"）。
    """
    c, nbad = prefill_nan(ctrl, nan_policy)
    c = _median3_mesh(c, threshold=fthresh, mode="nearest")
    out, _ = op_spline_natural(c, D, shape, nan_policy="keep")
    return _clip_to_ctrl_range(out, c), nbad


def op_spline_natural_clip(ctrl, D, shape, nan_policy="nearest_valid", **_):
    """自然边界双三次样条 + 值域钳制（对照：钳制能否救回未滤波样条的失控）。"""
    c, nbad = prefill_nan(ctrl, nan_policy)
    out, _ = op_spline_natural(c, D, shape, nan_policy="keep")
    return _clip_to_ctrl_range(out, c), nbad


def op_bicubic_cc_clip(ctrl, D, shape, nan_policy="nearest_valid", **_):
    """Keys 三次卷积 + 值域钳制（对照：钳制能否救回 bicubic_cc 的 761）。"""
    c, nbad = prefill_nan(ctrl, nan_policy)
    out, _ = op_bicubic_cc(c, D, shape, nan_policy="keep")
    return _clip_to_ctrl_range(out, c), nbad


def op_gpr_rbf_clip(ctrl, D, shape, nan_policy="nearest_valid", **kw):
    """局部 GPR（平方指数核）+ 值域钳制（对照：钳制能否救回 gpr_rbf 的 7.8e5）。"""
    c, nbad = prefill_nan(ctrl, nan_policy)
    out, _ = op_gpr(c, D, shape, nan_policy="keep", **kw)
    return _clip_to_ctrl_range(out, c), nbad


# ---------------------------------------------------------------------------
# F 天文主流实现语义
# ---------------------------------------------------------------------------
def op_photutils_zoom3(ctrl, D, shape, nan_policy="nearest_valid", order=3,
                       clip=True, filt=True, **_):
    """photutils 2.2.0 Background2D 默认语义：median3 -> scipy.ndimage.zoom(order=3,
    mode='reflect', grid_mode=True) -> clip 到输入值域（interpolators.py:96-115）。"""
    c, nbad = prefill_nan(ctrl, nan_policy)
    if filt:
        c = _median3_mesh(c, threshold=None, mode="nearest")
    if np.ptp(c) == 0:
        return np.full(shape, np.min(c), dtype=np.float64), nbad
    zf = (shape[0] / c.shape[0], shape[1] / c.shape[1])
    out = ndimage.zoom(c, zf, order=order, mode="reflect", grid_mode=True)
    out = out[:shape[0], :shape[1]]
    if out.shape != tuple(shape):      # 非整数倍时补齐（边缘 replicate）
        pad = [(0, shape[i] - out.shape[i]) for i in range(2)]
        out = np.pad(out, pad, mode="edge")
    if clip:
        np.clip(out, np.min(c), np.max(c), out=out)
    return out, nbad


def op_photutils_zoom1(ctrl, D, shape, nan_policy="nearest_valid", **_):
    """同上但 order=1（双线性），用于分离"中值滤波"与"样条阶数"两个因素。"""
    return op_photutils_zoom3(ctrl, D, shape, nan_policy=nan_policy, order=1)


def op_idw(ctrl, D, shape, nan_policy="keep", n_neighbors=10, power=1.0, reg=0.0, **_):
    """Shepard IDW（photutils utils/interpolation.py:12 语义：w=1/(d^power+reg)）。

    NaN 控制点按 photutils 语义直接丢弃（interpolators.py:183-185）。
    """
    c = np.asarray(ctrl, np.float64)
    ny, nx = c.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    good = np.isfinite(c)
    nbad = int((~good).sum())
    if good.sum() == 0:
        return np.full(shape, np.nan), nbad
    pts = np.column_stack([yy[good], xx[good]]).astype(np.float64)
    vals = c[good]
    tree = cKDTree(pts)
    ys = _axis_positions(shape[0], ny, D)
    xs = _axis_positions(shape[1], nx, D)
    qy, qx = np.meshgrid(ys, xs, indexing="ij")
    q = np.column_stack([qy.ravel(), qx.ravel()])
    k = int(min(n_neighbors, vals.size))
    d, idx = tree.query(q, k=k, workers=1)
    if k == 1:
        return vals[idx].reshape(shape), nbad
    d = np.atleast_2d(d.T).T if d.ndim == 1 else d
    idx = np.atleast_2d(idx.T).T if idx.ndim == 1 else idx
    with np.errstate(divide="ignore", invalid="ignore"):
        w = 1.0 / (np.power(d, power) + reg)
        exact = d[:, 0] <= 0.0
        out = (w * vals[idx]).sum(axis=1) / w.sum(axis=1)
    out[exact] = vals[idx[exact, 0]]
    return out.reshape(shape), nbad


# ---------------------------------------------------------------------------
# D/E 薄板样条 / RBF / GPR
# ---------------------------------------------------------------------------
def _ctrl_points(ctrl):
    c = np.asarray(ctrl, np.float64)
    ny, nx = c.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    good = np.isfinite(c)
    return np.column_stack([yy[good], xx[good]]).astype(np.float64), c[good], int((~good).sum())


def _rbf_reconstruct(ctrl, D, shape, kernel, smoothing=0.0, epsilon=None, **_):
    pts, vals, nbad = _ctrl_points(ctrl)
    if vals.size == 0:
        return np.full(shape, np.nan), nbad
    kw = {}
    if epsilon is not None:
        kw["epsilon"] = float(epsilon)
    if vals.size < 4:
        return op_bilinear(ctrl, D, shape)
    itp = RBFInterpolator(pts, vals, kernel=kernel, smoothing=float(smoothing),
                          epsilon=kw.get("epsilon"), neighbors=None)
    ny, nx = np.asarray(ctrl).shape
    ys = _axis_positions(shape[0], ny, D)
    xs = _axis_positions(shape[1], nx, D)
    qy, qx = np.meshgrid(ys, xs, indexing="ij")
    q = np.column_stack([qy.ravel(), qx.ravel()])
    out = itp(q)                      # RBFInterpolator 内部按 chunk 评估，避免大矩阵
    return out.reshape(shape), nbad


def op_tps(ctrl, D, shape, **_):
    """薄板样条精确插值（r^2 log r 核，smoothing=0）。"""
    return _rbf_reconstruct(ctrl, D, shape, kernel="thin_plate_spline", smoothing=0.0)


def op_tps_smooth(ctrl, D, shape, smoothing=1.0, **_):
    """薄板样条平滑（smoothing>0），用于压制控制点估计噪声。"""
    return _rbf_reconstruct(ctrl, D, shape, kernel="thin_plate_spline", smoothing=smoothing)


def op_rbf_mq(ctrl, D, shape, epsilon=1.0, **_):
    return _rbf_reconstruct(ctrl, D, shape, kernel="multiquadric", epsilon=epsilon)


def op_rbf_gauss(ctrl, D, shape, epsilon=1.0, **_):
    return _rbf_reconstruct(ctrl, D, shape, kernel="gaussian", epsilon=epsilon)


def _kernel_matrix(d2, kernel, ell):
    """局部 GPR 核：输入平方距离 d2（cell 单位）。返回协方差。"""
    if kernel == "rbf":                      # 平方指数
        return np.exp(-0.5 * d2 / (ell * ell))
    if kernel == "matern32":
        r = np.sqrt(np.maximum(d2, 0.0)) / ell
        s = np.sqrt(3.0) * r
        return (1.0 + s) * np.exp(-s)
    if kernel == "matern52":
        r = np.sqrt(np.maximum(d2, 0.0)) / ell
        s = np.sqrt(5.0) * r
        return (1.0 + s + s * s / 3.0) * np.exp(-s)
    if kernel == "exp":                      # 指数（Matérn-1/2）
        r = np.sqrt(np.maximum(d2, 0.0)) / ell
        return np.exp(-r)
    raise ValueError("unknown GPR kernel %r" % (kernel,))


def op_gpr(ctrl, D, shape, kernel="rbf", ell=1.5, sigma_n=0.0, win=4,
           nan_policy="nearest_valid", **_):
    """局部窗口高斯过程回归（后验均值）。

    ell      : 相关长度（cell 单位）
    sigma_n  : 控制点值噪声标准差（同单位；0 = 无噪插值）
    win      : 局部窗口半径（cell 数），窗口 K=(2*win+1)^2 个控制点

    算法（O(H*W*K)，与 N 无关）：
      ① 控制点网格按 edge 复制 pad R 圈，使所有 cell 的邻域偏移一致；
      ② 解 (Kgg + sigma_n^2 I) alpha = Y（cell 无关，只解一次 KxK）；
      ③ 预测：out[cell, 像素内偏移] = k*(偏移) · alpha[cell]。
    cell 内偏移只有 D^2 种，故 Kstar 为 (D^2, K)，一次矩阵乘即得全部像素。
    """
    c, nbad = prefill_nan(ctrl, nan_policy)
    ny, nx = c.shape
    R = int(win)
    ys = np.arange(-R, R + 1)
    gy, gx = np.meshgrid(ys, ys, indexing="ij")
    gpts = np.column_stack([gy.ravel(), gx.ravel()]).astype(np.float64)
    Kgg = _kernel_matrix(((gpts[:, None, :] - gpts[None, :, :]) ** 2).sum(-1), kernel, ell)
    # 数值抖动（GPR 标准做法）：无噪精确插值时核矩阵近奇异，jitter 保证可解且平坦场可复现
    #    抖动取 1e-6*k(0)：光滑核在规则网格上精确插值时核矩阵条件数 ~1e7-1e14，
    #    float64 下必须加 nugget 才能保证平坦场可复现（量级见 selftest S1）
    jit = 1e-6 * float(np.mean(np.diag(Kgg))) if sigma_n <= 0 else 0.0
    Ky = Kgg + ((sigma_n ** 2) + jit) * np.eye(Kgg.shape[0])
    Ky_inv = np.linalg.inv(Ky)

    # ① pad + 邻域矩阵 A: (ny*nx, K)
    cp = np.pad(c, R, mode="edge")
    K = (2 * R + 1) ** 2
    A = np.empty((ny * nx, K), np.float64)
    for i in range(2 * R + 1):
        for j in range(2 * R + 1):
            A[:, i * (2 * R + 1) + j] = cp[i:i + ny, j:j + nx].ravel()
    # 局部常数均值（simple kriging with local mean）：零先验均值的 GP 在窗口截断下
    # 对常数场有虚假偏移，扣局部均值后残差做 GP，再加回均值 —— 常数场逐位精确复现
    mu = A.mean(axis=1, keepdims=True)         # (ncell, 1)
    alpha = (A - mu) @ Ky_inv.T                # (ncell, K)

    # ② cell 内偏移（cell 单位）：像素 (cy*D+my, cx*D+mx) 的偏移
    #    dyv[a] = (a + 0.5)/D - 0.5，a = 0..D-1（与 _axis_positions 的控制点中心约定一致）
    H, W = int(shape[0]), int(shape[1])
    dyv = (np.arange(D, dtype=np.float64) + 0.5) / D - 0.5
    gyf = gy.ravel().astype(np.float64)        # (K,) 邻域行偏移
    gxf = gx.ravel().astype(np.float64)        # (K,) 邻域列偏移
    # 逐 my 行构造 Kstar (D, K)，避免一次性 D^2 x K 的中间量过大
    pred = np.empty((D * D, ny * nx), np.float64)
    for a in range(D):
        d2row = (dyv[a] - gyf)[None, :] ** 2 + (dyv[:, None] - gxf[None, :]) ** 2   # (D, K)
        pred[a * D:(a + 1) * D, :] = _kernel_matrix(d2row, kernel, ell) @ alpha.T + mu.T
    # ③ 像素 -> (cell, 像素内偏移)
    py = np.arange(H); px = np.arange(W)
    cy = np.minimum(py // D, ny - 1); cx = np.minimum(px // D, nx - 1)
    oy = np.minimum(py % D, D - 1); ox = np.minimum(px % D, D - 1)
    flat = (oy[:, None] * D + ox[None, :]) * (ny * nx) + (cy[:, None] * nx + cx[None, :])
    out = pred.ravel()[flat]
    return out, nbad


# ---------------------------------------------------------------------------
# 算子注册表
# ---------------------------------------------------------------------------
OPERATORS = {
    # name: (callable, 类别, 是否重算子(全局稠密核), 默认 kwargs)
    "nn":                (op_nn,                "A_nearest",  False, {}),
    "bilinear":          (op_bilinear,          "B_local_poly", False, {}),
    "bilinear_prod":     (op_bilinear_prod,     "B_local_poly", False, {}),
    "sextractor_spline_clip": (op_sextractor_spline_clip, "C_spline_clip", False, {}),
    "spline_natural_clip":    (op_spline_natural_clip,    "C_spline_clip", False, {}),
    "bicubic_cc_clip":        (op_bicubic_cc_clip,        "B_local_poly_clip", False, {}),
    "gpr_rbf_clip":           (op_gpr_rbf_clip,           "E_gpr_clip", False, {}),
    "bicubic_cc":        (op_bicubic_cc,        "B_local_poly", False, {}),
    "spline_natural":    (op_spline_natural,    "C_spline",   False, {}),
    "sextractor_spline": (op_sextractor_spline, "F_astro_impl", False, {}),
    "photutils_zoom3":   (op_photutils_zoom3,   "F_astro_impl", False, {}),
    "photutils_zoom1":   (op_photutils_zoom1,   "F_astro_impl", False, {}),
    "idw":               (op_idw,               "F_astro_impl", False, {}),
    "tps":               (op_tps,               "D_smooth_spline", True, {}),
    "tps_smooth":        (op_tps_smooth,        "D_smooth_spline", True, {"smoothing": 1.0}),
    "rbf_mq":            (op_rbf_mq,            "E_rbf_gpr",   True, {"epsilon": 1.0}),
    "rbf_gauss":         (op_rbf_gauss,         "E_rbf_gpr",   True, {"epsilon": 1.0}),
    "gpr_rbf":           (op_gpr,               "E_rbf_gpr",   False, {"kernel": "rbf"}),
    "gpr_matern32":      (op_gpr,               "E_rbf_gpr",   False, {"kernel": "matern32"}),
    "gpr_exp":           (op_gpr,               "E_rbf_gpr",   False, {"kernel": "exp"}),
}

# 主对比集（同输入同判据的核心集：全部算子都跑）
PRIMARY_OPS = ["nn", "bilinear", "bicubic_cc", "spline_natural", "sextractor_spline",
               "photutils_zoom3", "photutils_zoom1", "idw", "tps", "tps_smooth",
               "rbf_mq", "rbf_gauss", "gpr_rbf", "gpr_matern32", "gpr_exp"]
# 扩展集（全 (ℓ,s,Δ) 网格用；只含 O(N) 局部算子，重算子单独报标度）
CHEAP_OPS = ["nn", "bilinear", "bicubic_cc", "spline_natural", "sextractor_spline",
             "photutils_zoom3", "photutils_zoom1", "idw", "gpr_rbf", "gpr_matern32", "gpr_exp"]


def run_operator(name, ctrl, D, shape, **kw):
    """按注册表调用算子，返回 (dense, n_nan_ctrl_in, wall_s)。

    n_nan_ctrl_in = **输入**控制点中的 NaN 个数（算子内部如何填充不影响该计数）。
    """
    import time
    fn, _cls, _heavy, dflt = OPERATORS[name]
    kw2 = dict(dflt)
    kw2.update(kw)
    nbad_in = int((~np.isfinite(np.asarray(ctrl, np.float64))).sum())
    t0 = time.perf_counter()
    out, _nbad = fn(ctrl, D, shape, **kw2)
    return np.asarray(out, np.float64), nbad_in, time.perf_counter() - t0
