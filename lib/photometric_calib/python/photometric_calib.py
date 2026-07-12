# -*- coding: utf-8 -*-
"""
Photometric Calib - 简化版测光校准 C++ DLL Python封装
功能: 加载photometric_calib.dll, 封装pc_calibrate_simple C接口
用途: 供pipeline_adapter调用, 实现WCS投影+星匹配+MAD清洗+全局scale校正
依赖: numpy, ctypes
调用: from photometric_calib import PhotometricCalib
      pc = PhotometricCalib()
      out_img, n_matched, scale = pc.calibrate_simple(...)
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
    """查找photometric_calib.dll路径"""
    # 本文件: lib/photometric_calib/python/photometric_calib.py
    # DLL:    lib/photometric_calib/cpp/photometric_calib.dll
    this_dir = os.path.dirname(os.path.abspath(__file__))
    dll_path = os.path.normpath(os.path.join(
        this_dir, "..", "cpp", "photometric_calib.dll"))
    if os.path.isfile(dll_path):
        return dll_path
    raise FileNotFoundError(
        f"photometric_calib.dll 未找到, 期望路径: {dll_path}\n"
        f"请先运行 cpp/build.ps1 编译DLL")


class PhotometricCalib:
    """简化版测光校准DLL封装

    封装pc_calibrate_simple C接口, 实现:
      1. WCS投影Gaia星到像素坐标 (TAN+SIP)
      2. 暴力最近邻匹配PSF星和Gaia星 (距离<3px)
      3. MAD离群清洗 (r=log10(F_instr/F_syn), sigma=3.0)
      4. scale=median(F_syn/F_instr)
      5. I_cal=I*scale
    """

    def __init__(self, dll_path: Optional[str] = None):
        """加载DLL

        Args:
            dll_path: DLL路径, None时自动查找
        """
        if dll_path is None:
            dll_path = _find_dll()
        logger.info("加载 photometric_calib.dll: %s", dll_path)

        self._dll = cdll.LoadLibrary(dll_path)
        self._setup_signature()
        logger.info("DLL加载成功, API已绑定")

    def _setup_signature(self):
        """设置C函数签名"""
        self._dll.pc_calibrate_simple.restype = c_int
        self._dll.pc_calibrate_simple.argtypes = [
            # pixels, width, height
            POINTER(c_float), c_int, c_int,
            # gaia_ra, gaia_dec, gaia_mag, gaia_fsyn, n_gaia
            POINTER(c_double), POINTER(c_double),
            POINTER(c_double), POINTER(c_double), c_int,
            # psf_cx, psf_cy, psf_flux, psf_status, n_psf
            POINTER(c_double), POINTER(c_double),
            POINTER(c_double), POINTER(c_int), c_int,
            # WCS参数
            c_double, c_double, c_double, c_double,
            c_double, c_double, c_double, c_double,
            # SIP
            c_int,
            POINTER(c_double), POINTER(c_double),
            POINTER(c_double), POINTER(c_double),
            # 输出
            POINTER(c_float), POINTER(c_int), POINTER(c_double),
        ]

    def calibrate_simple(
        self,
        pixels: np.ndarray,
        gaia_ra: np.ndarray,
        gaia_dec: np.ndarray,
        gaia_mag: np.ndarray,
        gaia_fsyn: np.ndarray,
        psf_cx: np.ndarray,
        psf_cy: np.ndarray,
        psf_flux: np.ndarray,
        psf_status: np.ndarray,
        crval1: float, crval2: float,
        crpix1: float, crpix2: float,
        cd11: float, cd12: float, cd21: float, cd22: float,
        sip_order: int = 0,
        sip_a: Optional[np.ndarray] = None,
        sip_b: Optional[np.ndarray] = None,
        sip_ap: Optional[np.ndarray] = None,
        sip_bp: Optional[np.ndarray] = None,
    ) -> Tuple[np.ndarray, int, float]:
        """简化版测光校准

        Args:
            pixels: 图像像素 float32 [H, W] (2D)
            gaia_ra/dec/mag/fsyn: Gaia星数组 float64 [n_gaia]
            psf_cx/cy/flux: PSF星数组 float64 [n_psf]
            psf_status: PSF星状态 int32 [n_psf] (0=成功)
            WCS参数: crval1/2, crpix1/2, cd11/12/21/22
            sip_order: SIP阶数 (0=无SIP)
            sip_a/b/ap/bp: SIP系数数组 float64 [36] (按i*6+j索引)

        Returns:
            (out_pixels, n_matched, scale_factor)
            out_pixels: 校正后图像 float32 [H, W]
            n_matched: 匹配星数 (MAD清洗后)
            scale_factor: scale因子
        """
        # ---- 输入校验与类型转换 ----
        pixels = np.ascontiguousarray(pixels, dtype=np.float32)
        if pixels.ndim != 2:
            raise ValueError(f"pixels必须为2D, 实际为{pixels.ndim}D")
        height, width = pixels.shape

        gaia_ra = np.ascontiguousarray(gaia_ra, dtype=np.float64)
        gaia_dec = np.ascontiguousarray(gaia_dec, dtype=np.float64)
        gaia_mag = np.ascontiguousarray(gaia_mag, dtype=np.float64)
        gaia_fsyn = np.ascontiguousarray(gaia_fsyn, dtype=np.float64)
        n_gaia = gaia_ra.size

        psf_cx = np.ascontiguousarray(psf_cx, dtype=np.float64)
        psf_cy = np.ascontiguousarray(psf_cy, dtype=np.float64)
        psf_flux = np.ascontiguousarray(psf_flux, dtype=np.float64)
        psf_status = np.ascontiguousarray(psf_status, dtype=np.int32)
        n_psf = psf_cx.size

        # ---- SIP系数 ----
        def _prep_sip(arr):
            if arr is None:
                return None
            return np.ascontiguousarray(arr, dtype=np.float64)

        sip_a_c = _prep_sip(sip_a)
        sip_b_c = _prep_sip(sip_b)
        sip_ap_c = _prep_sip(sip_ap)
        sip_bp_c = _prep_sip(sip_bp)

        # ---- 输出缓冲 ----
        out_pixels = np.zeros(width * height, dtype=np.float32)
        n_matched = c_int(0)
        scale_factor = c_double(0.0)

        # ---- 调用C函数 ----
        ret = self._dll.pc_calibrate_simple(
            pixels.ctypes.data_as(POINTER(c_float)), width, height,
            gaia_ra.ctypes.data_as(POINTER(c_double)),
            gaia_dec.ctypes.data_as(POINTER(c_double)),
            gaia_mag.ctypes.data_as(POINTER(c_double)),
            gaia_fsyn.ctypes.data_as(POINTER(c_double)), n_gaia,
            psf_cx.ctypes.data_as(POINTER(c_double)),
            psf_cy.ctypes.data_as(POINTER(c_double)),
            psf_flux.ctypes.data_as(POINTER(c_double)),
            psf_status.ctypes.data_as(POINTER(c_int)), n_psf,
            crval1, crval2, crpix1, crpix2,
            cd11, cd12, cd21, cd22,
            sip_order,
            sip_a_c.ctypes.data_as(POINTER(c_double)) if sip_a_c is not None else None,
            sip_b_c.ctypes.data_as(POINTER(c_double)) if sip_b_c is not None else None,
            sip_ap_c.ctypes.data_as(POINTER(c_double)) if sip_ap_c is not None else None,
            sip_bp_c.ctypes.data_as(POINTER(c_double)) if sip_bp_c is not None else None,
            out_pixels.ctypes.data_as(POINTER(c_float)),
            byref(n_matched), byref(scale_factor),
        )

        if ret != 0:
            raise RuntimeError(f"pc_calibrate_simple 失败, 返回码={ret}")

        # ---- 重塑输出 ----
        out_pixels = out_pixels.reshape(height, width)
        logger.info("测光校准完成: n_matched=%d, scale=%.6e",
                    n_matched.value, scale_factor.value)
        return out_pixels, n_matched.value, scale_factor.value


# ============================================================================
# 模块自测
# ============================================================================
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")

    print("=" * 60)
    print("PhotometricCalib DLL 封装测试")
    print("=" * 60)

    try:
        pc = PhotometricCalib()
    except FileNotFoundError as e:
        print(f"[SKIP] {e}")
        sys.exit(0)

    # ---- 构造测试数据 ----
    # 简单TAN投影: crpix=100,100, crval=10,20, cd=0.01
    crpix1, crpix2 = 100.0, 100.0
    crval1, crval2 = 10.0, 20.0
    cd_val = 0.01
    img_w, img_h = 200, 200

    # 10颗Gaia星 (像素坐标已知, 反推RA/Dec)
    px_vals = np.linspace(25, 175, 10)
    py_vals = np.full(10, 100.0)
    # 手动计算RA/Dec (无SIP, 简单TAN)
    from astropy.wcs import WCS
    w = WCS(naxis=2)
    w.wcs.cd = [[cd_val, 0], [0, cd_val]]
    w.wcs.crval = [crval1, crval2]
    w.wcs.crpix = [crpix1, crpix2]
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    world = w.all_pix2world(px_vals, py_vals, 0)
    gaia_ra = world[0]
    gaia_dec = world[1]
    gaia_mag = np.full(10, 12.0)
    gaia_fsyn = np.full(10, 50000.0)

    # PSF星: 像素位置 + 微小偏移, flux = f_syn / 10
    psf_cx = px_vals + 0.1
    psf_cy = py_vals - 0.1
    psf_flux = gaia_fsyn / 10.0
    psf_status = np.zeros(10, dtype=np.int32)

    # 原图: 200x200 float32, 值1000
    image = np.full((img_h, img_w), 1000.0, dtype=np.float32)

    # ---- 调用 ----
    out_img, n_matched, scale = pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        crval1, crval2, crpix1, crpix2,
        cd_val, 0.0, 0.0, cd_val,
    )

    print(f"n_matched = {n_matched} (期望 10)")
    print(f"scale = {scale:.6e} (期望 ~10.0, 因 F_syn/F_instr = 50000/5000 = 10)")
    print(f"out_img[0,0] = {out_img[0, 0]:.4f} (期望 ~10000.0)")
    print(f"out_img shape = {out_img.shape}")

    ok = (n_matched == 10 and abs(scale - 10.0) < 0.1
          and abs(out_img[0, 0] - 10000.0) < 100.0)
    print(f"[{'PASS' if ok else 'FAIL'}] DLL封装测试")
