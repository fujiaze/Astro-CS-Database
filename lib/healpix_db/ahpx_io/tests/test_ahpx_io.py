"""
test_ahpx_io.py - .ahpx 格式读写单元测试

功能: 通过 Python ctypes 绑定测试 ahpx_io.dll 的 .ahpx 格式读写
用途: 验证 SCALAR/GRID/PIXEL 三种权重模式、zstd 压缩、文件识别与头解析

运行:
    cd lib/healpix_db/ahpx_io/tests
    pytest test_ahpx_io.py -v -s
"""

from __future__ import annotations

import os
import sys
import json

import numpy as np
import pytest

# 将 ahpx_io.py 所在目录加入 sys.path (tests/ 的上级目录)
_AHPX_MODULE_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
if _AHPX_MODULE_DIR not in sys.path:
    sys.path.insert(0, _AHPX_MODULE_DIR)

# 检测 DLL 可用性: 加载失败则跳过全部测试 (而非 fail)
_DLL_AVAILABLE = False
_DLL_ERROR = ""
try:
    from ahpx_io import AhpxReader, AhpxWriter, is_ahpx, _find_dll, _load_dll  # noqa: E402
    _load_dll(_find_dll())  # 触发实际 DLL 加载
    _DLL_AVAILABLE = True
except Exception as e:  # OSError / ImportError 等
    _DLL_ERROR = f"{type(e).__name__}: {e}"

pytestmark = pytest.mark.skipif(
    not _DLL_AVAILABLE,
    reason=f"ahpx_io DLL 加载失败, 跳过测试: {_DLL_ERROR}",
)


# ---------------------------------------------------------------------------
# 辅助函数
# ---------------------------------------------------------------------------

def _build_metadata(width: int, height: int, channels: int) -> str:
    """构造测试用元数据 JSON 字符串 (含 image/wcs/observation/calibration)

    注意: 不包含 weight 字段, 由 writer 自动注入数字 mode (0/1/2),
    避免 reader 解析字符串 mode 失败。
    """
    metadata = {
        "image": {
            "width": width,
            "height": height,
            "channels": channels,
            "dtype": "float32",
        },
        "wcs": {
            "ctype1": "RA---TAN",
            "ctype2": "DEC--TAN",
            "crval1": 10.6847,
            "crval2": 41.2687,
            "cdelt1": -0.00027778,
            "cdelt2": 0.00027778,
        },
        "observation": {
            "target": "M31",
            "exposure": 180.0,
            "filter": "Red",
            "date": "2025-01-01T00:00:00",
        },
        "calibration": {
            "dark": "master_dark.fits",
            "flat": "master_flat.fits",
            "bias": "master_bias.fits",
        },
    }
    return json.dumps(metadata)


def _write_frame(path: str, pixels: np.ndarray, snr: np.ndarray, *,
                 weight_mode: str = "scalar", weight_data=None,
                 zstd_level: int = 5, metadata: str | None = None) -> None:
    """通用写入辅助: 构造 writer 并写入文件

    pixels: float32 (H, W, C)
    snr:    float32 (H, W)
    weight_data: scalar -> float; grid -> (gh, gw); pixel -> (H, W)
    """
    height, width, channels = pixels.shape
    if metadata is None:
        metadata = _build_metadata(width, height, channels)
    writer = AhpxWriter()
    try:
        writer.set_metadata(metadata)
        writer.set_pixels(pixels, width, height, channels)
        writer.set_snr(snr, width, height)
        if weight_mode == "scalar":
            writer.set_weight_scalar(float(weight_data if weight_data is not None else 1.0))
        elif weight_mode == "grid":
            # weight_data shape = (gh, gw) (numpy 行优先: gh 行 gw 列)
            gh, gw = weight_data.shape
            writer.set_weight_grid(weight_data, gw, gh)
        elif weight_mode == "pixel":
            writer.set_weight_pixel(weight_data, width, height)
        else:
            raise ValueError(f"未知权重模式: {weight_mode}")
        writer.write(path, zstd_level=zstd_level)
    finally:
        writer.close()


# ---------------------------------------------------------------------------
# 测试 1: SCALAR 权重模式读写
# ---------------------------------------------------------------------------

