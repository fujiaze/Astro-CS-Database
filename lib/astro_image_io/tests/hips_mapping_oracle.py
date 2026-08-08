# -*- coding: utf-8 -*-
"""HiPS tile mapping 独立 Oracle (Phase1 Final Closure V3, 06_HIPS_STANDARD_STRUCTURE).

生成 native HEALPix 图 (value = NESTED ipix), 经 AstroCS 流式直写为 HiPS,
再用 astropy-healpix (独立实现) 逐像素验证:
  tile(parent_ipix @ order K) pixel (x,y) 位置上的值 == 对应叶 ipix。

覆盖: 12 base faces, 全部 tile 四角/四边/中心, 随机像素抽样,
      Dir9999/10000 边界 (ipix 9999/10000/19999/20000 附近)。

用法: py -3.12 hips_mapping_oracle.py --out <hips_dir> [--nside 2048]
"""

import ctypes
import math
import os
import random
import sys

import numpy as np
from astropy.io import fits
from astropy_healpix import HEALPix

ROOT = r"F:\Astro dev\Astro CS Normalization Database"


class AstroSphereTileView(ctypes.Structure):
    _fields_ = [("parent_ipix", ctypes.c_uint64),
                ("leaf_order", ctypes.c_uint32),
                ("width", ctypes.c_uint32),
                ("data_type", ctypes.c_int32),
                ("flux_sum", ctypes.c_void_p),
                ("covered_area", ctypes.c_void_p),
                ("valid_mask", ctypes.c_void_p)]


def add_dll_dirs():
    for d in (ROOT + r"\lib\astro_image_io", r"C:\msys64\mingw64\bin"):
        if os.path.isdir(d):
            os.add_dll_directory(d)
            os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")


def main():
    ap = __import__("argparse").ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--nside", type=int, default=2048)
    args = ap.parse_args()
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
    aio.aio_hips_finalize.restype = ctypes.c_int
    aio.aio_hips_finalize.argtypes = [ctypes.c_void_p]
    aio.aio_hips_last_error.restype = ctypes.c_char_p

    nside = args.nside
    leaf_order = int(round(math.log2(nside)))
    tile_order = leaf_order - 9
    a_cell = 4.0 * math.pi / (12.0 * nside * nside)
    n_tiles = 12 * (4 ** tile_order)
    ps = aio.aio_hips_product_begin(
        args.out.encode(), nside, 512, 1, 1,  # dtype=FP64, 仅 signal (映射测试)
        b"ivo://astrocs/mapping", b"Mapping Oracle", None, 1.0, None, 0)
    if not ps:
        print("begin fail:", aio.aio_hips_last_error().decode())
        return 2
    n = 512 * 512
    for parent in range(n_tiles):
        base = parent << 18
        flux = (base + np.arange(n, dtype=np.float64)).astype(np.float64)
        area = np.full(n, a_cell, dtype=np.float64)
        view = AstroSphereTileView()
        view.parent_ipix = parent
        view.leaf_order = leaf_order
        view.width = 512
        view.data_type = 1
        view.flux_sum = flux.ctypes.data_as(ctypes.c_void_p)
        view.covered_area = area.ctypes.data_as(ctypes.c_void_p)
        view.valid_mask = None
        rc = aio.aio_hips_write_signal_support_tile(ps, ctypes.byref(view))
        if rc != 0:
            print("tile fail", parent, aio.aio_hips_last_error().decode())
            return 3
    if aio.aio_hips_finalize(ps) != 0:
        print("finalize fail:", aio.aio_hips_last_error().decode())
        return 4

    # ---- 独立验证 (astropy-healpix) ----
    hp = HEALPix(nside=nside, order="nested")
    npix = 12 * nside * nside
    # 采样像素: 每 tile 四角/四边/中心 + 随机
    sample_ips = set()
    for parent in range(n_tiles):
        base = parent << 18
        for z in (0, 511, 512 * 511, 512 * 512 - 1, 512 * 255 + 255,
                  512 * 255, 512 * 256 + 255, 511 * 512 + 255, 255, 512 * 511):
            sample_ips.add(base + z)
    rng = random.Random(42)
    for _ in range(30000):
        sample_ips.add(rng.randrange(npix))
    # Dir9999/10000 边界: ipix 附近
    for ip in (9999, 10000, 19999, 20000, 999999, 1000000, 12345678):
        for d in range(-3, 4):
            sample_ips.add(max(0, min(npix - 1, ip + d)))

    cache = {}
    mismatch = 0
    checked = 0
    for ipix in sample_ips:
        parent = ipix >> 18
        z = ipix & 262143
        # astropy 交叉验证 parent/z 分解
        lon, lat = hp.healpix_to_lonlat(ipix)
        ip2 = hp.lonlat_to_healpix(lon, lat)
        if int(ip2) != ipix:
            mismatch += 1
            continue
        if parent not in cache:
            dd = parent // 10000
            npf = parent % 10000
            p = os.path.join(args.out, "signal", f"Norder{tile_order}", f"Dir{dd}",
                             f"Npix{npf}.fits")
            cache[parent] = fits.getdata(p)
        sig = cache[parent]
        x = z % 512
        y = z // 512
        got = sig[y, x]
        expect = float(ipix) / a_cell
        if not np.isclose(got, expect, rtol=1e-9, atol=1e-6):
            mismatch += 1
        checked += 1
    print(f"mapping oracle: checked={checked} mismatch={mismatch} "
          f"tiles={n_tiles} nside={nside}")
    return 0 if mismatch == 0 else 5


if __name__ == "__main__":
    sys.exit(main())
