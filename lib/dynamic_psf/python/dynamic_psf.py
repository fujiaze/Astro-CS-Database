"""
Dynamic PSF - Moffat4 PSF拟合引擎 Python封装
功能: 对天文图像中的星点进行Moffat4 PSF拟合
用途: 单点/批量PSF参数测量，支持16bit整数输入
"""

from __future__ import annotations

import os
from ctypes import (
    Structure, c_int, c_double, c_void_p, c_uint16, c_float,
    POINTER, byref, cdll,
)
from dataclasses import dataclass
from typing import Optional

import numpy as np


@dataclass
class DPSFFitParamsPy:
    fitRadius: int = 8
    maxIter: int = 200
    tolerance: float = 1e-8


@dataclass
class DPSFFitResultPy:
    status: int = -1
    B: float = 0.0
    A: float = 0.0
    cx: float = 0.0
    cy: float = 0.0
    sx: float = 0.0
    sy: float = 0.0
    theta: float = 0.0
    fwhm_x: float = 0.0
    fwhm_y: float = 0.0
    mad: float = 0.0
    flux: float = 0.0
    eccentricity: float = 0.0


DPSF_FIT_OK = 0
DPSF_FIT_NO_CONVERGENCE = 1
DPSF_FIT_INVALID_PARAMS = 2
DPSF_FIT_ITERATION_LIMIT = 3

DPSF_FIT_STATUS_NAMES = {
    DPSF_FIT_OK: "OK",
    DPSF_FIT_NO_CONVERGENCE: "NO_CONVERGENCE",
    DPSF_FIT_INVALID_PARAMS: "INVALID_PARAMS",
    DPSF_FIT_ITERATION_LIMIT: "ITERATION_LIMIT",
}


class _CDPSFFitParams(Structure):
    _fields_ = [
        ("fitRadius", c_int),
        ("maxIter", c_int),
        ("tolerance", c_double),
    ]


class _CDPSFFitResult(Structure):
    _fields_ = [
        ("status", c_int),
        ("B", c_double),
        ("A", c_double),
        ("cx", c_double),
        ("cy", c_double),
        ("sx", c_double),
        ("sy", c_double),
        ("theta", c_double),
        ("fwhm_x", c_double),
        ("fwhm_y", c_double),
        ("mad", c_double),
        ("flux", c_double),
        ("eccentricity", c_double),
    ]


def _params_py_to_c(p: DPSFFitParamsPy) -> _CDPSFFitParams:
    return _CDPSFFitParams(
        fitRadius=p.fitRadius,
        maxIter=p.maxIter,
        tolerance=p.tolerance,
    )


