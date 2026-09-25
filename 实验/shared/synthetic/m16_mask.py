#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ACSD RELEASE-02 / M16-SCENE —— 哈勃 M16 WFC3/UVIS 三帧的**有效域掩膜**生成器。

上游数据（负责人上传，真实哈勃 HLSP Heritage 产品，各 268,917,120 B）::

    testdata/HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_f502n_v1_drz.fits   [O III]  PHOTPLAM 5009.64 A
    testdata/HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits   H-alpha  PHOTPLAM 6566.61 A
    testdata/HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_f673n_v1_drz.fits   [S II]   PHOTPLAM 6765.92 A

三帧**同一天区、同一 WCS 网格**（CRVAL 274.721587/-13.841549、CRPIX 4000/4200、
CD 矩阵逐位相同、纯 TAN 无 SIP），因此**共用一套几何有效域**（逐帧仍各自成图，
因为零填充/坏点图样逐帧可能不同）。

为什么必须带掩膜（负责人 2026-09-19 提示）
------------------------------------------
「这组数据**不是全部区域可用**。边缘有一些 **0 数据区域和坏点区域**」。
实测：整帧 zero 10.25%、非有限 0.000%、负值 0.001-0.004%；
有效域是一个**旋转（ORIENTAT = -35 deg）的 WFC3/UVIS 双芯片足迹**，
四角与上/下边界为 0 填充（drizzle 无覆盖），**不是矩形**。

掩膜判据（全部写死在本模块，逐条给数值；见 MaskConfig）
--------------------------------------------------------
位标志（uint8 flags）::

    bit 0 (  1) NONFINITE   非有限值（NaN/Inf）                        -> 无效
    bit 1 (  2) ZERO        值 == 0.0（零填充 / 无覆盖 / 真零通量）    -> 无效
    bit 2 (  4) NEGATIVE    值 < 0（drizzle 合成天光扣除残差）         -> 无效
    bit 3 (  8) EDGE        落在「最大全有效轴对齐矩形」之外           -> 无效
    bit 4 ( 16) EXTREME_NEG v < bg - k_extreme * sigma_blk             -> 无效
    bit 5 ( 32) SPIKE       孤立尖峰（坏点/未剔除宇宙线嫌疑）          -> **保留**（可疑标记）

valid = 1 当且仅当 flags & (NONFINITE|ZERO|NEGATIVE|EDGE|EXTREME_NEG) == 0；
**SPIKE 不置无效**（理由见下）。

孤立尖峰判据（坏点 / 宇宙线幸存者）
-----------------------------------
在**欠采样**的 HST PSF 下，真实星点也是一个「尖峰」，因此判据必须来自 PSF 本身：

    SPIKE  <=>  (v - bg) > k_spike_sigma * sigma_blk        [显著]
            且  (v - max8(v)) / (v - bg) > r_spike          [孤立：无 PSF 翼]

max8 为 8 邻域最大值。对**任何** FWHM >= 1.2 px 的 PSF，峰值像素相对其最亮邻像素
的「超额占比」r 有上界（数值标定见 m16_scene.py 的 psf_r_max 与
run/reverse_verify/m16_scene/masks/mask_build_manifest.json 的 r_spike_calibration）；
热像素/单像素宇宙线的邻域与背景同亮 => r -> 1。
**保守取向**：SPIKE 只标记不剔除（宁可不剔，不可把真星当坏点），
其数量与跨波段重合率逐帧登记，供下游按需启用（exclude_spike=True）。

**为什么不用全局 sigma**：亮星云视场里全局 sigma 由星云结构主导，
真正的坏点判据必须是**局部**的 => 用 64x64 块的稳健中位/MAD 上采样成 bg/sigma 面。

诚实边界（不得省略）
--------------------
* 本产品**只有 SCI 帧**（PrimaryHDU 单 HDU），**没有** _wht 权重帧、**没有** DQ 帧
  （头里 D001OUWE 指向的 wht 文件未随数据提供）=> **无法**区分
  「无覆盖的零」与「覆盖到但通量为零的零」，也无法使用官方 DQ 标志；
  本掩膜的 EDGE/ZERO 是**从零值几何反推**的，不是官方覆盖图。
