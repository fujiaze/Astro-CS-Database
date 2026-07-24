# -*- coding: utf-8 -*-
"""
Cosmetic Corrector - 坏点修复模块
功能: 检测和修复热像素、冷像素等坏点，使用两种检测方法可组合使用，
      使用连通区域大小过滤排除星点。
用途: 天文图像标准校准流程的坏点修复步骤，可对单帧或批量帧进行校正，
      支持文件模式(correct_frame/correct_frames)与生产模式内存直通(correct_data)。
依赖: numpy, scipy.ndimage(label), scipy.interpolate.griddata,
      astro_image_io (ImageReader / FITSWriter / FITSKeywordPy)
调用: from cosmetic_corrector import CosmeticCorrector
      cc = CosmeticCorrector(max_workers=16)
      cc.correct_frames(frame_paths, output_dir, master_dark="master_dark.fits")
      # 或内存直通:
      corrected, info = cc.correct_data(light_data, dark_data=master_dark_data)
数据流: 输入图像 -> [从Dark检测热像素] + [从Bias检测冷像素]
      -> 合并掩码 -> 连通区域过滤排除星点 -> 中值/双线性插值修复 -> 输出
检测原理:
  - 热像素: 暗场中异常高值（阈值 = median + threshold * 1.4826 * MAD）
  - 冷像素: 偏置中异常低值（阈值 = median - threshold * 1.4826 * MAD）
  - 星点过滤: 连通区域大小 >= max_structure_size 的区域视为星点并排除
"""

from __future__ import annotations

import os
import sys
import ctypes
import logging
from datetime import datetime

import numpy as np
import scipy.ndimage as ndi
from scipy.interpolate import griddata

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
        _LOG_DIR, "cosmetic_corrector_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("cosmetic_corrector")
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


# ============================ C++ DLL 加载（模块级缓存） ============================

_cpp_dll = None
_cpp_dll_loaded = False


def _load_cpp_dll():
    """加载C++坏点修复DLL（模块级缓存）。
    成功返回 ctypes.CDLL 对象，失败返回 None。
    """
    global _cpp_dll, _cpp_dll_loaded
    if _cpp_dll_loaded:
        return _cpp_dll
    _cpp_dll_loaded = True
    try:
        dll_path = os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "cosmetic_corrector.dll",
        )
        if not os.path.isfile(dll_path):
            logger.debug("C++ DLL不存在: %s", dll_path)
            return None
        _cpp_dll = ctypes.CDLL(dll_path)
        # 设置函数签名
        _cpp_dll.cc_correct_median.argtypes = [
            ctypes.POINTER(ctypes.c_float),  # data
            ctypes.POINTER(ctypes.c_uint8),  # bad_mask
            ctypes.c_int,                    # H
            ctypes.c_int,                    # W
            ctypes.c_int,                    # window
        ]
        _cpp_dll.cc_correct_median.restype = ctypes.c_longlong

        _cpp_dll.cc_detect_hot.argtypes = [
            ctypes.POINTER(ctypes.c_float),  # dark_data
            ctypes.c_int,                    # H
            ctypes.c_int,                    # W
            ctypes.c_double,                 # sigma
            ctypes.c_int,                    # max_structure_size
            ctypes.POINTER(ctypes.c_uint8),  # out_mask
        ]
        _cpp_dll.cc_detect_hot.restype = ctypes.c_longlong

        _cpp_dll.cc_detect_cold.argtypes = _cpp_dll.cc_detect_hot.argtypes
        _cpp_dll.cc_detect_cold.restype = ctypes.c_longlong

        _cpp_dll.cc_last_error.argtypes = []
        _cpp_dll.cc_last_error.restype = ctypes.c_char_p

        logger.info("C++ DLL加载成功: %s", dll_path)
        return _cpp_dll
    except Exception as e:
        logger.warning("C++ DLL加载失败: %s", e)
        return None


# ============================ 连通区域过滤（星点保护） ============================

