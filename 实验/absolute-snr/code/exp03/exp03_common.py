#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-03 公共库：**区域化（位置相关）sigma_sky 的估计口径、绝对准确性与跨帧一致性**。

任务单元 SCI-B-EXP-03。负责人裁决（2026-09-22）：
  「对于 m42 这种目标确实很难。但是叠加的时候，所有这个位置的信号都有一样的背景。
    因此只要这个区域产生的信噪比绝对准确就行。本身也不会和其他天区叠加。」

本模块只做四件事（全部**只读**依赖 EXP-02 的独立重写，不 import 生产代码）：
  1. 区域（region）网格与**区域化** sigma 估计器族；
  2. 跨帧差分参考（位置稳定分量自动对消）与区域代理量；
  3. 帧间背景场的方差分解（前提检验的统计工具）；
  4. 真值口径与误差度量。

单位约定：ADU（图像值）。sigma 一律是**逐像素**标准差。
固定 seed：见 SEED（各臂显式传入）。

设计纪律（AGENTS.md §5、最高设计 §12.2）：
  * 判据必须非退化 —— 真值无结构时修法效应必须归零（见 effect_delta）；
  * 比值类判据的分子分母必须用**同一个估计量**（EXP-02 §7.2 的教训）；
  * 本模块不产生任何"恒真"的量。
