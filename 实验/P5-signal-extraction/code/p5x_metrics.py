"""P5 信号提取：判据口径（接缝、帧间一致性、结构保留度）与加权叠加。

三条判据都定义在产物自己的并集网格上（40 个 order-12 瓦片、512²、叶像素 nside 2²¹，
即 0.1007″/px），不重采样、不插值。

接缝
  · 边界边集 = 瓦片内 4 邻接对 (p,q)，两侧**都有数据**且**覆盖帧集不同**（`covmask(p) ≠ covmask(q)`）；
    足迹外缘不计入（与科学正本「只有两侧都在数据内部的帧边界计入判据」一致）。
  · 有符号台阶 Δ = stack(p) − stack(q)，沿法向 p→q。
  · 相对读数 = Δ / bg，bg 取**局部背景电平**：整幅堆栈的 33×33 窗口中位数，逐边在该图上取值。
  · 逐边界结算：同一批边上比较各路径相对基准的 |Δ|，给改善/恶化计数。

帧间一致性
  · 对覆盖帧数 ≥2 的像素，`s(p) = sqrt(Σ_k w_k (x_k − μ_p)² / Σ_k w_k)`，报其在多帧像素上的中位数。

结构保留度
  · 结构掩膜由**未校正堆栈**冻结：`stack_raw > p99.9`，二值膨胀 2 px；
  · 电平保留 `L = median(stack_path / stack_raw)`（结构掩膜内）；
  · 起伏保留 `B = rms(stack_path − 低通) / rms(stack_raw − 低通)`（结构掩膜内，低通 = 81 px 中值）；
  · 背景噪声 `N = rms(stack − 9 px 中值)`（非结构、覆盖 ≥2 的像素）。
"""

from __future__ import annotations

import numpy as np
from scipy import ndimage

TILE = 512
BG_WIN = 16          # 局部背景窗口半径（像素），整窗 33×33
LOWPASS = 81         # 低通中值窗（起伏保留的尺度）


# ------------------------------------------------------------------ 叠加
def stack_weighted(arrays, prod, weights=None):
    """逆方差加权叠加：只统计有效样本，权重和为零处不除法。"""
    w = (1.0 / np.asarray(prod.var, dtype=np.float64)) if weights is None else np.asarray(weights, dtype=np.float64)
    ok = prod.valid & np.isfinite(arrays) & np.isfinite(w) & (w > 0)
    w = np.where(ok, w, 0.0)
    num = np.nansum(w * np.nan_to_num(arrays), axis=0)
    den = np.nansum(w, axis=0)
    return np.where(den > 0, num / np.maximum(den, 1e-300), np.nan), den


# ------------------------------------------------------------------ 接缝
def seam_edges(prod):
    """覆盖帧集不同的瓦片内邻接边：返回 (p, q) 两组并集网格索引。"""
    loc = prod.local_of
    row, col = loc // TILE, loc % TILE
    r = np.flatnonzero(col < TILE - 1)
    d = np.flatnonzero(row < TILE - 1)
    cov = prod.covmask
    er = r[cov[r] != cov[r + 1]]
    ed = d[cov[d] != cov[d + TILE]]
    p = np.concatenate([er, ed])
    q = np.concatenate([er + 1, ed + TILE])
    nc = prod.ncover
    both = (nc[p] >= 1) & (nc[q] >= 1)          # 只计两侧都有数据的帧边界
    return p[both], q[both]