def filter_by_structure_size(mask, max_structure_size=4):
    """
    使用连通区域大小过滤坏像素掩码，排除星点等大结构。

    关键思想：
    - 热像素/冷像素通常是孤立的 1-3 像素
    - 星点是更大的空间结构（几十到几百像素）
    - 只保留小于 max_structure_size 的孤立结构

    Args:
        mask: np.ndarray (H, W) bool，原始坏像素掩码
        max_structure_size: int，最大结构大小（小于此值的结构被保留，默认4）

    Returns:
        np.ndarray (H, W) bool，过滤后的掩码（排除了星点等大结构）
    """
    mask = np.asarray(mask, dtype=bool)
    if not mask.any():
        return mask

    labeled, num_features = ndi.label(mask)
    filtered_mask = np.zeros_like(mask)

    if num_features > 0:
        sizes = np.bincount(labeled.ravel())
        sizes[0] = max_structure_size
        small_labels = np.where(sizes < max_structure_size)[0]
        if len(small_labels) > 0:
            small_mask = np.isin(labeled, small_labels)
            filtered_mask[small_mask] = True

    n_before = int(mask.sum())
    n_after = int(filtered_mask.sum())
    logger.debug(
        "结构大小过滤: 候选 %d -> 保留 %d (剔除大结构 %d, max_structure_size=%d)",
        n_before, n_after, n_before - n_after, max_structure_size,
    )
    return filtered_mask


# ============================ 坏点检测 ============================

def detect_hot_pixels_from_dark(dark_data, threshold=5.0, min_value=None,
                                max_structure_size=4):
    """
    从暗场检测热像素（全局统计法）。
    热像素在暗场中表现为异常高的值（热噪声累积）。
    Dark 主帧本身不做修复，此函数仅检测坏点位置用于后续修复。

    优先调用C++ DLL（OpenMP并行 + BFS连通区域过滤），失败时fallback到Python。

    Args:
        dark_data: np.ndarray (H, W)，暗场数据
        threshold: float，sigma 倍数（默认 5.0）
        min_value: float | None，绝对值下限（可选，C++路径不使用此参数）
        max_structure_size: int，最大结构大小（用于过滤星点）

    Returns:
        np.ndarray (H, W) bool，热像素掩码
    """
    data = np.ascontiguousarray(dark_data, dtype=np.float32)
    h, w = data.shape

    # 优先尝试C++ DLL（min_value参数仅在Python fallback路径生效）
    try:
        cc = _load_cpp_dll()
        if cc is not None:
            out_mask = np.zeros((h, w), dtype=np.uint8)
            n_hot = cc.cc_detect_hot(
                data.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                ctypes.c_int(h), ctypes.c_int(w),
                ctypes.c_double(threshold),
                ctypes.c_int(max_structure_size),
                out_mask.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8)),
            )
            if n_hot >= 0:
                hot_mask = out_mask.astype(bool)
                logger.info(
                    "C++ DLL检测热像素(OpenMP): threshold=%.1fσ, 检测到 %d 个",
                    threshold, int(n_hot),
                )
                return hot_mask
            err = cc.cc_last_error().decode("utf-8") if cc.cc_last_error() else "未知"
            logger.warning("C++ DLL cc_detect_hot 返回错误: %s，fallback到Python", err)
    except Exception as e:
        logger.warning("C++ DLL调用失败，fallback到Python: %s", e)

    # Python fallback (现有逻辑)
    median = float(np.median(data))
    mad = float(np.median(np.abs(data - median)))
    std = 1.4826 * mad if mad > 0 else float(np.std(data))

    hot_threshold = median + threshold * std
    if min_value is not None:
        hot_threshold = max(hot_threshold, min_value)

    logger.info(
        "从Dark检测热像素(全局统计): median=%.4f, MAD=%.4f, std=%.4f, threshold=%.1fσ (%.4f)",
        median, mad, std, threshold, hot_threshold,
    )

    hot_mask = data > hot_threshold
    n_candidates = int(hot_mask.sum())
    hot_mask = filter_by_structure_size(hot_mask, max_structure_size)
    logger.info("热像素检测: 候选 %d -> 过滤后 %d", n_candidates, int(hot_mask.sum()))
    return hot_mask


