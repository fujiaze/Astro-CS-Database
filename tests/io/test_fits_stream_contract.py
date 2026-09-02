#!/usr/bin/env python3
"""IO-001 fits_core 契约测试 + astropy oracle 交叉验证 (tests/io/)。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md IO-001 / docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md §12):
  - 接口 schema/ABI 完整性: 头文件符号/结构/错误码与实现一致 (静态检查见 test_api_schema)
  - astropy 交叉 oracle: 本实现产出 FITS ↔ astropy 互读; DATASUM 交叉一致
  - CHECKSUM: 本实现写 CHECKSUM → fitsverify 语义自洽 (篡改后拒绝)
  - 字节序: 大端 FITS 读回与本机值一致
  - dtype 全覆盖: 8/16/32/64/-32/-64 读写往返
"""
from __future__ import annotations

import ctypes
import math
import pathlib
import struct
import sys
import tempfile
import unittest

import numpy as np

REPO = pathlib.Path(__file__).resolve().parents[2]
FITS_CORE = REPO / "runtime" / "io" / "fits_core.c"
INCLUDE = REPO / "modules" / "services" / "io" / "include"
LIB_SO = REPO / "runtime" / "io" / "libfits_core_test.so"


def _build_lib():
    """若无 .so 则现场编译 (Linux 控制节点; 产物被 .gitignore 忽略)。"""
    if LIB_SO.exists():
        return
    import subprocess
    r = subprocess.run(
        ["gcc", "-std=c11", "-O2", "-fPIC", "-shared",
         "-I", str(INCLUDE), str(FITS_CORE), "-lm", "-o", str(LIB_SO)],
        capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"build libfits_core_test.so failed: {r.stderr}")


_build_lib()
LIB = ctypes.CDLL(str(LIB_SO))

# ---------- 常量 (与 fits_stream_v1.h 一致) ----------
ACS_FIO_OK = 0
ACS_FIO_ERR_CHECKSUM = 11
ACS_FIO_ERR_MISMATCH = 10
ACS_FIO_ERR_NANINF = 12
ACS_FIO_ERR_BAD_HEADER = 9
ACS_FIO_ERR_TRUNCATED = 8
ACS_FIO_ABI_VERSION_V1 = 1

BITPIX_DTYPE = {8: np.uint8, 16: np.int16, 32: np.int32, 64: np.int64,
                -32: np.float32, -64: np.float64}


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


class Reader(ctypes.Structure):
    pass


class Writer(ctypes.Structure):
    pass


class TraceHooks(ctypes.Structure):
    _fields_ = [("struct_size", ctypes.c_uint32),
                ("abi_version", ctypes.c_uint32),
                ("on_read_bytes", ctypes.c_void_p),
                ("on_write_bytes", ctypes.c_void_p),
                ("is_cancelled", ctypes.c_void_p),
                ("user_data", ctypes.c_void_p)]


