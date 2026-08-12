#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
V11 P11-2/P11-5: REF_HIPS 源图生成（外部标准判决用）

生成两张合法 WCS FITS（2048x2048，2°x2°，中心 ra=60 dec=20）：
  - ref_visual.fits : 视觉图案（L/F/箭头/RA·Dec 梯度/圆/矩形/编码区）
  - ref_oracle.fits : 精确 oracle 编码（float64：v = ra*1e6*1e6 + (dec+90)*1e6，
                      可从像素值恢复天球坐标，用于 FITS tile 布局外部判决）

两者 FOV/指向一致，由 CDS Hipsgen 各自生成 REF_HIPS（不使用 AstroCS writer）。
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from astropy.io import fits
from astropy.wcs import WCS

W = H = 2048
RA0, DEC0 = 60.0, 20.0
FOV = 2.0                 # 度
PIXEL = FOV / W           # 度/像素（≈3.52″）


def wcs_header(ra0: float, dec0: float) -> dict:
    return {
        "CTYPE1": "RA---TAN",
        "CTYPE2": "DEC--TAN",
        "CRVAL1": ra0,
        "CRVAL2": dec0,
        "CRPIX1": W / 2.0 + 0.5,
        "CRPIX2": H / 2.0 + 0.5,
        "CD1_1": -PIXEL,
        "CD1_2": 0.0,
        "CD2_1": 0.0,
        "CD2_2": PIXEL,
        "RADESYS": "ICRS",
        "EQUINOX": 2000.0,
    }


def pixel_to_sky(x: np.ndarray, y: np.ndarray, ra0: float, dec0: float):
    """WCS 正投影（x/y 0-based 像素坐标）→ (ra, dec) 度。"""
    x0 = (x + 0.5) - (W / 2.0 + 0.5)
    y0 = (y + 0.5) - (H / 2.0 + 0.5)
    # CD1_1 = -PIXEL：RA 随像素 x 减小（东在左，西在右）
    dra = -x0 * PIXEL
    ddec = y0 * PIXEL
    ra = ra0 + dra
    dec = dec0 + ddec
    return ra, dec


def box_mask(x, y, ra_c, dec_c, w_deg, h_deg):
    ra, dec = pixel_to_sky(x, y, RA0, DEC0)
    return (np.abs(ra - ra_c) <= w_deg / 2.0) & (np.abs(dec - dec_c) <= h_deg / 2.0)


def circle_mask(x, y, ra_c, dec_c, r_deg):
    ra, dec = pixel_to_sky(x, y, RA0, DEC0)
    return np.hypot(ra - ra_c, dec - dec_c) <= r_deg


def build_visual() -> np.ndarray:
    xx, yy = np.meshgrid(np.arange(W), np.arange(H))
    ra, dec = pixel_to_sky(xx, yy, RA0, DEC0)
    # 基础：RA 梯度 + Dec 梯度（值随 RA/Dec 递增，用于方向判读）
    v = 100.0 + 200.0 * (ra - (RA0 - 1.0)) / 2.0 + 100.0 * (dec - (DEC0 - 1.0)) / 2.0
    # L 形（非对称方向标记）
    L = box_mask(xx, yy, 60.18, 20.12, 0.20, 0.05) | box_mask(xx, yy, 60.13, 20.12, 0.05, 0.16)
    v[L] = 1000.0
    # F 形
    F = (box_mask(xx, yy, 59.62, 19.92, 0.16, 0.05) |
         box_mask(xx, yy, 59.55, 19.92, 0.04, 0.14) |
         box_mask(xx, yy, 59.62, 19.99, 0.10, 0.04))
    v[F] = 900.0
    # 向右箭头（三角形 + 矩形，指向 RA 增大方向）
    tri = ((ra - 60.42) >= 0) & (np.abs(dec - 19.72) <= 0.06 * (1.0 - (ra - 60.42) / 0.10)) & ((ra - 60.42) <= 0.10)
    shaft = box_mask(xx, yy, 60.44, 19.72, 0.12, 0.03)
    v[tri | shaft] = 850.0
    # 三个不同尺寸圆
    for (rc, dc, r, val) in [(59.82, 20.30, 0.05, 700.0),
                             (60.28, 20.32, 0.10, 750.0),
                             (59.45, 20.48, 0.15, 800.0)]:
        v[circle_mask(xx, yy, rc, dc, r)] = val
    # 长矩形
    v[box_mask(xx, yy, 60.02, 19.52, 0.40, 0.05)] = 950.0
    # 两个编码区域（3x5 棋盘，不可因旋转/镜像混淆）
    for (rc, dc, base) in [(59.30, 20.62, 600.0), (60.72, 19.32, 650.0)]:
        for i in range(3):
            for j in range(5):
                if (i + j) % 2 == 0:
                    v[box_mask(xx, yy, rc + (i - 1) * 0.05, dc + (j - 2) * 0.05,
                               0.04, 0.04)] = base + i * 5.0 + j
    return v.astype(np.float64)


def build_oracle() -> np.ndarray:
    xx, yy = np.meshgrid(np.arange(W), np.arange(H))
    ra, dec = pixel_to_sky(xx, yy, RA0, DEC0)
    # 纯线性二维场（V11 修订：Hipsgen 双线性重采样对线性场精确）。
    # v = A*ra + B*dec；A/B 选为使 tile 内相邻 HEALPix cell（~0.0009°）的
    # 值差（~600-900）远大于 float64 插值误差，且 (A,B) 无理比例保证单射。
    A = 1_000_000.0
    B = 707_106.7811865475
    return (A * ra + B * dec).astype(np.float64)


def write_fits(path: Path, data: np.ndarray, bitpix: int = -64):
    hdr = fits.Header(wcs_header(RA0, DEC0))
    hdr["OBJECT"] = "ASTROCS_REF_V11"
    hdu = fits.PrimaryHDU(data, header=hdr)
    hdu.writeto(path, overwrite=True)
    # WCS 合法性自检
    w = WCS(hdr)
    ra, dec = w.all_pix2world([(W - 1) / 2], [(H - 1) / 2], 0)
    assert abs(ra[0] - RA0) < 1e-9 and abs(dec[0] - DEC0) < 1e-9, "WCS center check failed"
    print(f"wrote {path}")


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("run/temp/p2_v11/ref_src")
    out.mkdir(parents=True, exist_ok=True)
    write_fits(out / "ref_visual.fits", build_visual())
    write_fits(out / "ref_oracle.fits", build_oracle())
    return 0


if __name__ == "__main__":
    sys.exit(main())
