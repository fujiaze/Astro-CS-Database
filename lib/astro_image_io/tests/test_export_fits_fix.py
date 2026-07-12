"""
测试 aio_frame_export_block_fits 修复 (SIMPLE逻辑值 + 大端字节序)
功能: 验证导出的 FITS 文件可被 astropy 正确读取, 数据值非垃圾
用途: 确认 C++ 端 FITS 导出 bug 已修复 (SIMPLE卡片格式 + 字节序)
"""

import os
import sys
import shutil
import tempfile
import traceback

import numpy as np
from astropy.io import fits

# 添加 python 目录到 path
_HERE = os.path.dirname(os.path.abspath(__file__))
_PYTHON_DIR = os.path.normpath(os.path.join(_HERE, "..", "python"))
if _PYTHON_DIR not in sys.path:
    sys.path.insert(0, _PYTHON_DIR)

from astro_image_io import PipelineFramePy

# ============================================================================
# 测试工具
# ============================================================================

PASS = 0
FAIL = 0
ERRORS = []


def check(condition, msg):
    global PASS, FAIL
    if condition:
        PASS += 1
        print(f"  [PASS] {msg}")
    else:
        FAIL += 1
        ERRORS.append(msg)
        print(f"  [FAIL] {msg}")


def section(title):
    print(f"\n=== {title} ===")


# ============================================================================
# 测试用例
# ============================================================================

