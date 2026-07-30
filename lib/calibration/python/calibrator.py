# -*- coding: utf-8 -*-
"""
Calibrator - 图像校准模块
功能: 对 Light 帧进行标准 CCD 校准（Bias/Dark/Flat），支持暗场优化（残差最小化，默认关闭）
用途: 天文图像标准校准流程的核心步骤，对单帧或多帧 Light 进行校准，
      支持文件模式(calibrate_frame)与生产模式内存直通(calibrate_data)。
依赖: numpy, astro_image_io (ImageReader / FITSWriter / FITSKeywordPy)
调用: from calibrator import Calibrator
      cal = Calibrator(max_workers=16)
      cal.calibrate_frame("light.fits", "calibrated.fits", calibration_dir="calibration_files/")
      # 或内存直通:
      calibrated, stats = cal.calibrate_data(light_data, master_bias=bias_data,
                                             master_dark=dark_data, master_flat=flat_data)
校准公式: Calibrated = (Light - Bias - K*(Dark - Bias)) / NormalizedFlat
数据流: Light帧 -> [主帧自动匹配] -> [数据范围统一] -> [Flat归一化]
      -> [暗场优化(可选)] -> 标准校准公式 -> 写FITS / 返回内存
关键设计:
  - 暗场优化默认关闭，需显式启用(dark_optimization=True)
  - 暗场优化使用黄金分割搜索最优K值，目标为背景区域MAD最小
  - Flat归一化到 median=1.0，最小值裁剪 0.1
  - 文件模式从FITS头读取BITPIX/EXPTIME/FILTER，自动统一数据范围与匹配主帧
  - 内存模式假设数据范围已统一（上游负责）
"""

from __future__ import annotations

import os
import re
import sys
import logging
import ctypes
from datetime import datetime
from typing import Optional

import numpy as np

# ---- 导入项目统一的 astro_image_io 接口 ----
_lib_base = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "astro_image_io", "python",
)
if _lib_base not in sys.path:
    sys.path.insert(0, _lib_base)
from astro_image_io import ImageReader, FITSWriter, FITSKeywordPy

# ---- 导入 C++ 校准 DLL 封装（同目录 astro_calibration.py） ----
_lib_calib_dir = os.path.dirname(os.path.abspath(__file__))
if _lib_calib_dir not in sys.path:
    sys.path.insert(0, _lib_calib_dir)
from astro_calibration import _dll as _ac_dll, _to_float_ptr, AC_OK as _AC_OK


# ============================ 日志配置 ============================