* 因此本掩膜对「星云真实的零通量像素」会**过度剔除**（保守方向，安全）。
* SPIKE 判据在**星云丝状结构**上会退化（结构本身使 sigma_blk 变大 => 灵敏度下降），
  故它是**下界**，不是坏点完备集。
* 掩膜**不入库**，落 run/reverse_verify/m16_scene/masks/；
  仓库内只登记索引与统计（实验/shared/data/real/m16_scene_index.json）。

CLI
---
    export TMPDIR=/dev/shm/astrocs_m16
    python3 m16_mask.py --frames-dir testdata/HST_M16 \
        --outdir ../../../run/reverse_verify/m16_scene/masks
    python3 m16_mask.py --selftest        # 判据负例：合成已知坏点必须被检出
"""

from __future__ import annotations

import argparse
import json
import math
import time
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]                      # 仓库根

# --- 位标志 ---------------------------------------------------------------
F_NONFINITE = 1
F_ZERO = 2
F_NEGATIVE = 4
F_EDGE = 8
F_EXTREME_NEG = 16
F_SPIKE = 32

INVALID_BITS = F_NONFINITE | F_ZERO | F_NEGATIVE | F_EDGE | F_EXTREME_NEG

FLAG_NAMES = {
    F_NONFINITE: "NONFINITE", F_ZERO: "ZERO", F_NEGATIVE: "NEGATIVE",
    F_EDGE: "EDGE", F_EXTREME_NEG: "EXTREME_NEG", F_SPIKE: "SPIKE",
}

# --- 三帧登记表（波段 -> 文件） -------------------------------------------
BANDS: Dict[str, Dict[str, Any]] = {
    "F502N": {"file": "hlsp_heritage_hst_wfc3-uvis_m16_f502n_v1_drz.fits",
              "line": "[O III]", "photom_key": "OIII_5007"},
    "F657N": {"file": "hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits",
              "line": "H-alpha", "photom_key": "Ha_6563"},
    "F673N": {"file": "hlsp_heritage_hst_wfc3-uvis_m16_f673n_v1_drz.fits",
              "line": "[S II]", "photom_key": "SII_6716_6731"},
}
BAND_ORDER = ("F657N", "F673N", "F502N")


# ---------------------------------------------------------------------------
# 配置
# ---------------------------------------------------------------------------
@dataclass
class MaskConfig:
    """掩膜判据参数（**全部写死在此，逐条进 meta**）。"""

    block: int = 64                 # 局部背景/散布的块尺寸 [px]
    chunk_rows: int = 1024          # 流式处理行块（控内存）
    k_extreme: float = 20.0         # EXTREME_NEG: v < bg - k*sigma_blk
    k_spike_sigma: float = 10.0     # SPIKE: (v-bg) > k*sigma_blk
    r_spike: float = 0.80           # SPIKE: (v-max8)/(v-bg) > r_spike
    spike_abs_floor_e: float = 0.0  # SPIKE 的绝对下限 [e/s]（0 = 不设）
    sigma_floor: float = 1e-9       # sigma_blk 下限，防除零

    def as_dict(self) -> Dict[str, Any]:
        return asdict(self)


# ---------------------------------------------------------------------------
# 1. 分块稳健背景 / 散布
# ---------------------------------------------------------------------------
def block_robust_stats(arr: np.ndarray, block: int) -> Tuple[np.ndarray, np.ndarray]:
    """把 (ny,nx) 切成整块，返回逐块 (median, 1.4826*MAD)。仅用整块。"""
    ny, nx = arr.shape
    nby, nbx = ny // block, nx // block
    if nby == 0 or nbx == 0:
        return np.zeros((0, 0)), np.zeros((0, 0))
    b = (arr[:nby * block, :nbx * block]
         .reshape(nby, block, nbx, block).transpose(0, 2, 1, 3)
         .reshape(nby, nbx, block * block).astype(np.float64))
    med = np.median(b, axis=2)
    mad = 1.4826 * np.median(np.abs(b - med[:, :, None]), axis=2)
    return med, mad


def upsample_blocks(blk: np.ndarray, block: int, shape: Tuple[int, int],
                    row0: int = 0) -> np.ndarray:
    """把块图按行区间 [row0, row0+shape[0]) 展开成逐像素面（最近块，无插值）。"""
    ny, nx = shape
    nby, nbx = blk.shape
    iy0 = row0 // block
    iy1 = min(nby, int(math.ceil((row0 + ny) / block)))
    sub = blk[iy0:iy1, :]
    up = np.repeat(np.repeat(sub, block, axis=0), block, axis=1)
    y_off = row0 - iy0 * block
    if up.shape[0] < y_off + ny:          # 末块不足：用最后一行块补齐（边界外推）
        pad = np.repeat(up[-1:, :], y_off + ny - up.shape[0], axis=0)
        up = np.concatenate([up, pad], axis=0)
    return up[y_off:y_off + ny, :nx]


RING8 = np.ones((3, 3), dtype=bool)
RING8[1, 1] = False


# ---------------------------------------------------------------------------
# 2. 单帧掩膜
# ---------------------------------------------------------------------------
def build_frame_mask(path, cfg: MaskConfig, *, verbose: bool = True
                     ) -> Tuple[np.ndarray, np.ndarray, Dict[str, Any]]:
    """读一帧 -> (valid uint8, flags uint8, meta)。

    流式：一次只驻留 cfg.chunk_rows + 2 行。
    """
    from astropy.io import fits
    from scipy.ndimage import maximum_filter

    p = Path(path)
    t0 = time.time()
    with fits.open(p, memmap=True) as h:
        d = h[0].data
        hdr = h[0].header
        ny, nx = d.shape
        block = cfg.block

        # ---- 第一遍：全局计数 + 行/列零剖面 + 块稳健统计 ----
        cnt = dict(total=int(ny) * int(nx), nonfinite=0, zero=0, neg=0, pos=0)
        row_zero = np.zeros(ny, dtype=np.int64)
        col_zero = np.zeros(nx, dtype=np.int64)
        nby, nbx = ny // block, nx // block
        blk_med = np.full((nby, nbx), np.nan)
        blk_sig = np.full((nby, nbx), np.nan)
        gmin, gmax = math.inf, -math.inf
        for y0 in range(0, ny, cfg.chunk_rows):
            y1 = min(y0 + cfg.chunk_rows, ny)
            sub = np.asarray(d[y0:y1])
            fin = np.isfinite(sub)
            z = fin & (sub == 0.0)
            ng = fin & (sub < 0.0)
            cnt["nonfinite"] += int((~fin).sum())
            cnt["zero"] += int(z.sum())
            cnt["neg"] += int(ng.sum())
            cnt["pos"] += int((fin & (sub > 0.0)).sum())
            row_zero[y0:y1] = z.sum(axis=1)
            col_zero += z.sum(axis=0)
            if fin.any():
                gmin = min(gmin, float(sub[fin].min()))
                gmax = max(gmax, float(sub[fin].max()))
            by = (y1 - y0) // block
            if by > 0:
                m, s = block_robust_stats(np.asarray(sub[:by * block], dtype=np.float32), block)
                blk_med[y0 // block:y0 // block + by] = m
                blk_sig[y0 // block:y0 // block + by] = s

        # ---- 第二遍：逐行块算 flags（EXTREME / SPIKE；EDGE 稍后统一叠加） ----
        flags = np.zeros((ny, nx), dtype=np.uint8)
        spike_vals: List[Tuple[int, int, float]] = []
        halo = 1
        for y0 in range(0, ny, cfg.chunk_rows):
            y1 = min(y0 + cfg.chunk_rows, ny)
            ya, yb = max(0, y0 - halo), min(ny, y1 + halo)
            band = np.asarray(d[ya:yb], dtype=np.float32)
            off = y0 - ya
            inner = band[off:off + (y1 - y0)]
            fin = np.isfinite(inner)
            z = fin & (inner == 0.0)
            ng = fin & (inner < 0.0)
            bg = upsample_blocks(blk_med, block, inner.shape, row0=y0).astype(np.float32)
            sg = upsample_blocks(blk_sig, block, inner.shape, row0=y0).astype(np.float32)
            sg = np.maximum(sg, cfg.sigma_floor)
            f = np.zeros(inner.shape, dtype=np.uint8)
            f[~fin] |= F_NONFINITE
            f[z] |= F_ZERO
            f[ng] |= F_NEGATIVE
            dev = inner - bg
            f[fin & (dev < -cfg.k_extreme * sg)] |= F_EXTREME_NEG
            mx8 = maximum_filter(band, footprint=RING8, mode="nearest")[off:off + (y1 - y0)]
            bright = fin & (dev > cfg.k_spike_sigma * sg)
            if cfg.spike_abs_floor_e > 0:
                bright = bright & (dev > cfg.spike_abs_floor_e)
            r = np.where(bright, (inner - mx8) / np.maximum(dev, 1e-12), -np.inf)
            sp = bright & (r > cfg.r_spike)
            f[sp] |= F_SPIKE
            if sp.any():
                yy, xx = np.nonzero(sp)
                for a, b in zip(yy.tolist(), xx.tolist()):
                    spike_vals.append((y0 + a, b, float(inner[a, b])))
            flags[y0:y1] = f
        # ---- EDGE：与**画幅边框连通**的零/非有限像素 = drizzle 无覆盖的拼接边界 ----
        from scipy.ndimage import label as _label
        nocov = ((flags & (F_ZERO | F_NONFINITE)) != 0)
        lab, nlab = _label(nocov, structure=np.array([[0, 1, 0], [1, 1, 1], [0, 1, 0]]))
        border_labels = np.unique(np.concatenate([
            lab[0, :], lab[-1, :], lab[:, 0], lab[:, -1]]))
        border_labels = border_labels[border_labels > 0]
        edge = np.isin(lab, border_labels) if border_labels.size else np.zeros_like(nocov)
        del lab, nocov
        flags[edge] |= F_EDGE
        valid = ((flags & INVALID_BITS) == 0).astype(np.uint8)
        del edge

        # ---- 矩形便利量（供需要矩形裁切的用户；不进 valid 判据） ----
        bad_all = valid == 0
        rect_max = max_rect_pixels(bad_all, ny, nx)
        del bad_all
        bad_cov = (flags & (F_ZERO | F_NONFINITE)) != 0
        rect_foot = max_rect_pixels(bad_cov, ny, nx)
        del bad_cov

        # ---- 统计 ----
        is_edge = (flags & F_EDGE) != 0
        is_zero = (flags & F_ZERO) != 0
        interior_zero = int(np.count_nonzero(is_zero & ~is_edge))
        border = np.zeros((ny, nx), dtype=bool)
        border[0, :] = True; border[-1, :] = True
        border[:, 0] = True; border[:, -1] = True
        border_zero_frac = float(np.count_nonzero(is_zero & border) / max(border.sum(), 1))
        edge_stats = {
            "row0_zero_frac": float(is_zero[0].mean()),
            "rowN_zero_frac": float(is_zero[-1].mean()),
            "col0_zero_frac": float(is_zero[:, 0].mean()),
            "colN_zero_frac": float(is_zero[:, -1].mean()),
            "outer1px_border_zero_frac": border_zero_frac,
            "outer10rows_zero_frac": float(is_zero[:10].mean()),
            "outer10cols_zero_frac": float(is_zero[:, :10].mean()),
            "outer100rows_zero_frac": float(np.concatenate([is_zero[:100], is_zero[-100:]]).mean()),
            "outer100cols_zero_frac": float(np.concatenate([is_zero[:, :100], is_zero[:, -100:]],
                                                           axis=1).mean()),
            "allzero_rows": int(np.count_nonzero(is_zero.all(axis=1))),
            "allzero_cols": int(np.count_nonzero(is_zero.all(axis=0))),
        }
        # 逐像素无效的**互斥归因**（取最低置位）
        prim = np.zeros(6, dtype=np.int64)
        acc = np.zeros((ny, nx), dtype=bool)
        for i, b in enumerate((F_NONFINITE, F_ZERO, F_NEGATIVE, F_EDGE, F_EXTREME_NEG)):
            m = ((flags & b) != 0) & ~acc
            prim[i] = int(np.count_nonzero(m))
            acc |= m
        prim_counts = {FLAG_NAMES[b]: int(prim[i]) for i, b in enumerate(
            (F_NONFINITE, F_ZERO, F_NEGATIVE, F_EDGE, F_EXTREME_NEG))}
        del acc, is_edge
        meta: Dict[str, Any] = {
            "band": hdr.get("FILTER"), "file": p.name, "shape": [int(ny), int(nx)],
            "header": {k: hdr.get(k) for k in
                       ("TELESCOP", "INSTRUME", "DETECTOR", "FILTER", "APERTURE", "TARGNAME",
                        "PROPOSID", "EXPTIME", "TEXPTIME", "DARKTIME", "NDRIZIM", "BUNIT",
                        "PHOTFLAM", "PHOTPLAM", "PHOTZPT", "PHOTMODE", "CCDGAIN", "ATODGNA",
                        "READNSEA", "READNSED", "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2",
                        "CD1_1", "CD1_2", "CD2_1", "CD2_2", "CTYPE1", "CTYPE2", "ORIENTAT",
                        "PA_V3", "SUNANGLE", "MOONANGL", "EXPSTART", "EXPEND", "CAL_VER",
                        "ASN_ID", "WCSNAME", "D001SCAL", "D001ISCL", "D001KERN", "D001FVAL",
                        "D001WTSC", "PFLTFILE", "MDRIZTAB") if k in hdr},
            "counts": cnt,
            "fractions": {k: cnt[k] / cnt["total"] for k in ("nonfinite", "zero", "neg", "pos")},
            "min": None if not math.isfinite(gmin) else gmin,
            "max": None if not math.isfinite(gmax) else gmax,
            "valid_pixels": int(valid.sum()),
            "valid_fraction": float(valid.mean()),
            "invalid_by_bit": {FLAG_NAMES[b]: int(np.count_nonzero(flags & b))
                               for b in (F_NONFINITE, F_ZERO, F_NEGATIVE, F_EDGE, F_EXTREME_NEG)},
            "spike": {"count": int(np.count_nonzero(flags & F_SPIKE)),
                      "top20": sorted(spike_vals, key=lambda t: -t[2])[:20],
                      "criterion": {"k_sigma": cfg.k_spike_sigma, "r_gt": cfg.r_spike,
                                    "abs_floor_e": cfg.spike_abs_floor_e,
                                    "note": "只标记不剔除；见模块 docstring 的 PSF 上界标定"}},
            "invalid_primary_reason": prim_counts,
            "edge_definition": "与画幅边框 4-连通 的 (ZERO|NONFINITE) 连通域 = drizzle 无覆盖拼接边界",
            "n_edge_components_touching_border": int(border_labels.size),
            "interior_zero_pixels": interior_zero,
            "interior_zero_fraction_of_frame": interior_zero / cnt["total"],
            "border_1px_zero_fraction": border_zero_frac,
            "edge_zero_stats": edge_stats,
            "valid_rect_max_px": [int(v) for v in rect_max],
            "valid_rect_max_shape": [int(rect_max[2] - rect_max[0]), int(rect_max[3] - rect_max[1])],
            "valid_rect_max_fraction": float((rect_max[2] - rect_max[0])
                                             * (rect_max[3] - rect_max[1])) / cnt["total"],
            "footprint_rect_px": [int(v) for v in rect_foot],
            "footprint_rect_shape": [int(rect_foot[2] - rect_foot[0]),
                                     int(rect_foot[3] - rect_foot[1])],
            "footprint_rect_fraction": float((rect_foot[2] - rect_foot[0])
                                             * (rect_foot[3] - rect_foot[1])) / cnt["total"],
            "row_zero_fraction_profile": {
                "first20": (row_zero[:20] / nx).round(6).tolist(),
                "last20": (row_zero[-20:] / nx).round(6).tolist()},
            "col_zero_fraction_profile": {
                "first20": (col_zero[:20] / ny).round(6).tolist(),
                "last20": (col_zero[-20:] / ny).round(6).tolist()},
            "block_robust": {"block_px": block, "n_blocks": [int(nby), int(nbx)],
                             "median_of_block_medians": float(np.nanmedian(blk_med)),
                             "median_of_block_sigmas": float(np.nanmedian(blk_sig))},
            "nonzero_rows": int(np.count_nonzero(row_zero < nx)),
            "nonzero_cols": int(np.count_nonzero(col_zero < ny)),
            "allzero_rows": int(np.count_nonzero(row_zero == nx)),
            "allzero_cols": int(np.count_nonzero(col_zero == ny)),
            "elapsed_s": time.time() - t0,
            "config": cfg.as_dict(),
        }
    return valid, flags, meta


def max_axis_aligned_rect(mask: np.ndarray) -> Tuple[int, int, int, int]:
    """布尔图上面积最大的全 True 轴对齐矩形（直方图 + 单调栈）。返回 (y0,x0,y1,x1)。"""
    ny, nx = mask.shape
    if ny == 0 or nx == 0:
        return (0, 0, 0, 0)
    heights = np.zeros(nx, dtype=np.int64)
    best = (0, 0, 0, 0, 0)
    for y in range(ny):
        heights = np.where(mask[y], heights + 1, 0)
        stack: List[int] = []
        h = heights.tolist() + [0]
        for x in range(nx + 1):
            while stack and h[stack[-1]] >= h[x]:
                hh = h[stack.pop()]
                x0 = (stack[-1] + 1) if stack else 0
                area = hh * (x - x0)
                if hh > 0 and area > best[0]:
                    best = (area, y - hh + 1, x0, y + 1, x)
            stack.append(x)
    return (int(best[1]), int(best[2]), int(best[3]), int(best[4]))


def max_rect_pixels(bad: np.ndarray, ny: int, nx: int, down: int = 8
                    ) -> Tuple[int, int, int, int]:
    """(ny,nx) 布尔「坏」图上最大全好轴对齐矩形：8x8 降采样粗定位 + 逐像素外扩。"""
    dy, dx = ny // down, nx // down
    if dy == 0 or dx == 0:
        return (0, 0, 0, 0)
    bad_ds = bad[:dy * down, :dx * down].reshape(dy, down, dx, down).any(axis=(1, 3))
    r = max_axis_aligned_rect(~bad_ds)
    return expand_rect_pixelwise(bad, r, down, ny, nx)


def expand_rect_pixelwise(badcore: np.ndarray, rect_blk: Tuple[int, int, int, int],
                          block: int, ny: int, nx: int) -> Tuple[int, int, int, int]:
    """把块级矩形逐像素外扩，只要新增的行/列在矩形跨度内 100% 无坏核像素。"""
    y0, x0, y1, x1 = (rect_blk[0] * block, rect_blk[1] * block,
                      min(rect_blk[2] * block, ny), min(rect_blk[3] * block, nx))
    if y1 <= y0 or x1 <= x0:
        return (0, 0, 0, 0)
    changed = True
    while changed:
        changed = False
        if y0 > 0 and not badcore[y0 - 1, x0:x1].any():
            y0 -= 1; changed = True
        if y1 < ny and not badcore[y1, x0:x1].any():
            y1 += 1; changed = True
        if x0 > 0 and not badcore[y0:y1, x0 - 1].any():
            x0 -= 1; changed = True
        if x1 < nx and not badcore[y0:y1, x1].any():
            x1 += 1; changed = True
    return (y0, x0, y1, x1)


# ---------------------------------------------------------------------------
# 3. 落盘 / 读回
# ---------------------------------------------------------------------------
def write_mask_products(valid: np.ndarray, flags: np.ndarray, meta: Dict[str, Any],
                        outdir: Path) -> Dict[str, str]:
    from astropy.io import fits
    outdir.mkdir(parents=True, exist_ok=True)
    band = meta["band"]
    hdr = fits.Header()
    hdr["BUNIT"] = ("1", "1 = valid, 0 = invalid")
    hdr["BAND"] = (band, "HST WFC3/UVIS filter")
    hdr["SRCFILE"] = (meta["file"], "src drz frame")
    hdr["NDRIZIM"] = (meta["header"].get("NDRIZIM", 0), "n exposures combined in drz")
    hdr["EXPTIME"] = (meta["header"].get("EXPTIME", 0.0), "[s] real exposure")
    hdr["PHOTFLAM"] = (meta["header"].get("PHOTFLAM", 0.0), "absolute flux calib")
    hdr["PHOTPLAM"] = (meta["header"].get("PHOTPLAM", 0.0), "[Angstrom] pivot wavelength")
    hdr["VALIDFRC"] = (meta["valid_fraction"], "valid pixel fraction")
    hdr["RECTY0"] = (meta["valid_rect_max_px"][0], "max all-valid rect y0")
    hdr["RECTX0"] = (meta["valid_rect_max_px"][1], "max all-valid rect x0")
    hdr["RECTY1"] = (meta["valid_rect_max_px"][2], "max all-valid rect y1 (excl)")
    hdr["RECTX1"] = (meta["valid_rect_max_px"][3], "max all-valid rect x1 (excl)")
    hdr.add_history("M16-SCENE validity mask; bits: 1 NONFINITE 2 ZERO 4 NEGATIVE "
                    "8 EDGE 16 EXTREME_NEG 32 SPIKE(suspect, kept valid)")
    hdr.add_history("built by 实验/shared/synthetic/m16_mask.py; NOT committed (run/ only)")
    vp = outdir / ("%s_valid.fits" % band)
    fp = outdir / ("%s_flags.fits" % band)
    fits.PrimaryHDU(data=valid, header=hdr).writeto(vp, overwrite=True)
    fits.PrimaryHDU(data=flags, header=hdr).writeto(fp, overwrite=True)
    mp = outdir / ("%s_mask_meta.json" % band)
    with open(mp, "w", encoding="utf-8") as fh:
        json.dump(meta, fh, indent=1, ensure_ascii=False, default=float)
    return {"valid_fits": str(vp), "flags_fits": str(fp), "meta_json": str(mp)}


def load_valid_mask(band: str, mask_dir) -> Tuple[np.ndarray, Dict[str, Any]]:
    """读回有效域掩膜（uint8 0/1）与 meta。"""
    from astropy.io import fits
    d = Path(mask_dir)
    with fits.open(d / ("%s_valid.fits" % band), memmap=True) as h:
        v = np.asarray(h[0].data)
    with open(d / ("%s_mask_meta.json" % band), encoding="utf-8") as fh:
        m = json.load(fh)
    return v, m


# ---------------------------------------------------------------------------
# 4. 自检（负例：已知注入的坏点必须被检出）
# ---------------------------------------------------------------------------
def selftest(verbose: bool = True) -> Dict[str, Any]:
    """判据自检：合成帧上注入已知坏点，判据必须红；干净帧上必须不误报。"""
    cfg = MaskConfig()
    rng = np.random.default_rng(20260924)
    ny, nx = 512, 512
    block = cfg.block
    base = np.full((ny, nx), 10.0, dtype=np.float32)
    base += (0.05 * rng.normal(size=(ny, nx))).astype(np.float32)
    spikes = [(100, 120), (300, 400), (255, 256)]
    negs = [(50, 60), (400, 100)]
    zeros = [(10, 10), (11, 10), (12, 10)]
    truth = np.zeros((ny, nx), dtype=bool)
    bad = base.copy()
    for (y, x) in spikes:
        bad[y, x] = 500.0; truth[y, x] = True
    for (y, x) in negs:
        bad[y, x] = -50.0; truth[y, x] = True
    for (y, x) in zeros:
        bad[y, x] = 0.0; truth[y, x] = True
    from scipy.ndimage import gaussian_filter, maximum_filter
    stars = [(200, 200), (201, 400), (350, 50)]
    star_truth = np.zeros((ny, nx), dtype=bool)
    for (y, x) in stars:
        s = np.zeros((ny, nx), dtype=np.float32)
        s[y, x] = 1000.0
        s = gaussian_filter(s, 0.85)
        s *= 1000.0 / max(float(s.max()), 1e-9)
        bad += s
        star_truth |= s > 0.05 * float(s.max())
    res: Dict[str, Any] = {}
    m, sg = block_robust_stats(bad, block)
    bg = upsample_blocks(m, block, (ny, nx))
    sig = np.maximum(upsample_blocks(sg, block, (ny, nx)), cfg.sigma_floor)
    dev = bad - bg
    mx8 = maximum_filter(bad, footprint=RING8, mode="nearest")
    bright = dev > cfg.k_spike_sigma * sig
    r = np.where(bright, (bad - mx8) / np.maximum(dev, 1e-12), -np.inf)
    sp = bright & (r > cfg.r_spike)
    hit = int(np.count_nonzero(sp & truth))
    res["injected_bad"] = len(spikes) + len(negs) + len(zeros)
    res["spike_detected_of_injected_spikes"] = int(np.count_nonzero(sp & truth))
    res["n_injected_point_spikes"] = len(spikes)
    res["spike_false_positive_on_stars"] = int(np.count_nonzero(sp & star_truth))
    res["spike_total"] = int(sp.sum())
    res["extreme_neg_detected"] = int(np.count_nonzero(dev < -cfg.k_extreme * sig))
    # 判据写死：SPIKE 必须抓全部注入的单像素尖峰；EXTREME_NEG 必须抓全部注入负值
    n_spike_hit = int(sum(1 for (y, x) in spikes if sp[y, x]))
    n_neg_hit = int(sum(1 for (y, x) in negs
                        if (bad[y, x] - bg[y, x]) < -cfg.k_extreme * sig[y, x]))
    res["spike_hit_of_injected"] = n_spike_hit
    res["neg_hit_of_injected"] = n_neg_hit
    res["verdict_spike_recall"] = bool(n_spike_hit == len(spikes))
    res["verdict_spike_no_star_fp"] = bool(res["spike_false_positive_on_stars"] == 0)
    res["verdict_neg_recall"] = bool(n_neg_hit == len(negs))
    res["all_pass"] = bool(res["verdict_spike_recall"] and res["verdict_spike_no_star_fp"]
                           and res["verdict_neg_recall"])
    if verbose:
        print(json.dumps(res, indent=2, ensure_ascii=False, default=float))
    return res


# ---------------------------------------------------------------------------
# 5. 驱动
# ---------------------------------------------------------------------------
def build_all(frames_dir: Path, outdir: Path, *, cfg: Optional[MaskConfig] = None,
              bands: Tuple[str, ...] = BAND_ORDER, verbose: bool = True) -> Dict[str, Any]:
    cfg = cfg or MaskConfig()
    outdir = Path(outdir)
    man: Dict[str, Any] = {"frames_dir": str(frames_dir), "outdir": str(outdir),
                           "config": cfg.as_dict(),
                           "built_at": time.strftime("%Y-%m-%dT%H:%M:%S"), "frames": {}}
    for band in bands:
        f = frames_dir / BANDS[band]["file"]
        if not f.exists():
            man["frames"][band] = {"status": "MISSING", "path": str(f)}
            continue
        valid, flags, meta = build_frame_mask(f, cfg, verbose=verbose)
        paths = write_mask_products(valid, flags, meta, outdir)
        rec = {"status": "OK", "valid_fraction": meta["valid_fraction"],
               "invalid_by_bit": meta["invalid_by_bit"],
               "invalid_primary_reason": meta["invalid_primary_reason"],
               "spike": meta["spike"]["count"],
               "valid_rect_max_px": meta["valid_rect_max_px"],
               "footprint_rect_px": meta["footprint_rect_px"],
               "interior_zero_pixels": meta["interior_zero_pixels"],
               "border_1px_zero_fraction": meta["border_1px_zero_fraction"],
               "interior_zero_pixels": meta["interior_zero_pixels"],
               "elapsed_s": meta["elapsed_s"]}
        rec.update(paths)
        man["frames"][band] = rec
        if verbose:
            print("[mask] %-6s valid=%.5f zero=%.5f neg=%.2e edge=%.5f extreme=%d "
                  "spike=%d rectmax=%s footrect=%s %.1fs" % (
                      band, meta["valid_fraction"], meta["fractions"]["zero"],
                      meta["fractions"]["neg"], meta["invalid_primary_reason"]["EDGE"],
                      meta["invalid_primary_reason"]["EXTREME_NEG"], meta["spike"]["count"],
                      meta["valid_rect_max_shape"], meta["footprint_rect_shape"],
                      meta["elapsed_s"]))
    return man


def main() -> int:
    ap = argparse.ArgumentParser(description="M16 有效域掩膜生成器")
    ap.add_argument("--frames-dir", type=str, default="testdata/HST_M16")
    ap.add_argument("--outdir", type=str, default="run/reverse_verify/m16_scene/masks")
    ap.add_argument("--bands", type=str, default=",".join(BAND_ORDER))
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return 0 if selftest()["all_pass"] else 1
    fd = Path(a.frames_dir)
    if not fd.is_absolute():
        fd = ROOT / fd
    od = Path(a.outdir)
    if not od.is_absolute():
        od = ROOT / od
    man = build_all(fd, od, bands=tuple(a.bands.split(",")))
    with open(od / "mask_build_manifest.json", "w", encoding="utf-8") as fh:
        json.dump(man, fh, indent=1, ensure_ascii=False, default=float)
    print("[mask] manifest -> %s" % (od / "mask_build_manifest.json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
