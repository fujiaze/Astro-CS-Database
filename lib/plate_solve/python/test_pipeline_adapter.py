# -*- coding: utf-8 -*-
"""
plate_solve pipeline_adapter 测试脚本
功能: 构造含 data块 + header KV块的 PipelineFrame, 调用 plate_solve handler, 验证 WCS 注入
用途: 验证命名块容器版 pipeline_adapter 的正确性

前置条件:
  - astro_image_io.dll 已编译
  - ipv_solver.dll 已编译
  - star_detector.dll 已编译
  - gaia_client.dll 已编译
  - GaiaDR3SP 数据库存在
  - testdata 中有 FITS 测试图像

运行:
  cd lib\plate_solve\python
  python test_pipeline_adapter.py --fits <path\to\image.fits>
  python test_pipeline_adapter.py  # 使用默认测试图像
"""

from __future__ import annotations

import os
import sys

# ============================ 环境初始化 ============================

PROJECT_ROOT = r"F:\Astro dev\Astro CS Normalization Database"
MINGW_BIN = r"C:\msys64\mingw64\bin"

if MINGW_BIN not in os.environ.get("PATH", ""):
    os.environ["PATH"] = MINGW_BIN + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(MINGW_BIN)
    except (OSError, FileNotFoundError):
        pass

_ASTRO_IO_DIR = os.path.join(PROJECT_ROOT, "lib", "astro_image_io")
if _ASTRO_IO_DIR not in os.environ.get("PATH", ""):
    os.environ["PATH"] = _ASTRO_IO_DIR + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(_ASTRO_IO_DIR)
    except (OSError, FileNotFoundError):
        pass

sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "plate_solve", "python"))
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python"))
sys.path.insert(0, os.path.join(PROJECT_ROOT, "lib", "star_detector", "python"))
sys.path.insert(
    0,
    os.path.join(PROJECT_ROOT, "lib", "plate_solve", "archive", "vector_method", "python", "python"),
)

import argparse
import numpy as np

from astro_image_io import (
    PipelineFramePy, PipelineEngine,
    STAGE_PLATESOLVE,
)
from astro_image_io import ImageReader

from pipeline_adapter import register_platesolve_handler, PlateSolveParams


# ============================ 默认测试图像 ============================

DEFAULT_FITS = os.path.join(
    PROJECT_ROOT,
    "testdata", "Galaxy_Center_T4", "lights", "panel1",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts",
)


# ============================ 辅助函数 ============================

def build_frame_from_fits(fits_path: str) -> PipelineFramePy:
    """从 FITS 文件构造 PipelineFrame

    读取 FITS 图像, 创建 "data" 块 (FLOAT32[H,W]) 和 "header" KV 块
    (含 OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ)
    """
    print("=" * 60)
    print("读取 FITS 文件: %s" % fits_path)

    reader = ImageReader()
    img = reader.read(fits_path)

    pixels = np.ascontiguousarray(img.data, dtype=np.float32)
    h, w = pixels.shape
    print("  图像尺寸: %dx%d" % (w, h))

    # 创建 PipelineFrame
    frame = PipelineFramePy()

    # 添加 "data" 块
    frame.add_block("data", pixels, description="图像像素数据")

    # 添加 "header" KV 块 (从 FITS 头读取关键参数)
    objctra = img.get_keyword("OBJCTRA", "")
    objctdec = img.get_keyword("OBJCTDEC", "")
    focallen = img.get_keyword_float("FOCALLEN", 0.0)
    xpixsz = img.get_keyword_float("XPIXSZ", 0.0)

    print("  OBJCTRA=%s, OBJCTDEC=%s" % (objctra, objctdec))
    print("  FOCALLEN=%s, XPIXSZ=%s" % (focallen, xpixsz))

    # 写入 header KV 块
    frame.kv_set("header", "OBJCTRA", str(objctra))
    frame.kv_set("header", "OBJCTDEC", str(objctdec))
    frame.kv_set_double("header", "FOCALLEN", focallen)
    frame.kv_set_double("header", "XPIXSZ", xpixsz)

    # 额外的元数据
    exptime = img.get_keyword_float("EXPTIME", 0.0)
    filter_name = img.get_keyword("FILTER", "Unknown")
    frame.kv_set("header", "EXPTIME", str(exptime))
    frame.kv_set("header", "FILTER", str(filter_name))

    img.close()

    print("  PipelineFrame 构造完成: blocks=%s" % frame.list_blocks())
    print("=" * 60)
    return frame


