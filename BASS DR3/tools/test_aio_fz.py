# -*- coding: utf-8 -*-
"""aio fpack(.fz) 支持验证脚本

加载指定 astro_image_io.dll, 对 BASS science/weight/od 的 .fits.fz 直接读取,
与 funpack 后的标准 FITS 逐像素对比, 并核对元数据 (尺寸/WCS/滤镜/曝光)。

用法:
  py -3.12 test_aio_fz.py --dll <astro_image_io.dll> --fz-dir <含 .fits.fz 目录>
"""

from __future__ import annotations

import argparse
import ctypes
import os
import sys
from pathlib import Path

import numpy as np

class KW(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 72),
        ("value", ctypes.c_char * 72),
        ("comment", ctypes.c_char * 72),
    ]


class WCS(ctypes.Structure):
    _fields_ = [
        ("crpix1", ctypes.c_double), ("crpix2", ctypes.c_double),
        ("crval1", ctypes.c_double), ("crval2", ctypes.c_double),
        ("ctype1", ctypes.c_char * 32), ("ctype2", ctypes.c_char * 32),
        ("cd1_1", ctypes.c_double), ("cd1_2", ctypes.c_double),
        ("cd2_1", ctypes.c_double), ("cd2_2", ctypes.c_double),
        ("cdelt1", ctypes.c_double), ("cdelt2", ctypes.c_double),
        ("has_cdelt1", ctypes.c_int), ("has_cdelt2", ctypes.c_int),
        ("radesys", ctypes.c_char * 32),
        ("equinox", ctypes.c_double),
        ("has_equinox", ctypes.c_int),
        ("lonpole", ctypes.c_double), ("latpole", ctypes.c_double),
        ("has_lonpole", ctypes.c_int), ("has_latpole", ctypes.c_int),
        ("has_wcs", ctypes.c_int),
    ]


class Cal(ctypes.Structure):
    _fields_ = [
        ("exptime", ctypes.c_double),
        ("filter_name", ctypes.c_char * 64),
        ("gain", ctypes.c_double),
        ("ccd_temp", ctypes.c_double),
        ("has_ccd_temp", ctypes.c_int),
        ("frame_type", ctypes.c_char * 32),
        ("bunit", ctypes.c_char * 32),
    ]


class Obs(ctypes.Structure):
    _fields_ = [
        ("date_obs", ctypes.c_char * 64),
        ("date_end", ctypes.c_char * 64),
        ("jd_obs", ctypes.c_double),
        ("has_jd_obs", ctypes.c_int),
        ("longobs", ctypes.c_double), ("latobs", ctypes.c_double), ("altobs", ctypes.c_double),
        ("has_longobs", ctypes.c_int), ("has_latobs", ctypes.c_int), ("has_altobs", ctypes.c_int),
        ("observat", ctypes.c_char * 64),
        ("focallen", ctypes.c_double), ("xpixsz", ctypes.c_double),
        ("aperture", ctypes.c_double), ("focal_ratio", ctypes.c_double),
        ("has_focallen", ctypes.c_int), ("has_xpixsz", ctypes.c_int),
        ("has_aperture", ctypes.c_int), ("has_focal_ratio", ctypes.c_int),
        ("object_name", ctypes.c_char * 128),
    ]