def detect_cold_pixels_from_bias(bias_data, threshold=5.0, max_structure_size=4):
    """
    从偏置检测冷像素（全局统计法）。
    冷像素在偏置中表现为异常低的值（响应不足）。
    Bias 主帧本身不做修复，此函数仅检测坏点位置用于后续修复。

    优先调用C++ DLL（OpenMP并行 + BFS连通区域过滤），失败时fallback到Python。

    Args:
        bias_data: np.ndarray (H, W)，偏置数据
        threshold: float，sigma 倍数（默认 5.0）
        max_structure_size: int，最大结构大小（用于过滤星点）

    Returns:
        np.ndarray (H, W) bool，冷像素掩码
    """
    data = np.ascontiguousarray(bias_data, dtype=np.float32)
    h, w = data.shape

    # 优先尝试C++ DLL
    try:
        cc = _load_cpp_dll()
        if cc is not None:
            out_mask = np.zeros((h, w), dtype=np.uint8)
            n_cold = cc.cc_detect_cold(
                data.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                ctypes.c_int(h), ctypes.c_int(w),
                ctypes.c_double(threshold),
                ctypes.c_int(max_structure_size),
                out_mask.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8)),
            )
            if n_cold >= 0:
                cold_mask = out_mask.astype(bool)
                logger.info(
                    "C++ DLL检测冷像素(OpenMP): threshold=%.1fσ, 检测到 %d 个",
                    threshold, int(n_cold),
                )
                return cold_mask
            err = cc.cc_last_error().decode("utf-8") if cc.cc_last_error() else "未知"
            logger.warning("C++ DLL cc_detect_cold 返回错误: %s，fallback到Python", err)
    except Exception as e:
        logger.warning("C++ DLL调用失败，fallback到Python: %s", e)

    # Python fallback (现有逻辑)
    median = float(np.median(data))
    mad = float(np.median(np.abs(data - median)))
    std = 1.4826 * mad if mad > 0 else float(np.std(data))

    cold_threshold = median - threshold * std

    logger.info(
        "从Bias检测冷像素(全局统计): median=%.4f, MAD=%.4f, std=%.4f, threshold=%.1fσ (%.4f)",
        median, mad, std, threshold, cold_threshold,
    )

    cold_mask = data < cold_threshold
    n_candidates = int(cold_mask.sum())
    cold_mask = filter_by_structure_size(cold_mask, max_structure_size)
    logger.info("冷像素检测: 候选 %d -> 过滤后 %d", n_candidates, int(cold_mask.sum()))
    return cold_mask


# ============================ 插值修复 ============================

def interpolate_bad_pixels(data, bad_mask, method="median"):
    """
    插值修复坏像素。优先调用C++ DLL（OpenMP并行），失败时fallback到Python scipy。

    Args:
        data: np.ndarray (H, W) float，原始图像数据
        bad_mask: np.ndarray (H, W) bool，坏像素掩码
        method: "median" 用5×5中值滤波结果替换坏像素；
               "bilinear" 用 scipy.interpolate.griddata 对坏像素做双线性插值

    Returns:
        np.ndarray (H, W) float32，修复后的图像（不修改输入）
    """
    data = np.ascontiguousarray(data, dtype=np.float32)
    bad_mask = np.asarray(bad_mask, dtype=bool)
    result = data.copy()

    n_bad = int(bad_mask.sum())
    if n_bad == 0:
        logger.info("无坏像素需要修复")
        return result

    # 优先尝试C++ DLL（仅 median 方法支持）
    if method == "median":
        try:
            cc = _load_cpp_dll()
            if cc is not None:
                mask_u8 = np.ascontiguousarray(bad_mask, dtype=np.uint8)
                h, w = result.shape
                n_fixed = cc.cc_correct_median(
                    result.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                    mask_u8.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8)),
                    ctypes.c_int(h), ctypes.c_int(w), ctypes.c_int(5),
                )
                if n_fixed >= 0:
                    logger.info("C++ DLL修复 %d 个坏像素 (OpenMP并行)", int(n_fixed))
                    return result
                err = cc.cc_last_error().decode("utf-8") if cc.cc_last_error() else "未知"
                logger.warning("C++ DLL cc_correct_median 返回错误: %s，fallback到Python", err)
        except Exception as e:
            logger.warning("C++ DLL调用失败，fallback到Python: %s", e)

    # Python fallback (现有逻辑)
    if method == "median":
        logger.info("使用5×5中值滤波修复 %d 个坏像素", n_bad)
        med = ndi.median_filter(data, size=5, mode="reflect")
        result[bad_mask] = med[bad_mask]
    elif method == "bilinear":
        logger.info("使用双线性插值(griddata)修复 %d 个坏像素", n_bad)
        h, w = data.shape
        good = ~bad_mask
        ys, xs = np.mgrid[0:h, 0:w]
        points = np.column_stack([ys[good], xs[good]]).astype(np.float64)
        values = data[good].astype(np.float64)
        interp_points = np.column_stack([ys[bad_mask], xs[bad_mask]]).astype(np.float64)
        interp_values = griddata(points, values, interp_points, method="linear")
        n_nan = int(np.isnan(interp_values).sum())
        if n_nan > 0:
            logger.warning("griddata 产生 %d 个 NaN（边缘像素），用最近邻回填", n_nan)
            nn_values = griddata(points, values, interp_points, method="nearest")
            nan_mask = np.isnan(interp_values)
            interp_values[nan_mask] = nn_values[nan_mask]
        result[bad_mask] = interp_values.astype(np.float32)
    else:
        raise ValueError(f"不支持的插值方法: {method}（可选: median / bilinear）")

    return result


