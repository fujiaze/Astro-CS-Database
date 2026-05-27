#!/usr/bin/env python3
"""
Star Detector 饱和星点检测测试脚本
功能: 测试 sdet_detect_ex 和调试图像输出
"""

import os
import sys
import time
import numpy as np

project_root = r"F:\Astro dev\Astro CS Normalization Database"
sys.path.insert(0, os.path.join(project_root, "lib", "star_detector", "python"))
sys.path.insert(0, os.path.join(project_root, "lib", "astro_image_io", "python"))

from star_detector import StarDetector, SDetParamsPy
from astro_image_io import ImageReader

def main():
    print("=" * 60)
    print("Star Detector 饱和星点检测测试")
    print("=" * 60)

    test_image = r"F:\Astro dev\Astro CS Normalization Database\testdata\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts"

    if not os.path.exists(test_image):
        print(f"错误: 测试图像不存在: {test_image}")
        return

    print("\n=== 读取图像 ===")
    reader = ImageReader()
    with reader.read(test_image) as img_hdr:
        w, h = img_hdr.width, img_hdr.height
        image_array = img_hdr.data.copy()
        print(f"  图像: {w}x{h}, dtype={image_array.dtype}")
        print(f"  像素范围: [{image_array.min()}, {image_array.max()}]")

    print("\n=== 检测星点 (含饱和星) ===")
    t0 = time.time()
    sd_params = SDetParamsPy(iterativeClipSigma=9.0, fwhmClipSigma=3.0, maxAxisRatio=2.0, fitRadius=8)
    detector = StarDetector(params=sd_params)

    coords, fluxes, saturated = detector.detect_ex(image_array)
    detector.close()
    det_time = time.time() - t0

    n_saturated = sum(1 for s in saturated if s == 1)
    n_normal = sum(1 for s in saturated if s == 0)

    print(f"  总星数: {len(coords)}")
    print(f"  饱和星: {n_saturated}")
    print(f"  正常星: {n_normal}")
    print(f"  耗时: {det_time:.1f}s")

    if n_saturated > 0:
        print(f"\n  前10个饱和星点:")
        sat_count = 0
        for i, ((x, y), flux, sat) in enumerate(zip(coords, fluxes, saturated)):
            if sat == 1:
                print(f"    [{sat_count}] x={x:.2f}, y={y:.2f}, flux={flux:.1f}")
                sat_count += 1
                if sat_count >= 10:
                    break

    if n_normal > 0:
        print(f"\n  前5个正常星点:")
        normal_count = 0
        for i, ((x, y), flux, sat) in enumerate(zip(coords, fluxes, saturated)):
            if sat == 0:
                print(f"    [{normal_count}] x={x:.2f}, y={y:.2f}, flux={flux:.1f}")
                normal_count += 1
                if normal_count >= 5:
                    break

    print("\n=== 生成调试图像 ===")
    output_path = os.path.join(os.path.dirname(test_image), "star_detector_debug.png")

    detector2 = StarDetector(params=sd_params)
    result = detector2.detect_debug_image(image_array, output_path)
    detector2.close()

    n_sat_debug = sum(1 for s in result['saturated'] if s == 1)
    n_norm_debug = sum(1 for s in result['saturated'] if s == 0)
    print(f"  调试图像星数: 饱和={n_sat_debug}, 正常={n_norm_debug}")
    print(f"  调试图像已保存: {output_path}")

    print("\n" + "=" * 60)
    print("测试完成!")
    print("=" * 60)

if __name__ == "__main__":
    main()
