"""
stf.py - Screen Transfer Function (STF) 非破坏性拉伸

功能：对天文图像做显示拉伸，不修改原始数据
用途：浏览器显示前的实时拉伸，参考 PixInsight HT 自动 STF

支持模式：
- auto: MAD 中位数 + 1.4826×MAD 自动计算
- manual: 手动 shadows/highlights/midtones
- preset: linear/sqrt/asinh/log

核心公式 - Midtones Transfer Function (MTF):
    MTF(x, m) = ((m - 1) * x) / ((2m - 1) * x - m)
    其中 x∈[0,1] 为归一化输入, m∈(0,1) 为中点参数
    满足: MTF(0,m)=0, MTF(1,m)=1, MTF(m,m)=0.5

使用示例:
    from stf import STFEngine, STFParams

    engine = STFEngine()
    # 自动拉伸
    params = engine.auto_stretch(data)
    display = engine.apply_stf(data, params)  # uint8 (H,W) 或 (H,W,3)

    # 手动拉伸
    params = STFParams(shadows=0.1, highlights=0.9, midtones=0.25)
    display = engine.apply_stf(data, params)

    # 预设
    display = engine.apply_preset(data, "asinh")
"""

from __future__ import annotations

import os
import json
import logging
import datetime
from dataclasses import dataclass, asdict
from typing import Optional, Tuple, Dict, Any

import numpy as np


# ============================================================================
# 日志配置
# ============================================================================
_LOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
os.makedirs(_LOG_DIR, exist_ok=True)

logger = logging.getLogger("healpix_browser.stf")
if not logger.handlers:
    _log_file = os.path.join(
        _LOG_DIR, f"stf_{datetime.datetime.now().strftime('%Y%m%d')}.log")
    _fh = logging.FileHandler(_log_file, encoding="utf-8")
    _fh.setFormatter(logging.Formatter(
        "%(asctime)s [%(levelname)s] %(message)s"))
    logger.addHandler(_fh)
    _sh = logging.StreamHandler()
    _sh.setFormatter(logging.Formatter("[STF] %(message)s"))
    logger.addHandler(_sh)
    logger.setLevel(logging.INFO)


# ============================================================================
# STF 参数数据类
# ============================================================================

@dataclass
class STFParams:
    """STF 拉伸参数

    Attributes:
        shadows: 暗部裁剪点 [0,1), 低于此值的像素映射为 0
        highlights: 亮部裁剪点 (0,1], 高于此值的像素映射为 1
        midtones: 中点参数 (0,1), MTF 公式的 m 值; 0.5=线性, <0.5 提亮暗部
        compression: asinh/log 预设的压缩强度 (0=无压缩, 1=强压缩)
    """
    shadows: float = 0.0
    highlights: float = 1.0
    midtones: float = 0.5
    compression: float = 0.0

    def validate(self) -> None:
        """校验参数范围"""
        if not (0.0 <= self.shadows < 1.0):
            raise ValueError(f"shadows 超范围: {self.shadows}, 应在 [0, 1)")
        if not (0.0 < self.highlights <= 1.0):
            raise ValueError(f"highlights 超范围: {self.highlights}, 应在 (0, 1]")
        if self.shadows >= self.highlights:
            raise ValueError(
                f"shadows({self.shadows}) >= highlights({self.highlights})")
        if not (0.0 < self.midtones < 1.0):
            raise ValueError(f"midtones 超范围: {self.midtones}, 应在 (0, 1)")
        if not (0.0 <= self.compression <= 1.0):
            raise ValueError(
                f"compression 超范围: {self.compression}, 应在 [0, 1]")


# ============================================================================
# STF 引擎
# ============================================================================

