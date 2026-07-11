# -*- coding: utf-8 -*-
"""
Master Generator - 主校准帧生成模块
功能: 从多帧原始校准帧（Bias/Dark/Flat）合并为主校准帧
用途: 天文图像标准校准流程的第一步，生成 Master Bias / Master Dark / Master Flat，
      支持 sigma-clip 离群值剔除与 median/mean 合并，输出 FITS float32 格式
依赖: numpy, logging, astro_image_io (ImageReader / FITSWriter / FITSKeywordPy)
调用: from master_generator import MasterGenerator
      gen = MasterGenerator(max_workers=16)
      gen.generate_master_bias(bias_paths, "master_bias.fits")
      gen.generate_master_dark(dark_paths, "master_dark.fits", master_bias_path="master_bias.fits")
      gen.generate_master_flat(flat_paths, "master_flat.fits", master_bias_path="master_bias.fits")
数据流: 多帧原始图像 -> 并行加载堆叠 (N,H,W) -> [减Bias] -> [逐帧归一化(Flat)] -> sigma-clip -> median/mean 合并 -> 写 FITS
"""

from __future__ import annotations

import os
import sys
import logging
import warnings
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor
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


# ============================ 日志配置 ============================

_LOG_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "logs")
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR, "master_generator_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("master_generator")
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


# ============================ 核心算法 ============================

def sigma_clip_reject(stack, sigma_low=3.0, sigma_high=3.0, max_iterations=5):
    """
    向量化 sigma-clip 离群值剔除：对 stack (N,H,W) 沿 axis=0 迭代剔除离群值。
    使用 NaN 标记被剔除的值，后续用 np.nanmedian / np.nanmean 计算。
    全程 numpy 向量化，禁止逐像素 Python 循环。

    Args:
        stack: np.ndarray (N, H, W) float32/float64
        sigma_low: 下限 sigma 倍数
        sigma_high: 上限 sigma 倍数
        max_iterations: 最大迭代次数

    Returns:
        np.ndarray (N, H, W)，被剔除位置为 NaN（工作副本，不修改原始数据）
    """
    work = stack.astype(np.float64, copy=True)  # float64 保证精度，不修改原始
    n_total = work.shape[0]
    n_pixels = work.shape[1] * work.shape[2]

    logger.info(
        "sigma-clip 开始: N=%d, shape=(%d,%d), sigma_low=%.1f, sigma_high=%.1f, max_iter=%d",
        n_total, work.shape[1], work.shape[2], sigma_low, sigma_high, max_iterations,
    )

    for it in range(max_iterations):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", category=RuntimeWarning)
            # 沿 axis=0 计算中位数（忽略已有 NaN）
            median = np.nanmedian(work, axis=0)  # (H, W)
            # 绝对偏差
            abs_dev = np.abs(work - median)  # (N, H, W)，NaN 自动传播
            # MAD = 中位数绝对偏差
            mad = np.nanmedian(abs_dev, axis=0)  # (H, W)

        # 归一化 MAD -> 等效 sigma（1.4826 = 1/Phi^-1(0.75)）
        sigma = 1.4826 * mad
        # 对常量区域（sigma=0）不进行剔除，避免误杀
        safe_sigma = np.where(sigma > 0, sigma, np.inf)

        lower = median - sigma_low * safe_sigma   # (H, W)
        upper = median + sigma_high * safe_sigma  # (H, W)

        # 生成保留掩码：在 [lower, upper] 范围内的值保留，NaN 值不参与比较
        mask = (work >= lower) & (work <= upper)  # (N, H, W)，NaN 处为 False
        # 已被剔除的 NaN 位置保持 NaN（不重新激活）
        rejected_this_iter = (~mask & ~np.isnan(work))
        n_rejected = int(rejected_this_iter.sum())

        # 将掩码外的值设为 NaN
        work[~mask] = np.nan

        n_nan_total = int(np.isnan(work).sum())
        logger.info(
            "  迭代 %d/%d: 本轮剔除 %d 像素 (%.4f%%), 累计 NaN %d (%.4f%%)",
            it + 1, max_iterations, n_rejected,
            100.0 * n_rejected / (n_total * n_pixels) if n_pixels > 0 else 0.0,
            n_nan_total,
            100.0 * n_nan_total / (n_total * n_pixels) if n_pixels > 0 else 0.0,
        )

        # 收敛判定：本轮无新剔除则提前结束
        if n_rejected == 0:
            logger.info("  sigma-clip 收敛，迭代 %d 轮后无新剔除", it + 1)
            break

    n_final_nan = int(np.isnan(work).sum())
    logger.info(
        "sigma-clip 完成: 总剔除 %d 像素 (%.4f%%)",
        n_final_nan,
        100.0 * n_final_nan / (n_total * n_pixels) if n_pixels > 0 else 0.0,
    )
    return work


