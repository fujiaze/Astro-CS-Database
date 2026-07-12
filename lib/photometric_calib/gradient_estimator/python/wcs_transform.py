# -*- coding: utf-8 -*-
"""
WCS 坐标转换模块
================
功能: 封装 astropy.wcs.WCS，提供天球坐标(RA/Dec)与像素坐标(x/y)之间的双向转换
用途: 在测光定标流程中，将 Gaia 星表的天球坐标映射到图像像素坐标，
      或将检测到的星点像素坐标反演为天球坐标，支持 SIP 多项式畸变修正
依赖: astropy (>=5.0), numpy
调用: from wcs_transform import WCSTransform
      t = WCSTransform(crpix1=100, crpix2=100, crval1=10, crval2=20,
                       cd11=0.01, cd12=0, cd21=0, cd22=0.01)
      x, y = t.sky_to_pixel(10, 20)      # 天球 -> 像素 (0-based)
      ra, dec = t.pixel_to_sky(99, 99)   # 像素 (0-based) -> 天球
作者: Astro CS Normalization Database
日期: 2026-07-10
"""

from __future__ import annotations

import logging
import os
from typing import Optional

import numpy as np
from astropy.wcs import WCS, Sip

logger = logging.getLogger(__name__)


class WCSTransform:
    """
    WCS 坐标转换器

    封装 astropy.wcs.WCS 对象，提供天球坐标 <-> 像素坐标转换。
    支持 TAN 投影 + SIP 多项式畸变修正。

    坐标约定:
        - CRPIX: 1-based FITS 约定 (与 astropy 一致)
        - 像素坐标输入/输出: 0-based (与 astropy wcs_world2pix/wcs_pix2world origin=0 一致)
        - 天球坐标: 度 (degrees)

    属性:
        wcs: 底层 astropy.wcs.WCS 对象
    """

    def __init__(self, crpix1, crpix2, crval1, crval2,
                 cd11, cd12, cd21, cd22,
                 sip_order=0, sip_a=None, sip_b=None,
                 sip_ap_order=0, sip_ap=None, sip_bp=None,
                 ctype1="RA---TAN", ctype2="DEC--TAN"):
        """
        构造 WCS 坐标转换器

        参数:
            crpix1, crpix2: 参考像素坐标 (1-based FITS 约定)
            crval1, crval2: 参考天球坐标 RA/Dec (度)
            cd11, cd12, cd21, cd22: CD 矩阵元素 (度/像素)
            sip_order: 前向 SIP 多项式阶数 (0=无 SIP)
            sip_a: 前向 SIP A 系数，长度 36 的扁平数组或 (order+1)x(order+1) 矩阵
            sip_b: 前向 SIP B 系数，同上
            sip_ap_order: 逆向 SIP 多项式阶数 (0=无逆向 SIP)
            sip_ap: 逆向 SIP AP 系数
            sip_bp: 逆向 SIP BP 系数
            ctype1: CTYPE1 字符串 (如 "RA---TAN" 或 "RA---TAN-SIP")
            ctype2: CTYPE2 字符串 (如 "DEC--TAN" 或 "DEC--TAN-SIP")
        """
        w = WCS(naxis=2)
        w.wcs.cd = [[float(cd11), float(cd12)],
                     [float(cd21), float(cd22)]]
        w.wcs.crval = [float(crval1), float(crval2)]
        w.wcs.crpix = [float(crpix1), float(crpix2)]

        self._has_sip = sip_order > 0

        if self._has_sip:
            # 有 SIP 时自动设置 -SIP 后缀的 ctype (如果调用方未指定)
            if "SIP" not in ctype1:
                ctype1 = "RA---TAN-SIP"
            if "SIP" not in ctype2:
                ctype2 = "DEC--TAN-SIP"

            a_mat = self._build_sip_matrix(sip_a, sip_order)
            b_mat = self._build_sip_matrix(sip_b, sip_order)

            if sip_ap_order > 0 and sip_ap is not None and sip_bp is not None:
                ap_mat = self._build_sip_matrix(sip_ap, sip_ap_order)
                bp_mat = self._build_sip_matrix(sip_bp, sip_ap_order)
                w.sip = Sip(a_mat, b_mat, ap_mat, bp_mat, w.wcs.crpix)
                logger.debug("WCS 含前向 SIP(order=%d) + 逆向 SIP(order=%d)",
                             sip_order, sip_ap_order)
            else:
                # 无逆向多项式，astropy 内部迭代求解
                w.sip = Sip(a_mat, b_mat, None, None, w.wcs.crpix)
                logger.debug("WCS 含前向 SIP(order=%d), 无逆向 SIP", sip_order)
        else:
            logger.debug("WCS 无 SIP (纯 TAN 投影)")

        w.wcs.ctype = [ctype1, ctype2]
        self.wcs = w

        # 缓存常用值
        self._crpix1 = float(crpix1)
        self._crpix2 = float(crpix2)
        self._crval1 = float(crval1)
        self._crval2 = float(crval2)
        self._sip_order = sip_order
        self._sip_ap_order = sip_ap_order

    @staticmethod
    def _build_sip_matrix(coeffs, order):
        """
        将 SIP 系数数组重塑为 (order+1)x(order+1) 矩阵

        系数索引约定: coeffs[i*(order+1)+j] 或 coeffs[i*6+j] 对应 dx^i * dy^j
        仅填充 i+j <= order 的有效项，其余为 0

        参数:
            coeffs: 长度 36 的扁平数组，或已是矩阵的 numpy 数组
            order: SIP 多项式阶数

        返回:
            (order+1) x (order+1) 的 numpy 数组
        """
        size = order + 1
        mat = np.zeros((size, size))

        if coeffs is None:
            return mat

        arr = np.asarray(coeffs, dtype=float)

        # 如果已经是矩阵形状，直接使用
        if arr.shape == (size, size):
            return arr

        # 扁平数组: 按 i*6+j 索引填充 (与 C 端 IpvWcsResult 一致)
        for i in range(size):
            for j in range(size - i):
                idx = i * 6 + j
                if idx < len(arr):
                    mat[i, j] = arr[idx]

        return mat

    @classmethod
    def from_ipv_result(cls, result):
        """
        从 IpvWcsResult ctypes Structure 构造 WCSTransform

        参数:
            result: IpvWcsResult 结构体 (来自 ipv_solver.py)

        返回:
            WCSTransform 实例
        """
        ctype1 = result.ctype1.decode('utf-8', errors='ignore').rstrip('\x00')
        ctype2 = result.ctype2.decode('utf-8', errors='ignore').rstrip('\x00')

        # ctype 为空时使用默认值
        if not ctype1:
            ctype1 = "RA---TAN-SIP" if result.sip_order > 0 else "RA---TAN"
        if not ctype2:
            ctype2 = "DEC--TAN-SIP" if result.sip_order > 0 else "DEC--TAN"

        cd11, cd12, cd21, cd22 = result.cd
        crval1, crval2 = result.crval
        crpix1, crpix2 = result.crpix

        logger.info("从 IpvWcsResult 构造 WCS: CRVAL=(%.6f, %.6f), CRPIX=(%.2f, %.2f), "
                    "sip_order=%d, sip_ap_order=%d",
                    crval1, crval2, crpix1, crpix2,
                    result.sip_order, result.sip_ap_order)

        return cls(
            crpix1=crpix1, crpix2=crpix2,
            crval1=crval1, crval2=crval2,
            cd11=cd11, cd12=cd12, cd21=cd21, cd22=cd22,
            sip_order=result.sip_order,
            sip_a=list(result.sip_a) if result.sip_order > 0 else None,
            sip_b=list(result.sip_b) if result.sip_order > 0 else None,
            sip_ap_order=result.sip_ap_order,
            sip_ap=list(result.sip_ap) if result.sip_ap_order > 0 else None,
            sip_bp=list(result.sip_bp) if result.sip_ap_order > 0 else None,
            ctype1=ctype1,
            ctype2=ctype2,
        )

    def sky_to_pixel(self, ra_deg, dec_deg):
        """
        天球坐标转像素坐标

        参数:
            ra_deg: 赤经 (度)
            dec_deg: 赤纬 (度)

        返回:
            (x, y) 像素坐标元组，0-based
        """
        world = np.array([[float(ra_deg), float(dec_deg)]])
        pix = self.wcs.all_world2pix(world, 0)
        return float(pix[0, 0]), float(pix[0, 1])

    def pixel_to_sky(self, x_px, y_px):
        """
        像素坐标转天球坐标

        参数:
            x_px: 像素 x 坐标 (0-based)
            y_px: 像素 y 坐标 (0-based)

        返回:
            (ra, dec) 天球坐标元组 (度)
        """
        pix = np.array([[float(x_px), float(y_px)]])
        world = self.wcs.all_pix2world(pix, 0)
        return float(world[0, 0]), float(world[0, 1])

    def sky_to_pixel_batch(self, ra_arr, dec_arr):
        """
        批量天球坐标转像素坐标

        参数:
            ra_arr: 赤经数组 (度)，支持 list 或 np.ndarray
            dec_arr: 赤纬数组 (度)，支持 list 或 np.ndarray

        返回:
            (x_array, y_array) 像素坐标元组，均为 0-based numpy 数组
        """
        ra = np.asarray(ra_arr, dtype=float)
        dec = np.asarray(dec_arr, dtype=float)
        world = np.column_stack([ra, dec])
        pix = self.wcs.all_world2pix(world, 0)
        return pix[:, 0], pix[:, 1]

    def pixel_to_sky_batch(self, x_arr, y_arr):
        """
        批量像素坐标转天球坐标

        参数:
            x_arr: 像素 x 坐标数组 (0-based)
            y_arr: 像素 y 坐标数组 (0-based)

        返回:
            (ra_array, dec_array) 天球坐标元组 (度)，均为 numpy 数组
        """
        x = np.asarray(x_arr, dtype=float)
        y = np.asarray(y_arr, dtype=float)
        pix = np.column_stack([x, y])
        world = self.wcs.all_pix2world(pix, 0)
        return world[:, 0], world[:, 1]

    @property
    def has_sip(self):
        """是否包含 SIP 畸变修正"""
        return self._has_sip


