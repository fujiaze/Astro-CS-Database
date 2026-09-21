#!/usr/bin/env python3
"""
正向 drizzle: T4 FITS → .hiss (nside=65536, pixfrac=1.0)
用途: 生成 2x 采样密度的 HEALPix 网格, 满足奈奎斯特采样条件 + 性能测试
      pixfrac=1.0 不收缩源像素, 避免相邻源像素之间的固有缝隙
"""
import sys
import os
import time

# UTF-8 编码
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

# 添加模块路径
sys.path.insert(0, r'f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_drizzle')
sys.path.insert(0, r'f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_io')

from healpix_drizzle import hp_drizzle_fits_to_ahpx

def main():
    # 输入 FITS (platesolve 后含 WCS)
    fits_path = r'f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red\2a_platesolve_solve.fits'

    # 输出 .hiss
    output_dir = r'f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red\drizzle'
    os.makedirs(output_dir, exist_ok=True)
    output_path = os.path.join(output_dir, 'T4_2x_nside65536.hiss')

    # 参数
    nside = 65536  # 2x drizzle (3.22"/px)
    nested = True
    pixfrac = 1.0  # 不收缩源像素, 消除固有缝隙

    print(f"=== 正向 drizzle 性能测试 ===")
    print(f"输入: {fits_path}")
    print(f"输出: {output_path}")
    print(f"参数: nside={nside} nested={nested} pixfrac={pixfrac}")

    # 调用 drizzle + 计时
    t0 = time.time()
    result = hp_drizzle_fits_to_ahpx(
        fits_path=fits_path,
        output_path=output_path,
        nside=nside,
        nested=nested,
        pixfrac=pixfrac,
    )
    elapsed = time.time() - t0

    print(f"\n=== 性能结果 ===")
    print(f"耗时: {elapsed:.2f}s")
    print(f"源像素数: {result.n_source_pixels}")
    print(f"HEALPix 像素数: {result.n_healpix_pixels}")
    print(f"吞吐: {result.n_source_pixels/elapsed:.0f} 源像素/s")

    print(f"\n输出文件: {output_path}")
    print(f"文件大小: {os.path.getsize(output_path)} bytes")

if __name__ == '__main__':
    main()
