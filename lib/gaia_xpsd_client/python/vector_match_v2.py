# -*- coding: utf-8 -*-
"""
Vector Match V2 - Gaia 数据库 Python 客户端绑定
功能: 封装 gaia_client.dll 的 C API，提供批量坐标->光谱查询接口
用途: 为光谱定标模块提供基于坐标列表的 Gaia DR3SP BP/RP 光谱批量查询能力，
      支持 336-1020nm 范围 343 个采样点，小半径最近邻匹配
依赖: ctypes (调用 DLL), numpy (光谱数组), gaia_spectrum_client (GaiaSpectrumStarPy 数据结构)
调用: from vector_match_v2 import GaiaClientPy
      client = GaiaClientPy('GaiaDR3SP', db_type=2)
      stars = client.query_spectrum_by_coords([266.4], [-29.0], match_radius_arcsec=3.0)
"""

from __future__ import annotations

import ctypes
import logging
import os
from typing import List, Optional, Tuple

import numpy as np

from gaia_spectrum_client import GaiaSpectrumStarPy

logger = logging.getLogger(__name__)

GAIA_DB_AUTO = 0
GAIA_DB_DR3 = 1
GAIA_DB_DR3SP = 2


class _GaiaSpectrumStar(ctypes.Structure):
    """对应 C 端 GaiaSpectrumStar 结构体: ra, dec, magG"""
    _fields_ = [
        ("ra", ctypes.c_double),
        ("dec", ctypes.c_double),
        ("magG", ctypes.c_double),
    ]


def _find_dll() -> str:
    """查找 gaia_client.dll"""
    module_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(module_dir, "..", "gaia_client.dll"),
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

    lib.gaia_client_create.argtypes = [ctypes.c_char_p]
    lib.gaia_client_create.restype = ctypes.c_void_p
    lib.gaia_client_create_ex.argtypes = [ctypes.c_char_p, ctypes.c_int]
    lib.gaia_client_create_ex.restype = ctypes.c_void_p
    lib.gaia_client_destroy.argtypes = [ctypes.c_void_p]
    lib.gaia_client_destroy.restype = None

    lib.gaia_client_query_spectrum_by_coords.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_double),
        ctypes.POINTER(ctypes.c_double),
        ctypes.c_int,
        ctypes.c_double,
        ctypes.c_double,
        ctypes.c_double,
        ctypes.POINTER(ctypes.POINTER(_GaiaSpectrumStar)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_int)),
        ctypes.POINTER(ctypes.c_int),
    ]
    lib.gaia_client_query_spectrum_by_coords.restype = ctypes.c_int

    lib.gaia_client_get_spectrum_params.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
    ]
    lib.gaia_client_get_spectrum_params.restype = ctypes.c_int

    return lib


class GaiaClientPy:
    """Gaia 数据库 Python 客户端封装

    封装 gaia_client.dll，提供批量坐标->光谱查询接口。
    支持 DR3 (无光谱) 和 DR3SP (含 BP/RP 光谱) 两种数据库类型。

    用法:
        with GaiaClientPy("GaiaDR3SP", db_type=2) as client:
            stars = client.query_spectrum_by_coords([266.4], [-29.0], match_radius_arcsec=3.0)
    """

    def __init__(self, data_dir: str, db_type: int = GAIA_DB_AUTO):
        dll_path = _find_dll()
        logger.info("加载 gaia_client.dll: %s", dll_path)
        self._dll = _load_dll(dll_path)
        self._msvcrt = ctypes.CDLL("msvcrt.dll")
        data_dir_bytes = data_dir.encode("utf-8")
        if db_type == GAIA_DB_AUTO:
            self._handle = self._dll.gaia_client_create(data_dir_bytes)
        else:
            self._handle = self._dll.gaia_client_create_ex(data_dir_bytes, db_type)
        if not self._handle:
            raise RuntimeError(f"Gaia客户端创建失败: {data_dir}")
        self._closed = False
        logger.info("GaiaClient 创建成功: data_dir=%s, db_type=%d", data_dir, db_type)

    def get_spectrum_params(self) -> Tuple[int, int, int]:
        """获取光谱参数

        Returns:
            (start_nm, step_nm, count) = (336, 2, 343)
        """
        start_nm = ctypes.c_int(0)
        step_nm = ctypes.c_int(0)
        count = ctypes.c_int(0)
        ret = self._dll.gaia_client_get_spectrum_params(
            self._handle, ctypes.byref(start_nm), ctypes.byref(step_nm), ctypes.byref(count))
        if ret != 1:
            raise RuntimeError(f"获取光谱参数失败 (当前数据库无光谱), ret={ret}")
        return start_nm.value, step_nm.value, count.value

    def query_spectrum_by_coords(
        self,
        ra_list: List[float],
        dec_list: List[float],
        match_radius_arcsec: float = 3.0,
        mag_low: float = 0.0,
        mag_high: float = 22.0,
    ) -> List[Optional[GaiaSpectrumStarPy]]:
        """批量坐标->光谱查询

        对每个输入坐标，在小半径内搜索最近的 Gaia 星并返回其 BP/RP 光谱。
        C 端使用 OpenMP 16 线程并行搜索各坐标。

        Args:
            ra_list: RA 列表 (度)
            dec_list: Dec 列表 (度)
            match_radius_arcsec: 匹配半径 (角秒), 默认 3.0
            mag_low: 星等下界
            mag_high: 星等上界

        Returns:
            每项为匹配到的 GaiaSpectrumStarPy 或 None (未匹配)
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

        logger.info("批量光谱查询: n_coords=%d, radius=%.1f arcsec, mag=[%.1f, %.1f]",
                    n, match_radius_arcsec, mag_low, mag_high)
        ret = self._dll.gaia_client_query_spectrum_by_coords(
            self._handle, ra_arr, dec_arr, n,
            match_radius_arcsec, mag_low, mag_high,
            ctypes.byref(out_stars), ctypes.byref(out_spectra),
            ctypes.byref(out_match_idx), ctypes.byref(out_count))

        if ret != 0:
            logger.error("批量光谱查询失败, 错误码=%d", ret)
            raise RuntimeError(f"query_spectrum_by_coords 失败, 错误码={ret}")

        count = out_count.value
        logger.info("批量光谱查询完成: 匹配=%d/%d", count, n)

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
                    dtype=np.uint8
                ).copy()
                results[i] = GaiaSpectrumStarPy(
                    ra=star.ra, dec=star.dec, mag_g=star.magG,
                    spectrum=spectrum
                )

        self._msvcrt.free(out_stars)
        self._msvcrt.free(out_spectra)
        self._msvcrt.free(out_match_idx)
        logger.info("已释放 C 端内存")

        return results

    def close(self):
        if not self._closed and self._handle:
            self._dll.gaia_client_destroy(self._handle)
            self._handle = None
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
