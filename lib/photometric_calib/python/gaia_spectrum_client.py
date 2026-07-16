# -*- coding: utf-8 -*-
"""
Gaia Spectrum Client - Gaia 光谱数据库 Python 客户端
功能: 封装 gaia_client.dll 的 C API，提供锥形搜索 + BP/RP 光谱数据获取
用途: 为 photometric_calib 模块提供真实 Gaia DR3SP 光谱数据源，
      支持 336-1020nm 范围 343 个采样点的 BP/RP 光谱查询，用于合成测光与 SED 验证
迁移日期: 2026-07-16 (架构重构 spec G5 Phase 3)
迁移来源: lib/photometric_calib/spectrum_integrator/python/gaia_spectrum_client.py
路径调整: _find_dll() 从回溯3级 (spectrum_integrator/python/ -> lib) 改为回溯2级 (python/ -> lib)
依赖: ctypes (调用 DLL), numpy (光谱数组), logging (日志)
调用: from gaia_spectrum_client import GaiaSpectrumClient, GaiaSpectrumStarPy
      client = GaiaSpectrumClient(data_dir=".../GaiaDR3SP", db_type=2)
      stars = client.cone_search_with_spectrum(ra, dec, radius, mag_low, mag_high)
      wl = client.get_wavelength_array()  # [336, 338, ..., 1020]

C API 对应 (gaia_client.h):
  - gaia_client_create_ex / gaia_client_destroy          客户端创建/销毁
  - gaia_client_cone_search_with_spectrum                锥形搜索+光谱
  - gaia_client_get_spectrum_params                      光谱参数 (336/2/343)
光谱布局: out_spectra 是 flat uint8 数组, 第 i 颗星第 j 点 = out_spectra[i*count + j]
波长数组: wl[j] = start_nm + j * step_nm = 336 + j * 2  (j=0..342)
"""

from __future__ import annotations

import ctypes
import logging
import os
from dataclasses import dataclass
from typing import List, Optional, Tuple

import numpy as np

logger = logging.getLogger(__name__)

# GaiaDbType 枚举 (对应 C 端 GaiaDbType)
GAIA_DB_AUTO = 0
GAIA_DB_DR3 = 1
GAIA_DB_DR3SP = 2


# ============================================================================
# 数据结构
# ============================================================================

@dataclass
class GaiaSpectrumStarPy:
    """Gaia 光谱星点 (Python 端数据结构)

    对应 C 端 GaiaSpectrumStar，额外携带 BP/RP 光谱数组。
    光谱 API 不返回 source_id/BP/RP 星等，故 source_id 默认 0。
    """
    ra: float = 0.0                              # 赤经 (度)
    dec: float = 0.0                             # 赤纬 (度)
    mag_g: float = 0.0                           # G 波段星等
    source_id: int = 0                           # Gaia source_id (光谱 API 不返回)
    spectrum: Optional[np.ndarray] = None        # BP/RP 光谱 uint8[343]


# ============================================================================
# ctypes 结构体定义
# ============================================================================

class _GaiaSpectrumStar(ctypes.Structure):
    """对应 C 端 GaiaSpectrumStar 结构体 (24 字节): ra, dec, magG"""
    _fields_ = [
        ("ra", ctypes.c_double),
        ("dec", ctypes.c_double),
        ("magG", ctypes.c_double),
    ]


# ============================================================================
# DLL 加载 (模块级单例，避免重复加载)
# ============================================================================

def _find_dll() -> str:
    """查找 gaia_client.dll，按候选路径优先级搜索"""
    module_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        # 相对当前文件回溯二级: python -> photometric_calib -> lib
        # 原 spectrum_integrator/python/ 回溯三级，现 python/ 回溯二级
        os.path.join(module_dir, "..", "..", "gaia_xpsd_client", "gaia_client.dll"),
        # 项目根目录绝对路径 (兜底)
        os.path.join(r"F:\Astro dev\Astro CS Normalization Database",
                     "lib", "gaia_xpsd_client", "gaia_client.dll"),
    ]
    for c in candidates:
        p = os.path.normpath(c)
        if os.path.exists(p):
            return p
    raise FileNotFoundError("未找到 gaia_client.dll")