# ============================ 统一入口函数 ============================

def correct_frame(input_path, output_path,
                  hot_sigma=5.0, cold_sigma=5.0,
                  method="median", max_structure_size=4,
                  master_dark=None, master_bias=None,
                  reader=None, writer=None):
    """
    校正单帧图像的坏点（文件模式）。
    在校准后的 Light 图像上检测坏点并修复。
    检测方法：Dark检测热像素 + Bias检测冷像素。
    不检测 cosmic ray（留给叠加时 3sigma 处理）。

    流程:
        1. 读取校准后图像
        2. 从 Master Dark 检测热像素位置（全局统计 + 结构过滤）
        3. 从 Master Bias 检测冷像素位置（全局统计 + 结构过滤）
        4. 掩码 OR 合并
        5. 插值修复
        6. 写 FITS（头记录 CCHOT, CCCOLD, CCMETHOD, CCMAXSZ）

    Args:
        input_path: str，输入图像路径（FITS/XISF，应为校准后的 Light）
        output_path: str，输出 FITS 路径
        hot_sigma: float，Dark 热像素检测 sigma 倍数（默认 5.0，<=0 跳过）
        cold_sigma: float，Bias 冷像素检测 sigma 倍数（默认 5.0，<=0 跳过）
        method: "median" | "bilinear"，插值方法
        max_structure_size: int，最大结构大小（默认4，>=此值的结构视为星点）
        master_dark: str | None，Master Dark 路径（用于检测热像素位置）
        master_bias: str | None，Master Bias 路径（用于检测冷像素位置）
        reader: ImageReader|None，可选外部 reader（复用）
        writer: FITSWriter|None，可选外部 writer（复用）

    Returns:
        dict: success, hot_pixels, cold_pixels, total_bad, original_mean,
              corrected_mean, output_path, method
    """
    logger.info("=" * 60)
    logger.info("坏点校正（文件模式）: %s -> %s", input_path, output_path)

    own_reader = reader is None
    own_writer = writer is None
    if own_reader:
        reader = ImageReader()
    if own_writer:
        writer = FITSWriter()

    try:
        # 1. 读取校准后图像
        img = reader.read(input_path)
        data = img.data.astype(np.float32, copy=True)
        keywords = img.keywords
        img.close()
        original_mean = float(np.mean(data))
        logger.info("图像加载: shape=%s, mean=%.4f", str(data.shape), original_mean)

        # 2. 初始化掩码
        hot_mask = np.zeros_like(data, dtype=bool)
        cold_mask = np.zeros_like(data, dtype=bool)

        # 3. 从 Master Dark 检测热像素位置
        dark_data = None
        if master_dark:
            dimg = reader.read(master_dark)
            dark_data = dimg.data.astype(np.float32, copy=True)
            dimg.close()
            logger.info("Master Dark 加载: shape=%s", str(dark_data.shape))

        if dark_data is not None and hot_sigma > 0:
            hot_mask = detect_hot_pixels_from_dark(
                dark_data, threshold=hot_sigma, max_structure_size=max_structure_size,
            )

        # 4. 从 Master Bias 检测冷像素位置
        bias_data = None
        if master_bias:
            bimg = reader.read(master_bias)
            bias_data = bimg.data.astype(np.float32, copy=True)
            bimg.close()
            logger.info("Master Bias 加载: shape=%s", str(bias_data.shape))

        if bias_data is not None and cold_sigma > 0:
            cold_mask = detect_cold_pixels_from_bias(
                bias_data, threshold=cold_sigma, max_structure_size=max_structure_size,
            )

        logger.info("坏点检测策略: Dark热像素 + Bias冷像素")

        # 6. 合并掩码
        n_hot = int(hot_mask.sum())
        n_cold = int(cold_mask.sum())
        all_bad = hot_mask | cold_mask
        n_total = int(all_bad.sum())
        logger.info(
            "合并掩码: 热坏点=%d, 冷坏点=%d, 总坏像素=%d",
            n_hot, n_cold, n_total,
        )

        # 7. 插值修复
        corrected = interpolate_bad_pixels(data, all_bad, method=method)
        corrected_mean = float(np.mean(corrected))
        logger.info(
            "修复完成: 原始mean=%.4f -> 修复后mean=%.4f, 修复像素=%d",
            original_mean, corrected_mean, n_total,
        )

        # 8. 写 FITS
        out_keywords = [
            kw for kw in (keywords or [])
            if kw.name.upper() not in ("BZERO", "BSCALE")
        ]
        out_keywords.append(FITSKeywordPy(name="CCHOT", value=str(n_hot), comment="Hot pixels detected"))
        out_keywords.append(FITSKeywordPy(name="CCCOLD", value=str(n_cold), comment="Cold pixels detected"))
        out_keywords.append(FITSKeywordPy(name="CCMETHOD", value=method, comment="Interpolation method"))
        out_keywords.append(FITSKeywordPy(name="CCMAXSZ", value=str(max_structure_size), comment="Max structure size"))
        writer.write(corrected, output_path, keywords=out_keywords, float_sample=True)
        logger.info("写入完成: %s", output_path)

        return {
            "success": True,
            "hot_pixels": n_hot,
            "cold_pixels": n_cold,
            "total_bad": n_total,
            "original_mean": original_mean,
            "corrected_mean": corrected_mean,
            "output_path": output_path,
            "method": method,
        }
    except Exception as e:
        logger.error("校正失败: %s", e, exc_info=True)
        return {
            "success": False,
            "error": str(e),
            "input_path": input_path,
            "output_path": output_path,
        }


