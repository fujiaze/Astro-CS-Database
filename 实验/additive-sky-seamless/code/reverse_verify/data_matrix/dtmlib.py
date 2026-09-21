#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DATA-TYPE-MATRIX 实验共享库：渲染 MC、稳健天光 rms、canon 式 PSF 加权提取、度量。

**判据先行**：各 exp 脚本顶部写死阈值，本库只提供估计量，不提供阈值。

估计量约定（与 实验/SCI-B/docs/frame-snr-canon.md 一致）
    sigma_sky   : 逐像素空背景 rms [ADU]，由**分块 sigma 裁剪 std 的中位数**稳健估计
    P_i         : 离散归一化 PSF（sum=1），真值核（合成实验里 PSF 是配置量）
    sigma_i^2   : sigma_sky^2 + max(F,0)*P_i/g          [ADU^2]（天光散粒已含在 sigma_sky）
    F_hat       : sum_i (P_i/sigma_i^2)(I_i - b_hat) / sum_i (P_i^2/sigma_i^2)
    sigma_F     : 1/sqrt(sum_i P_i^2/sigma_i^2)
    SNR         : F_hat / sigma_F
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

import numpy as np

HERE = Path(__file__).resolve()
RV = HERE.parents[1]                       # code/reverse_verify/（provenance）
ROOT = HERE.parents[5]                     # 仓库根
SHARED = ROOT / "实验" / "shared"
sys.path.insert(0, str(SHARED / "synthetic"))
import noise_model as NM      # noqa: E402
import render as R            # noqa: E402

OUT_ROOT = ROOT / "run/reverse_verify/data_matrix"
RESULT_DIR = OUT_ROOT / "results"


# ---------------------------------------------------------------------------
# 渲染
# ---------------------------------------------------------------------------
def load_scene(rel: str) -> Dict[str, Any]:
    """rel 形如 'synthetic/scenes/xxx.json'（相对 reverse_verify/）。"""
    return R.load_scene(SHARED / rel)


