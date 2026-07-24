#!/usr/bin/env python3
"""
动态区域迭代阈值背景分离 - 测试与验证
功能: 用动态区域迭代sigma-clip方法分离天文图像的背景层和细节层，替代à-trous小波分解
用途: 输入16bit FITS图像，输出背景层、细节层、à-trous细节层的FITS文件供可视化对比
"""

import sys
import os
import time
from pathlib import Path
import numpy as np


def iterative_sigma_clip(data: np.ndarray, clip_sigma: float = 3.0, max_rounds: int = 3):
    """对1D数组执行迭代sigma-clip，返回3轮迭代med的中位数和mad的中位数"""
    buf = data.copy()
    meds = []
    mads = []

    for rnd in range(max_rounds):
        med = np.median(buf)
        mad = np.median(np.abs(buf - med)) * 1.4826
        meds.append(med)
        mads.append(mad)
        threshold = med + clip_sigma * mad
        mask = buf <= threshold
        clipped = buf[mask]
        if len(clipped) < 10:
            break
        buf = clipped

    return float(np.median(meds)), float(np.median(mads))


def dynamic_regional_background(
    image: np.ndarray,
    block_size: int = 100,
    block_overlap: int = 50,
    sub_block_size: int = 20,
    sub_block_overlap: int = 10,
    clip_sigma: float = 3.0,
    max_rounds: int = 3,
    fill_radius: int = 16,
) -> tuple[np.ndarray, np.ndarray]:
    """
    动态区域迭代阈值背景分离

    算法:
    1. 100px基础窗口做sigma-clip，得到每块局部med/mad
    2. 判断每块是否正常：均值/中位数比在0.707~1/0.707范围内为正常
    3. 正常块直接用整块阈值(med + clip_sigma * mad)
    4. 异常块(均值偏离中位数)拆成20x20子块精细化迭代
    5. 生成全图阈值图 → 掩膜 → 邻近填充 → 背景层

    参数:
        image: 输入图像(float32)
        block_size: 基础分块大小
        block_overlap: 基础块间重叠像素数(50%)
        sub_block_size: 异常块细化子块大小
        sub_block_overlap: 子块间重叠像素数(50%)
        clip_sigma: 迭代clipping的sigma倍数
        max_rounds: 每块最大迭代轮数
        fill_radius: 掩膜区域填充时搜索周围非掩膜像素的半径

    返回:
        (background, detail): 背景层和细节层
    """
    h, w = image.shape

    step = block_size - block_overlap
    n_blocks_x = max(1, (w - block_overlap + step - 1) // step)
    n_blocks_y = max(1, (h - block_overlap + step - 1) // step)

    med_map = np.zeros_like(image)
    mad_map = np.zeros_like(image)
    weight = np.zeros_like(image)

    for by in range(n_blocks_y):
        for bx in range(n_blocks_x):
            x0 = bx * step
            y0 = by * step
            x1 = min(x0 + block_size, w)
            y1 = min(y0 + block_size, h)

            block = image[y0:y1, x0:x1].ravel()
            med, mad = iterative_sigma_clip(block, clip_sigma, max_rounds)
            mean_val = np.mean(block)

            ratio = mean_val / (med + 1e-30)
            is_normal = (ratio >= 0.707) and (ratio <= 1.0 / 0.707)

            if is_normal:
                cx0, cy0 = x0, y0
                cx1, cy1 = x1, y1
                med_map[cy0:cy1, cx0:cx1] += med
                mad_map[cy0:cy1, cx0:cx1] += mad
                weight[cy0:cy1, cx0:cx1] += 1.0
            else:
                sub_step = sub_block_size - sub_block_overlap
                n_sub_x = max(1, (x1 - x0 - sub_block_overlap + sub_step - 1) // sub_step)
                n_sub_y = max(1, (y1 - y0 - sub_block_overlap + sub_step - 1) // sub_step)

                for sby in range(n_sub_y):
                    for sbx in range(n_sub_x):
                        sx0 = x0 + sbx * sub_step
                        sy0 = y0 + sby * sub_step
                        sx1 = min(sx0 + sub_block_size, x1)
                        sy1 = min(sy0 + sub_block_size, y1)

                        sub_block = image[sy0:sy1, sx0:sx1].ravel()
                        s_med, s_mad = iterative_sigma_clip(sub_block, clip_sigma, max_rounds)

                        med_map[sy0:sy1, sx0:sx1] += s_med
                        mad_map[sy0:sy1, sx0:sx1] += s_mad
                        weight[sy0:sy1, sx0:sx1] += 1.0

    mask = weight > 0
    med_map[mask] /= weight[mask]
    mad_map[mask] /= weight[mask]
    mad_map[~mask] = 1.0

    star_mask = image > (med_map + clip_sigma * mad_map)

    mask_ratio = np.sum(star_mask) / star_mask.size
    print(f"  [BG] 星点掩膜比例: {mask_ratio*100:.2f}% ({np.sum(star_mask)}/{star_mask.size})")

    background = image.copy()

    if np.any(star_mask):
        from scipy.ndimage import uniform_filter

        inv_mask = (~star_mask).astype(np.float64)
        image_float = image.astype(np.float64)

        sum_img = uniform_filter(image_float * inv_mask, size=fill_radius * 2 + 1, mode='reflect')
        cnt_img = uniform_filter(inv_mask, size=fill_radius * 2 + 1, mode='reflect')

        valid = cnt_img > 0.01
        fill_values = np.zeros_like(image_float)
        fill_values[valid] = sum_img[valid] / cnt_img[valid]
        fill_values[~valid] = med_map[~valid]

        background[star_mask] = fill_values[star_mask].astype(np.float32)

    detail = image - background

    return background, detail


def test_basic_shape():
    """测试: 输出背景层和细节层形状正确"""
    np.random.seed(42)
    image = np.random.normal(1000, 10, (512, 512)).astype(np.float32)
    bg, detail = dynamic_regional_background(image)
    assert bg.shape == image.shape, f"背景层形状错误: {bg.shape} != {image.shape}"
    assert detail.shape == image.shape, f"细节层形状错误: {detail.shape} != {image.shape}"
    print("  [PASS] 输出形状正确")


def test_detail_equals_original_minus_background():
    """测试: 细节层 = 原图 - 背景层"""
    np.random.seed(42)
    image = np.random.normal(1000, 10, (256, 256)).astype(np.float32)
    bg, detail = dynamic_regional_background(image)
    diff = np.max(np.abs(detail - (image - bg)))
    assert diff < 1e-3, f"细节层 != 原图-背景层, 最大差异={diff}"
    print("  [PASS] 细节层 = 原图 - 背景层")


def test_star_signal_in_detail():
    """测试: 细节层中星点位置有正信号"""
    np.random.seed(42)
    image = np.random.normal(1000, 10, (512, 512)).astype(np.float32)
    star_positions = [(100, 100), (200, 200), (300, 300)]
    for sx, sy in star_positions:
        for dy in range(-3, 4):
            for dx in range(-3, 4):
                r2 = dx * dx + dy * dy
                if r2 <= 9:
                    image[sy + dy, sx + dx] += 500 * np.exp(-r2 / 2.0)

    bg, detail = dynamic_regional_background(image)

    for sx, sy in star_positions:
        val = detail[sy, sx]
        assert val > 10, f"星点({sx},{sy})细节层信号过弱: {val}"
    print("  [PASS] 星点位置细节层有正信号")


def test_background_smooth_at_stars():
    """测试: 背景层中星点区域被平滑填充"""
    np.random.seed(42)
    image = np.random.normal(1000, 10, (512, 512)).astype(np.float32)
    sx, sy = 256, 256
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            r2 = dx * dx + dy * dy
            if r2 <= 9:
                image[sy + dy, sx + dx] += 500 * np.exp(-r2 / 2.0)

    bg, detail = dynamic_regional_background(image)

    bg_val_at_star = bg[sy, sx]
    bg_val_nearby = bg[sy + 10, sx + 10]
    diff = abs(bg_val_at_star - bg_val_nearby)
    assert diff < 50, f"背景层星点位置({bg_val_at_star})与附近({bg_val_nearby})差异过大: {diff}"
    print("  [PASS] 背景层星点区域被平滑填充")


def test_regional_adaptation():
    """测试: 不同区域的局部阈值不同"""
    np.random.seed(42)
    image = np.zeros((512, 512), dtype=np.float32)
    image[:, :256] = np.random.normal(500, 5, (512, 256)).astype(np.float32)
    image[:, 256:] = np.random.normal(2000, 5, (512, 256)).astype(np.float32)

    bg, detail = dynamic_regional_background(image, block_size=256, block_overlap=32)

    bg_left = np.mean(bg[:, :128])
    bg_right = np.mean(bg[:, 384:])
    assert abs(bg_left - 500) < 30, f"左侧背景估计偏差: {bg_left}"
    assert abs(bg_right - 2000) < 30, f"右侧背景估计偏差: {bg_right}"
    print("  [PASS] 不同区域背景自适应")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("动态区域迭代阈值背景分离 - 单元测试")
    print("=" * 60)

    tests = [
        ("输出形状正确", test_basic_shape),
        ("细节层 = 原图 - 背景层", test_detail_equals_original_minus_background),
        ("星点位置细节层有正信号", test_star_signal_in_detail),
        ("背景层星点区域平滑填充", test_background_smooth_at_stars),
        ("不同区域背景自适应", test_regional_adaptation),
    ]

    passed = 0
    failed = 0
    for name, test_fn in tests:
        print(f"\n[TEST] {name}")
        try:
            test_fn()
            passed += 1
        except Exception as e:
            print(f"  [FAIL] {e}")
            failed += 1

    print(f"\n{'=' * 60}")
    print(f"测试结果: {passed} passed, {failed} failed")
    print(f"{'=' * 60}")
    return failed == 0


def output_fits_comparison():
    """输出背景层、细节层、à-trous细节层的FITS文件供可视化对比"""
    print("\n" + "=" * 60)
    print("输出FITS对比文件")
    print("=" * 60)

    test_data_dir = Path(r"F:\Astro dev\Astro CS Normalization Database\testdata")
    output_dir = Path(r"F:\Astro dev\Astro CS Normalization Database\lib\star_detector\test_output")
    output_dir.mkdir(exist_ok=True)

    fits_files = list(test_data_dir.rglob("*Red.fts"))
    if not fits_files:
        fits_files = list(test_data_dir.rglob("*.fts"))
    if not fits_files:
        print("未找到测试FITS文件")
        return

    image_path = fits_files[0]
    print(f"测试图像: {image_path.name}")

    from astropy.io import fits as pyfits
    with pyfits.open(str(image_path)) as hdul:
        data = hdul[0].data
        if data.dtype != np.uint16:
            data = np.clip(data, 0, 65535).astype(np.uint16)

    print(f"图像尺寸: {data.shape}")
    image = data.astype(np.float32)

    t0 = time.time()
    bg, detail = dynamic_regional_background(image)
    t1 = time.time()
    print(f"动态阈值背景分离: {t1 - t0:.2f}s")

    bg_path = output_dir / "dynamic_background.fits"
    detail_path = output_dir / "dynamic_detail.fits"
    pyfits.PrimaryHDU(bg.astype(np.float32)).writeto(str(bg_path), overwrite=True)
    pyfits.PrimaryHDU(detail.astype(np.float32)).writeto(str(detail_path), overwrite=True)
    print(f"背景层: {bg_path}")
    print(f"细节层: {detail_path}")

    sys.path.insert(0, str(Path(__file__).parent))
    from star_detector import StarDetector, SDetParamsPy
    from ctypes import POINTER, c_float, c_double, c_int, c_uint16, c_void_p, byref

    params = SDetParamsPy(iterativeClipSigma=9.0, fitRadius=8)
    detector = StarDetector(params=params)

    c_data = np.ascontiguousarray(data, dtype=np.uint16)
    height, width = c_data.shape
    data_ptr = c_data.ctypes.data_as(POINTER(c_uint16))

    out_x = POINTER(c_double)()
    out_y = POINTER(c_double)()
    out_count = c_int(0)
    out_detail = POINTER(c_float)()
    out_smap = POINTER(c_float)()
    out_binary = POINTER(c_float)()

    dll = detector._dll
    dll.sdet_detect_debug.argtypes = [
        c_void_p, POINTER(c_uint16), c_int, c_int,
        POINTER(POINTER(c_double)), POINTER(POINTER(c_double)), POINTER(c_int),
        POINTER(POINTER(c_float)), POINTER(POINTER(c_float)), POINTER(POINTER(c_float)),
    ]
    dll.sdet_detect_debug.restype = c_int
    dll.sdet_free_debug_maps.argtypes = [POINTER(c_float)]
    dll.sdet_free_debug_maps.restype = None

    ret = dll.sdet_detect_debug(
        detector._handle, data_ptr, width, height,
        byref(out_x), byref(out_y), byref(out_count),
        byref(out_detail), byref(out_smap), byref(out_binary),
    )

    if ret == 0 and out_detail:
        n_pixels = height * width
        atrous_arr = np.ctypeslib.as_array(out_detail, shape=(n_pixels,)).copy()
        atrous_detail = atrous_arr.reshape(height, width)
        atrous_path = output_dir / "atrous_detail.fits"
        pyfits.PrimaryHDU(atrous_detail.astype(np.float32)).writeto(str(atrous_path), overwrite=True)
        print(f"à-trous细节层: {atrous_path}")
        dll.sdet_free_debug_maps(out_detail)
        if out_smap:
            dll.sdet_free_debug_maps(out_smap)
        if out_binary:
            dll.sdet_free_debug_maps(out_binary)
        dll.sdet_free_coords(out_x)
        dll.sdet_free_coords(out_y)
    else:
        print("获取à-trous细节层失败")

    detector.close()

    print(f"\n对比统计:")
    print(f"  动态细节层 - med={np.median(detail):.2f}, mad={np.median(np.abs(detail - np.median(detail)))*1.4826:.2f}, max={np.max(detail):.1f}")
    print(f"  动态背景层 - med={np.median(bg):.2f}, min={np.min(bg):.1f}, max={np.max(bg):.1f}")


def main():
    all_passed = run_tests()
    if all_passed:
        output_fits_comparison()


if __name__ == "__main__":
    main()