# ============================ CosmeticCorrector 类 ============================

class CosmeticCorrector:
    """坏点修复器：支持批量文件校正与内存直通校正"""

    def __init__(self, max_workers=4):
        self._max_workers = max_workers
        self._reader = ImageReader()
        self._writer = FITSWriter()
        logger.info("CosmeticCorrector 初始化: max_workers=%d", max_workers)

    def correct_frames(self, frame_paths, output_dir,
                       hot_sigma=5.0, cold_sigma=5.0,
                       method="median", max_structure_size=4,
                       master_dark=None, master_bias=None):
        """
        批量校正多帧图像（文件模式）。
        两种检测方法可组合：Dark检测热像素 + Bias检测冷像素。

        Args:
            frame_paths: list[str]，输入图像路径列表
            output_dir: str，输出目录
            hot_sigma: float，热像素检测 sigma 倍数
            cold_sigma: float，冷像素检测 sigma 倍数
            method: "median" | "bilinear"
            max_structure_size: int，最大结构大小（默认4）
            master_dark: str | None，Master Dark 路径
            master_bias: str | None，Master Bias 路径

        Returns:
            list[dict]，每帧的校正结果
        """
        logger.info("=" * 60)
        logger.info("批量校正: %d 帧 -> %s", len(frame_paths), output_dir)
        os.makedirs(output_dir, exist_ok=True)

        results = []
        for i, path in enumerate(frame_paths):
            base = os.path.splitext(os.path.basename(path))[0]
            out_path = os.path.join(output_dir, base + "_cc.fits")
            logger.info("处理帧 %d/%d: %s", i + 1, len(frame_paths), path)
            res = correct_frame(
                path, out_path,
                hot_sigma=hot_sigma, cold_sigma=cold_sigma,
                method=method, max_structure_size=max_structure_size,
                master_dark=master_dark, master_bias=master_bias,
                reader=self._reader, writer=self._writer,
            )
            results.append(res)

        n_success = sum(1 for r in results if r.get("success"))
        n_total_bad = sum(r.get("total_bad", 0) for r in results if r.get("success"))
        logger.info(
            "批量校正完成: 成功 %d/%d, 总修复像素 %d",
            n_success, len(results), n_total_bad,
        )
        return results

    def correct_data(self, data,
                     hot_sigma=5.0, cold_sigma=5.0,
                     method="median", max_structure_size=4,
                     dark_data=None, bias_data=None):
        """
        直接在内存中校正图像数据（生产模式用）。
        检测方法：Dark检测热像素 + Bias检测冷像素。

        Args:
            data: np.ndarray (H, W)，校准后 Light 图像数据
            hot_sigma: float，Dark 热像素检测 sigma 倍数
            cold_sigma: float，Bias 冷像素检测 sigma 倍数
            method: "median" | "bilinear"，插值方法
            max_structure_size: int，最大结构大小（默认4）
            dark_data: np.ndarray (H, W) | None，Master Dark 数据
            bias_data: np.ndarray (H, W) | None，Master Bias 数据

            Returns:
            (corrected_data, info_dict):
              corrected_data: np.ndarray (H, W) float32
              info_dict: success, hot_pixels, cold_pixels, total_bad,
                         original_mean, corrected_mean, method
        """
        logger.info("=" * 60)
        logger.info("坏点校正（内存直通模式）: shape=%s", str(data.shape))

        try:
            data = np.asarray(data, dtype=np.float32)
            original_mean = float(np.mean(data))

            # 初始化掩码
            hot_mask = np.zeros_like(data, dtype=bool)
            cold_mask = np.zeros_like(data, dtype=bool)

            # 从 Master Dark 检测热像素位置
            if dark_data is not None and hot_sigma > 0:
                hot_mask = detect_hot_pixels_from_dark(
                    dark_data, threshold=hot_sigma, max_structure_size=max_structure_size,
                )

            # 从 Master Bias 检测冷像素位置
            if bias_data is not None and cold_sigma > 0:
                cold_mask = detect_cold_pixels_from_bias(
                    bias_data, threshold=cold_sigma, max_structure_size=max_structure_size,
                )

            n_hot = int(hot_mask.sum())
            n_cold = int(cold_mask.sum())
            all_bad = hot_mask | cold_mask
            n_total = int(all_bad.sum())
            logger.info(
                "合并掩码: 热坏点=%d, 冷坏点=%d, 总坏像素=%d",
                n_hot, n_cold, n_total,
            )

            corrected = interpolate_bad_pixels(data, all_bad, method=method)
            corrected_mean = float(np.mean(corrected))
            logger.info(
                "修复完成: 原始mean=%.4f -> 修复后mean=%.4f, 修复像素=%d",
                original_mean, corrected_mean, n_total,
            )

            info = {
                "success": True,
                "hot_pixels": n_hot,
                "cold_pixels": n_cold,
                "total_bad": n_total,
                "original_mean": original_mean,
                "corrected_mean": corrected_mean,
                "method": method,
            }
            return corrected, info
        except Exception as e:
            logger.error("内存校正失败: %s", e, exc_info=True)
            info = {"success": False, "error": str(e)}
            return np.asarray(data, dtype=np.float32), info
