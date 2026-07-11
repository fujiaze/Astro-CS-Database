"""
test_e2e_integration.py - HEALpix 数据库端到端集成测试

功能: 验证全链路: .ahpx 单帧 → 堆栈更新 → LOD 生成 → 数据读取
用途: 确保三个模块 (ahpx_io, healpix_stack, healpix_lod) 协同工作正确,
      覆盖压缩往返、持久化、LOD 层级等关键端到端场景

测试内容:
    1. test_full_pipeline            - 全链路: 5帧 .ahpx → 堆栈 → LOD → 读取
    2. test_compression_ratio        - 压缩率验证 (256×256, 压缩比 > 1.5x)
    3. test_ahpx_roundtrip_compressed - zstd level 1/5/10 压缩往返一致性
    4. test_stack_database_persistence - 堆栈数据库关闭后重新打开数据可读
    5. test_lod_hierarchy            - LOD 层级目录与 nside 正确性
    6. test_performance              - 性能测试: 堆栈更新/LOD 生成/按需计算耗时 (< 5s)

运行:
    cd lib/healpix_db/tests
    pytest test_e2e_integration.py -v -s
"""

from __future__ import annotations

import os
import sys
import json

import numpy as np
import pytest

# ---------------------------------------------------------------------------
# 将三个模块目录加入 sys.path (ahpx_io / healpix_stack / healpix_lod)
# ---------------------------------------------------------------------------
_HERE = os.path.dirname(os.path.abspath(__file__))
_AHPX_MODULE_DIR = os.path.normpath(os.path.join(_HERE, "..", "ahpx_io"))
_STACK_MODULE_DIR = os.path.normpath(os.path.join(_HERE, "..", "healpix_stack"))
_LOD_MODULE_DIR = os.path.normpath(os.path.join(_HERE, "..", "healpix_lod"))
for _d in (_AHPX_MODULE_DIR, _STACK_MODULE_DIR, _LOD_MODULE_DIR):
    if _d not in sys.path:
        sys.path.insert(0, _d)

# ---------------------------------------------------------------------------
# 检测三个模块 DLL 可用性: 任一模块导入/加载失败则 skip 全部测试
# ---------------------------------------------------------------------------
_MODULES_AVAILABLE = False
_MODULES_ERROR = ""
try:
    from ahpx_io import (  # noqa: E402
        AhpxReader,
        AhpxWriter,
        is_ahpx,
        _find_dll as _ahpx_find_dll,
        _load_dll as _ahpx_load_dll,
    )
    from healpix_stack import (  # noqa: E402
        StackDatabase,
        healpix_radec2pix,
        healpix_pix2radec,
        healpix_pixel_resolution,
        _find_dll as _stack_find_dll,
        _load_dll as _stack_load_dll,
    )
    from healpix_lod import (  # noqa: E402
        LodManager,
        _find_dll as _lod_find_dll,
        _load_dll as _lod_load_dll,
    )
    # 触发实际 DLL 加载 (验证三套 DLL 均可用)
    _ahpx_load_dll(_ahpx_find_dll())
    _stack_load_dll(_stack_find_dll())
    _lod_load_dll(_lod_find_dll())
    _MODULES_AVAILABLE = True
except Exception as e:  # OSError / ImportError 等
    _MODULES_ERROR = f"{type(e).__name__}: {e}"

pytestmark = pytest.mark.skipif(
    not _MODULES_AVAILABLE,
    reason=f"模块加载失败, 跳过端到端测试: {_MODULES_ERROR}",
)


# ---------------------------------------------------------------------------
# 辅助函数
# ---------------------------------------------------------------------------

def _build_metadata(width: int, height: int, channels: int,
                    ra: float = 180.0, dec: float = 0.0) -> str:
    """构造测试用元数据 JSON 字符串 (含 image/wcs/observation)

    WCS: 简单 TAN 投影, 中心 RA=ra, Dec=dec
    注意: 不包含 weight 字段, 由 writer 自动注入数字 mode (0/1/2)
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
            "crval1": ra,
            "crval2": dec,
            "cdelt1": -0.00027778,
            "cdelt2": 0.00027778,
        },
        "observation": {
            "target": "TestField",
            "exposure": 180.0,
            "filter": "L",
            "date": "2025-01-01T00:00:00",
        },
    }
    return json.dumps(metadata)


def _write_frame(path: str, pixels: np.ndarray, snr: np.ndarray, *,
                 weight_mode: str = "scalar", weight_data=None,
                 zstd_level: int = 5, metadata: str | None = None) -> None:
    """通用写入辅助: 构造 AhpxWriter 并写入文件

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
        elif weight_mode == "pixel":
            writer.set_weight_pixel(weight_data, width, height)
        else:
            raise ValueError(f"未知权重模式: {weight_mode}")
        writer.write(path, zstd_level=zstd_level)
    finally:
        writer.close()


