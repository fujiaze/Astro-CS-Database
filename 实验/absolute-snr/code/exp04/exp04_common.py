#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 公共库：三类实验数据面、控制点估计量、判据、门。

三类实验数据（AGENTS.md §5 / 最高设计 §12.2）：
  ① 纯解析代数合成：σ(x,y)=10^(s·g) 已知真值（含"真值无空间效应⇒归零"负例 s=0，
     以及含 cell 内未分辨结构的分支）；
  ② HST M16 真实信号模板 + 完整物理前向仿真（Poisson + 读出噪声，逐像素解析真值 σ）；
  ③ testdata M42 Red 300 s 真实帧（1 px 棋盘 hold-out 真值，控制值与评价目标零重叠）。

判据（每个算子都要给）：
  A1 精度 rmse_log_rho  ：两侧中位数归一后的 RMSE(log10 σ̂/σ_true) [dex]
  A2 水平偏差 level_bias：median(log10 σ̂/σ_true) [dex]
  A3 权重效率损失 E     ：Var_w/Var_opt − 1（全局尺度相消；E=0 ⇔ σ̂ ∝ σ_true）
                          —— §8b 确立的非退化判据
  A4 空间增益 G         ：G = E_frame − E_op（帧级臂相对该算子的效率损失差）
  A5 代价               ：重建 wall_s、控制点数 N、字节数
  A6 边界行为           ：无效区传播半径、超调（overshoot）、NaN 泄漏
  A7 跨帧一致性         ：同一天区不同实现的 σ̂ 相对离散

禁止用作证据的判据（§8b 警告，本模块显式实现为 tautology 复核）：
  "帧级臂 RMSE ≤ K·s_field" 对任意真值场恒真（K 足够大即可），只作退化演示。