def verify_wcs_in_header(frame: PipelineFramePy) -> bool:
    """验证 header KV 块中是否包含 WCS 字段

    检查: CTYPE1, CTYPE2, CRVAL1, CRVAL2, CRPIX1, CRPIX2,
          CD1_1, CD1_2, CD2_1, CD2_2
    """
    print("=" * 60)
    print("验证 WCS 字段注入")
    all_ok = True

    wcs_keys = [
        ("CTYPE1", str),
        ("CTYPE2", str),
        ("CRVAL1", float),
        ("CRVAL2", float),
        ("CRPIX1", float),
        ("CRPIX2", float),
        ("CD1_1", float),
        ("CD1_2", float),
        ("CD2_1", float),
        ("CD2_2", float),
    ]

    for key, dtype in wcs_keys:
        if dtype is str:
            val = frame.kv_get("header", key)
            if val:
                print("  [OK] %s = %s" % (key, val))
            else:
                print("  [FAIL] %s 缺失" % key)
                all_ok = False
        else:
            val = frame.kv_get_double("header", key, -999.0)
            if val != -999.0:
                print("  [OK] %s = %.10e" % (key, val))
            else:
                print("  [FAIL] %s 缺失" % key)
                all_ok = False

    # 检查 SIP (可选)
    sip_order_str = frame.kv_get("header", "A_ORDER")
    if sip_order_str:
        sip_order = int(sip_order_str)
        print("  [INFO] SIP A_ORDER = %d" % sip_order)
        # 检查 A_0_0
        a00 = frame.kv_get_double("header", "A_0_0", -999.0)
        if a00 != -999.0:
            print("  [OK] A_0_0 = %.10e" % a00)
        else:
            print("  [FAIL] A_0_0 缺失" )
            all_ok = False

    # 检查 star_det 块
    if frame.has_block("star_det"):
        star_det = frame.get_block_data("star_det")
        if star_det is not None:
            print("  [OK] star_det 块: shape=%s, dtype=%s" % (star_det.shape, star_det.dtype))
        else:
            print("  [WARN] star_det 块存在但数据为空")
    else:
        print("  [INFO] star_det 块不存在")

    # 检查 gaia_cat 块
    if frame.has_block("gaia_cat"):
        gaia_cat = frame.get_block_data("gaia_cat")
        if gaia_cat is not None:
            print("  [OK] gaia_cat 块: shape=%s, dtype=%s" % (gaia_cat.shape, gaia_cat.dtype))
        else:
            print("  [WARN] gaia_cat 块存在但数据为空")
    else:
        print("  [INFO] gaia_cat 块不存在")

    print("=" * 60)
    return all_ok


# ============================ 主函数 ============================

def main():
    parser = argparse.ArgumentParser(
        description="测试 plate_solve pipeline_adapter (命名块容器版)"
    )
    parser.add_argument("--fits", default=DEFAULT_FITS,
                        help="FITS 图像路径 (默认: %s)" % DEFAULT_FITS)
    args = parser.parse_args()

    fits_path = args.fits
    if not os.path.isfile(fits_path):
        print("[FAIL] FITS 文件不存在: %s" % fits_path)
        return 1

    try:
        # 1. 构造 PipelineFrame
        frame = build_frame_from_fits(fits_path)

        # 2. 创建引擎并注册 handler
        print("创建 PipelineEngine 并注册 plate_solve handler ...")
        engine = PipelineEngine()
        params = PlateSolveParams()
        register_platesolve_handler(engine, params)
        print("handler 注册完成")

        # 3. 执行管线 (仅 PLATESOLVE 阶段)
        print("执行 STAGE_PLATESOLVE ...")
        engine.run_single(frame, STAGE_PLATESOLVE, STAGE_PLATESOLVE)
        print("管线执行完成")

        # 4. 验证结果
        ok = verify_wcs_in_header(frame)

        # 5. 列出最终块
        print("最终 frame 块列表: %s" % frame.list_blocks())

        # 清理
        engine.close()
        frame.close()

        if ok:
            print("\n[SUCCESS] 所有 WCS 字段验证通过")
            return 0
        else:
            print("\n[FAIL] 部分 WCS 字段缺失")
            return 1

    except Exception as e:
        print("[ERROR] %s" % e)
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