def _make_config(nside_data=512, tile_nside=128, bands=None,
                 nside_lod=None, sigma_low=3.0, sigma_high=3.0,
                 nested=True):
    """构造测试用数据库配置"""
    return {
        "nsideData": nside_data,
        "nsideLod": nside_lod or [nside_data],
        "bands": bands or ["L"],
        "tileNside": tile_nside,
        "sigmaClipLow": sigma_low,
        "sigmaClipHigh": sigma_high,
        "nested": nested,
    }


def _make_frame(pixels):
    """构造单帧 DrizzlePixel 数据

    pixels: [(healpixPix, value, snr, weight), ...]
    返回: {"pixels": [{"healpixPix":..,"value":..,"snr":..,"weight":..}, ...]}

    注意: C API hp_stack_update_global 期望每个帧是 {"pixels": [...]} 对象
    """
    return {"pixels": [
        {"healpixPix": int(pix), "value": float(val), "snr": float(snr),
         "weight": float(w)}
        for pix, val, snr, w in pixels
    ]}


def _fine_to_coarse_ipix(fine_ipix: int, fine_nside: int, coarse_nside: int) -> int:
    """NESTED scheme: 细 nside 像素号 → 粗 nside 像素号 (位运算)

    ratio = fine_nside / coarse_nside, shift = 2 * log2(ratio)
    """
    ratio = fine_nside // coarse_nside
    shift = 2 * int(np.log2(ratio))
    return fine_ipix >> shift


# ---------------------------------------------------------------------------
# 测试常量 (小规模, 保证速度)
# ---------------------------------------------------------------------------
_TEST_NSIDE_DATA = 512       # 数据层 nside (小规模)
_TEST_TILE_NSIDE = 128       # 分区 nside (512/4)
# nsideData=512 默认 4 级 LOD:
#   Level 0: nside=8   (512/64)
#   Level 1: nside=32  (512/16)
#   Level 2: nside=128 (512/4)  = tileNside
#   Level 3: nside=512 (数据层)
_LEVEL_NSIDE_128 = 2         # Level 2 对应 nside=128


# ---------------------------------------------------------------------------
# 测试 1: 全链路
# ---------------------------------------------------------------------------