class Meta(ctypes.Structure):
    _fields_ = [
        ("_geo_pad", ctypes.c_uint8 * 12),  # width/height/channels 直接读前 12 字节
        ("_opt_pad", ctypes.c_uint8 * 8),
        ("wcs", WCS),
        ("observation", Obs),
        ("calibration", Cal),
    ]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", type=Path, required=True)
    ap.add_argument("--fz-dir", type=Path, required=True)
    args = ap.parse_args()

    # 依赖 DLL (zstd/lz4/z) 搜索路径
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(r"C:\msys64\mingw64\bin")
    aio = ctypes.CDLL(str(args.dll.resolve()))

    aio.aio_read.restype = ctypes.c_void_p
    aio.aio_read.argtypes = [ctypes.c_char_p]
    aio.aio_read_header_only.restype = ctypes.c_void_p
    aio.aio_read_header_only.argtypes = [ctypes.c_char_p]
    aio.aio_free_image_data.argtypes = [ctypes.c_void_p]
    aio.aio_get_width.argtypes = [ctypes.c_void_p]
    aio.aio_get_width.restype = ctypes.c_int
    aio.aio_get_height.argtypes = [ctypes.c_void_p]
    aio.aio_get_height.restype = ctypes.c_int
    aio.aio_get_channels.argtypes = [ctypes.c_void_p]
    aio.aio_get_channels.restype = ctypes.c_int
    aio.aio_get_dtype.argtypes = [ctypes.c_void_p]
    aio.aio_get_dtype.restype = ctypes.c_uint8
    aio.aio_get_pixel_data.argtypes = [ctypes.c_void_p]
    aio.aio_get_pixel_data.restype = ctypes.POINTER(ctypes.c_float)
    aio.aio_get_pixel_data_f64.argtypes = [ctypes.c_void_p]
    aio.aio_get_pixel_data_f64.restype = ctypes.POINTER(ctypes.c_double)
    aio.aio_set_precision_mode.argtypes = [ctypes.c_int]
    aio.aio_get_keyword_count.argtypes = [ctypes.c_void_p]
    aio.aio_get_keyword_count.restype = ctypes.c_int
    aio.aio_get_keyword.argtypes = [ctypes.c_void_p, ctypes.c_int]
    aio.aio_get_keyword.restype = KW
    aio.aio_get_metadata.argtypes = [ctypes.c_void_p]
    aio.aio_get_metadata.restype = Meta

    def keywords(img):
        n = aio.aio_get_keyword_count(img)
        return [(aio.aio_get_keyword(img, i).name.decode(), aio.aio_get_keyword(img, i).value.decode()) for i in range(n)]

    def meta(img):
        m = aio.aio_get_metadata(img)
        return m.wcs, m.observation, m.calibration

    def read(path: str, header_only: bool = False):
        fn = aio.aio_read_header_only if header_only else aio.aio_read
        ptr = fn(path.encode("utf-8"))
        if not ptr:
            raise RuntimeError(f"aio_read failed: {path}")
        w = aio.aio_get_width(ptr)
        h = aio.aio_get_height(ptr)
        c = aio.aio_get_channels(ptr)
        dt = aio.aio_get_dtype(ptr)
        meta = None
        arr = None
        if not header_only:
            if dt == 0:
                p = aio.aio_get_pixel_data(ptr)
                arr = np.ctypeslib.as_array(p, shape=(w * h * c,)).copy()
            else:
                p = aio.aio_get_pixel_data_f64(ptr)
                arr = np.ctypeslib.as_array(p, shape=(w * h * c,)).copy()
        return w, h, c, dt, arr, ptr

    fz_dir = args.fz_dir.resolve()
    cases = []
    # funpack 对照 (用临时 DLL 读标准 FITS 也可, 但这里直接对比 funpack 文件)
    funpack_ref = fz_dir / "p7030g0031_1.fits"
    for name in ("p7030g0031_1.fits.fz", "p7030g0031_1.wht.fits.fz", "p7030g0031_1_od.fits.fz"):
        cases.append(name)

    all_ok = True
    for name in cases:
        fz = fz_dir / name
        w, h, c, dt, arr, ptr = read(str(fz))
        aio.aio_free_image_data(ptr)
        print(f"[read] {name}: {w}x{h}x{c} dtype={dt}")
        if w != 4096 or h != 4032 or c != 1:
            print(f"  FAIL dims"); all_ok = False

    # 逐像素 + 关键字 + 元数据对比
    for name in ("p7030g0031_1.fits.fz", "p7030g0031_1.wht.fits.fz", "p7030g0031_1_od.fits.fz"):
        stem = name.replace(".fz", "")
        ref = fz_dir / stem
        if not ref.exists():
            print(f"[skip] funpack 对照缺失: {ref}")
            continue
        w, h, c, dt, arr, ptr = read(str(fz_dir / name))
        kw_fz = keywords(ptr)
        m_fz = meta(ptr)
        aio.aio_free_image_data(ptr)
        rw, rh, rc, rdt, rarr, rptr = read(str(ref))
        kw_ref = keywords(rptr)
        m_ref = meta(rptr)
        aio.aio_free_image_data(rptr)
        if arr is None or rarr is None:
            print(f"  FAIL pixel read for {name}"); all_ok = False; continue
        exact = np.array_equal(arr, rarr)
        maxdiff = float(np.abs(arr.astype(np.float64) - rarr.astype(np.float64)).max()) if arr.size else 0.0
        tol_ok = bool(np.allclose(arr, rarr, rtol=1e-5, atol=1e-5)) if arr.size else True
        print(f"[compare] {name} vs {ref}: exact={exact} allclose1e-5={tol_ok} maxdiff={maxdiff:.6g}")
        all_ok &= (exact or tol_ok)
        # 关键字名集合一致 (忽略顺序)
        names_fz = {k for k, _ in kw_fz}
        names_ref = {k for k, _ in kw_ref}
        if names_fz != names_ref:
            print(f"  keyword names differ: fz-only={sorted(names_fz - names_ref)[:5]} ref-only={sorted(names_ref - names_fz)[:5]}")
            all_ok = False
        else:
            print(f"  keywords: {len(names_fz)} 个名称一致")
        # 关键元数据
        checks = [
            ("crval1", m_fz[0].crval1, m_ref[0].crval1),
            ("crval2", m_fz[0].crval2, m_ref[0].crval2),
            ("cd1_1", m_fz[0].cd1_1, m_ref[0].cd1_1),
            ("cd2_2", m_fz[0].cd2_2, m_ref[0].cd2_2),
            ("has_wcs", m_fz[0].has_wcs, m_ref[0].has_wcs),
            ("exptime", m_fz[2].exptime, m_ref[2].exptime),
            ("filter", m_fz[2].filter_name.decode(), m_ref[2].filter_name.decode()),
            ("frame_type", m_fz[2].frame_type.decode(), m_ref[2].frame_type.decode()),
            ("object", m_fz[1].object_name.decode(), m_ref[1].object_name.decode()),
        ]
        for label, a, b in checks:
            okc = (a == b)
            all_ok &= bool(okc)
            if not okc:
                print(f"  metadata mismatch {label}: fz={a!r} ref={b!r}")

    # FP64 模式
    aio.aio_set_precision_mode(1)
    w, h, c, dt, arr64, ptr = read(str(fz_dir / "p7030g0031_1.fits.fz"))
    aio.aio_free_image_data(ptr)
    print(f"[fp64] {w}x{h} dtype={dt} sample={arr64[100000] if arr64 is not None else None}")
    all_ok &= (dt == 1)
    aio.aio_set_precision_mode(0)

    # header-only + 元数据
    w, h, c, dt, arr, ptr = read(str(fz_dir / "p7030g0031_1.fits.fz"), header_only=True)
    aio.aio_free_image_data(ptr)
    print(f"[header_only] {w}x{h} dtype={dt}")
    all_ok &= (w == 4096 and h == 4032)

    print("ALL PASSED" if all_ok else "FAILED")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
