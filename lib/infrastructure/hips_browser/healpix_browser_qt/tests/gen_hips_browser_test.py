# -*- coding: utf-8 -*-
"""V4 G3 Browser HiPS 后端测试产品生成器.

生成 signal/support/snr 三子产品 (FP32/FP64 各一套), 供
test_hips_browser_backend.exe 与 browser_cli.exe --hips 使用。
通过 astro_image_io.dll 的 AIO HiPS Writer API (ctypes) 写入。

用法: py -3.12 gen_hips_browser_test.py <out_base> [--nside 2048]
"""
import argparse
import ctypes
import math
import os
import shutil
import sys

import numpy as np
import astropy_healpix as ah
import astropy.units as u



def add_dll_dirs():
    if not hasattr(os, "add_dll_directory"):   # POSIX: 无此 API
        return
    for d in (os.path.dirname(AIO_DLL), r"C:\msys64\mingw64\bin"):
        if os.path.isdir(d):
            os.add_dll_directory(d)
            os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")


# 唯一权威镜像 (SCI-FIX-AIO #2 / V11-N-01): 本文件不得再内联 _fields_ ——
# 内联镜像 = 镜像分叉 (修复前 C=40B vs 镜像=32B 静默错位写数据的根因)。
_HERE = Path(__file__).resolve().parent
_REPO = None
for _cand in [_HERE] + list(_HERE.parents):
    if (_cand / "lib" / "infrastructure" / "aio" / "include" / "aio_hips.h").is_file():
        _REPO = _cand
        break
if _REPO is None:
    raise SystemExit("找不到仓库根 (lib/infrastructure/aio/include/aio_hips.h)")
sys.path.insert(0, str(_REPO / "lib" / "infrastructure" / "aio" / "tools"))
import aio_abi_mirror as abi  # noqa: E402
AstroSphereTileView = abi.AstroSphereTileView
AioHipsSnrPoint = abi.AioHipsSnrPoint
ROOT = _REPO
AIO_DLL = os.environ.get("ASTROCS_AIO_DLL") or str(
    _REPO / "lib" / "infrastructure" / "aio" / "astro_image_io.dll")


def gen(out, dtype, nside, tile_ra_dec, pts):
    add_dll_dirs()
    aio = ctypes.CDLL(AIO_DLL)
    aio.aio_hips_product_begin.restype = ctypes.c_void_p
    aio.aio_hips_product_begin.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32,
                                           ctypes.c_int32, ctypes.c_int, ctypes.c_char_p,
                                           ctypes.c_char_p, ctypes.c_char_p, ctypes.c_double,
                                           ctypes.c_char_p, ctypes.c_uint32]
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

    tile_order = int(round(math.log2(nside))) - 9
    leaf_order = int(round(math.log2(nside)))
    dt = 1 if dtype == "f64" else 0
    npdt = np.float64 if dtype == "f64" else np.float32
    a_cell = 4.0 * math.pi / (12.0 * nside * nside)
    if os.path.isdir(out):
        shutil.rmtree(out)
    ps = aio.aio_hips_product_begin(
        out.encode(), nside, 512, dt, 7, b"ivo://astrocs/g3browser",
        b"V4 Browser backend test", b"L", 300.0, b"2026-08-09", 0)
    if not ps:
        print("begin failed:", aio.aio_hips_last_error().decode())
        return 1
    n = 512 * 512
    tile_ipixs = []
    for ti, (ra, dec) in enumerate(tile_ra_dec):
        t = int(ah.lonlat_to_healpix(ra * u.deg, dec * u.deg, 1 << tile_order,
                                     order="nested"))
        if t in tile_ipixs:
            continue
        tile_ipixs.append(t)
        flux = np.full(n, 0.0, dtype=npdt)
        area = np.zeros(n, dtype=npdt)
        flux[:] = 3.0 + 0.5 * len(tile_ipixs)
        area[:] = (0.75 + 0.05 * len(tile_ipixs)) * a_cell
        view = AstroSphereTileView()
        view.parent_ipix = t
        view.leaf_order = leaf_order
        view.width = 512
        view.data_type = dt
        view.flux_sum = flux.ctypes.data_as(ctypes.c_void_p)
        view.covered_area = area.ctypes.data_as(ctypes.c_void_p)
        view.valid_mask = None
        rc = aio.aio_hips_write_signal_support_tile(ps, ctypes.byref(view))
        if rc != 0:
            print("write tile failed:", rc, aio.aio_hips_last_error().decode())
            return 2
    arr = (AioHipsSnrPoint * len(pts))(*pts)
    rc = aio.aio_hips_write_snr_points(ps, arr, len(pts))
    if rc != 0:
        print("snr failed:", rc)
        return 3
    rc = aio.aio_hips_finalize(ps)
    if rc != 0:
        print("finalize failed:", rc, aio.aio_hips_last_error().decode())
        return 4
    print("OK", out, "tiles=", len(tile_ipixs), "snr=", len(pts))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_base")
    ap.add_argument("--nside", type=int, default=2048)
    args = ap.parse_args()
    tile_ra_dec = [(10.0, 25.0), (16.0, 27.0), (22.0, 24.0)]
    pts = []
    for i, (ra, dec) in enumerate([(10.5, 25.2), (12.0, 25.8), (15.5, 27.1),
                                   (17.0, 26.4), (20.0, 24.5), (22.5, 24.2)]):
        qf = 1 | (2 if i % 3 == 0 else 0)
        ps = 1 if i % 2 == 0 else (2 if i % 4 == 0 else 0)
        pts.append(AioHipsSnrPoint(ra, dec, 5.0 + 2.0 * i, 1001 + i, qf, ps))
    rc = gen(args.out_base + "_f32", "f32", args.nside, tile_ra_dec, pts)
    if rc:
        return rc
    return gen(args.out_base + "_f64", "f64", args.nside, tile_ra_dec, pts)


if __name__ == "__main__":
    sys.exit(main())
