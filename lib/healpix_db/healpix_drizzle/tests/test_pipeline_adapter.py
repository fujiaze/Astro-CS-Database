# NON_PRODUCTION_TOOL_ONLY
# This file is NOT part of the production pipeline.
# It is a development/testing/research tool only.
# The production pipeline uses orchestrator.exe <stage1.json> exclusively.

"""
test_pipeline_adapter.py - Drizzle 管线适配器测试

功能: 测试 pipeline_adapter.DrizzleParams + register_drizzle_handler
用途: 验证从 PipelineFrame (data 块 + header KV 块) 经命名块直通
      hp_drizzle_run → 生成 .hiss 文件的完整流程, 并用 hiss_read 验证

覆盖场景:
    1. 正常流程: data 块 + WCS header → .hiss 文件生成 + hiss_read 验证
    2. 错误处理: 缺少 data 块 → handler 返回 -1, run_single 抛 RuntimeError
    3. SIP 系数: header 含 A_ORDER/B_ORDER → Drizzle 成功 + .hiss 生成

运行:
    cd lib/healpix_db/healpix_drizzle
    python -m pytest tests/test_pipeline_adapter.py -v -s

依赖:
    - healpix_drizzle.dll (healpix_drizzle/ 目录下)
    - healpix_io.dll (healpix_drizzle/ 目录下, 或 healpix_io/ 目录下)
    - astro_image_io.dll (astro_image_io/ 目录下)
    - astropy, numpy, pytest
"""

from __future__ import annotations

import os
import sys

import numpy as np
import pytest

# ============================================================================
# 路径设置: 添加 healpix_drizzle, healpix_io 和 astro_image_io 模块路径
# ============================================================================
_TEST_DIR = os.path.dirname(os.path.abspath(__file__))
# healpix_drizzle 模块目录 (tests/ 的上级)
_DRIZZLE_MODULE_DIR = os.path.normpath(os.path.join(_TEST_DIR, ".."))
# healpix_io 模块目录 (lib/healpix_db/healpix_io)
_HIO_MODULE_DIR = os.path.normpath(os.path.join(_TEST_DIR, "..", "..", "healpix_io"))
# astro_image_io 模块目录 (lib/astro_image_io/python)
_AIO_MODULE_DIR = os.path.normpath(
    os.path.join(_TEST_DIR, "..", "..", "..", "astro_image_io", "python")
)
# astro_image_io.dll 所在目录 (lib/astro_image_io/)
_AIO_DLL_DIR = os.path.normpath(os.path.join(_AIO_MODULE_DIR, ".."))
# healpix_io.dll 所在目录 (lib/healpix_db/healpix_io/)
_HIO_DLL_DIR = _HIO_MODULE_DIR

# healpix_drizzle.dll 依赖 astro_image_io.dll + healpix_io.dll, 需将它们加入 DLL 搜索路径
for _dll_dir in (_AIO_DLL_DIR, _HIO_DLL_DIR, _DRIZZLE_MODULE_DIR):
    try:
        os.add_dll_directory(_dll_dir)
    except (OSError, FileNotFoundError):
        pass
    os.environ["PATH"] = _dll_dir + ";" + os.environ.get("PATH", "")

for _p in (_DRIZZLE_MODULE_DIR, _HIO_MODULE_DIR, _AIO_MODULE_DIR):
    if _p not in sys.path:
        sys.path.insert(0, _p)

# ============================================================================
# 检测依赖可用性: 加载失败则跳过全部测试
# ============================================================================
_DEPS_AVAILABLE = False
_DEPS_ERROR = ""
try:
    from astro_image_io import (  # noqa: E402
        PipelineFramePy, PipelineEngine, STAGE_DRIZZLE,
    )
    from healpix_drizzle import _get_dll  # noqa: E402
    _get_dll()  # 触发 healpix_drizzle.dll 加载
    from healpix_io import hiss_read  # noqa: E402
    from pipeline_adapter import DrizzleParams, register_drizzle_handler  # noqa: E402
    _DEPS_AVAILABLE = True
except Exception as e:
    _DEPS_ERROR = f"{type(e).__name__}: {e}"

