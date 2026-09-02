#!/usr/bin/env python3
"""IO-002 hips_core 契约测试 + fixture 重建验证 (tests/io/)。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md IO-002 /
docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md §8):
  - 非 2 次幂 tile width 拒绝
  - 布局不符 (Norder/ipix/NAXIS 冲突) 拒绝
  - 缺 properties 拒绝
  - 未知 frame 拒绝
  - PNG-only (hips_tile_format=png) 拒绝
  - parent fallback 拒绝 (缺 tile 不返回父 order 内容)
  - partial tree + 缺 tile 状态 (PRESENT/MISSING/INVALID)
  - MOC optional hint (有/无 MOC 均可; 叶级 ipix 枚举)
  - fixture 可重建 (确定性重建, 内容一致)
  - FITS-only 科学平面读取 (f32/f64, 与生成数据一致)
"""
from __future__ import annotations

import ctypes
import hashlib
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

import numpy as np

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import make_hips_fixture as mhf  # noqa: E402

INCLUDE = REPO / "modules" / "services" / "io" / "include"
FITS_CORE = REPO / "runtime" / "io" / "fits_core.c"
HIPS_CORE = REPO / "runtime" / "io" / "hips_core.c"
LIB_SO = REPO / "runtime" / "io" / "libhips_core_test.so"

ACS_HIPS_OK = 0
ACS_HIPS_ERR_PARAM = 1
ACS_HIPS_ERR_PROPERTIES = 8
ACS_HIPS_ERR_ADDRESS = 9
ACS_HIPS_ERR_TILE_MISSING = 10
ACS_HIPS_ERR_TILE_INVALID = 11
ACS_HIPS_ERR_UNSUPPORTED = 5

ACS_HIPS_TILE_PRESENT = 1
ACS_HIPS_TILE_MISSING = 2
ACS_HIPS_TILE_INVALID = 3

_ERR = ctypes.create_string_buffer(160)


def _build_lib():
    if LIB_SO.exists():
        return
    r = subprocess.run(
        ["gcc", "-std=c11", "-O2", "-fPIC", "-shared",
         "-I", str(INCLUDE), str(FITS_CORE), str(HIPS_CORE), "-lm",
         "-o", str(LIB_SO)],
        capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"build libhips_core_test.so failed: {r.stderr}")


_build_lib()
LIB = ctypes.CDLL(str(LIB_SO))


def bind():
    lib = LIB
    lib.acs_hips_open_v1.restype = ctypes.c_int
    lib.acs_hips_open_v1.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                     ctypes.c_void_p,
                                     ctypes.POINTER(ctypes.c_void_p),
                                     ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_hips_close_v1.restype = None
    lib.acs_hips_close_v1.argtypes = [ctypes.c_void_p]
    lib.acs_hips_props_get_v1.restype = ctypes.c_int
    lib.acs_hips_props_get_v1.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                          ctypes.c_char_p, ctypes.c_size_t,
                                          ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_hips_props_serialize_v1.restype = ctypes.c_int
    lib.acs_hips_props_serialize_v1.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                                ctypes.c_size_t,
                                                ctypes.POINTER(ctypes.c_size_t),
                                                ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_hips_get_order_v1.restype = ctypes.c_int
    lib.acs_hips_get_order_v1.argtypes = [ctypes.c_void_p,
                                          ctypes.POINTER(ctypes.c_int32)]
    lib.acs_hips_get_tile_width_v1.restype = ctypes.c_int
    lib.acs_hips_get_tile_width_v1.argtypes = [ctypes.c_void_p,
                                               ctypes.POINTER(ctypes.c_int32)]
    lib.acs_hips_tile_count_v1.restype = ctypes.c_int
    lib.acs_hips_tile_count_v1.argtypes = [ctypes.c_void_p,
                                           ctypes.POINTER(ctypes.c_int64)]
    lib.acs_hips_tile_ipix_v1.restype = ctypes.c_int
    lib.acs_hips_tile_ipix_v1.argtypes = [ctypes.c_void_p, ctypes.c_int64,
                                          ctypes.POINTER(ctypes.c_uint64)]
    lib.acs_hips_tile_exists_v1.restype = ctypes.c_int
    lib.acs_hips_tile_exists_v1.argtypes = [ctypes.c_void_p, ctypes.c_uint64,
                                            ctypes.POINTER(ctypes.c_int)]
    lib.acs_hips_tile_status_v1.restype = ctypes.c_int
    lib.acs_hips_tile_status_v1.argtypes = [ctypes.c_void_p, ctypes.c_uint64,
                                            ctypes.POINTER(ctypes.c_int32),
                                            ctypes.c_char_p, ctypes.c_size_t]
    lib.acs_hips_read_tile_plane_f32_v1.restype = ctypes.c_int
    lib.acs_hips_read_tile_plane_f32_v1.argtypes = [ctypes.c_void_p,
                                                    ctypes.c_uint64,
                                                    ctypes.POINTER(ctypes.c_float),
                                                    ctypes.c_int64,
                                                    ctypes.POINTER(ctypes.c_int64),
                                                    ctypes.c_char_p,
                                                    ctypes.c_size_t]
    lib.acs_hips_read_tile_plane_f64_v1.restype = ctypes.c_int
    lib.acs_hips_read_tile_plane_f64_v1.argtypes = [ctypes.c_void_p,
                                                    ctypes.c_uint64,
                                                    ctypes.POINTER(ctypes.c_double),
                                                    ctypes.c_int64,
                                                    ctypes.POINTER(ctypes.c_int64),
                                                    ctypes.c_char_p,
                                                    ctypes.c_size_t]
    return lib


