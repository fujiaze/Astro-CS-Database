# -*- coding: utf-8 -*-
"""
e_common.py - Gate E 公共约定模块 (E-001 ~ E-004 共享)

公共约定 (SSOT, 各子任务必须遵守):
  - 模块边界: 本文件定义数据结构 + HISS 读取 + 坐标转换 + 日志, 不含算法
  - 数据结构: PanelData (单帧像素表) / ControlPointTable (采样后控制点)
  - 坐标系: RA/Dec 度 (J2000), astropy_healpix HEALPix->lonlat
  - 有效覆盖定义: support==1 (V1 HISS 所有存储像素 support=1, 见 hiss_v2.v1_read_snr_model)
  - 曲面基底: 2D 多项式 [1, ra, dec, ra*dec, ra^2, dec^2] (D=2, M=6 系数)
              线性梯度 (a*ra + b*dec) 被 ra/dec 一次项精确捕获, 用于 E-004 恢复验证
  - 帧偏移: 每帧一个加性偏移 off_f (只加性, 禁止乘性项)
  - 零均值规范: sum_f off_f = 0 (等权, 契约要求"偏移量之和=0")
  - 权重: 联合权重 = SNR^2 * inv_var (E-002), 不得使用无权重梯度
  - 求解器: scipy.sparse 加权最小二乘 + 等式约束 (E-003)

禁止捷径 (契约级):
  - 不得使用无权重梯度
  - 不得选单一参考帧 (全局共识曲面, 非差异拟合)
  - 不得加入乘性项 (只加性 offset)
  - 只采有效覆盖 (support=1)
  - 星点/饱和必须掩膜
  - SNR^2 权重必须实现
  - 全局零均值规范
  - 已知梯度恢复必须验证 (E-004)
"""
from __future__ import annotations

import logging
import os
import sys
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

# 项目路径 (SSOT)
PROJECT_ROOT = r'f:\Astro dev\Astro CS Normalization Database'
HISS_V2_PATH = os.path.join(PROJECT_ROOT, 'lib', 'astro_image_io', 'python')
D001_DIR = os.path.join(PROJECT_ROOT, 'output', 'D-001')

# 三片面板名 (固定顺序, 影响偏移量索引)
PANEL_NAMES = ['panel1', 'panel2', 'panel3']

# 曲面基底: 2D 多项式 D=2, M=6 系数 [1, ra, dec, ra*dec, ra^2, dec^2]
# 注意: 为数值稳定, 求解时对 ra/dec 做中心化 (减去参考点), 但接口返回原始 ra/dec
POLY_DEGREE = 2
N_SURFACE_COEFFS = 6  # (D+1)(D+2)/2 = 6 for D=2


# ============================================================================
# 日志配置
# ============================================================================

def setup_logger(name: str, log_dir: Optional[str] = None,
                 level: int = logging.INFO) -> logging.Logger:
    """配置模块日志, 可选写入文件。"""
    logger = logging.getLogger(name)
    logger.setLevel(level)
    logger.handlers.clear()
    fmt = logging.Formatter('[%(asctime)s][%(levelname)s][%(name)s] %(message)s',
                            datefmt='%H:%M:%S')
    sh = logging.StreamHandler(sys.stdout)
    sh.setFormatter(fmt)
    logger.addHandler(sh)
    if log_dir:
        os.makedirs(log_dir, exist_ok=True)
        fh = logging.FileHandler(os.path.join(log_dir, f'{name}.log'),
                                 encoding='utf-8')
        fh.setFormatter(fmt)
        logger.addHandler(fh)
    return logger


# ============================================================================
# 数据结构
# ============================================================================

@dataclass
class PanelData:
    """单帧 HISS 像素表 (全像素, 未采样未掩膜)。"""
    panel_id: str
    nside: int
    nested: bool
    ipix: np.ndarray         # u64 [N] HEALPix 像素索引
    signal: np.ndarray       # f32 [N] 信号值
    support: np.ndarray      # u1 [N] 覆盖标记 (V1: 全 1)
    ra: np.ndarray           # f64 [N] 赤经 (度)
    dec: np.ndarray          # f64 [N] 赤纬 (度)
    meta: Dict[str, Any] = field(default_factory=dict)
    snr_model: Any = None    # V1 HissV2SnrModel 或 None

    @property
    def n_pix(self) -> int:
        return int(self.ipix.size)