# ============================================================================
# 模块自测
# ============================================================================

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")

    print("=" * 60)
    print("WCSTransform 模块自测")
    print("=" * 60)

    # ---- 测试 1: 简单 TAN 投影 (无 SIP) ----
    print("\n[测试 1] 简单 TAN 投影 (无 SIP)")
    crpix1, crpix2 = 100.0, 100.0
    crval1, crval2 = 10.0, 20.0
    cd_val = 0.01  # 度/像素

    t = WCSTransform(
        crpix1=crpix1, crpix2=crpix2,
        crval1=crval1, crval2=crval2,
        cd11=cd_val, cd12=0.0, cd21=0.0, cd22=cd_val,
    )

    # 验证: sky_to_pixel(CRVAL) 应约等于 (CRPIX-1, CRPIX-1) (0-based)
    x, y = t.sky_to_pixel(crval1, crval2)
    expected_x = crpix1 - 1
    expected_y = crpix2 - 1
    print(f"  sky_to_pixel({crval1}, {crval2}) = ({x:.6f}, {y:.6f})")
    print(f"  期望 (CRPIX-1)            = ({expected_x}, {expected_y})")
    print(f"  误差: dx={abs(x - expected_x):.2e}, dy={abs(y - expected_y):.2e}")
    assert abs(x - expected_x) < 1e-10, "x 坐标误差过大"
    assert abs(y - expected_y) < 1e-10, "y 坐标误差过大"
    print("  [PASS] sky_to_pixel(CRVAL) == (CRPIX-1)")

    # 验证: pixel_to_sky(CRPIX-1) 应约等于 CRVAL
    ra, dec = t.pixel_to_sky(expected_x, expected_y)
    print(f"  pixel_to_sky({expected_x}, {expected_y}) = ({ra:.6f}, {dec:.6f})")
    print(f"  期望 CRVAL                      = ({crval1}, {crval2})")
    assert abs(ra - crval1) < 1e-10, "RA 误差过大"
    assert abs(dec - crval2) < 1e-10, "Dec 误差过大"
    print("  [PASS] pixel_to_sky(CRPIX-1) == CRVAL")

    # 验证: 往返一致性
    test_ra, test_dec = 10.05, 20.03
    x2, y2 = t.sky_to_pixel(test_ra, test_dec)
    ra2, dec2 = t.pixel_to_sky(x2, y2)
    print(f"  往返测试: ({test_ra}, {test_dec}) -> ({x2:.4f}, {y2:.4f}) -> ({ra2:.6f}, {dec2:.6f})")
    assert abs(ra2 - test_ra) < 1e-10 and abs(dec2 - test_dec) < 1e-10
    print("  [PASS] 往返一致性")

    # ---- 测试 2: 批量转换 ----
    print("\n[测试 2] 批量转换")
    ra_list = [10.0, 10.01, 10.02, 10.03]
    dec_list = [20.0, 20.01, 20.02, 20.03]
    xs, ys = t.sky_to_pixel_batch(ra_list, dec_list)
    print(f"  输入 RA:  {ra_list}")
    print(f"  输入 Dec: {dec_list}")
    print(f"  输出 x:   {xs}")
    print(f"  输出 y:   {ys}")
    # 第一个点应为 CRPIX-1
    assert abs(xs[0] - expected_x) < 1e-10 and abs(ys[0] - expected_y) < 1e-10
    print("  [PASS] 批量转换首点 == (CRPIX-1)")

    # 批量逆向
    ra_back, dec_back = t.pixel_to_sky_batch(xs, ys)
    assert np.allclose(ra_back, ra_list) and np.allclose(dec_back, dec_list)
    print("  [PASS] 批量往返一致")

    # ---- 测试 3: 带 SIP 的 WCS ----
    print("\n[测试 3] 带 SIP 的 WCS (二阶)")
    # 构造一个二阶 SIP: A[0][0]=0, A[1][0]=1e-5, A[0][1]=2e-5 等
    sip_order = 2
    sip_a = np.zeros(36)
    sip_b = np.zeros(36)
    # i*6+j 索引: A[1,0]=idx 6, A[0,1]=idx 1
    sip_a[6] = 1e-6   # A[1][0]
    sip_a[1] = 0.5e-6  # A[0][1]
    sip_b[6] = 0.5e-6  # B[1][0]
    sip_b[1] = 1e-6    # B[0][1]

    t_sip = WCSTransform(
        crpix1=crpix1, crpix2=crpix2,
        crval1=crval1, crval2=crval2,
        cd11=cd_val, cd12=0.0, cd21=0.0, cd22=cd_val,
        sip_order=sip_order,
        sip_a=sip_a, sip_b=sip_b,
    )
    print(f"  has_sip = {t_sip.has_sip}")
    assert t_sip.has_sip

    x_sip, y_sip = t_sip.sky_to_pixel(crval1, crval2)
    print(f"  sky_to_pixel(CRVAL) = ({x_sip:.6f}, {y_sip:.6f})")
    print(f"  期望 (CRPIX-1)       = ({expected_x}, {expected_y})")
    # 有 SIP 时在参考点仍应接近 CRPIX-1 (SIP 在参考点附近贡献极小)
    assert abs(x_sip - expected_x) < 1e-6 and abs(y_sip - expected_y) < 1e-6
    print("  [PASS] SIP WCS 参考点 sky_to_pixel(CRVAL) ~= (CRPIX-1)")

    # SIP 往返一致性
    x3, y3 = t_sip.sky_to_pixel(10.05, 20.03)
    ra3, dec3 = t_sip.pixel_to_sky(x3, y3)
    print(f"  SIP 往返: (10.05, 20.03) -> ({x3:.4f}, {y3:.4f}) -> ({ra3:.6f}, {dec3:.6f})")
    assert abs(ra3 - 10.05) < 1e-8 and abs(dec3 - 20.03) < 1e-8
    print("  [PASS] SIP 往返一致性")

    # ---- 测试 4: from_ipv_result (模拟) ----
    print("\n[测试 4] from_ipv_result (模拟 IpvWcsResult)")
    try:
        from ipv_solver import IpvWcsResult
        result = IpvWcsResult()
        result.cd = (cd_val, 0.0, 0.0, cd_val)
        result.crval = (crval1, crval2)
        result.crpix = (crpix1, crpix2)
        result.sip_order = 0
        result.sip_ap_order = 0
        result.ctype1 = b"RA---TAN"
        result.ctype2 = b"DEC--TAN"
        result.success = 1

        t_ipv = WCSTransform.from_ipv_result(result)
        x4, y4 = t_ipv.sky_to_pixel(crval1, crval2)
        print(f"  from_ipv_result -> sky_to_pixel(CRVAL) = ({x4:.6f}, {y4:.6f})")
        assert abs(x4 - expected_x) < 1e-10 and abs(y4 - expected_y) < 1e-10
        print("  [PASS] from_ipv_result 转换正确")
    except ImportError:
        print("  [SKIP] 无法导入 ipv_solver (需要同目录运行)")

    print("\n" + "=" * 60)
    print("所有测试通过!")
    print("=" * 60)
