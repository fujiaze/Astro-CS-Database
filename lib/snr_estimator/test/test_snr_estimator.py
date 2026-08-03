# NON_PRODUCTION_TOOL_ONLY
# This file is NOT part of the production pipeline.
# It is a development/testing/research tool only.
# The production pipeline uses orchestrator.exe <stage1.json> exclusively.

# -*- coding: utf-8 -*-
"""
test_snr_estimator.py - SNR估算模块测试
功能: 验证 snr_estimate C++ DLL 的 6 项测试
依赖: numpy, ctypes, pytest(可选)
运行: python test_snr_estimator.py
      python -m pytest test_snr_estimator.py -v
"""

from __future__ import annotations

import os
import sys
import math
from typing import Tuple

import numpy as np

# 确保能导入 snr_estimator 模块
_this_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_this_dir, "..", "python"))

from snr_estimator import SNREstimator  # noqa: E402

LN10 = math.log(10.0)


def _make_psf(n: int, w: int, h: int,
              A: float = 500.0, B: float = 100.0, mad: float = 10.0,
              status: int = 0) -> np.ndarray:
    """构造 PSF 星数组 [n, 9], 星点均匀分布在图像内"""
    psf = np.zeros((n, 9), dtype=np.float64)
    xs = np.linspace(20, w - 20, n) if n > 1 else np.array([w / 2.0])
    ys = np.linspace(20, h - 20, n) if n > 1 else np.array([h / 2.0])
    for i in range(n):
        psf[i] = [status, B, 50000.0, xs[i], ys[i], 3.0, A, mad, 0.1]
    return psf


def _load_estimator() -> SNREstimator:
    """加载DLL, 失败则跳过测试"""
    return SNREstimator()


