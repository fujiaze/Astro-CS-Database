"""
Star Detector - 天文图像星点检测器 Python封装
功能: 检测天文图像中的星点坐标，支持PSF拟合过滤、FWHM剪裁、圆度过滤
用途: 输入16bit FITS图像，输出通过所有过滤的星点坐标列表
依赖: dynamic_psf (https://github.com/fujiaze/Dynamic-PSF)
"""

from __future__ import annotations

import os
from ctypes import (
    Structure, c_int, c_float, c_double, c_void_p, c_uint16,
    POINTER, byref, cdll,
)
from dataclasses import dataclass
from typing import Optional

import numpy as np


@dataclass
class SDetParamsPy:
    structureLayers: int = 5
    hotPixelFilterRadius: int = 1
    iterativeClipSigma: float = 9.0
    iterativeMaxRounds: int = 5
    medianFilterDetail: int = 1
    maxStars: int = 0
    fitRadius: int = 8
    fwhmClipSigma: float = 3.0
    maxAxisRatio: float = 2.0


class _CSDetParams(Structure):
    _fields_ = [
        ("structureLayers", c_int),
        ("hotPixelFilterRadius", c_int),
        ("iterativeClipSigma", c_float),
        ("iterativeMaxRounds", c_int),
        ("medianFilterDetail", c_int),
        ("maxStars", c_int),
        ("fitRadius", c_int),
        ("fwhmClipSigma", c_float),
        ("maxAxisRatio", c_float),
    ]


def _params_py_to_c(p: SDetParamsPy) -> _CSDetParams:
    return _CSDetParams(
        structureLayers=p.structureLayers,
        hotPixelFilterRadius=p.hotPixelFilterRadius,
        iterativeClipSigma=p.iterativeClipSigma,
        iterativeMaxRounds=p.iterativeMaxRounds,
        medianFilterDetail=p.medianFilterDetail,
        maxStars=p.maxStars,
        fitRadius=p.fitRadius,
        fwhmClipSigma=p.fwhmClipSigma,
        maxAxisRatio=p.maxAxisRatio,
    )


def _load_dll(dll_path: str):
    mingw_bin = r"C:\msys64\mingw64\bin"
    if os.path.isdir(mingw_bin):
        os.environ["PATH"] = mingw_bin + ";" + os.environ.get("PATH", "")
        try:
            os.add_dll_directory(mingw_bin)
        except OSError:
            pass
    dll_dir = os.path.dirname(os.path.abspath(dll_path))
    try:
        os.add_dll_directory(dll_dir)
    except OSError:
        pass
    dll = cdll.LoadLibrary(dll_path)
    dll.sdet_create.argtypes = [POINTER(_CSDetParams)]
    dll.sdet_create.restype = c_void_p
    dll.sdet_destroy.argtypes = [c_void_p]
    dll.sdet_destroy.restype = None
    dll.sdet_detect.argtypes = [
        c_void_p, POINTER(c_uint16), c_int, c_int,
        POINTER(POINTER(c_double)), POINTER(POINTER(c_double)), POINTER(c_int),
    ]
    dll.sdet_detect.restype = c_int
    dll.sdet_free_coords.argtypes = [POINTER(c_double)]
    dll.sdet_free_coords.restype = None
    return dll


class StarDetector:
    def __init__(self, dll_path: Optional[str] = None, params: Optional[SDetParamsPy] = None):
        if dll_path is None:
            base = os.path.dirname(os.path.abspath(__file__))
            dll_path = os.path.normpath(os.path.join(base, "..", "star_detector.dll"))
        self._dll = _load_dll(dll_path)
        if params is None:
            params = SDetParamsPy()
        c_params = _params_py_to_c(params)
        self._handle = self._dll.sdet_create(byref(c_params))
        self._closed = False
        if not self._handle:
            raise RuntimeError(f"sdet_create 失败: {dll_path}")

    def detect(self, image: np.ndarray) -> list[tuple[float, float]]:
        if image.ndim != 2:
            raise ValueError(f"image 必须为2D数组, 当前 ndim={image.ndim}")
        if image.dtype == np.uint16:
            img = np.ascontiguousarray(image, dtype=np.uint16)
        else:
            img = np.ascontiguousarray(np.clip(image, 0, 65535).astype(np.uint16), dtype=np.uint16)
        height, width = img.shape
        data_ptr = img.ctypes.data_as(POINTER(c_uint16))
        out_x = POINTER(c_double)()
        out_y = POINTER(c_double)()
        out_count = c_int(0)
        ret = self._dll.sdet_detect(
            self._handle, data_ptr, width, height,
            byref(out_x), byref(out_y), byref(out_count),
        )
        if ret != 0:
            raise RuntimeError(f"sdet_detect 返回错误码: {ret}")
        count = out_count.value
        coords = []
        if count > 0 and out_x and out_y:
            for i in range(count):
                coords.append((out_x[i], out_y[i]))
            self._dll.sdet_free_coords(out_x)
            self._dll.sdet_free_coords(out_y)
        return coords

    def close(self) -> None:
        if not self._closed and self._handle:
            self._dll.sdet_destroy(self._handle)
            self._handle = None
            self._closed = True

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