def combine_frames(stack, rejection="sigma_clip", combine="median",
                   sigma_low=3.0, sigma_high=3.0, max_iterations=5):
    """
    合并多帧图像。rejection 与 combine 两个参数独立组合。

    Args:
        stack: np.ndarray (N, H, W)
        rejection: "none" | "sigma_clip"
        combine: "median" | "mean"
        sigma_low, sigma_high, max_iterations: sigma-clip 参数

    Returns:
        np.ndarray (H, W) float32 合并结果
    """
    if rejection not in ("none", "sigma_clip"):
        raise ValueError(f"不支持的 rejection 模式: {rejection}（可选: none / sigma_clip）")
    if combine not in ("median", "mean"):
        raise ValueError(f"不支持的 combine 模式: {combine}（可选: median / mean）")

    logger.info("合并帧: rejection=%s, combine=%s", rejection, combine)

    if rejection == "sigma_clip":
        work = sigma_clip_reject(stack, sigma_low, sigma_high, max_iterations)
    else:
        work = stack

    with warnings.catch_warnings():
        warnings.simplefilter("ignore", category=RuntimeWarning)
        if combine == "median":
            result = np.nanmedian(work, axis=0)
        else:  # mean
            result = np.nanmean(work, axis=0)

    # 检查全 NaN 像素（所有帧该像素都被剔除）
    n_all_nan = int(np.isnan(result).sum())
    if n_all_nan > 0:
        logger.warning("合并结果中有 %d 像素所有帧均被剔除（NaN），建议检查输入数据", n_all_nan)

    result = result.astype(np.float32)
    logger.info(
        "合并统计: mean=%.4f, std=%.4f, min=%.4f, max=%.4f, median=%.4f",
        float(np.nanmean(result)), float(np.nanstd(result)),
        float(np.nanmin(result)), float(np.nanmax(result)),
        float(np.nanmedian(result)),
    )
    return result


def normalize_flat(flat_data):
    """
    归一化平场到 median=1.0，最小值裁剪 0.1。

    Args:
        flat_data: np.ndarray (H, W)

    Returns:
        np.ndarray (H, W) float32，归一化后的平场
    """
    flat = np.asarray(flat_data, dtype=np.float32)
    median_val = float(np.nanmedian(flat))
    if median_val <= 0:
        logger.warning("平场中位数为 %.4f（<=0），跳过归一化", median_val)
        return flat
    normalized = flat / median_val
    # 最小值裁剪到 0.1，避免除零和过度校正
    normalized = np.maximum(normalized, 0.1)
    logger.info(
        "平场归一化: 原始 median=%.4f -> 归一化后 mean=%.4f, min=%.4f, max=%.4f",
        median_val, float(np.nanmean(normalized)),
        float(np.nanmin(normalized)), float(np.nanmax(normalized)),
    )
    return normalized.astype(np.float32)


# ============================ 辅助函数 ============================

def _find_keyword(keywords, name):
    """从关键词列表中查找指定关键词的值（字符串），找不到返回 None"""
    target = name.upper()
    for kw in keywords:
        if kw.name.upper() == target:
            return kw.value
    return None


def _find_keyword_float(keywords, name, default=0.0):
    """从关键词列表中查找浮点值，找不到或解析失败返回 default"""
    val = _find_keyword(keywords, name)
    if val is None:
        return default
    try:
        return float(val)
    except (ValueError, TypeError):
        return default