# ============================================================================
# 测试 1: 正常路径 - 10颗PSF星 + sigma_residual=0.1, 验证SNR公式
# ============================================================================
def test_1_normal_path():
    print("\n=== 测试 1: 正常路径 (10颗PSF星, sigma=0.1) ===")
    est = _load_estimator()
    w, h = 100, 100
    image = np.full((h, w), 1000.0, dtype=np.float32)
    psf = _make_psf(10, w, h, A=500.0, B=100.0, mad=10.0)
    sigma = 0.1

    snr, code = est.estimate(image, psf, sigma)

    # SNR_phot = 1/(ln(10)*0.1) ≈ 4.342945
    snr_phot = 1.0 / (LN10 * sigma)
    # 所有星 SNR_psf = (500-100)/10 = 40, median=40
    # SNR = SNR_phot * (40/40) = SNR_phot
    assert code == 0, f"返回码应为0, 实际={code}"

    # 在星位置附近, SNR_psf = 40, SNR = SNR_phot
    center_val = float(snr[h // 2, w // 2])
    print(f"  SNR_phot = {snr_phot:.6f}")
    print(f"  snr[center] = {center_val:.6f}")
    assert abs(center_val - snr_phot) < 0.01, \
        f"中心SNR={center_val}, 期望={snr_phot}"

    # 全图应该接近 snr_phot (因为所有星 SNR_psf 相同, median=星值)
    mean_val = float(snr.mean())
    print(f"  snr.mean = {mean_val:.6f}")
    assert abs(mean_val - snr_phot) < 0.1, \
        f"SNR均值={mean_val}, 期望≈{snr_phot}"

    print("  [PASS] 正常路径SNR公式验证")
    return True


# ============================================================================
# 测试 2: n_stars=0 退化 - out_snr 全填 SNR_phot
# ============================================================================
def test_2_degenerate_n_stars_zero():
    print("\n=== 测试 2: n_stars=0 退化 (全填SNR_phot) ===")
    est = _load_estimator()
    w, h = 50, 50
    image = np.full((h, w), 1000.0, dtype=np.float32)
    psf = np.zeros((0, 9), dtype=np.float64)
    sigma = 0.05

    snr, code = est.estimate(image, psf, sigma)

    snr_phot = 1.0 / (LN10 * sigma)
    assert code == 1, f"返回码应为1, 实际={code}"

    # 全填 SNR_phot
    expected = snr_phot
    actual = float(snr[0, 0])
    print(f"  SNR_phot = {expected:.6f}")
    print(f"  snr[0,0] = {actual:.6f}")
    assert abs(actual - expected) < 1e-4, \
        f"snr[0,0]={actual}, 期望={expected}"

    # 检查全图一致
    assert np.allclose(snr, expected, atol=1e-4), "全图应全填SNR_phot"
    print(f"  全图一致: {np.allclose(snr, expected, atol=1e-4)}")
    print("  [PASS] n_stars=0退化路径")
    return True


# ============================================================================
# 测试 3: sigma_residual=0 退化 - out_snr 全填 1.0
# ============================================================================
def test_3_degenerate_sigma_zero():
    print("\n=== 测试 3: sigma_residual=0 退化 (全填1.0) ===")
    est = _load_estimator()
    w, h = 50, 50
    image = np.full((h, w), 1000.0, dtype=np.float32)
    psf = _make_psf(10, w, h)
    sigma = 0.0

    snr, code = est.estimate(image, psf, sigma)

    assert code == 2, f"返回码应为2, 实际={code}"

    actual = float(snr[0, 0])
    print(f"  snr[0,0] = {actual:.6f} (期望 1.0)")
    assert abs(actual - 1.0) < 1e-6, f"snr[0,0]={actual}, 期望=1.0"

    assert np.allclose(snr, 1.0, atol=1e-6), "全图应全填1.0"
    print(f"  全图一致: {np.allclose(snr, 1.0, atol=1e-6)}")
    print("  [PASS] sigma_residual=0退化路径")
    return True


# ============================================================================
# 测试 4: PSF星 A<=B 跳过
# ============================================================================
def test_4_skip_a_le_b():
    print("\n=== 测试 4: PSF星 A<=B 跳过 ===")
    est = _load_estimator()
    w, h = 100, 100
    image = np.full((h, w), 1000.0, dtype=np.float32)
    sigma = 0.1
    snr_phot = 1.0 / (LN10 * sigma)

    # 混合: 5颗有效星(A>B) + 5颗无效星(A<=B)
    psf = np.zeros((10, 9), dtype=np.float64)
    xs = np.linspace(20, 80, 10)
    for i in range(10):
        if i < 5:
            # 有效星: A=500 > B=100
            psf[i] = [0, 100, 50000, xs[i], 50.0, 3.0, 500, 10, 0.1]
        else:
            # 无效星: A=50 <= B=100, 应跳过
            psf[i] = [0, 100, 50000, xs[i], 50.0, 3.0, 50, 10, 0.1]

    snr, code = est.estimate(image, psf, sigma)
    assert code == 0, f"返回码应为0, 实际={code}"

    # 5颗有效星 SNR_psf = (500-100)/10 = 40, median=40
    # SNR = SNR_phot * (40/40) = SNR_phot
    center_val = float(snr[h // 2, w // 2])
    print(f"  SNR_phot = {snr_phot:.6f}")
    print(f"  snr[center] = {center_val:.6f}")
    assert abs(center_val - snr_phot) < 0.01, \
        f"中心SNR={center_val}, 期望={snr_phot} (A<=B星应被跳过)"

    print("  [PASS] A<=B星跳过验证")
    return True


# ============================================================================
# 测试 5: nullptr 检查 - 返回错误码 3
# ============================================================================
def test_5_nullptr_check():
    print("\n=== 测试 5: nullptr 检查 (返回3) ===")
    est = _load_estimator()
    w, h = 50, 50

    # 直接调用底层DLL, 传 None 指针
    from ctypes import POINTER, c_float, c_int, c_double
    # 传 data=None
    psf = _make_psf(5, w, h)
    out_snr = np.zeros(w * h, dtype=np.float32)

    ret = est._dll.snr_estimate(
        None, h, w,  # data = None
        psf.ctypes.data_as(POINTER(c_double)), 5,
        c_double(0.1),
        out_snr.ctypes.data_as(POINTER(c_float)),
    )
    print(f"  data=None -> ret={ret}")
    assert ret == 3, f"data=None应返回3, 实际={ret}"

    # 传 psf=None
    image = np.full((h, w), 1000.0, dtype=np.float32)
    ret = est._dll.snr_estimate(
        image.ctypes.data_as(POINTER(c_float)), h, w,
        None, 5,  # psf = None
        c_double(0.1),
        out_snr.ctypes.data_as(POINTER(c_float)),
    )
    print(f"  psf=None -> ret={ret}")
    assert ret == 3, f"psf=None应返回3, 实际={ret}"

    # 传 out_snr=None
    ret = est._dll.snr_estimate(
        image.ctypes.data_as(POINTER(c_float)), h, w,
        psf.ctypes.data_as(POINTER(c_double)), 5,
        c_double(0.1),
        None,  # out_snr = None
    )
    print(f"  out_snr=None -> ret={ret}")
    assert ret == 3, f"out_snr=None应返回3, 实际={ret}"

    print("  [PASS] nullptr检查")
    return True


# ============================================================================
# 测试 6: SNR数值范围 - 全部 > 0
# ============================================================================
def test_6_positive_range():
    print("\n=== 测试 6: SNR数值范围 (全部>0) ===")
    est = _load_estimator()
    w, h = 80, 80
    image = np.full((h, w), 500.0, dtype=np.float32)

    # 不同 A/B/mad 值的星, 制造空间变化
    psf = np.zeros((8, 9), dtype=np.float64)
    xs = np.linspace(15, w - 15, 4)
    ys = np.linspace(15, h - 15, 2)
    idx = 0
    for x in xs:
        for y in ys:
            # A 在 200-800, B=100, mad=5-20
            A = 200.0 + idx * 80
            B = 100.0
            mad = 5.0 + idx * 2
            psf[idx] = [0, B, 50000, x, y, 3.0, A, mad, 0.1]
            idx += 1

    sigma = 0.15
    snr, code = est.estimate(image, psf, sigma)
    assert code == 0, f"返回码应为0, 实际={code}"

    snr_flat = snr.ravel()
    min_val = float(snr_flat.min())
    max_val = float(snr_flat.max())
    print(f"  SNR min = {min_val:.6f}")
    print(f"  SNR max = {max_val:.6f}")
    print(f"  SNR > 0 count: {np.sum(snr_flat > 0)} / {snr_flat.size}")

    assert np.all(snr_flat > 0), f"存在非正SNR值: min={min_val}"
    assert min_val > 0, f"最小SNR={min_val} 应>0"

    print("  [PASS] SNR数值范围全部>0")
    return True


# ============================================================================
# 主函数: 运行全部测试
# ============================================================================
def main():
    print("=" * 60)
    print("SNR Estimator 模块测试 (6项)")
    print("=" * 60)

    tests = [
        ("正常路径", test_1_normal_path),
        ("n_stars=0退化", test_2_degenerate_n_stars_zero),
        ("sigma=0退化", test_3_degenerate_sigma_zero),
        ("A<=B跳过", test_4_skip_a_le_b),
        ("nullptr检查", test_5_nullptr_check),
        ("数值范围>0", test_6_positive_range),
    ]

    results = []
    for name, fn in tests:
        try:
            ok = fn()
            results.append((name, ok))
        except Exception as e:
            print(f"  [FAIL] {name}: {e}")
            results.append((name, False))

    print("\n" + "=" * 60)
    print("测试结果汇总")
    print("=" * 60)
    n_pass = sum(1 for _, ok in results if ok)
    for name, ok in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")
    print(f"\n  {n_pass}/{len(results)} 通过")
    return 0 if n_pass == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
