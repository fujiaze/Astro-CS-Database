# -*- coding: utf-8 -*-
"""
SNR Estimator - 信噪比估算 C++ DLL Python封装
功能: 加载snr_estimator.dll, 封装snr_estimate C接口
用途: 基于乘法模型计算每像素信噪比 SNR = SNR_phot × (SNR_psf/median)
      SNR_phot = 1/(ln(10)×sigma_residual) 全帧常数
      SNR_psf = IDW插值(PSF星位置, (A-B)/mad) 反距离加权
依赖: numpy, ctypes
调用: from snr_estimator import SNREstimator
      est = SNREstimator()
      snr, code = est.estimate(data, psf, sigma_residual)
"""

from __future__ import annotations

import logging
import os
import sys
from ctypes import (
    c_int, c_double, c_float,
    POINTER, byref, cdll,
)
from typing import Optional, Tuple

import numpy as np

logger = logging.getLogger(__name__)


def _find_dll() -> str:
    """查找snr_estimator.dll路径"""
    # 本文件: lib/snr_estimator/python/snr_estimator.py
    # DLL:    lib/snr_estimator/cpp/snr_estimator.dll
    this_dir = os.path.dirname(os.path.abspath(__file__))
    dll_path = os.path.normpath(os.path.join(
        this_dir, "..", "cpp", "snr_estimator.dll"))
    if os.path.isfile(dll_path):
        return dll_path
    raise FileNotFoundError(
        f"snr_estimator.dll 未找到, 期望路径: {dll_path}\n"
        f"请先运行 cpp/build.ps1 编译DLL")


class SNREstimator:
    """信噪比估算DLL封装

    封装snr_estimate C接口, 实现:
      1. SNR_phot = 1.0 / (ln(10) × sigma_residual)  全帧常数
      2. SNR_psf(pixel) = IDW(PSF星位置, (A-B)/mad)  反距离加权插值
         - IDW power=2.0, 搜索半径=FOV对角线像素
         - 跳过 status!=0 或 A<=B 或 mad<=0 的星
      3. SNR = SNR_phot × (SNR_psf / median(SNR_psf))

    退化路径:
      - n_stars<=0: out_snr 全填 SNR_phot (返回 1)
      - sigma_residual<=0: out_snr 全填 1.0 (返回 2)
      - nullptr: 返回 3
    """

    def __init__(self, dll_path: Optional[str] = None):
        """加载DLL

        Args:
            dll_path: DLL路径, None时自动查找
        """
        if dll_path is None:
            dll_path = _find_dll()
        logger.info("加载 snr_estimator.dll: %s", dll_path)

        self._dll = cdll.LoadLibrary(dll_path)
        self._setup_signature()
        logger.info("DLL加载成功, API已绑定")

    def _setup_signature(self):
        """设置C函数签名"""
        self._dll.snr_estimate.restype = c_int
        self._dll.snr_estimate.argtypes = [
            # data, h, w
            POINTER(c_float), c_int, c_int,
            # psf, n_stars
            POINTER(c_double), c_int,
            # sigma_residual
            c_double,
            # out_snr
            POINTER(c_float),
        ]

    def estimate(
        self,
        data: np.ndarray,
        psf: np.ndarray,
        sigma_residual: float,
    ) -> Tuple[np.ndarray, int]:
        """SNR估算

        Args:
            data: 图像像素 float32 [H, W] (2D, 来自CALIBRATE阶段)
            psf: PSF拟合结果 float64 [n_stars, 9]
                 每行: [status, B, flux, cx, cy, fwhm, A, mad, eccentricity]
            sigma_residual: 测光残差sigma (来自photo_stats块SIGMA_RESIDUAL)

        Returns:
            (snr_array, ret_code)
            snr_array: SNR图 float32 [H, W]
            ret_code: 0=成功, 1=n_stars<=0退化, 2=sigma_residual<=0退化, 3=nullptr

        Raises:
            RuntimeError: DLL调用返回nullptr错误码(3)
            ValueError: 输入维度错误
        """
        # ---- 输入校验与类型转换 ----
        data = np.ascontiguousarray(data, dtype=np.float32)
        if data.ndim != 2:
            raise ValueError(f"data必须为2D, 实际为{data.ndim}D")
        height, width = data.shape

        psf = np.ascontiguousarray(psf, dtype=np.float64)
        if psf.ndim != 2 or psf.shape[1] != 9:
            raise ValueError(f"psf必须为[n_stars, 9], 实际shape={psf.shape}")
        n_stars = psf.shape[0]

        logger.info(
            "SNR估算: image=%dx%d, n_stars=%d, sigma_residual=%.6f",
            width, height, n_stars, sigma_residual)

        # ---- 输出缓冲 ----
        out_snr = np.zeros(width * height, dtype=np.float32)

        # ---- 调用C函数 ----
        ret = self._dll.snr_estimate(
            data.ctypes.data_as(POINTER(c_float)), height, width,
            psf.ctypes.data_as(POINTER(c_double)), n_stars,
            c_double(sigma_residual),
            out_snr.ctypes.data_as(POINTER(c_float)),
        )

        # ---- 重塑输出 ----
        out_snr = out_snr.reshape(height, width)

        code_msg = {0: "成功", 1: "n_stars<=0退化(全填SNR_phot)",
                    2: "sigma_residual<=0退化(全填1.0)", 3: "nullptr错误"}.get(
            ret, f"未知错误码{ret}")
        logger.info("SNR估算完成: ret=%d (%s)", ret, code_msg)

        if ret == 3:
            raise RuntimeError(f"snr_estimate 失败, nullptr错误 (ret={ret})")

        return out_snr, ret

    @staticmethod
    def print_stats(snr_array: np.ndarray) -> None:
        """打印SNR数组统计信息

        Args:
            snr_array: SNR图 float32 [H, W]
        """
        snr = np.asarray(snr_array, dtype=np.float64).ravel()
        print(f"  SNR统计 (n={snr.size}):")
        print(f"    min    = {snr.min():.6f}")
        print(f"    max    = {snr.max():.6f}")
        print(f"    median = {np.median(snr):.6f}")
        print(f"    mean   = {snr.mean():.6f}")
        print(f"    std    = {snr.std():.6f}")


