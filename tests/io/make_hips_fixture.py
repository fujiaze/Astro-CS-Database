#!/usr/bin/env python3
"""IO-002 HiPS fixture 重建生成器 (tests/io/make_hips_fixture.py)。

按固定随机种子合成一个合规 IVOA HiPS 1.4 子产品目录 (partial tree):

    <out>/properties          # hips_version/order/tile_width/frame/tile_format=fits/…
    <out>/Moc.fits            # BINTABLE UNIQ (MOCORDER=K), 叶级 ipix 集合
    <out>/Norder{K}/Dir{D}/Npix{N}.fits   # K=order 叶 tile (标准 HiPS 行主序)

生成内容确定 (seed 固定 → 字节一致), 支持 --rebuild 一键重建 (fixture 可重建验收)。
tile FITS 由 IO-001 fits_core (libfits_core_test.so) 原子写; Moc.fits 由 astropy
(独立 oracle) 写 BINTABLE。数据规律: 每 tile 平面 = 0.25*(col+1) + 0.5*row 梯度
(可直接与 read_tile_plane_f32 读回比对, 容忍 float32 舍入)。

用法:
  python3 tests/io/make_hips_fixture.py <out_dir> [--order K] [--seed N]
  python3 tests/io/make_hips_fixture.py --rebuild <out_dir>   # 删除重建

K=0..3; 默认 K=1, seed=20260902。tile 数 = 12*4^K 中的 n_covered 个 (partial,
由 seed 决定), 默认覆盖部分 (K=1 时 4 个: ipix 0,1,2,4)。
"""
from __future__ import annotations

import argparse
import ctypes
import pathlib
import shutil
import sys

import numpy as np

REPO = pathlib.Path(__file__).resolve().parents[2]
FITS_CORE = REPO / "runtime" / "io" / "fits_core.c"
INCLUDE = REPO / "modules" / "services" / "io" / "include"
LIB_SO = REPO / "runtime" / "io" / "libfits_core_test.so"

ACS_FIO_ABI_VERSION_V1 = 1
BITPIX_F32 = -32


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


def make_tile_fits(lib: ctypes.CDLL, path: pathlib.Path, width: int,
                   data: np.ndarray, extra_cards: list[tuple[str, str]],
                   height: int | None = None) -> None:
    """用 IO-001 fits_core 原子写 tile FITS (f32 平面 + 附加头卡)。

    height 缺省 = width (方阵)。传入不同 height 可生成 NAXIS1!=NAXIS2 的
    非法布局 tile (契约负测用)。"""
    if height is None:
        height = width
    # C 主序视图: 每行 NAXIS1(width) 个元素, 共 NAXIS2(height) 行
    assert data.dtype == np.float32 and data.shape == (height, width)
    kwarr = (FioKeyword * 1024)()
    for i, (k, v) in enumerate(extra_cards):
        kwarr[i].name = k.encode()[:8]
        kwarr[i].value = v.encode()
    decl = FioHeader()
    decl.struct_size = ctypes.sizeof(FioHeader)
    decl.abi_version = ACS_FIO_ABI_VERSION_V1
    decl.bitpix = BITPIX_F32
    decl.naxis = 2
    decl.naxis_n[0] = width
    decl.naxis_n[1] = height
    decl.keyword_count = len(extra_cards)
    decl.keywords = kwarr
    err = ctypes.create_string_buffer(128)
    wr = ctypes.c_void_p()
    st = lib.acs_fio_writer_begin_v1(str(path).encode(), ctypes.byref(decl),
                                     None, 1, None, ctypes.byref(wr),
                                     err, len(err))
    if st != 0:
        raise RuntimeError(f"writer_begin {path}: {err.value!r}")
    st = lib.acs_fio_write_plane_v1(wr, 0, data.ctypes.data_as(ctypes.c_void_p),
                                    data.nbytes, None, err, len(err))
    if st != 0:
        lib.acs_fio_writer_abort_v1(wr)
        raise RuntimeError(f"write_plane {path}: {err.value!r}")
    st = lib.acs_fio_writer_end_v1(wr, 1, 0, 0, None, err, len(err))
    if st != 0:
        raise RuntimeError(f"writer_end {path}: {err.value!r}")


