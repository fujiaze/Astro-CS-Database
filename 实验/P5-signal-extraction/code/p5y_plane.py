"""P5 信号提取（网格合规版）：把各路径的成品放到**与生产 export 同一约定**的平面 WCS 网格上。

生产 `export` 的网格约定（`run/E2E-M42-P5-EXPORT/out/full_p3_whole/output_phase3.fits` 与
`p3_wcs.json` 读出）：TAN 投影、`CD = diag(-0.0005, +0.0005)` 度（1.8″/px，正北朝上、东向左）、
`CRPIX` 取图像中心、采样器 `bilinear`。

本模块：
  1. 按该约定建立输出网格（中心取联合模型的参考中心，尺寸紧贴真实覆盖区外接框）；
  2. 用**面元重采样**把叶像素（nside 2²¹，0.1007″/px）积分进输出像素：逐叶像素算输出像素号，
     用 `bincount` 求 Σw、Σw·x、Σw·x²、有效帧数 —— 空洞只出现在**真实足迹之外**，
     不再有最近邻稀疏填充造成的条纹；
  3. 判据（接缝、帧间一致性、结构保留）在新网格上重算。

重采样是「从细到粗」的积分（18×18 个叶像素入一个输出像素），等价于面积加权平均，与生产的
bilinear 采样同向；不引入任何插值放大。
"""

from __future__ import annotations

import numpy as np
from astropy.wcs import WCS
from scipy import ndimage

CD_DEG = 0.0005          # 生产 export 的 CD（度/像素）
BAND_LOWPASS = 9         # 结构保留的低通窗（输出像素，1.8″ × 9 ≈ 16″）
BG_WIN = 4               # 接缝局部背景窗半径（9×9）


# ------------------------------------------------------------------ 网格
def production_wcs(crval_ra, crval_dec, nx, ny):
    """生产 export 约定的 TAN WCS：CD = diag(-0.0005, +0.0005)，CRPIX 取图像中心。"""
    w = WCS(naxis=2)
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    w.wcs.cunit = ["deg", "deg"]
    w.wcs.crval = [crval_ra, crval_dec]
    w.wcs.crpix = [(nx + 1) / 2.0, (ny + 1) / 2.0]
    w.wcs.cd = np.array([[-CD_DEG, 0.0], [0.0, CD_DEG]])
    return w


def make_grid(prod, margin=8):
    """以联合模型的参考中心为 CRVAL，尺寸紧贴**真实覆盖区**外接框。"""
    cov = prod.ncover >= 1
    ra_c, dec_c = prod.sp["ra0_deg"], prod.sp["dec0_deg"]
    # 估计外接框：把覆盖像素投到以 (ra0,dec0) 为中心的临时大网格上
    w0 = production_wcs(ra_c, dec_c, 1, 1)
    x, y = w0.all_world2pix(prod.ra[cov], prod.dec[cov], 0)
    dx = x.max() - x.min()
    dy = y.max() - y.min()
    nx = int(np.ceil(dx)) + 2 * margin + 2
    ny = int(np.ceil(dy)) + 2 * margin + 2
    # 让覆盖区居中
    cx = 0.5 * (x.max() + x.min())
    cy = 0.5 * (y.max() + y.min())
    w = production_wcs(ra_c, dec_c, 1, 1)
    # 平移 CRPIX 使覆盖中心落在图像中心
    w.wcs.crpix = [(nx + 1) / 2.0 - cx, (ny + 1) / 2.0 - cy]
    return w, nx, ny


# ------------------------------------------------------------------ 重采样
def _bins(prod, w, nx, ny):
    """覆盖叶像素 → 输出像素号的映射（面元）。"""
    cov = prod.ncover >= 1
    sel = np.flatnonzero(cov)
    x, y = w.all_world2pix(prod.ra[sel], prod.dec[sel], 0)
    xi = np.floor(x).astype(np.int64)
    yi = np.floor(y).astype(np.int64)
    keep = (xi >= 0) & (xi < nx) & (yi >= 0) & (yi < ny)
    return {"sel": sel, "keep": keep, "idx": (yi[keep] * nx + xi[keep]).astype(np.int64), "n": nx * ny}


