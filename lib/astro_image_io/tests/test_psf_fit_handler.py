"""
测试 psf_fit_handler
功能: 验证 PSF 拟合 handler 的逻辑（star_det 块读取、data 块读取、psf 块写入）
用途: 确认 orchestrator.make_psf_fit_handler 在管线中能正确工作

测试场景:
  1. 正常流程: star_det + data → psf 块 (shape (5,6))
  2. 无 star_det 块: 跳过 PSF 拟合 (返回 0, 无 psf 块)
  3. 无 data 块: 返回错误 (返回 -1)
  4. DLL 不可用时: 使用 mock 验证逻辑路径
"""

import os
import sys
import traceback
from ctypes import create_string_buffer

import numpy as np

# 添加 python 目录到 path
_HERE = os.path.dirname(os.path.abspath(__file__))
_PYTHON_DIR = os.path.normpath(os.path.join(_HERE, "..", "python"))
if _PYTHON_DIR not in sys.path:
    sys.path.insert(0, _PYTHON_DIR)

from astro_image_io import PipelineFramePy
from orchestrator import make_psf_fit_handler


# ============================================================================
# 辅助函数
# ============================================================================

PASS = 0
FAIL = 0


def _check_dll_available():
    """检查 dynamic_psf DLL 是否可用"""
    try:
        from dynamic_psf import DynamicPSF
        DynamicPSF._ensure_dll()
        return True
    except Exception:
        return False


def _assert(cond, msg):
    global PASS, FAIL
    if cond:
        PASS += 1
        print(f"  [OK] {msg}")
    else:
        FAIL += 1
        print(f"  [FAIL] {msg}")


def _make_test_frame(n_stars=5, img_size=64):
    """构造含 star_det 块 + data 块的 frame"""
    frame = PipelineFramePy()

    # star_det: float32[N, 4] (cx, cy, flux, snr)
    star_det = np.array([
        [10.0, 10.0, 1000.0, 50.0],
        [20.0, 20.0, 2000.0, 60.0],
        [30.0, 30.0, 1500.0, 55.0],
        [40.0, 40.0, 1800.0, 65.0],
        [50.0, 50.0, 1200.0, 45.0],
    ], dtype=np.float32)[:n_stars]
    frame.add_block("star_det", star_det, description="星点检测坐标")

    # data: float32[64, 64] - 模拟星空图像（在星点位置放高斯峰）
    data = np.full((img_size, img_size), 100.0, dtype=np.float32)
    for cx, cy, flux, _ in star_det:
        x, y = int(cx), int(cy)
        data[y, x] = flux
        if x > 0:
            data[y, x - 1] = flux * 0.5
        if x < img_size - 1:
            data[y, x + 1] = flux * 0.5
        if y > 0:
            data[y - 1, x] = flux * 0.5
        if y < img_size - 1:
            data[y + 1, x] = flux * 0.5
    frame.add_block("data", data, description="像素数据")

    return frame


# ============================================================================
# 测试用例
# ============================================================================