pytestmark = pytest.mark.skipif(
    not _DEPS_AVAILABLE,
    reason=f"依赖加载失败, 跳过测试: {_DEPS_ERROR}",
)


# ============================================================================
# 辅助函数: 构造含 data 块 + header KV 块的 PipelineFramePy
# ============================================================================
def _make_test_frame(width=64, height=64, value=1.0, with_sip=False):
    """构造测试用 PipelineFramePy

    参数:
        width/height: 图像尺寸
        value:        均匀像素值
        with_sip:     是否添加 SIP 系数 (A_ORDER=2, B_ORDER=2)
    """
    frame = PipelineFramePy()
    pixels = np.full((height, width), value, dtype=np.float32)
    frame.add_block("data", pixels, description="图像像素")

    # header KV 块: WCS + 元数据
    frame.kv_set("header", "CTYPE1", "RA---TAN")
    frame.kv_set("header", "CTYPE2", "DEC--TAN")
    frame.kv_set("header", "CRVAL1", "180.0")
    frame.kv_set("header", "CRVAL2", "30.0")
    frame.kv_set("header", "CRPIX1", str(width / 2.0 + 0.5))
    frame.kv_set("header", "CRPIX2", str(height / 2.0 + 0.5))
    frame.kv_set("header", "CD1_1", "-0.00277778")
    frame.kv_set("header", "CD1_2", "0.0")
    frame.kv_set("header", "CD2_1", "0.0")
    frame.kv_set("header", "CD2_2", "0.00277778")
    frame.kv_set("header", "RADESYS", "ICRS")
    frame.kv_set("header", "EQUINOX", "2000.0")
    frame.kv_set("header", "OBJECT", "TestField")
    frame.kv_set("header", "SOURCE_PATH", "test_light.fits")
    # DrizzleMeta 元数据 (filter/exposure_s/obs_time + fits_meta)
    frame.kv_set("header", "FILTER", "Lum")
    frame.kv_set("header", "EXPTIME", "600.0")
    frame.kv_set("header", "DATE-OBS", "2025-05-03T03:15:25")
    frame.kv_set("header", "OBJCTRA", "12 00 00")
    frame.kv_set("header", "OBJCTDEC", "+30 00 00")
    frame.kv_set("header", "IMAGETYP", "LIGHT")
    frame.kv_set("header", "SITELAT", "40.0")
    frame.kv_set("header", "SITELONG", "116.0")

    if with_sip:
        # SIP 阶数=2, 添加几个非零系数
        frame.kv_set("header", "A_ORDER", "2")
        frame.kv_set("header", "A_0_2", "1e-5")
        frame.kv_set("header", "A_1_1", "2e-5")
        frame.kv_set("header", "A_2_0", "3e-5")
        frame.kv_set("header", "B_ORDER", "2")
        frame.kv_set("header", "B_0_2", "-1e-5")
        frame.kv_set("header", "B_1_1", "-2e-5")
        frame.kv_set("header", "B_2_0", "-3e-5")

    return frame