def local_bg_image(stack_img, prod, win=BG_WIN, dec=4):
    """局部背景电平图：每瓦片先在 1/dec 抽样上做窗口中值，再升采样（等效口径 ≈2·win+1 像素）。"""
    arr = stack_img.reshape(len(prod.tiles), TILE, TILE)
    out = np.empty_like(arr)
    k = max(3, (2 * win + 1) // dec | 1)
    for j in range(arr.shape[0]):
        a = arr[j]
        if not np.any(np.isfinite(a)):
            out[j] = np.nan
            continue
        small = a[::dec, ::dec]
        filled = np.where(np.isfinite(small), small, np.nanmedian(a))
        sm = ndimage.median_filter(filled, size=k, mode="nearest")
        out[j] = np.repeat(np.repeat(sm, dec, axis=0), dec, axis=1)[:TILE, :TILE]
    glob = float(np.nanmedian(stack_img))
    bg = out.reshape(-1)
    return np.where(np.isfinite(bg) & (bg != 0), bg, glob)


def seam_readout(prod, stack_img, p, q, bg_img=None):
    """逐边有符号台阶与相对读数。"""
    a, b = stack_img[p], stack_img[q]
    good = np.isfinite(a) & np.isfinite(b)
    p, q, a, b = p[good], q[good], a[good], b[good]
    if bg_img is None:
        bg_img = local_bg_image(stack_img, prod)
    mid = (p + q) // 2
    bg = bg_img[mid]
    d = a - b
    rel = np.where(np.isfinite(bg) & (bg != 0), d / bg, np.nan)
    return {"p": p, "q": q, "step": d, "rel": rel, "abs": np.abs(d), "n": int(p.size)}


def seam_summary(st):
    r = np.abs(st["rel"])
    a = st["abs"]
    keep = np.isfinite(r)
    return {"n_edges": int(keep.sum()),
            "abs_step_median": float(np.median(a[keep])),
            "abs_step_p95": float(np.percentile(a[keep], 95)),
            "rel_median_abs": float(np.median(r[keep])),
            "rel_p95_abs": float(np.percentile(r[keep], 95)),
            "rel_max_abs": float(np.max(r[keep]))}


def seam_groups(prod, st, bg_img=None):
    """按「离开/进入的帧」归并边界：给逐帧的**有符号系统电平差**（噪声在边内平均掉）。"""
    cm_p = prod.covmask[st["p"]]
    cm_q = prod.covmask[st["q"]]
    x = cm_p ^ cm_q
    rel = st["rel"]
    out = {}
    for k in range(prod.nframe):
        b = (x >> k) & 1
        m = (b == 1) & np.isfinite(rel)
        if not m.any():
            continue
        r = rel[m]
        out[f"frame{k}"] = {"n_edges": int(m.sum()),
                            "mean_rel": float(np.mean(r)),
                            "se_off_mean": float(np.std(r) / np.sqrt(m.sum())),
                            "median_abs_rel": float(np.median(np.abs(r)))}
    m = np.isfinite(rel)
    out["all"] = {"n_edges": int(m.sum()), "mean_abs_rel": float(np.mean(np.abs(rel[m]))),
                  "median_abs_rel": float(np.median(np.abs(rel[m])))}
    return out


def noise_floor(prod, st):
    """边界台阶的噪声下限（由逐像素逆方差和给出）：噪声下台阶的期望绝对值中位。"""
    w = np.where(prod.valid, 1.0 / np.maximum(prod.var, 1e-300), 0.0)
    W = w.sum(axis=0)
    v = np.where(W > 0, 2.0 / np.maximum(W, 1e-300), np.nan)
    x = np.sqrt(v)
    a, b = x[st["p"]], x[st["q"]]
    m = np.isfinite(a) & np.isfinite(b)
    return float(np.median(np.sqrt(a[m] ** 2 + b[m] ** 2)))


def compare_edges(base, new):
    """同一批边的逐边改善/恶化计数（|相对台阶| 变小为改善）。"""
    b, n = np.abs(base["rel"]), np.abs(new["rel"])
    ok = np.isfinite(b) & np.isfinite(n)
    b, n = b[ok], n[ok]
    imp = n < b
    return {"n": int(b.size), "improved": int(imp.sum()), "worsened": int((~imp).sum()),
            "frac_improved": float(imp.mean()),
            "rel_median_base": float(np.median(b)), "rel_median_new": float(np.median(n)),
            "rel_p95_base": float(np.percentile(b, 95)), "rel_p95_new": float(np.percentile(n, 95))}


# ------------------------------------------------------------------ 一致性与结构
def frame_consistency(prod, arrays, weights=None):
    w = (1.0 / np.asarray(prod.var, dtype=np.float64)) if weights is None else np.asarray(weights, dtype=np.float64)
    ok = prod.valid & np.isfinite(arrays) & np.isfinite(w) & (w > 0)
    w = np.where(ok, w, 0.0)
    x = np.nan_to_num(arrays)
    den = np.nansum(w, axis=0)
    mu = np.where(den > 0, np.nansum(w * x, axis=0) / np.maximum(den, 1e-300), np.nan)
    var = np.nansum(w * (x - mu[None, :]) ** 2, axis=0)
    s = np.sqrt(np.where(den > 0, var / np.maximum(den, 1e-300), np.nan))
    m = (prod.ncover >= 2) & np.isfinite(s)
    return {"median": float(np.nanmedian(s[m])), "n_pixels": int(m.sum())}


def structure_mask(prod, ref_img, q=99.9, dilate=2):
    hi = np.nanpercentile(ref_img, q)
    m = np.isfinite(ref_img) & (ref_img > hi)
    return ndimage.binary_dilation(m, iterations=dilate)


def structure_metrics(prod, stack_img, ref_img, mask=None):
    if mask is None:
        mask = structure_mask(prod, ref_img)
    ratio = stack_img[mask] / ref_img[mask]
    ratio = ratio[np.isfinite(ratio)]
    hp = lambda im: np.nan_to_num(im) - ndimage.median_filter(np.nan_to_num(im), size=9)
    lp = lambda im: ndimage.median_filter(np.nan_to_num(im), size=LOWPASS)
    f_new = np.nan_to_num(stack_img)[mask] - lp(stack_img)[mask]
    f_ref = np.nan_to_num(ref_img)[mask] - lp(ref_img)[mask]
    quiet = (prod.ncover >= 2) & ~mask & np.isfinite(stack_img)
    return {"n_struct_px": int(mask.sum()),
            "level_retention_median": float(np.median(ratio)),
            "level_retention_p05": float(np.percentile(ratio, 5)),
            "band_rms_ratio": float(np.sqrt(np.mean(f_new ** 2)) / max(np.sqrt(np.mean(f_ref ** 2)), 1e-300)),
            "noise_highpass_rms": float(np.sqrt(np.mean(hp(stack_img)[quiet] ** 2))),
            "quiet_pixels": int(quiet.sum())}