"""
from __future__ import annotations

import os
import sys
import time

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(_HERE))          # code/
sys.path.insert(0, _HERE)
import sci_b_common as C  # noqa: E402
import operators as O     # noqa: E402

# ---------------------------------------------------------------------------
# 冻结配置（固定 seed；所有子实验只用 SEED_BASE + 固定偏移）
# ---------------------------------------------------------------------------
SEED_BASE = C.SEED_BASE                 # 20260921
CROP = 1024                             # 本单元所有面统一 1024x1024 中心裁剪
P_DENSE = 32                            # 稠密口径的 patch 尺寸（与 B3 同口径）
DELTA_GRID = [16, 32, 64, 128, 256]
DELTA_PROD = 64                         # 现行生产值（07_noise_snr.md §5）
BUDGET_BYTES = 1024 * 1024              # HiPS 每帧 SNR 层预算 1 MiB（EXP-205 冻结门）
FRAME_PX = 4096                         # 生产帧边长
BYTES_CTRL_SPARSE = 8                   # 稀疏路径全程 FP64（负责人裁决 SCI-PREC-01）
BYTES_PX_DENSE = 4                      # 稠密路径默认 FP32
ELL_SYNTH = [8.0, 16.0, 32.0, 64.0, 128.0]
S_FIELD_SYNTH = [0.0, 0.03, 0.10, 0.30]
ELL_UNRES = [4.0, 8.0]
A_UNRES = [0.10, 0.30]
ELL_PRIMARY = [16.0, 64.0]
S_PRIMARY = [0.03, 0.30]

# 物理前向仿真参数（与 B3 同口径，声明坐标，不做物理闭合反推）
GAIN, RN, DARK, SIG_MED_E, SKY_E = 1.3, 10.0, 0.5, 200.0, 200.0
M16 = os.path.join(C.HST_M16, "hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits")
M42 = [os.path.join(C.TESTDATA, p) for p in [
    "M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts",
    "M42_T2T3_mosaic_Flying_dutchman/T2/M2/M42_M2_T2_flying_dutchman-20251212@020002-300S-Red.fts",
    "M42_T2T3_mosaic_Flying_dutchman/T2/M4/M42_M4_T2_flying_dutchman-20251224@045919-300S-Red.fts"]]

# 控制点估计量的自身噪声下限 [dex]（1.4826×MAD 的相对标准误 1.166/√N，Rousseeuw-Croux）
def eps_ref_dex(D):
    return 1.166 / np.log(10.0) / np.sqrt(D * D / 2.0)


def rng(offset=0):
    return np.random.default_rng(SEED_BASE + int(offset))


# ---------------------------------------------------------------------------
# ① 纯解析合成面
# ---------------------------------------------------------------------------
def synth_sigma_face(ell, s_field, n=CROP, seed_off=0, ell_unres=None, a_unres=0.0):
    """σ(x,y) = 10^(s·g_ℓ) · (1 + a·u_{ℓ_u})；g/u 为单位方差 GRF，真值解析已知。

    s_field=0 且 a_unres=0 ⇒ 平坦场（真值无空间效应）——退化用例的输入。
    ell_unres 远小于 Δ 时，cell 内出现**未分辨结构**（B3 的三因子之一）。
    """
    r = rng(seed_off)
    ny = nx = n
    fy = np.fft.fftfreq(ny)[:, None]; fx = np.fft.rfftfreq(nx)[None, :]
    k2 = fy ** 2 + fx ** 2

    def _grf(ell_px):
        pk = np.exp(-2.0 * (np.pi ** 2) * (ell_px ** 2) * k2)
        w = r.normal(size=(ny, nx // 2 + 1)) + 1j * r.normal(size=(ny, nx // 2 + 1))
        g = np.fft.irfft2(w * np.sqrt(pk), s=(ny, nx))
        return g / g.std()

    g = _grf(ell)
    sigma = 10.0 ** (s_field * g)
    if ell_unres is not None and a_unres > 0:
        # 乘性对数正态调制：exp(a·u) 严格为正（1+a·u 在 u<-1/a 时为负 ⇒ σ<0，非物理）
        sigma = sigma * np.exp(a_unres * _grf(ell_unres))
    return sigma


def synth_data_face(sigma, seed_off=1):
    """纯解析面数据：白噪声 N(0, σ(x,y))（真值场即 σ 本身）。"""
    r = rng(seed_off)
    return r.normal(0.0, 1.0, size=sigma.shape) * sigma, sigma


# ---------------------------------------------------------------------------
# ② HST M16 真实信号模板 + 完整物理前向仿真
# ---------------------------------------------------------------------------
def load_m16(seed_off=2, n=CROP):
    from astropy.io import fits
    with fits.open(M16, memmap=False) as h:
        raw = np.asarray(h[0].data, dtype=np.float64)
        hdr = h[0].header
    ny, nx = raw.shape
    y0, x0 = (ny - n) // 2, (nx - n) // 2
    sig = np.maximum(raw[y0:y0 + n, x0:x0 + n], 0.0)
    scale = SIG_MED_E / max(np.median(sig), 1e-12)
    sig_e = sig * scale
    r = rng(seed_off)
    lam = sig_e + SKY_E + DARK
    d = r.poisson(lam).astype(np.float64) + r.normal(0.0, RN, size=lam.shape)
    img = d / GAIN
    truth = np.sqrt((sig_e + SKY_E + DARK + RN ** 2)) / GAIN   # 解析逐像素 σ（含源泊松）
    return img, truth, dict(file=os.path.basename(M16), bunit=str(hdr.get("BUNIT", "")).strip(),
                            scale_to_median_e=float(scale), median_signal_e=float(np.median(sig_e)),
                            max_over_median=float(sig_e.max() / max(np.median(sig_e), 1e-12)),
                            crop=int(n), gain=GAIN, rn_e=RN, dark_e=DARK, sky_e=SKY_E)


# ---------------------------------------------------------------------------
# ③ testdata M42 真实帧（1 px 棋盘 hold-out）
# ---------------------------------------------------------------------------
def load_m42(path, n=CROP):
    from astropy.io import fits
    with fits.open(path, memmap=False) as h:
        raw = np.asarray(h[0].data, dtype=np.float64)
        hdr = h[0].header
    ny, nx = raw.shape
    y0, x0 = (ny - n) // 2, (nx - n) // 2
    img = raw[y0:y0 + n, x0:x0 + n]
    return img, dict(file=os.path.basename(path), exptime=float(hdr.get("EXPTIME", 0.0)),
                     filter=str(hdr.get("FILTER", "")), crop=int(n))


def holdout_split(img):
    """1 px 棋盘 hold-out：族 0 用于估计，族 1 用于真值（零像素重叠）。"""
    yy, xx = np.mgrid[0:img.shape[0], 0:img.shape[1]]
    fam = (yy + xx) % 2
    return (np.where(fam == 0, img, np.nan), np.where(fam == 1, img, np.nan), fam)


# ---------------------------------------------------------------------------
# 控制点估计量
# ---------------------------------------------------------------------------
def ctrl_estimated(img, D):
    """生产口径镜像：逐 Δ×Δ cell 的 1.4826×MAD（5σ×2 轮裁剪），NaN 允许。"""
    return C.sigma_field_fast(img, D)


def ctrl_oracle(truth, D):
    """Oracle 控制值：cell 内真值 σ 的 RMS（= 该 cell 的最优常数代表）。"""
    h, w = truth.shape
    ny, nx = h // D, w // D
    t = truth[:ny * D, :nx * D].reshape(ny, D, nx, D).transpose(0, 2, 1, 3).reshape(ny, nx, D * D)
    with np.errstate(all="ignore"):
        return np.sqrt(np.nanmean(t ** 2, axis=2))


def block_constant(field, D, shape):
    """把 Δ 网格场按块常数展开到像素级（dense-at-Δ / frame 臂用）。"""
    ny, nx = field.shape
    iy = np.minimum(np.arange(shape[0]) // D, ny - 1)
    ix = np.minimum(np.arange(shape[1]) // D, nx - 1)
    return field[np.ix_(iy, ix)]


def patch_agg(field, P):
    """P×P patch 聚合（中位数），与 B3 的评价粒度一致。"""
    h, w = field.shape
    ny, nx = h // P, w // P
    t = field[:ny * P, :nx * P].reshape(ny, P, nx, P).transpose(0, 2, 1, 3).reshape(ny, nx, P * P)
    with np.errstate(all="ignore"):
        return np.nanmedian(t, axis=2)


def patch_rms(field, P):
    h, w = field.shape
    ny, nx = h // P, w // P
    t = field[:ny * P, :nx * P].reshape(ny, P, nx, P).transpose(0, 2, 1, 3).reshape(ny, nx, P * P)
    with np.errstate(all="ignore"):
        return np.sqrt(np.nanmean(t ** 2, axis=2))


# ---------------------------------------------------------------------------
# 判据
# ---------------------------------------------------------------------------
def metrics(est, truth, mask):
    """A1/A2/A3：精度、水平偏差、权重效率损失。全部在**同一评价掩膜**上算。"""
    m = (mask & np.isfinite(est) & np.isfinite(truth) & (est > 0) & (truth > 0))
    n = int(m.sum())
    if n < 64:
        return dict(n_eval=n, rmse_log_rho=float("nan"), level_bias_dex=float("nan"),
                    eff_loss=float("nan"), skipped=True)
    a = est[m] / np.median(est[m]); b = truth[m] / np.median(truth[m])
    d = np.log10(a) - np.log10(b)
    return dict(n_eval=n,
                rmse_log_rho=float(np.sqrt(np.mean(d * d))),
                level_bias_dex=float(np.median(np.log10(est[m] / truth[m]))),
                eff_loss=C.weight_efficiency_loss(est[m], truth[m], np.ones(n, bool)),
                skipped=False)


def eval_field(est, truth, mask):
    """在像素级评价（est 与 truth 同形状）。"""
    return metrics(np.asarray(est, np.float64), np.asarray(truth, np.float64), mask)


def storage_bytes(kind, D=None, n_px=FRAME_PX ** 2):
    """A5：每帧 SNR 层字节数（稀疏 FP64 控制值；稠密 FP32 逐像素）。"""
    if kind == "dense":
        return int(BYTES_PX_DENSE * n_px)
    if kind == "frame":
        return int(BYTES_CTRL_SPARSE)
    if kind == "sparse":
        n_ctrl = int(np.ceil(np.sqrt(n_px) / D) ** 2)
        return int(BYTES_CTRL_SPARSE * n_ctrl)
    raise ValueError(kind)


def n_ctrl(n_px_side, D):
    return int(np.ceil(n_px_side / D) ** 2)


def now():
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def jdump(obj, path):
    return C.save_json(path, obj)
