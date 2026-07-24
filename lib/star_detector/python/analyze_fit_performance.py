"""
Star Detector 拟合性能分析脚本
功能: 详细分析Moffat4拟合的性能瓶颈
用途: 统计连通域特征分布、拟合失败率、高耗时星点等
"""

import sys
import time
import numpy as np
from collections import Counter

sys.path.insert(0, 'lib/star_detector/python')
sys.path.insert(0, 'lib/astro_image_io/python')

from star_detector import StarDetector, SDetParamsPy
from astro_image_io import ImageReader


def analyze_star_detector(image_path: str):
    """详细分析星点检测器的拟合性能"""

    # 读取图像
    reader = ImageReader()
    img_data = reader.read(image_path)
    image = img_data.data
    h, w = image.shape
    print(f"图像: {w}x{h}, dtype={image.dtype}, range=[{image.min()}, {image.max()}]")

    # 获取debug输出（细节层+二值图）
    params = SDetParamsPy()
    det = StarDetector(params=params)

    # 使用detect_ex获取基本结果
    t0 = time.time()
    result = det.detect_ex(image, extra_names=['fwhm_x', 'fwhm_y', 'sx', 'sy', 'r'])
    t1 = time.time()
    total_time = t1 - t0

    print(f"\n{'='*60}")
    print(f"总体结果")
    print(f"{'='*60}")
    print(f"总耗时: {total_time:.3f}s")
    print(f"总星数: {result.count}")
    print(f"正常星: {result.normal_count}")
    print(f"饱和星: {result.saturated_count}")

    # 使用C++的连通域分析来获取详细的候选统计
    # 我们需要重新实现连通域分析来获取详细统计
    fimg = image.astype(np.float32)

    # 动态背景分离（通过C++完成，获取细节层）
    # 使用debug接口获取二值图
    import ctypes
    from ctypes import POINTER, c_float, c_int, c_double, c_uint16, byref, c_char_p

    # 直接用numpy做连通域分析来统计
    # 先获取细节层 - 通过C++的debug接口
    # 简化：直接在Python中做分析

    # 半阈值二值化统计
    img_min = float(fimg.min())
    img_max = float(fimg.max())
    half_threshold = (img_max + img_min) / 2.0
    print(f"\n半阈值: {half_threshold:.1f} (min={img_min:.1f}, max={img_max:.1f})")

    # 细节层>0二值化统计
    # 通过C++获取细节层
    result2 = det._dll.sdet_detect_debug(
        det._handle,
        image.ctypes.data_as(POINTER(c_uint16)),
        w, h,
        byref(ctypes.POINTER(c_double)()),
        byref(ctypes.POINTER(c_double)()),
        byref(c_int()),
        byref(ctypes.POINTER(c_float)()),
        byref(ctypes.POINTER(c_float)()),
        byref(ctypes.POINTER(c_float)()),
        None, 0,
        byref(ctypes.POINTER(ctypes.POINTER(c_float))()),
    )

    # 读取细节层和二值图
    # 由于debug接口返回的是指针，我们需要知道图像大小
    n = w * h

    # 重新获取debug数据
    out_x = ctypes.POINTER(c_double)()
    out_y = ctypes.POINTER(c_double)()
    out_count = c_int(0)
    out_detail = ctypes.POINTER(c_float)()
    out_smap = ctypes.POINTER(c_float)()
    out_binary = ctypes.POINTER(c_float)()

    ret = det._dll.sdet_detect_debug(
        det._handle,
        image.ctypes.data_as(POINTER(c_uint16)),
        w, h,
        byref(out_x), byref(out_y), byref(out_count),
        byref(out_detail), byref(out_smap), byref(out_binary),
        None, 0,
        byref(ctypes.POINTER(ctypes.POINTER(c_float))()),
    )

    if ret != 0:
        print(f"debug检测失败: {ret}")
        return

    # 拷贝细节层数据
    detail_np = np.ctypeslib.as_array(out_detail, shape=(n,)).copy()
    binary_np = np.ctypeslib.as_array(out_binary, shape=(n,)).copy()
    det._dll.sdet_free_debug_maps(out_detail)
    det._dll.sdet_free_debug_maps(out_smap)
    det._dll.sdet_free_debug_maps(out_binary)
    det._dll.sdet_free_coords(out_x)
    det._dll.sdet_free_coords(out_y)

    detail_2d = detail_np.reshape(h, w)
    binary_2d = binary_np.reshape(h, w)

    print(f"\n细节层统计: min={detail_np.min():.2f}, max={detail_np.max():.2f}, "
          f"mean={detail_np.mean():.2f}, >0像素数={np.sum(detail_np > 0)}")
    print(f"二值图统计: >0像素数={np.sum(binary_np > 0)}")

    # Python连通域分析
    from scipy import ndimage

    labeled, num_features = ndimage.label(binary_2d > 0)
    print(f"\n连通域总数: {num_features}")

    # 统计每个连通域的特征
    component_sizes = []
    component_bboxes = []
    component_aspect_ratios = []
    component_max_vals = []
    component_mean_vals = []

    for i in range(1, num_features + 1):
        mask = (labeled == i)
        size = int(np.sum(mask))
        if size <= 4:
            continue

        ys, xs = np.where(mask)
        bw = int(xs.max() - xs.min() + 1)
        bh = int(ys.max() - ys.min() + 1)
        ar = max(bw, bh) / max(min(bw, bh), 1)

        vals = fimg[mask]
        component_sizes.append(size)
        component_bboxes.append((bw, bh))
        component_aspect_ratios.append(ar)
        component_max_vals.append(float(vals.max()))
        component_mean_vals.append(float(vals.mean()))

    if not component_sizes:
        print("无有效连通域")
        return

    sizes = np.array(component_sizes)
    ars = np.array(component_aspect_ratios)
    maxvals = np.array(component_max_vals)
    meanvals = np.array(component_mean_vals)

    print(f"\n{'='*60}")
    print(f"连通域特征分布（过滤≤4px后）")
    print(f"{'='*60}")
    print(f"候选数: {len(sizes)}")

    # 像素数分布
    print(f"\n--- 像素数分布 ---")
    bins = [5, 10, 20, 50, 100, 200, 500, 1000, 5000]
    for i in range(len(bins) + 1):
        if i == 0:
            lo, hi = 5, bins[0]
            label = f"  {lo}-{hi}"
        elif i < len(bins):
            lo, hi = bins[i-1], bins[i]
            label = f"  {lo}-{hi}"
        else:
            lo = bins[-1]
            count = int(np.sum(sizes >= lo))
            print(f"{label:>12s} px: {count:>6d} ({count/len(sizes)*100:.1f}%)")
            continue
        count = int(np.sum((sizes >= lo) & (sizes < hi)))
        print(f"{label:>12s} px: {count:>6d} ({count/len(sizes)*100:.1f}%)")

    # 长宽比分布
    print(f"\n--- 长宽比分布 ---")
    ar_bins = [1.0, 1.5, 2.0, 3.0, 5.0, 10.0, 100.0]
    for i in range(len(ar_bins) + 1):
        if i == 0:
            lo, hi = 0, ar_bins[0]
        elif i < len(ar_bins):
            lo, hi = ar_bins[i-1], ar_bins[i]
        else:
            lo = ar_bins[-1]
            count = int(np.sum(ars >= lo))
            print(f"  ≥{lo:.1f}: {count:>6d} ({count/len(ars)*100:.1f}%)")
            continue
        count = int(np.sum((ars >= lo) & (ars < hi)))
        print(f"  {lo:.1f}-{hi:.1f}: {count:>6d} ({count/len(ars)*100:.1f}%)")

    # 拟合区域面积分析（fitRadius=8, 17x17=289像素）
    fit_radius = 8
    fit_area = (2 * fit_radius + 1) ** 2
    print(f"\n--- 拟合区域分析 (fitRadius={fit_radius}, 区域={fit_area}像素) ---")
    print(f"  候选数: {len(sizes)}")
    print(f"  每候选拟合区域: ~{fit_area}像素")
    print(f"  总拟合像素: ~{len(sizes) * fit_area:,} ({len(sizes)*fit_area*4/1024/1024:.0f}MB float32)")

    # 按像素数分类的拟合成本估算
    print(f"\n--- 按像素数分类的拟合成本估算 ---")
    # Moffat4拟合时间主要取决于：1) 采样像素数 2) 迭代次数
    # 采样区域固定为fitRadius，所以每个候选的采样成本相同
    # 但小连通域更可能是噪声，拟合成功率低
    size_categories = [
        ("5-9 (极小)", 5, 10),
        ("10-19 (小)", 10, 20),
        ("20-49 (中)", 20, 50),
        ("50-99 (大)", 50, 100),
        ("100-499 (很大)", 100, 500),
        ("≥500 (巨大)", 500, 999999),
    ]
    for label, lo, hi in size_categories:
        mask = (sizes >= lo) & (sizes < hi)
        count = int(np.sum(mask))
        if count == 0:
            continue
        pct = count / len(sizes) * 100
        # 估算拟合时间占比（每个候选耗时相同）
        time_pct = pct  # 假设每个候选耗时相同
        print(f"  {label}: {count:>6d} ({pct:.1f}%) → 估算耗时占比 {time_pct:.1f}%")

    # 分析拟合结果
    print(f"\n{'='*60}")
    print(f"拟合结果分析")
    print(f"{'='*60}")

    # 正常星FWHM分布
    if result.normal_count > 0:
        fwhm_x = result.extras.get('fwhm_x', [])
        fwhm_y = result.extras.get('fwhm_y', [])
        if fwhm_x and fwhm_y:
            normal_fwhm_x = [fwhm_x[i] for i in range(result.count) if result.saturated[i] == 0]
            normal_fwhm_y = [fwhm_y[i] for i in range(result.count) if result.saturated[i] == 0]
            if normal_fwhm_x:
                avg_fwhm = [(normal_fwhm_x[i] + normal_fwhm_y[i])/2 for i in range(len(normal_fwhm_x))]
                avg_fwhm = np.array(avg_fwhm)
                print(f"正常星FWHM: med={np.median(avg_fwhm):.4f}, "
                      f"mad={np.median(np.abs(avg_fwhm - np.median(avg_fwhm))):.4f}, "
                      f"range=[{avg_fwhm.min():.4f}, {avg_fwhm.max():.4f}]")

    # 饱和星r分布
    if result.saturated_count > 0:
        r_vals = result.extras.get('r', [])
        if r_vals:
            sat_r = [r_vals[i] for i in range(result.count) if result.saturated[i] == 1]
            if sat_r:
                sat_r = np.array(sat_r)
                print(f"饱和星r: med={np.median(sat_r):.1f}, range=[{sat_r.min():.1f}, {sat_r.max():.1f}]")

    # 拟合成功率分析
    print(f"\n{'='*60}")
    print(f"拟合成功率分析")
    print(f"{'='*60}")
    total_candidates = len(sizes)
    normal_stars = result.normal_count
    fit_success_rate = normal_stars / total_candidates * 100 if total_candidates > 0 else 0
    print(f"候选数: {total_candidates}")
    print(f"拟合成功: {normal_stars} ({fit_success_rate:.1f}%)")
    print(f"拟合失败: {total_candidates - normal_stars} ({100-fit_success_rate:.1f}%)")
    print(f"  → 每成功1颗星需要拟合 {total_candidates/normal_stars:.1f} 个候选")

    # 噪声候选分析
    print(f"\n{'='*60}")
    print(f"噪声候选分析")
    print(f"{'='*60}")
    # 小连通域(5-9px)更可能是噪声
    small_mask = sizes < 10
    small_count = int(np.sum(small_mask))
    print(f"极小候选(5-9px): {small_count} ({small_count/total_candidates*100:.1f}%)")
    print(f"  这些候选的拟合成功率可能很低，浪费大量时间")

    # 建议优化方向
    print(f"\n{'='*60}")
    print(f"优化建议")
    print(f"{'='*60}")

    # 计算不同过滤阈值下的候选数
    for min_px in [5, 6, 8, 10, 15, 20]:
        remaining = int(np.sum(sizes >= min_px))
        saved_pct = (1 - remaining / total_candidates) * 100
        print(f"  像素数≥{min_px}: 候选{remaining} (减少{saved_pct:.1f}%)")

    # 长宽比过滤
    for max_ar in [2.0, 3.0, 5.0]:
        remaining = int(np.sum(ars <= max_ar))
        saved_pct = (1 - remaining / total_candidates) * 100
        print(f"  长宽比≤{max_ar:.1f}: 候选{remaining} (减少{saved_pct:.1f}%)")

    # 组合过滤
    for min_px in [5, 8, 10]:
        for max_ar in [3.0, 5.0]:
            remaining = int(np.sum((sizes >= min_px) & (ars <= max_ar)))
            saved_pct = (1 - remaining / total_candidates) * 100
            print(f"  像素≥{min_px} + 长宽比≤{max_ar:.1f}: 候选{remaining} (减少{saved_pct:.1f}%)")

    det.close()


if __name__ == '__main__':
    image_path = r'testdata\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@062109-180S-Red.fts'
    analyze_star_detector(image_path)