# ============================================================================
# 测试 1: 正常流程 - data + WCS → .hiss 文件生成 + hiss_read 验证
# ============================================================================
def test_drizzle_handler_produces_hiss(tmp_path):
    """验证 handler 能从 PipelineFrame 生成 .hiss 文件, 且 hiss_read 可读取

    流程:
        1. 构造 frame (data[64,64] + header WCS + FILTER/EXPTIME/DATE-OBS)
        2. 注册 Drizzle handler (nside=4096, pixfrac=0.8)
        3. engine.run_single(STAGE_DRIZZLE, STAGE_DRIZZLE)
        4. 验证 .hiss 文件存在且非空
        5. 用 hiss_read 读取 .hiss, 验证 nside/nested/n_pix/meta 正确
    """
    frame = _make_test_frame(width=64, height=64, value=1.0)

    engine = PipelineEngine()
    params = DrizzleParams(
        nside=4096,
        nested=True,
        pixfrac=0.8,
        output_dir=str(tmp_path),
    )
    register_drizzle_handler(engine, params)

    ret = engine.run_single(frame, STAGE_DRIZZLE, STAGE_DRIZZLE)
    assert ret == 0, f"run_single 应返回 0, 实际 {ret}"

    # 验证 .hiss 文件生成 (SOURCE_PATH=test_light.fits → test_light.hiss)
    expected_path = os.path.join(str(tmp_path), "test_light.hiss")
    assert os.path.exists(expected_path), f"输出文件未生成: {expected_path}"
    assert os.path.getsize(expected_path) > 0, "输出文件为空"
    print(f"[ok] 生成: {expected_path} ({os.path.getsize(expected_path)} bytes)")

    # 用 hiss_read 读取验证
    nside, nested, ipix, pixel, meta = hiss_read(expected_path)
    n_pix = len(ipix)
    assert nside == 4096, f"nside 应为 4096, 实际 {nside}"
    assert nested is True, f"nested 应为 True, 实际 {nested}"
    assert n_pix > 0, f"n_pix 应 > 0, 实际 {n_pix}"
    assert len(pixel) == n_pix, f"pixel 长度 {len(pixel)} != n_pix {n_pix}"
    # 验证 meta 包含 filter/exposure_s/obs_time/pixfrac
    assert "filter" in meta, f"meta 缺少 filter 字段: {list(meta.keys())}"
    assert meta["filter"] == "Lum", f"filter 应为 Lum, 实际 {meta['filter']}"
    assert "exposure_s" in meta, f"meta 缺少 exposure_s 字段"
    assert "obs_time" in meta, f"meta 缺少 obs_time 字段"
    assert "pixfrac" in meta, f"meta 缺少 pixfrac 字段"
    assert "fits_meta" in meta, f"meta 缺少 fits_meta 字段"
    assert meta["fits_meta"].get("IMAGETYP") == "LIGHT", f"fits_meta.IMAGETYP 错误: {meta['fits_meta']}"
    print(f"[ok] hiss_read 验证通过: nside={nside}, n_pix={n_pix}, filter={meta['filter']}")


# ============================================================================
# 测试 2: 错误处理 - 缺少 data 块
# ============================================================================
def test_drizzle_handler_no_data_block(tmp_path):
    """验证缺少 data 块时 handler 返回 -1, run_single 抛 RuntimeError

    流程:
        1. 构造 frame (只有 header KV, 无 data 块)
        2. 注册 Drizzle handler
        3. run_single 应抛 RuntimeError (handler 返回 -1)
    """
    frame = PipelineFramePy()
    frame.kv_set("header", "CTYPE1", "RA---TAN")  # 只有 header, 无 data

    engine = PipelineEngine()
    params = DrizzleParams(nside=4096, output_dir=str(tmp_path))
    register_drizzle_handler(engine, params)

    with pytest.raises(RuntimeError, match="管线执行失败"):
        engine.run_single(frame, STAGE_DRIZZLE, STAGE_DRIZZLE)
    print("[ok] 缺少 data 块时正确报错")


# ============================================================================
# 测试 3: SIP 系数 - header 含 A_ORDER/B_ORDER
# ============================================================================
def test_drizzle_handler_with_sip(tmp_path):
    """验证含 SIP 系数的 header 能正确完成 Drizzle 并生成 .hiss

    流程:
        1. 构造 frame (data + WCS + SIP A_ORDER=2/B_ORDER=2)
        2. 注册 Drizzle handler
        3. run_single 应成功
        4. 验证 .hiss 文件生成
    """
    frame = _make_test_frame(width=64, height=64, value=5.0, with_sip=True)

    engine = PipelineEngine()
    params = DrizzleParams(
        nside=4096,
        nested=True,
        pixfrac=0.8,
        output_dir=str(tmp_path),
    )
    register_drizzle_handler(engine, params)

    ret = engine.run_single(frame, STAGE_DRIZZLE, STAGE_DRIZZLE)
    assert ret == 0, f"SIP Drizzle 应成功, 实际 ret={ret}"

    expected_path = os.path.join(str(tmp_path), "test_light.hiss")
    assert os.path.exists(expected_path), "SIP Drizzle 后应生成 .hiss 文件"
    assert os.path.getsize(expected_path) > 0
    print(f"[ok] SIP Drizzle 完成: {expected_path} "
          f"({os.path.getsize(expected_path)} bytes)")