def test_simple_card_format():
    """测试 1: SIMPLE 卡片为逻辑值 T (非字符串 'T')"""
    section("测试 1: SIMPLE 卡片格式")
    tmpdir = tempfile.mkdtemp(prefix="aio_fits_fix_")
    try:
        frame = PipelineFramePy()
        try:
            data = np.arange(16, dtype=np.float32).reshape(4, 4)
            frame.add_block("data", data)
            fits_path = os.path.join(tmpdir, "simple_test.fits")
            frame.export_block_fits("data", fits_path)

            # 用 astropy 读取头
            with fits.open(fits_path) as hdul:
                hdr = hdul[0].header
                # SIMPLE 应为 True (逻辑值), astropy 能正常打开即说明格式正确
                check(hdul[0].header["SIMPLE"] is True,
                      f"SIMPLE == True (逻辑值, 实际={hdr['SIMPLE']})")
                # EXTEND 也应为逻辑值
                check(hdul[0].header["EXTEND"] is True,
                      f"EXTEND == True (逻辑值, 实际={hdr['EXTEND']})")
                # BITPIX 应为 -32 (FLOAT32)
                check(hdul[0].header["BITPIX"] == -32,
                      f"BITPIX == -32 (实际={hdr['BITPIX']})")
        finally:
            frame.close()
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def test_float32_data_correctness():
    """测试 2: FLOAT32 数据值正确 (非垃圾值)"""
    section("测试 2: FLOAT32 数据正确性")
    tmpdir = tempfile.mkdtemp(prefix="aio_fits_fix_")
    try:
        frame = PipelineFramePy()
        try:
            # 构造已知数据
            data = np.array([
                [100.0, 200.0, 300.0, 400.0],
                [500.0, 600.0, 700.0, 800.0],
                [900.0, 1000.0, 1100.0, 1200.0],
                [1300.0, 1400.0, 1500.0, 1600.0],
            ], dtype=np.float32)
            frame.add_block("data", data)

            fits_path = os.path.join(tmpdir, "f32_test.fits")
            frame.export_block_fits("data", fits_path)

            # 用 astropy 读取数据
            with fits.open(fits_path) as hdul:
                out = hdul[0].data
                check(out is not None, "astropy 读取到数据 (非 None)")
                check(out.shape == (4, 4), f"shape == (4,4) (实际={out.shape})")
                check(out.dtype.itemsize == 4 and out.dtype.kind == 'f',
                      f"dtype 为 4字节浮点 (实际={out.dtype})")

                # 关键验证: 数据值不是垃圾 (如 9.8e-44)
                out_flat = out.flatten()
                data_flat = data.flatten()
                check(np.allclose(out_flat, data_flat, rtol=1e-6),
                      f"数据值一致 (max_diff={np.max(np.abs(out_flat - data_flat)):.2e})")

                # 验证不是垃圾值范围
                check(out.min() > 1.0, f"min > 1.0 (实际={out.min()}, 非垃圾值)")
                check(out.max() < 10000.0, f"max < 10000 (实际={out.max()}, 非垃圾值)")
                check(abs(out[0, 0] - 100.0) < 0.01, f"[0,0]==100 (实际={out[0,0]})")
                check(abs(out[3, 3] - 1600.0) < 0.01, f"[3,3]==1600 (实际={out[3,3]})")
        finally:
            frame.close()
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def test_float32_random_data():
    """测试 3: FLOAT32 随机大数据 (64x64) 正确性"""
    section("测试 3: FLOAT32 随机大数据")
    tmpdir = tempfile.mkdtemp(prefix="aio_fits_fix_")
    try:
        frame = PipelineFramePy()
        try:
            np.random.seed(42)
            data = np.random.rand(64, 64).astype(np.float32) * 10000.0
            frame.add_block("data", data)

            fits_path = os.path.join(tmpdir, "f32_random.fits")
            frame.export_block_fits("data", fits_path)

            with fits.open(fits_path) as hdul:
                out = hdul[0].data
                check(out.shape == (64, 64), f"shape == (64,64) (实际={out.shape})")
                check(np.allclose(out, data, rtol=1e-5),
                      f"随机数据一致 (max_rel_err={np.max(np.abs(out-data)/np.maximum(np.abs(data),1e-10)):.2e})")
                # 统计值对比
                check(abs(out.mean() - data.mean()) < 1e-3,
                      f"mean一致 (data={data.mean():.4f}, out={out.mean():.4f})")
                check(abs(out.std() - data.std()) < 1e-3,
                      f"std一致 (data={data.std():.4f}, out={out.std():.4f})")
        finally:
            frame.close()
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def test_float64_data_correctness():
    """测试 4: FLOAT64 数据值正确 (8字节字节序)"""
    section("测试 4: FLOAT64 数据正确性")
    tmpdir = tempfile.mkdtemp(prefix="aio_fits_fix_")
    try:
        frame = PipelineFramePy()
        try:
            data = np.array([1.5, 2.5, 3.5, 4.5, 5.5, 6.5], dtype=np.float64)
            frame.add_block("data", data)

            fits_path = os.path.join(tmpdir, "f64_test.fits")
            frame.export_block_fits("data", fits_path)

            with fits.open(fits_path) as hdul:
                out = hdul[0].data
                check(out is not None, "astropy 读取到 FLOAT64 数据")
                check(out.dtype.itemsize == 8 and out.dtype.kind == 'f',
                      f"dtype 为 8字节浮点 (实际={out.dtype})")
                check(np.allclose(out, data, rtol=1e-10),
                      f"FLOAT64 数据一致 (max_diff={np.max(np.abs(out-data)):.2e})")
        finally:
            frame.close()
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def test_int32_data_correctness():
    """测试 5: INT32 数据值正确"""
    section("测试 5: INT32 数据正确性")
    tmpdir = tempfile.mkdtemp(prefix="aio_fits_fix_")
    try:
        frame = PipelineFramePy()
        try:
            data = np.array([100, 200, 300, 400, 500], dtype=np.int32)
            frame.add_block("data", data)

            fits_path = os.path.join(tmpdir, "i32_test.fits")
            frame.export_block_fits("data", fits_path)

            with fits.open(fits_path) as hdul:
                out = hdul[0].data
                check(out is not None, "astropy 读取到 INT32 数据")
                check(np.array_equal(out, data),
                      f"INT32 数据一致 (out={out})")
        finally:
            frame.close()
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def test_header_raw_bytes():
    """测试 6: 直接检查头字节 (SIMPLE 卡片应为 'SIMPLE  = T' 无引号)"""
    section("测试 6: 头字节原始检查")
    tmpdir = tempfile.mkdtemp(prefix="aio_fits_fix_")
    try:
        frame = PipelineFramePy()
        try:
            data = np.zeros(4, dtype=np.float32)
            frame.add_block("data", data)
            fits_path = os.path.join(tmpdir, "raw_test.fits")
            frame.export_block_fits("data", fits_path)

            # 读取前 80 字节 (第一张卡片)
            with open(fits_path, "rb") as f:
                card0 = f.read(80).decode("ascii")
            print(f"  卡片0: '{card0}'")
            # SIMPLE 卡片: 逻辑值 T (无引号), T 在 column 11 (index 10)
            check(card0.startswith("SIMPLE  = T"),
                  f"卡片以 'SIMPLE  = T' 开头 (实际='{card0[:12]}')")
            check("'T'" not in card0[:20],
                  "SIMPLE 值无引号 (非字符串 'T')")

            # 第二张卡片应为 BITPIX
            with open(fits_path, "rb") as f:
                f.seek(80)
                card1 = f.read(80).decode("ascii")
            print(f"  卡片1: '{card1}'")
            check("BITPIX" in card1, "第二张卡片含 BITPIX")
        finally:
            frame.close()
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


# ============================================================================
# 主入口
# ============================================================================

def main():
    print("aio_frame_export_block_fits 修复验证测试")
    print("=" * 60)

    tests = [
        test_simple_card_format,
        test_float32_data_correctness,
        test_float32_random_data,
        test_float64_data_correctness,
        test_int32_data_correctness,
        test_header_raw_bytes,
    ]

    for t in tests:
        try:
            t()
        except Exception as e:
            global FAIL
            FAIL += 1
            ERRORS.append(f"{t.__name__} 异常: {e}")
            print(f"  [ERROR] {t.__name__} 抛出异常:")
            traceback.print_exc()

    print("\n" + "=" * 60)
    print(f"测试结果: {PASS} 通过, {FAIL} 失败")
    if ERRORS:
        print("\n失败项:")
        for e in ERRORS:
            print(f"  - {e}")
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
