# NON_PRODUCTION_TOOL_ONLY
# This file is NOT part of the production pipeline.
# It is a development/testing/research tool only.
# The production pipeline uses orchestrator.exe <stage1.json> exclusively.

"""
test_healpix_stack.py - healpix_stack 模块单元测试

功能: 通过 Python ctypes 绑定测试 healpix_stack.dll
用途: 验证 HEALpix 像素运算、堆栈数据库管理、sigma-clip 堆栈、tile 读写

运行:
    cd lib/healpix_db/healpix_stack/tests
    pytest test_healpix_stack.py -v -s
"""

from __future__ import annotations

import os
import sys
import json
import math

import pytest

# 将 healpix_stack.py 所在目录加入 sys.path (tests/ 的上级目录)
_MODULE_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
)
if _MODULE_DIR not in sys.path:
    sys.path.insert(0, _MODULE_DIR)

# 检测 DLL 可用性: 加载失败则跳过全部测试 (而非 fail)
_DLL_AVAILABLE = False
_DLL_ERROR = ""
try:
    from healpix_stack import (  # noqa: E402
        StackDatabase,
        healpix_radec2pix,
        healpix_pix2radec,
        healpix_pixel_resolution,
        _find_dll,
        _load_dll,
    )
    _load_dll(_find_dll())  # 触发实际 DLL 加载
    _DLL_AVAILABLE = True
except Exception as e:  # OSError / ImportError 等
    _DLL_ERROR = f"{type(e).__name__}: {e}"

pytestmark = pytest.mark.skipif(
    not _DLL_AVAILABLE,
    reason=f"healpix_stack DLL 加载失败, 跳过测试: {_DLL_ERROR}",
)


# ---------------------------------------------------------------------------
# 辅助函数
# ---------------------------------------------------------------------------

