"""
test_healpix_lod.py - healpix_lod 模块单元测试

功能: 通过 Python ctypes 绑定测试 healpix_lod.dll
用途: 验证 LOD 金字塔生成、降采样、增量更新、按需计算

运行:
    cd lib/healpix_db/healpix_lod/tests
    pytest test_healpix_lod.py -v -s
"""

from __future__ import annotations

import os
import sys
import json
import math

import pytest

# 将 healpix_lod.py 所在目录加入 sys.path (tests/ 的上级目录)
_LOD_MODULE_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
if _LOD_MODULE_DIR not in sys.path:
    sys.path.insert(0, _LOD_MODULE_DIR)

# 将 healpix_stack.py 所在目录加入 sys.path (同级 healpix_stack 模块)
_STACK_MODULE_DIR = os.path.normpath(
    os.path.join(_LOD_MODULE_DIR, "..", "healpix_stack")
)
if _STACK_MODULE_DIR not in sys.path:
    sys.path.insert(0, _STACK_MODULE_DIR)

# 检测 LOD DLL 可用性: 加载失败则跳过全部测试
_LOD_DLL_AVAILABLE = False
_LOD_ERROR = ""
try:
    from healpix_lod import (  # noqa: E402
        LodManager,
        _find_dll,
        _load_dll,
    )
    _load_dll(_find_dll())  # 触发实际 DLL 加载
    _LOD_DLL_AVAILABLE = True
except Exception as e:  # OSError / ImportError 等
    _LOD_ERROR = f"{type(e).__name__}: {e}"

# 检测 healpix_stack 模块可用性 (测试需要先创建堆栈数据库)
_STACK_AVAILABLE = False
_STACK_ERROR = ""
try:
    from healpix_stack import (  # noqa: E402
        StackDatabase,
        healpix_radec2pix,
    )
    _STACK_AVAILABLE = True
except Exception as e:
    _STACK_ERROR = f"{type(e).__name__}: {e}"

pytestmark = pytest.mark.skipif(
    not _LOD_DLL_AVAILABLE,
    reason=f"healpix_lod DLL 加载失败, 跳过测试: {_LOD_ERROR}",
)

# healpix_stack 不可用时, 需要堆栈数据库的测试单独 skip
_needs_stack = pytest.mark.skipif(
    not _STACK_AVAILABLE,
    reason=f"healpix_stack 模块不可用, 跳过测试: {_STACK_ERROR}",
)


# ---------------------------------------------------------------------------
# 测试常量
# ---------------------------------------------------------------------------

# 测试数据库配置: nsideData=512, tileNside=128
# 默认 LOD 层级 (nsideData=512):
#   Level 0: nside=8   (512/64)
#   Level 1: nside=32  (512/16)
#   Level 2: nside=128 (512/4)
#   Level 3: nside=512 (数据层)
# tileNside=128 = Level 2 nside, 每个 tile 包含 (512/128)^2=16 个数据层像素
_TEST_NSIDE_DATA = 512
_TEST_TILE_NSIDE = 128
# Level 2 对应 nside=128 (数据层 nside/4)
_LEVEL_NSIDE_128 = 2


# ---------------------------------------------------------------------------
# 辅助函数
# ---------------------------------------------------------------------------

