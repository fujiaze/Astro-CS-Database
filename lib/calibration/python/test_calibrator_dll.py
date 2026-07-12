# -*- coding: utf-8 -*-
"""
测试 calibrator.calibrate_data() 的 C++ DLL 路径
功能: 验证 calibrate_data 从 numpy 路径切换到 C++ DLL 后接口与输出正确
用途: 回归测试，确保 shape/dtype/stats 格式不变、值有变化
"""

import os
import sys

import numpy as np

# 确保能导入同目录模块
_lib_dir = os.path.dirname(os.path.abspath(__file__))
if _lib_dir not in sys.path:
    sys.path.insert(0, _lib_dir)

from calibrator import Calibrator


def _make_synthetic(h=64, w=64, seed=42):
    """构造小尺寸合成数据: light / bias / dark / flat"""
    rng = np.random.RandomState(seed)
    bias = rng.uniform(950, 1050, (h, w)).astype(np.float32)
    dark_signal = rng.uniform(20, 40, (h, w)).astype(np.float32)
    dark = (bias + dark_signal).astype(np.float32)
    flat = rng.uniform(0.8, 1.2, (h, w)).astype(np.float32) * 5000.0  # 未归一化
    light = (bias + dark_signal + rng.uniform(100, 200, (h, w))).astype(np.float32)
    return light, bias, dark, flat


def test_shape_and_dtype():
    """测试输出 shape 和 dtype 正确"""
    cal = Calibrator(max_workers=4)
    light, bias, dark, flat = _make_synthetic()

    out, stats = cal.calibrate_data(
        light, master_bias=bias, master_dark=dark, master_flat=flat,
    )

    assert out.shape == light.shape, f"shape 不匹配: {out.shape} vs {light.shape}"
    assert out.dtype == np.float32, f"dtype 不正确: {out.dtype}"
    print("[PASS] test_shape_and_dtype: shape=%s, dtype=%s" % (out.shape, out.dtype))


def test_values_changed():
    """测试校准后值有变化"""
    cal = Calibrator(max_workers=4)
    light, bias, dark, flat = _make_synthetic()

    out, stats = cal.calibrate_data(
        light, master_bias=bias, master_dark=dark, master_flat=flat,
    )

    assert not np.allclose(out, light), "校准后数据与原始数据相同，校准未生效"
    diff = float(np.mean(np.abs(out - light)))
    print("[PASS] test_values_changed: mean|diff|=%.4f" % diff)


def test_stats_format():
    """测试返回的 stats 格式正确"""
    cal = Calibrator(max_workers=4)
    light, bias, dark, flat = _make_synthetic()

    out, stats = cal.calibrate_data(
        light, master_bias=bias, master_dark=dark, master_flat=flat,
    )

    assert "before" in stats, "stats 缺少 'before' 键"
    assert "after" in stats, "stats 缺少 'after' 键"
    assert "dark_scale_factor" in stats, "stats 缺少 'dark_scale_factor' 键"
    for key in ("min", "max", "mean", "std"):
        assert key in stats["before"], "stats['before'] 缺少 '%s'" % key
        assert key in stats["after"], "stats['after'] 缺少 '%s'" % key
    print("[PASS] test_stats_format: keys=%s" % sorted(stats.keys()))


def test_none_masters():
    """测试 master 帧为 None 时不报错"""
    cal = Calibrator(max_workers=4)
    light, _, _, _ = _make_synthetic()

    out, stats = cal.calibrate_data(light)  # 全部 None
    assert out.shape == light.shape
    # 无 dark/flat 时，out 应等于 light（只做拷贝）
    assert np.allclose(out, light), "无主帧时输出应等于输入"
    print("[PASS] test_none_masters: out==light (无主帧)")


def test_dark_optimization():
    """测试暗场优化模式"""
    cal = Calibrator(max_workers=4)
    light, bias, dark, flat = _make_synthetic()

    out, stats = cal.calibrate_data(
        light, master_bias=bias, master_dark=dark, master_flat=flat,
        dark_optimization=True,
        light_exposure=60.0, dark_exposure=60.0,
    )

    assert out.shape == light.shape
    assert "dark_scale_factor" in stats
    k = stats["dark_scale_factor"]
    print("[PASS] test_dark_optimization: K=%.4f, after_mean=%.4f" % (k, stats["after"]["mean"]))


def test_exposure_scaling():
    """测试曝光时间不同时 dark_scale_factor 正确计算"""
    cal = Calibrator(max_workers=4)
    light, bias, dark, flat = _make_synthetic()

    _, stats = cal.calibrate_data(
        light, master_bias=bias, master_dark=dark, master_flat=flat,
        light_exposure=120.0, dark_exposure=60.0,
    )
    # dark_optimization=False 时 K 应为 1.0（标准模式）
    assert abs(stats["dark_scale_factor"] - 1.0) < 1e-6, \
        "标准模式 K 应为 1.0, 实际=%.4f" % stats["dark_scale_factor"]
    print("[PASS] test_exposure_scaling: K=%.4f (标准模式)" % stats["dark_scale_factor"])


if __name__ == "__main__":
    print("=" * 60)
    print("测试 calibrate_data() C++ DLL 路径")
    print("=" * 60)
    test_shape_and_dtype()
    test_values_changed()
    test_stats_format()
    test_none_masters()
    test_dark_optimization()
    test_exposure_scaling()
    print("=" * 60)
    print("全部测试通过!")