bind()
Handle = ctypes.c_void_p


def hips_open(d: pathlib.Path, product: str | None = None) -> Handle:
    h = Handle()
    st = LIB.acs_hips_open_v1(str(d).encode(),
                              product.encode() if product else None,
                              None, ctypes.byref(h), _ERR, len(_ERR))
    if st != ACS_HIPS_OK:
        raise AssertionError(f"open failed rc={st}: {_ERR.value!r}")
    return h


class HipsContractTest(unittest.TestCase):
    def setUp(self):
        self._td = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self._td.name)

    def tearDown(self):
        self._td.cleanup()

    def make_fixture(self, order=1, **kw) -> pathlib.Path:
        out = self.root / "fix"
        mhf.build_fixture(out, order=order, seed=20260902, **kw)
        return out

    # ---------- properties 正例 ----------
    def test_open_valid_fixture_props(self):
        d = self.make_fixture()
        h = hips_open(d)
        buf = ctypes.create_string_buffer(64)
        st = LIB.acs_hips_props_get_v1(h, b"hips_version", buf, len(buf),
                                       _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertEqual(buf.value, b"1.4")
        buf2 = ctypes.create_string_buffer(64)
        st = LIB.acs_hips_props_get_v1(h, b"hips_tile_format", buf2, len(buf2),
                                       _ERR, len(_ERR))
        self.assertEqual(buf2.value, b"fits")
        order = ctypes.c_int32()
        tw = ctypes.c_int32()
        LIB.acs_hips_get_order_v1(h, ctypes.byref(order))
        LIB.acs_hips_get_tile_width_v1(h, ctypes.byref(tw))
        self.assertEqual(order.value, 1)
        self.assertEqual(tw.value, 512)
        LIB.acs_hips_close_v1(h)

    # ---------- tile width 非 2 次幂拒绝 ----------
    def test_reject_tile_width_not_pow2(self):
        d = self.make_fixture()
        props = (d / "properties").read_text(encoding="utf-8")
        (d / "properties").write_text(
            props.replace("hips_tile_width=512", "hips_tile_width=513"),
            encoding="utf-8")
        h = Handle()
        st = LIB.acs_hips_open_v1(str(d).encode(), None, None,
                                  ctypes.byref(h), _ERR, len(_ERR))
        self.assertNotEqual(st, ACS_HIPS_OK, "非 2 次幂 tile width 应拒绝")

    # ---------- 布局不符: tile FITS NAXIS1 != TW ----------
    def test_reject_layout_mismatch_naxis(self):
        d = self.make_fixture()
        # 篡改一个 tile: 覆写为 NAXIS1=512 × NAXIS2=511 非方阵 (NAXIS2 != TW)
        tile = d / "Norder1" / "Dir0" / "Npix0.fits"
        self.assertTrue(tile.exists())
        bad = np.zeros((511, 512), dtype=np.float32)  # shape=(NAXIS2行, NAXIS1列)
        mhf.make_tile_fits(mhf._ensure_lib(), tile, 512, bad,
                           [("PIXTYPE", "HEALPIX"), ("ORDERING", "NESTED"),
                            ("COORDSYS", "C"), ("NSIDE", str(1 << 10)),
                            ("FIRSTPIX", "0"),
                            ("LASTPIX", str(512 * 512 - 1))],
                           height=511)
        h = hips_open(d)
        stt = ctypes.c_int32()
        st = LIB.acs_hips_tile_status_v1(h, 0, ctypes.byref(stt), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertEqual(stt.value, ACS_HIPS_TILE_INVALID,
                         "NAXIS2 != TW 应判 INVALID")
        LIB.acs_hips_close_v1(h)

    # ---------- 布局不符: tile 头卡冲突 (ORDERING=RING) ----------
    def test_reject_tile_ordering_ring(self):
        d = self.make_fixture()
        tile = d / "Norder1" / "Dir0" / "Npix5.fits"
        self.assertTrue(tile.exists())
        data = mhf.tile_plane(512, 5)
        mhf.make_tile_fits(mhf._ensure_lib(), tile, 512, data,
                           [("PIXTYPE", "HEALPIX"), ("ORDERING", "RING"),
                            ("COORDSYS", "C"), ("NSIDE", str(1 << 10)),
                            ("FIRSTPIX", "0"),
                            ("LASTPIX", str(512 * 512 - 1))])
        h = hips_open(d)
        stt = ctypes.c_int32()
        st = LIB.acs_hips_tile_status_v1(h, 5, ctypes.byref(stt), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertEqual(stt.value, ACS_HIPS_TILE_INVALID,
                         "ORDERING=RING (非 NESTED) 应判 INVALID")
        LIB.acs_hips_close_v1(h)

    # ---------- 布局不符: tile NSIDE 与 order 不符 ----------
    def test_reject_tile_nside_mismatch(self):
        d = self.make_fixture()
        tile = d / "Norder1" / "Dir0" / "Npix8.fits"
        self.assertTrue(tile.exists())
        data = mhf.tile_plane(512, 8)
        # order1 期望 NSIDE=1024; 写成 2048 (order2 的 nside)
        mhf.make_tile_fits(mhf._ensure_lib(), tile, 512, data,
                           [("PIXTYPE", "HEALPIX"), ("ORDERING", "NESTED"),
                            ("COORDSYS", "C"), ("NSIDE", "2048"),
                            ("FIRSTPIX", "0"),
                            ("LASTPIX", str(512 * 512 - 1))])
        h = hips_open(d)
        stt = ctypes.c_int32()
        st = LIB.acs_hips_tile_status_v1(h, 8, ctypes.byref(stt), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertEqual(stt.value, ACS_HIPS_TILE_INVALID,
                         "NSIDE 与 order 不符应判 INVALID")
        LIB.acs_hips_close_v1(h)

    # ---------- 缺 properties 拒绝 ----------
    def test_reject_missing_properties(self):
        d = self.make_fixture()
        (d / "properties").unlink()
        h = Handle()
        st = LIB.acs_hips_open_v1(str(d).encode(), None, None,
                                  ctypes.byref(h), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_ERR_PROPERTIES)

    def test_reject_bad_properties_value(self):
        d = self.make_fixture()
        props = (d / "properties").read_text(encoding="utf-8")
        (d / "properties").write_text(
            props.replace("hips_order=1", "hips_order=abc"), encoding="utf-8")
        h = Handle()
        st = LIB.acs_hips_open_v1(str(d).encode(), None, None,
                                  ctypes.byref(h), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_ERR_PROPERTIES)

    # ---------- 未知 frame 拒绝 ----------
    def test_reject_unknown_frame(self):
        d = self.make_fixture()
        props = (d / "properties").read_text(encoding="utf-8")
        (d / "properties").write_text(
            props.replace("hips_frame=equatorial", "hips_frame=galactic"),
            encoding="utf-8")
        h = Handle()
        st = LIB.acs_hips_open_v1(str(d).encode(), None, None,
                                  ctypes.byref(h), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_ERR_UNSUPPORTED)

    # ---------- PNG-only 拒绝 ----------
    def test_reject_png_only(self):
        d = self.make_fixture()
        props = (d / "properties").read_text(encoding="utf-8")
        (d / "properties").write_text(
            props.replace("hips_tile_format=fits", "hips_tile_format=png"),
            encoding="utf-8")
        h = Handle()
        st = LIB.acs_hips_open_v1(str(d).encode(), None, None,
                                  ctypes.byref(h), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_ERR_UNSUPPORTED)

    # ---------- parent fallback 拒绝 ----------
    def test_no_parent_fallback_missing_tile(self):
        d = self.make_fixture()
        h = hips_open(d)
        # order=1 域 48 单元; 请求未覆盖 ipix=3
        # 构造父 order 同族存在: 父单元 = ipix >> 2 (order 0), 补一个父树
        # 目录 (模拟父 order 存在同族 hierarchy tile), 断言仍 MISSING
        parent_dir = d / "Norder0" / "Dir0"
        parent_dir.mkdir(parents=True, exist_ok=True)
        # 父 tile 为 512x512, 同 Npix 命名规则 (hierarchy 惯例 Npix=A)
        fake_parent = parent_dir / "Npix0.fits"
        data = np.full((512, 512), -999.0, dtype=np.float32)
        mhf.make_tile_fits(mhf._ensure_lib(), fake_parent, 512, data, [])
        stt = ctypes.c_int32()
        st = LIB.acs_hips_tile_status_v1(h, 3, ctypes.byref(stt), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertEqual(stt.value, ACS_HIPS_TILE_MISSING,
                         "缺 tile 必须 MISSING, 即使父 order 存在同族 tile")
        # 读取也必须 MISSING 错误 (绝不返回父内容)
        buf = (ctypes.c_float * (512 * 512))()
        got = ctypes.c_int64()
        st = LIB.acs_hips_read_tile_plane_f32_v1(h, 3, buf, 512 * 512,
                                                 ctypes.byref(got), _ERR,
                                                 len(_ERR))
        self.assertEqual(st, ACS_HIPS_ERR_TILE_MISSING)
        self.assertEqual(got.value, 0)
        LIB.acs_hips_close_v1(h)

    # ---------- ipix 越界 → ADDRESS ----------
    def test_ipix_out_of_domain_address(self):
        d = self.make_fixture()
        h = hips_open(d)
        stt = ctypes.c_int32()
        st = LIB.acs_hips_tile_status_v1(h, 48, ctypes.byref(stt), _ERR,
                                         len(_ERR))
        self.assertEqual(st, ACS_HIPS_ERR_ADDRESS)
        LIB.acs_hips_close_v1(h)

    # ---------- partial tree + 缺 tile 状态 ----------
    def test_tile_status_present_missing(self):
        d = self.make_fixture()
        h = hips_open(d)
        # MOC 叶级集合 [0,5,8,18,28,40,46]
        cnt = ctypes.c_int64()
        LIB.acs_hips_tile_count_v1(h, ctypes.byref(cnt))
        self.assertGreaterEqual(cnt.value, 1)
        ips = []
        for i in range(cnt.value):
            ip = ctypes.c_uint64()
            st = LIB.acs_hips_tile_ipix_v1(h, i, ctypes.byref(ip))
            self.assertEqual(st, ACS_HIPS_OK)
            ips.append(ip.value)
        for ip in ips:
            stt = ctypes.c_int32()
            st = LIB.acs_hips_tile_status_v1(h, ip, ctypes.byref(stt), _ERR,
                                             len(_ERR))
            self.assertEqual(st, ACS_HIPS_OK)
            self.assertEqual(stt.value, ACS_HIPS_TILE_PRESENT)
        # 域内但未覆盖: MISSING (partial tree)
        missing = [x for x in range(48) if x not in ips]
        self.assertTrue(missing)
        for ip in missing[:3]:
            stt = ctypes.c_int32()
            st = LIB.acs_hips_tile_status_v1(h, ip, ctypes.byref(stt), _ERR,
                                             len(_ERR))
            self.assertEqual(st, ACS_HIPS_OK)
            self.assertEqual(stt.value, ACS_HIPS_TILE_MISSING)
        # tile_exists 快探
        ex = ctypes.c_int(0)
        st = LIB.acs_hips_tile_exists_v1(h, ips[0], ctypes.byref(ex))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertEqual(ex.value, 1)
        ex = ctypes.c_int(1)
        st = LIB.acs_hips_tile_exists_v1(h, missing[0], ctypes.byref(ex))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertEqual(ex.value, 0)
        LIB.acs_hips_close_v1(h)

    # ---------- MOC optional hint: 无 MOC 时枚举 0 但单 tile 定位仍可用 ----------
    def test_moc_optional_hint_absent(self):
        d = self.make_fixture()
        (d / "Moc.fits").unlink()
        h = hips_open(d)
        cnt = ctypes.c_int64()
        st = LIB.acs_hips_tile_count_v1(h, ctypes.byref(cnt))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertEqual(cnt.value, 0, "无 MOC: 枚举 0 (optional)")
        # 单 tile 定位不受影响
        stt = ctypes.c_int32()
        st = LIB.acs_hips_tile_status_v1(h, 0, ctypes.byref(stt), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertEqual(stt.value, ACS_HIPS_TILE_PRESENT)
        LIB.acs_hips_close_v1(h)

    # ---------- FITS-only 科学平面读取往返 (f32/f64 与生成数据一致) ----------
    def test_read_signal_plane_matches_fixture(self):
        d = self.make_fixture()
        h = hips_open(d)
        order = 1
        width = 512
        # 用与生成器相同规律重建期望数据
        cnt = ctypes.c_int64()
        LIB.acs_hips_tile_count_v1(h, ctypes.byref(cnt))
        for i in range(min(cnt.value, 3)):
            ip = ctypes.c_uint64()
            LIB.acs_hips_tile_ipix_v1(h, i, ctypes.byref(ip))
            expect = mhf.tile_plane(width, ip.value)
            buf32 = (ctypes.c_float * (width * width))()
            got = ctypes.c_int64()
            st = LIB.acs_hips_read_tile_plane_f32_v1(
                h, ip.value, buf32, width * width, ctypes.byref(got), _ERR,
                len(_ERR))
            self.assertEqual(st, ACS_HIPS_OK)
            self.assertEqual(got.value, width * width)
            arr32 = np.ctypeslib.as_array(buf32).reshape(width, width)
            np.testing.assert_allclose(arr32, expect, rtol=1e-5, atol=1e-5)
            # f64 接口
            buf64 = (ctypes.c_double * (width * width))()
            got = ctypes.c_int64()
            st = LIB.acs_hips_read_tile_plane_f64_v1(
                h, ip.value, buf64, width * width, ctypes.byref(got), _ERR,
                len(_ERR))
            self.assertEqual(st, ACS_HIPS_OK)
            arr64 = np.ctypeslib.as_array(buf64).reshape(width, width)
            np.testing.assert_allclose(arr64, expect.astype(np.float64),
                                       rtol=1e-6, atol=1e-6)
        LIB.acs_hips_close_v1(h)

    # ---------- 容量不足拒绝 ----------
    def test_read_plane_capacity_too_small(self):
        d = self.make_fixture()
        h = hips_open(d)
        buf = (ctypes.c_float * 16)()
        got = ctypes.c_int64()
        st = LIB.acs_hips_read_tile_plane_f32_v1(h, 0, buf, 16,
                                                 ctypes.byref(got), _ERR,
                                                 len(_ERR))
        self.assertEqual(st, ACS_HIPS_ERR_PARAM)
        LIB.acs_hips_close_v1(h)

    # ---------- 产品集根目录 + 子产品选择 ----------
    def test_product_subdir_open(self):
        d = self.make_fixture()
        root = d.parent / "prods"
        root.mkdir(exist_ok=True)
        shutil.move(str(d), str(root / "signal"))
        # 未知产品拒绝
        h = Handle()
        st = LIB.acs_hips_open_v1(str(root).encode(), b"snr", None,
                                  ctypes.byref(h), _ERR, len(_ERR))
        self.assertEqual(st, ACS_HIPS_ERR_UNSUPPORTED)
        # 合法子产品
        h2 = hips_open(root, "signal")
        cnt = ctypes.c_int64()
        st = LIB.acs_hips_tile_count_v1(h2, ctypes.byref(cnt))
        self.assertEqual(st, ACS_HIPS_OK)
        self.assertGreaterEqual(cnt.value, 1)
        LIB.acs_hips_close_v1(h2)

    # ---------- fixture 可重建 (确定性) ----------
    def test_fixture_rebuild_deterministic(self):
        d1 = self.root / "a"
        mhf.build_fixture(d1, order=1, seed=20260902)
        # 删除重建 (--rebuild 语义)
        d2 = self.root / "b"
        mhf.build_fixture(d2, order=1, seed=20260902)

        def digest_tree(p: pathlib.Path):
            h = hashlib.sha256()
            for f in sorted(p.rglob("*")):
                if f.is_file():
                    h.update(str(f.relative_to(p)).encode())
                    h.update(f.read_bytes())
            return h.hexdigest()

        self.assertEqual(digest_tree(d1), digest_tree(d2),
                         "fixture 重建必须逐字节一致")
        # 生成器 CLI --rebuild 亦可用
        cli = subprocess.run(
            [sys.executable, str(pathlib.Path(__file__).resolve().parent /
                                 "make_hips_fixture.py"),
             "--rebuild", str(d1), "--order", "1"],
            capture_output=True, text=True)
        self.assertEqual(cli.returncode, 0, cli.stderr)
        self.assertEqual(digest_tree(d1), digest_tree(d2),
                         "CLI --rebuild 后内容一致")


if __name__ == "__main__":
    unittest.main(verbosity=2)
