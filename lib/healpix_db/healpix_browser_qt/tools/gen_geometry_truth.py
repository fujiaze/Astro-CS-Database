#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
V9 P9-3: synthetic visual geometry truth HiPS 生成器（NON_PRODUCTION_TOOL_ONLY）

生成标准 AstroCS HiPS 布局（signal/support，NESTED，Norder0..K），供真实
browser 显示与几何自动测试使用：
  - RA 大尺度递增梯度（0..360 → 值线性递增，0/360 wrap 连续）；
  - Dec 递增梯度（-90..90）；
  - 已知 RA/Dec 的圆形 fiducial markers（含极区与 wrap 处）；
  - 12 个 HEALPix base face 离散标签（face 0..11 递增偏移）；
  - 1° 天空网格暗线（人工可识别方向/镜像/旋转）；
  - 零覆盖测试区（signal=NaN, support=0，验证空区/NaN 处理）；
  - 多 order：Norder0 全空（12 tiles）+ Norder1 子集（赤道 wrap 区 + 两极）
    + Norder2 局部（RA45/Dec0 放大区）。

用法:
  py -3.12 lib/healpix_db/healpix_browser_qt/tools/gen_geometry_truth.py \
      <out_dir>
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from astropy.io import fits
from astropy_healpix import HEALPix
from astropy.coordinates import ICRS
import astropy.units as u

TW = 512
SHIFT = 9
MARKERS = [
    (0.25, 0.25), (45.25, 0.25), (90.25, 0.25), (180.25, 0.25),
    (270.25, 0.25), (0.25, 88.25), (180.25, -88.25), (359.75, 0.25),
]
ZERO_REGION = (178.0, 182.0, -1.5, 1.5)   # ra_min, ra_max, dec_min, dec_max


def angular_distance_v(ra, dec, ra2, dec2):
    r1 = np.radians(ra)
    d1 = np.radians(dec)
    r2 = math.radians(ra2)
    d2 = math.radians(dec2)
    c = np.sin(d1) * math.sin(d2) + np.cos(d1) * math.cos(d2) * np.cos(r1 - r2)
    return np.degrees(np.arccos(np.clip(c, -1.0, 1.0)))


def sky_value_v(ra, dec):
    """连续天空函数（所有 order 一致；向量化，返回 (signal, support)）。"""
    ra = np.mod(ra, 360.0)
    nan_mask = ((ra > ZERO_REGION[0]) & (ra < ZERO_REGION[1]) &
                (dec > ZERO_REGION[2]) & (dec < ZERO_REGION[3]))
    v = 0.20 + 0.50 * (ra / 360.0) + 0.25 * ((dec + 90.0) / 180.0)
    # base face 标签（nside=1 nested ipix = 0..11，C 加速）
    hp1 = HEALPix(nside=1, order="nested", frame=ICRS())
    face = hp1.skycoord_to_healpix(ICRS(ra=ra * u.deg, dec=dec * u.deg))
    v += 0.012 * face
    # 1° 网格暗线
    frac_ra = np.mod(ra, 1.0)
    frac_dec = np.mod(dec + 90.0, 1.0)
    grid = ((frac_ra < 0.015) | (frac_ra > 0.985) |
            (frac_dec < 0.015) | (frac_dec > 0.985))
    v[grid] -= 0.30
    # fiducial markers
    for mra, mdec in MARKERS:
        d = angular_distance_v(ra, dec, mra, mdec)
        mask = d < 0.55
        if np.any(mask):
            v[mask] += 0.30 * (1.0 - d[mask] / 0.55)
    v[nan_mask] = math.nan
    sup = np.where(nan_mask, 0.0, 1.0)
    return v, sup


def local_to_fits_perm():
    """local nested index k -> fits index (511-x)*512+y；缓存。"""
    x = np.zeros(TW * TW, dtype=np.int64)
    y = np.zeros(TW * TW, dtype=np.int64)
    for i in range(SHIFT):
        x |= ((np.arange(TW * TW, dtype=np.int64) >> (2 * i)) & 1) << i
        y |= ((np.arange(TW * TW, dtype=np.int64) >> (2 * i + 1)) & 1) << i
    return (TW - 1 - x) * TW + y


_FITS_PERM = None


def fits_perm():
    global _FITS_PERM
    if _FITS_PERM is None:
        _FITS_PERM = local_to_fits_perm()
    return _FITS_PERM


def tile_radec_grid(order, tile_ipix):
    """返回该 tile 全部 leaf 的 (ra, dec) 数组（local nested 序）。"""
    hp = HEALPix(nside=1 << (order + SHIFT), order="nested", frame=ICRS())
    locals_ = np.arange(TW * TW, dtype=np.uint64)
    ipix = (np.uint64(tile_ipix) << np.uint64(18)) | locals_
    sky = hp.healpix_to_skycoord(ipix.astype(np.int64))
    return np.asarray(sky.ra.deg), np.asarray(sky.dec.deg)


def order1_region_tiles():
    """order1 子集：赤道 wrap 区（face4: ra0-90, face7: ra270-360）+ 两极全部。"""
    hp = HEALPix(nside=1 << (1 + SHIFT), order="nested", frame=ICRS())
    # tile 中心 local（x=y=256 的 nested 编码）
    local_center = 0
    for i in range(9):
        local_center |= ((256 >> i) & 1) << (2 * i)
        local_center |= ((256 >> i) & 1) << (2 * i + 1)
    keep = []
    for t in range(48):
        ipix = (np.uint64(t) << np.uint64(18)) | np.uint64(local_center)
        sky = hp.healpix_to_skycoord(np.array([int(ipix)], dtype=np.int64))
        ra = float(sky.ra.deg[0] % 360.0)
        dec = float(sky.dec.deg[0])
        if dec > 41.0 or dec < -41.0:      # 极面全部
            keep.append(t)
        elif (ra < 90.0 or ra > 270.0) and -41.0 <= dec <= 41.0:
            keep.append(t)
    return sorted(keep)