def rebin_sums(prod, w, nx, ny, arrays, weights=None):
    """面元重采样：返回 stack、wsum、覆盖帧集位数、逐帧 (Σw, Σw·x, Σw·x², 计数)。"""
    b = _bins(prod, w, nx, ny)
    sel, keep, idx, n_out = b["sel"], b["keep"], b["idx"], b["n"]
    nf = arrays.shape[0]
    ww = (1.0 / np.asarray(prod.var, float)) if weights is None else np.asarray(weights, float)
    ww = np.where(prod.valid & np.isfinite(arrays) & np.isfinite(ww) & (ww > 0), ww, 0.0)
    W = ww[:, sel][:, keep]
    X = np.nan_to_num(arrays[:, sel][:, keep])
    num = np.zeros(n_out); den = np.zeros(n_out); cnt = np.zeros(n_out, dtype=np.int64)
    bits = np.zeros(n_out, dtype=np.int32)
    frames = []
    q1 = np.zeros((nf, n_out)); q2 = np.zeros((nf, n_out))
    for k in range(nf):
        m = W[k] > 0
        IDX = idx[m]; wk = W[k][m]; xk = X[k][m]
        s1 = np.bincount(IDX, weights=wk * xk, minlength=n_out)
        s2 = np.bincount(IDX, weights=wk * xk * xk, minlength=n_out)
        sd = np.bincount(IDX, weights=wk, minlength=n_out)
        c = np.bincount(IDX, minlength=n_out)
        num += s1; den += sd; cnt += c
        bits |= ((c > 0).astype(np.int32) << k)
        frames.append((sd, s1, s2, c))
        q1[k] = s1; q2[k] = s2
    st = np.where(den > 0, num / np.maximum(den, 1e-300), np.nan)
    return {"stack": st, "num": num, "wsum": den, "ncover": cnt, "bits": bits,
            "frames": frames, "q1": q1, "q2": q2, "grid": (nx, ny), "binned": b}


# ------------------------------------------------------------------ 平面判据
def plane_consistency(frames, q1, q2, ncover):
    """输出像素上的逐帧加权散度中位（覆盖 ≥2）。

    逐帧权重和 `sd`、加权一阶矩 `s1`、二阶矩 `s2` 由面元积分给出，合并池化后算散度。
    """
    nf = q1.shape[0]
    ncover = np.asarray(ncover).reshape(-1)
    SD = np.stack([f[0] for f in frames])
    W = SD.sum(axis=0)
    MU = np.where(W > 0, q1.sum(axis=0) / np.maximum(W, 1e-300), np.nan)
    tot = np.zeros_like(MU)
    for k in range(nf):
        sd = SD[k]
        m = sd > 0
        tot[m] += q2[k][m] - 2 * MU[m] * q1[k][m] + MU[m] ** 2 * sd[m]
    s = np.sqrt(np.where(W > 0, tot / np.maximum(W, 1e-300), np.nan))
    ok = (ncover >= 2) & np.isfinite(s)
    return {"median": float(np.nanmedian(s[ok])), "n_pixels": int(ok.sum())}


def plane_seam_edges(bits, ncover):
    """平面网格上的帧边界边集：4 邻接、两侧都有数据、覆盖帧集不同。"""
    B = np.asarray(bits)
    C = np.asarray(ncover)
    ny, nx = ncover.shape
    p_list, q_list = [], []
    # 水平
    a = B[:, :nx - 1].reshape(-1); b = B[:, 1:].reshape(-1)
    ca = C[:, :nx - 1].reshape(-1); cb = C[:, 1:].reshape(-1)
    m = (ca >= 1) & (cb >= 1) & (a != b)
    ia = (np.arange(nx * ny).reshape(ny, nx)[:, :nx - 1]).reshape(-1)[m]
    p_list.append(ia); q_list.append(ia + 1)
    # 垂直
    a = B[:ny - 1, :].reshape(-1); b = B[1:, :].reshape(-1)
    ca = C[:ny - 1, :].reshape(-1); cb = C[1:, :].reshape(-1)
    m = (ca >= 1) & (cb >= 1) & (a != b)
    ia = (np.arange(nx * ny).reshape(ny, nx)[:ny - 1, :]).reshape(-1)[m]
    p_list.append(ia); q_list.append(ia + nx)
    return np.concatenate(p_list), np.concatenate(q_list)