@dataclass
class ControlPointTable:
    """稀疏控制点表 (采样后, 含掩膜标记与权重)。SoA 布局。"""
    panel_id: str
    ipix: np.ndarray         # u64 [M] 采样控制点像素索引
    ra: np.ndarray           # f64 [M]
    dec: np.ndarray          # f64 [M]
    signal: np.ndarray       # f32 [M]
    snr: np.ndarray          # f32 [M] IDW 插值的 SNR
    weight: np.ndarray       # f64 [M] 联合权重 SNR^2 * inv_var (E-002 填充, E-001 默认 1)
    valid: np.ndarray        # bool [M] 有效 (非掩膜 且 support=1)
    mask_star: np.ndarray    # bool [M] True=星点 (被掩)
    mask_sat: np.ndarray     # bool [M] True=饱和 (被掩)
    mask_anom: np.ndarray    # bool [M] True=异常 (被掩)

    @property
    def n_points(self) -> int:
        return int(self.ipix.size)

    @property
    def n_valid(self) -> int:
        return int(self.valid.sum())


# ============================================================================
# HISS 读取 + 坐标转换
# ============================================================================

def load_panel(panel_id: str, hiss_dir: str = D001_DIR) -> PanelData:
    """读取单片 V1 HISS, 转换 ipix->RA/Dec, support 全 1 (V1 约定)。

    Args:
        panel_id: 'panel1'/'panel2'/'panel3'
        hiss_dir: HISS 文件目录

    Returns:
        PanelData
    """
    # 延迟导入 hiss_v2 (路径注入)
    if HISS_V2_PATH not in sys.path:
        sys.path.insert(0, HISS_V2_PATH)
    from hiss_v2 import v1_read_snr_model  # type: ignore
    from astropy_healpix import HEALPix
    from astropy import units as u

    fname = f'T4_RED_GalaxyCenter_{panel_id}.hiss'
    path = os.path.join(hiss_dir, fname)
    if not os.path.isfile(path):
        raise FileNotFoundError(f'HISS 文件不存在: {path}')

    nside, nested, ipix, pixel, meta, snr_model = v1_read_snr_model(path)
    n_pix = int(ipix.size)

    # V1 所有存储像素均为覆盖像素 (hiss_v2.py v1_to_v2_converter 注释明示)
    support = np.ones(n_pix, dtype=np.uint8)
    signal = pixel.astype(np.float32)

    # ipix -> RA/Dec
    hp = HEALPix(nside=nside, order='nested' if nested else 'ring')
    lon, lat = hp.healpix_to_lonlat(ipix.astype(np.int64))
    ra = np.asarray(lon.to(u.deg).value, dtype=np.float64)
    dec = np.asarray(lat.to(u.deg).value, dtype=np.float64)

    return PanelData(
        panel_id=panel_id, nside=int(nside), nested=bool(nested),
        ipix=ipix.astype(np.uint64), signal=signal, support=support,
        ra=ra, dec=dec, meta=dict(meta), snr_model=snr_model,
    )


def load_all_panels(hiss_dir: str = D001_DIR) -> Dict[str, PanelData]:
    """读取三片 HISS。"""
    return {p: load_panel(p, hiss_dir) for p in PANEL_NAMES}


# ============================================================================
# 曲面基底 (2D 多项式, 求解时中心化)
# ============================================================================

def poly_basis(ra: np.ndarray, dec: np.ndarray,
               center: Optional[Tuple[float, float]] = None
               ) -> Tuple[np.ndarray, Tuple[float, float]]:
    """2D 多项式基底 [1, ra', dec', ra'*dec', ra'^2, dec'^2] (D=2, M=6)。

    为数值稳定, 对 ra/dec 做中心化 (减去参考点)。返回基底矩阵 + 使用的中心。

    Args:
        ra, dec: f64 [N] 度
        center: (ra0, dec0) 中心化参考点; None 则用均值

    Returns:
        basis: f64 [N, 6]
        center: (ra0, dec0) 实际使用的中心
    """
    if center is None:
        ra0, dec0 = float(np.mean(ra)), float(np.mean(dec))
    else:
        ra0, dec0 = float(center[0]), float(center[1])
    x = ra - ra0
    y = dec - dec0
    # [1, x, y, x*y, x^2, y^2]
    basis = np.stack([np.ones_like(x), x, y, x * y, x * x, y * y], axis=-1)
    return basis, (ra0, dec0)


