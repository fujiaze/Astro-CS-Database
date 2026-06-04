#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试自适应fitRadius功能
验证连通域大小估算FWHM并自动调整fitRadius
"""

import sys
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))

from star_detector import StarDetector, SDetParamsPy

def test_auto_fitradius():
    """测试自适应fitRadius"""
    test_data_dir = Path(r"F:\Astro dev\Astro CS Normalization Database\testdata\lights\panel1")
    
    fits_files = list(test_data_dir.glob("*.fts")) + list(test_data_dir.glob("*.fits"))
    
    if not fits_files:
        print("未找到测试图像")
        return
    
    test_image = fits_files[0]
    print(f"测试图像: {test_image}")
    
    try:
        from astropy.io import fits
        with fits.open(test_image) as hdul:
            data = hdul[0].data
            if data.dtype != np.uint16:
                data = np.clip(data, 0, 65535).astype(np.uint16)
    except Exception as e:
        print(f"读取图像失败: {e}")
        return
    
    print(f"图像尺寸: {data.shape}")
    
    # 测试1: fitRadius=0 (自动模式)
    print("\n=== 测试1: fitRadius=0 (自动模式) ===")
    params_auto = SDetParamsPy(
        iterativeClipSigma=9.0,
        fwhmClipSigma=3.0,
        maxAxisRatio=2.0,
        fitRadius=0  # 自动模式
    )
    
    detector_auto = StarDetector(params=params_auto)
    coords_auto = detector_auto.detect(data)
    print(f"检测到 {len(coords_auto)} 颗星")
    detector_auto.close()
    
    # 测试2: fitRadius=6 (固定值)
    print("\n=== 测试2: fitRadius=6 (固定值) ===")
    params_fixed = SDetParamsPy(
        iterativeClipSigma=9.0,
        fwhmClipSigma=3.0,
        maxAxisRatio=2.0,
        fitRadius=6
    )
    
    detector_fixed = StarDetector(params=params_fixed)
    coords_fixed = detector_fixed.detect(data)
    print(f"检测到 {len(coords_fixed)} 颗星")
    detector_fixed.close()
    
    # 测试3: fitRadius=12 (固定值)
    print("\n=== 测试3: fitRadius=12 (固定值) ===")
    params_large = SDetParamsPy(
        iterativeClipSigma=9.0,
        fwhmClipSigma=3.0,
        maxAxisRatio=2.0,
        fitRadius=12
    )
    
    detector_large = StarDetector(params=params_large)
    coords_large = detector_large.detect(data)
    print(f"检测到 {len(coords_large)} 颗星")
    detector_large.close()
    
    print("\n=== 结果对比 ===")
    print(f"自动模式 (fitRadius=0): {len(coords_auto)} 颗星")
    print(f"固定模式 (fitRadius=6):  {len(coords_fixed)} 颗星")
    print(f"固定模式 (fitRadius=12): {len(coords_large)} 颗星")

if __name__ == "__main__":
    test_auto_fitradius()