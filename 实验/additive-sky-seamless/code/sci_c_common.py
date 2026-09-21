# 实验/additive-sky-seamless/code/sci_c_common.py
"""SCI-C（加性天光与无接缝叠加）公共库：仿真、真值、度量、生产驱动封装。

设计约束（见 实验/additive-sky-seamless/README.md §2）：
- 固定 seed：SEED_BASE=20260923，所有随机性由 derive_rng(tag) 派生，无时间/环境随机源；
- 所有度量在**保留背景**的前提下定义（退化判据单列，用于反证）；
- 仓内实测一律经 run/SCI-403/{upm_probe,sky_probe}（只读链接生产 libastrocs_phase2.a）。
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

import numpy as np

SEED_BASE = 20260923
ROOT = Path(__file__).resolve().parents[3]
CODE = Path(__file__).resolve().parent          # 本单元 code/（从脚本自身位置推导，随目录改名不失效）
UNIT = CODE.parent                              # 本单元根（实验/additive-sky-seamless/）
RESULTS = UNIT / "results"
FIGS = RESULTS / "figs"
RUN = ROOT / "run" / "SCI-403"

TILE_PX = 512          # 一个 HEALPix tile = 512x512（leaf order = target_order+9）
GRID = 8               # 8x8 control cell / tile（生产 M7-C-001 冻结常量）
CELL = TILE_PX // GRID # 64 px
K_CORR = 1.4           # Drizzle 相关校正冻结默认（SCI-UPM §5）
RN_E = 10.0            # 读出噪声 [e-]
PSF_SIGMA_PX = 1.5     # 注入 PSF（高斯 sigma, px）


# ---------------------------------------------------------------- 固定 seed
def derive_rng(tag: str) -> np.random.Generator:
    h = hashlib.sha256(("SCI-C|%d|%s" % (SEED_BASE, tag)).encode("utf-8")).digest()
    return np.random.default_rng(int.from_bytes(h[:8], "little"))


# ---------------------------------------------------------------- 真值信号
_HST_CACHE = {}


def load_hst_signal(nx: int = TILE_PX, ny: int = TILE_PX, origin=(700, 700),
                    level: float = 200.0, blur: float = PSF_SIGMA_PX) -> np.ndarray:
    key = (nx, ny, tuple(origin), float(level), float(blur))
    if key in _HST_CACHE:
        return _HST_CACHE[key].copy()
    v = _load_hst_signal_uncached(nx, ny, origin, level, blur)
    _HST_CACHE[key] = v
    return v.copy()


def _load_hst_signal_uncached(nx, ny, origin, level, blur) -> np.ndarray:
    """HST M16 F657N 真实结构模板当**纯信号**（最高设计 §12.2 第 1 类数据）。

    读出 -> 中心裁剪 nx*ny -> 非有限置 0 -> 缩放到掩膜外中位 = level [e-]
    -> 高斯 PSF 模糊（sigma=blur px）。
    """
    from astropy.io import fits
    path = ROOT / "testdata" / "HST_M16" / "hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits"
    with fits.open(path, memmap=True) as hdul:
        d = np.asarray(hdul[0].data, dtype=np.float64)
    y0, x0 = origin
    img = d[y0:y0 + ny, x0:x0 + nx].copy()
    img[~np.isfinite(img)] = 0.0
    med = np.median(img)
    img = img / med * level
    if blur > 0:
        from scipy.ndimage import gaussian_filter
        img = gaussian_filter(img, blur, mode="nearest")
    return img


def star_mask(img: np.ndarray, nsig: float = 8.0, dilate: int = 3) -> np.ndarray:
    """星点/高结构掩膜：> median + nsig*1.4826*MAD 的像素按半径 dilate 膨胀。"""
    from scipy.ndimage import binary_dilation
    med = np.median(img)
    mad = 1.4826 * np.median(np.abs(img - med))
    m = img > (med + nsig * max(mad, 1e-9))
    if dilate > 0:
        m = binary_dilation(m, iterations=dilate)
    return m


# ---------------------------------------------------------------- 天光/帧合成
def smooth_sky_field(nx: int, ny: int, rng: np.random.Generator,
                     amps, waves, phases=None) -> np.ndarray:
    """多尺度平滑天光场（正弦叠加）：amps[e-] / waves[px] 一一对应。

    waves 取 > 2*CELL 时，该分量在 8x8 双线性 control 基上**不可精确表示**
    （UPM 基外残差），这正是"纯加性猜想"的适用条件变量。
    """
    yy, xx = np.mgrid[0:ny, 0:nx]
    out = np.zeros((ny, nx), dtype=np.float64)
    if phases is None:
        phases = rng.uniform(0, 2 * np.pi, size=len(amps))
    for a, w, p in zip(amps, waves, phases):
        kx = np.cos(p) * 2 * np.pi / w
        ky = np.sin(p) * 2 * np.pi / w
        ph = rng.uniform(0, 2 * np.pi)
        out += a * np.sin(kx * xx + ky * yy + ph)
    return out


def synth_frame(signal: np.ndarray, sky: np.ndarray, rng: np.random.Generator,
                rn_e: float = RN_E) -> np.ndarray:
    """完整物理前向：源+天光电子域 Poisson -> 读出噪声 Gaussian（电子域）。

    返回 [e-]（gain=1 e-/ADU，见 README 诚实边界）。
    """
    lam = np.clip(signal + sky, 0.0, None)
    img = rng.poisson(lam).astype(np.float64)
    img += rng.normal(0.0, rn_e, size=img.shape)
    return img


def cell_slices():
    for gy in range(GRID):
        for gx in range(GRID):
            yield gx, gy, (slice(gy * CELL, (gy + 1) * CELL),
                           slice(gx * CELL, (gx + 1) * CELL))


def patch_estimate(img: np.ndarray, mask: np.ndarray, gx: int, gy: int,
                   clip_sigma: float = 3.0, clip_iters: int = 3):
    """生产口径的 control/patch 估计：median + 1.4826*MAD + 亮端 sigma-clipping。

    返回 (value, sigma_bg, n_total, n_retained)；与 sampler.cpp / sky_plane.h 的
    P2SkyPatchEstimate 同一数学定义（median + MAD 尺度 + 亮端单侧裁剪 + 保留门）。
    """
    sl = (slice(gy * CELL, (gy + 1) * CELL), slice(gx * CELL, (gx + 1) * CELL))
    v = img[sl][~mask[sl]]
    n_total = v.size
    if n_total == 0:
        return np.nan, np.nan, 0, 0
    v = v.copy()
    keep = np.ones(v.size, dtype=bool)
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


def control_ivar(sigma_bg: float, n_retained: int, k_corr: float = K_CORR) -> float:
    """冻结式 control_variance = k_corr*(pi/2)*sigma_bg^2/N_retained（SCI-UPM §5）。"""
    var = k_corr * (np.pi / 2.0) * sigma_bg ** 2 / max(n_retained, 1)
    return 1.0 / var if var > 0 else np.nan


# ---------------------------------------------------------------- 场景构造
FRAME_IDS = {"A": 1001, "B": 1002, "C": 1003, "D": 1004}
FRAME_ORDER = ["A", "B", "C", "D"]
# 各帧覆盖的 control cell 列（x 向条带；行全覆盖）——覆盖子集在 cell 边界突变
COVER_GX = {"A": [0, 1, 2], "B": [2, 3, 4], "C": [4, 5, 6], "D": [6, 7]}
BOUNDARIES = [128, 192, 256, 320, 384, 448]   # = cell 边界 2,3,4,5,6,7


def coverage_mask(name: str) -> np.ndarray:
    m = np.zeros((GRID, GRID), dtype=bool)
    for gx in COVER_GX[name]:
        m[:, gx] = True
    return m


# 每帧天光系数真值：offset[e-] / 平面项[e- @ u=±1] / 二次项 / 正弦项
SKY_TRUE = {
    "A": dict(off=0.0, pl=(0.0, 0.0), qu=(0.0, 0.0, 0.0), wv=(0.0, 0.0)),
    "B": dict(off=6.0, pl=(2.0, -1.5), qu=(0.0, 0.0, 0.0), wv=(0.0, 0.0)),
    "C": dict(off=-4.0, pl=(-1.0, 2.5), qu=(0.0, 0.0, 0.0), wv=(0.0, 0.0)),
    "D": dict(off=9.0, pl=(1.0, 1.0), qu=(0.0, 0.0, 0.0), wv=(0.0, 0.0)),
}


def sky_field(nm, coeff, nx=TILE_PX, ny=TILE_PX, waves=(420.0, 260.0)):
    """加性天光真值场：B0 + offset + 平面 + 二次 + 正弦（后两项=基外分量来源）。

    u,v ∈ [-1,1]；pl/qu 单位为 [e-]（在 u=±1 处的幅度）；wv 为正弦幅度 [e-]。
    """
    yy, xx = np.mgrid[0:ny, 0:nx]
    u = (xx - nx / 2.0) / (nx / 2.0)
    v = (yy - ny / 2.0) / (ny / 2.0)
    f = (100.0 + coeff["off"]
         + coeff["pl"][0] * u + coeff["pl"][1] * v
         + coeff["qu"][0] * (u ** 2 - 1.0 / 3) + coeff["qu"][1] * u * v
         + coeff["qu"][2] * (v ** 2 - 1.0 / 3))
    wv = coeff.get("wv", (0.0, 0.0))
    ph = coeff.get("ph", (0.7, 1.1, 2.3))
    if wv[0]:
        f = f + wv[0] * np.sin(2 * np.pi * xx / waves[0] + ph[0]) * np.cos(
            2 * np.pi * yy / (waves[0] * 1.31) + ph[1])
    if wv[1]:
        f = f + wv[1] * np.sin(2 * np.pi * (xx + 0.6 * yy) / waves[1] + ph[2])
    return f


def build_world(seed_tag: str = "c1", delta_k=None, coeffs=None,
                level=200.0, blur=PSF_SIGMA_PX, rn_e=RN_E, nframes=None,
                **legacy):
    """构造纯加性世界。

    帧模型（电子域，gain=1）：raw_k(x,y) = Poisson(s(x,y) + b_k(x,y)) + N(0,RN^2)
      s(x,y)   HST 模板（纯信号，帧间连续）
      b_k(x,y) = sky_field(...)  加性天光（平缓梯度 + 可选基外分量）
    返回 dict（含真值、帧、控制观测）。
    """
    names = FRAME_ORDER if nframes is None else FRAME_ORDER[:nframes]
    coeff = dict(SKY_TRUE) if coeffs is None else coeffs
    if delta_k is not None:
        coeff = {k: dict(v, off=delta_k.get(k, v["off"])) for k, v in coeff.items()}
    rng = derive_rng(seed_tag)
    signal = load_hst_signal(level=level, blur=blur)
    mask = star_mask(signal)
    sky = {nm: sky_field(nm, coeff[nm]) for nm in names}
    frames = {nm: synth_frame(signal, sky[nm], rng, rn_e=rn_e) for nm in names}

    obs = []
    ctrl = {}
    for nm in names:
        cov = coverage_mask(nm)
        for gx, gy, _ in cell_slices():
            if not cov[gy, gx]:
                continue
            val, sig, ntot, nret = patch_estimate(frames[nm], mask, gx, gy)
            iv = control_ivar(sig, nret)
            cid = gy * GRID + gx + 1
            obs.append(dict(frame_id=FRAME_IDS[nm], control_id=cid,
                            cell_gx=gx, cell_gy=gy, tile=0,
                            value=val, uncertainty=float(np.sqrt(1.0 / iv)),
                            control_variance=float(1.0 / iv), control_ivar=iv,
                            snr=10.0, snr_available=1, support=1.0,
                            quality_flags=1))
            ctrl[(nm, gx, gy)] = dict(value=val, sigma=sig, n_total=ntot,
                                      n_retained=nret, control_ivar=iv)
    return dict(names=names, signal=signal, mask=mask, sky=sky, frames=frames,
                obs=obs, ctrl=ctrl, coeffs=coeff, B0=100.0)


# ---------------------------------------------------------------- 生产驱动
def _probe_bin(bin_path: Path, nframes: int, nprobe: int):
    a = np.fromfile(bin_path, dtype="<i4", count=2)
    assert a[0] == nframes and a[1] == nprobe, (a, nframes, nprobe)
    raw = np.fromfile(bin_path, dtype="<f8", offset=8)
    need = nframes * nprobe * 2
    assert raw.size == need, (raw.size, need)
    # 驱动按"逐帧写 (场1, 场2)"交错布局：reshape(nF,2,NP)
    raw = raw.reshape(nframes, 2, nprobe)
    return raw[:, 0, :], raw[:, 1, :]


def run_upm_probe(scenario: dict, tag: str, probe: bool = True):
    """调用生产 p2_upm_build 等；返回 (out_json, corr[nF,ny,nx], C[nF,ny,nx])。"""
    RUN.mkdir(parents=True, exist_ok=True)
    sc = dict(scenario)
    bin_path = RUN / ("upm_%s.bin" % tag)
    if probe:
        sc["out_bin"] = str(bin_path)
    sp = RUN / ("upm_%s.json" % tag)
    op = RUN / ("upm_%s.out.json" % tag)
    sp.write_text(json.dumps(sc), encoding="utf-8")
    r = subprocess.run([str(RUN / "upm_probe"), str(sp), str(op)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("upm_probe rc=%d stderr=%s" % (r.returncode, r.stderr[-2000:]))
    out = json.loads(op.read_text(encoding="utf-8"))
    if not probe:
        return out, None, None
    p = sc["probe"]
    nF = len(sc["frames"])
    corr, cf = _probe_bin(bin_path, nF, p["nx"] * p["ny"])
    shp = (nF, p["ny"], p["nx"])
    # 驱动用 input=0 调 calibrate_block，得到 out = 0 − (C+G) = −(C+G)。
    # 统一约定：本库返回的 corr = 应当被**减掉**的校正场 = (C+G)，
    # 使 stack_mosaic 的 "raw − corr" 语义一致。
    return out, (-corr).reshape(shp), cf.reshape(shp)


def run_upm_ma_probe(scenario: dict, tag: str):
    """调用生产 p2_upm_ma_build（乘性/加性分离求解器），返回 out JSON。"""
    RUN.mkdir(parents=True, exist_ok=True)
    sc = dict(scenario)
    sc.pop("out_bin", None)
    sp = RUN / ("ma_%s.json" % tag)
    op = RUN / ("ma_%s.out.json" % tag)
    sp.write_text(json.dumps(sc), encoding="utf-8")
    r = subprocess.run([str(RUN / "upm_probe"), str(sp), str(op)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("upm_probe(ma) rc=%d stderr=%s" % (r.returncode, r.stderr[-2000:]))
    out = json.loads(op.read_text(encoding="utf-8"))
    fid = {str(v): k for k, v in FRAME_IDS.items()}
    out["g"] = {fid.get(k, k): v for k, v in out.get("ma_g", {}).items()}
    out["b"] = {fid.get(k, k): v for k, v in out.get("ma_b", {}).items()}
    return out


def run_sky_probe(scenario: dict, tag: str):
    RUN.mkdir(parents=True, exist_ok=True)
    sc = dict(scenario)
    bin_path = RUN / ("sky_%s.bin" % tag)
    sc["out_bin"] = str(bin_path)
    sp = RUN / ("sky_%s.json" % tag)
    op = RUN / ("sky_%s.out.json" % tag)
    sp.write_text(json.dumps(sc), encoding="utf-8")
    r = subprocess.run([str(RUN / "sky_probe"), str(sp), str(op)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("sky_probe rc=%d stderr=%s" % (r.returncode, r.stderr[-2000:]))
    out = json.loads(op.read_text(encoding="utf-8"))
    nF = len(sc["frames"])
    NP = len(sc["probe"]["ra_deg"])
    delta, bk = _probe_bin(bin_path, nF, NP)
    return out, delta, bk


# ---------------------------------------------------------------- 叠加与接缝度量
def stack_mosaic(frames: dict, names, corr, weights, probe_origin=(0, 0),
                 nx=TILE_PX, ny=TILE_PX):
    """按覆盖子集加权叠加：mosaic(p) = sum_{k in S(p)} w_k c_k(p) / sum w_k。

    corr[k] = (C+G) 场（生产 calibrate_block 输出取负）；w_k = 该 (帧, cell) 的
    权重（**拟合权重与堆叠权重同源**，见 README §4.4）。
    """
    num = np.zeros((ny, nx))
    den = np.zeros((ny, nx))
    for i, nm in enumerate(names):
        cov = coverage_mask(nm)
        cm = np.zeros((ny, nx), dtype=bool)
        for gx in range(GRID):
            if cov[0, gx]:
                cm[:, gx * CELL:(gx + 1) * CELL] = True
        c = frames[nm][probe_origin[1]:probe_origin[1] + ny,
                      probe_origin[0]:probe_origin[0] + nx] - corr[i]
        w = np.zeros((ny, nx))
        for gx in range(GRID):
            if cov[0, gx]:
                for gy in range(GRID):
                    w[gy * CELL:(gy + 1) * CELL, gx * CELL:(gx + 1) * CELL] = weights[(nm, gx, gy)]
        num += np.where(cm, w * c, 0.0)
        den += np.where(cm, w, 0.0)
    with np.errstate(invalid="ignore", divide="ignore"):
        mos = np.where(den > 0, num / np.maximum(den, 1e-300), np.nan)
    return mos


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
    """**非退化接缝判据**：保留背景，度量覆盖子集突变处的背景电平跳变。

    1. profile m(x) = nanmedian_y mosaic(x,y)（对星点稳健）；
    2. 在每个边界两侧 base_win 内（挖掉 ±(halfwin+2)）拟合 order 阶多项式基线；
    3. step = median(残差 右 halfwin 列) − median(残差 左 halfwin 列)；
    4. rel  = step / median(m 于窗口内)；
    5. **excess** = step(x_b) − median(step(x_b+δ), δ∈offsets)：用同一条带内
       偏离边界的对照线扣掉"真结构"造成的系统台阶（q2 §6 的 off-locus 对照）。
       强结构背景（HST 星云、真实 M42）必须看 excess，否则真结构会被误判为接缝。
    返回 list of dict(x=, step=, rel=, excess=, level=, nvalid=)。
    """
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


def degenerate_steps(frames, names, sky_truth, boundaries=None, **kw):
    """**退化判据（反证用）**：先把整张天光背景从每帧减掉，再看"帧间差/边界跳变"。

    背景皆零时该度量天然为零 —— 用于证明它没有鉴别力。
    """
    mos_num = np.zeros((TILE_PX, TILE_PX))
    mos_den = np.zeros((TILE_PX, TILE_PX))
    for nm in names:
        cov = coverage_mask(nm)
        cm = np.zeros((TILE_PX, TILE_PX), dtype=bool)
        for gx in range(GRID):
            if cov[0, gx]:
                cm[:, gx * CELL:(gx + 1) * CELL] = True
        c = frames[nm] - sky_truth[nm]
        mos_num += np.where(cm, c, 0.0)
        mos_den += cm.astype(float)
    with np.errstate(invalid="ignore"):
        mos = np.where(mos_den > 0, mos_num / np.maximum(mos_den, 1e-300), np.nan)
    return seam_steps(mos, boundaries=boundaries, **kw)


# ---------------------------------------------------------------- 小结工具
def json_dump(obj, name: str):
    RESULTS.mkdir(parents=True, exist_ok=True)
    p = RESULTS / name
    p.write_text(json.dumps(obj, indent=2, ensure_ascii=False, default=_np_default),
                 encoding="utf-8")
    return p


def _np_default(o):
    if isinstance(o, (np.floating,)):
        return float(o)
    if isinstance(o, (np.integer,)):
        return int(o)
    if isinstance(o, np.ndarray):
        return o.tolist()
    if isinstance(o, (np.bool_,)):
        return bool(o)
    raise TypeError(type(o))


def stats(v):
    v = np.asarray(v, dtype=float)
    v = v[np.isfinite(v)]
    if v.size == 0:
        return dict(n=0)
    return dict(n=int(v.size), mean=float(np.mean(v)), median=float(np.median(v)),
                std=float(np.std(v, ddof=1)) if v.size > 1 else 0.0,
                min=float(np.min(v)), max=float(np.max(v)),
                p16=float(np.percentile(v, 16)), p84=float(np.percentile(v, 84)))


class Gates:
    """判据收集器：每条判据显式记录 证据等级（见 README §5 的证据分级）。"""

    def __init__(self):
        self.rows = []

    def add(self, gid, desc, value, ok, level="data", note=""):
        self.rows.append(dict(id=gid, desc=desc, value=value, ok=bool(ok),
                              level=level, note=note))
        return ok

    def summary(self):
        return dict(n=len(self.rows), n_pass=int(sum(r["ok"] for r in self.rows)),
                    n_fail=int(sum(not r["ok"] for r in self.rows)), rows=self.rows)