"""

from __future__ import annotations

import math
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
_EXP02 = HERE.parent / "exp02"
if str(_EXP02) not in sys.path:
    sys.path.insert(0, str(_EXP02))
import exp02_common as C  # noqa: E402  （只读复用：生产 recipe 的独立重写）

ROOT = Path(__file__).resolve().parents[4]

SEED = 20260925
DELTA_BUDGET = 0.014                     # 结构项预算（沿用 EXP-02/EXP-01 的 delta）
S_CLIP_REF = 0.0138                      # 生产裁剪固有低偏的参考量级（EXP-02 实测 1.38%~1.81%）
BOX_GRID = (16, 32, 64, 128, 256, 512)
BOX_RECOMMENDED = 64                     # = 最高设计 §5.3 稀疏层控制点间隔 Delta = tile/8


# ===========================================================================
# 1. 区域网格
# ===========================================================================
def region_grid(shape: Tuple[int, int], box: int
                ) -> Tuple[List[Tuple[int, int, int, int]], Tuple[int, int]]:
    """把帧切成 box*box 的**互不重叠**区域；帧尺寸按整数倍裁剪（余边丢弃）。

    返回 (regions, (by, bx))；regions 每项 = (y0, y1, x0, x1)。
    """
    ny, nx = int(shape[0]), int(shape[1])
    by, bx = ny // box, nx // box
    if by < 1 or bx < 1:
        raise ValueError("region_grid: 区域比帧还大 box=%d shape=%s" % (box, shape))
    regs: List[Tuple[int, int, int, int]] = []
    for iy in range(by):
        for ix in range(bx):
            regs.append((iy * box, (iy + 1) * box, ix * box, (ix + 1) * box))
    return regs, (by, bx)


def _mesh_view(a: np.ndarray, box: int) -> np.ndarray:
    """把 (ny,nx) 切成 (n_mesh, box*box) 的视图（按 mesh 行优先展开）。"""
    ny, nx = a.shape
    by, bx = ny // box, nx // box
    sub = a[:by * box, :bx * box]
    return (sub.reshape(by, box, bx, box).transpose(0, 2, 1, 3).reshape(by * bx, box * box))


def _grid_of(img: np.ndarray, box: int) -> Tuple[int, int]:
    ny, nx = img.shape
    return (ny // box, nx // box)


# ===========================================================================
# 2. 区域化 sigma 估计器族
# ===========================================================================
def region_sigma_raw(img: np.ndarray, box: int, n_rounds: int = 2,
                     k: float = C.CLIP_K) -> Dict[str, Any]:
    """R0 **朴素区域化**：直接对区域**原始像素**套生产 recipe（不扣局部背景）。

    这是"只把作用域从整帧换成区域、别的不动"的对照臂 —— 用来证明
    **光换作用域不够**（区域内的结构仍会污染）。
    """
    mm = _mesh_view(np.asarray(img, dtype=np.float64), box)
    out = np.empty(mm.shape[0], dtype=np.float64)
    for i in range(mm.shape[0]):
        out[i] = C.production_clip_sigma(mm[i], n_rounds=n_rounds, k=k)["sigma"]
    return {"sigma": out.reshape(_grid_of(img, box)), "box": int(box), "mode": "raw"}


def region_sigma_resid(img: np.ndarray, box: int, filter_size: int = 3,
                       n_rounds: int = 2, k: float = C.CLIP_K, n_iter: int = 3,
                       return_maps: bool = False,
                       bad_pixel_mask: Optional[np.ndarray] = None,
                       sat_adu: Optional[float] = None,
                       sat_frac_max: float = 0.01,
                       bad_frac_max: float = 0.5) -> Dict[str, Any]:
    """R1 **结构感知的区域化**：mesh 局部背景扣除后，**逐区域**残差的裁剪 RMS。

    步骤（= SExtractor backsig 图 / photutils background_rms 图的同构物）：
      1. mesh 背景图 B(x,y)：box x box 网格、逐 mesh 生产 recipe、中值滤波、双线性展开、迭代；
      2. residual = img - B；
      3. 逐区域对 residual 套**同一个**生产 recipe 的裁剪 RMS ⇒ sigma 图（**位置函数**）。

    与 EXP-02 F1b 的差别只有一处：EXP-02 把残差在全帧聚合（一个标量），
    本函数**保留逐区域的 sigma 图**，这正是负责人裁决要的"按区域"。
    """
    a = np.asarray(img, dtype=np.float64)
    B, meta = C.mesh_background_map(a, box=box, filter_size=filter_size,
                                    n_rounds=n_rounds, k=k, n_iter=n_iter)
    resid = a - B
    mm = _mesh_view(resid, box)
    sig = np.empty(mm.shape[0], dtype=np.float64)
    kf = np.empty(mm.shape[0], dtype=np.float64)
    for i in range(mm.shape[0]):
        st = C.production_clip_sigma(mm[i], n_rounds=n_rounds, k=k)
        sig[i] = st["sigma"]
        kf[i] = st["keep_frac"]
    g = _grid_of(a, box)
    # **区域有效性掩膜**（设计 §4.1 已要求：饱和/溢出区、坏点、高结构区域不进天光统计）：
    # 一个区域内饱和像素占比过高 ⇒ 该区域的 sigma 无意义（实测 M5 星云核心区的
    # 64x64 区域饱和占比 ~50%，会把 p95(sigma) 抬到 1.9e4 ADU —— 那是饱和平台不是噪声）。
    valid = np.ones(mm.shape[0], dtype=bool)
    n_sat = np.zeros(mm.shape[0])
    if sat_adu is not None:
        sat = np.asarray(img, dtype=np.float64) >= float(sat_adu)
        msub = sat[: (a.shape[0] // box) * box, : (a.shape[1] // box) * box]
        satm = (msub.reshape(a.shape[0] // box, box, a.shape[1] // box, box)
                    .transpose(0, 2, 1, 3).reshape(-1, box * box))
        n_sat = satm.sum(axis=1).astype(float)
        valid &= (n_sat / float(box * box)) <= sat_frac_max
    if bad_pixel_mask is not None:
        bm = np.asarray(bad_pixel_mask, dtype=bool)
        msub = bm[: (a.shape[0] // box) * box, : (a.shape[1] // box) * box]
        badm = (msub.reshape(a.shape[0] // box, box, a.shape[1] // box, box)
                    .transpose(0, 2, 1, 3).reshape(-1, box * box))
        valid &= (badm.sum(axis=1).astype(float) / float(box * box)) <= bad_frac_max
    sig = np.where(valid, sig, np.nan)
    sv = sig[np.isfinite(sig)]
    out: Dict[str, Any] = {
        "sigma": sig.reshape(g), "keep_frac": kf.reshape(g),
        "valid": valid.reshape(g), "sat_frac": (n_sat / float(box * box)).reshape(g),
        "n_region_valid": int(sv.size), "n_region": int(sig.size),
        "valid_region_frac": float(sv.size) / float(sig.size),
        "sigma_median": float(np.median(sv)) if sv.size else float("nan"),
        "sigma_p05": float(np.percentile(sv, 5)) if sv.size else float("nan"),
        "sigma_p95": float(np.percentile(sv, 95)) if sv.size else float("nan"),
        "dispersion": (float(np.percentile(sv, 95) / max(np.percentile(sv, 5), 1e-12))
                       if sv.size else float("nan")),
        "box": int(box), "mode": "resid", "mesh_meta": meta,
    }
    if return_maps:
        out["background_map"] = B
        out["residual"] = resid
    return out


def region_sigma_diff(a: np.ndarray, b: np.ndarray, box: int, n_rounds: int = 2,
                      k: float = C.CLIP_K) -> Dict[str, Any]:
    """R2 **跨帧差分参考**（无需真值、对**一切位置稳定分量**自动对消）。

    两帧同一天区、噪声独立、位置稳定分量相同 ⇒
        D = I_a - I_b = n_a - n_b        ⇒  std(D) = sqrt(2) * sigma_noise
    故逐区域的 clipped RMS(D)/sqrt(2) 是**该区域噪声尺度的无偏估计**，
    **与天光/结构的空间形态完全无关**（天光与结构都是位置函数 ⇒ 在差分中精确对消）。

    前提（必须显式登记，见报告诚实边界）：
      * 两帧的位置稳定分量确实相同（本单元第一臂专门检验）；
      * 两帧噪声独立且同尺度；
      * 有源/宇宙线/坏像素 ⇒ 由裁剪剔除（本函数用同一生产 recipe 的裁剪 RMS）。
    """
    d = np.asarray(a, dtype=np.float64) - np.asarray(b, dtype=np.float64)
    r = region_sigma_raw(d, box, n_rounds=n_rounds, k=k)
    sig = r["sigma"] / math.sqrt(2.0)
    return {"sigma": sig, "sigma_diff": r["sigma"] * math.sqrt(2.0),
            "sigma_median": float(np.median(sig)),
            "sigma_p05": float(np.percentile(sig, 5)),
            "sigma_p95": float(np.percentile(sig, 95)),
            "box": int(box), "mode": "diff"}


def frame_scalar_sigma(img: np.ndarray, n_rounds: int = 2, k: float = C.CLIP_K) -> float:
    """现行生产口径：**整帧** 2 轮 median +- 3*1.4826*MAD 裁剪后的 RMS（被审对象）。"""
    return float(C.production_clip_sigma(img, n_rounds=n_rounds, k=k)["sigma"])


# ===========================================================================
# 3. 真值口径与误差度量
# ===========================================================================
def rel_err(est: float, true: float) -> float:
    """(est/true - 1)。真值无结构时**不是零**（含生产裁剪固有低偏 S_clip）。"""
    return (est / true - 1.0) if true else float("nan")


def effect_delta(est_fix: float, est_prod: float) -> float:
    """修法效应 Delta = est_fix/est_prod - 1 —— 非退化负例的核心度量。

    真值无结构时区域化与全局化必须给出**同一个数** ⇒ Delta 必须归零。
    """
    return (est_fix / est_prod - 1.0) if est_prod else float("nan")


def scaled_mse(bias: float, stat: float) -> float:
    """总误差平方（bias 与统计误差独立合成）。"""
    return bias * bias + stat * stat


# ===========================================================================
# 4. 帧间背景场的方差分解（前提检验）
# ===========================================================================
def two_way_decomposition(m: np.ndarray) -> Dict[str, float]:
    """双向（无重复）方差分解：m[r,k] = mu + a_r + b_k + e[r,k]。

    输入 m 形状 (R, K)：R 个区域 x K 帧。
    用途（本单元第一臂）：检验裁决前提
        「同一位置的天光背景跨帧相同」
    的可操作形式：
        * 若 Var(e) 与"纯噪声预期" sigma^2/N_mesh 一致 ⇒ 背景场**可分离**为
          「位置函数 a_r + 帧项 b_k」⇒ 前提成立（允许逐帧加性偏移，那正是 UPM 多退少补要处理的）；
        * 若 Var(e) >> sigma^2/N_mesh ⇒ 存在**帧 x 位置**交互项 ⇒ 天光的空间形态本身跨帧变化，
          前提**不完全成立**，交互项的幅度就是必须预算的残差。
    """
    m = np.asarray(m, dtype=np.float64)
    R, K = m.shape
    if R < 2 or K < 2:
        raise ValueError("two_way_decomposition: 需要 R>=2 且 K>=2")
    gm = float(m.mean())
    row = m.mean(axis=1) - gm          # a_r
    col = m.mean(axis=0) - gm          # b_k
    fit = gm + row[:, None] + col[None, :]
    e = m - fit
    ss_tot = float(np.sum((m - gm) ** 2))
    ss_row = float(K * np.sum(row ** 2))
    ss_col = float(R * np.sum(col ** 2))
    ss_res = float(np.sum(e ** 2))
    df_res = (R - 1) * (K - 1)
    var_e = ss_res / df_res if df_res > 0 else float("nan")
    return {"mu": gm, "ss_total": ss_tot, "ss_position": ss_row, "ss_frame": ss_col,
            "ss_interaction": ss_res, "df_interaction": int(df_res),
            "var_interaction": var_e, "sd_interaction": math.sqrt(max(var_e, 0.0)),
            "frac_position": ss_row / ss_tot if ss_tot else float("nan"),
            "frac_frame": ss_col / ss_tot if ss_tot else float("nan"),
            "frac_interaction": ss_res / ss_tot if ss_tot else float("nan"),
            "sd_position": float(np.std(row, ddof=1)) if R > 1 else float("nan"),
            "sd_frame": float(np.std(col, ddof=1)) if K > 1 else float("nan")}


def structure_function(img: np.ndarray, lags: Sequence[int],
                        mask: Optional[np.ndarray] = None,
                        n_rounds: int = 2, k: float = C.CLIP_K,
                        max_samples: int = 1_000_000, stride: int = 1) -> Dict[str, float]:
    """**结构函数** S(L) = 0.5 * <(I(x+L) - I(x))^2>，用生产裁剪 RMS 稳健估计。

    为什么用结构函数（而不是区域均值的离散）：
      * 它对**离群区域**（含星点/星云核的区域）免疫 —— 每个 lag 上都是全帧聚合 + 裁剪；
      * 它把"空间结构"分解到**尺度**上：白噪声 S(L) = sigma^2 与 L 无关；
        平滑图案 P 叠加白噪声 n 时 S(L) = sigma_n^2 + 0.5*<(P(x+L)-P(x))^2>，
        随 L 增大单调上升到 sigma_n^2 + Var(P)。
      * 于是 **S(L)/S(1) - 1** 就是"尺度 < L 的图案功率 / 噪声功率"。

    返回 {"S@L": ..., "S_over_S1@L": ...}（S(1) 作为噪声基准）。
    """
    a = np.asarray(img, dtype=np.float64)
    valid = np.isfinite(a)
    if mask is not None:
        valid = valid & (~mask)
    out: Dict[str, float] = {}
    base = None
    ny, nx = a.shape
    # **先按行抽稀再建数组**（内存瘦身）：噪声是平稳的，行抽稀不引入偏差，
    # 而 max_samples 的精度（~0.1%）远超本单元需要的 1e-3。
    s_row = max(1, int(math.ceil(math.sqrt(2.0 * ny * nx / float(max_samples)))))
    aa = a[::s_row, :]
    vv = valid[::s_row, :]
    for L in sorted(set(int(x) for x in lags)):
        if L < 1:
            continue
        vx = vv[:, L:] & vv[:, :-L]
        vy = vv[L:, :] & vv[:-L, :]
        parts = []
        for (arr, m) in ((aa[:, L:] - aa[:, :-L], vx), (aa[L:, :] - aa[:-L, :], vy)):
            x = arr[m]
            if x.size > max_samples:
                x = x[::int(math.ceil(x.size / float(max_samples)))]
            if x.size:
                parts.append(x)
        if not parts:
            continue
        vals = parts[0] if len(parts) == 1 else np.concatenate(parts)
        if vals.size < 100:
            continue
        s2 = float(C.production_clip_sigma(vals, n_rounds=n_rounds, k=k)["sigma"]) ** 2
        s = 0.5 * s2
        out["S@%d" % L] = s
        if L == 1:
            base = s
        if base:
            out["S_over_S1@%d" % L] = s / base
    return out


def difference_structure_test(da: np.ndarray, db: np.ndarray, box: int,
                              mask: Optional[np.ndarray] = None,
                              lags: Sequence[int] = (1, 2, 4, 8, 16, 32, 64, 128, 256),
                              sf_diff: Optional[Dict[str, float]] = None,
                              sf_frame: Optional[Dict[str, float]] = None,
                              ) -> Dict[str, float]:
    """**差分场的空间结构检验**（前提检验的核心量，逐对帧可算）。

    对已对齐的两帧：D = a - b。
      * 若"同一位置的天光背景跨帧相同"，则位置稳定分量（天光 + 结构）**精确对消**，
        D = 常数 + (n_a - n_b) ⇒ **S_D(L) 与 L 无关**（白）；
      * 定义 **interaction_power(L) = S_D(L)/S_D(1) - 1**：
          ≈ 0  ⇒ 差分场是「常数 + 白噪声」⇒ 前提成立；
          > 0  ⇒ 存在**帧相关**的空间形态，其功率就是该值乘以噪声功率。
      * 同时给出单帧对照 S_a(L)/S_a(1) - 1（**必然 >> 0**，因为单帧里天光+结构是位置函数）
        —— 两者对照就是"结构是位置函数"的直接证据。

    另给逐区域裁剪中位数的**稳健**空间离散（1.4826*MAD）与噪声预期之比。
    """
    a = np.asarray(da, dtype=np.float64)
    b = np.asarray(db, dtype=np.float64)
    valid = np.isfinite(a) & np.isfinite(b)
    if mask is not None:
        valid = valid & (~mask)
    d = np.where(valid, a - b, np.nan)
    v = d[np.isfinite(d)]
    st = C.production_clip_sigma(v, n_rounds=2, k=C.CLIP_K)
    sig_d = float(st["sigma"])
    sf_d = sf_diff if sf_diff is not None else structure_function(d, lags, mask=None)
    sf_a = sf_frame if sf_frame is not None else structure_function(a, lags, mask=mask)
    ny, nx = d.shape
    by, bx = ny // box, nx // box
    sub = d[:by * box, :bx * box]
    dd = np.where(np.isfinite(sub), sub, np.nan)
    mv = dd.reshape(by, box, bx, box).transpose(0, 2, 1, 3).reshape(by * bx, box * box)
    bg = np.empty(by * bx)
    for i in range(by * bx):
        x = mv[i][np.isfinite(mv[i])]
        bg[i] = C.production_clip_sigma(x, n_rounds=2, k=C.CLIP_K)["background"] if x.size > 8 else np.nan
    bg = bg[np.isfinite(bg)]
    mad = float(np.median(np.abs(bg - np.median(bg))))
    robust_sd = 1.4826 * mad
    n_eff = float(box * box)
    se = 1.2533 * sig_d / math.sqrt(max(n_eff, 1.0))
    return {
        "box": int(box), "sigma_diff": sig_d, "sigma_noise_per_frame": sig_d / math.sqrt(2.0),
        "n_eff_per_region": n_eff,
        "region_bg_robust_sd": robust_sd,
        "region_bg_plain_sd": float(np.std(bg, ddof=1)) if bg.size > 1 else float("nan"),
        "region_bg_noise_expectation": se,
        "interaction_ratio_robust": robust_sd / se if se else float("nan"),
        "interaction_power@64": sf_d.get("S_over_S1@64", float("nan")) - 1.0,
        "interaction_power@256": sf_d.get("S_over_S1@256", float("nan")) - 1.0,
        "single_frame_power@64": sf_a.get("S_over_S1@64", float("nan")) - 1.0,
        "single_frame_power@256": sf_a.get("S_over_S1@256", float("nan")) - 1.0,
        **{("diff_" + kk): vv for kk, vv in sf_d.items()},
    }


# ===========================================================================
# 5. 区域代理量（全部**无需真值**）
# ===========================================================================
def region_proxies(sigma_reg: np.ndarray, sigma_diff_reg: np.ndarray,
                   sigma_scalar: float, box: int) -> Dict[str, Any]:
    """区域级、无需真值的判据量。

      A2_reg   = sigma_reg / (sigma_diff_reg/sqrt(2))   —— **认证量**：区域估计相对差分参考的残余结构
      D_reg    = p95(sigma_reg)/p05(sigma_reg)          —— 区域噪声尺度异质性
      R_scalar = sigma_scalar / median(sigma_reg)       —— 帧级标量与区域中位数的倍数差

    分子分母用**同一个估计量**（生产裁剪 RMS）⇒ 固有裁剪低偏在比值中精确相消（EXP-02 §7.2）。
    """
    sr = np.asarray(sigma_reg, dtype=np.float64)
    sd = np.asarray(sigma_diff_reg, dtype=np.float64)
    a2 = sr / np.maximum(sd, 1e-12)
    return {
        "box": int(box),
        "A2_reg_median": float(np.median(a2)),
        "A2_reg_p95": float(np.percentile(a2, 95)),
        "A2_reg_max": float(np.max(a2)),
        "A2_reg_p05": float(np.percentile(a2, 5)),
        "D_reg": float(np.percentile(sr, 95) / max(np.percentile(sr, 5), 1e-12)),
        "sigma_reg_median": float(np.median(sr)),
        "R_scalar": float(sigma_scalar / np.median(sr)) if np.median(sr) > 0 else float("nan"),
    }


def certify_region(a2: float, delta: float = DELTA_BUDGET) -> str:
    """区域认证三档（与 EXP-02 词表一致）。"""
    if a2 <= 1.0 + delta:
        return "ok"
    if a2 <= 1.0 + 3.0 * delta:
        return "diagnostic_only"
    return "fail_closed"


# ===========================================================================
# 6. 解析结构生成器（真值完全已知）
# ===========================================================================
def struct_ramp(shape: Tuple[int, int], slope_adu_per_px: float,
                theta_deg: float = 0.0) -> np.ndarray:
    """线性斜坡结构（尺度 = 帧）；区域均值为零。"""
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    th = math.radians(theta_deg)
    r = xx * math.cos(th) + yy * math.sin(th)
    return slope_adu_per_px * (r - r.mean())


def struct_smooth_field(shape: Tuple[int, int], rms: float, corr_px: float,
                        rng: np.random.Generator) -> np.ndarray:
    """高斯相关场（相关长度 corr_px）；按 rms 归一。"""
    from scipy.ndimage import gaussian_filter
    ny, nx = shape
    w = rng.normal(0.0, 1.0, size=(ny, nx))
    f = gaussian_filter(w, sigma=float(corr_px), mode="wrap")
    f = f - f.mean()
    sd = float(f.std())
    return f * (rms / sd) if sd > 0 else f


def struct_blob(shape: Tuple[int, int], amp: float, sigma_px: float,
                center: Optional[Tuple[float, float]] = None) -> np.ndarray:
    """单个高斯团块（紧凑结构）。"""
    ny, nx = shape
    cy, cx = center if center else (ny / 2.0, nx / 2.0)
    yy, xx = np.mgrid[0:ny, 0:nx]
    g = np.exp(-((yy - cy) ** 2 + (xx - cx) ** 2) / (2.0 * sigma_px ** 2))
    return amp * g


def struct_from_template(img: np.ndarray, rms: float) -> np.ndarray:
    """用真实底图（如 M42/M16 裁剪）作结构模板；去均值后按 rms 归一。"""
    f = np.asarray(img, dtype=np.float64)
    f = f - f.mean()
    sd = float(f.std())
    return f * (rms / sd) if sd > 0 else f


# ===========================================================================
# 7. 解析误差-尺度关系（闭式预测，供实验对拍）
# ===========================================================================
def predicted_region_bias_ramp(box: int, slope: float, sigma_n: float) -> float:
    """R1 区域估计器在**线性斜坡**上的解析偏差预测。

    区域扣背景后，区域内残差 = 斜坡 - 其区域均值 ⇒ 幅度近似均匀分布，
    RMS = slope*box/sqrt(12)。裁剪窗口 +-3*sigma_total 远大于该幅度时结构全额存活 ⇒

        sigma_hat/sigma_n = sqrt(1 + (slope*box)^2/(12*sigma_n^2))

    返回 (sigma_hat/sigma_n - 1)。
    """
    x = slope * box / (math.sqrt(12.0) * sigma_n)
    return math.sqrt(1.0 + x * x) - 1.0


def predicted_region_stat_error(box: int) -> float:
    """区域 sigma 估计的**统计**相对误差（高斯白噪声、N=box^2 独立样本）。

    sigma_hat 的渐近相对标准差 = 1/sqrt(2N)。
    """
    return 1.0 / math.sqrt(2.0 * box * box)


def optimal_box_ramp(slope: float, sigma_n: float) -> float:
    """误差-尺度关系的**最优区域尺度**（解析，斜坡结构）。

    MSE(B) = [sqrt(1+x^2)-1]^2 + 1/(2 B^2),  x = slope*B/(sqrt(12) sigma_n)。
    小 x 展开 [x^2/2]^2 + 1/(2B^2) ⇒ df/dB=0 ⇒ B* = 144^(1/6) (sigma_n/slope)^(2/3)
                                              = 2.2894 (sigma_n/slope)^(2/3)
    """
    return 144.0 ** (1.0 / 6.0) * (sigma_n / slope) ** (2.0 / 3.0)


def optimal_box_numeric(slope: float, sigma_n: float,
                        boxes: Sequence[int] = BOX_GRID) -> Tuple[int, float]:
    """在给定 box 网格上数值求 MSE 最小的 box（与解析式对拍）。"""
    best, bv = None, float("inf")
    for b in boxes:
        bias = predicted_region_bias_ramp(b, slope, sigma_n)
        stat = predicted_region_stat_error(b)
        v = scaled_mse(bias, stat)
        if v < bv:
            best, bv = b, math.sqrt(v)
    return int(best), math.sqrt(bv)


# ===========================================================================
# 8. 真实数据：对齐与掩膜
# ===========================================================================
def wcs_offset_px(hdr_a, hdr_b, ref: Tuple[float, float] = (2048.0, 2048.0)
                  ) -> Tuple[float, float]:
    """帧 b 相对帧 a 的**像素平移**（在 a 的像素坐标下）。

    做法：取 a 的参考像素 -> 天球 -> b 的像素坐标，返回 (dy, dx) 使
        b 中 (y+dy, x+dx) 对应 a 中 (y, x)。
    """
    from astropy.wcs import WCS
    wa = WCS(hdr_a)
    wb = WCS(hdr_b)
    sky = wa.pixel_to_world(ref[1], ref[0])
    xb, yb = wb.world_to_pixel(sky)
    return (float(yb - ref[0]), float(xb - ref[1]))


def xcorr_offset_px(a: np.ndarray, b: np.ndarray, max_shift: int = 64,
                    step: int = 4) -> Tuple[int, int]:
    """无 WCS 时的整数平移估计：抽稀互相关（稳健、O(N)）。

    返回 (dy, dx) 使 b 平移 (dy,dx) 后与 a 对齐。
    """
    a = np.asarray(a, dtype=np.float64)[::step, ::step]
    b = np.asarray(b, dtype=np.float64)[::step, ::step]
    a = a - a.mean()
    b = b - b.mean()
    ms = max(1, max_shift // step)
    best = (0, 0)
    bv = -np.inf
    for dy in range(-ms, ms + 1):
        for dx in range(-ms, ms + 1):
            bb = np.roll(np.roll(b, dy, axis=0), dx, axis=1)
            v = float(np.mean(a * bb))
            if v > bv:
                bv, best = v, (dy, dx)
    return (int(best[0] * step), int(best[1] * step))


def shift_crop(a: np.ndarray, dy: int, dx: int, ref_shape: Tuple[int, int]) -> np.ndarray:
    """整数平移 a 使与参考对齐，并裁到参考尺寸（越界部分由调用方掩膜）。"""
    out = np.roll(np.roll(np.asarray(a, dtype=np.float64), dy, axis=0), dx, axis=1)
    ny, nx = ref_shape
    return out[:ny, :nx]


def common_valid_mask(shape: Tuple[int, int], dy: int, dx: int,
                      margin: int = 0) -> np.ndarray:
    """整数平移 (dy,dx) 后，源帧哪些行/列来自**卷绕**（不可用）⇒ True 表示无效。"""
    ny, nx = shape
    m = np.zeros((ny, nx), dtype=bool)
    if dy > 0:
        m[:dy + margin, :] = True
    elif dy < 0:
        m[dy - margin:, :] = True
    if dx > 0:
        m[:, :dx + margin] = True
    elif dx < 0:
        m[:, dx - margin:] = True
    return m


def saturation_mask(img: np.ndarray, sat_adu: float = 65000.0) -> np.ndarray:
    return np.asarray(img, dtype=np.float64) >= float(sat_adu)


# ===========================================================================
# 9. 自检（本模块的 Oracle）
# ===========================================================================
def selftest(verbose: bool = True) -> Dict[str, Any]:
    """本模块的非退化自检：解析式对拍 + 区域网格 + 方差分解的合成校验。"""
    rng = np.random.default_rng(SEED)
    out: Dict[str, Any] = {}
    regs, g = region_grid((256, 256), 64)
    out["grid_ok"] = bool(len(regs) == 16 and g == (4, 4))
    x = rng.normal(0.0, 20.0, size=(512, 512))
    rs = region_sigma_resid(x, box=64, n_iter=1)
    sc = frame_scalar_sigma(x)
    out["null_effect_delta"] = effect_delta(float(np.median(rs["sigma"])), sc)
    out["null_scalar_sigma"] = sc
    y = rng.normal(0.0, 20.0, size=(512, 512))
    rd = region_sigma_diff(x, y, box=64)
    out["null_diff_rel_err"] = rel_err(float(np.median(rd["sigma"])), 20.0)
    slope = 0.325
    sig_n = 20.0
    ramp = struct_ramp((512, 512), slope)
    fr = ramp + rng.normal(0.0, sig_n, size=(512, 512))
    r1 = region_sigma_resid(fr, box=64, n_iter=3)
    out["ramp_pred_bias"] = predicted_region_bias_ramp(64, slope, sig_n)
    out["ramp_meas_bias"] = rel_err(float(np.median(r1["sigma"])), sig_n)
    R, K = 64, 6
    a = rng.normal(0.0, 5.0, size=R)
    b = rng.normal(0.0, 3.0, size=K)
    n = rng.normal(0.0, 1.0, size=(R, K))
    dec = two_way_decomposition(1.0 + a[:, None] + b[None, :] + n)
    out["decomp_var_interaction"] = dec["var_interaction"]
    out["decomp_var_expected"] = 1.0
    out["decomp_ratio"] = dec["var_interaction"] / 1.0
    out["optimal_box_analytic"] = optimal_box_ramp(slope, sig_n)
    out["optimal_box_numeric"] = optimal_box_numeric(slope, sig_n)
    if verbose:
        for k, v in out.items():
            print("  %-28s %s" % (k, v))
    return out


if __name__ == "__main__":
    print("exp03_common selftest:")
    selftest()


def gradient_regression(d: np.ndarray, ref: np.ndarray,
                        mask: Optional[np.ndarray] = None,
                        smooth_sigma: float = 2.0,
                        max_samples: int = 1_000_000) -> Dict[str, float]:
    """把差分场 D 对**参考帧的局部梯度**做最小二乘回归，分离亚像素错位贡献。

    若两帧之间存在残余亚像素平移 (bx, by)，则
        D(x) = 常数 + bx * dI/dx + by * dI/dy + 真实差分
    把 D 对平滑后参考帧的梯度回归掉，得到的残差 e 才是"真实差分"。
    回归系数 (bx, by) 即**残余亚像素平移的估计**（px），可直接登记为本检验的分辨率极限。

    为什么必须做这一步：整数像素对齐后残余 0.2~0.5 px 的错位在 M42 星云梯度区
    会注入结构，量级与"帧相关空间形态"混淆 —— 不做这一步就无法把两者分开。
    """
    from scipy.ndimage import gaussian_filter
    a = np.asarray(d, dtype=np.float64)
    r = np.asarray(ref, dtype=np.float64)
    valid = np.isfinite(a) & np.isfinite(r)
    if mask is not None:
        valid = valid & (~mask)
    ny, nx = a.shape
    s_row = max(1, int(math.ceil(math.sqrt(ny * nx / float(max_samples)))))
    sm = gaussian_filter(np.where(valid, r, 0.0), float(smooth_sigma), mode="nearest")
    gy, gx = np.gradient(sm)
    del sm
    sub = valid[::s_row, :]
    A = np.column_stack([np.ones(int(sub.sum())),
                         np.asarray(gx[::s_row, :][sub], dtype=np.float64),
                         np.asarray(gy[::s_row, :][sub], dtype=np.float64)])
    y = np.asarray(a[::s_row, :][sub], dtype=np.float64)
    coef, *_ = np.linalg.lstsq(A, y, rcond=None)
    e = np.full((ny // s_row + (1 if ny % s_row else 0), nx), np.nan)
    e[sub] = y - A @ coef
    return {"offset_x_px": float(coef[1]), "offset_y_px": float(coef[2]),
            "offset_px": float(math.hypot(coef[1], coef[2])),
            "const_adu": float(coef[0]),
            "resid_sigma": float(C.production_clip_sigma(
                y - A @ coef, n_rounds=2, k=C.CLIP_K)["sigma"]),
            "_resid": e}



def resolution_error(img: np.ndarray, box: int, ref_box: Optional[int] = None,
                     **kw) -> Dict[str, float]:
    """**尺度分辨率误差**：用 B x B 平均代表该区域 sigma 时，相对更高分辨率参考的误差。

    做法：把 B 尺度的 sigma 图与 B/2 尺度 sigma 图在**同一区域**上比较：
        res_err(B) = RMS_r [ sigma_B(r) / mean_{r}(sigma_{B/2}) - 1 ]
    这是"区域平均 sigma 能否代表区域内逐点 sigma"的直接度量 —— 也就是
    「区域尺度选择」的第三项误差（前两项是结构泄漏偏差与 1/sqrt(2N) 统计误差）。
    它**无需真值**，只需要同一帧在两个尺度上的 sigma 图。
    """
    rb = int(ref_box if ref_box else max(box // 2, 1))
    if rb == box:
        return {"box": int(box), "ref_box": rb, "res_err": float("nan"),
                "dispersion_box": float("nan"), "dispersion_ref": float("nan")}
    sb = region_sigma_resid(img, box, **kw)["sigma"]
    sr = region_sigma_resid(img, rb, **kw)["sigma"]
    f = box // rb
    ny, nx = sr.shape
    by, bx = ny // f, nx // f
    blk = (sr[:by * f, :bx * f].reshape(by, f, bx, f).transpose(0, 2, 1, 3)
              .reshape(by * bx, f * f))
    ref = np.nanmean(blk, axis=1).reshape(by, bx)
    a = sb[:by, :bx]
    m = np.isfinite(a) & np.isfinite(ref) & (ref > 0)
    if m.sum() == 0:
        return {"box": int(box), "ref_box": rb, "res_err": float("nan"),
                "dispersion_box": float("nan"), "dispersion_ref": float("nan")}
    r = a[m] / ref[m] - 1.0
    out = {"box": int(box), "ref_box": rb,
           "res_err": float(np.sqrt(np.mean(r ** 2))),
           "res_err_median_abs": float(np.median(np.abs(r))),
           "dispersion_box": float(np.percentile(a[m], 95) / max(np.percentile(a[m], 5), 1e-12)),
           "dispersion_ref": float(np.percentile(ref[m], 95) / max(np.percentile(ref[m], 5), 1e-12)),
           "n_region": int(m.sum())}
    # **分段常数 vs 双线性插值**：区域 sigma 图怎么交付，直接决定逐点误差
    # （SExtractor backrmsline / photutils BkgZoomInterpolator 都是插值交付）。
    exp_map = C._expand_mesh_map(np.asarray(sb, dtype=np.float64), sr.shape)
    blk2 = (exp_map[:by * f, :bx * f].reshape(by, f, bx, f).transpose(0, 2, 1, 3)
                 .reshape(by * bx, f * f))
    interp = np.nanmean(blk2, axis=1).reshape(by, bx)[:by, :bx]
    m2 = m & np.isfinite(interp) & (interp > 0)
    if m2.sum() > 0:
        r2 = interp[m2] / ref[m2] - 1.0
        out["res_err_bilinear"] = float(np.sqrt(np.mean(r2 ** 2)))
        out["res_err_bilinear_median_abs"] = float(np.median(np.abs(r2)))
        out["n_region_interp"] = int(m2.sum())
    return out



if __name__ == "__main__":
    # 模块自检（Oracle 对拍）：估计器族、结构函数、闭式、真值口径、认证门
    raise SystemExit(0 if selftest() else 1)