_LOG_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "logs")
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR, "calibrator_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("calibrator")
    lg.setLevel(logging.DEBUG)
    lg.propagate = False
    if not lg.handlers:
        fh = logging.FileHandler(log_file, encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(formatter)
        lg.addHandler(fh)

        ch = logging.StreamHandler(sys.stdout)
        ch.setLevel(logging.INFO)
        ch.setFormatter(formatter)
        lg.addHandler(ch)

    lg.info("日志系统初始化完成，日志文件: %s", log_file)
    return lg


logger = _init_logger()


# ============================ 主帧自动匹配 ============================

# 文件名解析正则
_RE_DARK_EXPOSURE = re.compile(r"EXPOSURE-([\d.]+)s", re.IGNORECASE)
_RE_FLAT_FILTER = re.compile(r"FILTER-([^_]+)_mono", re.IGNORECASE)


def find_matching_master_dark(dark_dir, exposure, tolerance=10.0):
    """
    在 dark_dir 中查找曝光时间匹配的 Master Dark 文件。
    从文件名解析 EXPOSURE-XXX.XXs，找最接近 exposure 的。
    tolerance 为容差（秒）。

    Args:
        dark_dir: str，存放 Master Dark 文件的目录
        exposure: float，目标曝光时间（秒）
        tolerance: float，曝光时间容差（秒）

    Returns:
        str|None: 匹配的 Master Dark 文件完整路径，无匹配返回 None
    """
    if not os.path.isdir(dark_dir):
        logger.warning("Dark 目录不存在: %s", dark_dir)
        return None

    candidates = []
    for fname in os.listdir(dark_dir):
        lower = fname.lower()
        if "dark" not in lower:
            continue
        m = _RE_DARK_EXPOSURE.search(fname)
        if not m:
            continue
        try:
            exp = float(m.group(1))
        except ValueError:
            continue
        diff = abs(exp - exposure)
        candidates.append((diff, exp, fname))

    if not candidates:
        logger.warning("在 %s 中未找到包含 EXPOSURE 信息的 Dark 文件", dark_dir)
        return None

    candidates.sort(key=lambda x: x[0])
    best_diff, best_exp, best_fname = candidates[0]

    if best_diff > tolerance:
        logger.warning(
            "最接近的 Dark 曝光 %.2fs 与目标 %.2fs 差异 %.2fs 超过容差 %.2fs",
            best_exp, exposure, best_diff, tolerance,
        )
        return None

    path = os.path.join(dark_dir, best_fname)
    logger.info(
        "匹配 Master Dark: %s (曝光 %.2fs, 目标 %.2fs, 差异 %.2fs)",
        best_fname, best_exp, exposure, best_diff,
    )
    return path


def find_matching_master_flat(flat_dir, filter_name):
    """
    在 flat_dir 中查找滤镜匹配的 Master Flat 文件。
    从文件名解析 FILTER-XXX_mono，匹配 filter_name。

    Args:
        flat_dir: str，存放 Master Flat 文件的目录
        filter_name: str，目标滤镜名（如 "Red", "H-alpha"）

    Returns:
        str|None: 匹配的 Master Flat 文件完整路径，无匹配返回 None
    """
    if not os.path.isdir(flat_dir):
        logger.warning("Flat 目录不存在: %s", flat_dir)
        return None

    target = filter_name.strip().lower()
    if not target:
        logger.warning("滤镜名为空，无法匹配 Master Flat")
        return None

    for fname in os.listdir(flat_dir):
        lower = fname.lower()
        if "flat" not in lower:
            continue
        m = _RE_FLAT_FILTER.search(fname)
        if not m:
            continue
        if m.group(1).lower() == target:
            path = os.path.join(flat_dir, fname)
            logger.info("匹配 Master Flat: %s (滤镜 %s)", fname, filter_name)
            return path

    logger.warning("在 %s 中未找到滤镜 %s 的 Flat 文件", flat_dir, filter_name)
    return None


def _find_master_bias(calibration_dir):
    """
    在 calibration_dir 中查找 Master Bias 文件。
    查找文件名包含 "bias" 且包含 "master" 的文件。

    Returns:
        str|None: 匹配的 Master Bias 文件完整路径，无匹配返回 None
    """
    if not os.path.isdir(calibration_dir):
        return None

    for fname in os.listdir(calibration_dir):
        lower = fname.lower()
        if "bias" in lower and "master" in lower:
            path = os.path.join(calibration_dir, fname)
            logger.info("匹配 Master Bias: %s", fname)
            return path

    logger.warning("在 %s 中未找到 Master Bias 文件", calibration_dir)
    return None


# ============================ 数据范围统一 ============================

def _bitpix_to_maxval(bitpix):
    """
    将 BITPIX 转为最大值（2^bitpix - 1）。
    负数（浮点类型 BITPIX=-32/-64）默认用 16 位（65535）。
    """
    if bitpix is None or bitpix <= 0:
        bitpix = 16
    return float((1 << int(bitpix)) - 1)


def unify_data_range(light_data, light_bitpix, master_data, master_bitpix):
    """
    统一 Light 和主帧的数据范围。
    如果一方是归一化的（max<=1.5）而另一方不是，将归一化的乘以 2^bitpix-1。
    bitpix 取"非归一化那一方"的 bitpix（负数/浮点用 16 位默认）。

    Args:
        light_data: np.ndarray (H, W)
        light_bitpix: int，Light 的 BITPIX
        master_data: np.ndarray (H, W)
        master_bitpix: int，Master 的 BITPIX

    Returns:
        (light_data, master_data): 统一范围后的两个数组（float32）
    """
    light = np.asarray(light_data, dtype=np.float32)
    master = np.asarray(master_data, dtype=np.float32)

    light_max = float(np.max(light))
    master_max = float(np.max(master))
    light_normalized = light_max <= 1.5
    master_normalized = master_max <= 1.5

    if light_normalized == master_normalized:
        # 两者都归一化或都不归一化，无需调整
        logger.debug(
            "数据范围一致: light_max=%.4f, master_max=%.4f, 无需统一",
            light_max, master_max,
        )
        return light, master

    if light_normalized and not master_normalized:
        # Light 归一化，Master 不归一化：Light 乘以 Master 的 maxval
        maxval = _bitpix_to_maxval(master_bitpix)
        light = (light * maxval).astype(np.float32)
        logger.info(
            "统一数据范围: Light 归一化(max=%.4f) -> 乘以 %.0f (master bitpix=%d)",
            light_max, maxval, master_bitpix,
        )
    else:
        # Master 归一化，Light 不归一化：Master 乘以 Light 的 maxval
        maxval = _bitpix_to_maxval(light_bitpix)
        master = (master * maxval).astype(np.float32)
        logger.info(
            "统一数据范围: Master 归一化(max=%.4f) -> 乘以 %.0f (light bitpix=%d)",
            master_max, maxval, light_bitpix,
        )

    return light, master


# ============================ 背景区域提取 ============================

def extract_background_mask(data):
    """
    提取背景区域掩码（排除星点）。
    用 sigma-clip 方法：计算全局 median 和 MAD，
    保留 |data - median| <= 3*1.4826*MAD 的像素作为背景。

    Args:
        data: np.ndarray (H, W)

    Returns:
        np.ndarray (H, W) bool，True 表示背景像素
    """
    data = np.asarray(data, dtype=np.float32)
    median = float(np.median(data))
    mad = float(np.median(np.abs(data - median)))
    sigma = 1.4826 * mad

    if sigma <= 0:
        logger.warning("背景提取: MAD=%.6f (<=0)，无法估计噪声，返回全图掩码", mad)
        return np.ones(data.shape, dtype=bool)

    mask = np.abs(data - median) <= 3.0 * sigma
    n_bg = int(mask.sum())
    n_total = mask.size
    logger.info(
        "背景提取: median=%.4f, MAD=%.4f, sigma=%.4f, 背景像素 %d/%d (%.2f%%)",
        median, mad, sigma, n_bg, n_total, 100.0 * n_bg / n_total if n_total > 0 else 0.0,
    )
    return mask


# ============================ 暗场优化 ============================

def optimize_dark_scale(light, bias, dark, flat, k_init):
    """
    黄金分割搜索最优 K 值，使校准后背景区域 MAD 最小。

    算法:
        1. 提取背景掩码
        2. 搜索范围 [k_init*0.5, k_init*1.5]
        3. 对候选 K，计算 (light - bias - K*(dark-bias)) / flat 的背景 MAD
        4. 黄金分割搜索（0.618 法），精度 0.01，最大 20 次迭代

    Args:
        light: np.ndarray (H, W)，Light 帧
        bias: np.ndarray (H, W)，Master Bias
        dark: np.ndarray (H, W)，Master Dark
        flat: np.ndarray (H, W)|None，已归一化的 Master Flat（None 时不除 flat）
        k_init: float，初始 K 值（搜索中心）

    Returns:
        float: 最优 K 值
    """
    logger.info("暗场优化开始: k_init=%.4f", k_init)

    # 1. 提取背景掩码
    bg_mask = extract_background_mask(light)
    if int(bg_mask.sum()) < 100:
        logger.warning("背景像素过少(%d)，跳过暗场优化，返回 k_init", int(bg_mask.sum()))
        return float(k_init)

    # 2. 预计算背景区域的值（避免每次迭代全图运算）
    light_bg = light[bg_mask].astype(np.float64)
    bias_bg = bias[bg_mask].astype(np.float64)
    dark_minus_bias_bg = (dark.astype(np.float64) - bias.astype(np.float64))[bg_mask]
    if flat is not None:
        flat_bg = flat[bg_mask].astype(np.float64)
        flat_bg = np.where(flat_bg > 1e-10, flat_bg, 1.0)  # 防除零
    else:
        flat_bg = None

    def objective(k):
        """目标函数：背景区域的 MAD"""
        residual = light_bg - bias_bg - k * dark_minus_bias_bg
        if flat_bg is not None:
            residual = residual / flat_bg
        med = np.median(residual)
        mad = np.median(np.abs(residual - med))
        return float(mad)

    # 3. 黄金分割搜索（0.618 法）
    phi = 0.6180339887498949  # (√5 - 1) / 2
    a = k_init * 0.5
    b = k_init * 1.5

    x1 = b - phi * (b - a)
    x2 = a + phi * (b - a)
    f1 = objective(x1)
    f2 = objective(x2)

    for i in range(20):
        if (b - a) < 0.01:
            logger.debug("  迭代 %d: 区间 [%.4f, %.4f] < 0.01，收敛", i + 1, a, b)
            break
        if f1 < f2:
            # 最小值在 [a, x2]，收缩右边界
            b, x2, f2 = x2, x1, f1
            x1 = b - phi * (b - a)
            f1 = objective(x1)
        else:
            # 最小值在 [x1, b]，收缩左边界
            a, x1, f1 = x1, x2, f2
            x2 = a + phi * (b - a)
            f2 = objective(x2)
        logger.debug(
            "  迭代 %d: [%.4f, %.4f], x1=%.4f(f=%.6f), x2=%.4f(f=%.6f)",
            i + 1, a, b, x1, f1, x2, f2,
        )

    best_k = (a + b) / 2.0
    best_mad = objective(best_k)
    logger.info("暗场优化完成: 最优 K=%.4f, 背景MAD=%.6f (迭代 %d 轮)", best_k, best_mad, i + 1)
    return float(best_k)


# ============================ 标准校准函数 ============================

def calibrate(light_data, master_bias=None, master_dark=None, master_flat=None,
              dark_scale_factor=1.0, dark_optimization=False,
              light_exposure=0.0, dark_exposure=0.0):
    """
    标准 CCD 校准。

    无暗场优化: Calibrated = (Light - Dark) / NormalizedFlat
        Dark 已含 Bias，直接减 Dark 即可，不需要 Bias。

    有暗场优化: Calibrated = (Light - Bias - K*(Dark - Bias)) / NormalizedFlat
        需要从 Dark 中提取纯暗噪声 (Dark - Bias)，乘以系数 K，
        Light 减去 Bias 和 K*纯暗噪声。
        Bias 此时是必需的。

    dark_optimization=True 时：调用 optimize_dark_scale 搜索最优 K
    dark_optimization=False 时：直接用 (Light - Dark) / Flat

    Flat 归一化: median=1.0，最小值裁剪 0.1

    Args:
        light_data: np.ndarray (H, W)，Light 帧
        master_bias: np.ndarray|None，Master Bias（仅暗场优化时需要）
        master_dark: np.ndarray|None，Master Dark（含 bias）
        master_flat: np.ndarray|None，Master Flat（已减 Bias 并归一化）
        dark_scale_factor: float，暗场缩放因子（不优化时直接使用）
        dark_optimization: bool，是否启用暗场优化
        light_exposure: float，Light 曝光时间（用于暗场优化的 k_init 估计）
        dark_exposure: float，Dark 曝光时间（用于暗场优化的 k_init 估计）

    Returns:
        (calibrated_data, actual_k, stats_dict):
            calibrated_data: np.ndarray (H, W) float32
            actual_k: float，实际使用的 K 值
            stats_dict: {"before": {...}, "after": {...}}，校准前后的 min/max/mean/std
    """
    light = np.asarray(light_data, dtype=np.float32)
    bias = np.asarray(master_bias, dtype=np.float32) if master_bias is not None else None
    dark = np.asarray(master_dark, dtype=np.float32) if master_dark is not None else None
    flat = np.asarray(master_flat, dtype=np.float32) if master_flat is not None else None

    # 统计校准前
    stats_before = {
        "min": float(np.min(light)), "max": float(np.max(light)),
        "mean": float(np.mean(light)), "std": float(np.std(light)),
    }
    logger.info(
        "校准前统计: min=%.4f, max=%.4f, mean=%.4f, std=%.4f",
        stats_before["min"], stats_before["max"], stats_before["mean"], stats_before["std"],
    )

    # Flat 归一化（median=1.0，最小值裁剪 0.1）
    if flat is not None:
        flat_median = float(np.median(flat))
        if flat_median > 0:
            flat = flat / flat_median
            flat = np.maximum(flat, 0.1).astype(np.float32)
            logger.info(
                "Flat 归一化: 原始 median=%.4f -> 归一化后 min=%.4f, max=%.4f, median=%.4f",
                flat_median, float(np.min(flat)), float(np.max(flat)), float(np.median(flat)),
            )
        else:
            logger.warning("Flat median=%.4f (<=0)，跳过归一化", flat_median)

    if dark_optimization:
        # 暗场优化模式: (Light - Bias - K*(Dark - Bias)) / Flat
        # K = t_light / t_dark，从 FITS 头 EXPTIME 读取
        if bias is None:
            logger.error("暗场优化需要 Master Bias，但未提供")
            actual_k = 1.0
        elif dark is None:
            logger.error("暗场优化需要 Master Dark，但未提供")
            actual_k = 1.0
        else:
            # K = t_light / t_dark（从 FITS EXPTIME 计算）
            if light_exposure <= 0 or dark_exposure <= 0:
                raise ValueError(
                    "暗场优化需要 FITS 头 EXPTIME 关键字计算 K = t_light/t_dark，"
                    "但 light_exposure=%.4f, dark_exposure=%.4f" % (light_exposure, dark_exposure)
                )
            actual_k = light_exposure / dark_exposure
            logger.info("暗场优化: K = t_light/t_dark = %.2f/%.2f = %.4f",
                        light_exposure, dark_exposure, actual_k)

        calibrated = light.astype(np.float32, copy=True)
        if bias is not None:
            calibrated = calibrated - bias
        if dark is not None and bias is not None:
            # 纯暗噪声 = Dark - Bias
            dark_current = dark - bias
            calibrated = calibrated - actual_k * dark_current
        if flat is not None:
            calibrated = calibrated / flat
    else:
        # 标准模式: (Light - Dark) / Flat
        # Dark 已含 Bias，直接减 Dark
        actual_k = 1.0
        logger.info("标准校准: (Light - Dark) / Flat（Dark 已含 Bias）")

        calibrated = light.astype(np.float32, copy=True)
        if dark is not None:
            calibrated = calibrated - dark
        else:
            logger.warning("无 Master Dark，跳过暗场扣除")
        if flat is not None:
            calibrated = calibrated / flat

    calibrated = calibrated.astype(np.float32)

    # 统计校准后
    stats_after = {
        "min": float(np.min(calibrated)), "max": float(np.max(calibrated)),
        "mean": float(np.mean(calibrated)), "std": float(np.std(calibrated)),
    }
    logger.info(
        "校准后统计: min=%.4f, max=%.4f, mean=%.4f, std=%.4f",
        stats_after["min"], stats_after["max"], stats_after["mean"], stats_after["std"],
    )

    stats = {"before": stats_before, "after": stats_after}
    return calibrated, actual_k, stats


# ============================ Calibrator 类 ============================

class Calibrator:
    """图像校准器：支持文件模式与内存直通模式"""

    def __init__(self, max_workers=4):
        """
        Args:
            max_workers: int，C++ DLL OpenMP 线程数
        """
        self._max_workers = max_workers
        self._reader = ImageReader()
        self._writer = FITSWriter()
        _ac_dll.ac_set_num_threads(max_workers)
        logger.info("Calibrator 初始化: max_workers=%d（C++ DLL 线程数已设置）", max_workers)

    def calibrate_frame(self, light_path, output_path,
                        master_bias=None, master_dark=None, master_flat=None,
                        dark_optimization=False,
                        calibration_dir=None):
        """
        校准单帧 Light（文件模式）。

        流程:
            1. 读取 Light 帧，获取 data / keywords / EXPTIME / FILTER / BITPIX
            2. 若 calibration_dir 提供，自动匹配未显式指定的主帧
            3. 加载主帧数据，统一数据范围
            4. 调用 calibrate_data 进行校准
            5. 写 FITS，头记录 CALIBRAT / DARKSCAL / MASTERBI / MASTERDA / MASTERFL

        Args:
            light_path: str，Light 帧路径（FITS/XISF）
            output_path: str，输出 FITS 路径
            master_bias: str|None，Master Bias 路径（None 且有 calibration_dir 时自动匹配）
            master_dark: str|None，Master Dark 路径
            master_flat: str|None，Master Flat 路径
            dark_optimization: bool，是否启用暗场优化
            calibration_dir: str|None，主校准帧目录（用于自动匹配）

        Returns:
            dict: success, output_path, stats, dark_scale_factor
                  失败时额外含 error / light_path
        """
        logger.info("=" * 60)
        logger.info("图像校准（文件模式）: %s -> %s", light_path, output_path)

        try:
            # 1. 读取 Light 帧
            img = self._reader.read(light_path)
            light_data = img.data.astype(np.float32, copy=True)
            light_keywords = img.keywords
            light_exposure = img.get_keyword_float("EXPTIME", 0.0)
            light_filter = img.get_keyword("FILTER", "") or ""
            light_bitpix = img.get_keyword_int("BITPIX", 16)
            img.close()

            logger.info(
                "Light 加载: shape=%s, EXPTIME=%.2fs, FILTER=%s, BITPIX=%d",
                str(light_data.shape), light_exposure, light_filter, light_bitpix,
            )

            # 2. 主帧自动匹配（calibration_dir 提供时，补充未显式指定的主帧）
            bias_path = master_bias
            dark_path = master_dark
            flat_path = master_flat

            if calibration_dir is not None:
                if bias_path is None:
                    bias_path = _find_master_bias(calibration_dir)
                if dark_path is None and light_exposure > 0:
                    dark_path = find_matching_master_dark(calibration_dir, light_exposure)
                if flat_path is None and light_filter:
                    flat_path = find_matching_master_flat(calibration_dir, light_filter)

            # 3. 加载主帧数据并统一数据范围
            bias_data = None
            bias_bitpix = None
            dark_data = None
            dark_exposure = 0.0
            dark_bitpix = None
            flat_data = None
            flat_bitpix = None

            if bias_path is not None:
                bimg = self._reader.read(bias_path)
                bias_data = bimg.data.astype(np.float32, copy=True)
                bias_bitpix = bimg.get_keyword_int("BITPIX", -32)
                bimg.close()
                logger.info("Master Bias 加载: shape=%s, BITPIX=%d", str(bias_data.shape), bias_bitpix)

            if dark_path is not None:
                dimg = self._reader.read(dark_path)
                dark_data = dimg.data.astype(np.float32, copy=True)
                dark_exposure = dimg.get_keyword_float("EXPTIME", 0.0)
                dark_bitpix = dimg.get_keyword_int("BITPIX", -32)
                dimg.close()
                logger.info(
                    "Master Dark 加载: shape=%s, EXPTIME=%.2fs, BITPIX=%d",
                    str(dark_data.shape), dark_exposure, dark_bitpix,
                )

            if flat_path is not None:
                fimg = self._reader.read(flat_path)
                flat_data = fimg.data.astype(np.float32, copy=True)
                flat_bitpix = fimg.get_keyword_int("BITPIX", -32)
                fimg.close()
                logger.info("Master Flat 加载: shape=%s, BITPIX=%d", str(flat_data.shape), flat_bitpix)

            # 4. 统一数据范围（Light 与各主帧分别统一）
            if bias_data is not None:
                light_data, bias_data = unify_data_range(
                    light_data, light_bitpix, bias_data, bias_bitpix,
                )
            if dark_data is not None:
                light_data, dark_data = unify_data_range(
                    light_data, light_bitpix, dark_data, dark_bitpix,
                )
            if flat_data is not None:
                light_data, flat_data = unify_data_range(
                    light_data, light_bitpix, flat_data, flat_bitpix,
                )

            # 5. 校准
            calibrated, actual_k, stats = calibrate(
                light_data,
                master_bias=bias_data,
                master_dark=dark_data,
                master_flat=flat_data,
                dark_scale_factor=1.0,
                dark_optimization=dark_optimization,
                light_exposure=light_exposure,
                dark_exposure=dark_exposure,
            )

            # 6. 写 FITS
            # 过滤 BZERO/BSCALE：校准后为 float32 数据，不应携带原始 16 位帧的 BZERO/BSCALE
            # 否则后续读取时 C++ 会再次应用 BZERO 导致数据偏移
            out_keywords = [
                kw for kw in (light_keywords or [])
                if kw.name.upper() not in ("BZERO", "BSCALE")
            ]
            out_keywords.append(FITSKeywordPy(name="CALIBRAT", value="T", comment="Calibrated"))
            out_keywords.append(FITSKeywordPy(name="DARKSCAL", value=f"{actual_k:.6f}", comment="Dark scale factor K"))
            if bias_path is not None:
                out_keywords.append(FITSKeywordPy(
                    name="MASTERBI", value=os.path.basename(bias_path),
                    comment="Master Bias file",
                ))
            if dark_path is not None:
                out_keywords.append(FITSKeywordPy(
                    name="MASTERDA", value=os.path.basename(dark_path),
                    comment="Master Dark file",
                ))
            if flat_path is not None:
                out_keywords.append(FITSKeywordPy(
                    name="MASTERFL", value=os.path.basename(flat_path),
                    comment="Master Flat file",
                ))

            out_dir = os.path.dirname(output_path)
            if out_dir:
                os.makedirs(out_dir, exist_ok=True)

            self._writer.write(calibrated, output_path, keywords=out_keywords, float_sample=True)
            logger.info("校准结果写入完成: %s", output_path)

            return {
                "success": True,
                "output_path": output_path,
                "stats": stats,
                "dark_scale_factor": actual_k,
            }

        except Exception as e:
            logger.error("校准失败: %s", e, exc_info=True)
            return {
                "success": False,
                "error": str(e),
                "light_path": light_path,
                "output_path": output_path,
            }

    def calibrate_data(self, light_data,
                       master_bias=None, master_dark=None, master_flat=None,
                       dark_optimization=False,
                       light_exposure=0.0, dark_exposure=0.0):
        """
        在内存中校准（生产模式用），调用 C++ DLL ac_calibrate_frame。
        输入 numpy 数组，返回 (calibrated_data, stats_dict)。
        不读写文件。

        假设数据范围已统一（上游负责），内部不调用 unify_data_range。
        暗场缩放因子: 若 light_exposure 和 dark_exposure 都 > 0，则 K_init = light/dark，
                     否则 K_init = 1.0。dark_optimization=True 时以此为初值搜索。
        Flat 归一化（median=1.0，clip 0.1）在 Python 端预处理后传给 DLL，
        其余校准运算（减 bias/dark、除 flat、暗场优化搜索）全部由 C++ DLL 完成。

        Args:
            light_data: np.ndarray (H, W)，Light 帧数据
            master_bias: np.ndarray|None，Master Bias 数据
            master_dark: np.ndarray|None，Master Dark 数据
            master_flat: np.ndarray|None，Master Flat 数据
            dark_optimization: bool，是否启用暗场优化
            light_exposure: float，Light 曝光时间（秒）
            dark_exposure: float，Dark 曝光时间（秒）

        Returns:
            (calibrated_data, stats_dict):
                calibrated_data: np.ndarray (H, W) float32
                stats_dict: {"before": {...}, "after": {...}, "dark_scale_factor": float}
        """
        logger.info("=" * 60)
        logger.info("图像校准（内存直通模式，C++ DLL）: shape=%s", str(np.asarray(light_data).shape))

        light = np.ascontiguousarray(light_data, dtype=np.float32)
        h, w = light.shape[0], light.shape[1]

        # 统计校准前
        stats_before = {
            "min": float(np.min(light)), "max": float(np.max(light)),
            "mean": float(np.mean(light)), "std": float(np.std(light)),
        }
        logger.info(
            "校准前统计: min=%.4f, max=%.4f, mean=%.4f, std=%.4f",
            stats_before["min"], stats_before["max"], stats_before["mean"], stats_before["std"],
        )

        # 准备主帧数据（float32 连续内存）
        bias = np.ascontiguousarray(master_bias, dtype=np.float32) if master_bias is not None else None
        dark = np.ascontiguousarray(master_dark, dtype=np.float32) if master_dark is not None else None
        flat = np.ascontiguousarray(master_flat, dtype=np.float32) if master_flat is not None else None

        # Flat 归一化（median=1.0，最小值裁剪 0.1）
        # C++ DLL 内部仅裁剪 0.1 不做 median 归一化，需在 Python 端预处理
        if flat is not None:
            flat_median = float(np.median(flat))
            if flat_median > 0:
                flat = np.maximum(flat / flat_median, 0.1).astype(np.float32)
                logger.info(
                    "Flat 归一化: 原始 median=%.4f -> 归一化后 min=%.4f, max=%.4f, median=%.4f",
                    flat_median, float(np.min(flat)), float(np.max(flat)), float(np.median(flat)),
                )
            else:
                logger.warning("Flat median=%.4f (<=0)，跳过归一化", flat_median)

        # 计算暗场缩放因子初值
        if light_exposure > 0 and dark_exposure > 0:
            dark_scale_factor = light_exposure / dark_exposure
            logger.info(
                "暗场缩放因子: light_exp/dark_exp = %.2f/%.2f = %.4f",
                light_exposure, dark_exposure, dark_scale_factor,
            )
        else:
            dark_scale_factor = 1.0
            logger.info("暗场缩放因子: 1.0（曝光时间未提供）")

        # 调用 C++ DLL 校准
        # API: ac_calibrate_frame(light, w, h, dark, flat, bias, out, dark_opt, k_init, actual_k)
        # 无暗场优化: (Light - Dark) / Flat
        # 有暗场优化: (Light - Bias - K*(Dark - Bias)) / Flat
        out = np.zeros((h, w), dtype=np.float32)
        actual_k = ctypes.c_float(dark_scale_factor)

        ret = _ac_dll.ac_calibrate_frame(
            _to_float_ptr(light), w, h,
            _to_float_ptr(dark) if dark is not None else None,
            _to_float_ptr(flat) if flat is not None else None,
            _to_float_ptr(bias) if bias is not None else None,
            _to_float_ptr(out),
            1 if dark_optimization else 0, dark_scale_factor,
            ctypes.pointer(actual_k),
        )

        if ret != _AC_OK:
            logger.error("ac_calibrate_frame 失败，错误码: %d", ret)
            raise RuntimeError("C++ DLL ac_calibrate_frame 失败，错误码: %d" % ret)

        k_val = float(actual_k.value)
        logger.info("C++ DLL 校准完成: K=%.4f", k_val)

        # 统计校准后
        stats_after = {
            "min": float(np.min(out)), "max": float(np.max(out)),
            "mean": float(np.mean(out)), "std": float(np.std(out)),
        }
        logger.info(
            "校准后统计: min=%.4f, max=%.4f, mean=%.4f, std=%.4f",
            stats_after["min"], stats_after["max"], stats_after["mean"], stats_after["std"],
        )

        stats = {"before": stats_before, "after": stats_after, "dark_scale_factor": k_val}
        return out, stats