def test_psf_fit_normal():
    """测试1: 正常流程 - star_det + data → psf 块"""
    print("\n=== 测试1: 正常流程 ===")
    dll_ok = _check_dll_available()
    frame = _make_test_frame(n_stars=5, img_size=64)

    if dll_ok:
        print("  [INFO] dynamic_psf DLL 可用，执行真实 PSF 拟合")
        handler_c = make_psf_fit_handler()
        err_buf = create_string_buffer(512)
        ret = handler_c(frame.c_frame, None, err_buf, 512)
    else:
        print("  [INFO] dynamic_psf DLL 不可用，使用 mock 验证逻辑")
        from dynamic_psf import DynamicPSF, DPSFFitResultPy
        from unittest.mock import patch

        mock_results = [
            DPSFFitResultPy(status=0, B=100.0, A=900.0, cx=10.0, cy=10.0,
                            fwhm_x=3.0, fwhm_y=3.2, flux=5000.0),
            DPSFFitResultPy(status=0, B=100.0, A=1900.0, cx=20.0, cy=20.0,
                            fwhm_x=2.8, fwhm_y=3.0, flux=6000.0),
            DPSFFitResultPy(status=0, B=100.0, A=1400.0, cx=30.0, cy=30.0,
                            fwhm_x=3.1, fwhm_y=3.3, flux=5500.0),
            DPSFFitResultPy(status=0, B=100.0, A=1700.0, cx=40.0, cy=40.0,
                            fwhm_x=2.9, fwhm_y=3.1, flux=5800.0),
            DPSFFitResultPy(status=0, B=100.0, A=1100.0, cx=50.0, cy=50.0,
                            fwhm_x=3.0, fwhm_y=3.0, flux=5200.0),
        ]
        with patch.object(DynamicPSF, 'fit_batch', return_value=mock_results):
            handler_c = make_psf_fit_handler()
            err_buf = create_string_buffer(512)
            ret = handler_c(frame.c_frame, None, err_buf, 512)

    _assert(ret == 0, f"handler 应返回 0 (成功), 实际 {ret}")
    if ret != 0:
        err_msg = err_buf.value.decode('utf-8', errors='replace')
        print(f"  错误信息: {err_msg}")

    _assert(frame.has_block("psf"), "psf 块应存在")
    if frame.has_block("psf"):
        psf = frame.get_block_data("psf")
        _assert(psf is not None, "psf 块数据不为 None")
        _assert(psf.shape == (5, 6), f"psf shape 应为 (5, 6), 实际 {psf.shape}")
        _assert(psf.dtype == np.float64, f"psf dtype 应为 float64, 实际 {psf.dtype}")
        print(f"  psf 数据 (status, B, flux, cx, cy, fwhm):")
        for i in range(5):
            print(f"    星{i}: status={int(psf[i, 0])}, B={psf[i, 1]:.1f}, "
                  f"flux={psf[i, 2]:.1f}, cx={psf[i, 3]:.2f}, cy={psf[i, 4]:.2f}, "
                  f"fwhm={psf[i, 5]:.2f}")

    frame.close()


def test_psf_fit_no_star_det():
    """测试2: 无 star_det 块 - 跳过 PSF 拟合"""
    print("\n=== 测试2: 无 star_det 块 ===")
    frame = PipelineFramePy()
    data = np.zeros((32, 32), dtype=np.float32)
    frame.add_block("data", data, description="像素数据")

    handler_c = make_psf_fit_handler()
    err_buf = create_string_buffer(512)
    ret = handler_c(frame.c_frame, None, err_buf, 512)

    _assert(ret == 0, f"无 star_det 时应返回 0 (跳过), 实际 {ret}")
    _assert(not frame.has_block("psf"), "无 star_det 时不应写入 psf 块")

    frame.close()


def test_psf_fit_no_data():
    """测试3: 无 data 块 - 返回错误"""
    print("\n=== 测试3: 无 data 块 ===")
    frame = PipelineFramePy()
    star_det = np.array([[10.0, 10.0, 100.0, 50.0]], dtype=np.float32)
    frame.add_block("star_det", star_det, description="星点坐标")

    handler_c = make_psf_fit_handler()
    err_buf = create_string_buffer(512)
    ret = handler_c(frame.c_frame, None, err_buf, 512)

    _assert(ret == -1, f"无 data 块时应返回 -1, 实际 {ret}")
    # 注意: 从 Python 端调用 CFUNCTYPE 回调时, c_char_p 参数被转为空 bytes (falsy),
    # _write_error 的 `if err_buf` 检查会跳过写入。这是 ctypes 回调机制的已知行为,
    # 在实际 C 端管线引擎调用时 err_buf 由 C 端管理。此处仅验证返回值。

    frame.close()


# ============================================================================
# 主入口
# ============================================================================

if __name__ == "__main__":
    print("=" * 60)
    print("PSF 拟合 handler 测试")
    print("=" * 60)

    try:
        test_psf_fit_normal()
        test_psf_fit_no_star_det()
        test_psf_fit_no_data()
    except Exception:
        traceback.print_exc()
        FAIL += 1

    print("\n" + "=" * 60)
    print(f"结果: {PASS} 通过, {FAIL} 失败")
    print("=" * 60)
    sys.exit(0 if FAIL == 0 else 1)