def frame_scene(scene: Dict[str, Any], frame_index: int,
                overrides: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """场景 + 第 k 帧覆盖 + 额外覆盖 的**最终有效场景**（与 render_frame 内部一致）。

    必须用它来取天光面/PSF 等参数：per-frame 覆盖写在 scene["frames"][k] 里，
    直接用 base scene 会取到**错误的**天光水平（exp1 首版 R_pred 恒为 1 的根因）。
    """
    frames = scene.get("frames") or [{}]
    sc = R.deep_merge(scene, frames[frame_index] if frame_index < len(frames) else {})
    sc.pop("frames", None)
    return R.deep_merge(sc, overrides or {})


def render_mc(scene: Dict[str, Any], *, frame_index: int, seeds: Sequence[int],
              overrides: Optional[Dict[str, Any]] = None,
              cache: Optional[Dict[Any, Any]] = None
              ) -> List[Tuple[NM.Frame, Dict[str, Any]]]:
    """对同一场景/帧号渲染多个噪声实现（不做磁盘 IO）。"""
    sc = R.deep_merge(scene, overrides or {})
    out = []
    for s in seeds:
        f, truth = R.render_frame(sc, seed=int(s), frame_index=frame_index,
                                  canvas_cache=cache)
        out.append((f, truth))
    return out


# ---------------------------------------------------------------------------
# 稳健天光 rms（分块 sigma 裁剪 std 的中位数）
# ---------------------------------------------------------------------------
def clipped_std(v: np.ndarray, n_sigma: float = 4.0, iters: int = 3) -> float:
    """sigma 裁剪样本 std（一致估计量）。

    默认 **4 sigma**：3 sigma 裁剪对纯高斯有 ~1.2% 的**向下偏差**
    （E[x^2 | |x|<3sigma] = 0.976），4 sigma 降到 ~0.05%，避免把裁剪偏差
    带进方差闭合判据。
    """
    v = np.asarray(v, dtype=float).ravel()
    v = v[np.isfinite(v)]
    for _ in range(iters):
        if v.size < 8:
            break
        mu, sd = float(v.mean()), float(v.std())
        if sd <= 0:
            break
        keep = np.abs(v - mu) <= n_sigma * sd
        if keep.all():
            break
        v = v[keep]
    return float(v.std(ddof=1)) if v.size > 1 else 0.0


def block_sigma_adu(img: np.ndarray, block: int = 32, n_sigma: float = 4.0,
                    mask: Optional[np.ndarray] = None, min_frac: float = 0.3
                    ) -> Tuple[float, np.ndarray]:
    """分块稳健 rms：返回 (块 rms 的中位数, 逐块 rms 图)。

    取**中位数**（而非均值/最小值）以抗源污染。**但仅靠中位数不够**：
    Moffat(beta=4) 的宽翼 + 星云结构会同时抬高几乎所有块（实测使低天光端 sigma
    高估 ~45%，见 exp1 首版 C2 失败），因此本函数支持传入**源掩膜**（见
    sky_mask_from_scene），只统计掩膜外的干净天光像素。
    """
    ny, nx = img.shape
    by, bx = ny // block, nx // block
    rms = np.full((by, bx), np.nan)
    for i in range(by):
        for j in range(bx):
            sl = (slice(i * block, (i + 1) * block), slice(j * block, (j + 1) * block))
            blk = img[sl]
            if mask is not None:
                m = mask[sl]
                if m.mean() < min_frac:
                    continue
                blk = blk[m]
            rms[i, j] = clipped_std(blk, n_sigma=n_sigma)
    return float(np.nanmedian(rms)), rms


def sky_mask_from_scene(scene: Dict[str, Any], truth: Dict[str, Any], *,
                        star_radius_fwhm: float = 6.0,
                        nebula_frac: float = 0.02) -> np.ndarray:
    """由**场景配方 + 真值星表**构造"干净天光"掩膜（True = 可用于估计天光 rms）。

    * 星点：距离任一真值星 > star_radius_fwhm * FWHM 的像素（Moffat 宽翼需大半径）；
    * 星云：解析星云面 < nebula_frac * 峰值 的像素（**用配方算，不用数据猜**）；
    * 真实底场景：无解析星云面 ⇒ 只做星点掩膜，残差结构由实验显式登记。
    """
    ny, nx = truth["shape"]
    mask = np.ones((ny, nx), dtype=bool)
    fwhm = float(scene["psf"]["fwhm_px"])
    r = float(star_radius_fwhm) * fwhm
    for s in truth.get("stars_in_frame", []):
        y0, y1 = max(0, int(s["y"] - r)), min(ny, int(s["y"] + r) + 1)
        x0, x1 = max(0, int(s["x"] - r)), min(nx, int(s["x"] + r) + 1)
        if y0 >= y1 or x0 >= x1:
            continue
        sy, sx = np.mgrid[y0:y1, x0:x1]
        mask[y0:y1, x0:x1] &= ((sy - s["y"]) ** 2 + (sx - s["x"]) ** 2) > r * r
    comps = scene.get("nebula") or []
    if comps:
        ptr = truth.get("pointing", {})
        m = int(ptr.get("canvas_margin_px", 0))
        dy, dx = ptr.get("offset_px", [0, 0])
        cs = (ny + 2 * m, nx + 2 * m)
        neb = R.nebula_surface(cs, comps)
        y0, x0 = m - int(dy), m - int(dx)
        neb = neb[y0:y0 + ny, x0:x0 + nx]
        if neb.max() > 0:
            mask &= neb < float(nebula_frac) * float(neb.max())
    return mask


# ---------------------------------------------------------------------------
# canon 式 PSF 加权提取
# ---------------------------------------------------------------------------
def local_psf(kernel: np.ndarray, y: float, x: float, half: int) -> np.ndarray:
    """以最近整数像素为中心、含亚像素位移的局部 PSF 模型（sum=1）。"""
    cy, cx = int(round(y)), int(round(x))
    canvas = np.zeros((2 * half + 1, 2 * half + 1), dtype=float)
    R.stamp(kernel, canvas, half + (y - cy), half + (x - cx), 1.0)
    return canvas


def extract_star(img_adu: np.ndarray, y: float, x: float, kernel: np.ndarray,
                 *, half: Optional[int] = None, ann_in: Optional[float] = None,
                 ann_out: Optional[float] = None, gain: float = 1.5,
                 sigma_sky_adu: Optional[float] = None,
                 source_term: bool = True) -> Dict[str, float]:
    """PSF 加权最优提取（canon 式 2.2/2.7）。返回 F_hat/sigma_F/SNR/b_hat。"""
    fwhm = None
    kh = kernel.shape[0]
    half = kh // 2 if half is None else int(half)
    fwhm = fwhm or max(2.0, (kh - 1) / 6.0)
    ann_in = 3.0 * fwhm if ann_in is None else ann_in
    ann_out = 5.0 * fwhm if ann_out is None else ann_out
    cy, cx = int(round(y)), int(round(x))
    yy, xx = np.mgrid[0:img_adu.shape[0], 0:img_adu.shape[1]]
    rr = np.hypot(yy - y, xx - x)
    ann = (rr >= ann_in) & (rr <= ann_out)
    b_hat = float(np.median(img_adu[ann]))
    sig_sky = float(sigma_sky_adu if sigma_sky_adu is not None
                    else NM.robust_sigma_adu(img_adu[ann]))
    P = local_psf(kernel, y, x, half)
    sub = img_adu[cy - half:cy + half + 1, cx - half:cx + half + 1].astype(float)
    if sub.shape != P.shape:
        raise ValueError("aperture outside frame")
    d = sub - b_hat
    F = float(np.sum(P * d) / np.sum(P * P))          # 首轮（常数 sigma）
    for _ in range(2):
        var = sig_sky ** 2 + (max(F, 0.0) * P / gain if source_term else 0.0)
        w = P / var
        F = float(np.sum(w * d) / np.sum(w * P))
    var = sig_sky ** 2 + (max(F, 0.0) * P / gain if source_term else 0.0)
    sigma_F = float(1.0 / math.sqrt(np.sum(P * P / var)))
    return {"F_hat_adu": F, "sigma_F_adu": sigma_F, "snr": F / sigma_F if sigma_F > 0 else 0.0,
            "b_hat_adu": b_hat, "sigma_sky_adu": sig_sky,
            "sum_p2": float(np.sum(P * P)), "y": float(y), "x": float(x)}


def canon_snr_pred(F_e: float, sigma_sky_adu: float, P: np.ndarray,
                   gain: float) -> Dict[str, float]:
    """canon 式 (2.1)-(2.3) 的**解析预测** SNR（用真值通量 + 解析天光 rms）。

        sigma_i^2 = sigma_sky^2 + max(F,0)*P_i/g          [ADU^2]
        sigma_F   = 1/sqrt(sum_i P_i^2/sigma_i^2)
        SNR       = (F/g) / sigma_F

    注意：**当源项不可忽略时 SNR 不再正比于 1/sigma_sky**（亮星在低天光端由
    源散粒主导）—— 这是 exp1 首版"用 sigma 比值当预测"失败的原因。
    """
    F_adu = float(F_e) / float(gain)
    var = float(sigma_sky_adu) ** 2 + np.maximum(F_adu, 0.0) * P / float(gain)
    sigma_F = float(1.0 / math.sqrt(np.sum(P * P / var)))
    return {"snr_pred": F_adu / sigma_F, "sigma_F_pred_adu": sigma_F,
            "source_term_peak_frac": float(np.max(np.maximum(F_adu, 0.0) * P / gain / var))}


def pick_star(truth: Dict[str, Any], *, flux_lo: float, flux_hi: float,
              min_sep_px: float = 30.0, margin_px: float = 40.0,
              prefer: str = "bright") -> Optional[Dict[str, float]]:
    """从真值星表选一颗孤立星（亮/暗可选）。"""
    stars = [s for s in truth.get("stars_in_frame", [])
             if flux_lo <= s["flux_e"] <= flux_hi]
    ny, nx = truth["shape"]
    stars = [s for s in stars
             if margin_px <= s["y"] < ny - margin_px and margin_px <= s["x"] < nx - margin_px]
    allst = truth.get("stars_in_frame", [])
    ok = []
    for s in stars:
        sep = min((math.hypot(s["y"] - o["y"], s["x"] - o["x"]) for o in allst
                   if o is not s), default=1e9)
        if sep >= min_sep_px:
            ok.append(s)
    if not ok:
        return None
    key = (lambda s: -s["flux_e"]) if prefer == "bright" else (lambda s: s["flux_e"])
    return sorted(ok, key=key)[0]


# ---------------------------------------------------------------------------
# 结果落盘
# ---------------------------------------------------------------------------
def save_result(name: str, payload: Dict[str, Any]) -> Path:
    RESULT_DIR.mkdir(parents=True, exist_ok=True)
    p = RESULT_DIR / ("%s.json" % name)
    with open(p, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=1, ensure_ascii=False, default=float)
    print("[result] %s" % p)
    return p


def verdict(name: str, ok: bool, detail: str = "") -> Dict[str, Any]:
    print("  [%s] %s %s" % ("PASS" if ok else "FAIL", name, detail))
    return {"name": name, "pass": bool(ok), "detail": detail}