def test_write_read_scalar(tmp_path):
    """写入+读取, 权重模式 SCALAR (scalar=1.0)"""
    width, height, channels = 64, 64, 1
    rng = np.random.default_rng(42)
    pixels = rng.random((height, width, channels), dtype=np.float32)
    snr = rng.random((height, width), dtype=np.float32) * 100.0

    out_path = str(tmp_path / "test_scalar.ahpx")
    print(f"[scalar] 写入: {out_path}")
    _write_frame(out_path, pixels, snr, weight_mode="scalar", weight_data=1.0)

    reader = AhpxReader(out_path)
    try:
        read_pixels = reader.read_pixels()
        read_snr = reader.read_snr()
        read_weight = reader.read_weight()
        header = json.loads(reader.header_json)
        print(f"[scalar] 读取完成: pixels{read_pixels.shape}, snr{read_snr.shape}, "
              f"weight={read_weight}, dims={reader.width}x{reader.height}x{reader.channels}")
    finally:
        reader.close()

    # 像素一致
    np.testing.assert_array_almost_equal(read_pixels, pixels)
    # SNR 一致
    np.testing.assert_array_almost_equal(read_snr, snr)
    # 权重 = 1.0
    assert read_weight == pytest.approx(1.0)
    # 头 JSON 解析正确
    assert "image" in header
    assert "wcs" in header
    assert "observation" in header
    assert "calibration" in header
    assert "weight" in header
    assert header["image"]["width"] == width
    assert header["image"]["height"] == height
    assert header["image"]["channels"] == channels
    # 几何属性
    assert reader.width == width
    assert reader.height == height
    assert reader.channels == channels
    print("[scalar] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 2: GRID 权重模式读写
# ---------------------------------------------------------------------------

def test_write_read_grid(tmp_path):
    """权重模式 GRID (8×6 网格)"""
    width, height, channels = 64, 64, 1
    gw, gh = 8, 6
    rng = np.random.default_rng(123)
    pixels = rng.random((height, width, channels), dtype=np.float32)
    snr = rng.random((height, width), dtype=np.float32) * 100.0
    weight_grid = rng.random((gh, gw), dtype=np.float32)

    out_path = str(tmp_path / "test_grid.ahpx")
    print(f"[grid] 写入: {out_path} (grid {gh}x{gw})")
    _write_frame(out_path, pixels, snr, weight_mode="grid", weight_data=weight_grid)

    reader = AhpxReader(out_path)
    try:
        read_weight = reader.read_weight()
        print(f"[grid] 读取权重 shape: {read_weight.shape}")
    finally:
        reader.close()

    # 权重数组形状
    assert read_weight.shape == (gh, gw)
    # 权重数据一致
    np.testing.assert_array_almost_equal(read_weight, weight_grid)
    print("[grid] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 3: PIXEL 权重模式读写
# ---------------------------------------------------------------------------

def test_write_read_pixel(tmp_path):
    """权重模式 PIXEL (64×64 逐像素)"""
    width, height, channels = 64, 64, 1
    rng = np.random.default_rng(456)
    pixels = rng.random((height, width, channels), dtype=np.float32)
    snr = rng.random((height, width), dtype=np.float32) * 100.0
    weight_pixel = rng.random((height, width), dtype=np.float32)

    out_path = str(tmp_path / "test_pixel.ahpx")
    print(f"[pixel] 写入: {out_path}")
    _write_frame(out_path, pixels, snr, weight_mode="pixel", weight_data=weight_pixel)

    reader = AhpxReader(out_path)
    try:
        read_weight = reader.read_weight()
        print(f"[pixel] 读取权重 shape: {read_weight.shape}")
    finally:
        reader.close()

    # 权重数组形状
    assert read_weight.shape == (height, width)
    # 权重数据一致
    np.testing.assert_array_almost_equal(read_weight, weight_pixel)
    print("[pixel] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 4: 压缩级别验证
# ---------------------------------------------------------------------------

def test_compression(tmp_path):
    """zstd level 1/5/10 写入, 验证数据一致性与文件大小"""
    width, height, channels = 64, 64, 1
    rng = np.random.default_rng(789)
    # 构造可压缩数据 (平滑渐变 + 少量噪声), 使不同压缩级别有差异
    xs = np.arange(width, dtype=np.float32) / width
    ys = np.arange(height, dtype=np.float32) / height
    pixels = (xs[None, :, None] + ys[:, None, None]) * 0.5
    pixels = pixels.astype(np.float32)
    pixels += rng.random((height, width, channels), dtype=np.float32) * 0.001
    snr = np.full((height, width), 50.0, dtype=np.float32)
    snr += rng.random((height, width), dtype=np.float32) * 5.0

    sizes = {}
    for level in (1, 5, 10):
        out_path = str(tmp_path / f"test_comp_l{level}.ahpx")
        _write_frame(out_path, pixels, snr, weight_mode="scalar", weight_data=1.0,
                     zstd_level=level)
        sizes[level] = os.path.getsize(out_path)
        print(f"[comp] level={level}: 文件大小 {sizes[level]} bytes")

        # 读取验证数据一致
        reader = AhpxReader(out_path)
        try:
            read_pixels = reader.read_pixels()
            read_snr = reader.read_snr()
        finally:
            reader.close()
        np.testing.assert_array_almost_equal(read_pixels, pixels)
        np.testing.assert_array_almost_equal(read_snr, snr)

    # 高级别应该更小或相近 (允许 10% 容差, 防止压缩头开销导致偶发膨胀)
    assert sizes[10] <= sizes[1] * 1.1, (
        f"level 10 ({sizes[10]}B) 应不显著大于 level 1 ({sizes[1]}B)"
    )
    print(f"[comp] 文件大小: l1={sizes[1]}, l5={sizes[5]}, l10={sizes[10]} - 断言通过")


# ---------------------------------------------------------------------------
# 测试 5: 文件识别 is_ahpx
# ---------------------------------------------------------------------------

def test_is_ahpx(tmp_path):
    """is_ahpx 文件识别"""
    # 先创建一个有效的 .ahpx 文件
    width, height, channels = 64, 64, 1
    pixels = np.zeros((height, width, channels), dtype=np.float32)
    snr = np.ones((height, width), dtype=np.float32)

    ahpx_path = str(tmp_path / "test_scalar.ahpx")
    _write_frame(ahpx_path, pixels, snr, weight_mode="scalar", weight_data=1.0)
    print(f"[is_ahpx] 有效文件: {ahpx_path}")

    # 有效 .ahpx 文件返回 True
    assert is_ahpx(ahpx_path) is True

    # 不存在的文件返回 False
    missing_path = str(tmp_path / "notexist.txt")
    assert is_ahpx(missing_path) is False

    # 非 .ahpx 文本文件返回 False
    txt_path = str(tmp_path / "plain.txt")
    with open(txt_path, "w", encoding="utf-8") as f:
        f.write("not an ahpx file")
    assert is_ahpx(txt_path) is False
    print("[is_ahpx] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 6: 头解析
# ---------------------------------------------------------------------------

def test_header_parsing(tmp_path):
    """读取 header_json, 验证包含 image/wcs/observation/calibration 字段及几何属性"""
    width, height, channels = 64, 64, 1
    pixels = np.zeros((height, width, channels), dtype=np.float32)
    snr = np.ones((height, width), dtype=np.float32)

    out_path = str(tmp_path / "test_header.ahpx")
    _write_frame(out_path, pixels, snr, weight_mode="scalar", weight_data=1.0)
    print(f"[header] 读取: {out_path}")

    reader = AhpxReader(out_path)
    try:
        header = json.loads(reader.header_json)
        print(f"[header] 字段: {list(header.keys())}")
        print(f"[header] dims: {reader.width}x{reader.height}x{reader.channels}")
    finally:
        reader.close()

    # 验证包含必需字段
    assert "image" in header
    assert "wcs" in header
    assert "observation" in header
    assert "calibration" in header
    # 验证 image 子字段
    assert header["image"]["width"] == width
    assert header["image"]["height"] == height
    assert header["image"]["channels"] == channels
    # 验证 width/height/channels 属性
    assert reader.width == width
    assert reader.height == height
    assert reader.channels == channels
    print("[header] 全部断言通过")