def write_tile(root: Path, product: str, order: int, tile_ipix: int,
               values: np.ndarray):
    d = root / product / f"Norder{order}" / f"Dir{tile_ipix // 10000}"
    d.mkdir(parents=True, exist_ok=True)
    hdr = fits.Header()
    hdr["PIXTYPE"] = ("HEALPIX", "HEALPix pixelization")
    hdr["ORDERING"] = ("NESTED", "Pixel ordering")
    hdr["COORDSYS"] = ("C", "Equatorial")
    hdr["NSIDE"] = 1 << (order + SHIFT)
    hdr["FIRSTPIX"] = 0
    hdr["LASTPIX"] = TW * TW - 1
    hdr["OBJECT"] = "GEOMETRY_TRUTH"
    hdr["FILTER"] = "Synthetic"
    fits.writeto(d / f"Npix{tile_ipix % 10000}.fits", values, hdr,
                 overwrite=True)


def fill_tile_values(ra, dec):
    """按 FITS 布局返回 (signal, support) 512×512 float32。"""
    perm = fits_perm()
    sig = np.full(TW * TW, math.nan, dtype=np.float64)
    sup = np.zeros(TW * TW, dtype=np.float64)
    chunk = 65536
    for i in range(0, TW * TW, chunk):
        j = min(i + chunk, TW * TW)
        s, u = sky_value_v(ra[i:j], dec[i:j])
        p = perm[i:j]
        sig[p] = s
        sup[p] = u
    return sig.reshape(TW, TW).astype(np.float32), \
        sup.reshape(TW, TW).astype(np.float32)


def write_properties(root: Path, product: str, leaf_order: int, n_tiles: int):
    lines = [
        "creator_did=ivo://astrocs/geometry-truth",
        "obs_title=Geometry Truth (synthetic)",
        "obs_filter=Synthetic",
        "hips_version=1.4",
        f"hips_order={leaf_order}",
        "hips_tile_width=512",
        "hips_frame=equatorial",
        "dataproduct_type=image",
        "dataproduct_subtype=surface brightness",
        "hips_tile_format=fits",
        "hips_status=public master",
        "hips_creator=AstroCS geometry truth generator",
        "hips_initial_fov=60",
        "hips_pixel_scale=58.6",
        "astrocs_signal_dtype=float32",
        "moc_sky_fraction=1.0",
        "astrocs_covered_sky_fraction=1.0",
        "",
    ]
    (root / product / "properties").write_text("\n".join(lines), encoding="utf-8")


def write_manifest(root: Path, leaf_order: int, n_leaf_tiles: int):
    import json
    man = {
        "format_version": 1,
        "hips_version": "1.4",
        "nside": 1 << leaf_order,
        "tile_width": 512,
        "data_type": "float32",
        "products": ["signal", "support"],
        "n_leaf_tiles": n_leaf_tiles,
        "moc_sky_fraction": 1.0,
        "astrocs_covered_sky_fraction": 1.0,
        "signal_dtype": "float32",
    }
    (root / "manifest.json").write_text(json.dumps(man, indent=2), encoding="utf-8")


def write_moc(root: Path, leaf_order: int, tiles):
    from astropy.table import Table
    order_uniq_base = 4 * (1 << (2 * leaf_order))
    t = Table()
    t["UNIQ"] = [order_uniq_base + int(ip) for ip in tiles]
    hdu = fits.BinTableHDU(t)
    hdu.header["PIXTYPE"] = "HEALPIX"
    hdu.header["ORDERING"] = "NESTED"
    hdu.header["COORDSYS"] = "C"
    fits.HDUList([fits.PrimaryHDU(), hdu]).writeto(
        root / "signal" / "Moc.fits", overwrite=True)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: gen_geometry_truth.py <out_dir>")
        return 2
    root = Path(sys.argv[1])
    root.mkdir(parents=True, exist_ok=True)

    # order0 全空 12 tiles
    for t in range(12):
        ra, dec = tile_radec_grid(0, t)
        sig, sup = fill_tile_values(ra, dec)
        write_tile(root, "signal", 0, t, sig)
        write_tile(root, "support", 0, t, sup)
    print(f"order0: 12 tiles written")

    # order1 子集
    o1 = order1_region_tiles()
    for t in o1:
        ra, dec = tile_radec_grid(1, t)
        sig, sup = fill_tile_values(ra, dec)
        write_tile(root, "signal", 1, t, sig)
        write_tile(root, "support", 1, t, sup)
    print(f"order1: {len(o1)} tiles written")

    # order2 局部（RA45/Dec0 放大区：以 nside=2 tile (45,0) 的 4 个子 tile）
    hp2 = HEALPix(nside=2, order="nested", frame=ICRS())
    parent = int(hp2.skycoord_to_healpix(ICRS(ra=45.0 * u.deg, dec=0.0 * u.deg)))
    o2 = [(parent << 2) + c for c in range(4)]
    for t in o2:
        ra, dec = tile_radec_grid(2, t)
        sig, sup = fill_tile_values(ra, dec)
        write_tile(root, "signal", 2, t, sig)
        write_tile(root, "support", 2, t, sup)
    print(f"order2: {len(o2)} tiles written")

    write_properties(root, "signal", 2, len(o2))
    write_properties(root, "support", 2, len(o2))
    write_moc(root, 2, o2)
    write_manifest(root, 2, len(o2))
    print(f"done: {root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
