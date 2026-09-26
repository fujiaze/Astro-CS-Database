# -*- coding: utf-8 -*-
"""P5 补实验公共库（独立审计 · 补实验-5.07与relstep）。

独立重实现，不 import 仓库任何 Python、不跑 eng/**。几何与口径常量按被审计
文档登记口径重写（docs/science/PHASE2_UPM.md §7a/§9a/§16、11_upm.md §1/§4、
实验/additive-sky-seamless/README.md §4.1）：
- tile 512x512、8x8 control cell（cell=64 px）、帧 A-D 覆盖列带与边界表；
- control 估计 = patch median + 1.4826*MAD 亮端裁剪（sampler 登记口径）；
- control_ivar = 1/(k_corr*(pi/2)*sigma_bg^2/N_retained)，k_corr=1.4；
- 接缝度量 = 覆盖子集边界处、二阶多项式基线扣除后的有符号电平台阶
  （halfwin=8, base_win=64, order=2, min_valid=8；对称挖除 ±(halfwin+2)）。
固定 seed：SEED_BASE=20250926（本补实验专用，写死，无环境随机源）。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

SEED_BASE = 20250926
HERE = Path(__file__).resolve().parent
RESULTS = HERE.parent / "results"

TILE_PX = 512
GRID = 8
CELL = TILE_PX // GRID
K_CORR = 1.4
RN_E = 10.0

FRAME_ORDER = ["A", "B", "C", "D"]
# 帧覆盖的 control cell 列（x 向条带，行全覆盖）；覆盖子集在 cell 边界突变
COVER_GX = {"A": [0, 1, 2], "B": [2, 3, 4], "C": [4, 5, 6], "D": [6, 7]}
BOUNDARIES = [128, 192, 256, 320, 384, 448]  # cell 边界 2..7

# 帧间天光差真值系数（与被审单元 SKY_TRUE 同源口径：offset / 平面项 @ u=±1）
SKY_TRUE = {
    "A": dict(off=0.0, pl=(0.0, 0.0)),
    "B": dict(off=6.0, pl=(2.0, -1.5)),
    "C": dict(off=-4.0, pl=(-1.0, 2.5)),
    "D": dict(off=9.0, pl=(1.0, 1.0)),
}
# 基外条纹注入：逐帧幅度形状与相位表（沿用原扫描发布的几何常量，保证同口径）
OOB_SHAPE = {"A": 0.0, "B": 1.0, "C": -0.8, "D": 0.6}
OOB_PHASE = {"A": (0.0, 0.0, 0.0), "B": (0.7, 1.1, 2.3),
             "C": (2.1, 0.4, 4.0), "D": (3.3, 2.7, 1.2)}
OOB_AMP = 8.0


def derive_rng(tag: str) -> np.random.Generator:
    """由固定 seed 基 + 标签派生确定性生成器。"""
    h = hashlib.sha256(("P5C|%d|%s" % (SEED_BASE, tag)).encode("utf-8")).digest()
    return np.random.default_rng(int.from_bytes(h[:8], "little"))


def coverage_mask(name: str) -> np.ndarray:
    m = np.zeros((GRID, GRID), dtype=bool)
    for gx in COVER_GX[name]:
        m[:, gx] = True
    return m


def coverage_px(name: str) -> np.ndarray:
    cm = np.zeros((TILE_PX, TILE_PX), dtype=bool)
    cov = coverage_mask(name)
    for gx in range(GRID):
        if cov[:, gx].any():
            cm[:, gx * CELL:(gx + 1) * CELL] = True
    return cm


# ---------------------------------------------------------------- 真值场
def signal_field(level: float = 200.0) -> np.ndarray:
    """解析平滑信号模板（多尺度正弦，帧间连续），中位缩放到 level [e-]。

    补实验不用 HST 真实模板（保持纯 numpy）；信号帧间连续、进入公共面，
    对帧间相对对齐无系统影响（报告诚实边界登记）。
    """
    yy, xx = np.mgrid[0:TILE_PX, 0:TILE_PX]
    rng = derive_rng("signal")
    out = np.zeros((TILE_PX, TILE_PX))
    for amp, w, ang in ((60.0, 300.0, 0.7), (35.0, 130.0, 2.1),
                        (18.0, 57.0, 4.0), (8.0, 23.0, 5.3)):
        kx = np.cos(ang) * 2 * np.pi / w
        ky = np.sin(ang) * 2 * np.pi / w
        ph = rng.uniform(0, 2 * np.pi)
        out += amp * np.sin(kx * xx + ky * yy + ph)
    # 弱信号（std=level 的 2.5%）：帧间公共分量在 delta 中相消，但信号结构的
    # cell 中位数会经单帧列填充泄漏进校正场；压低幅度使 control 中位数近似
    # 纯背景，与原单元星点掩膜后的口径等效（报告诚实边界登记）。
    out = out / np.std(out) * (level * 0.025)
    return level + out


def base_sky(nm: str, b0: float = 100.0) -> np.ndarray:
    """帧 k 的可表示（低阶）天光真值场：B0 + offset + 平面项。"""
    yy, xx = np.mgrid[0:TILE_PX, 0:TILE_PX]
    u = (xx - TILE_PX / 2.0) / (TILE_PX / 2.0)
    v = (yy - TILE_PX / 2.0) / (TILE_PX / 2.0)
    c = SKY_TRUE[nm]
    return b0 + c["off"] + c["pl"][0] * u + c["pl"][1] * v


def stripe_term(nm: str, wav: float, amp: float = OOB_AMP) -> np.ndarray:
    """沿 y 相干的 x 向条纹（帧间天光差的不可表示分量），同原扫描口径：
    两成分（wav 与 0.41*wav），逐帧形状系数与相位取发布几何常量。"""
    yy, xx = np.mgrid[0:TILE_PX, 0:TILE_PX]
    a = OOB_SHAPE[nm]
    p = OOB_PHASE[nm]
    return (amp * a * np.sin(2 * np.pi * xx / wav + p[0])
            + 0.5 * amp * a * np.sin(2 * np.pi * xx / (wav * 0.41) + p[2]))


def synth_frame(signal: np.ndarray, sky: np.ndarray, rng: np.random.Generator,
                noisy: bool = True, rn_e: float = RN_E) -> np.ndarray:
    """物理前向：Poisson(信号+天光)+高斯读噪；noisy=False 时保留确定性别。"""
    lam = np.clip(signal + sky, 0.0, None)
    if not noisy:
        return lam.copy()
    img = rng.poisson(lam).astype(np.float64)
    img += rng.normal(0.0, rn_e, size=img.shape)
    return img


# ---------------------------------------------------------------- control 估计
def patch_estimate(img: np.ndarray, gx: int, gy: int,
                   clip_sigma: float = 3.0, clip_iters: int = 3):
    """patch median + 1.4826*MAD 尺度 + 亮端单侧裁剪（sampler 登记口径）。"""
    sl = (slice(gy * CELL, (gy + 1) * CELL), slice(gx * CELL, (gx + 1) * CELL))
    v = img[sl].ravel()
    n_total = v.size
    keep = np.ones(n_total, dtype=bool)
    for _ in range(clip_iters):
        med = np.median(v[keep])
        mad = 1.4826 * np.median(np.abs(v[keep] - med))
        s = max(mad, 1e-9)
        newkeep = v <= med + clip_sigma * s
        if newkeep.sum() == keep.sum():
            break
        keep = newkeep
    vv = v[keep]
    value = float(np.median(vv))
    sigma = float(1.4826 * np.median(np.abs(vv - np.median(vv))))
    return value, sigma, int(n_total), int(vv.size)


def control_ivar(sigma_bg: float, n_retained: int,
                 k_corr: float = K_CORR) -> float:
    var = k_corr * (np.pi / 2.0) * sigma_bg ** 2 / max(n_retained, 1)
    return 1.0 / var if var > 0 else float("nan")


def control_table(frames: dict) -> dict:
    """全部 (帧, cell) 的 control 值与 ivar（本场景无星点，掩膜 = 无）。"""
    ctrl = {}
    for nm, img in frames.items():
        cov = coverage_mask(nm)
        for gy in range(GRID):
            for gx in range(GRID):
                if not cov[gy, gx]:
                    continue
                val, sig, ntot, nret = patch_estimate(img, gx, gy)
                ctrl[(nm, gx, gy)] = dict(value=val, sigma=sig,
                                          n_total=ntot, n_retained=nret,
                                          ivar=control_ivar(sig, nret))
    return ctrl


# ---------------------------------------------------------------- 公共面与校正
def common_plane_fill(ctrl: dict, fill_single: bool = True):
    """公共面 B_ref + 逐帧校正 delta（文档口径的最小忠实实现）。

    模型（11_upm §1/§4.3）：全帧联合构建公共面 B_ref(x)，每帧校正
    delta_k = y_k - B_ref，多退少补把各帧对齐到公共面。
    - B_ref 在 ≥2 帧覆盖的列上 = 各帧 control 值的 ivar 加权均值；
    - 单帧列（0,1,3,5,7）：fill_single=True 按已定列线性内插/常数外推
      （对应 "harmonic continuation 填单帧区"）；fill_single=False 为
      gauge 变体（单帧列保留自身电平）。
    返回 (bref[GRID,GRID], delta{nm:[GRID,GRID]})，未覆盖 = nan。
    """
    names = sorted({k[0] for k in ctrl})
    y = {nm: np.full((GRID, GRID), np.nan) for nm in names}
    w = {nm: np.full((GRID, GRID), np.nan) for nm in names}
    for (nm, gx, gy), rec in ctrl.items():
        y[nm][gy, gx] = rec["value"]
        w[nm][gy, gx] = rec["ivar"]
    ncover = np.zeros(GRID, dtype=int)
    for gx in range(GRID):
        ncover[gx] = sum(1 for nm in names if coverage_mask(nm)[:, gx].any())
    det_cols = [gx for gx in range(GRID) if ncover[gx] >= 2]
    bref = np.full((GRID, GRID), np.nan)
    wsum = np.zeros((GRID, GRID))
    ysum = np.zeros((GRID, GRID))
    for nm in names:
        m = np.isfinite(y[nm]) & np.isfinite(w[nm])
        wsum[m] += w[nm][m]
        ysum[m] += w[nm][m] * y[nm][m]
    multi = (wsum > 0) & (np.tile(ncover, (GRID, 1)) >= 2)
    bref[multi] = (ysum / np.maximum(wsum, 1e-300))[multi]
    if det_cols:
        gxs = np.arange(GRID)
        for gy in range(GRID):
            vals = bref[gy, det_cols]
            filled = np.interp(gxs, np.array(det_cols, dtype=float), vals)
            for gx in range(GRID):
                if ncover[gx] == 1:
                    nm_single = next(nm for nm in names
                                     if coverage_mask(nm)[gy, gx])
                    bref[gy, gx] = filled[gx] if fill_single \
                        else y[nm_single][gy, gx]
    delta = {}
    for nm in names:
        d = np.full((GRID, GRID), np.nan)
        m = np.isfinite(y[nm]) & np.isfinite(bref)
        d[m] = y[nm][m] - bref[m]
        delta[nm] = d
    return bref, delta


def bilinear_from_cells(vals: np.ndarray) -> np.ndarray:
    """cell 值 -> 像素域双线性连续场（cell 中心节点，边界常数外拓）。"""
    gy, gx = np.mgrid[0:TILE_PX, 0:TILE_PX]
    fx = np.clip(gx / CELL - 0.5, 0, GRID - 1 - 1e-9)
    fy = np.clip(gy / CELL - 0.5, 0, GRID - 1 - 1e-9)
    x0 = np.floor(fx).astype(int)
    y0 = np.floor(fy).astype(int)
    tx = fx - x0
    ty = fy - y0
    return (vals[y0, x0] * (1 - tx) * (1 - ty)
            + vals[y0, x0 + 1] * tx * (1 - ty)
            + vals[y0 + 1, x0] * (1 - tx) * ty
            + vals[y0 + 1, x0 + 1] * tx * ty)


def fill_nearest_rows(vals: np.ndarray) -> np.ndarray:
    """按行用最近有限 cell 值外推填满（避免未覆盖 cell 以 0 拖入双线性插值）。"""
    out = vals.copy()
    for gy in range(GRID):
        row = out[gy]
        fin = np.isfinite(row)
        if not fin.any():
            continue
        idx = np.arange(GRID)
        row[~fin] = np.interp(idx[~fin], idx[fin], row[fin])
    return out


def stack_mosaic(frames: dict, delta: dict, ctrl: dict) -> np.ndarray:
    """加权叠加：mosaic(p) = sum_k w_k(p) (raw_k - delta_k)(p) / sum w_k(p)。"""
    num = np.zeros((TILE_PX, TILE_PX))
    den = np.zeros((TILE_PX, TILE_PX))
    for nm, img in frames.items():
        d = bilinear_from_cells(fill_nearest_rows(delta[nm]))
        w = np.zeros((GRID, GRID))
        for (n2, gx, gy), rec in ctrl.items():
            if n2 == nm:
                w[gy, gx] = rec["ivar"]
        wpix = bilinear_from_cells(w)
        cm = coverage_px(nm)
        num += np.where(cm, wpix * (img - d), 0.0)
        den += np.where(cm, wpix, 0.0)
    with np.errstate(invalid="ignore", divide="ignore"):
        return np.where(den > 0, num / np.maximum(den, 1e-300), np.nan)


# ---------------------------------------------------------------- 接缝度量
def seam_steps(mosaic, boundaries=None, halfwin=8, base_win=64, order=2,
               min_valid=8):
    """覆盖子集边界处的有符号电平台阶（登记口径的独立重实现）。

    m(x) = 对 y 取 nanmedian 剖面；每边界两侧 base_win 内（对称挖除
    ±(halfwin+2)）拟合 order 阶多项式基线；step = median(右 halfwin 残差)
    - median(左 halfwin 残差)；rel = step / 窗口内中位电平。
    """
    if boundaries is None:
        boundaries = BOUNDARIES
    m = np.nanmedian(mosaic, axis=0)
    out = []
    for xb in boundaries:
        lo, hi = max(0, xb - base_win), min(TILE_PX, xb + base_win)
        xs = np.arange(lo, hi)
        good = np.isfinite(m[lo:hi])
        excl = (xs >= xb - halfwin - 2) & (xs <= xb + halfwin + 2)
        fitm = good & (~excl)
        if fitm.sum() < order + 2:
            out.append(dict(x=int(xb), step=np.nan, rel=np.nan))
            continue
        coef = np.polyfit(xs[fitm], m[lo:hi][fitm], order)
        res = m[lo:hi] - np.polyval(coef, xs)
        L = res[(xs >= xb - halfwin) & (xs <= xb - 1)]
        R = res[(xs >= xb) & (xs <= xb + halfwin - 1)]
        if L.size < min_valid or R.size < min_valid:
            out.append(dict(x=int(xb), step=np.nan, rel=np.nan))
            continue
        step = float(np.median(R) - np.median(L))
        bg = float(np.nanmedian(m[lo:hi]))
        out.append(dict(x=int(xb), step=step, rel=step / bg if bg else np.nan))
    return out


def seam_metrics(mosaic):
    st = seam_steps(mosaic)
    steps = np.array([abs(s["step"]) for s in st if np.isfinite(s["step"])])
    rels = np.array([abs(s["rel"]) for s in st if np.isfinite(s["rel"])])
    return dict(seam_max=float(steps.max()) if steps.size else np.nan,
                seam_med=float(np.median(steps)) if steps.size else np.nan,
                rel_max=float(rels.max()) if rels.size else np.nan)


# ---------------------------------------------------------------- RMS 口径
def gauss_highpass_1d(d: np.ndarray, sigma: float = 64.0) -> np.ndarray:
    """纯 numpy 一维高斯高通（nearest 边界）。"""
    r = int(4 * sigma)
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k /= k.sum()
    ext = np.concatenate([np.full(r, d[0]), d, np.full(r, d[-1])])
    return d - np.convolve(ext, k, mode="valid")


def out_of_basis_rms(sky: dict, sigma_px: float = 64.0) -> float:
    """帧间天光差 x 向高通（σ=64 px）残差 RMS 对帧对取平均（原口径）。"""
    names = [nm for nm in FRAME_ORDER if nm in sky]
    vals = []
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            d = np.median(sky[names[i]] - sky[names[j]], axis=0)
            hp = gauss_highpass_1d(d, sigma_px)
            vals.append(float(np.sqrt(np.mean(hp ** 2))))
    return float(np.mean(vals))


def save_json(path: Path, obj) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, ensure_ascii=False),
                    encoding="utf-8")
    print("wrote", path)