def _make_config(nside_data=_TEST_NSIDE_DATA, tile_nside=_TEST_TILE_NSIDE,
                 bands=None, nside_lod=None, sigma_low=3.0, sigma_high=3.0,
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
    """构造单帧数据

    pixels: [(healpixPix, value, snr, weight), ...]
    返回: {"pixels": [{"healpixPix":..,"value":..,"snr":..,"weight":..}, ...]}
    """
    return {"pixels": [
        {"healpixPix": int(pix), "value": float(val), "snr": float(snr),
         "weight": float(w)}
        for pix, val, snr, w in pixels
    ]}


def _create_test_db(tmp_path, pixels=None, nside_data=_TEST_NSIDE_DATA,
                    tile_nside=_TEST_TILE_NSIDE):
    """创建测试用堆栈数据库并添加数据

    Args:
        tmp_path: pytest 临时目录
        pixels: [(healpixPix, value, snr, weight), ...] 列表
        nside_data: 数据层 nside
        tile_nside: 分区 nside

    Returns:
        db_path: 数据库目录路径
    """
    db_path = str(tmp_path / "test_db")
    config = _make_config(nside_data=nside_data, tile_nside=tile_nside)
    db = StackDatabase.create(db_path, config)
    try:
        if pixels:
            frame = _make_frame(pixels)
            db.update_global([frame])
    finally:
        db.close()
    return db_path


# ---------------------------------------------------------------------------
# 测试 1: 基本降采样
# ---------------------------------------------------------------------------

@_needs_stack
def test_downsample_basic(tmp_path):
    """基本降采样: 4 个子像素 → 1 个父像素, 验证加权均值和权重和

    构造 nside=512 的 4 个子像素 (同一父像素, NESTED scheme):
      pixel 0,1,2,3 → 父像素 0 (nside=128, ipix_coarse = ipix_fine >> 4)

    降采样到 nside=128 (Level 2):
      value  = Σ(value*weight) / Σ(weight) = (1+2+3+4)/4 = 2.5
      weight = Σ(weight) = 4.0
      count  = 4 (贡献的子像素数)
    """
    # 4 个子像素 (nside=512, NESTED: 0,1,2,3 → 父 0 at nside=128)
    pixels = [
        (0, 1.0, 10.0, 1.0),
        (1, 2.0, 10.0, 1.0),
        (2, 3.0, 10.0, 1.0),
        (3, 4.0, 10.0, 1.0),
    ]

    db_path = _create_test_db(tmp_path, pixels=pixels)
    print(f"[downsample] 数据库创建: {db_path}")

    # 生成 LOD 金字塔
    ret = LodManager.generate_full(db_path, band_index=0)
    assert ret is True, "generate_full 失败"
    print("[downsample] generate_full 完成")

    # 按需计算 Level 2 (nside=128), tile 0
    tile_data = LodManager.compute_on_demand(
        db_path, band_index=0, level=_LEVEL_NSIDE_128, tile_ipix=0)

    print(f"[downsample] tile_data: {json.dumps(tile_data, indent=2)}")

    # 验证返回结构
    assert "nside" in tile_data, "返回缺少 nside 字段"
    assert "pixels" in tile_data, "返回缺少 pixels 字段"
    assert "values" in tile_data, "返回缺少 values 字段"
    assert "weights" in tile_data, "返回缺少 weights 字段"
    assert "counts" in tile_data, "返回缺少 counts 字段"

    # 验证 nside
    expected_nside = _TEST_NSIDE_DATA // 4  # 512/4 = 128
    assert tile_data["nside"] == expected_nside, (
        f"nside 不匹配: {tile_data['nside']} != {expected_nside}")

    # 验证: 1 个父像素
    pixel_count = tile_data.get("pixelCount", len(tile_data["pixels"]))
    assert pixel_count == 1, (
        f"应只有 1 个父像素, 实际: {pixel_count}")

    # 验证: 值 = 加权均值 = (1+2+3+4)/4 = 2.5
    expected_value = (1.0 * 1.0 + 2.0 * 1.0 + 3.0 * 1.0 + 4.0 * 1.0) / 4.0
    actual_value = tile_data["values"][0]
    assert actual_value == pytest.approx(expected_value, rel=0.01), (
        f"加权均值不匹配: {actual_value} != {expected_value}")

    # 验证: 权重 = 权重和 = 4.0
    expected_weight = 1.0 + 1.0 + 1.0 + 1.0
    actual_weight = tile_data["weights"][0]
    assert actual_weight == pytest.approx(expected_weight, rel=0.01), (
        f"权重和不匹配: {actual_weight} != {expected_weight}")

    # 验证: count = 4 (贡献的子像素数)
    assert tile_data["counts"][0] == 4, (
        f"count 应为 4, 实际: {tile_data['counts'][0]}")

    print(f"[downsample] 父像素: value={actual_value:.4f} (期望 {expected_value}), "
          f"weight={actual_weight:.4f} (期望 {expected_weight}), "
          f"count={tile_data['counts'][0]}")
    print("[downsample] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 2: 完整金字塔生成
# ---------------------------------------------------------------------------

@_needs_stack
def test_lod_generate_full(tmp_path):
    """完整金字塔生成: 创建堆栈数据库 → generate_full → 验证 LOD 层目录存在

    默认 LOD 层级 (nsideData=512):
      Level 0: nside=8   → tiles/nside_8/
      Level 1: nside=32  → tiles/nside_32/
      Level 2: nside=128 → tiles/nside_128/
      Level 3: nside=512 (数据层, 已存在)
    """
    pixels = [
        (0, 1.0, 10.0, 1.0),
        (1, 2.0, 10.0, 1.0),
    ]
    db_path = _create_test_db(tmp_path, pixels=pixels)
    print(f"[genfull] 数据库创建: {db_path}")

    # 验证数据层目录存在
    data_dir = os.path.join(db_path, "tiles", f"nside_{_TEST_NSIDE_DATA}")
    assert os.path.isdir(data_dir), f"数据层目录不存在: {data_dir}"

    # 生成 LOD 金字塔
    ret = LodManager.generate_full(db_path, band_index=0)
    assert ret is True, "generate_full 失败"
    print("[genfull] generate_full 完成")

    # 验证 LOD 层目录存在 (nside_8, nside_32, nside_128)
    tiles_dir = os.path.join(db_path, "tiles")
    expected_lod_nsides = [
        _TEST_NSIDE_DATA // 64,  # Level 0: nside=8
        _TEST_NSIDE_DATA // 16,  # Level 1: nside=32
        _TEST_NSIDE_DATA // 4,   # Level 2: nside=128
    ]

    for nside in expected_lod_nsides:
        lod_dir = os.path.join(tiles_dir, f"nside_{nside}")
        assert os.path.isdir(lod_dir), (
            f"LOD 目录不存在: nside_{nside}")
        print(f"[genfull] LOD 目录存在: nside_{nside}")

    # 验证 .ahpl 文件存在
    ahpl_count = 0
    for nside in expected_lod_nsides:
        lod_dir = os.path.join(tiles_dir, f"nside_{nside}")
        if os.path.isdir(lod_dir):
            for fname in os.listdir(lod_dir):
                if fname.endswith(".ahpl"):
                    ahpl_count += 1
                    print(f"[genfull] LOD tile 文件: nside_{nside}/{fname}")

    assert ahpl_count > 0, "未生成任何 .ahpl 文件"

    # 验证层级数
    level_count = LodManager.get_level_count(db_path)
    assert level_count == 4, (
        f"层级数应为 4, 实际: {level_count}")
    print(f"[genfull] 层级数: {level_count}")

    print("[genfull] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 3: 按需计算
# ---------------------------------------------------------------------------

@_needs_stack
def test_compute_on_demand(tmp_path):
    """按需计算: generate_full 后请求某 tile 的某层, 验证返回 dict 结构"""
    pixels = [
        (0, 1.0, 10.0, 1.0),
        (1, 2.0, 10.0, 1.0),
        (2, 3.0, 10.0, 1.0),
        (3, 4.0, 10.0, 1.0),
    ]
    db_path = _create_test_db(tmp_path, pixels=pixels)

    # 生成 LOD 金字塔
    LodManager.generate_full(db_path, band_index=0)
    print("[demand] generate_full 完成")

    # 请求 Level 2 (nside=128), tile 0
    tile_data = LodManager.compute_on_demand(
        db_path, band_index=0, level=_LEVEL_NSIDE_128, tile_ipix=0)

    print(f"[demand] tile_data keys: {list(tile_data.keys())}")
    print(f"[demand] nside={tile_data.get('nside')}, "
          f"pixelCount={tile_data.get('pixelCount')}")

    # 验证返回 dict 包含必需字段
    required_fields = ["nside", "tileIpix", "pixelCount",
                       "pixels", "values", "weights", "counts"]
    for field in required_fields:
        assert field in tile_data, f"返回 dict 缺少字段: {field}"

    # 验证字段类型
    assert isinstance(tile_data["nside"], int), "nside 应为 int"
    assert isinstance(tile_data["pixels"], list), "pixels 应为 list"
    assert isinstance(tile_data["values"], list), "values 应为 list"
    assert isinstance(tile_data["weights"], list), "weights 应为 list"
    assert isinstance(tile_data["counts"], list), "counts 应为 list"

    # 验证 nside
    expected_nside = _TEST_NSIDE_DATA // 4  # 128
    assert tile_data["nside"] == expected_nside, (
        f"nside 不匹配: {tile_data['nside']} != {expected_nside}")

    # 验证 tileIpix
    assert tile_data["tileIpix"] == 0, (
        f"tileIpix 不匹配: {tile_data['tileIpix']} != 0")

    # 验证数组长度一致
    n = tile_data["pixelCount"]
    assert len(tile_data["pixels"]) == n, "pixels 数组长度不匹配"
    assert len(tile_data["values"]) == n, "values 数组长度不匹配"
    assert len(tile_data["weights"]) == n, "weights 数组长度不匹配"
    assert len(tile_data["counts"]) == n, "counts 数组长度不匹配"

    # 验证至少有 1 个像素
    assert n >= 1, f"pixelCount 应 >= 1, 实际: {n}"

    # 也请求 Level 1 (nside=32) 验证更粗层
    tile_data_l1 = LodManager.compute_on_demand(
        db_path, band_index=0, level=1, tile_ipix=0)
    assert "nside" in tile_data_l1, "Level 1 返回缺少 nside"
    expected_nside_l1 = _TEST_NSIDE_DATA // 16  # 32
    assert tile_data_l1["nside"] == expected_nside_l1, (
        f"Level 1 nside 不匹配: {tile_data_l1['nside']} != {expected_nside_l1}")

    print(f"[demand] Level 2: nside={tile_data['nside']}, pixels={n}")
    print(f"[demand] Level 1: nside={tile_data_l1['nside']}, "
          f"pixels={tile_data_l1['pixelCount']}")
    print("[demand] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 4: 增量更新
# ---------------------------------------------------------------------------

@_needs_stack
def test_incremental_update(tmp_path):
    """增量更新: generate_full → 更新数据层 → update_incremental → 验证 LOD 值变化

    初始: 4 像素 (值 1,2,3,4, 权重 1,1,1,1)
      父像素 value = (1+2+3+4)/4 = 2.5, weight = 4.0

    更新: pixel 0 添加新观测 (值=10, 权重=1)
      pixel 0: value = (1*1+10*1)/(1+1) = 5.5, weight = 2.0
      父像素 value = (5.5*2+2*1+3*1+4*1)/(2+1+1+1) = 20/5 = 4.0
      父像素 weight = 2+1+1+1 = 5.0
    """
    # 初始数据
    pixels = [
        (0, 1.0, 10.0, 1.0),
        (1, 2.0, 10.0, 1.0),
        (2, 3.0, 10.0, 1.0),
        (3, 4.0, 10.0, 1.0),
    ]
    db_path = _create_test_db(tmp_path, pixels=pixels)

    # 生成 LOD 金字塔
    LodManager.generate_full(db_path, band_index=0)
    print("[incr] generate_full 完成")

    # 读取初始 LOD 值
    tile_data_before = LodManager.compute_on_demand(
        db_path, band_index=0, level=_LEVEL_NSIDE_128, tile_ipix=0)

    initial_value = tile_data_before["values"][0]
    initial_weight = tile_data_before["weights"][0]
    print(f"[incr] 初始: value={initial_value:.4f}, weight={initial_weight:.4f}")

    # 验证初始值
    expected_initial_value = 2.5
    expected_initial_weight = 4.0
    assert initial_value == pytest.approx(expected_initial_value, rel=0.01), (
        f"初始 value 不匹配: {initial_value} != {expected_initial_value}")
    assert initial_weight == pytest.approx(expected_initial_weight, rel=0.01), (
        f"初始 weight 不匹配: {initial_weight} != {expected_initial_weight}")

    # 更新数据层: 给 pixel 0 添加新观测 (值=10, 权重=1)
    db = StackDatabase.open(db_path)
    try:
        frame = _make_frame([(0, 10.0, 10.0, 1.0)])
        ret = db.update_global([frame])
        print(f"[incr] 数据层更新: update_global 返回 {ret}")
        assert ret > 0, f"update_global 失败: ret={ret}"
    finally:
        db.close()

    # 增量更新 LOD (tile 0 的数据变化了)
    ret = LodManager.update_incremental(db_path, band_index=0, changed_tiles=[0])
    assert ret is True, "update_incremental 失败"
    print("[incr] update_incremental 完成")

    # 读取更新后的 LOD 值
    tile_data_after = LodManager.compute_on_demand(
        db_path, band_index=0, level=_LEVEL_NSIDE_128, tile_ipix=0)

    updated_value = tile_data_after["values"][0]
    updated_weight = tile_data_after["weights"][0]
    print(f"[incr] 更新后: value={updated_value:.4f}, weight={updated_weight:.4f}")

    # 验证值已变化
    assert updated_value != initial_value, (
        f"value 未变化: {updated_value} == {initial_value}")
    assert updated_weight != initial_weight, (
        f"weight 未变化: {updated_weight} == {initial_weight}")

    # 验证更新后的值正确
    # pixel 0 更新后: value=(1*1+10*1)/(1+1)=5.5, weight=2.0
    # 父像素: value=(5.5*2+2*1+3*1+4*1)/(2+1+1+1)=20/5=4.0, weight=5.0
    expected_updated_value = (5.5 * 2.0 + 2.0 * 1.0 + 3.0 * 1.0 + 4.0 * 1.0) / 5.0
    expected_updated_weight = 2.0 + 1.0 + 1.0 + 1.0

    assert updated_value == pytest.approx(expected_updated_value, rel=0.01), (
        f"更新后 value 不匹配: {updated_value} != {expected_updated_value}")
    assert updated_weight == pytest.approx(expected_updated_weight, rel=0.01), (
        f"更新后 weight 不匹配: {updated_weight} != {expected_updated_weight}")

    print(f"[incr] 值变化: {initial_value:.4f} → {updated_value:.4f} "
          f"(期望 {expected_updated_value})")
    print(f"[incr] 权重变化: {initial_weight:.4f} → {updated_weight:.4f} "
          f"(期望 {expected_updated_weight})")
    print("[incr] 全部断言通过")
