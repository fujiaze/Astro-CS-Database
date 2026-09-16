# -*- coding: utf-8 -*-
"""HiPS 直写冒烟测试 (Phase1 Final Closure V3).
通过 AIO 流式 API 写合成 tile, 再用 astropy (独立 reader) 验证结构。
"""

import ctypes
import json
import math
import os
import sys

import numpy as np

ROOT = r"F:\Astro dev\Astro CS Normalization Database"


def add_dll_dirs():
    for d in (ROOT + r"\lib\astro_image_io", r"C:\msys64\mingw64\bin"):
        if os.path.isdir(d):
            os.add_dll_directory(d)
            os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")


class AstroSphereTileView(ctypes.Structure):
    _fields_ = [("parent_ipix", ctypes.c_uint64),
                ("leaf_order", ctypes.c_uint32),
                ("width", ctypes.c_uint32),
                ("data_type", ctypes.c_int32),
                ("flux_sum", ctypes.c_void_p),
                ("covered_area", ctypes.c_void_p),
                ("valid_mask", ctypes.c_void_p)]


class AioHipsSnrPoint(ctypes.Structure):
    _fields_ = [("ra_deg", ctypes.c_double),
                ("dec_deg", ctypes.c_double),
                ("snr", ctypes.c_double),
                ("source_id", ctypes.c_int64)]


def main():
    add_dll_dirs()
    aio = ctypes.CDLL(ROOT + r"\lib\astro_image_io\astro_image_io.dll")
    aio.aio_hips_product_begin.restype = ctypes.c_void_p
    aio.aio_hips_product_begin.argtypes = [
        ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int32,
        ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
        ctypes.c_double, ctypes.c_char_p, ctypes.c_uint32]
    aio.aio_hips_write_signal_support_tile.restype = ctypes.c_int
    aio.aio_hips_write_signal_support_tile.argtypes = [ctypes.c_void_p,
                                                       ctypes.POINTER(AstroSphereTileView)]
    aio.aio_hips_write_snr_points.restype = ctypes.c_int
    aio.aio_hips_write_snr_points.argtypes = [ctypes.c_void_p,
                                              ctypes.POINTER(AioHipsSnrPoint), ctypes.c_int]
    aio.aio_hips_finalize.restype = ctypes.c_int
    aio.aio_hips_finalize.argtypes = [ctypes.c_void_p]
    aio.aio_hips_last_error.restype = ctypes.c_char_p
    aio.aio_hips_last_error.argtypes = []

    out = sys.argv[1] if len(sys.argv) > 1 else r"run\temp\hips_smoke"
    nside = 2048          # leaf order 11, tile order 2
    leaf_order = 11
    tile_order = 2
    a_cell = 4.0 * math.pi / (12.0 * nside * nside)

    ps = aio.aio_hips_product_begin(
        out.encode(), nside, 512, 0, 7,
        b"ivo://astrocs/test", b"Smoke HiPS", b"L", 300.0, b"2026-08-08", 0)
    if not ps:
        print("begin failed:", aio.aio_hips_last_error().decode())
        return 1

    n = 512 * 512
    # 两个 tile: 0 号 (北极区) 和 1 号, 合成常量信号 + 部分覆盖
    for tile_ipix in (0, 1):
        flux = np.full(n, 0.0, dtype=np.float32)
        area = np.zeros(n, dtype=np.float32)
        # 中心 100x100 方块有数据: flux=2.0 (sr), area=0.5*A_cell... 直接用面积
        cy, cx = 256, 256
        for dy in range(-50, 50):
            for dx in range(-50, 50):
                i = (cy + dy) * 512 + (cx + dx)
                flux[i] = 2.0
                area[i] = 0.5 * a_cell
        view = AstroSphereTileView()
        view.parent_ipix = tile_ipix
        view.leaf_order = leaf_order
        view.width = 512
        view.data_type = 0
        view.flux_sum = flux.ctypes.data_as(ctypes.c_void_p)
        view.covered_area = area.ctypes.data_as(ctypes.c_void_p)
        view.valid_mask = None
        rc = aio.aio_hips_write_signal_support_tile(ps, ctypes.byref(view))
        if rc != 0:
            print("write tile failed:", rc, aio.aio_hips_last_error().decode())
            return 2

    pts = (AioHipsSnrPoint * 3)(
        AioHipsSnrPoint(10.0, 89.5, 12.3, 1001),
        AioHipsSnrPoint(20.0, 89.7, 8.1, 1002),
        AioHipsSnrPoint(30.0, -45.0, 5.5, 1003))
    rc = aio.aio_hips_write_snr_points(ps, pts, 3)
    if rc != 0:
        print("snr failed:", rc)
        return 3
    rc = aio.aio_hips_finalize(ps)
    if rc != 0:
        print("finalize failed:", rc, aio.aio_hips_last_error().decode())
        return 4
    print("finalize ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
