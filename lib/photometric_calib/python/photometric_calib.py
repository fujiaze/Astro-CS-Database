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
    c_int, c_double, c_float, c_void_p,
    POINTER, byref, cdll, Structure,
)
from typing import Optional, Tuple

import numpy as np

logger = logging.getLogger(__name__)


# ============================================================================
# P12-001: PhotometricDiag ctypes 镜像结构体
# 与 C++ 端 lib/photometric_calib/cpp/include/photometric_calib.h 中
# struct PhotometricDiag 字段顺序与类型一一对应 (12 int + 8 double = 20 字段).
# ============================================================================
class PhotometricDiag(Structure):
    """P12-001 分阶段诊断结构体 (ctypes 镜像)

    与 C++ 端 PhotometricDiag 一一对应:
      阶段1 Fsyn:  spectrum_rows_total / valid_fsyn
      阶段2 投影:  gaia_projected_in_frame
      阶段3 PSF:   psf_total / psf_valid
      阶段4/5 匹配: spatial_candidates / unique_matches
      阶段6 拒绝:  rejected_ambiguous / rejected_distance / rejected_quality
      阶段7 拟合:  fit_used / robust_iterations / scale_factor / sigma_residual
      阶段8 残差:  r_median / r_p90 / r_max
                   match_distance_median / match_distance_p90 / match_distance_max
    """
    _fields_ = [
        # 阶段1: Fsyn
        ("spectrum_rows_total", c_int),
        ("valid_fsyn", c_int),
        # 阶段2: 投影
        ("gaia_projected_in_frame", c_int),
        # 阶段3: PSF
        ("psf_total", c_int),
        ("psf_valid", c_int),
        # 阶段4/5: 匹配
        ("spatial_candidates", c_int),
        ("unique_matches", c_int),
        # 阶段6: 拒绝原因
        ("rejected_ambiguous", c_int),
        ("rejected_distance", c_int),
        ("rejected_quality", c_int),
        # 阶段7: 拟合
        ("fit_used", c_int),
        ("robust_iterations", c_int),
        ("scale_factor", c_double),
        ("sigma_residual", c_double),
        # 阶段8: 残差/距离统计
        ("r_median", c_double),
        ("r_p90", c_double),
        ("r_max", c_double),
        ("match_distance_median", c_double),
        ("match_distance_p90", c_double),
        ("match_distance_max", c_double),
    ]

    def to_dict(self) -> dict:
        """转 dict (供日志/JSON 序列化)"""
        return {
            "spectrum_rows_total": self.spectrum_rows_total,
            "valid_fsyn": self.valid_fsyn,
            "gaia_projected_in_frame": self.gaia_projected_in_frame,
            "psf_total": self.psf_total,
            "psf_valid": self.psf_valid,
            "spatial_candidates": self.spatial_candidates,
            "unique_matches": self.unique_matches,
            "rejected_ambiguous": self.rejected_ambiguous,
            "rejected_distance": self.rejected_distance,
            "rejected_quality": self.rejected_quality,
            "fit_used": self.fit_used,
            "robust_iterations": self.robust_iterations,
            "scale_factor": self.scale_factor,
            "sigma_residual": self.sigma_residual,
            "r_median": self.r_median,
            "r_p90": self.r_p90,
            "r_max": self.r_max,
            "match_distance_median": self.match_distance_median,
            "match_distance_p90": self.match_distance_p90,
            "match_distance_max": self.match_distance_max,
        }


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

        # P12-001: 显式添加 DLL 所在目录到搜索路径, 确保 gaia_client.dll 等依赖能被找到
        # (ctypes.cdll.LoadLibrary 默认不搜索 DLL 同目录的依赖)
        dll_dir = os.path.dirname(os.path.abspath(dll_path))
        try:
            os.add_dll_directory(dll_dir)
        except (AttributeError, OSError):
            pass  # 非 Windows 或目录已添加

        # MinGW 运行时 DLL (libgomp-1.dll/zlib1.dll 等) 在 mingw64/bin,
        # gaia_client_p002006.dll 依赖它们. 自动添加常见 MinGW 路径.
        for mingw_bin in (r"C:\msys64\mingw64\bin", r"C:\msys64\usr\bin"):
            if os.path.isdir(mingw_bin):
                try:
                    os.add_dll_directory(mingw_bin)
                except (AttributeError, OSError):
                    pass
                break

        # 预加载 gaia_client*.dll (photometric_calib.dll 的依赖),
        # 让 Windows 把 libgomp-1.dll/zlib1.dll 等二级依赖也加载到进程地址空间.
        for dep_name in ("gaia_client_p002006.dll", "gaia_client.dll"):
            dep_path = os.path.join(dll_dir, dep_name)
            if os.path.isfile(dep_path):
                try:
                    cdll.LoadLibrary(dep_path)
                    logger.debug("预加载依赖 DLL: %s", dep_path)
                except OSError as e:
                    logger.debug("预加载 %s 失败 (可忽略): %s", dep_path, e)
                break

        self._dll = cdll.LoadLibrary(dll_path)
        self._setup_signature()
        logger.info("DLL加载成功, API已绑定")

    def _setup_signature(self):
        """设置C函数签名

        P12-001: argtypes 末尾新增 POINTER(PhotometricDiag) 出参.
        GAP-012: 同时补齐 QE 参数 (qe_wl/qe_trans/qe_count) 以匹配 C++ ABI.
        """
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
            # GAP-012: qe_wl, qe_trans, qe_count (pc_calibrate_simple 不计算 F_syn, 可为 nullptr/0)
            POINTER(c_double), POINTER(c_double), c_int,
            # WCS参数
            c_double, c_double, c_double, c_double,
            c_double, c_double, c_double, c_double,
            # SIP
            c_int,
            POINTER(c_double), POINTER(c_double),
            POINTER(c_double), POINTER(c_double),
            # 输出
            POINTER(c_float), POINTER(c_int), POINTER(c_double),
            POINTER(c_double),  # out_sigma_residual (供 SNR 模块 §14, 可为 nullptr 向后兼容)
            POINTER(PhotometricDiag),  # P12-001: out_diag (可为 nullptr 向后兼容)
        ]

        # 新接口: pc_calibrate_simple_with_gaia (DLL 内部完成锥形搜索+光谱积分)
        self._dll.pc_calibrate_simple_with_gaia.restype = c_int
        self._dll.pc_calibrate_simple_with_gaia.argtypes = [
            # gaia_client_handle
            c_void_p,
            # 锥形搜索中心与半径, 星等范围
            c_double, c_double, c_double,
            c_double, c_double,
            # 滤光片波长/透过率/数量
            POINTER(c_double), POINTER(c_double), c_int,
            # GAP-012: qe_wl, qe_trans, qe_count
            POINTER(c_double), POINTER(c_double), c_int,
            # 光谱波长数组/数量
            POINTER(c_double), c_int,
            # pixels, width, height
            POINTER(c_float), c_int, c_int,
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
            POINTER(c_double),  # out_sigma_residual (供 SNR 模块 §14, 可为 nullptr 向后兼容)
            POINTER(PhotometricDiag),  # P12-001: out_diag (可为 nullptr 向后兼容)
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
        qe_wl: Optional[np.ndarray] = None,
        qe_trans: Optional[np.ndarray] = None,
    ) -> Tuple[np.ndarray, int, float, float, PhotometricDiag]:
        """简化版测光校准

        Args:
            pixels: 图像像素 float32 [H, W] (2D)
            gaia_ra/dec/mag/fsyn: Gaia星数组 float64 [n_gaia]
            psf_cx/cy/flux: PSF星数组 float64 [n_psf]
            psf_status: PSF星状态 int32 [n_psf] (0=成功)
            WCS参数: crval1/2, crpix1/2, cd11/12/21/22
            sip_order: SIP阶数 (0=无SIP)
            sip_a/b/ap/bp: SIP系数数组 float64 [36] (按i*6+j索引)
            qe_wl: GAP-012 CCD QE 波长数组 [n_qe] (nm), None 时 Q(λ)=1.0
                   注: pc_calibrate_simple 不在 DLL 内部计算 F_syn, QE 参数仅作 API 一致性保留
            qe_trans: CCD QE 透过率数组 [n_qe] [0,1]

        Returns:
            (out_pixels, n_matched, scale_factor, sigma_residual, diag)
            out_pixels: 校正后图像 float32 [H, W]
            n_matched: 匹配星数 (MAD清洗后)
            scale_factor: scale因子
            sigma_residual: MAD/0.6745 (供 SNR 模块 §14 计算 SNR_phot)
            diag: P12-001 PhotometricDiag 分阶段诊断结构体 (20字段)
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

        # ---- GAP-012: QE 曲线 (pc_calibrate_simple 不计算 F_syn, 但需匹配 ABI) ----
        qe_wl_c = _prep_sip(qe_wl)
        qe_trans_c = _prep_sip(qe_trans)
        qe_count = qe_wl_c.size if qe_wl_c is not None else 0

        # ---- 输出缓冲 ----
        out_pixels = np.zeros(width * height, dtype=np.float32)
        n_matched = c_int(0)
        scale_factor = c_double(0.0)
        sigma_residual = c_double(0.0)
        diag = PhotometricDiag()  # P12-001: 全 0 初始化

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
            qe_wl_c.ctypes.data_as(POINTER(c_double)) if qe_wl_c is not None else None,
            qe_trans_c.ctypes.data_as(POINTER(c_double)) if qe_trans_c is not None else None,
            qe_count,
            crval1, crval2, crpix1, crpix2,
            cd11, cd12, cd21, cd22,
            sip_order,
            sip_a_c.ctypes.data_as(POINTER(c_double)) if sip_a_c is not None else None,
            sip_b_c.ctypes.data_as(POINTER(c_double)) if sip_b_c is not None else None,
            sip_ap_c.ctypes.data_as(POINTER(c_double)) if sip_ap_c is not None else None,
            sip_bp_c.ctypes.data_as(POINTER(c_double)) if sip_bp_c is not None else None,
            out_pixels.ctypes.data_as(POINTER(c_float)),
            byref(n_matched), byref(scale_factor),
            byref(sigma_residual),
            byref(diag),
        )

        if ret != 0:
            raise RuntimeError(f"pc_calibrate_simple 失败, 返回码={ret}")

        # ---- 重塑输出 ----
        out_pixels = out_pixels.reshape(height, width)
        logger.info("测光校准完成: n_matched=%d, scale=%.6e, sigma_residual=%.6f",
                    n_matched.value, scale_factor.value, sigma_residual.value)
        logger.info("P12-001 diag: %s", diag.to_dict())
        return out_pixels, n_matched.value, scale_factor.value, sigma_residual.value, diag

    def calibrate_with_gaia(
        self,
        gaia_client_handle,
        ra_center: float, dec_center: float, radius_deg: float,
        mag_min: float, mag_max: float,
        filter_wl: np.ndarray, filter_trans: np.ndarray,
        spectrum_wl: np.ndarray,
        pixels: np.ndarray,
        psf_cx: np.ndarray, psf_cy: np.ndarray,
        psf_flux: np.ndarray, psf_status: np.ndarray,
        crval1: float, crval2: float,
        crpix1: float, crpix2: float,
        cd11: float, cd12: float, cd21: float, cd22: float,
        sip_order: int = 0,
        sip_a: Optional[np.ndarray] = None,
        sip_b: Optional[np.ndarray] = None,
        sip_ap: Optional[np.ndarray] = None,
        sip_bp: Optional[np.ndarray] = None,
        qe_wl: Optional[np.ndarray] = None,
        qe_trans: Optional[np.ndarray] = None,
    ) -> Tuple[np.ndarray, int, float, float, PhotometricDiag]:
        """用 gaia_client handle 调用新 DLL 接口 pc_calibrate_simple_with_gaia

        DLL 内部完成: 锥形搜索 Gaia DR3SP -> BP/RP 光谱 Akima+Simpson 积分 F_syn ->
                      WCS 投影 -> KDTree 匹配 PSF 星 -> MAD 清洗 -> 全局 scale 校正

        Args:
            gaia_client_handle: gaia_client_create_ex 返回的 handle (int 或 c_void_p)
            ra_center, dec_center: 锥形搜索中心 (度)
            radius_deg: 锥形搜索半径 (度)
            mag_min, mag_max: 星等范围
            filter_wl, filter_trans: 滤光片波长(nm)与透过率[0,1] float64 数组
            spectrum_wl: 光谱波长数组 [336, 338, ..., 1020] nm float64 数组
            pixels: 图像像素 float32 [H, W] (2D)
            psf_cx/cy/flux: PSF 星数组 float64 [n_psf]
            psf_status: PSF 星状态 int32 [n_psf] (0=成功)
            WCS参数: crval1/2, crpix1/2, cd11/12/21/22
            sip_order: SIP 阶数 (0=无SIP)
            sip_a/b/ap/bp: SIP 系数数组 float64 [36] (按 i*6+j 索引)
            qe_wl: GAP-012 CCD QE 波长数组 [n_qe] (nm), None 时 Q(λ)=1.0
            qe_trans: CCD QE 透过率数组 [n_qe] [0,1]

        Returns:
            (out_pixels, n_matched, scale_factor, sigma_residual, diag)
            out_pixels: 校正后图像 float32 [H, W]
            n_matched: 匹配星数 (MAD清洗后)
            scale_factor: scale 因子
            sigma_residual: MAD/0.6745 (供 SNR 模块 §14 计算 SNR_phot)
            diag: P12-001 PhotometricDiag 分阶段诊断结构体 (20字段, 完整填充 8 个阶段)

        Raises:
            RuntimeError: DLL 调用失败 (ret != 0)
            ValueError: gaia_client_handle 为空或 pixels 维度错误
        """
        # ---- 输入校验与类型转换 ----
        if gaia_client_handle is None:
            raise ValueError("gaia_client_handle 不能为 None")

        pixels = np.ascontiguousarray(pixels, dtype=np.float32)
        if pixels.ndim != 2:
            raise ValueError(f"pixels 必须为 2D, 实际为 {pixels.ndim}D")
        height, width = pixels.shape

        filter_wl = np.ascontiguousarray(filter_wl, dtype=np.float64)
        filter_trans = np.ascontiguousarray(filter_trans, dtype=np.float64)
        filter_count = filter_wl.size
        if filter_count == 0:
            raise ValueError("filter_wl 不能为空")

        spectrum_wl = np.ascontiguousarray(spectrum_wl, dtype=np.float64)
        spectrum_count = spectrum_wl.size
        if spectrum_count == 0:
            raise ValueError("spectrum_wl 不能为空")

        psf_cx = np.ascontiguousarray(psf_cx, dtype=np.float64)
        psf_cy = np.ascontiguousarray(psf_cy, dtype=np.float64)
        psf_flux = np.ascontiguousarray(psf_flux, dtype=np.float64)
        psf_status = np.ascontiguousarray(psf_status, dtype=np.int32)
        n_psf = psf_cx.size

        # ---- SIP 系数 ----
        def _prep_sip(arr):
            if arr is None:
                return None
            return np.ascontiguousarray(arr, dtype=np.float64)

        sip_a_c = _prep_sip(sip_a)
        sip_b_c = _prep_sip(sip_b)
        sip_ap_c = _prep_sip(sip_ap)
        sip_bp_c = _prep_sip(sip_bp)

        # ---- GAP-012: QE 曲线 ----
        qe_wl_c = _prep_sip(qe_wl)
        qe_trans_c = _prep_sip(qe_trans)
        qe_count = qe_wl_c.size if qe_wl_c is not None else 0

        # ---- handle 转换 (int 或 c_void_p 均可, ctypes 自动处理) ----
        if isinstance(gaia_client_handle, c_void_p):
            handle_param = gaia_client_handle
        else:
            handle_param = c_void_p(gaia_client_handle)

        # ---- 输出缓冲 ----
        out_pixels = np.zeros(width * height, dtype=np.float32)
        n_matched = c_int(0)
        scale_factor = c_double(0.0)
        sigma_residual = c_double(0.0)
        diag = PhotometricDiag()  # P12-001: 全 0 初始化

        logger.info(
            "调用 pc_calibrate_simple_with_gaia: center=(%.6f, %.6f), r=%.4f°, "
            "mag=[%.1f, %.1f], filter=%dpts, spectrum=%dpts, psf=%d颗, %dx%d, qe=%dpts",
            ra_center, dec_center, radius_deg, mag_min, mag_max,
            filter_count, spectrum_count, n_psf, width, height, qe_count)

        # ---- 调用 C 函数 ----
        ret = self._dll.pc_calibrate_simple_with_gaia(
            handle_param,
            ra_center, dec_center, radius_deg,
            mag_min, mag_max,
            filter_wl.ctypes.data_as(POINTER(c_double)),
            filter_trans.ctypes.data_as(POINTER(c_double)),
            filter_count,
            qe_wl_c.ctypes.data_as(POINTER(c_double)) if qe_wl_c is not None else None,
            qe_trans_c.ctypes.data_as(POINTER(c_double)) if qe_trans_c is not None else None,
            qe_count,
            spectrum_wl.ctypes.data_as(POINTER(c_double)),
            spectrum_count,
            pixels.ctypes.data_as(POINTER(c_float)), width, height,
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
            byref(sigma_residual),
            byref(diag),
        )

        if ret != 0:
            raise RuntimeError(
                f"pc_calibrate_simple_with_gaia 失败, 返回码={ret} "
                f"(-1=参数无效, -2=handle为空, -3=锥形搜索失败或无光谱星)")

        # ---- 重塑输出 ----
        out_pixels = out_pixels.reshape(height, width)
        logger.info("测光校准(带Gaia)完成: n_matched=%d, scale=%.6e, sigma_residual=%.6f",
                    n_matched.value, scale_factor.value, sigma_residual.value)
        logger.info("P12-001 diag: %s", diag.to_dict())
        return out_pixels, n_matched.value, scale_factor.value, sigma_residual.value, diag


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
    out_img, n_matched, scale, sigma_residual, diag = pc.calibrate_simple(
        image, gaia_ra, gaia_dec, gaia_mag, gaia_fsyn,
        psf_cx, psf_cy, psf_flux, psf_status,
        crval1, crval2, crpix1, crpix2,
        cd_val, 0.0, 0.0, cd_val,
    )

    print(f"n_matched = {n_matched} (期望 10)")
    print(f"scale = {scale:.6e} (期望 ~10.0, 因 F_syn/F_instr = 50000/5000 = 10)")
    print(f"sigma_residual = {sigma_residual:.6f}")
    print(f"out_img[0,0] = {out_img[0, 0]:.4f} (期望 ~10000.0)")
    print(f"out_img shape = {out_img.shape}")

    ok = (n_matched == 10 and abs(scale - 10.0) < 0.1
          and abs(out_img[0, 0] - 10000.0) < 100.0)
    print(f"[{'PASS' if ok else 'FAIL'}] DLL封装测试")
