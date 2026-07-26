"""P05-003: 生成负面测试用 fixture (黑色/极小 FITS).

不修改业务源码; 仅在 evidence 目录生成测试输入.
"""
import os
import sys
import numpy as np
from astropy.io import fits

FIX_DIR = os.path.dirname(os.path.abspath(__file__))


def make_black_fits(path, width=4096, height=4096):
    """全黑图像 (无星点), 用于触发 PLATESOLVE_FAILED."""
    data = np.zeros((height, width), dtype=np.uint16)
    hdr = fits.Header()
    hdr["SIMPLE"] = "T"
    hdr["BITPIX"] = 16
    hdr["NAXIS"] = 2
    hdr["NAXIS1"] = width
    hdr["NAXIS2"] = height
    hdr["EXPTIME"] = 600.0
    hdr["EXPOSURE"] = 600.0
    hdr["FILTER"] = "Red"
    hdr["IMAGETYP"] = "LIGHT"
    hdr["OBJECT"] = "P05003_BLACK_TEST"
    hdr["CCD-TEMP"] = -20.0
    hdr["DATE-OBS"] = "2026-07-25T22:00:00"
    hdr["OBJCTRA"] = "05 30 00"
    hdr["OBJCTDEC"] = "-30 00 00"
    hdr["RA"] = 82.5
    hdr["DEC"] = -30.0
    hdr["EQUINOX"] = 2000.0
    hdr["RADESYS"] = "ICRS"
    hdr["TELESCOP"] = "P05003_TEST"
    hdr["INSTRUME"] = "P05003_TEST_CAM"
    hdr["GAIN"] = 1.0
    hdu = fits.PrimaryHDU(data=data, header=hdr)
    fits.writeto(path, data=data, header=hdr, overwrite=True, output_verify="silentfix")
    print(f"[fixture] 写入 {path} ({width}x{height}, all-zero)")


def make_tiny_fits(path, width=10, height=10):
    """极小图像 (10x10), 用于触发 PLATESOLVE_FAILED."""
    rng = np.random.default_rng(seed=42)
    # 注入随机噪声 + 一个伪星点, 但因尺寸过小 plate solve 必失败
    data = rng.integers(100, 200, size=(height, width), dtype=np.uint16)
    data[5, 5] = 60000  # 单一亮像素
    hdr = fits.Header()
    hdr["SIMPLE"] = "T"
    hdr["BITPIX"] = 16
    hdr["NAXIS"] = 2
    hdr["NAXIS1"] = width
    hdr["NAXIS2"] = height
    hdr["EXPTIME"] = 600.0
    hdr["EXPOSURE"] = 600.0
    hdr["FILTER"] = "Red"
    hdr["IMAGETYP"] = "LIGHT"
    hdr["OBJECT"] = "P05003_TINY_TEST"
    hdr["CCD-TEMP"] = -20.0
    hdr["DATE-OBS"] = "2026-07-25T22:00:00"
    hdr["OBJCTRA"] = "05 30 00"
    hdr["OBJCTDEC"] = "-30 00 00"
    hdr["RA"] = 82.5
    hdr["DEC"] = -30.0
    hdr["EQUINOX"] = 2000.0
    hdr["RADESYS"] = "ICRS"
    hdr["TELESCOP"] = "P05003_TEST"
    hdr["INSTRUME"] = "P05003_TEST_CAM"
    hdr["GAIN"] = 1.0
    fits.writeto(path, data=data, header=hdr, overwrite=True, output_verify="silentfix")
    print(f"[fixture] 写入 {path} ({width}x{height}, noise+spot)")


if __name__ == "__main__":
    black_path = os.path.join(FIX_DIR, "black_4096x4096.fits")
    tiny_path = os.path.join(FIX_DIR, "tiny_10x10.fits")
    make_black_fits(black_path)
    make_tiny_fits(tiny_path)
    print("[fixture] OK")