def _log_array_stats(label, arr):
    """记录数组的统计信息"""
    logger.info(
        "%s 统计: shape=%s, mean=%.4f, std=%.4f, min=%.4f, max=%.4f, median=%.4f",
        label, str(arr.shape),
        float(np.nanmean(arr)), float(np.nanstd(arr)),
        float(np.nanmin(arr)), float(np.nanmax(arr)),
        float(np.nanmedian(arr)),
    )


# ============================ 主帧生成器 ============================

class MasterGenerator:
    """主校准帧生成器：从多帧 Bias/Dark/Flat 生成 Master 校准帧"""

    def __init__(self, max_workers=4):
        """
        Args:
            max_workers: 并行加载帧的最大线程数
        """
        self._max_workers = max_workers
        self._reader = ImageReader()
        self._writer = FITSWriter()
        logger.info("MasterGenerator 初始化: max_workers=%d", max_workers)

    def _load_frames(self, frame_paths):
        """
        并行加载多帧图像，返回 (stack, keywords_list)。

        Returns:
            stack: np.ndarray (N, H, W) float32
            keywords_list: list[list[FITSKeywordPy]]，每帧的 FITS 头关键词
        """
        if not frame_paths:
            raise ValueError("frame_paths 为空，至少需要 1 帧")

        logger.info("开始加载 %d 帧图像 (max_workers=%d)...", len(frame_paths), self._max_workers)

        def _load_one(path):
            img = self._reader.read(path)
            data = img.data  # 已是 float32 副本
            kws = img.keywords
            img.close()
            return data, kws

        # 并行加载（ctypes 调用释放 GIL，线程池可加速 I/O）
        with ThreadPoolExecutor(max_workers=self._max_workers) as ex:
            results = list(ex.map(_load_one, frame_paths))

        data_list = [r[0] for r in results]
        kw_list = [r[1] for r in results]

        # 校验形状一致性
        ref_shape = data_list[0].shape
        for i, d in enumerate(data_list):
            if d.shape != ref_shape:
                raise ValueError(
                    f"帧 {i} 形状 {d.shape} 与首帧 {ref_shape} 不一致: {frame_paths[i]}"
                )

        stack = np.stack(data_list, axis=0)  # (N, H, W)
        logger.info("加载完成: stack shape=%s, dtype=%s", str(stack.shape), stack.dtype)
        _log_array_stats("输入帧统计", stack)
        return stack, kw_list

    def _load_master_bias(self, master_bias_path):
        """加载 Master Bias 帧，返回 (H, W) float32"""
        logger.info("加载 Master Bias: %s", master_bias_path)
        img = self._reader.read(master_bias_path)
        bias = img.data.copy()
        img.close()
        _log_array_stats("Master Bias", bias)
        return bias

    def _build_keywords(self, imagetyp, ncombine, exptime=None, filter_name=None):
        """构造 FITS 头关键词列表"""
        kws = [
            FITSKeywordPy(name="IMAGETYP", value=imagetyp, comment="Frame type"),
            FITSKeywordPy(name="NCOMBINE", value=str(ncombine), comment="Number of frames combined"),
            FITSKeywordPy(name="BUNIT", value="ADU", comment="Brightness units"),
        ]
        if exptime is not None:
            kws.append(FITSKeywordPy(name="EXPTIME", value=f"{exptime:.6f}", comment="Exposure time (seconds)"))
        if filter_name is not None:
            kws.append(FITSKeywordPy(name="FILTER", value=str(filter_name), comment="Filter name"))
        return kws

    def generate_master_bias(self, frame_paths, output_path,
                             rejection="sigma_clip", combine="median"):
        """
        生成 Master Bias：加载多帧 -> sigma-clip -> median 合并 -> 写 FITS

        Args:
            frame_paths: list[str]，Bias 帧路径列表
            output_path: str，输出 FITS 路径
            rejection: "none" | "sigma_clip"
            combine: "median" | "mean"
        """
        logger.info("=" * 60)
        logger.info("生成 Master Bias: %d 帧 -> %s", len(frame_paths), output_path)

        stack, _ = self._load_frames(frame_paths)
        result = combine_frames(stack, rejection=rejection, combine=combine)

        keywords = self._build_keywords("Master Bias", len(frame_paths))
        self._writer.write(result, output_path, keywords=keywords, float_sample=True)
        logger.info("Master Bias 写入完成: %s", output_path)
        _log_array_stats("Master Bias 结果", result)

    def generate_master_dark(self, frame_paths, output_path,
                             master_bias_path=None,
                             rejection="sigma_clip", combine="median"):
        """
        生成 Master Dark：可选减 Bias -> sigma-clip -> median 合并 -> 写 FITS（记录 EXPTIME）

        Args:
            frame_paths: list[str]，Dark 帧路径列表
            output_path: str，输出 FITS 路径
            master_bias_path: str|None，Master Bias 路径（可选，用于减偏置）
            rejection: "none" | "sigma_clip"
            combine: "median" | "mean"
        """
        logger.info("=" * 60)
        logger.info("生成 Master Dark: %d 帧 -> %s", len(frame_paths), output_path)

        stack, kw_list = self._load_frames(frame_paths)

        # 可选减 Bias
        if master_bias_path is not None:
            bias = self._load_master_bias(master_bias_path)
            if bias.shape != stack.shape[1:]:
                raise ValueError(
                    f"Master Bias 形状 {bias.shape} 与 Dark 帧 {stack.shape[1:]} 不一致"
                )
            stack = stack - bias[np.newaxis, :, :]  # 广播减法
            logger.info("已减去 Master Bias")

        # 从首帧读取 EXPTIME
        exptime = _find_keyword_float(kw_list[0], "EXPTIME", 0.0)
        logger.info("Dark 曝光时间 EXPTIME=%.3f s", exptime)

        result = combine_frames(stack, rejection=rejection, combine=combine)

        keywords = self._build_keywords("Master Dark", len(frame_paths), exptime=exptime)
        self._writer.write(result, output_path, keywords=keywords, float_sample=True)
        logger.info("Master Dark 写入完成: %s", output_path)
        _log_array_stats("Master Dark 结果", result)

    def generate_master_flat(self, frame_paths, output_path,
                             master_bias_path=None,
                             rejection="sigma_clip", combine="mean"):
        """
        生成 Master Flat：减 Bias -> 逐帧归一化 -> sigma-clip -> mean 合并 -> 再归一化 -> 写 FITS（记录 FILTER）

        Args:
            frame_paths: list[str]，Flat 帧路径列表
            output_path: str，输出 FITS 路径
            master_bias_path: str|None，Master Bias 路径（可选，用于减偏置）
            rejection: "none" | "sigma_clip"
            combine: "median" | "mean"
        """
        logger.info("=" * 60)
        logger.info("生成 Master Flat: %d 帧 -> %s", len(frame_paths), output_path)

        stack, kw_list = self._load_frames(frame_paths)

        # 减 Bias
        if master_bias_path is not None:
            bias = self._load_master_bias(master_bias_path)
            if bias.shape != stack.shape[1:]:
                raise ValueError(
                    f"Master Bias 形状 {bias.shape} 与 Flat 帧 {stack.shape[1:]} 不一致"
                )
            stack = stack - bias[np.newaxis, :, :]
            logger.info("已减去 Master Bias")

        # 逐帧归一化（每帧 median=1.0，最小值裁剪 0.1）
        logger.info("逐帧归一化 Flat 帧...")
        for i in range(stack.shape[0]):
            stack[i] = normalize_flat(stack[i])

        # 从首帧读取 FILTER
        filter_name = _find_keyword(kw_list[0], "FILTER")
        if filter_name is None:
            filter_name = "Unknown"
        logger.info("Flat 滤镜 FILTER=%s", filter_name)

        # sigma-clip + 合并
        result = combine_frames(stack, rejection=rejection, combine=combine)

        # 再归一化合并结果
        logger.info("对合并结果进行最终归一化...")
        result = normalize_flat(result)

        keywords = self._build_keywords("Master Flat", len(frame_paths), filter_name=filter_name)
        self._writer.write(result, output_path, keywords=keywords, float_sample=True)
        logger.info("Master Flat 写入完成: %s", output_path)
        _log_array_stats("Master Flat 结果", result)