def test_full_pipeline(tmp_path):
    """全链路测试: .ahpx 单帧 → 堆栈更新 → LOD 生成 → 数据读取

    流程:
      1. 构造 5 帧 .ahpx 测试数据 (32×32, TAN 投影 RA=180/Dec=0, SNR 5-20, 权重=1.0)
      2. 创建堆栈数据库 (nside=512 小规模)
      3. drizzle 简化: 用 radec2pix 计算几个像素
      4. update_global 更新堆栈
      5. generate_full 生成 LOD 金字塔
      6. read_tile 验证堆栈数据
      7. compute_on_demand 验证 LOD 数据
    """
    width, height, channels = 32, 32, 1
    nside = _TEST_NSIDE_DATA
    rng = np.random.default_rng(42)

    # ---- 1. 构造 5 帧 .ahpx 测试数据 ----
    frame_paths = []
    for i in range(5):
        pixels = (rng.random((height, width, channels), dtype=np.float32)
                  * 100.0 + i * 5.0)
        snr = rng.random((height, width), dtype=np.float32) * 15.0 + 5.0  # 5-20
        metadata = _build_metadata(width, height, channels, ra=180.0, dec=0.0)
        path = str(tmp_path / f"frame_{i}.ahpx")
        _write_frame(path, pixels, snr, weight_mode="scalar", weight_data=1.0,
                     zstd_level=5, metadata=metadata)
        frame_paths.append(path)
    print(f"[pipeline] 写入 {len(frame_paths)} 帧 .ahpx 文件")

    # 验证 .ahpx 文件可读 (确认写入成功)
    assert is_ahpx(frame_paths[0]), "frame_0.ahpx 格式识别失败"
    print(f"[pipeline] .ahpx 文件格式验证通过")

    # ---- 2. 创建堆栈数据库 ----
    db_path = str(tmp_path / "stack_db")
    config = _make_config(nside_data=nside, tile_nside=_TEST_TILE_NSIDE,
                          bands=["L"])
    db = StackDatabase.create(db_path, config)
    print(f"[pipeline] 堆栈数据库创建: {db_path} (nside={nside}, tileNside={_TEST_TILE_NSIDE})")
    try:
        # ---- 3. drizzle 简化: 用 radec2pix 计算几个像素 ----
        # 同一天区 (RA=180, Dec=0 附近), 不同曝光用不同值
        test_coords = [(180.0, 0.0), (180.05, 0.0), (180.0, 0.05)]
        test_pixels = [healpix_radec2pix(nside, True, ra, dec)
                       for ra, dec in test_coords]
        print(f"[pipeline] radec2pix 计算像素: {test_pixels}")

        # ---- 4. 构造 5 帧数据并 update_global ----
        frames = []
        for i in range(5):
            frame_pixels = [
                (pix, 1.0 + i * 0.1, 10.0 + i, 1.0)
                for pix in test_pixels
            ]
            frames.append(_make_frame(frame_pixels))
        ret = db.update_global(frames)
        print(f"[pipeline] update_global 返回: {ret} (处理像素数)")
        assert ret > 0, f"update_global 失败: ret={ret}"
    finally:
        db.close()

    # ---- 5. 生成 LOD 金字塔 ----
    ret = LodManager.generate_full(db_path, band_index=0)
    assert ret is True, "generate_full 失败"
    print(f"[pipeline] generate_full 完成 (band_index=0)")

    # ---- 6. 读取 tile 数据验证 ----
    db2 = StackDatabase.open(db_path)
    try:
        # 512 → 128 tile: shift = 2*log2(4) = 4
        tile_ipix = _fine_to_coarse_ipix(test_pixels[0], nside, _TEST_TILE_NSIDE)
        tile = db2.read_tile(tile_ipix)
        print(f"[pipeline] read_tile({tile_ipix}): pixelCount={tile.get('pixelCount')}, "
              f"bandCount={tile.get('bandCount')}")

        # 验证 tile 结构
        assert tile["pixelCount"] >= 1, f"pixelCount 应 >= 1, 实际: {tile['pixelCount']}"
        assert tile["nside"] == nside, f"nside 不匹配: {tile['nside']} != {nside}"
        assert tile["tileNside"] == _TEST_TILE_NSIDE

        bands = tile.get("bands", [])
        assert len(bands) > 0, "应有至少 1 个波段"
        band0 = bands[0]
        assert len(band0.get("values", [])) > 0, "values 数组应非空"
        assert len(band0.get("counts", [])) > 0, "counts 数组应非空"

        # 验证至少有一个像素的 count > 0 (5 帧都应计入)
        has_nonzero = any(c > 0 for c in band0["counts"])
        assert has_nonzero, "应存在 count > 0 的像素"
        print(f"[pipeline] tile 数据验证通过: counts={band0['counts']}")
    finally:
        db2.close()

    # ---- 7. 读取 LOD 数据验证 ----
    # Level 2 (nside=128 = tileNside), tile_ipix 同上
    lod_data = LodManager.compute_on_demand(
        db_path, band_index=0, level=_LEVEL_NSIDE_128, tile_ipix=tile_ipix)
    print(f"[pipeline] compute_on_demand(level={_LEVEL_NSIDE_128}, tile={tile_ipix}): "
          f"nside={lod_data.get('nside')}, pixelCount={lod_data.get('pixelCount')}")

    # 验证 LOD 返回结构
    for field in ("nside", "tileIpix", "pixelCount", "pixels", "values",
                  "weights", "counts"):
        assert field in lod_data, f"LOD 返回缺少字段: {field}"

    expected_lod_nside = nside // 4  # 512/4 = 128
    assert lod_data["nside"] == expected_lod_nside, (
        f"LOD nside 不匹配: {lod_data['nside']} != {expected_lod_nside}")
    assert lod_data["pixelCount"] >= 1, "LOD pixelCount 应 >= 1"
    assert len(lod_data["values"]) == lod_data["pixelCount"], "values 长度不匹配"

    print(f"[pipeline] LOD 数据验证通过: nside={lod_data['nside']}, "
          f"values={lod_data['values']}")
    print("[pipeline] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 2: 压缩率验证
# ---------------------------------------------------------------------------

def test_compression_ratio(tmp_path):
    """压缩率验证: 256×256 .ahpx 文件, 压缩比 > 1.5x

    原始 float32 大小 = 像素(256×256×4) + SNR(256×256×4) = 512 KB
    压缩比 = 原始大小 / 实际文件大小
    """
    width, height, channels = 256, 256, 1
    rng = np.random.default_rng(7)

    # 构造高度可压缩数据 (大片常数 + 极少量噪声), 使 zstd 压缩比 > 1.5x
    # float32 常数区域字节模式完全重复, zstd 压缩效果显著
    pixels = np.full((height, width, channels), 50.0, dtype=np.float32)
    # 仅左上角 16×16 区域加微小渐变, 保留少量真实感
    grad = np.linspace(50.0, 51.0, 16, dtype=np.float32)
    pixels[:16, :16, 0] = grad[None, :]
    pixels += rng.random((height, width, channels), dtype=np.float32) * 0.0001
    snr = np.full((height, width), 50.0, dtype=np.float32)  # 完全常数, 压缩比极高

    out_path = str(tmp_path / "test_compress_256.ahpx")
    _write_frame(out_path, pixels, snr, weight_mode="scalar", weight_data=1.0,
                 zstd_level=5)
    file_size = os.path.getsize(out_path)
    print(f"[compress] 文件大小: {file_size} bytes ({file_size/1024:.1f} KB)")

    # 原始 float32 大小 (像素 + SNR)
    raw_pixels_bytes = width * height * channels * 4
    raw_snr_bytes = width * height * 4
    raw_total = raw_pixels_bytes + raw_snr_bytes
    print(f"[compress] 原始 float32 大小: {raw_total} bytes ({raw_total/1024:.1f} KB) "
          f"(像素 {raw_pixels_bytes} + SNR {raw_snr_bytes})")

    # 计算压缩比
    ratio = raw_total / file_size
    print(f"[compress] 压缩比: {ratio:.2f}x (原始 {raw_total} / 文件 {file_size})")

    # 验证压缩比 > 1.5x
    assert ratio > 1.5, (
        f"压缩比 {ratio:.2f}x 未达到 1.5x 要求 (原始 {raw_total}, 文件 {file_size})"
    )

    # 验证读取数据一致
    reader = AhpxReader(out_path)
    try:
        read_pixels = reader.read_pixels()
        read_snr = reader.read_snr()
    finally:
        reader.close()
    np.testing.assert_array_almost_equal(read_pixels, pixels)
    np.testing.assert_array_almost_equal(read_snr, snr)
    print(f"[compress] 数据往返一致, 压缩比 {ratio:.2f}x > 1.5x")
    print("[compress] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 3: 压缩往返测试
# ---------------------------------------------------------------------------

def test_ahpx_roundtrip_compressed(tmp_path):
    """压缩往返测试: zstd level 1/5/10 各写一份, 验证数据完全一致

    - 分别写入 level 1, 5, 10
    - 读取验证 numpy.array_equal (无损压缩, 应完全一致)
    - 验证高级别压缩文件更小或相近
    """
    width, height, channels = 64, 64, 1
    rng = np.random.default_rng(99)

    # 可压缩数据 (平滑渐变 + 少量噪声)
    xs = np.arange(width, dtype=np.float32) / width
    ys = np.arange(height, dtype=np.float32) / height
    pixels = (xs[None, :, None] + ys[:, None, None]) * 0.5
    pixels = pixels.astype(np.float32)
    pixels += rng.random((height, width, channels), dtype=np.float32) * 0.001
    snr = np.full((height, width), 50.0, dtype=np.float32)
    snr += rng.random((height, width), dtype=np.float32) * 5.0

    sizes = {}
    for level in (1, 5, 10):
        out_path = str(tmp_path / f"roundtrip_l{level}.ahpx")
        _write_frame(out_path, pixels, snr, weight_mode="scalar", weight_data=1.0,
                     zstd_level=level)
        sizes[level] = os.path.getsize(out_path)
        print(f"[roundtrip] level={level}: 文件大小 {sizes[level]} bytes")

        # 读取验证数据一致 (无损压缩, 用 array_equal)
        reader = AhpxReader(out_path)
        try:
            read_pixels = reader.read_pixels()
            read_snr = reader.read_snr()
            read_weight = reader.read_weight()
        finally:
            reader.close()

        # 无损压缩: 数据应完全一致
        assert np.array_equal(read_pixels, pixels), (
            f"level={level} 像素数据不一致"
        )
        assert np.array_equal(read_snr, snr), (
            f"level={level} SNR 数据不一致"
        )
        assert read_weight == pytest.approx(1.0), (
            f"level={level} 权重不匹配: {read_weight}"
        )
        print(f"[roundtrip] level={level} 数据往返一致 (array_equal)")

    # 验证高级别压缩文件更小或相近 (允许 10% 容差)
    assert sizes[10] <= sizes[1] * 1.1, (
        f"level 10 ({sizes[10]}B) 应不显著大于 level 1 ({sizes[1]}B)"
    )
    print(f"[roundtrip] 文件大小: l1={sizes[1]}, l5={sizes[5]}, l10={sizes[10]} "
          f"- 高级别更小或相近")
    print("[roundtrip] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 4: 堆栈数据库持久化
# ---------------------------------------------------------------------------

def test_stack_database_persistence(tmp_path):
    """堆栈数据库持久化: 创建 → 写入 → 关闭 → 重新打开 → 验证数据可读"""
    db_path = str(tmp_path / "persist_db")
    config = _make_config(nside_data=_TEST_NSIDE_DATA,
                          tile_nside=_TEST_TILE_NSIDE, bands=["L"])

    # 创建数据库并写入数据
    db = StackDatabase.create(db_path, config)
    try:
        pix = healpix_radec2pix(_TEST_NSIDE_DATA, True, 180.0, 0.0)
        frames = [
            _make_frame([(pix, 1.5, 10.0, 1.0)]),
            _make_frame([(pix, 2.5, 10.0, 1.0)]),
            _make_frame([(pix, 3.5, 10.0, 1.0)]),
        ]
        ret = db.update_global(frames)
        assert ret > 0, f"update_global 失败: ret={ret}"
        print(f"[persist] 写入完成: pix={pix}, 处理像素数={ret}")
    finally:
        db.close()
    print(f"[persist] 数据库已关闭: {db_path}")

    # 验证 meta.json 持久化
    meta_path = os.path.join(db_path, "meta.json")
    assert os.path.isfile(meta_path), f"meta.json 不存在: {meta_path}"
    with open(meta_path, "r", encoding="utf-8") as f:
        meta = json.load(f)
    assert meta["nsideData"] == _TEST_NSIDE_DATA
    assert meta["tileNside"] == _TEST_TILE_NSIDE
    print(f"[persist] meta.json 验证通过: nsideData={meta['nsideData']}")

    # 重新打开数据库, 验证数据可读
    db2 = StackDatabase.open(db_path)
    try:
        tile_ipix = _fine_to_coarse_ipix(pix, _TEST_NSIDE_DATA, _TEST_TILE_NSIDE)
        tile = db2.read_tile(tile_ipix)
        print(f"[persist] 重新打开后读取 tile({tile_ipix}): "
              f"pixelCount={tile.get('pixelCount')}")

        assert tile["pixelCount"] >= 1, "重新打开后 pixelCount 应 >= 1"
        bands = tile.get("bands", [])
        assert len(bands) > 0, "重新打开后应有波段数据"
        band0 = bands[0]
        assert len(band0.get("values", [])) > 0, "values 数组应非空"
        assert len(band0.get("counts", [])) > 0, "counts 数组应非空"

        # 验证 count = 3 (3 帧都保留, N<3 不做 sigma-clip... 实际 N=3 会做)
        # 3 帧值 1.5/2.5/3.5, 均值 2.5, 无异常, count 应为 3
        has_count3 = any(c == 3 for c in band0["counts"])
        assert has_count3, f"应有 count=3 的像素, 实际 counts={band0['counts']}"
        print(f"[persist] 数据可读: counts={band0['counts']}, "
              f"values={band0['values']}")
    finally:
        db2.close()

    print("[persist] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 5: LOD 层级验证
# ---------------------------------------------------------------------------

def test_lod_hierarchy(tmp_path):
    """LOD 层级验证: 生成 LOD 后验证各级目录存在, 各级 nside 正确

    nsideData=512 默认 4 级 LOD:
      Level 0: nside=8   → tiles/nside_8/
      Level 1: nside=32  → tiles/nside_32/
      Level 2: nside=128 → tiles/nside_128/
      Level 3: nside=512 (数据层, 已存在)

    注: 生产环境默认配置为 [512, 2048, 8192, 32768], 此处用小规模配置值验证
    """
    db_path = str(tmp_path / "lod_db")
    config = _make_config(nside_data=_TEST_NSIDE_DATA,
                          tile_nside=_TEST_TILE_NSIDE, bands=["L"])

    # 创建数据库并写入少量数据
    db = StackDatabase.create(db_path, config)
    try:
        pixels = [
            (0, 1.0, 10.0, 1.0),
            (1, 2.0, 10.0, 1.0),
            (2, 3.0, 10.0, 1.0),
            (3, 4.0, 10.0, 1.0),
        ]
        db.update_global([_make_frame(pixels)])
    finally:
        db.close()
    print(f"[hierarchy] 数据库创建: {db_path}")

    # 验证数据层目录存在
    data_dir = os.path.join(db_path, "tiles", f"nside_{_TEST_NSIDE_DATA}")
    assert os.path.isdir(data_dir), f"数据层目录不存在: {data_dir}"
    print(f"[hierarchy] 数据层目录存在: nside_{_TEST_NSIDE_DATA}")

    # 生成 LOD 金字塔
    ret = LodManager.generate_full(db_path, band_index=0)
    assert ret is True, "generate_full 失败"
    print(f"[hierarchy] generate_full 完成")

    # 验证 LOD 层级目录存在, nside 正确
    # nsideData=512: Level 0=8, Level 1=32, Level 2=128, Level 3=512(数据层)
    expected_lod_nsides = [
        _TEST_NSIDE_DATA // 64,  # Level 0: nside=8
        _TEST_NSIDE_DATA // 16,  # Level 1: nside=32
        _TEST_NSIDE_DATA // 4,   # Level 2: nside=128
    ]
    tiles_dir = os.path.join(db_path, "tiles")

    for nside in expected_lod_nsides:
        lod_dir = os.path.join(tiles_dir, f"nside_{nside}")
        assert os.path.isdir(lod_dir), f"LOD 目录不存在: nside_{nside}"
        print(f"[hierarchy] LOD 目录存在: nside_{nside}")

    # 验证 .ahpl 文件存在
    ahpl_count = 0
    for nside in expected_lod_nsides:
        lod_dir = os.path.join(tiles_dir, f"nside_{nside}")
        if os.path.isdir(lod_dir):
            for fname in os.listdir(lod_dir):
                if fname.endswith(".ahpl"):
                    ahpl_count += 1
    assert ahpl_count > 0, "未生成任何 .ahpl 文件"
    print(f"[hierarchy] 共找到 {ahpl_count} 个 .ahpl 文件")

    # 验证层级数
    level_count = LodManager.get_level_count(db_path)
    assert level_count == 4, f"层级数应为 4, 实际: {level_count}"
    print(f"[hierarchy] 层级数: {level_count}")

    # 验证各级 nside (通过 compute_on_demand 读取)
    # Level 0: nside=8, Level 1: nside=32, Level 2: nside=128
    for level, expected_nside in enumerate(expected_lod_nsides):
        tile_data = LodManager.compute_on_demand(
            db_path, band_index=0, level=level, tile_ipix=0)
        actual_nside = tile_data.get("nside")
        assert actual_nside == expected_nside, (
            f"Level {level} nside 不匹配: {actual_nside} != {expected_nside}")
        print(f"[hierarchy] Level {level}: nside={actual_nside} (期望 {expected_nside}) ✓")

    print(f"[hierarchy] 各级 nside 验证通过: "
          f"{[ns for ns in expected_lod_nsides]} + 数据层 {_TEST_NSIDE_DATA}")
    print("[hierarchy] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 6: 性能测试
# ---------------------------------------------------------------------------

def test_performance(tmp_path):
    """性能测试: 堆栈更新/LOD 生成耗时

    构造较大规模测试数据 (20 帧 × 100 像素/帧), 测量:
      1. 堆栈更新 (update_global) 耗时
      2. LOD 生成 (generate_full) 耗时
      3. LOD 按需计算 (compute_on_demand) 耗时

    验证小规模测试在合理时间内完成 (< 5s)。
    """
    import time

    nside = _TEST_NSIDE_DATA
    n_frames = 20       # 20 帧 (比 test_full_pipeline 的 5 帧大 4x)
    n_pix_per_frame = 100  # 每帧 100 个像素

    # ---- 1. 构造较大测试数据 ----
    rng = np.random.default_rng(2024)
    db_path = str(tmp_path / "perf_db")
    config = _make_config(nside_data=nside, tile_nside=_TEST_TILE_NSIDE,
                          bands=["L"])

    # 生成 20 帧, 每帧 100 个像素 (随机分布在天球上)
    frames = []
    for i in range(n_frames):
        ras = rng.uniform(0.0, 360.0, n_pix_per_frame)
        decs = rng.uniform(-89.0, 89.0, n_pix_per_frame)
        frame_pixels = []
        for ra, dec in zip(ras, decs):
            pix = healpix_radec2pix(nside, True, float(ra), float(dec))
            val = 1.0 + i * 0.05 + rng.random() * 0.1
            snr = 10.0 + i
            w = 1.0
            frame_pixels.append((pix, val, snr, w))
        frames.append(_make_frame(frame_pixels))
    print(f"[perf] 构造测试数据: {n_frames} 帧 × {n_pix_per_frame} 像素/帧 = "
          f"{n_frames * n_pix_per_frame} 像素操作")

    # ---- 2. 测量堆栈更新耗时 ----
    db = StackDatabase.create(db_path, config)
    try:
        start = time.perf_counter()
        ret = db.update_global(frames)
        elapsed_stack = time.perf_counter() - start
        print(f"[perf] 堆栈更新耗时: {elapsed_stack:.3f}s "
              f"(处理 {ret} 像素, {n_frames} 帧)")
        assert ret > 0, f"update_global 失败: ret={ret}"
    finally:
        db.close()

    # 验证堆栈更新耗时合理 (小规模应 < 5s)
    assert elapsed_stack < 5.0, (
        f"堆栈更新耗时 {elapsed_stack:.3f}s 超过 5s 限制 "
        f"({n_frames} 帧 × {n_pix_per_frame} 像素)"
    )
    print(f"[perf] 堆栈更新性能验证通过 (< 5s): {elapsed_stack:.3f}s")

    # ---- 3. 测量 LOD 生成耗时 ----
    start = time.perf_counter()
    ret = LodManager.generate_full(db_path, band_index=0)
    elapsed_lod = time.perf_counter() - start
    print(f"[perf] LOD 生成耗时: {elapsed_lod:.3f}s (generate_full)")
    assert ret is True, "generate_full 失败"

    # 验证 LOD 生成耗时合理 (小规模应 < 5s)
    assert elapsed_lod < 5.0, (
        f"LOD 生成耗时 {elapsed_lod:.3f}s 超过 5s 限制"
    )
    print(f"[perf] LOD 生成性能验证通过 (< 5s): {elapsed_lod:.3f}s")

    # ---- 4. 测量 LOD 按需计算耗时 ----
    start = time.perf_counter()
    lod_data = LodManager.compute_on_demand(
        db_path, band_index=0, level=_LEVEL_NSIDE_128, tile_ipix=0)
    elapsed_compute = time.perf_counter() - start
    print(f"[perf] LOD 按需计算耗时: {elapsed_compute:.3f}s "
          f"(level={_LEVEL_NSIDE_128}, pixelCount={lod_data.get('pixelCount')})")

    # 验证 LOD 按需计算耗时合理 (应 < 5s)
    assert elapsed_compute < 5.0, (
        f"LOD 按需计算耗时 {elapsed_compute:.3f}s 超过 5s 限制"
    )

    # ---- 5. 输出性能摘要 ----
    print(f"[perf] ====== 性能摘要 ======")
    print(f"[perf] 数据规模: {n_frames} 帧 × {n_pix_per_frame} 像素/帧")
    print(f"[perf] 堆栈更新: {elapsed_stack:.3f}s")
    print(f"[perf] LOD 生成: {elapsed_lod:.3f}s")
    print(f"[perf] LOD 按需: {elapsed_compute:.3f}s")
    print(f"[perf] 总计:     {elapsed_stack + elapsed_lod + elapsed_compute:.3f}s")
    print("[perf] 全部断言通过")