class STFEngine:
    """STF 拉伸引擎, 非破坏性显示拉伸

    所有方法不修改输入数据, 返回新的显示用数组 (uint8)。
    """

    # 预设名称 → (midtones, compression) 映射
    PRESETS: Dict[str, Tuple[float, float]] = {
        "linear": (0.5, 0.0),
        "sqrt":   (0.25, 0.0),
        "asinh":  (0.25, 0.5),
        "log":    (0.15, 0.8),
    }

    def __init__(self):
        logger.info("STFEngine 初始化完成")

    # ------------------------------------------------------------------
    # 核心数学: MTF (Midtones Transfer Function)
    # ------------------------------------------------------------------

    @staticmethod
    def mtf(x: np.ndarray, m: float) -> np.ndarray:
        """Midtones Transfer Function

        MTF(x, m) = ((m-1)*x) / ((2m-1)*x - m)

        Args:
            x: 归一化输入数组 [0, 1]
            m: 中点参数 (0, 1), 0.5=线性无变换

        Returns:
            变换后的数组 [0, 1]
        """
        if abs(m - 0.5) < 1e-10:
            return x.copy()
        # 分母: (2m-1)*x - m
        # 当 x=0 时分母=-m (<0), 分子=0, 结果=0
        # 需要保护除零: 分母接近 0 时钳位
        denom = (2.0 * m - 1.0) * x - m
        # 分母不会为零 (因为 m≠0.5 且 x∈[0,1])
        # 但浮点精度可能导致极小值, 添加 epsilon
        eps = 1e-12
        denom = np.where(np.abs(denom) < eps, eps, denom)
        result = ((m - 1.0) * x) / denom
        return np.clip(result, 0.0, 1.0)

    # ------------------------------------------------------------------
    # 自动拉伸 (MAD)
    # ------------------------------------------------------------------

    def auto_stretch(self, data: np.ndarray,
                     bg_fraction: float = 0.2) -> STFParams:
        """基于 MAD 的自动拉伸参数计算

        参考 PixInsight HT 自动 STF 算法:
        1. 计算中位数 (bg) 和 MAD
        2. sigma = 1.4826 * MAD
        3. shadows = bg - 2.8 * sigma
        4. highlights = bg + sigma * (bg / sigma + ...) 简化为 bg + k*sigma
        5. midtones = 归一化后的 bg 值 (MTF(bg_norm, m)=0.5 → m=bg_norm)

        Args:
            data: 输入数据 (任意形状, float)
            bg_fraction: 背景估计分位数 (默认 0.2 = 20th percentile)

        Returns:
            STFParams 自动计算的拉伸参数
        """
        logger.info("计算自动拉伸参数 (MAD)...")
        # 取有限值 (排除 NaN/Inf)
        valid = data[np.isfinite(data)]
        if valid.size == 0:
            logger.warning("数据全为 NaN/Inf, 返回默认参数")
            return STFParams()

        # 如果有多通道, 展平处理
        flat = valid.ravel()

        # 1. 中位数 (背景估计)
        median_val = float(np.median(flat))
        logger.info(f"  中位数 = {median_val:.6e}")

        # 2. MAD (Median Absolute Deviation)
        mad = float(np.median(np.abs(flat - median_val)))
        sigma = 1.4826 * mad
        logger.info(f"  MAD = {mad:.6e}, sigma(1.4826*MAD) = {sigma:.6e}")

        if sigma < 1e-30:
            logger.warning("  sigma≈0 (数据方差极小), 返回默认参数")
            return STFParams()

        # 3. shadows / highlights (在原始数据空间)
        shadows_raw = median_val - 2.8 * sigma
        highlights_raw = median_val + sigma * (
            median_val / sigma + 35.0) if sigma > 0 else median_val + 35.0 * sigma
        # 简化: highlights = 中位数 + 若干倍 sigma, 取数据范围上限
        data_max = float(np.max(flat))
        data_min = float(np.min(flat))
        highlights_raw = min(highlights_raw, data_max)
        shadows_raw = max(shadows_raw, data_min)

        logger.info(f"  原始空间: shadows={shadows_raw:.6e}, "
                    f"highlights={highlights_raw:.6e}")

        # 4. 归一化到 [0, 1]
        data_range = highlights_raw - shadows_raw
        if data_range < 1e-30:
            return STFParams()

        shadows_norm = 0.0  # shadows 映射到 0
        highlights_norm = 1.0  # highlights 映射到 1
        # 中位数在归一化空间的位置
        midtones_norm = (median_val - shadows_raw) / data_range
        midtones_norm = float(np.clip(midtones_norm, 0.001, 0.999))

        params = STFParams(
            shadows=shadows_norm,
            highlights=highlights_norm,
            midtones=midtones_norm,
            compression=0.0,
        )
        logger.info(f"  STF 参数: shadows={params.shadows:.4f}, "
                    f"highlights={params.highlights:.4f}, "
                    f"midtones={params.midtones:.4f}")
        return params

    # ------------------------------------------------------------------
    # 应用 STF
    # ------------------------------------------------------------------

    def apply_stf(self, data: np.ndarray, params: STFParams) -> np.ndarray:
        """应用 STF 拉伸, 返回 uint8 显示图像

        流程:
        1. 数据归一化到 [0, 1] (用 shadows/highlights 裁剪)
        2. 应用 MTF (midtones)
        3. 如果 compression > 0, 叠加 asinh 压缩
        4. 缩放到 [0, 255] uint8

        Args:
            data: 输入数据 (H, W) 或 (H, W, C), float
            params: STF 拉伸参数

        Returns:
            uint8 显示图像, 与输入同形状 (灰度→H,W; 彩色→H,W,C)
        """
        params.validate()
        logger.info(f"应用 STF: shadows={params.shadows:.4f}, "
                    f"highlights={params.highlights:.4f}, "
                    f"midtones={params.midtones:.4f}, "
                    f"compression={params.compression:.4f}")

        if data.size == 0:
            return np.zeros(data.shape, dtype=np.uint8)

        # float64 计算, 避免精度问题
        work = data.astype(np.float64)
        has_channels = (work.ndim == 3)
        if not has_channels:
            work = work[:, :, np.newaxis]

        # 1. shadows/highlights 裁剪 → 归一化到 [0, 1]
        s = params.shadows
        h = params.highlights
        rng = h - s
        if rng < 1e-30:
            rng = 1.0
        norm = (work - s) / rng
        norm = np.clip(norm, 0.0, 1.0)

        # 2. 应用 MTF
        stretched = self.mtf(norm, params.midtones)

        # 3. compression > 0 时叠加 asinh
        if params.compression > 1e-6:
            stretched = self._apply_asinh(stretched, params.compression)

        # 4. 缩放到 uint8
        display = (stretched * 255.0 + 0.5).astype(np.uint8)

        if not has_channels:
            display = display[:, :, 0]

        logger.info(f"  STF 完成, 输出形状={display.shape}, dtype={display.dtype}")
        return display

    # ------------------------------------------------------------------
    # 直方图计算
    # ------------------------------------------------------------------

    @staticmethod
    def compute_histogram(data: np.ndarray, bins: int = 256
                          ) -> Tuple[np.ndarray, np.ndarray]:
        """计算数据直方图

        Args:
            data: 输入数据 (任意形状)
            bins: 直方图桶数

        Returns:
            (hist, bin_edges): hist 为计数数组, bin_edges 为桶边界
        """
        valid = data[np.isfinite(data)].ravel()
        if valid.size == 0:
            return np.zeros(bins, dtype=np.int64), np.zeros(bins + 1)

        hist, edges = np.histogram(valid, bins=bins)
        logger.info(f"直方图: bins={bins}, 范围=[{edges[0]:.4e}, "
                    f"{edges[-1]:.4e}], 总计={hist.sum()}")
        return hist.astype(np.int64), edges

    # ------------------------------------------------------------------
    # 预设拉伸
    # ------------------------------------------------------------------

    def apply_preset(self, data: np.ndarray, preset_name: str
                     ) -> np.ndarray:
        """应用预设拉伸

        Args:
            data: 输入数据
            preset_name: 预设名 ("linear"/"sqrt"/"asinh"/"log")

        Returns:
            uint8 显示图像
        """
        if preset_name not in self.PRESETS:
            raise ValueError(
                f"未知预设: {preset_name}, 可用: {list(self.PRESETS.keys())}")

        midtones, compression = self.PRESETS[preset_name]
        logger.info(f"应用预设: {preset_name} (midtones={midtones}, "
                    f"compression={compression})")

        # 先用 auto 计算数据范围 (shadows/highlights), 再套用预设的 midtones
        auto_params = self.auto_stretch(data)
        params = STFParams(
            shadows=auto_params.shadows,
            highlights=auto_params.highlights,
            midtones=midtones,
            compression=compression,
        )

        if preset_name == "linear":
            # 线性: 不做 MTF, 只裁剪归一化
            params.midtones = 0.5
            params.compression = 0.0
        elif preset_name == "sqrt":
            # 平方根: midtones=0.25 的 MTF 近似
            params.midtones = 0.25
            params.compression = 0.0
        elif preset_name == "asinh":
            # asinh: MTF + compression
            params.midtones = 0.25
            params.compression = 0.5
        elif preset_name == "log":
            # log: 强 MTF + 强 compression
            params.midtones = 0.15
            params.compression = 0.8

        return self.apply_stf(data, params)

    # ------------------------------------------------------------------
    # asinh 压缩 (内部使用)
    # ------------------------------------------------------------------

    @staticmethod
    def _apply_asinh(x: np.ndarray, compression: float) -> np.ndarray:
        """asinh 压缩函数

        f(x) = asinh(x / scale) / asinh(1 / scale)
        其中 scale 由 compression 控制: scale = (1 - compression) / compression

        Args:
            x: 归一化输入 [0, 1]
            compression: 压缩强度 [0, 1]

        Returns:
            压缩后的 [0, 1] 数组
        """
        if compression < 1e-6:
            return x
        scale = max((1.0 - compression) / compression, 1e-6)
        result = np.arcsinh(x / scale) / np.arcsinh(1.0 / scale)
        return np.clip(result, 0.0, 1.0)

    # ------------------------------------------------------------------
    # log 压缩 (内部使用)
    # ------------------------------------------------------------------

    @staticmethod
    def _apply_log(x: np.ndarray, compression: float) -> np.ndarray:
        """对数压缩函数

        f(x) = log(1 + x * k) / log(1 + k)
        其中 k 由 compression 控制

        Args:
            x: 归一化输入 [0, 1]
            compression: 压缩强度 [0, 1]

        Returns:
            压缩后的 [0, 1] 数组
        """
        if compression < 1e-6:
            return x
        k = compression * 1000.0  # 压缩系数
        result = np.log1p(x * k) / np.log1p(k)
        return np.clip(result, 0.0, 1.0)

    # ------------------------------------------------------------------
    # 参数序列化 (JSON)
    # ------------------------------------------------------------------

    @staticmethod
    def save_params(params: STFParams, path: str) -> None:
        """保存 STF 参数到 JSON 文件

        Args:
            params: STF 参数
            path: JSON 文件路径
        """
        data = asdict(params)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        logger.info(f"STF 参数已保存: {path}")

    @staticmethod
    def load_params(path: str) -> STFParams:
        """从 JSON 文件加载 STF 参数

        Args:
            path: JSON 文件路径

        Returns:
            STFParams 对象
        """
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        params = STFParams(**data)
        params.validate()
        logger.info(f"STF 参数已加载: {path}")
        return params
