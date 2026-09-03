#!/usr/bin/env python3
"""IO-003 测试辅助：确定性构建合法 FITS tile + properties（tests/io/hips_output_fixture.py）。

复用 IO-001 fits_core（libfits_core_test.so）原子写 tile FITS（f32、DATASUM 卡），
保证发布流水线 fitsverify（DATASUM）步骤面对真实合法 FITS；内容确定（与 IO-002
fixture 生成器同源风格：梯度 + 偏移），无第三方依赖。

用法：
  build_properties(order=0, tile_width=16, frame="equatorial") -> bytes
  make_tile_fits_bytes(width, ipix) -> bytes（合法 f32 FITS 含 DATASUM 卡）
"""
from __future__ import annotations

import ctypes
import pathlib
import sys
import tempfile

import numpy as np

REPO = pathlib.Path(__file__).resolve().parents[2]
INCLUDE = REPO / "modules" / "services" / "io" / "include"
FITS_CORE = REPO / "runtime" / "io" / "fits_core.c"
LIB_SO = REPO / "runtime" / "io" / "libfits_core_test.so"

ACS_FIO_ABI_VERSION_V1 = 1
BITPIX_F32 = -32


class FioKeyword(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char * 9),
                ("value", ctypes.c_char * 72),
                ("comment", ctypes.c_char * 72)]


class FioHeader(ctypes.Structure):
    _fields_ = [("struct_size", ctypes.c_uint32),
                ("abi_version", ctypes.c_uint32),
                ("bitpix", ctypes.c_int32),
                ("naxis", ctypes.c_int32),
                ("naxis_n", ctypes.c_int64 * 3),
                ("keyword_count", ctypes.c_int32),
                ("keywords", FioKeyword * 1024)]


def _ensure_lib() -> ctypes.CDLL:
    import subprocess
    if not LIB_SO.exists():
        r = subprocess.run(
            ["gcc", "-std=c11", "-O2", "-fPIC", "-shared",
             "-I", str(INCLUDE), str(FITS_CORE), "-lm", "-o", str(LIB_SO)],
            capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"build libfits_core_test.so failed: {r.stderr}")
    lib = ctypes.CDLL(str(LIB_SO))
    lib.acs_fio_writer_begin_v1.restype = ctypes.c_int
    lib.acs_fio_writer_begin_v1.argtypes = [ctypes.c_char_p, ctypes.c_void_p,
                                            ctypes.c_char_p, ctypes.c_int,
                                            ctypes.c_void_p, ctypes.c_void_p,
                                            ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_fio_write_plane_v1.restype = ctypes.c_int
    lib.acs_fio_write_plane_v1.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                           ctypes.c_void_p, ctypes.c_size_t,
                                           ctypes.c_void_p, ctypes.c_char_p,
                                           ctypes.c_size_t]
    lib.acs_fio_writer_end_v1.restype = ctypes.c_int
    lib.acs_fio_writer_end_v1.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                          ctypes.c_int, ctypes.c_int,
                                          ctypes.c_void_p, ctypes.c_char_p,
                                          ctypes.c_size_t]
    lib.acs_fio_writer_abort_v1.restype = None
    lib.acs_fio_writer_abort_v1.argtypes = [ctypes.c_void_p]
    return lib


def build_properties(order: int = 0, tile_width: int = 16,
                     frame: str = "equatorial") -> bytes:
    """合规 HiPS properties（IO-002 §3.1 必填键；hips_tile_format=fits）。"""
    return ("hips_version=1.4\n"
            f"hips_order={order}\n"
            f"hips_tile_width={tile_width}\n"
            "hips_tile_format=fits\n"
            f"hips_frame={frame}\n").encode("ascii")


def make_tile_fits_bytes(width: int = 16, ipix: int = 0,
                         height: int | None = None) -> bytes:
    """确定性合法 f32 FITS tile（含 DATASUM 卡）字节。

    平面 = 0.25*(col+1) + 0.5*row + ipix*0.001（float32；确定性）。
    返回完整 FITS 文件字节（可直接 fitsverify）。
    """
    lib = _ensure_lib()
    if height is None:
        height = width
    arr = (0.25 * (np.arange(width, dtype=np.float32)[None, :] + 1) +
           0.5 * np.arange(height, dtype=np.float32)[:, None]).astype(np.float32)
    arr = arr + float(ipix) * 0.001
    decl = FioHeader()
    decl.struct_size = ctypes.sizeof(FioHeader)
    decl.abi_version = ACS_FIO_ABI_VERSION_V1
    decl.bitpix = BITPIX_F32
    decl.naxis = 2
    decl.naxis_n[0] = width
    decl.naxis_n[1] = height
    decl.keyword_count = 0
    err = ctypes.create_string_buffer(160)
    with tempfile.TemporaryDirectory() as td:
        p = pathlib.Path(td) / "tile.fits"
        wr = ctypes.c_void_p()
        st = lib.acs_fio_writer_begin_v1(str(p).encode(), ctypes.byref(decl),
                                         None, 1, None, ctypes.byref(wr),
                                         err, len(err))
        if st != 0:
            raise RuntimeError(f"writer_begin: {err.value!r}")
        st = lib.acs_fio_write_plane_v1(
            wr, 0, arr.ctypes.data_as(ctypes.c_void_p), arr.nbytes,
            None, err, len(err))
        if st != 0:
            lib.acs_fio_writer_abort_v1(wr)
            raise RuntimeError(f"write_plane: {err.value!r}")
        st = lib.acs_fio_writer_end_v1(wr, 1, 0, 0, None, err, len(err))
        if st != 0:
            raise RuntimeError(f"writer_end: {err.value!r}")
        return p.read_bytes()


def standard_hips_files(order: int = 0, width: int = 16,
                        ipix_list: tuple[int, ...] = (0, 1, 2)) -> list[tuple[str, bytes]]:
    """标准目录产物文件清单：properties + 若干 NorderK/DirD/NpixN.fits。"""
    files: list[tuple[str, bytes]] = [("properties", build_properties(order, width))]
    for ipix in ipix_list:
        d = ipix // 10000
        n = ipix % 10000
        files.append((f"Norder{order}/Dir{d}/Npix{n}.fits",
                      make_tile_fits_bytes(width, ipix)))
    return files