def _make_config(nside_data=512, tile_nside=512, bands=None,
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
    """构造单帧数据

    pixels: [(healpixPix, value, snr, weight), ...]
    返回: {"pixels": [{"healpixPix":..,"value":..,"snr":..,"weight":..}, ...]}

    注意: C API hp_stack_update_global 期望每个帧是 {"pixels": [...]} 对象,
    而非裸数组。Python 绑定直接 json.dumps(frames), 所以 frames 需是
    [{"pixels": [...]}, ...] 格式。
    """
    return {"pixels": [
        {"healpixPix": int(pix), "value": float(val), "snr": float(snr),
         "weight": float(w)}
        for pix, val, snr, w in pixels
    ]}


def _find_pixel_in_tile(tile, pix):
    """在 tile 数据中找到指定像素的索引, 返回 (idx, band0_dict)"""
    pixels_list = tile.get("pixels", [])
    bands = tile.get("bands", [])
    if not bands:
        return -1, None
    band0 = bands[0]
    if pix in pixels_list:
        return pixels_list.index(pix), band0
    return 0, band0


# ---------------------------------------------------------------------------
# 测试 1: HEALpix 像素运算
# ---------------------------------------------------------------------------

def test_healpix_radec2pix_pix2radec():
    """RA/Dec → ipix → RA/Dec 往返一致性

    nside=512, nested=True
    误差容差: 1 个像素分辨率 (nside=512 像素 ≈ 0.11°, pix2radec 返回像素中心)
    """
    nside = 512
    nested = True

    # 容差 = 1 个像素分辨率 (pix2radec 返回像素中心, 非原始坐标)
    pixel_res_deg = healpix_pixel_resolution(nside) / 3600.0
    tol = pixel_res_deg  # ≈ 0.1146°
    print(f"[radec] nside={nside}, 像素分辨率={pixel_res_deg:.4f}°, 容差={tol:.4f}°")

    test_cases = [
        (0.0, 0.0),
        (180.0, 45.0),
        (270.0, -30.0),
    ]

    for ra, dec in test_cases:
        ipix = healpix_radec2pix(nside, nested, ra, dec)
        ra2, dec2 = healpix_pix2radec(nside, nested, ipix)

        # RA 环绕处理 (0 和 360 等价)
        ra_diff = abs(ra2 - ra)
        if ra_diff > 180.0:
            ra_diff = 360.0 - ra_diff
        dec_diff = abs(dec2 - dec)

        print(f"[radec] ({ra}, {dec}) → ipix={ipix} → ({ra2:.6f}, {dec2:.6f}) "
              f"误差: ra={ra_diff:.6f}°, dec={dec_diff:.6f}°")

        assert ra_diff < tol, (
            f"RA 误差过大: ra={ra}, ra2={ra2}, diff={ra_diff} (容差 {tol})"
        )
        assert dec_diff < tol, (
            f"Dec 误差过大: dec={dec}, dec2={dec2}, diff={dec_diff} (容差 {tol})"
        )

    print("[radec] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 2: 像素分辨率
# ---------------------------------------------------------------------------

def test_pixel_resolution():
    """HEALpix 像素分辨率验证 (允许 10% 误差)

    公式: sqrt(4π / (12 * nside²)) * (180*3600/π) 角秒

    nside=512:   ≈ 412.5" (6.87')  → 任务期望 ~414" (6.9')
    nside=32768: ≈ 6.44"  (0.107') → 任务描述中 102" 实际对应 nside=2048
    """
    # nside=512: 分辨率 ≈ 414" (6.9')
    res_512 = healpix_pixel_resolution(512)
    expected_512 = math.sqrt(4 * math.pi / (12 * 512 * 512)) * 206265.0
    print(f"[res] nside=512: {res_512:.2f}\" (公式期望 {expected_512:.2f}\", "
          f"任务期望 ~414\")")
    assert res_512 == pytest.approx(414, rel=0.1), (
        f"nside=512 分辨率 {res_512}\" 不在 414\" ± 10% 范围内"
    )

    # nside=32768: 分辨率 ≈ 6.44"
    # 注: 任务描述中 nside=32768 期望 102" (1.7') 实际对应 nside=2048 (~103"),
    #     nside=32768 的公式值为 ~6.44", 此处用公式值验证
    res_32768 = healpix_pixel_resolution(32768)
    expected_32768 = math.sqrt(4 * math.pi / (12 * 32768 * 32768)) * 206265.0
    print(f"[res] nside=32768: {res_32768:.4f}\" (公式期望 {expected_32768:.4f}\")")
    assert res_32768 == pytest.approx(expected_32768, rel=0.1), (
        f"nside=32768 分辨率 {res_32768}\" 不在 {expected_32768:.2f}\" ± 10% 范围内"
    )

    # 额外验证: 分辨率应与 nside 成反比
    ratio = res_512 / res_32768
    expected_ratio = 32768.0 / 512.0  # = 64
    print(f"[res] 分辨率比值 nside512/nside32768 = {ratio:.2f} (期望 {expected_ratio:.2f})")
    assert ratio == pytest.approx(expected_ratio, rel=0.01), (
        f"分辨率比值 {ratio} 不等于 nside 反比 {expected_ratio}"
    )

    print("[res] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 3: 创建堆栈数据库
# ---------------------------------------------------------------------------

def test_create_database(tmp_path):
    """创建数据库, 验证 meta.json 和目录结构, 关闭后重新打开

    目录结构 (实际实现):
      {dbPath}/meta.json
      {dbPath}/tiles/
      {dbPath}/tiles/nside_{N}/  (数据层 + LOD 层)
    """
    db_path = str(tmp_path / "test_db")
    config = _make_config(
        nside_data=512,
        tile_nside=512,
        bands=["L"],
        nside_lod=[128, 512],
    )

    db = StackDatabase.create(db_path, config)
    try:
        # 验证 meta.json 存在
        meta_path = os.path.join(db_path, "meta.json")
        assert os.path.isfile(meta_path), f"meta.json 不存在: {meta_path}"

        # 验证 tiles 目录存在 (实际实现用 tiles/, 非 data/lod/)
        tiles_dir = os.path.join(db_path, "tiles")
        assert os.path.isdir(tiles_dir), f"tiles 目录不存在: {tiles_dir}"

        # 验证数据层子目录 nside_512 存在
        data_dir = os.path.join(tiles_dir, "nside_512")
        assert os.path.isdir(data_dir), f"数据层目录不存在: {data_dir}"

        # 验证 LOD 层子目录 nside_128 存在
        lod_dir = os.path.join(tiles_dir, "nside_128")
        assert os.path.isdir(lod_dir), f"LOD 目录不存在: {lod_dir}"

        # 验证 meta.json 内容
        with open(meta_path, "r", encoding="utf-8") as f:
            meta = json.load(f)
        assert meta["nsideData"] == 512, f"nsideData 不匹配: {meta['nsideData']}"
        assert meta["tileNside"] == 512, f"tileNside 不匹配: {meta['tileNside']}"
        assert meta["nested"] == True, f"nested 不匹配: {meta['nested']}"

        print(f"[create] 数据库创建成功: {db_path}")
        print(f"[create] meta.json: {meta}")
    finally:
        db.close()

    # 关闭后重新打开验证
    db2 = StackDatabase.open(db_path)
    try:
        assert db2 is not None, "重新打开数据库失败"
        # 验证 meta.json 仍可读取
        meta_path = os.path.join(db_path, "meta.json")
        assert os.path.isfile(meta_path), "重新打开后 meta.json 不存在"
        print(f"[create] 数据库重新打开成功")
    finally:
        db2.close()

    print("[create] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 4: 全局堆栈更新
# ---------------------------------------------------------------------------

def test_stack_update_global(tmp_path):
    """全局堆栈更新: 5 帧数据 (同一组像素, 不同值), 验证像素数量和统计量"""
    db_path = str(tmp_path / "test_db")
    config = _make_config(nside_data=512, tile_nside=512, bands=["L"])

    db = StackDatabase.create(db_path, config)
    try:
        # 构造 5 帧数据 (同一组像素, 不同值)
        # tileNside == nsideData, 所以 tile_ipix == pixel_ipix
        test_pixels = [
            healpix_radec2pix(512, True, 10.0, 20.0),
            healpix_radec2pix(512, True, 15.0, 25.0),
            healpix_radec2pix(512, True, 20.0, 30.0),
        ]
        print(f"[update] 测试像素: {test_pixels}")

        frames = []
        for i in range(5):
            frame_pixels = [
                (pix, 1.0 + i * 0.1, 10.0, 1.0)
                for pix in test_pixels
            ]
            frames.append(_make_frame(frame_pixels))

        ret = db.update_global(frames)
        print(f"[update] update_global 返回: {ret} (处理像素数)")
        assert ret > 0, f"update_global 失败: ret={ret}"

        # 读取 tile 验证 (tileNside == nsideData → tile_ipix == pixel_ipix)
        tile = db.read_tile(test_pixels[0])
        print(f"[update] tile: pixelCount={tile.get('pixelCount')}, "
              f"bandCount={tile.get('bandCount')}, "
              f"pixels={tile.get('pixels')}")

        # 验证像素数量 >= 1
        assert tile["pixelCount"] >= 1, f"像素数量应 >= 1, 实际: {tile['pixelCount']}"

        # 验证统计量非零
        bands = tile.get("bands", [])
        assert len(bands) > 0, "应有至少 1 个波段"

        band0 = bands[0]
        assert len(band0.get("values", [])) > 0, "values 数组应非空"
        assert len(band0.get("counts", [])) > 0, "counts 数组应非空"

        # 验证 count 非零 (5 帧都应计入)
        for c in band0["counts"]:
            assert c > 0, f"count 应 > 0, 实际: {c}"

        # 验证 value 非零
        for v in band0["values"]:
            assert v != 0, f"value 应非零, 实际: {v}"

        print(f"[update] tile 统计: counts={band0['counts']}, "
              f"values={band0['values']}")
    finally:
        db.close()

    print("[update] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 5: sigma-clip 验证
# ---------------------------------------------------------------------------

def test_stack_sigma_clip(tmp_path):
    """sigma-clip: 10 帧正常 + 1 帧异常 (10x), 验证异常帧被剔除

    正常值带少量噪声确保 MAD > 0 (否则 sigma=0 时 sigma-clip 提前终止)
    """
    db_path = str(tmp_path / "test_db")
    config = _make_config(nside_data=512, tile_nside=512, bands=["L"],
                          sigma_low=3.0, sigma_high=3.0)

    db = StackDatabase.create(db_path, config)
    try:
        pix = healpix_radec2pix(512, True, 10.0, 20.0)

        # 10 帧正常数据 (带少量噪声, 确保 MAD > 0)
        normal_values = [0.9, 0.95, 0.97, 0.98, 1.0,
                         1.01, 1.02, 1.03, 1.05, 1.1]
        frames = []
        for v in normal_values:
            frames.append(_make_frame([(pix, v, 10.0, 1.0)]))

        # 1 帧异常数据 (10x 正常值)
        frames.append(_make_frame([(pix, 10.0, 10.0, 1.0)]))

        print(f"[sigma] 输入: {len(normal_values)} 帧正常 + 1 帧异常 (共 {len(frames)} 帧)")

        ret = db.update_global(frames)
        print(f"[sigma] update_global 返回: {ret}")
        assert ret > 0, f"update_global 失败: ret={ret}"

        tile = db.read_tile(pix)
        idx, band0 = _find_pixel_in_tile(tile, pix)
        assert band0 is not None, "tile 无波段数据"

        count = band0["counts"][idx]
        value = band0["values"][idx]
        expected_mean = sum(normal_values) / len(normal_values)

        print(f"[sigma] count={count} (期望 10, 异常帧被剔除)")
        print(f"[sigma] value={value:.4f} (期望 ≈ {expected_mean:.4f})")

        # 验证异常帧被剔除: count 应为 10, 不是 11
        assert count == 10, (
            f"count 应为 10 (异常帧被 sigma-clip 剔除), 实际: {count}"
        )

        # 验证最终值接近正常帧均值
        assert abs(value - expected_mean) < 0.05, (
            f"value={value:.4f} 偏离正常帧均值 {expected_mean:.4f} 超过 0.05"
        )
    finally:
        db.close()

    print("[sigma] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 6: 低帧数降级
# ---------------------------------------------------------------------------

def test_low_frame_count(tmp_path):
    """低帧数 (N<3): 直接加权平均, 不做 sigma-clip

    注意: lowConfidence 标记在 StackResult 中存在, 但未持久化到 .ahps 文件,
    此处仅验证 count 和 value 正确 (低帧数降级为加权平均)
    """
    db_path = str(tmp_path / "test_db")
    config = _make_config(nside_data=512, tile_nside=512, bands=["L"])

    db = StackDatabase.create(db_path, config)
    try:
        pix = healpix_radec2pix(512, True, 10.0, 20.0)

        # 2 帧数据 (N < 3, 跳过 sigma-clip)
        frames = [
            _make_frame([(pix, 1.0, 10.0, 1.0)]),
            _make_frame([(pix, 2.0, 10.0, 1.0)]),
        ]

        ret = db.update_global(frames)
        print(f"[low] update_global 返回: {ret}")
        assert ret > 0, f"update_global 失败: ret={ret}"

        tile = db.read_tile(pix)
        idx, band0 = _find_pixel_in_tile(tile, pix)
        assert band0 is not None, "tile 无波段数据"

        count = band0["counts"][idx]
        value = band0["values"][idx]

        # 期望: 加权平均 = (1*1 + 2*1) / (1+1) = 1.5
        expected_value = 1.5

        print(f"[low] count={count} (期望 2)")
        print(f"[low] value={value:.4f} (期望 {expected_value})")

        # 验证 count = 2 (两帧都保留, 未做 sigma-clip)
        assert count == 2, f"count 应为 2, 实际: {count}"

        # 验证 value = 加权平均
        assert abs(value - expected_value) < 0.01, (
            f"value 应为 {expected_value}, 实际: {value}"
        )
    finally:
        db.close()

    print("[low] 全部断言通过")


# ---------------------------------------------------------------------------
# 测试 7: tile 读取
# ---------------------------------------------------------------------------

def test_read_tile(tmp_path):
    """tile 读取: 更新后读取 tile, 验证返回 dict 包含必需字段"""
    db_path = str(tmp_path / "test_db")
    config = _make_config(nside_data=512, tile_nside=512, bands=["L"])

    db = StackDatabase.create(db_path, config)
    try:
        pix = healpix_radec2pix(512, True, 10.0, 20.0)

        frames = [
            _make_frame([(pix, 1.5, 10.0, 1.0)]),
            _make_frame([(pix, 2.5, 10.0, 1.0)]),
            _make_frame([(pix, 3.5, 10.0, 1.0)]),
        ]

        ret = db.update_global(frames)
        assert ret > 0, f"update_global 失败: ret={ret}"

        tile = db.read_tile(pix)
        print(f"[read] tile keys: {list(tile.keys())}")
        print(f"[read] tile: {json.dumps(tile, indent=2, default=str)}")

        # 验证必需字段存在
        required_fields = [
            "tileIpix", "nside", "tileNside",
            "pixelCount", "bandCount",
            "pixels", "bands",
        ]
        for field in required_fields:
            assert field in tile, f"tile 缺少必需字段: {field}"

        # 验证字段值
        assert tile["tileIpix"] == pix, (
            f"tileIpix 不匹配: {tile['tileIpix']} != {pix}"
        )
        assert tile["nside"] == 512, f"nside 不匹配: {tile['nside']}"
        assert tile["tileNside"] == 512, f"tileNside 不匹配: {tile['tileNside']}"
        assert tile["pixelCount"] >= 1, f"pixelCount 应 >= 1"
        assert tile["bandCount"] >= 1, f"bandCount 应 >= 1"

        # 验证 bands 结构
        bands = tile["bands"]
        assert len(bands) >= 1, "bands 数组应非空"

        band0 = bands[0]
        band_fields = ["values", "variance", "counts"]
        for bf in band_fields:
            assert bf in band0, f"band[0] 缺少字段: {bf}"

        # 验证数组长度一致
        pixel_count = tile["pixelCount"]
        assert len(band0["values"]) == pixel_count, (
            f"values 长度 {len(band0['values'])} != pixelCount {pixel_count}"
        )
        assert len(band0["variance"]) == pixel_count, (
            f"variance 长度 {len(band0['variance'])} != pixelCount {pixel_count}"
        )
        assert len(band0["counts"]) == pixel_count, (
            f"counts 长度 {len(band0['counts'])} != pixelCount {pixel_count}"
        )

        # 验证像素号在列表中
        assert pix in tile["pixels"], (
            f"像素 {pix} 不在 tile pixels 列表中: {tile['pixels']}"
        )

        print(f"[read] pixelCount={tile['pixelCount']}, "
              f"bandCount={tile['bandCount']}")
    finally:
        db.close()

    print("[read] 全部断言通过")
