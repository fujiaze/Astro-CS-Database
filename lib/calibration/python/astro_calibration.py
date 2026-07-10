# -*- coding: utf-8 -*-
"""
Astro Calibration - C++ DLL 的 Python ctypes 封装
功能: 天文CCD图像标准校准（主帧生成 + 图像校准 + 坏点修复），C++底层OpenMP多线程加速
用途: 通过ctypes调用astro_calibration.dll，提供与Python版等价的接口，
      适用于大批量图像的高性能校准处理
依赖: numpy, ctypes, astro_image_io (ImageReader / FITSWriter)
调用: from astro_calibration import AstroCalibration
      cal = AstroCalibration()
      # 生成主帧
      cal.generate_master_dark(dark_paths, "master_dark.fits")
      # 校准单帧
      cal.calibrate_frame("light.fits", "calibrated.fits", master_dark="master_dark.fits", master_flat="master_flat.fits")
      # 坏点修复
      cal.correct_frame("calibrated.fits", "final.fits", master_dark="master_dark.fits", master_bias="master_bias.fits")
校准公式:
      无暗场优化: Calibrated = (Light - Dark) / Flat  （Dark 已含 Bias）
      有暗场优化: Calibrated = (Light - Bias - K*(Dark - Bias)) / Flat
"""

from __future__ import annotations

import os
import sys
import ctypes
import logging
from datetime import datetime

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

_LOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "logs")
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR, "astro_calibration_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)
    lg = logging.getLogger("astro_calibration")
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


# ============================ DLL 加载 ============================

_DLL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_DLL_PATH = os.path.join(_DLL_DIR, "astro_calibration.dll")

try:
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(_DLL_DIR)
    _dll = ctypes.CDLL(_DLL_PATH)
    logger.info("DLL 加载成功: %s", _DLL_PATH)
except OSError as e:
    logger.error("DLL 加载失败: %s (%s)", _DLL_PATH, e)
    raise


# ============================ 常量 ============================

COMBINE_MEAN = 0
COMBINE_MEDIAN = 1

METHOD_MEDIAN = 0
METHOD_BILINEAR = 1

AC_OK = 0
AC_ERR_PARAM = -1
AC_ERR_MEMORY = -2
AC_ERR_INTERNAL = -3


# ============================ 函数签名绑定 ============================

_dll.ac_generate_master_bias.argtypes = [
    ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_int, ctypes.c_int,
    ctypes.POINTER(ctypes.c_float),
    ctypes.c_float, ctypes.c_float, ctypes.c_int, ctypes.c_int,
]
_dll.ac_generate_master_bias.restype = ctypes.c_int

_dll.ac_generate_master_dark.argtypes = _dll.ac_generate_master_bias.argtypes
_dll.ac_generate_master_dark.restype = ctypes.c_int

_dll.ac_generate_master_flat.argtypes = [
    ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_int, ctypes.c_int,
    ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_float),
    ctypes.c_float, ctypes.c_float, ctypes.c_int,
]
_dll.ac_generate_master_flat.restype = ctypes.c_int

_dll.ac_calibrate_frame.argtypes = [
    ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_int,
    ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_float),
    ctypes.c_int, ctypes.c_float,
    ctypes.POINTER(ctypes.c_float),
]
_dll.ac_calibrate_frame.restype = ctypes.c_int

_dll.ac_correct_frame.argtypes = [
    ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_int,
    ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_float),
    ctypes.c_float, ctypes.c_float,
    ctypes.c_int, ctypes.c_int,
    ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int),
]
_dll.ac_correct_frame.restype = ctypes.c_int

_dll.ac_set_num_threads.argtypes = [ctypes.c_int]
_dll.ac_set_num_threads.restype = None

_dll.ac_version.argtypes = []
_dll.ac_version.restype = ctypes.c_char_p


# ============================ 工具函数 ============================

def _to_float_ptr(arr: np.ndarray) -> ctypes.POINTER(ctypes.c_float):
    """将 numpy float32 数组转为 ctypes float 指针"""
    if arr.dtype != np.float32:
        arr = arr.astype(np.float32)
    if not arr.flags["C_CONTIGUOUS"]:
        arr = np.ascontiguousarray(arr)
    return arr.ctypes.data_as(ctypes.POINTER(ctypes.c_float))


def _to_int_ptr(val) -> ctypes.POINTER(ctypes.c_int):
    """创建 int 指针用于输出参数"""
    v = ctypes.c_int(val)
    return ctypes.pointer(v)