def bind():
    lib = LIB
    lib.acs_fio_reader_open_v1.restype = ctypes.c_int
    lib.acs_fio_reader_open_v1.argtypes = [ctypes.c_char_p, ctypes.POINTER(TraceHooks),
                                           ctypes.POINTER(ctypes.POINTER(Reader)),
                                           ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_fio_get_header_v1.restype = ctypes.c_int
    lib.acs_fio_get_header_v1.argtypes = [ctypes.POINTER(Reader), ctypes.POINTER(FioHeader),
                                          ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_fio_read_plane_v1.restype = ctypes.c_int
    lib.acs_fio_read_plane_v1.argtypes = [ctypes.POINTER(Reader), ctypes.c_int,
                                          ctypes.c_int64, ctypes.c_int64, ctypes.c_int32,
                                          ctypes.c_char_p,
                                          ctypes.c_void_p, ctypes.c_int64, ctypes.c_int,
                                          ctypes.POINTER(ctypes.c_int64), ctypes.c_void_p,
                                          ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_fio_reader_close_v1.restype = None
    lib.acs_fio_reader_close_v1.argtypes = [ctypes.POINTER(Reader)]
    lib.acs_fio_writer_begin_v1.restype = ctypes.c_int
    lib.acs_fio_writer_begin_v1.argtypes = [ctypes.c_char_p, ctypes.POINTER(FioHeader),
                                            ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p,
                                            ctypes.POINTER(ctypes.POINTER(Writer)),
                                            ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_fio_write_plane_v1.restype = ctypes.c_int
    lib.acs_fio_write_plane_v1.argtypes = [ctypes.POINTER(Writer), ctypes.c_int, ctypes.c_void_p,
                                           ctypes.c_size_t, ctypes.c_void_p,
                                           ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_fio_writer_end_v1.restype = ctypes.c_int
    lib.acs_fio_writer_end_v1.argtypes = [ctypes.POINTER(Writer), ctypes.c_int, ctypes.c_int,
                                          ctypes.c_int, ctypes.c_void_p,
                                          ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_fio_verify_file_v1.restype = ctypes.c_int
    lib.acs_fio_verify_file_v1.argtypes = [ctypes.c_char_p, ctypes.c_int,
                                           ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_fio_compute_file_datadigest_v1.restype = ctypes.c_int
    lib.acs_fio_compute_file_datadigest_v1.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                                       ctypes.c_size_t,
                                                       ctypes.POINTER(ctypes.c_size_t),
                                                       ctypes.c_char_p, ctypes.c_size_t]
    return lib


LIB_ = bind()


def errbuf():
    return ctypes.create_string_buffer(128)


def make_header(bitpix, shape, bunit=None, extra=None):
    h = FioHeader()
    ctypes.memset(ctypes.byref(h), 0, ctypes.sizeof(h))
    h.struct_size = ctypes.sizeof(FioHeader)
    h.abi_version = ACS_FIO_ABI_VERSION_V1
    h.bitpix = bitpix
    h.naxis = len(shape)
    for i, n in enumerate(shape):
        h.naxis_n[i] = n
    kw = []
    if bunit:
        kw.append((b"BUNIT", bunit.encode(), b""))
    for name, val in (extra or []):
        kw.append((name, val, b""))
    h.keyword_count = len(kw)
    for i, (n, v, c) in enumerate(kw):
        h.keywords[i].name = n[:8]
        h.keywords[i].value = v[:71]
        h.keywords[i].comment = c[:71]
    return h


def write_fits(path, bitpix, shape, arr, bunit=None, datasum=1, checksum=0):
    """用 fits_core 写一个单平面 FITS (arr 为 numpy, dtype 与 bitpix 对应)。"""
    h = make_header(bitpix, shape, bunit)
    wr = ctypes.POINTER(Writer)()
    err = errbuf()
    st = LIB_.acs_fio_writer_begin_v1(str(path).encode(), ctypes.byref(h), None, 1, None,
                                      ctypes.byref(wr), err, len(err))
    assert st == ACS_FIO_OK, f"begin: {st} {err.value}"
    data = np.ascontiguousarray(arr)
    st = LIB_.acs_fio_write_plane_v1(wr, 0, data.ctypes.data_as(ctypes.c_void_p),
                                     data.nbytes, None, err, len(err))
    assert st == ACS_FIO_OK, f"write: {st} {err.value}"
    st = LIB_.acs_fio_writer_end_v1(wr, datasum, checksum, 0, None, err, len(err))
    assert st == ACS_FIO_OK, f"end: {st} {err.value}"
    return path


def read_plane(path, bitpix, shape, dtype, strict=0, bunit=None):
    """用 fits_core 读一个平面返回 numpy。"""
    rd = ctypes.POINTER(Reader)()
    err = errbuf()
    st = LIB_.acs_fio_reader_open_v1(str(path).encode(), None, ctypes.byref(rd), err, len(err))
    assert st == ACS_FIO_OK, f"open: {st} {err.value}"
    try:
        n = int(np.prod(shape))
        npbuf = np.empty(n, dtype=dtype)
        got = ctypes.c_int64(0)
        nb = bunit.encode() if bunit else None
        st = LIB_.acs_fio_read_plane_v1(rd, 0, shape[0] if shape else 0,
                                        shape[1] if len(shape) > 1 else 0,
                                        bitpix, nb,
                                        npbuf.ctypes.data_as(ctypes.c_void_p),
                                        n, strict, ctypes.byref(got),
                                        None, err, len(err))
        assert st == ACS_FIO_OK, f"read: {st} {err.value}"
        assert got.value == n
        return npbuf
    finally:
        LIB_.acs_fio_reader_close_v1(rd)


class TestFitsCoreContract(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.tmp.cleanup()

    def path(self, name):
        return pathlib.Path(self.tmp.name) / name

    # ---- dtype 全覆盖往返 ----
    def test_roundtrip_all_dtypes(self):
        for bpix, dt in BITPIX_DTYPE.items():
            arr = np.arange(24, dtype=dt).reshape(4, 6)
            if np.issubdtype(dt, np.integer):
                pass
            p = self.path(f"rt_{bpix}.fits")
            write_fits(p, bpix, (6, 4), arr)  # NAXIS1=6, NAXIS2=4
            back = read_plane(p, bpix, (6, 4), dt)
            self.assertTrue(np.array_equal(back.reshape(4, 6), arr),
                            f"bitpix={bpix} roundtrip mismatch")

    # ---- astropy 交叉: fits_core 写 → astropy 读 ----
    def test_astropy_reads_ours(self):
        try:
            from astropy.io import fits as afits
        except Exception as e:  # pragma: no cover
            self.skipTest(f"astropy unavailable: {e}")
        arr = np.linspace(-2, 8, 30, dtype=np.float64).reshape(5, 6)
        p = self.path("astropy_read.fits")
        write_fits(p, -64, (6, 5), arr, bunit="ct")
        with afits.open(p) as hdul:
            self.assertEqual(hdul[0].header["BITPIX"], -64)
            self.assertEqual(hdul[0].header["NAXIS1"], 6)
            self.assertEqual(hdul[0].header["NAXIS2"], 5)
            self.assertEqual(hdul[0].header.get("BUNIT"), "ct")
            np.testing.assert_allclose(hdul[0].data, arr, rtol=0, atol=0)

    # ---- astropy 交叉: astropy 写 → fits_core 读 ----
    def test_reads_astropy(self):
        try:
            from astropy.io import fits as afits
        except Exception as e:  # pragma: no cover
            self.skipTest(f"astropy unavailable: {e}")
        arr = np.arange(20, dtype=np.float32).reshape(4, 5)
        p = self.path("astro_write.fits")
        hdu = afits.PrimaryHDU(data=arr)
        hdu.header["BUNIT"] = "adu"
        hdu.writeto(p, overwrite=True)
        back = read_plane(p, -32, (5, 4), np.float32)
        self.assertTrue(np.array_equal(back.reshape(4, 5), arr))

    # ---- DATASUM 交叉 (astropy compute_datasum) ----
    def test_datasum_cross(self):
        try:
            from astropy.io import fits as afits
        except Exception as e:  # pragma: no cover
            self.skipTest(f"astropy unavailable: {e}")
        arr = np.arange(64, dtype=np.float32).reshape(8, 8)
        p = self.path("ds.fits")
        write_fits(p, -32, (8, 8), arr, datasum=1)
        ds_buf = ctypes.create_string_buffer(32)
        got_len = ctypes.c_size_t(0)
        err = errbuf()
        st = LIB_.acs_fio_compute_file_datadigest_v1(str(p).encode(), ds_buf, len(ds_buf),
                                                     ctypes.byref(got_len), err, len(err))
        self.assertEqual(st, ACS_FIO_OK)
        ours = ds_buf.value.decode()
        with afits.open(p) as hdul:
            astro_ds = hdul[0].header.get("DATASUM", "")
        # 与卡上一致 (header DATASUM 为整数; ours 为十进制字符串)
        self.assertEqual(int(ours), int(astro_ds), "compute == header DATASUM")
        # 与 astropy 独立算法一致 (astropy ≥7: 删卡后 add_datasum 重算)
        with afits.open(p, mode="update") as hdul:
            del hdul[0].header["DATASUM"]
            hdul[0].add_datasum()
            ref = hdul[0].header["DATASUM"]
        self.assertEqual(int(ours), int(ref), "datasum cross-check vs astropy")

    # ---- CHECKSUM: verify 自洽 + 篡改拒绝 ----
    def test_checksum_verify_and_tamper(self):
        arr = np.arange(100, dtype=np.float64).reshape(10, 10)
        p = self.path("cs.fits")
        write_fits(p, -64, (10, 10), arr, datasum=1, checksum=1)
        err = errbuf()
        st = LIB_.acs_fio_verify_file_v1(str(p).encode(), 1, err, len(err))
        self.assertEqual(st, ACS_FIO_OK, f"verify ok: {err.value}")
        # 篡改数据区一字节
        raw = bytearray(p.read_bytes())
        raw[2880] ^= 0x01
        p.write_bytes(bytes(raw))
        st = LIB_.acs_fio_verify_file_v1(str(p).encode(), 1, err, len(err))
        self.assertEqual(st, ACS_FIO_ERR_CHECKSUM, f"tamper rejected: {err.value}")

    # ---- NaN/Inf ----
    def test_naninf_strict(self):
        arr = np.array([1.0, np.nan, np.inf, -np.inf, 2.0], dtype=np.float32)
        p = self.path("nan.fits")
        write_fits(p, -32, (5, 1), arr)
        rd = ctypes.POINTER(Reader)()
        err = errbuf()
        st = LIB_.acs_fio_reader_open_v1(str(p).encode(), None, ctypes.byref(rd), err, len(err))
        self.assertEqual(st, ACS_FIO_OK)
        buf = np.empty(5, dtype=np.float32)
        got = ctypes.c_int64(0)
        st = LIB_.acs_fio_read_plane_v1(rd, 0, 5, 1, -32, None,
                                        buf.ctypes.data_as(ctypes.c_void_p), 5, 1,
                                        ctypes.byref(got), None, err, len(err))
        self.assertEqual(st, ACS_FIO_ERR_NANINF, "strict naninf rejected")
        LIB_.acs_fio_reader_close_v1(rd)

    # ---- 非法 header / 截断 ----
    def test_bad_header_no_simple(self):
        p = self.path("bad.fits")
        raw = bytearray(b" " * 2880)
        # 手工 FITS 卡: BITPIX/NAXIS/NAXISn 无 SIMPLE
        cards = [(b"BITPIX  ", b"= -32"), (b"NAXIS   ", b"= 2"),
                 (b"NAXIS1  ", b"= 3"), (b"NAXIS2  ", b"= 2")]
        off = 0
        for name, val in cards:
            raw[off:off + 8] = name
            raw[off + 8:off + 8 + len(val)] = val
            off += 80
        raw[off:off + 3] = b"END"
        p.write_bytes(bytes(raw))
        rd = ctypes.POINTER(Reader)()
        err = errbuf()
        st = LIB_.acs_fio_reader_open_v1(str(p).encode(), None, ctypes.byref(rd), err, len(err))
        self.assertEqual(st, ACS_FIO_ERR_BAD_HEADER)

    def test_truncated(self):
        arr = np.arange(64, dtype=np.float32).reshape(8, 8)
        p = self.path("trunc.fits")
        write_fits(p, -32, (8, 8), arr)
        raw = p.read_bytes()
        p.write_bytes(raw[:2880 + 100])  # header + 部分数据
        rd = ctypes.POINTER(Reader)()
        err = errbuf()
        st = LIB_.acs_fio_reader_open_v1(str(p).encode(), None, ctypes.byref(rd), err, len(err))
        self.assertEqual(st, ACS_FIO_ERR_TRUNCATED)

    # ---- dtype/shape/unit mismatch ----
    def test_mismatch(self):
        arr = np.arange(6, dtype=np.float32).reshape(2, 3)
        p = self.path("mm.fits")
        write_fits(p, -32, (3, 2), arr, bunit="adu")
        rd = ctypes.POINTER(Reader)()
        err = errbuf()
        st = LIB_.acs_fio_reader_open_v1(str(p).encode(), None, ctypes.byref(rd), err, len(err))
        self.assertEqual(st, ACS_FIO_OK)
        buf = np.empty(6, dtype=np.float64)
        got = ctypes.c_int64(0)
        # dtype mismatch: 期望 -64
        st = LIB_.acs_fio_read_plane_v1(rd, 0, 0, 0, -64, None,
                                        buf.ctypes.data_as(ctypes.c_void_p), 6, 0,
                                        ctypes.byref(got), None, err, len(err))
        self.assertEqual(st, ACS_FIO_ERR_MISMATCH)
        # shape mismatch: nx=4
        fb = np.empty(6, dtype=np.float32)
        st = LIB_.acs_fio_read_plane_v1(rd, 0, 4, 0, 0, None,
                                        fb.ctypes.data_as(ctypes.c_void_p), 6, 0,
                                        ctypes.byref(got), None, err, len(err))
        self.assertEqual(st, ACS_FIO_ERR_MISMATCH)
        # unit mismatch
        st = LIB_.acs_fio_read_plane_v1(rd, 0, 0, 0, 0, b"e-",
                                        fb.ctypes.data_as(ctypes.c_void_p), 6, 0,
                                        ctypes.byref(got), None, err, len(err))
        self.assertEqual(st, ACS_FIO_ERR_MISMATCH)
        LIB_.acs_fio_reader_close_v1(rd)


if __name__ == "__main__":
    unittest.main(verbosity=2)
