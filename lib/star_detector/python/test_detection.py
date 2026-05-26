#!/usr/bin/env python3
"""
星点检测测试脚本
检测星点并在图像上标注输出
"""

import sys
import os
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))

from star_detector import StarDetector, SDetParamsPy

def histogram_stretch(image, low_percent=0.5, high_percent=99.5):
    """直方图拉伸"""
    low = np.percentile(image, low_percent)
    high = np.percentile(image, high_percent)
    stretched = np.clip((image - low) / (high - low) * 255, 0, 255)
    return stretched.astype(np.uint8)

def draw_cross(image, x, y, size=5, color=255):
    """在图像上画十字标记"""
    h, w = image.shape
    x = int(round(x))
    y = int(round(y))
    
    for dx in range(-size, size+1):
        if 0 <= x+dx < w:
            image[y, x+dx] = color
    for dy in range(-size, size+1):
        if 0 <= y+dy < h:
            image[y+dy, x] = color

def draw_circle(image, x, y, radius=3, color=255):
    """在图像上画圆圈标记"""
    h, w = image.shape
    x = int(round(x))
    y = int(round(y))
    
    for angle in range(0, 360, 5):
        rad = np.radians(angle)
        px = int(x + radius * np.cos(rad))
        py = int(y + radius * np.sin(rad))
        if 0 <= px < w and 0 <= py < h:
            image[py, px] = color

def test_detection(image_path, output_path=None):
    """测试星点检测"""
    print(f"测试图像: {image_path}")
    
    try:
        from astropy.io import fits
        with fits.open(image_path) as hdul:
            data = hdul[0].data
            if data.dtype != np.uint16:
                if data.dtype == np.float32 or data.dtype == np.float64:
                    data = np.clip(data, 0, 65535).astype(np.uint16)
                else:
                    data = data.astype(np.uint16)
    except Exception as e:
        print(f"读取图像失败: {e}")
        return None
    
    print(f"图像尺寸: {data.shape}")
    
    params = SDetParamsPy(
        iterativeClipSigma=9.0,
        fwhmClipSigma=3.0,
        maxAxisRatio=2.0,
        fitRadius=8
    )
    
    detector = StarDetector(params=params)
    
    print("开始检测...")
    coords = detector.detect(data)
    print(f"检测到 {len(coords)} 颗星")
    
    detector.close()
    
    if output_path and len(coords) > 0:
        print(f"生成标注图像...")
        
        stretched = histogram_stretch(data.astype(np.float32))
        
        annotated = np.stack([stretched, stretched, stretched], axis=-1)
        
        for i, (x, y) in enumerate(coords):
            draw_cross(annotated[:,:,0], x, y, size=3, color=255)
            draw_cross(annotated[:,:,1], x, y, size=3, color=0)
            draw_cross(annotated[:,:,2], x, y, size=3, color=0)
        
        try:
            from PIL import Image
            img = Image.fromarray(annotated)
            img.save(output_path)
            print(f"标注图像已保存: {output_path}")
        except Exception as e:
            print(f"保存图像失败: {e}")
            try:
                from astropy.io import fits
                hdu = fits.PrimaryHDU(annotated)
                hdu.writeto(output_path.replace('.png', '.fits'), overwrite=True)
                print(f"标注图像已保存: {output_path.replace('.png', '.fits')}")
            except Exception as e2:
                print(f"保存FITS也失败: {e2}")
    
    return coords

def main():
    test_data_dir = Path(r"F:\Astro dev\Astro CS Normalization Database\testdata")
    output_dir = Path(r"F:\Astro dev\Astro CS Normalization Database\lib\star_detector\test_output")
    output_dir.mkdir(exist_ok=True)
    
    fits_files = list(test_data_dir.rglob("*.fits")) + list(test_data_dir.rglob("*.fit")) + list(test_data_dir.rglob("*.fts"))
    
    if not fits_files:
        print("未找到测试图像")
        return
    
    test_image = fits_files[0]
    output_path = str(output_dir / "annotated.png")
    
    coords = test_detection(str(test_image), output_path)
    
    if coords is not None and len(coords) > 0:
        print(f"\n前20颗星坐标:")
        for i, (x, y) in enumerate(coords[:20]):
            print(f"  {i+1}: ({x:.2f}, {y:.2f})")

if __name__ == "__main__":
    main()