def _load_dll(dll_path: str) -> ctypes.CDLL:
    """加载 DLL 并声明函数签名"""
    # mingw 运行时路径 (DLL 依赖 libgcc/libstdc++)
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

    lib = ctypes.CDLL(dll_path)

    # 创建/销毁客户端
    lib.gaia_client_create_ex.argtypes = [ctypes.c_char_p, ctypes.c_int]
    lib.gaia_client_create_ex.restype = ctypes.c_void_p

    lib.gaia_client_destroy.argtypes = [ctypes.c_void_p]
    lib.gaia_client_destroy.restype = None

    # 带光谱的锥形搜索
    lib.gaia_client_cone_search_with_spectrum.argtypes = [
        ctypes.c_void_p,                                    # client
        ctypes.c_double,                                    # ra
        ctypes.c_double,                                    # dec
        ctypes.c_double,                                    # radius_deg
        ctypes.c_double,                                    # mag_low
        ctypes.c_double,                                    # mag_high
        ctypes.POINTER(ctypes.POINTER(_GaiaSpectrumStar)),  # out_stars (C 端 malloc)
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),     # out_spectra (C 端 malloc, flat)
        ctypes.POINTER(ctypes.c_int),                       # out_count
    ]
    lib.gaia_client_cone_search_with_spectrum.restype = ctypes.c_int

    # 获取光谱参数
    lib.gaia_client_get_spectrum_params.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_int),   # out_start_nm
        ctypes.POINTER(ctypes.c_int),   # out_step_nm
        ctypes.POINTER(ctypes.c_int),   # out_count
    ]
    lib.gaia_client_get_spectrum_params.restype = ctypes.c_int

    # 批量坐标->光谱查询 (V2 新增)
    lib.gaia_client_query_spectrum_by_coords.argtypes = [
        ctypes.c_void_p,                                    # client
        ctypes.POINTER(ctypes.c_double),                    # ra_list
        ctypes.POINTER(ctypes.c_double),                    # dec_list
        ctypes.c_int,                                       # n_coords
        ctypes.c_double,                                    # match_radius_arcsec
        ctypes.c_double,                                    # mag_low
        ctypes.c_double,                                    # mag_high
        ctypes.POINTER(ctypes.POINTER(_GaiaSpectrumStar)),  # out_stars (C 端 malloc)
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),     # out_spectra (C 端 malloc, flat)
        ctypes.POINTER(ctypes.POINTER(ctypes.c_int)),       # out_match_idx (C 端 malloc)
        ctypes.POINTER(ctypes.c_int),                       # out_count
    ]
    lib.gaia_client_query_spectrum_by_coords.restype = ctypes.c_int

    return lib


_dll: Optional[ctypes.CDLL] = None
_msvcrt: Optional[ctypes.CDLL] = None


def _get_dll() -> ctypes.CDLL:
    """获取模块级单例 DLL"""
    global _dll
    if _dll is None:
        dll_path = _find_dll()
        logger.info("加载 gaia_client.dll: %s", dll_path)
        _dll = _load_dll(dll_path)
    return _dll


def _get_msvcrt() -> ctypes.CDLL:
    """获取 msvcrt (用于释放 C 端 malloc 的内存)"""
    global _msvcrt
    if _msvcrt is None:
        _msvcrt = ctypes.CDLL("msvcrt.dll")
    return _msvcrt


# ============================================================================
# GaiaSpectrumClient
# ============================================================================

