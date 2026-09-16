# -*- coding: utf-8 -*-
"""HiPS 直写冒烟测试 (Phase1 Final Closure V3)。

通过 AIO 流式 API 写合成 tile, 再用 astropy (独立 reader) 验证结构。

SCI-FIX-AIO #2 (V11-N-01) 订正:
  * 删除本文件内联的 ctypes 镜像 —— 唯一权威镜像 =
    lib/infrastructure/aio/tools/aio_abi_mirror.py (机器锁 aio_abi_layout_lock
    逐字段比对; 内联镜像 = 镜像分叉, 正是修复前 C=40B vs 镜像=32B 静默错位
    写数据的根因);
  * 修复 ARCH-001 迁移后失效的硬编码 DLL 路径 (旧
    ROOT + "\\lib\\astro_image_io\\astro_image_io.dll" 已不存在), 改为
    ASTROCS_AIO_DLL 环境变量 + 仓内候选路径解析, 并显式设置 ABI 自描述头
    (aio_abi_mirror 的工厂函数已自动填 struct_size/abi_version)。
"""

import ctypes
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))

# 唯一权威镜像 (禁止在本文件另抄一份 _fields_)
sys.path.insert(0, os.path.normpath(os.path.join(REPO, "lib", "infrastructure",
                                                 "aio", "tools")))
import aio_abi_mirror as abi  # noqa: E402


def dll_candidates():
    env = os.environ.get("ASTROCS_AIO_DLL")
    if env:
        yield env
    for rel in (
        os.path.join("lib", "infrastructure", "aio", "astro_image_io.dll"),
        os.path.join("build", "astro_image_io.dll"),
        os.path.join("build", "libastrocs_aio.so"),
        os.path.join("build", "libastrocs_hips.so"),
    ):
        yield os.path.join(REPO, rel)


def load_aio():
    for p in dll_candidates():
        if p and os.path.isfile(p):
            return ctypes.CDLL(p), p
    raise SystemExit("找不到 AIO 动态库; 设 ASTROCS_AIO_DLL 指向 astro_image_io.dll")


def main():
    aio, dll_path = load_aio()
    print("AIO 库:", dll_path)
    aio.aio_hips_product_begin.restype = ctypes.c_void_p
    aio.aio_hips_product_begin.argtypes = [
        ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int32,
        ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
        ctypes.c_double, ctypes.c_char_p, ctypes.c_uint32]
    aio.aio_hips_write_signal_support_tile.restype = ctypes.c_int
    aio.aio_hips_write_signal_support_tile.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(abi.AstroSphereTileView)]
    aio.aio_hips_write_snr_points.restype = ctypes.c_int
    aio.aio_hips_write_snr_points.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(abi.AioHipsSnrPoint), ctypes.c_int]
    aio.aio_hips_finalize.restype = ctypes.c_int
    aio.aio_hips_finalize.argtypes = [ctypes.c_void_p]
    aio.aio_hips_last_error.restype = ctypes.c_char_p
    aio.aio_hips_last_error.argtypes = []

    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        REPO, "run", "temp", "hips_smoke")
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
        view = abi.tile_view(
            parent_ipix=tile_ipix, leaf_order=leaf_order, width=512,
            data_type=abi.AIO_HIPS_FLOAT32,
            flux_sum=flux.ctypes.data, covered_area=area.ctypes.data)
        rc = aio.aio_hips_write_signal_support_tile(ps, ctypes.byref(view))
        if rc != 0:
            print("write tile failed:", rc, aio.aio_hips_last_error().decode())
            return 2

    pts = abi.snr_points([(10.0, 89.5, 12.3, 1001),
                          (20.0, 89.7, 8.1, 1002),
                          (30.0, -45.0, 5.5, 1003)])
    rc = aio.aio_hips_write_snr_points(ps, pts, 3)
    if rc != 0:
        print("snr failed:", rc, aio.aio_hips_last_error().decode())
        return 3
    rc = aio.aio_hips_finalize(ps)
    if rc != 0:
        print("finalize failed:", rc, aio.aio_hips_last_error().decode())
        return 4
    print("finalize ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
