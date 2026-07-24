# -*- coding: utf-8 -*-
"""验证 astro_image_io (C++) 读取的 data Y 方向是否和 astropy.io.fits 一致"""

import os
import sys
import functools

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

print = functools.partial(print, flush=True)

PROJECT_ROOT = r"F:\Astro dev\Astro CS Normalization Database"
MINGW_BIN = r"C:\msys64\mingw64\bin"

if MINGW_BIN not in os.environ.get("PATH", ""):
    os.environ["PATH"] = MINGW_BIN + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(MINGW_BIN)
    except (OSError, FileNotFoundError):
        pass

sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))

import numpy as np
from astro_image_io import ImageReader


def main():
    fits_path = os.path.join(PROJECT_ROOT, "testdata", "lights",
                             "LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts")

    # 1. astro_image_io (C++) 读取
    reader = ImageReader()
    img = reader.read(fits_path)
    cpp_data = np.array(img.data, dtype=np.float32)
    img.close()
    print(f"C++ astro_image_io: shape={cpp_data.shape}, dtype={cpp_data.dtype}")

    # 2. astropy.io.fits 读取 (FITS 标准: data[0] = 底部)
    from astropy.io import fits
    with fits.open(fits_path) as hdul:
        fits_data = np.asarray(hdul[0].data, dtype=np.float32)
    print(f"astropy.io.fits: shape={fits_data.shape}, dtype={fits_data.dtype}")

    # 3. 对比
    print(f"\nshape 一致: {cpp_data.shape == fits_data.shape}")

    # 对比 data[0] (第一行)
    print(f"\n--- data[0] (第一行) 对比 ---")
    print(f"  C++   data[0, 0:5]   = {cpp_data[0, 0:5]}")
    print(f"  astropy data[0, 0:5] = {fits_data[0, 0:5]}")
    print(f"  C++   data[0, -5:]   = {cpp_data[0, -5:]}")
    print(f"  astropy data[0, -5:] = {fits_data[0, -5:]}")
    print(f"  data[0] 完全一致: {np.array_equal(cpp_data[0], fits_data[0])}")

    # 对比 data[-1] (最后一行)
    print(f"\n--- data[-1] (最后一行) 对比 ---")
    print(f"  C++   data[-1, 0:5]   = {cpp_data[-1, 0:5]}")
    print(f"  astropy data[-1, 0:5] = {fits_data[-1, 0:5]}")
    print(f"  data[-1] 完全一致: {np.array_equal(cpp_data[-1], fits_data[-1])}")

    # 检查是否 Y 翻转
    print(f"\n--- Y 方向检查 ---")
    print(f"  C++ data[0] == astropy data[-1]: {np.array_equal(cpp_data[0], fits_data[-1])}")
    print(f"  C++ data[-1] == astropy data[0]: {np.array_equal(cpp_data[-1], fits_data[0])}")
    print(f"  C++ data == astropy data (完全一致): {np.array_equal(cpp_data, fits_data)}")
    print(f"  C++ data == astropy data[::-1] (Y翻转): {np.array_equal(cpp_data, fits_data[::-1])}")

    # 用亮度梯度判断方向 (天图像通常有上下梯度)
    print(f"\n--- 亮度统计 ---")
    print(f"  C++   data[0] mean   = {np.mean(cpp_data[0]):.2f}")
    print(f"  C++   data[-1] mean  = {np.mean(cpp_data[-1]):.2f}")
    print(f"  astropy data[0] mean = {np.mean(fits_data[0]):.2f}")
    print(f"  astropy data[-1] mean= {np.mean(fits_data[-1]):.2f}")

    # 结论
    print(f"\n=== 结论 ===")
    if np.array_equal(cpp_data, fits_data):
        print("C++ data 与 astropy.io.fits 完全一致 (data[0] = FITS 底部)")
        print("=> WCS y 和 data 行索引方向一致, 无需 Y 翻转")
    elif np.array_equal(cpp_data, fits_data[::-1]):
        print("C++ data 是 astropy.io.fits 的 Y 翻转 (data[0] = 图像顶部)")
        print("=> WCS y 和 data 行索引方向相反, 需要 Y 翻转 WCS 或翻转图像")
    else:
        print("C++ data 与 astropy.io.fits 既不完全一致也不简单 Y 翻转")
        print("可能存在其他差异 (如 BSCALE/BZERO 处理)")


if __name__ == "__main__":
    main()