# ============================ 主类 ============================

class AstroCalibration:
    """C++ 天文校准模块的 Python 封装"""

    def __init__(self, max_workers: int = 16):
        self._max_workers = max_workers
        _dll.ac_set_num_threads(max_workers)
        self._reader = ImageReader()
        self._writer = FITSWriter()
        version = _dll.ac_version().decode("utf-8")
        logger.info("AstroCalibration 初始化: %s, 线程数=%d", version, max_workers)

    # -------------------- 主帧生成 --------------------

    def generate_master_bias(self, bias_paths: list, output_path: str,
                             sigma: float = 3.0, combine: int = COMBINE_MEDIAN) -> dict:
        """
        生成 Master Bias：sigma-clip + median/mean 合并
        Dark/Bias 主帧保留坏点（校准时扣除），不做修复
        """
        logger.info("=" * 60)
        logger.info("生成 Master Bias: %d 帧 -> %s", len(bias_paths), output_path)

        stack, w, h = self._load_stack(bias_paths)
        out = np.zeros((h, w), dtype=np.float32)

        ret = _dll.ac_generate_master_bias(
            _to_float_ptr(stack), len(bias_paths), w, h,
            _to_float_ptr(out),
            sigma, sigma, 5, combine,
        )

        if ret != AC_OK:
            logger.error("ac_generate_master_bias 失败: %d", ret)
            return {"success": False, "error": f"DLL错误: {ret}"}

        self._write_fits(out, output_path, "MASTERBIAS")
        mean_val = float(np.mean(out))
        logger.info("Master Bias 完成: mean=%.4f, shape=(%d,%d)", mean_val, h, w)
        return {"success": True, "output_path": output_path, "mean": mean_val, "shape": (h, w)}

    def generate_master_dark(self, dark_paths: list, output_path: str,
                             sigma: float = 3.0, combine: int = COMBINE_MEDIAN) -> dict:
        """
        生成 Master Dark：sigma-clip + median/mean 合并
        Dark 已含 Bias，不减 Bias，保留坏点
        """
        logger.info("=" * 60)
        logger.info("生成 Master Dark: %d 帧 -> %s", len(dark_paths), output_path)

        stack, w, h = self._load_stack(dark_paths)
        out = np.zeros((h, w), dtype=np.float32)

        ret = _dll.ac_generate_master_dark(
            _to_float_ptr(stack), len(dark_paths), w, h,
            _to_float_ptr(out),
            sigma, sigma, 5, combine,
        )

        if ret != AC_OK:
            logger.error("ac_generate_master_dark 失败: %d", ret)
            return {"success": False, "error": f"DLL错误: {ret}"}

        self._write_fits(out, output_path, "MASTERDARK")
        mean_val = float(np.mean(out))
        logger.info("Master Dark 完成: mean=%.4f, shape=(%d,%d)", mean_val, h, w)
        return {"success": True, "output_path": output_path, "mean": mean_val, "shape": (h, w)}

    def generate_master_flat(self, flat_paths: list, output_path: str,
                             master_bias_path: str | None = None,
                             sigma: float = 3.0) -> dict:
        """
        生成 Master Flat：减Bias -> 逐帧归一化 -> sigma-clip+mean -> 再归一化
        """
        logger.info("=" * 60)
        logger.info("生成 Master Flat: %d 帧 -> %s", len(flat_paths), output_path)

        stack, w, h = self._load_stack(flat_paths)
        out = np.zeros((h, w), dtype=np.float32)

        bias_ptr = None
        bias_data = None
        if master_bias_path:
            bimg = self._reader.read(master_bias_path)
            bias_data = bimg.data.astype(np.float32)
            bimg.close()
            bias_ptr = _to_float_ptr(bias_data)
            logger.info("Master Bias 加载: shape=%s", str(bias_data.shape))

        ret = _dll.ac_generate_master_flat(
            _to_float_ptr(stack), len(flat_paths), w, h,
            bias_ptr,
            _to_float_ptr(out),
            sigma, sigma, 5,
        )

        if ret != AC_OK:
            logger.error("ac_generate_master_flat 失败: %d", ret)
            return {"success": False, "error": f"DLL错误: {ret}"}

        self._write_fits(out, output_path, "MASTERFLAT")
        median_val = float(np.median(out))
        logger.info("Master Flat 完成: median=%.4f, shape=(%d,%d)", median_val, h, w)
        return {"success": True, "output_path": output_path, "median": median_val, "shape": (h, w)}

    # -------------------- 图像校准 --------------------

    def calibrate_frame(self, light_path: str, output_path: str,
                        master_dark: str | None = None,
                        master_flat: str | None = None,
                        master_bias: str | None = None,
                        dark_optimization: bool = False,
                        dark_scale_factor: float = 1.0) -> dict:
        """
        校准单帧 Light
        无暗场优化: (Light - Dark) / Flat
        有暗场优化: (Light - Bias - K*(Dark - Bias)) / Flat
        """
        logger.info("=" * 60)
        logger.info("校准: %s -> %s", light_path, output_path)

        light_img = self._reader.read(light_path)
        light_data = light_img.data.astype(np.float32)
        w, h = light_data.shape[1], light_data.shape[0]
        light_img.close()
        logger.info("Light 加载: shape=(%d,%d), mean=%.4f", h, w, float(np.mean(light_data)))

        dark_data = self._load_optional(master_dark, "Dark")
        flat_data = self._load_optional(master_flat, "Flat")
        bias_data = self._load_optional(master_bias, "Bias")

        out = np.zeros((h, w), dtype=np.float32)
        actual_k = ctypes.c_float(dark_scale_factor)

        ret = _dll.ac_calibrate_frame(
            _to_float_ptr(light_data), w, h,
            _to_float_ptr(dark_data) if dark_data is not None else None,
            _to_float_ptr(flat_data) if flat_data is not None else None,
            _to_float_ptr(bias_data) if bias_data is not None else None,
            _to_float_ptr(out),
            1 if dark_optimization else 0, dark_scale_factor,
            ctypes.pointer(actual_k),
        )

        if ret != AC_OK:
            logger.error("ac_calibrate_frame 失败: %d", ret)
            return {"success": False, "error": f"DLL错误: {ret}"}

        mean_val = float(np.mean(out))
        k_val = float(actual_k.value)
        logger.info("校准完成: mean=%.4f, K=%.4f", mean_val, k_val)

        self._write_fits(out, output_path, "CALIBRATED")
        return {"success": True, "output_path": output_path, "mean": mean_val, "k": k_val}

    # -------------------- 坏点修复 --------------------

    def correct_frame(self, input_path: str, output_path: str,
                      master_dark: str | None = None,
                      master_bias: str | None = None,
                      hot_sigma: float = 5.0, cold_sigma: float = 5.0,
                      method: int = METHOD_MEDIAN,
                      max_structure_size: int = 4) -> dict:
        """
        坏点修复：Dark检测热像素 + Bias检测冷像素 + 连通区域过滤 + 插值修复
        """
        logger.info("=" * 60)
        logger.info("坏点修复: %s -> %s", input_path, output_path)

        img = self._reader.read(input_path)
        data = img.data.astype(np.float32)
        w, h = data.shape[1], data.shape[0]
        keywords = img.keywords
        img.close()
        original_mean = float(np.mean(data))
        logger.info("图像加载: shape=(%d,%d), mean=%.4f", h, w, original_mean)

        dark_data = self._load_optional(master_dark, "Dark")
        bias_data = self._load_optional(master_bias, "Bias")

        out = np.zeros((h, w), dtype=np.float32)
        n_hot = ctypes.c_int(0)
        n_cold = ctypes.c_int(0)

        ret = _dll.ac_correct_frame(
            _to_float_ptr(data), w, h,
            _to_float_ptr(dark_data) if dark_data is not None else None,
            _to_float_ptr(bias_data) if bias_data is not None else None,
            _to_float_ptr(out),
            hot_sigma, cold_sigma,
            method, max_structure_size,
            ctypes.pointer(n_hot), ctypes.pointer(n_cold),
        )

        if ret != AC_OK:
            logger.error("ac_correct_frame 失败: %d", ret)
            return {"success": False, "error": f"DLL错误: {ret}"}

        corrected_mean = float(np.mean(out))
        logger.info(
            "修复完成: 热像素=%d, 冷像素=%d, mean=%.4f->%.4f",
            n_hot.value, n_cold.value, original_mean, corrected_mean,
        )

        out_keywords = [
            kw for kw in (keywords or [])
            if kw.name.upper() not in ("BZERO", "BSCALE")
        ]
        out_keywords.append(FITSKeywordPy(name="CCHOT", value=str(n_hot.value), comment="Hot pixels"))
        out_keywords.append(FITSKeywordPy(name="CCCOLD", value=str(n_cold.value), comment="Cold pixels"))
        self._writer.write(out, output_path, keywords=out_keywords, float_sample=True)

        return {
            "success": True, "output_path": output_path,
            "hot_pixels": n_hot.value, "cold_pixels": n_cold.value,
            "original_mean": original_mean, "corrected_mean": corrected_mean,
        }

    # -------------------- 全链路（内存直通） --------------------

    def calibrate_and_correct(self, light_path: str, output_path: str,
                              master_dark: str | None = None,
                              master_flat: str | None = None,
                              master_bias: str | None = None,
                              dark_optimization: bool = False,
                              dark_scale_factor: float = 1.0,
                              hot_sigma: float = 5.0, cold_sigma: float = 5.0,
                              method: int = METHOD_MEDIAN,
                              max_structure_size: int = 4) -> dict:
        """
        全链路处理：校准 + 坏点修复（内存直通，只写一次FITS）
        无暗场优化: (Light - Dark) / Flat -> 坏点修复 -> 输出
        有暗场优化: (Light - Bias - K*(Dark - Bias)) / Flat -> 坏点修复 -> 输出
        """
        logger.info("=" * 60)
        logger.info("全链路: %s -> %s", light_path, output_path)

        light_img = self._reader.read(light_path)
        light_data = light_img.data.astype(np.float32)
        w, h = light_data.shape[1], light_data.shape[0]
        keywords = light_img.keywords
        light_img.close()
        original_mean = float(np.mean(light_data))
        logger.info("Light 加载: shape=(%d,%d), mean=%.4f", h, w, original_mean)

        dark_data = self._load_optional(master_dark, "Dark")
        flat_data = self._load_optional(master_flat, "Flat")
        bias_data = self._load_optional(master_bias, "Bias")

        # 阶段1: 校准
        calibrated = np.zeros((h, w), dtype=np.float32)
        actual_k = ctypes.c_float(dark_scale_factor)

        ret = _dll.ac_calibrate_frame(
            _to_float_ptr(light_data), w, h,
            _to_float_ptr(dark_data) if dark_data is not None else None,
            _to_float_ptr(flat_data) if flat_data is not None else None,
            _to_float_ptr(bias_data) if bias_data is not None else None,
            _to_float_ptr(calibrated),
            1 if dark_optimization else 0, dark_scale_factor,
            ctypes.pointer(actual_k),
        )
        if ret != AC_OK:
            logger.error("校准失败: %d", ret)
            return {"success": False, "error": f"校准DLL错误: {ret}"}

        cal_mean = float(np.mean(calibrated))
        k_val = float(actual_k.value)
        logger.info("校准完成: mean=%.4f, K=%.4f", cal_mean, k_val)

        # 阶段2: 坏点修复
        corrected = np.zeros((h, w), dtype=np.float32)
        n_hot = ctypes.c_int(0)
        n_cold = ctypes.c_int(0)

        ret = _dll.ac_correct_frame(
            _to_float_ptr(calibrated), w, h,
            _to_float_ptr(dark_data) if dark_data is not None else None,
            _to_float_ptr(bias_data) if bias_data is not None else None,
            _to_float_ptr(corrected),
            hot_sigma, cold_sigma,
            method, max_structure_size,
            ctypes.pointer(n_hot), ctypes.pointer(n_cold),
        )
        if ret != AC_OK:
            logger.error("坏点修复失败: %d", ret)
            return {"success": False, "error": f"坏点修复DLL错误: {ret}"}

        final_mean = float(np.mean(corrected))
        logger.info(
            "坏点修复完成: 热像素=%d, 冷像素=%d, mean=%.4f->%.4f->%.4f",
            n_hot.value, n_cold.value, original_mean, cal_mean, final_mean,
        )

        out_keywords = [
            kw for kw in (keywords or [])
            if kw.name.upper() not in ("BZERO", "BSCALE")
        ]
        out_keywords.append(FITSKeywordPy(name="CCHOT", value=str(n_hot.value), comment="Hot pixels"))
        out_keywords.append(FITSKeywordPy(name="CCCOLD", value=str(n_cold.value), comment="Cold pixels"))
        out_keywords.append(FITSKeywordPy(name="ACVER", value="1.0.0", comment="Astro Calibration version"))
        self._writer.write(corrected, output_path, keywords=out_keywords, float_sample=True)
        logger.info("写入完成: %s", output_path)

        return {
            "success": True, "output_path": output_path,
            "original_mean": original_mean,
            "calibrated_mean": cal_mean,
            "corrected_mean": final_mean,
            "k": k_val,
            "hot_pixels": n_hot.value, "cold_pixels": n_cold.value,
        }

    def calibrate_and_correct_mem(self, light_data, w, h, keywords,
                                  dark_data=None, flat_data=None, bias_data=None,
                                  dark_optimization=False, dark_scale_factor=1.0,
                                  hot_sigma=5.0, cold_sigma=5.0,
                                  method=METHOD_MEDIAN, max_structure_size=4):
        """
        全链路内存直通：接受numpy数组，不读文件，用于批处理缓存主帧
        """
        light_data = np.ascontiguousarray(light_data, dtype=np.float32)
        original_mean = float(np.mean(light_data))

        calibrated = np.zeros((h, w), dtype=np.float32)
        actual_k = ctypes.c_float(dark_scale_factor)

        ret = _dll.ac_calibrate_frame(
            _to_float_ptr(light_data), w, h,
            _to_float_ptr(dark_data) if dark_data is not None else None,
            _to_float_ptr(flat_data) if flat_data is not None else None,
            _to_float_ptr(bias_data) if bias_data is not None else None,
            _to_float_ptr(calibrated),
            1 if dark_optimization else 0, dark_scale_factor,
            ctypes.pointer(actual_k),
        )
        if ret != AC_OK:
            return {"success": False, "error": f"校准DLL错误: {ret}"}

        cal_mean = float(np.mean(calibrated))

        corrected = np.zeros((h, w), dtype=np.float32)
        n_hot = ctypes.c_int(0)
        n_cold = ctypes.c_int(0)

        ret = _dll.ac_correct_frame(
            _to_float_ptr(calibrated), w, h,
            _to_float_ptr(dark_data) if dark_data is not None else None,
            _to_float_ptr(bias_data) if bias_data is not None else None,
            _to_float_ptr(corrected),
            hot_sigma, cold_sigma, method, max_structure_size,
            ctypes.pointer(n_hot), ctypes.pointer(n_cold),
        )
        if ret != AC_OK:
            return {"success": False, "error": f"坏点修复DLL错误: {ret}"}

        final_mean = float(np.mean(corrected))
        logger.info(
            "mean=%.1f->%.1f->%.1f  hot=%d  cold=%d",
            original_mean, cal_mean, final_mean, n_hot.value, n_cold.value,
        )

        return {
            "success": True,
            "data": corrected,
            "keywords": keywords,
            "original_mean": original_mean,
            "calibrated_mean": cal_mean,
            "corrected_mean": final_mean,
            "k": float(actual_k.value),
            "hot_pixels": n_hot.value, "cold_pixels": n_cold.value,
        }

    def write_fits(self, data, output_path, keywords=None):
        """写FITS文件（供批处理调用）"""
        out_keywords = [
            kw for kw in (keywords or [])
            if kw.name.upper() not in ("BZERO", "BSCALE")
        ]
        self._writer.write(data, output_path, keywords=out_keywords, float_sample=True)

    def read_image(self, path):
        """读取图像，返回(data, w, h, keywords)"""
        img = self._reader.read(path)
        data = np.ascontiguousarray(img.data.astype(np.float32))
        w, h = data.shape[1], data.shape[0]
        keywords = img.keywords
        img.close()
        return data, w, h, keywords

    # -------------------- 内部工具 --------------------

    def _load_stack(self, paths: list) -> tuple:
        """加载多帧图像为 (n, h, w) float32 栈"""
        images = []
        for p in paths:
            img = self._reader.read(p)
            images.append(img.data.astype(np.float32))
            img.close()
        stack = np.stack(images)
        h, w = stack.shape[1], stack.shape[2]
        logger.info("帧栈加载: %d 帧, shape=(%d,%d,%d)", len(paths), len(paths), h, w)
        return stack, w, h

    def _load_optional(self, path: str | None, name: str) -> np.ndarray | None:
        """加载可选的主帧文件"""
        if path is None:
            return None
        img = self._reader.read(path)
        data = img.data.astype(np.float32)
        img.close()
        logger.info("Master %s 加载: shape=%s", name, str(data.shape))
        return data

    def _write_fits(self, data: np.ndarray, path: str, label: str = ""):
        """写 FITS 文件"""
        self._writer.write(data, path, float_sample=True)
        logger.info("写入 FITS: %s (%s)", path, label)