def _result_c_to_py(c: _CDPSFFitResult) -> DPSFFitResultPy:
    return DPSFFitResultPy(
        status=c.status, B=c.B, A=c.A, cx=c.cx, cy=c.cy,
        sx=c.sx, sy=c.sy, theta=c.theta, fwhm_x=c.fwhm_x,
        fwhm_y=c.fwhm_y, mad=c.mad, flux=c.flux,
        eccentricity=c.eccentricity,
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
    dll.dpsf_fit.argtypes = [
        POINTER(c_uint16), c_int, c_int,
        c_double, c_double,
        POINTER(_CDPSFFitParams),
        POINTER(_CDPSFFitResult),
    ]
    dll.dpsf_fit.restype = c_int
    dll.dpsf_fit_batch.argtypes = [
        POINTER(c_uint16), c_int, c_int,
        POINTER(c_double), POINTER(c_double), c_int,
        POINTER(_CDPSFFitParams),
        POINTER(POINTER(_CDPSFFitResult)),
    ]
    dll.dpsf_fit_batch.restype = c_int
    dll.dpsf_free_results.argtypes = [POINTER(_CDPSFFitResult)]
    dll.dpsf_free_results.restype = None
    # float32 PSF 拟合 API (P02-005, v1.1)
    # int dpsf_fit_batch_f32(const float* image, int width, int height,
    #                         const double* detections, int n_detections,
    #                         const DPSFFitParams* params,
    #                         double* out_psf_params, int* out_n_valid)
    dll.dpsf_fit_batch_f32.argtypes = [
        POINTER(c_float), c_int, c_int,
        POINTER(c_double), c_int,
        POINTER(_CDPSFFitParams),
        POINTER(c_double), POINTER(c_int),
    ]
    dll.dpsf_fit_batch_f32.restype = c_int
    return dll


class DynamicPSF:
    _dll = None
    _dll_path = None

    @classmethod
    def _ensure_dll(cls, dll_path: Optional[str] = None):
        if cls._dll is not None and (dll_path is None or cls._dll_path == dll_path):
            return cls._dll
        if dll_path is None:
            base = os.path.dirname(os.path.abspath(__file__))
            dll_path = os.path.normpath(os.path.join(base, "..", "dynamic_psf.dll"))
        cls._dll = _load_dll(dll_path)
        cls._dll_path = dll_path
        return cls._dll

    @staticmethod
    def fit(image: np.ndarray, cx: float, cy: float,
            params: Optional[DPSFFitParamsPy] = None,
            dll_path: Optional[str] = None) -> DPSFFitResultPy:
        dll = DynamicPSF._ensure_dll(dll_path)
        if image.ndim != 2:
            raise ValueError(f"image 必须为2D数组, 当前 ndim={image.ndim}")
        if image.dtype == np.uint16:
            img = np.ascontiguousarray(image, dtype=np.uint16)
        else:
            img = np.ascontiguousarray(np.clip(image, 0, 65535).astype(np.uint16), dtype=np.uint16)
        height, width = img.shape
        data_ptr = img.ctypes.data_as(POINTER(c_uint16))
        if params is None:
            params = DPSFFitParamsPy()
        c_params = _params_py_to_c(params)
        result = _CDPSFFitResult()
        ret = dll.dpsf_fit(data_ptr, width, height, cx, cy, byref(c_params), byref(result))
        if ret != 0:
            return DPSFFitResultPy(status=ret)
        return _result_c_to_py(result)

    @staticmethod
    def fit_batch(image: np.ndarray,
                  cx_list: list[float], cy_list: list[float],
                  params: Optional[DPSFFitParamsPy] = None,
                  dll_path: Optional[str] = None) -> list[DPSFFitResultPy]:
        dll = DynamicPSF._ensure_dll(dll_path)
        if image.ndim != 2:
            raise ValueError(f"image 必须为2D数组, 当前 ndim={image.ndim}")
        if image.dtype == np.uint16:
            img = np.ascontiguousarray(image, dtype=np.uint16)
        else:
            img = np.ascontiguousarray(np.clip(image, 0, 65535).astype(np.uint16), dtype=np.uint16)
        height, width = img.shape
        data_ptr = img.ctypes.data_as(POINTER(c_uint16))
        n = len(cx_list)
        if n == 0:
            return []
        if n != len(cy_list):
            raise ValueError("cx_list 和 cy_list 长度不一致")
        cx_arr = (c_double * n)(*cx_list)
        cy_arr = (c_double * n)(*cy_list)
        if params is None:
            params = DPSFFitParamsPy()
        c_params = _params_py_to_c(params)
        out_results = POINTER(_CDPSFFitResult)()
        ret = dll.dpsf_fit_batch(data_ptr, width, height,
                                  cx_arr, cy_arr, n,
                                  byref(c_params), byref(out_results))
        if ret != 0:
            raise RuntimeError(f"dpsf_fit_batch 返回错误码: {ret}")
        results = []
        if out_results:
            for i in range(n):
                results.append(_result_c_to_py(out_results[i]))
            dll.dpsf_free_results(out_results)
        return results

    @staticmethod
    def fit_batch_f32(image: np.ndarray,
                      detections: np.ndarray,
                      params: Optional[DPSFFitParamsPy] = None,
                      dll_path: Optional[str] = None):
        """float32 PSF 批量拟合 (P02-005, v1.1)

        消费 star_det v1 (FLOAT64 [N,6]) 格式的检测结果, 直接在 float32 图像上拟合,
        不做 0-65535 clip, 不创建整张 uint16 图像。

        参数:
            image       - float32 图像, 2D (height, width)
            detections  - star_det v1 检测结果, shape=(N,6), dtype=float64
                          列: [0]=x_px [1]=y_px [2]=flux [3]=mag [4]=saturated [5]=has_saturated
            params      - 拟合参数, 可为 None 使用默认值 (fitRadius=8, maxIter=200, tol=1e-8)
            dll_path    - DLL 路径, 默认 lib/dynamic_psf/dynamic_psf.dll

        返回:
            (psf_params, n_valid) 元组:
              psf_params - np.ndarray shape=(N,9) dtype=float64
                           列: [0]=B [1]=A [2]=cx [3]=cy [4]=sx [5]=sy
                               [6]=theta [7]=fwhm_x [8]=fwhm_y
                           失败的拟合行全为 NaN
              n_valid    - 成功拟合的星点数
        """
        dll = DynamicPSF._ensure_dll(dll_path)
        if image.ndim != 2:
            raise ValueError(f"image 必须为2D数组, 当前 ndim={image.ndim}")
        if image.dtype != np.float32:
            img = np.ascontiguousarray(image, dtype=np.float32)
        else:
            img = np.ascontiguousarray(image, dtype=np.float32)
        height, width = img.shape

        if detections.ndim != 2 or detections.shape[1] != 6:
            raise ValueError(
                f"detections 必须为 shape=(N,6) 的数组, 当前 shape={detections.shape}")
        if detections.dtype != np.float64:
            det = np.ascontiguousarray(detections, dtype=np.float64)
        else:
            det = np.ascontiguousarray(detections, dtype=np.float64)
        n = det.shape[0]
        if n == 0:
            return np.zeros((0, 9), dtype=np.float64), 0

        img_ptr = img.ctypes.data_as(POINTER(c_float))
        det_ptr = det.ctypes.data_as(POINTER(c_double))
        if params is None:
            params = DPSFFitParamsPy()
        c_params = _params_py_to_c(params)

        out_params = np.zeros((n, 9), dtype=np.float64)
        out_ptr = out_params.ctypes.data_as(POINTER(c_double))
        n_valid = c_int(0)

        ret = dll.dpsf_fit_batch_f32(
            img_ptr, width, height,
            det_ptr, n,
            byref(c_params),
            out_ptr, byref(n_valid),
        )
        if ret != 0:
            raise RuntimeError(f"dpsf_fit_batch_f32 返回错误码: {ret}")
        return out_params, n_valid.value