def eval_surface(coeffs: np.ndarray, ra: np.ndarray, dec: np.ndarray,
                 center: Tuple[float, float]) -> np.ndarray:
    """用多项式系数评估曲面值。"""
    basis, _ = poly_basis(ra, dec, center=center)
    return basis @ np.asarray(coeffs, dtype=np.float64)


# ============================================================================
# 统计辅助 (MAD-based)
# ============================================================================

def mad_sigma(x: np.ndarray, axis: int = -1) -> np.ndarray:
    """MAD-based sigma 估计: 1.4826 * median(|x - median(x)|)。

    沿指定轴计算, 保持 NaN 安全。
    """
    x = np.asarray(x, dtype=np.float64)
    med = np.nanmedian(x, axis=axis, keepdims=True)
    mad = np.nanmedian(np.abs(x - med), axis=axis, keepdims=True)
    sigma = 1.4826 * mad
    return np.squeeze(sigma, axis=axis) if sigma.shape[axis] == 1 else sigma


def safe_mad_sigma(x: np.ndarray) -> float:
    """标量 MAD-sigma, 忽略 NaN/Inf。"""
    x = np.asarray(x, dtype=np.float64)
    x = x[np.isfinite(x)]
    if x.size == 0:
        return 0.0
    med = float(np.median(x))
    mad = float(np.median(np.abs(x - med)))
    return 1.4826 * mad


# ============================================================================
# IDW 插值 (用于 SNR 控制点 -> 任意位置)
# ============================================================================

def idw_interpolate(query_ra: np.ndarray, query_dec: np.ndarray,
                    src_ra: np.ndarray, src_dec: np.ndarray,
                    src_val: np.ndarray, power: float = 2.0,
                    k: int = 8) -> np.ndarray:
    """反距离加权插值 (IDW), 球面大圆弧距离。

    Args:
        query_ra, query_dec: f64 [Q] 查询点
        src_ra, src_dec: f64 [S] 源点
        src_val: f64 [S] 源值
        power: IDW 幂次 (默认 2.0, 与 V1 snr_model.idw_power 一致)
        k: 最近邻数

    Returns:
        f64 [Q] 插值结果 (无源点时返回 NaN)
    """
    query_ra = np.asarray(query_ra, dtype=np.float64)
    query_dec = np.asarray(query_dec, dtype=np.float64)
    src_ra = np.asarray(src_ra, dtype=np.float64)
    src_dec = np.asarray(src_dec, dtype=np.float64)
    src_val = np.asarray(src_val, dtype=np.float64)

    Q = query_ra.size
    S = src_ra.size
    out = np.full(Q, np.nan, dtype=np.float64)
    if S == 0:
        return out

    # 球面->3D 单位向量
    def to_xyz(ra, dec):
        r = np.deg2rad(ra)
        d = np.deg2rad(dec)
        return np.stack([np.cos(d) * np.cos(r), np.cos(d) * np.sin(r), np.sin(d)], axis=-1)
    src_xyz = to_xyz(src_ra, src_dec)  # [S,3]
    q_xyz = to_xyz(query_ra, query_dec)  # [Q,3]

    kk = min(k, S)
    for i in range(Q):
        # 大圆弧角 = arccos(dot), 数值稳定用 clip
        dots = np.clip(src_xyz @ q_xyz[i], -1.0, 1.0)
        ang = np.arccos(dots)  # [S] rad
        # 最近 k 个
        idx = np.argpartition(ang, kk - 1)[:kk]
        d_k = ang[idx]
        v_k = src_val[idx]
        # 零距离精确匹配
        if np.any(d_k < 1e-12):
            out[i] = v_k[d_k < 1e-12][0]
            continue
        w = 1.0 / np.power(d_k, power)
        out[i] = float(np.sum(w * v_k) / np.sum(w))
    return out


# ============================================================================
# JSON 序列化辅助
# ============================================================================

def json_default(o):
    """JSON 默认序列化器 (numpy -> python)。"""
    if isinstance(o, (np.bool_, bool)):
        return bool(o)
    if isinstance(o, np.integer):
        return int(o)
    if isinstance(o, np.floating):
        return float(o)
    if isinstance(o, np.ndarray):
        return o.tolist()
    return str(o)