def plane_bg(stack, win=BG_WIN):
    """局部背景电平：输出像素上的窗口中值（空缺用全局中位填后再滤波）。"""
    img = stack.reshape(stack.shape)
    fin = np.isfinite(img)
    if not fin.any():
        return np.full(img.shape, np.nan)
    glob = float(np.nanmedian(img))
    filled = np.where(fin, img, glob)
    return ndimage.median_filter(filled, size=2 * win + 1, mode="nearest")


def plane_seam(stack, p, q, bg):
    a, b = stack.reshape(-1)[p], stack.reshape(-1)[q]
    g = bg.reshape(-1)
    mid = (p + q) // 2
    ok = np.isfinite(a) & np.isfinite(b) & np.isfinite(g[mid]) & (g[mid] != 0)
    d = (a - b)[ok]
    rel = d / g[mid][ok]
    return {"p": p[ok], "q": q[ok], "step": d, "rel": rel, "abs": np.abs(d),
            "n": int(ok.sum()), "bits_p": None}


def plane_seam_groups(bits, st):
    cm_p = bits.reshape(-1)[st["p"]]
    cm_q = bits.reshape(-1)[st["q"]]
    x = cm_p ^ cm_q
    nf = int(np.log2(max(int(bits.max()), 1))) + 1
    out = {}
    rel = st["rel"]
    for k in range(nf):
        m = (((x >> k) & 1) == 1)
        if not m.any():
            continue
        r = rel[m]
        out[f"frame{k}"] = {"n_edges": int(m.sum()), "mean_rel": float(np.mean(r)),
                            "se_off_mean": float(np.std(r) / np.sqrt(m.sum())),
                            "median_abs_rel": float(np.median(np.abs(r)))}
    m = np.isfinite(rel)
    out["all"] = {"n_edges": int(m.sum()), "mean_abs_rel": float(np.mean(np.abs(rel[m]))),
                  "median_abs_rel": float(np.median(np.abs(rel[m])))}
    return out


def plane_summary(st):
    r = np.abs(st["rel"]); a = st["abs"]
    return {"n_edges": int(st["n"]),
            "abs_step_median": float(np.median(a)),
            "abs_step_p95": float(np.percentile(a, 95)),
            "rel_median_abs": float(np.median(r)),
            "rel_p95_abs": float(np.percentile(r, 95)),
            "rel_max_abs": float(np.max(r))}


def plane_compare(base, new):
    b, n = np.abs(base["rel"]), np.abs(new["rel"])
    ok = np.isfinite(b) & np.isfinite(n)
    b, n = b[ok], n[ok]
    imp = n < b
    return {"n": int(b.size), "improved": int(imp.sum()), "worsened": int((~imp).sum()),
            "frac_improved": float(imp.mean()),
            "rel_median_base": float(np.median(b)), "rel_median_new": float(np.median(n)),
            "rel_p95_base": float(np.percentile(b, 95)), "rel_p95_new": float(np.percentile(n, 95))}


def plane_structure(stack, ref, ncover, q=99.9, dilate=1):
    hi = np.nanpercentile(ref, q)
    m = np.isfinite(ref) & (ref > hi)
    m = ndimage.binary_dilation(m, iterations=dilate)
    ratio = (stack[m] / ref[m])
    ratio = ratio[np.isfinite(ratio)]
    hp = lambda im: np.nan_to_num(im) - ndimage.median_filter(np.nan_to_num(im), size=3)
    lp = lambda im: ndimage.median_filter(np.nan_to_num(im), size=BAND_LOWPASS)
    f_new = np.nan_to_num(stack)[m] - lp(stack)[m]
    f_ref = np.nan_to_num(ref)[m] - lp(ref)[m]
    quiet = (ncover >= 2) & ~m & np.isfinite(stack)
    return {"n_struct_px": int(m.sum()),
            "level_retention_median": float(np.median(ratio)),
            "level_retention_p05": float(np.percentile(ratio, 5)),
            "band_rms_ratio": float(np.sqrt(np.mean(f_new ** 2)) / max(np.sqrt(np.mean(f_ref ** 2)), 1e-300)),
            "noise_highpass_rms": float(np.sqrt(np.mean(hp(stack)[quiet] ** 2))),
            "quiet_pixels": int(quiet.sum())}