class GaiaSpectrumClient:
    """Gaia 光谱数据库 Python 客户端

    封装 gaia_client.dll，提供锥形搜索与 BP/RP 光谱数据获取。
    光谱范围 336-1020nm，343 个采样点，步长 2nm，值为 uint8 (0-255)。

    用法:
        with GaiaSpectrumClient(".../GaiaDR3SP", db_type=2) as client:
            stars = client.cone_search_with_spectrum(266.4, -29.0, 0.5, 8.0, 16.0)
            wl = client.get_wavelength_array()
    """

    def __init__(self, data_dir: str, db_type: int = GAIA_DB_DR3SP):
        """加载 DLL 并创建 GaiaClient

        Args:
            data_dir: 数据库目录路径 (如 ".../GaiaDR3SP")
            db_type: 数据库类型 (0=AUTO, 1=DR3, 2=DR3SP)，默认 DR3SP
        """
        self._lib = _get_dll()
        self._msvcrt = _get_msvcrt()
        data_dir_bytes = data_dir.encode("utf-8")
        logger.info("创建 GaiaClient: data_dir=%s, db_type=%d", data_dir, db_type)
        self._client = self._lib.gaia_client_create_ex(data_dir_bytes, db_type)
        if not self._client:
            raise RuntimeError(f"GaiaClient 创建失败: data_dir={data_dir}, db_type={db_type}")
        self._closed = False
        logger.info("GaiaClient 创建成功")

    def cone_search_with_spectrum(self, ra: float, dec: float, radius_deg: float,
                                  mag_low: float, mag_high: float) -> List[GaiaSpectrumStarPy]:
        """锥形搜索带光谱数据

        Args:
            ra: 中心赤经 (度)
            dec: 中心赤纬 (度)
            radius_deg: 搜索半径 (度)
            mag_low: 星等下限
            mag_high: 星等上限

        Returns:
            GaiaSpectrumStarPy 列表，每颗星含 ra/dec/mag_g/spectrum(uint8[343])

        Raises:
            RuntimeError: C 端搜索失败 (ret != 0)
        """
        out_stars = ctypes.POINTER(_GaiaSpectrumStar)()
        out_spectra = ctypes.POINTER(ctypes.c_uint8)()
        out_count = ctypes.c_int(0)

        logger.info("锥形搜索: RA=%.6f, Dec=%.6f, r=%.4f, mag=[%.1f, %.1f]",
                    ra, dec, radius_deg, mag_low, mag_high)
        ret = self._lib.gaia_client_cone_search_with_spectrum(
            self._client, ra, dec, radius_deg, mag_low, mag_high,
            ctypes.byref(out_stars), ctypes.byref(out_spectra), ctypes.byref(out_count))

        if ret != 0:
            logger.error("锥形搜索失败, 错误码=%d", ret)
            raise RuntimeError(f"锥形搜索失败, 错误码={ret}")

        count = out_count.value
        logger.info("锥形搜索完成, 星数=%d", count)

        if count <= 0:
            return []

        _, _, spec_count = self.get_spectrum_params()
        results: List[GaiaSpectrumStarPy] = []
        spectra_base = ctypes.addressof(out_spectra.contents)

        for i in range(count):
            star = out_stars[i]
            # 提取第 i 颗星光谱: [i*spec_count, (i+1)*spec_count) 的 flat 区段
            spectrum = np.frombuffer(
                (ctypes.c_uint8 * spec_count).from_address(
                    spectra_base + i * spec_count
                ),
                dtype=np.uint8
            ).copy()  # copy 使数据脱离 C 端内存，独立持有

            results.append(GaiaSpectrumStarPy(
                ra=star.ra, dec=star.dec, mag_g=star.magG,
                spectrum=spectrum
            ))

        # 释放 C 端 malloc 的星表与光谱内存
        self._msvcrt.free(out_stars)
        self._msvcrt.free(out_spectra)
        logger.info("已释放 C 端星表/光谱内存")

        return results

    def get_spectrum_params(self) -> Tuple[int, int, int]:
        """获取光谱参数

        Returns:
            (start_nm, step_nm, count) = (336, 2, 343)

        Raises:
            RuntimeError: 当前数据库不支持光谱 (ret != 1)
        """
        start_nm = ctypes.c_int(0)
        step_nm = ctypes.c_int(0)
        count = ctypes.c_int(0)
        ret = self._lib.gaia_client_get_spectrum_params(
            self._client, ctypes.byref(start_nm), ctypes.byref(step_nm), ctypes.byref(count))
        if ret != 1:
            raise RuntimeError(f"获取光谱参数失败 (当前数据库无光谱), ret={ret}")
        return start_nm.value, step_nm.value, count.value

    def get_wavelength_array(self) -> np.ndarray:
        """返回波长数组 [336, 338, ..., 1020] nm，共 343 个点 (float64)"""
        start_nm, step_nm, count = self.get_spectrum_params()
        wl = start_nm + np.arange(count, dtype=np.float64) * step_nm
        return wl

    def query_spectrum_by_coords(
        self,
        ra_list: List[float],
        dec_list: List[float],
        match_radius_arcsec: float = 3.0,
        mag_low: float = 0.0,
        mag_high: float = 22.0,
    ) -> List[Optional[GaiaSpectrumStarPy]]:
        """批量坐标->光谱查询

        对每个输入坐标，在四叉树中查找最近邻星（角距离 < match_radius_arcsec），
        返回匹配星的 BP/RP 光谱。C 端使用 OpenMP 16 线程并行搜索各坐标。

        Args:
            ra_list: RA 列表 (度)
            dec_list: Dec 列表 (度)
            match_radius_arcsec: 匹配半径 (角秒), 默认 3.0
            mag_low: 星等下界
            mag_high: 星等上界

        Returns:
            长度等于输入坐标数的列表，每项为匹配到的 GaiaSpectrumStarPy 或 None(未匹配)
        """
        n = len(ra_list)
        if n == 0:
            return []

        ra_arr = (ctypes.c_double * n)(*ra_list)
        dec_arr = (ctypes.c_double * n)(*dec_list)

        out_stars = ctypes.POINTER(_GaiaSpectrumStar)()
        out_spectra = ctypes.POINTER(ctypes.c_uint8)()
        out_match_idx = ctypes.POINTER(ctypes.c_int)()
        out_count = ctypes.c_int(0)

        logger.info(
            "批量坐标查询: n_coords=%d, radius=%.1f arcsec, mag=[%.1f, %.1f]",
            n, match_radius_arcsec, mag_low, mag_high,
        )
        ret = self._lib.gaia_client_query_spectrum_by_coords(
            self._client, ra_arr, dec_arr, n,
            match_radius_arcsec, mag_low, mag_high,
            ctypes.byref(out_stars), ctypes.byref(out_spectra),
            ctypes.byref(out_match_idx), ctypes.byref(out_count),
        )

        if ret != 0:
            logger.error("批量坐标查询失败, 错误码=%d", ret)
            raise RuntimeError(f"query_spectrum_by_coords 失败, 错误码={ret}")

        count = out_count.value
        logger.info("批量坐标查询完成: 匹配=%d/%d", count, n)

        _, _, spec_count = self.get_spectrum_params()
        spectra_base = ctypes.addressof(out_spectra.contents) if out_spectra else 0

        results: List[Optional[GaiaSpectrumStarPy]] = [None] * n
        for i in range(n):
            idx = out_match_idx[i]
            if idx >= 0 and idx < count:
                star = out_stars[idx]
                spectrum = np.frombuffer(
                    (ctypes.c_uint8 * spec_count).from_address(
                        spectra_base + idx * spec_count
                    ),
                    dtype=np.uint8,
                ).copy()
                results[i] = GaiaSpectrumStarPy(
                    ra=star.ra, dec=star.dec, mag_g=star.magG,
                    spectrum=spectrum,
                )

        self._msvcrt.free(out_stars)
        self._msvcrt.free(out_spectra)
        self._msvcrt.free(out_match_idx)
        logger.info("已释放 C 端星表/光谱/索引内存")

        return results

    def close(self):
        """销毁 GaiaClient，释放资源"""
        if not self._closed and self._client:
            self._lib.gaia_client_destroy(self._client)
            self._client = None
            self._closed = True
            logger.info("GaiaClient 已销毁")

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