# ============================================================================
# 模块自测
# ============================================================================
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")

    print("=" * 60)
    print("SNREstimator DLL 封装测试")
    print("=" * 60)

    try:
        est = SNREstimator()
    except FileNotFoundError as e:
        print(f"[SKIP] {e}")
        sys.exit(0)

    # ---- 构造测试数据 ----
    # 10颗PSF星, 100x100图像, sigma_residual=0.1
    img_w, img_h = 100, 100
    image = np.full((img_h, img_w), 1000.0, dtype=np.float32)

    # PSF星: status=0, B=100, flux=50000, cx/cy网格分布, fwhm=3, A=500, mad=10
    n = 10
    psf = np.zeros((n, 9), dtype=np.float64)
    xs = np.linspace(20, 80, n)
    for i in range(n):
        psf[i] = [0, 100, 50000, xs[i], 50.0, 3.0, 500, 10, 0.1]

    sigma = 0.1

    # ---- 调用 ----
    snr, code = est.estimate(image, psf, sigma)

    # SNR_phot = 1/(ln(10)*0.1) = 4.3429
    snr_phot = 1.0 / (np.log(10) * sigma)
    print(f"\n返回码: {code} (期望 0)")
    print(f"SNR_phot = 1/(ln(10)*{sigma}) = {snr_phot:.6f}")
    print(f"所有星 SNR_psf = (A-B)/mad = (500-100)/10 = 40.0")
    print(f"median(SNR_psf) = 40.0 -> SNR = SNR_phot * (40/40) = {snr_phot:.6f}")
    est.print_stats(snr)
    print(f"snr[50,50] = {snr[50, 50]:.6f} (期望 ~{snr_phot:.6f})")

    ok = (code == 0 and abs(snr[50, 50] - snr_phot) < 0.01)
    print(f"[{'PASS' if ok else 'FAIL'}] DLL封装测试")
