#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-02 公共库：生产天光估计器的**独立重写** + 结构感知候选估计器 + 判据。

设计原则（AGENTS.md §5「不以当前程序输出生成唯一 expected」）：
  * 本模块**不 import 生产代码、不运行任何 AstroCS 可执行文件**；
  * 生产 recipe 依据 lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp
    的 StarDetector::estimate_background（按符号名定位）逐字重写，并给出代码锚；
  * 与 EXP-01 的 exp01_common.production_clip_sigma 做**独立交叉核对**
    （见 crosscheck_mirror），差异如实登记，不假设二者相同。

单位约定：ADU（图像值）、e-（电子）；gain [e-/ADU]；RN [e-]。
"""

from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

# ── 生产常数（逐字取自 star_detector.cpp::estimate_background 的调用面） ──────
MAD_TO_SIGMA = 1.482602218505602      # star_detector.cpp 内联常数
CLIP_K = 3.0                          # 裁剪阈值倍数
CLIP_ROUNDS = 2                       # 裁剪轮数
SIGMA_FLOOR = 1e-9                    # if (!(*sigma > 0.0)) *sigma = 1e-9


# ===========================================================================
# 1. 生产 recipe 的独立重写（严格逐字）
# ===========================================================================
def _upper_median(v: np.ndarray) -> float:
    """生产 nth_value(scratch, kn, kn/2)：**上中位数**（偶数 n 时取第 n/2 下标元素）。

    注意：这与 np.median（偶数 n 时取两中值平均）**不同**。EXP-01 的镜像用的是
    np.median，本模块两版都算，差异在 crosscheck_mirror 中定量登记。
    """
    kn = v.size
    if kn == 0:
        return float("nan")
    k = kn // 2
    return float(np.partition(v, k)[k])


def _serial_sum(x: np.ndarray) -> float:
    """生产的串行顺序求和（for i: sum += x[i]），与 np.sum 的成对求和不同。

    np.cumsum 是顺序累加，其末元素即串行左到右求和 ⇒ 与生产逐位一致。
    内存 O(n) 的临时数组，调用方负责规模。
    """
    if x.size == 0:
        return 0.0
    return float(np.cumsum(x)[-1])


def production_clip_sigma(img: np.ndarray, n_rounds: int = CLIP_ROUNDS,
                          k: float = CLIP_K, upper_median: bool = True,
                          serial_sum: bool = True) -> Dict[str, float]:
    """**逐字重写** StarDetector::estimate_background 的 sigma 生产者。

    生产实现（star_detector.cpp::estimate_background，按符号名定位）：
      keep = 全帧像素（非有限 ⇒ 整帧 fail-closed，返回 false）
      for round in 0..1:
          med  = nth_value(keep, kn, kn/2)            # 上中位数
          mad  = nth_value(|keep - med|, kn, kn/2)    # 上中位数
          s    = 1.482602218505602 * mad
          keep = [v for v in keep if |v - med| <= 3.0 * (s > 0 ? s : 1e-9)]
          if keep 空: break
      bg    = nth_value(keep, kn, kn/2)               # 上中位数
      sigma = sqrt( sum_{i}(keep[i]-bg)^2 / kn )      # 串行顺序求和, 总体 RMS
      if sigma <= 0: sigma = 1e-9

    返回 dict(sigma, background, n_keep, n_in, keep_frac)。
    """
    v = np.asarray(img, dtype=np.float64).ravel()
    if not np.all(np.isfinite(v)):
        raise ValueError("production_clip_sigma: 非有限像素 ⇒ 生产 fail-closed")
    n_in = int(v.size)
    v = v.copy()
    for _ in range(max(int(n_rounds), 0)):
        kn = v.size
        if kn == 0:
            break
        med = _upper_median(v) if upper_median else float(np.median(v))
        dev = np.abs(v - med)
        mad = _upper_median(dev) if upper_median else float(np.median(dev))
        s = MAD_TO_SIGMA * mad
        thr = k * (s if s > 0.0 else SIGMA_FLOOR)
        keep = np.abs(v - med) <= thr
        if keep.all():
            break
        v = v[keep]
        if v.size == 0:
            break
    kn = v.size
    bg = _upper_median(v) if upper_median else float(np.median(v))
    d2 = (v - bg) * (v - bg)
    tot = _serial_sum(d2) if serial_sum else float(np.sum(d2))
    sigma = math.sqrt(tot / float(kn if kn > 0 else 1))
    if not (sigma > 0.0):
        sigma = SIGMA_FLOOR
    return {"sigma": sigma, "background": bg, "n_keep": int(kn), "n_in": n_in,
            "keep_frac": (kn / n_in) if n_in else float("nan")}


def crosscheck_mirror(rng: np.random.Generator, n: int = 1 << 20) -> Dict[str, Any]:
    """独立核对：本模块 vs EXP-01 的 exp01_common.production_clip_sigma。

    两条镜像来自**同一份 C++ 源码**但实现细节不同（上中位数 vs np.median；
    串行求和 vs 成对求和）。差异必须 ≪ 本单元关心的 1% 量级效应。
    """
    x = rng.normal(0.0, 20.0, size=n)
    mine = production_clip_sigma(x)
    import sys
    from pathlib import Path
    p = Path(__file__).resolve().parent.parent / "exp01"
    ref: Dict[str, Any] = {"available": False}
    if (p / "exp01_common.py").exists():
        sys.path.insert(0, str(p))
        try:
            import exp01_common as EC  # noqa: E402
            theirs = EC.production_clip_sigma(x)
            ref = {"available": True, "sigma": float(theirs["sigma"]),
                   "background": float(theirs["background"]),
                   "rel_diff_sigma": float(mine["sigma"] / theirs["sigma"] - 1.0),
                   "rel_diff_bg": float((mine["background"] - theirs["background"])
                                        / max(abs(theirs["background"]), 1e-12)),
                   "d_n_keep": int(mine["n_keep"] - theirs["n_keep"])}
        except Exception as exc:                     # pragma: no cover
            ref = {"available": False, "error": repr(exc)}
    # 逐位口径核验：串行求和 vs np.sum
    d2 = (x - mine["background"]) ** 2
    return {"n": int(n), "mine": {k: float(v) if isinstance(v, float) else v
                                  for k, v in mine.items()},
            "exp01_mirror": ref,
            "serial_vs_pairwise_rel": float(_serial_sum(d2) / float(np.sum(d2)) - 1.0),
            "upper_vs_np_median_rel": float(
                (_upper_median(x) / float(np.median(x)) - 1.0)
                if float(np.median(x)) != 0.0 else float("nan"))}


# ===========================================================================
# 2. 结构感知候选估计器（修法 F1）
# ===========================================================================
def _mesh_stats(mesh: np.ndarray, n_rounds: int = 2, k: float = CLIP_K
                ) -> Tuple[float, float, float]:
    """单 mesh 的稳健位置/尺度（与生产同 recipe，但作用域是 mesh）。"""
    r = production_clip_sigma(mesh, n_rounds=n_rounds, k=k)
    return r["background"], r["sigma"], r["keep_frac"]


def _median_filter_nan(a: np.ndarray, size: int) -> np.ndarray:
    """背景图 median filter（SExtractor BACK_FILTERSIZE 的作用面）。

    实现要点（**必须**用奇数窗口 + 边界复制）：
      * 窗口内元素个数为偶数时 np.median 取两中值**平均**，对单调结构会造出
        数据里不存在的值 ⇒ 边界窗口被裁剪成 2×k 时会注入假的背景结构。
        实测：线性斜坡上该 bug 使 mesh 残差 RMS 从 0.02% 抬到 13000%（本单元
        独立复核时踩到，见 results/exp02_diag_mesh_bug.json）。
      * 故此处用 scipy.ndimage.median_filter(mode="nearest")：窗口恒为 size×size
        （奇数），边界以复制值填充。mesh 图在本实现中恒无 NaN（帧被截到整数 mesh）。
    """
    if size <= 1:
        return a.copy()
    from scipy.ndimage import median_filter
    if not np.all(np.isfinite(a)):
        raise ValueError("_median_filter_nan: mesh 图含非有限值")
    return median_filter(a, size=int(size), mode="nearest")


def _expand_mesh_map(m: np.ndarray, shape: Tuple[int, int]) -> np.ndarray:
    """把 mesh 中心值双线性展开到全帧（SExtractor 用双三次样条；此处双线性，登记为差异）。

    实现要点：mesh 坐标 u = (i+0.5)*M/N − 0.5 **不做区间裁剪**（允许线性外推），
    否则帧边缘半个 mesh 内的背景被钉在首个 mesh 值上 —— 对线性结构会引入
    约 amp/(2M) 的假残差（实测把 mesh 残差 RMS 从 0.02% 抬到 13000%）。
    """
    ny, nx = shape
    my, mx = m.shape
    # 线性外推 1 格外扩：mesh 坐标 u ∈ (−0.5, M−0.5)，故 1 格足够覆盖全帧。
    # 若不外扩（把 y0/y1 一起 clip），帧边缘半个 mesh 的背景被钉在首个 mesh 值上，
    # 对线性结构注入约 amp/(2M) 的假残差（实测把残差 RMS 从 0.02% 抬到 +2.1%）。
    mp = np.empty((my + 2, mx + 2), dtype=np.float64)
    mp[1:-1, 1:-1] = m
    # 退化网格（my==1 或 mx==1，例如 box=帧宽 ⇒ 1×1 mesh ⇒ 背景退化为常数）：
    # 无邻居可外推，按常值复制。
    if my >= 2:
        mp[0, 1:-1] = 2.0 * m[0, :] - m[1, :]
        mp[-1, 1:-1] = 2.0 * m[-1, :] - m[-2, :]
    else:
        mp[0, 1:-1] = m[0, :]
        mp[-1, 1:-1] = m[0, :]
    if mx >= 2:
        mp[:, 0] = 2.0 * mp[:, 1] - mp[:, 2]
        mp[:, -1] = 2.0 * mp[:, -2] - mp[:, -3]
    else:
        mp[:, 0] = mp[:, 1]
        mp[:, -1] = mp[:, 1]
    yy = (np.arange(ny) + 0.5) * my / ny - 0.5 + 1.0
    xx = (np.arange(nx) + 0.5) * mx / nx - 0.5 + 1.0
    y0f = np.floor(yy).astype(int)
    x0f = np.floor(xx).astype(int)
    wy = (yy - y0f)[:, None]
    wx = (xx - x0f)[None, :]
    y0 = np.clip(y0f, 0, my + 1)
    x0 = np.clip(x0f, 0, mx + 1)
    y1 = np.clip(y0f + 1, 0, my + 1)
    x1 = np.clip(x0f + 1, 0, mx + 1)
    m00 = mp[np.ix_(y0, x0)]
    m01 = mp[np.ix_(y0, x1)]
    m10 = mp[np.ix_(y1, x0)]
    m11 = mp[np.ix_(y1, x1)]
    top = m00 * (1.0 - wx) + m01 * wx
    bot = m10 * (1.0 - wx) + m11 * wx
    return top * (1.0 - wy) + bot * wy


def mesh_background_map(img: np.ndarray, box: int = 64, filter_size: int = 3,
                        n_rounds: int = 2, k: float = CLIP_K,
                        n_iter: int = 3) -> Tuple[np.ndarray, Dict[str, Any]]:
    """迭代 mesh 背景图 B(x,y)（SExtractor BACK_SIZE/BACK_FILTERSIZE 的对应物）。

    **为什么必须迭代**：mesh 内若存在显著梯度，mesh 中位数不再是"局部背景"的无偏
    估计 —— 其误差量级是 σ_noise/sqrt(每列像素数) 而不是 σ_noise/sqrt(N_mesh)。
    实测（线性斜坡，斜率 6.8σ/px，box=64）：单轮 mesh 中位数相对真值 scatter
    0.27σ ⇒ 残差 RMS 被抬 +3.7%（超过 δ 预算）。迭代后残差在 mesh 内近乎平坦，
    第二轮起 mesh 中位数的误差回到 σ/sqrt(N) 量级。

    返回 (B, meta)。meta 含每轮的背景图增量范数与 mesh 保留比。
    """
    a = np.asarray(img, dtype=np.float64)
    ny, nx = a.shape
    by, bx = ny // box, nx // box
    if by < 1 or bx < 1:
        raise ValueError("mesh_background_map: 帧比 mesh 还小")
    sub = a[:by * box, :bx * box]
    meshes_idx = (sub.reshape(by, box, bx, box)
                     .transpose(0, 2, 1, 3)
                     .reshape(by * bx, box * box))
    # 每个像素属于哪个 mesh（用于从残差图重建 mesh 统计）
    B = np.zeros((ny, nx), dtype=np.float64)
    deltas: List[float] = []
    kf_last = np.ones(by * bx)
    for _ in range(max(int(n_iter), 1)):
        r = a - B
        msub = r[:by * box, :bx * box]
        mm = (msub.reshape(by, box, bx, box)
                  .transpose(0, 2, 1, 3)
                  .reshape(by * bx, box * box))
        loc = np.empty(by * bx)
        kf = np.empty(by * bx)
        for i in range(by * bx):
            st = production_clip_sigma(mm[i], n_rounds=n_rounds, k=k)
            loc[i] = st["background"]
            kf[i] = st["keep_frac"]
        loc_f = _median_filter_nan(loc.reshape(by, bx), filter_size)
        dB = _expand_mesh_map(loc_f, (ny, nx))
        B = B + dB
        deltas.append(float(np.std(dB)))
        kf_last = kf
    return B, {"n_iter": int(max(int(n_iter), 1)), "delta_std": deltas,
               "keep_frac_mesh_p50": float(np.median(kf_last)),
               "keep_frac_mesh_p05": float(np.percentile(kf_last, 5)),
               "mesh_grid": [int(by), int(bx)], "n_mesh": int(by * bx)}


def mesh_sigma(img: np.ndarray, box: int = 64, filter_size: int = 3,
               n_rounds: int = 2, k: float = CLIP_K,
               return_maps: bool = False, n_iter: int = 3) -> Dict[str, Any]:
    """候选 F1：**结构感知的鲁棒天光估计**（mesh 局部背景 + 残差稳健尺度）。

    步骤（对标 SExtractor BACK_SIZE/BACK_FILTERSIZE 与 photutils Background2D）：
      1. 全帧切成 box×box 的 mesh；每个 mesh 用**与生产同 recipe** 的裁剪中位数/RMS；
      2. 对 mesh 位置图做 filter_size 中值滤波（剔除被天体污染的 mesh，BACK_FILTERSIZE）；
      3. 双线性展开成背景图 B(x,y)；
      4. residual = img − B；对 residual 再做一次生产 recipe 的裁剪 RMS ⇒ sigma_resid；
      5. 稳健聚合：各 mesh 尺度的中位数 ⇒ sigma_mesh_median（对残差结构更稳）。

    返回 sigma_resid / sigma_mesh_median / sigma_mesh_p05 / sigma_mesh_p95 /
    keep_frac_mesh_p50 / bg_mesh 等。
    """
    a = np.asarray(img, dtype=np.float64)
    ny, nx = a.shape
    B, meta = mesh_background_map(a, box=box, filter_size=filter_size,
                                  n_rounds=n_rounds, k=k, n_iter=n_iter)
    resid = a - B
    rr = production_clip_sigma(resid, n_rounds=n_rounds, k=k)
    # 稳健聚合备选：各 mesh **残差** RMS 的中位数（对个别污染 mesh 稳健）
    by, bx = meta["mesh_grid"]
    box_eff = ny // by
    msub = resid[:by * box_eff, :bx * box_eff]
    mm = (msub.reshape(by, box_eff, bx, box_eff)
              .transpose(0, 2, 1, 3)
              .reshape(by * bx, box_eff * box_eff))
    sca = np.array([production_clip_sigma(m, n_rounds=n_rounds, k=k)["sigma"]
                    for m in mm])
    out: Dict[str, Any] = {
        "box": int(box), "filter_size": int(filter_size),
        "n_iter": int(meta["n_iter"]), "delta_std": meta["delta_std"],
        "n_mesh": int(meta["n_mesh"]), "mesh_grid": meta["mesh_grid"],
        "sigma_resid": float(rr["sigma"]),
        "sigma_resid_keep_frac": float(rr["keep_frac"]),
        "sigma_mesh_median": float(np.median(sca)),
        "sigma_mesh_p05": float(np.percentile(sca, 5)),
        "sigma_mesh_p95": float(np.percentile(sca, 95)),
        "sigma_mesh_dispersion": float(np.percentile(sca, 95) / max(np.percentile(sca, 5), 1e-12)),
        "keep_frac_mesh_p50": float(meta["keep_frac_mesh_p50"]),
        "keep_frac_mesh_p05": float(meta["keep_frac_mesh_p05"]),
    }
    if return_maps:
        out["background_map"] = B
        out["residual"] = resid
    return out


# ===========================================================================
# 3. 度量与判据（非退化）
# ===========================================================================
def rel_err(est: float, true: float) -> float:
    """(est/true - 1)：本单元的主度量。真值无结构时它**不是零**（含裁剪低偏），
    因此判据必须建立在"修法的**效应**"上，见 effect_delta。"""
    return (est / true - 1.0) if true else float("nan")


def effect_delta(est_fix: float, est_prod: float) -> float:
    """修法效应 Δ = σ̂_fix/σ̂_prod − 1。

    **非退化负例的核心度量**：真值无结构时，修法不得改变估计值 ⇒ Δ 必须归零。
    恒真的度量（如 σ̂>0）没有证据资格。
    """
    return (est_fix / est_prod - 1.0) if est_prod else float("nan")


def convergence_gate(img: np.ndarray, box: int, filter_size: int = 3,
                     n_rounds: int = 2, k: float = CLIP_K) -> Dict[str, float]:
    """**尺度收敛判据** C = |σ̂(box)/σ̂(2·box) − 1|（无需真值即可计算）。

    原理：结构感知估计器的残差污染来自"mesh 尺度以内的结构"。若把 mesh 尺寸加倍
    而 σ̂ 不变（C ≤ δ），说明该尺度以上的结构已被背景模型吸收 ⇒ 估计对结构不敏感。
    这是一个**可证明保守**的自洽门：它不依赖任何仿真真值。
    """
    r1 = mesh_sigma(img, box=box, filter_size=filter_size,
                    n_rounds=n_rounds, k=k)
    r2 = mesh_sigma(img, box=2 * box, filter_size=filter_size,
                    n_rounds=n_rounds, k=k)
    s1, s2 = r1["sigma_resid"], r2["sigma_resid"]
    return {"box": int(box), "sigma_box": float(s1), "sigma_2box": float(s2),
            "C": float(abs(s1 / s2 - 1.0)) if s2 else float("nan")}


def difference_sigma(img: np.ndarray, n_rounds: int = 2, k: float = CLIP_K
                     ) -> Dict[str, float]:
    """**一阶差分参考噪声尺度**（无需真值、对空间相关结构极敏感）。

    白噪声下相邻像素差 = sqrt(2)·σ；空间相关结构（相关长度 ℓ）对差分的贡献只有
    σ_s²·2(1−ρ(1))，ρ(1)=exp(−1/(2ℓ²))。故

        A1 = σ̂_global / (σ̂_diff/sqrt(2))

    在纯白噪声下 ≈ 1；一旦存在相关长度 ≳1 px 的结构，A1 立刻 > 1
    （ℓ=8 px、σ_s=σ_n 时 A1 ≈ 1.41，而 R_struct ≈ 1.0 —— 这正是全局/mesh 对比
    检测不到的结构尺度，见报告 §4.3）。

    **已知混杂**：真实帧经 drizzle/重采样后有相关噪声，A1 会合法地 > 1；
    故 A1 是"敏感但不特异"的检测量，方向保守（宁可误报）。
    """
    a = np.asarray(img, dtype=np.float64)
    dx = a[:, 1:] - a[:, :-1]
    dy = a[1:, :] - a[:-1, :]
    sx = production_clip_sigma(dx, n_rounds=n_rounds, k=k)["sigma"]
    sy = production_clip_sigma(dy, n_rounds=n_rounds, k=k)["sigma"]
    d = math.sqrt(0.5 * (sx * sx + sy * sy))
    # 稳健备选：1.4826*MAD（对稀疏离群——星点翼、宇宙线、热像素——远比裁剪 RMS 稳），
    # 代价是 16-bit 整数数据上 MAD 的量化粒度较大（EXP-01 §2.2③ 的诚实边界）。
    mx = MAD_TO_SIGMA * float(np.median(np.abs(dx - np.median(dx))))
    my = MAD_TO_SIGMA * float(np.median(np.abs(dy - np.median(dy))))
    dm = math.sqrt(0.5 * (mx * mx + my * my))
    return {"sigma_diff": d, "sigma_diff_over_sqrt2": d / math.sqrt(2.0),
            "sigma_diff_mad": dm, "sigma_diff_mad_over_sqrt2": dm / math.sqrt(2.0),
            "sigma_diff_x": sx, "sigma_diff_y": sy, "sigma_diff_mad_x": mx,
            "sigma_diff_mad_y": my}


def structure_proxies(img: np.ndarray, box: int = 32, filter_size: int = 3,
                      n_iter: int = 2) -> Dict[str, Any]:
    """全部**无需真值**的结构污染代理量（供三档门使用）。

      prod      = 生产全局裁剪 RMS（被审计对象）
      fix       = 候选 F1（mesh 结构感知）的 σ̂
      R_struct  = prod/fix                —— 尺度 > box 的结构污染
      C         = |σ̂(box)/σ̂(2box) − 1|   —— box..2box 尺度的结构
      A1        = prod/(σ_diff/sqrt2)     —— 任何空间相关结构（宽带，敏感不特异）
      A2        = fix /(σ_diff/sqrt2)     —— **修法之后**残余结构（判据主量）
      D         = p95(σ_mesh)/p05(σ_mesh) —— 帧内噪声尺度非均匀性
      kf        = 生产裁剪保留比（零成本，当前被丢弃）
    """
    prod = production_clip_sigma(img)
    m = mesh_sigma(img, box=box, filter_size=filter_size, n_iter=n_iter)
    g = convergence_gate(img, box=box, filter_size=filter_size)
    ds = difference_sigma(img)
    ref = ds["sigma_diff_over_sqrt2"]
    ref_mad = ds["sigma_diff_mad_over_sqrt2"]
    return {
        "prod_sigma": prod["sigma"], "prod_keep_frac": prod["keep_frac"],
        "fix_sigma": m["sigma_resid"], "box": box,
        # F1a（SExtractor 口径）：σ 图跨 mesh 的**中值**（back.c:846 的 backsig 对应物）
        "fix_sigma_meshmedian": m["sigma_mesh_median"],
        "sigma_diff_over_sqrt2": ref,
        "R_struct": prod["sigma"] / m["sigma_resid"],
        "sigma_diff_mad_over_sqrt2": ref_mad,
        "C": g["C"], "A1": prod["sigma"] / ref, "A2": m["sigma_resid"] / ref,
        "A1_mad": prod["sigma"] / ref_mad, "A2_mad": m["sigma_resid"] / ref_mad,
        "A2_meshmedian": m["sigma_mesh_median"] / ref,
        "A2_meshmedian_mad": m["sigma_mesh_median"] / ref_mad,
        "D": m["sigma_mesh_dispersion"], "kf": prod["keep_frac"],
        "fix_effect_delta": effect_delta(m["sigma_resid"], prod["sigma"]),
        "fix_effect_delta_meshmedian": effect_delta(m["sigma_mesh_median"], prod["sigma"]),
    }


def classify(R_struct: float, C: float, keep_frac: float,
             delta_budget: float = 0.014,
             keep_min: float = 0.95, A1: Optional[float] = None,
             A2: Optional[float] = None, D: Optional[float] = None,
             D_ok: float = 1.25) -> str:
    """三档判据口径：ok（可证明可忽略）/ diagnostic_only（可证明保守只出诊断）/ fail_closed。

    认证对象 = **即将写入 provenance 的那个 σ̂**。输入全部**无需真值**：

      A1 = σ̂_global/(σ_diff/sqrt2)  —— 帧内是否存在空间相关结构（宽带检测）
      R  = σ̂_global/σ̂_fix           —— 全局估计被结构抬高的倍数
      kf = 生产全局裁剪保留比        —— 零成本可得，当前被丢弃
      A2 = σ̂_fix/(σ_diff/sqrt2)     —— **修法之后**的残余结构（认证主量）
      C  = |σ̂(box)/σ̂(2box) − 1|     —— mesh 尺度收敛
      D  = p95(σ_mesh)/p05(σ_mesh)  —— 帧内噪声尺度非均匀性（局部污染检测）

    判据：
      ok               ⟺ 未检出结构（A1,R ≤ 1+δ 且 kf ≥ keep_min）
                          且 σ̂_global 自身通过全部自洽检查
                          ⇒ 全局估计的 σ_sky 语义**可证明可忽略**地未受结构影响
      diagnostic_only  ⟺ 检出结构，但**修法后的 σ̂_fix** 通过 A2/C/D/kf
                          ⇒ 只出诊断量，不进绝对 SNR 链的标定基础
      fail_closed      ⟺ 连修法后的估计也无法认证 ⇒ 该帧不产出 σ_sky（不伪造）

    阈值 δ=1.4% 取自 SCI-SNR-01 §4.5 的 δ 预算量级；标定与反例审计见
    e4_gates_selftest.py（要求"判 ok/diagnostic ⇒ 真值误差 ≤ 预算"零反例）。
    """
    a1 = 1.0 if A1 is None else A1
    a2 = 1.0 if A2 is None else A2
    d = 1.0 if D is None else D
    tol = 1.0 + delta_budget
    no_structure = (a1 <= tol) and (R_struct <= tol) and (keep_frac >= keep_min)
    fix_certified = (a2 <= tol) and (C <= delta_budget) and (d <= D_ok) \
        and (keep_frac >= 0.80)
    if no_structure and fix_certified:
        return "ok"
    if fix_certified:
        return "diagnostic_only"
    return "fail_closed"


# ===========================================================================
# 4. 解析结构场（纯解析合成臂）
# ===========================================================================
def struct_ramp(shape: Tuple[int, int], amp: float) -> np.ndarray:
    """线性斜坡结构：B = amp*(x/(W-1) - 0.5)。σ_B = amp/sqrt(12)。"""
    ny, nx = shape
    x = np.arange(nx, dtype=np.float64) / max(nx - 1, 1) - 0.5
    return np.tile(amp * x, (ny, 1))


def struct_smooth_field(shape: Tuple[int, int], rms: float, corr_px: float,
                        rng: np.random.Generator) -> np.ndarray:
    """高斯相关结构场：白噪 → 高斯平滑 → 归一到指定 rms。corr_px 控制空间尺度。"""
    from scipy.ndimage import gaussian_filter
    w = rng.normal(0.0, 1.0, size=shape)
    g = gaussian_filter(w, sigma=max(corr_px, 1e-6), mode="wrap")
    s = float(np.std(g))
    if s <= 0:
        return np.zeros(shape)
    return g * (rms / s)


def struct_blob(shape: Tuple[int, int], amp: float, sigma_px: float,
                center: Optional[Tuple[float, float]] = None) -> np.ndarray:
    """单个高斯团块结构（紧凑结构，覆盖比小）。"""
    ny, nx = shape
    cy, cx = center if center else (ny / 2.0, nx / 2.0)
    yy, xx = np.mgrid[0:ny, 0:nx]
    return amp * np.exp(-(((yy - cy) ** 2 + (xx - cx) ** 2) / (2.0 * sigma_px ** 2)))


def struct_from_template(img: np.ndarray, rms: float) -> np.ndarray:
    """把一张真实图像当作纯信号模板，去均值后归一到指定 rms（不改变形态）。"""
    a = np.asarray(img, dtype=np.float64)
    a = a - float(np.median(a))
    s = float(np.std(a))
    return a * (rms / s) if s > 0 else np.zeros_like(a)


def rms_of(x: np.ndarray) -> float:
    """以**中位数为中心**的总体 RMS（与生产 noise_sigma 同口径）。"""
    a = np.asarray(x, dtype=np.float64)
    return float(np.sqrt(np.mean((a - float(np.median(a))) ** 2)))


def add_noise_adu(struct_adu: np.ndarray, sigma_adu: float,
                  rng: np.random.Generator) -> np.ndarray:
    """纯解析合成：结构 + 高斯白噪声（ADU 域），真值 sigma 已知。"""
    return struct_adu + rng.normal(0.0, sigma_adu, size=struct_adu.shape)