def tile_plane(width: int, ipix: int) -> np.ndarray:
    """确定性的 tile 平面: 梯度 + ipix 偏移 (float32)。"""
    rng = np.random.default_rng(20260902 + ipix * 7919)
    base = np.zeros((width, width), dtype=np.float32)
    yy, xx = np.mgrid[0:width, 0:width]
    base = (0.25 * (xx + 1) + 0.5 * yy).astype(np.float32)
    noise = (rng.random((width, width)) * 1e-3).astype(np.float32)
    return base + noise + float(ipix) * 0.001


def build_fixture(out_dir: pathlib.Path, order: int, seed: int,
                  n_covered: int | None = None) -> pathlib.Path:
    """生成合规 HiPS 子产品目录; 返回 out_dir。"""
    lib = _ensure_lib()
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)
    width = 512
    nside = 1 << (order + 9)
    npix = 12 * (1 << (2 * order))          # order-K 单元总数
    rng = np.random.default_rng(seed)
    if n_covered is None:
        n_covered = min(npix, max(1, 4 + order * 3))
    # partial: 选择子集 (含 0, 保证至少一个 Dir0/Npix0)
    idx = sorted(set([0] + [int(i) for i in rng.choice(npix, n_covered - 1,
                                                       replace=False)]))
    idx = idx[:n_covered]

    # properties (IVOA 最小合规集 + 扩展键)
    props = [
        ("creator_did", "ivo://astrocs/test/io002"),
        ("obs_title", "IO-002 contract fixture"),
        ("hips_version", "1.4"),
        ("hips_order", str(order)),
        ("hips_tile_width", str(width)),
        ("hips_frame", "equatorial"),
        ("dataproduct_type", "image"),
        ("hips_tile_format", "fits"),
        ("hips_status", "private master"),
        ("hips_creator", "AstroCS (make_hips_fixture)"),
        ("hips_estsize", "1000000"),
        ("hips_pixel_scale", "11.519173063162576"),
        ("hips_initial_fov", "60"),
        ("moc_sky_fraction", f"{len(idx) / npix:.9f}"),
    ]
    (out_dir / "properties").write_text(
        "".join(f"{k}={v}\n" for k, v in props), encoding="utf-8")

    # Moc.fits (BINTABLE UNIQ, 独立 astropy oracle)
    try:
        from astropy.io import fits as afits
    except ImportError:
        afits = None
    moc_path = out_dir / "Moc.fits"
    if afits is not None:
        uniq = np.array([4 * (1 << (2 * order)) + i for i in idx],
                        dtype=np.int64)
        col = afits.Column(name="UNIQ", format="K", array=uniq)
        hdu = afits.BinTableHDU.from_columns([col])
        hdu.header["MOCORDER"] = order
        hdu.header["PIXCOUNT"] = len(uniq)
        hdu.header["ORDERING"] = "NESTED"
        hdu.header["COORDSYS"] = "C"
        primary = afits.PrimaryHDU()
        thdulist = afits.HDUList([primary, hdu])
        thdulist.writeto(moc_path, overwrite=True)

    # tiles (partial tree)
    for ipix in idx:
        d = ipix // 10000
        n = ipix % 10000
        rel_dir = out_dir / f"Norder{order}" / f"Dir{d}"
        rel_dir.mkdir(parents=True, exist_ok=True)
        data = tile_plane(width, ipix)
        cards = [
            ("PIXTYPE", "HEALPIX"),
            ("ORDERING", "NESTED"),
            ("COORDSYS", "C"),
            ("NSIDE", str(nside)),
            ("FIRSTPIX", "0"),
            ("LASTPIX", str(width * width - 1)),
        ]
        make_tile_fits(lib, rel_dir / f"Npix{n}.fits", width, data, cards)
    return out_dir


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("out_dir")
    ap.add_argument("--order", type=int, default=1)
    ap.add_argument("--seed", type=int, default=20260902)
    ap.add_argument("--n_covered", type=int, default=None)
    ap.add_argument("--rebuild", action="store_true",
                    help="删除已存在目录后重建 (fixture 可重建)")
    args = ap.parse_args()
    out = pathlib.Path(args.out_dir)
    if args.rebuild and out.exists():
        shutil.rmtree(out)
    if not args.order in (0, 1, 2, 3):
        print("order 必须 0..3", file=sys.stderr)
        return 2
    build_fixture(out, args.order, args.seed, args.n_covered)
    print(f"fixture ok: {out} (order={args.order}, seed={args.seed})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
