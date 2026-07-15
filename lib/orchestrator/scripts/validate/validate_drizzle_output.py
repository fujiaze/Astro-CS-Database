# -*- coding: utf-8 -*-
"""
validate_drizzle_output.py - Drizzle 输出验证脚本
功能: 验证 .ahpx 输出正确性
用途: 全链路调试阶段验证 drizzle 到 HEALPix 坐标系的结果
调用: python validate_drizzle_output.py <ahpx_file> [--input-fits <input_fits>] [--output <output_json>]

Drizzle 输出 .ahpx 元数据结构 (由 drizzle_engine.cpp 写入):
    {
        "image": {"width": n, "height": 1, "channels": 1},   # n=HEALPix 像素数
        "wcs": {
            "cd": [cd0, cd1, cd2, cd3],
            "crval": [crval0, crval1],
            "crpix": [crpix0, crpix1],
            "sip_order": N
        },
        "healpix": {
            "nside": N,
            "nested": true/false,
            "pixfrac": 0.8,
            "n_pixels": N
        },
        "ipix": [ipix1, ipix2, ...],       # HEALPix 像素索引数组
        "source": {"fits_path": "...", "n_source_pixels": N},
        "drizzle": {"n_healpix_pixels": N, "elapsed_sec": X.XXXX}
    }

像素数据存储为 (H=1, W=n, C=1) 的 float32 数组 (1D HEALPix 像素值序列)。

验证项:
    1. 文件存在且大小 > 0
    2. ahpx_io 可读取 (DLL 可用且文件格式正确)
    3. n_healpix_pixels > 0
    4. 像素值非全0
    5. 像素值无 NaN/Inf
    6. (可选) 通量守恒: |sum_out - sum_in * pixfrac^2| / sum_in < 10%
    7. 元数据含 WCS 字段 (cd/crval/crpix)
    8. HEALPix nside 参数合理 (64-32768 且为 2 的幂)

输出: JSON 到 output/pipeline_debug/<frame_name>/validate_drizzle.json
退出码: 0=全部通过, 1=存在失败项, 2=输入/运行时错误
"""

from __future__ import annotations

import os
import sys
import json
import logging
import argparse
import datetime
from typing import Optional

import numpy as np


# ============================ 路径设置 ============================
# 把 lib/healpix_db/ 加入 sys.path 以导入 ahpx_io 模块
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_PROJECT_ROOT = os.path.normpath(os.path.join(_THIS_DIR, "..", "..", "..", ".."))
_HEALPIX_DB_DIR = os.path.normpath(os.path.join(_PROJECT_ROOT, "lib", "healpix_db"))
if _HEALPIX_DB_DIR not in sys.path:
    sys.path.insert(0, _HEALPIX_DB_DIR)


# ============================ 日志配置 ============================
_LOG_DIR = os.path.join(_THIS_DIR, "..", "..", "logs", "validate")
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


def _init_logger() -> logging.Logger:
    """初始化模块日志，同时输出到文件（UTF-8）和控制台"""
    os.makedirs(_LOG_DIR, exist_ok=True)
    log_file = os.path.join(
        _LOG_DIR,
        "validate_drizzle_output_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".log",
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    lg = logging.getLogger("validate_drizzle_output")
    lg.setLevel(logging.DEBUG)
    lg.propagate = False
    if not lg.handlers:
        fh = logging.FileHandler(log_file, encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(formatter)
        lg.addHandler(fh)

        ch = logging.StreamHandler(sys.stdout)
        ch.setLevel(logging.INFO)
        ch.setFormatter(formatter)
        lg.addHandler(ch)

    lg.info("日志系统初始化完成，日志文件: %s", log_file)
    return lg


logger = _init_logger()


# ============================ ahpx_io 模块加载 ============================
AHPX_IO_AVAILABLE = False
AHPX_IO_ERROR = ""
AhpxReader = None
is_ahpx = None


def _load_ahpx_io_module():
    """加载 ahpx_io 模块

    加载策略 (按顺序尝试):
        1. 标准 import (要求 ahpx_io.py 存在或已安装)
        2. 从 __pycache__/ahpx_io.cpython-{ver}.pyc 加载 (源文件被删除时)
        3. 失败则返回 None (触发降级模式)

    返回:
        module 或 None
    """
    # 策略1: 标准 import
    try:
        import ahpx_io as _mod
        if hasattr(_mod, "AhpxReader"):
            logger.info("ahpx_io 标准导入成功 (路径: %s)", _HEALPIX_DB_DIR)
            return _mod
        logger.warning("ahpx_io 标准导入成功但缺少 AhpxReader 属性")
    except Exception as e:
        logger.debug("ahpx_io 标准导入失败: %s: %s", type(e).__name__, e)

    # 策略2: 从 __pycache__ 中的 .pyc 加载
    import importlib.util
    ahpx_dir = os.path.join(_HEALPIX_DB_DIR, "ahpx_io")
    pycache_dir = os.path.join(ahpx_dir, "__pycache__")
    py_version = f"cpython-{sys.version_info.major}{sys.version_info.minor}"
    pyc_candidates = [
        os.path.join(pycache_dir, f"ahpx_io.{py_version}.pyc"),
        os.path.join(ahpx_dir, "ahpx_io.pyc"),
    ]
    for pyc_path in pyc_candidates:
        if os.path.isfile(pyc_path):
            try:
                spec = importlib.util.spec_from_file_location("ahpx_io", pyc_path)
                if spec is None or spec.loader is None:
                    continue
                mod = importlib.util.module_from_spec(spec)
                # 加载前确保 ahpx_io.dll 所在目录可被找到
                try:
                    if os.path.isdir(ahpx_dir):
                        os.add_dll_directory(ahpx_dir)
                except (OSError, AttributeError):
                    pass
                spec.loader.exec_module(mod)
                if hasattr(mod, "AhpxReader"):
                    logger.info("ahpx_io 从 .pyc 加载成功: %s", pyc_path)
                    return mod
                logger.warning("ahpx_io .pyc 加载成功但缺少 AhpxReader: %s", pyc_path)
            except Exception as e:
                logger.debug("ahpx_io .pyc 加载失败 (%s): %s: %s",
                             pyc_path, type(e).__name__, e)

    return None


_ahpx_io_mod = _load_ahpx_io_module()
if _ahpx_io_mod is not None:
    AhpxReader = _ahpx_io_mod.AhpxReader
    is_ahpx = getattr(_ahpx_io_mod, "is_ahpx", None)
    AHPX_IO_AVAILABLE = True
    logger.info("ahpx_io 模块可用")
else:
    AHPX_IO_ERROR = "标准导入和 .pyc 后备加载均失败"
    logger.warning("ahpx_io 模块不可用: %s (将降级为文件存在性验证)", AHPX_IO_ERROR)


# ============================ 核心验证逻辑 ============================

def _parse_metadata(header_json: str) -> dict:
    """解析 .ahpx 元数据 JSON 字符串

    参数:
        header_json: AhpxReader.header_json 返回的 JSON 字符串

    返回:
        dict: 解析后的元数据字典 (解析失败时返回 {"raw": header_json})
    """
    try:
        return json.loads(header_json)
    except (json.JSONDecodeError, TypeError) as e:
        logger.warning("元数据 JSON 解析失败: %s", e)
        return {"raw": header_json}


def _read_input_fits_pixels(fits_path: str) -> Optional[np.ndarray]:
    """读取输入 FITS 文件的像素数据

    优先使用 astropy.io.fits；不可用时返回 None。

    参数:
        fits_path: FITS 文件路径

    返回:
        np.ndarray 或 None: 像素数据 (失败时返回 None)
    """
    if not os.path.isfile(fits_path):
        logger.error("输入 FITS 文件不存在: %s", fits_path)
        return None

    try:
        from astropy.io import fits as astropy_fits
    except ImportError:
        logger.warning("astropy 不可用，无法读取输入 FITS 进行通量守恒验证")
        return None

    try:
        with astropy_fits.open(fits_path, memmap=False) as hdul:
            # 取第一个有数据的 HDU
            for hdu in hdul:
                if hdu.data is not None and hasattr(hdu.data, "sum"):
                    arr = np.asarray(hdu.data, dtype=np.float64)
                    logger.info("读取输入 FITS: %s, shape=%s, dtype=%s",
                                fits_path, arr.shape, arr.dtype)
                    return arr
        logger.warning("FITS 文件中未找到像素数据: %s", fits_path)
        return None
    except Exception as e:
        logger.error("读取 FITS 文件失败: %s: %s", fits_path, e, exc_info=True)
        return None


def _is_power_of_two(n: int) -> bool:
    """判断整数 n 是否为 2 的幂"""
    return n > 0 and (n & (n - 1)) == 0


def validate_drizzle(ahpx_file: str,
                     input_fits: Optional[str] = None,
                     default_pixfrac: float = 0.5) -> dict:
    """执行 Drizzle 输出验证

    参数:
        ahpx_file:      .ahpx 文件路径
        input_fits:     可选输入 FITS 文件路径 (用于通量守恒验证)
        default_pixfrac: 元数据中未找到 pixfrac 时的默认值

    返回:
        dict: 验证结果（与输出 JSON 结构一致）
    """
    checks = []
    statistics: dict = {}

    logger.info("=" * 60)
    logger.info("开始 Drizzle 输出验证: %s", ahpx_file)
    logger.info("=" * 60)

    ahpx_file_norm = os.path.normpath(ahpx_file)

    # ==================== 验证项 1: 文件存在且大小 > 0 ====================
    file_exists = os.path.isfile(ahpx_file_norm)
    file_size = os.path.getsize(ahpx_file_norm) if file_exists else 0
    check_file_exists = file_exists and file_size > 0
    checks.append({
        "name": "file_exists",
        "pass": bool(check_file_exists),
        "detail": f"file size={file_size} bytes" if file_exists else "文件不存在",
    })
    logger.info("[检查1] file_exists: %s (size=%d bytes)",
                "PASS" if check_file_exists else "FAIL", file_size)

    if not check_file_exists:
        # 文件不存在则后续检查无意义
        logger.error("文件不存在或大小为0，跳过后续检查")
        overall_pass = False
        return {
            "ahpx_file": ahpx_file_norm,
            "overall_pass": bool(overall_pass),
            "checks": checks,
            "statistics": statistics,
            "degraded": not AHPX_IO_AVAILABLE,
        }

    # ==================== 降级模式: ahpx_io 不可用 ====================
    if not AHPX_IO_AVAILABLE:
        logger.warning("ahpx_io 模块不可用，降级为文件存在性验证")
        checks.append({
            "name": "ahpx_readable",
            "pass": False,
            "detail": f"无法读取 ahpx 内容 (ahpx_io 不可用: {AHPX_IO_ERROR})",
        })
        checks.append({
            "name": "n_healpix_pixels",
            "pass": False,
            "detail": "无法读取 (ahpx_io 不可用)",
        })
        checks.append({
            "name": "pixels_nonzero",
            "pass": False,
            "detail": "无法读取 (ahpx_io 不可用)",
        })
        checks.append({
            "name": "no_nan_inf",
            "pass": False,
            "detail": "无法读取 (ahpx_io 不可用)",
        })
        if input_fits:
            checks.append({
                "name": "flux_conservation",
                "pass": False,
                "detail": "无法读取 (ahpx_io 不可用)",
            })
        checks.append({
            "name": "wcs_metadata",
            "pass": False,
            "detail": "无法读取 (ahpx_io 不可用)",
        })
        checks.append({
            "name": "nside",
            "pass": False,
            "detail": "无法读取 (ahpx_io 不可用)",
        })
        overall_pass = False
        logger.info("=" * 60)
        logger.info("验证汇总 (降级模式): overall_pass=%s", overall_pass)
        logger.info("=" * 60)
        return {
            "ahpx_file": ahpx_file_norm,
            "overall_pass": bool(overall_pass),
            "checks": checks,
            "statistics": statistics,
            "degraded": True,
            "note": "无法读取 ahpx 内容，仅验证文件存在",
        }

    # ==================== 验证项 2: ahpx 可读取 ====================
    reader = None
    pixels = None
    metadata = None
    read_ok = False
    read_error = ""
    try:
        reader = AhpxReader(ahpx_file_norm)
        pixels = reader.read_pixels()
        header_json = reader.header_json
        metadata = _parse_metadata(header_json)
        read_ok = True
        logger.info("  ahpx 读取成功: pixels shape=%s, dtype=%s",
                    pixels.shape if hasattr(pixels, "shape") else "?",
                    pixels.dtype if hasattr(pixels, "dtype") else "?")
    except Exception as e:
        read_error = f"{type(e).__name__}: {e}"
        logger.error("  ahpx 读取失败: %s", read_error, exc_info=True)
    finally:
        if reader is not None:
            try:
                reader.close()
            except Exception:
                pass

    checks.append({
        "name": "ahpx_readable",
        "pass": bool(read_ok),
        "detail": "read OK" if read_ok else f"读取失败: {read_error}",
    })
    logger.info("[检查2] ahpx_readable: %s",
                "PASS" if read_ok else "FAIL")

    if not read_ok:
        # 读取失败则后续检查无意义
        logger.error("ahpx 读取失败，跳过后续检查")
        overall_pass = False
        return {
            "ahpx_file": ahpx_file_norm,
            "overall_pass": bool(overall_pass),
            "checks": checks,
            "statistics": statistics,
            "degraded": False,
        }

    # 提取像素 1D 数组 (drizzle 输出存储为 (1, n, 1))
    pixels_arr = np.asarray(pixels, dtype=np.float64)
    pixels_flat = pixels_arr.ravel()
    n_pixels = int(pixels_flat.size)

    # 从元数据提取关键字段
    healpix_meta = metadata.get("healpix", {}) if isinstance(metadata, dict) else {}
    nside = healpix_meta.get("nside", 0)
    nested = healpix_meta.get("nested", True)
    pixfrac = healpix_meta.get("pixfrac", default_pixfrac)
    n_healpix_pixels_meta = healpix_meta.get("n_pixels", 0)

    drizzle_meta = metadata.get("drizzle", {}) if isinstance(metadata, dict) else {}
    n_healpix_pixels_drizzle = drizzle_meta.get("n_healpix_pixels", 0)

    # 取元数据与像素数组中较大的 n_healpix_pixels
    n_healpix_pixels = max(n_pixels, int(n_healpix_pixels_meta or 0),
                           int(n_healpix_pixels_drizzle or 0))

    statistics["n_healpix_pixels"] = n_healpix_pixels
    statistics["nside"] = int(nside) if nside else 0
    statistics["pixfrac"] = float(pixfrac) if pixfrac is not None else default_pixfrac
    statistics["nested"] = bool(nested)
    statistics["sum_pixels"] = float(np.sum(pixels_flat))

    logger.info("元数据: nside=%s, nested=%s, pixfrac=%s, n_pixels=%s, "
                "drizzle.n_healpix_pixels=%s",
                nside, nested, pixfrac, n_healpix_pixels_meta,
                n_healpix_pixels_drizzle)

    # ==================== 验证项 3: n_healpix_pixels > 0 ====================
    check_n_pixels = n_healpix_pixels > 0
    checks.append({
        "name": "n_healpix_pixels",
        "pass": bool(check_n_pixels),
        "detail": f"n={n_healpix_pixels} > 0" if check_n_pixels
                  else f"n={n_healpix_pixels} (<=0)",
    })
    logger.info("[检查3] n_healpix_pixels: %s (n=%d)",
                "PASS" if check_n_pixels else "FAIL", n_healpix_pixels)

    # ==================== 验证项 4: 像素值非全0 ====================
    n_nonzero = int(np.count_nonzero(pixels_flat))
    check_nonzero = n_nonzero > 0
    checks.append({
        "name": "pixels_nonzero",
        "pass": bool(check_nonzero),
        "detail": f"non-zero pixels={n_nonzero}" if check_nonzero
                  else "所有像素均为0",
    })
    logger.info("[检查4] pixels_nonzero: %s (non-zero=%d / %d)",
                "PASS" if check_nonzero else "FAIL", n_nonzero, n_pixels)

    # ==================== 验证项 5: 像素值无 NaN/Inf ====================
    finite_mask = np.isfinite(pixels_flat)
    n_non_finite = int(np.sum(~finite_mask))
    check_no_nan_inf = n_non_finite == 0
    n_nan = int(np.sum(np.isnan(pixels_flat)))
    n_inf = int(np.sum(np.isinf(pixels_flat)))
    checks.append({
        "name": "no_nan_inf",
        "pass": bool(check_no_nan_inf),
        "detail": "no NaN/Inf" if check_no_nan_inf
                  else f"NaN={n_nan}, Inf={n_inf}",
    })
    logger.info("[检查5] no_nan_inf: %s (NaN=%d, Inf=%d)",
                "PASS" if check_no_nan_inf else "FAIL", n_nan, n_inf)

    # ==================== 验证项 6: (可选) 通量守恒 ====================
    if input_fits:
        input_pixels = _read_input_fits_pixels(input_fits)
        if input_pixels is None:
            checks.append({
                "name": "flux_conservation",
                "pass": False,
                "detail": f"无法读取输入 FITS: {input_fits}",
            })
            logger.error("[检查6] flux_conservation: FAIL (无法读取输入 FITS)")
        else:
            sum_input = float(np.sum(input_pixels))
            sum_output = float(np.sum(pixels_flat))
            pixfrac_val = float(pixfrac) if pixfrac is not None else default_pixfrac
            expected = sum_input * (pixfrac_val ** 2)

            if abs(sum_input) < 1e-30:
                # 输入和接近0时无法计算相对误差
                check_flux = abs(sum_output - expected) < 1e-10
                detail = (f"sum_out={sum_output:.4e}, sum_in={sum_input:.4e} "
                          f"(输入和≈0, 直接比较绝对差)")
            else:
                rel_err = abs(sum_output - expected) / abs(sum_input)
                check_flux = rel_err < 0.1
                detail = (f"sum_out={sum_output:.4e}, sum_in={sum_input:.4e}, "
                          f"pixfrac={pixfrac_val:.4f}, expected={expected:.4e}, "
                          f"error={rel_err*100:.2f}% {'<' if check_flux else '>='} 10%")

            checks.append({
                "name": "flux_conservation",
                "pass": bool(check_flux),
                "detail": detail,
            })
            logger.info("[检查6] flux_conservation: %s (%s)",
                        "PASS" if check_flux else "FAIL", detail)

            statistics["sum_input"] = sum_input
            statistics["expected_output"] = expected
    else:
        logger.info("[检查6] flux_conservation: 跳过 (未提供 --input-fits)")

    # ==================== 验证项 7: 元数据含 WCS 字段 ====================
    wcs_meta = metadata.get("wcs", {}) if isinstance(metadata, dict) else {}
    has_cd = "cd" in wcs_meta
    has_crval = "crval" in wcs_meta
    has_crpix = "crpix" in wcs_meta

    wcs_detail_parts = []
    if has_cd:
        cd = wcs_meta.get("cd", [])
        if isinstance(cd, list) and len(cd) >= 4:
            wcs_detail_parts.append(f"cd=[{cd[0]:.4e},{cd[1]:.4e},{cd[2]:.4e},{cd[3]:.4e}]")
        else:
            wcs_detail_parts.append(f"cd={cd}")
    if has_crval:
        crval = wcs_meta.get("crval", [])
        if isinstance(crval, list) and len(crval) >= 2:
            wcs_detail_parts.append(f"crval=[{crval[0]:.4f},{crval[1]:.4f}]")
        else:
            wcs_detail_parts.append(f"crval={crval}")
    if has_crpix:
        crpix = wcs_meta.get("crpix", [])
        if isinstance(crpix, list) and len(crpix) >= 2:
            wcs_detail_parts.append(f"crpix=[{crpix[0]:.4f},{crpix[1]:.4f}]")
        else:
            wcs_detail_parts.append(f"crpix={crpix}")

    # 至少要有 cd 和 crval 才算 WCS 完整
    check_wcs = has_cd and has_crval
    wcs_detail = ", ".join(wcs_detail_parts) if wcs_detail_parts else "WCS 字段缺失"
    checks.append({
        "name": "wcs_metadata",
        "pass": bool(check_wcs),
        "detail": wcs_detail,
    })
    logger.info("[检查7] wcs_metadata: %s (%s)",
                "PASS" if check_wcs else "FAIL", wcs_detail)

    # ==================== 验证项 8: HEALPix nside 参数合理 ====================
    # 范围: 64-32768 且为 2 的幂 (覆盖 drizzle 默认 nside=32768)
    nside_val = int(nside) if nside else 0
    nside_min, nside_max = 64, 32768
    check_nside_range = nside_min <= nside_val <= nside_max
    check_nside_pow2 = _is_power_of_two(nside_val)
    check_nside = check_nside_range and check_nside_pow2

    if check_nside:
        detail = f"nside={nside_val}"
    else:
        reasons = []
        if not check_nside_range:
            reasons.append(f"超出范围 [{nside_min},{nside_max}]")
        if not check_nside_pow2:
            reasons.append("非 2 的幂")
        detail = f"nside={nside_val} ({'; '.join(reasons)})"

    checks.append({
        "name": "nside",
        "pass": bool(check_nside),
        "detail": detail,
    })
    logger.info("[检查8] nside: %s (%s)",
                "PASS" if check_nside else "FAIL", detail)

    # ==================== 汇总 ====================
    overall_pass = all(c["pass"] for c in checks)
    logger.info("=" * 60)
    logger.info("验证汇总: overall_pass=%s", overall_pass)
    for c in checks:
        logger.info("  [%s] %s: %s",
                    "PASS" if c["pass"] else "FAIL", c["name"], c["detail"])
    logger.info("=" * 60)

    return {
        "ahpx_file": ahpx_file_norm,
        "overall_pass": bool(overall_pass),
        "checks": checks,
        "statistics": statistics,
        "degraded": False,
    }


# ============================ 输出路径推导 ============================

def derive_output_path(ahpx_file: str, output_arg: Optional[str] = None) -> str:
    """推导输出 JSON 路径

    规则:
        - 若 output_arg 指定，使用 output_arg
        - 否则根据 ahpx_file 文件名推导 frame_name，输出到
          output/pipeline_debug/<frame_name>/validate_drizzle.json

    frame_name 推导:
        从 ahpx_file 文件名中去除扩展名。
        例如: "NGC4945.ahpx" -> frame_name="NGC4945"
    """
    if output_arg:
        return output_arg

    ahpx_file = os.path.normpath(ahpx_file)
    fname = os.path.basename(ahpx_file)
    name_no_ext = os.path.splitext(fname)[0]

    frame_name = name_no_ext if name_no_ext else "unknown"

    output_path = os.path.join(
        _PROJECT_ROOT, "output", "pipeline_debug", frame_name, "validate_drizzle.json"
    )
    return output_path


# ============================ 主入口 ============================

def main(argv: Optional[list] = None) -> int:
    """主入口

    返回:
        0 = 全部验证通过
        1 = 存在验证失败项
        2 = 输入/运行时错误
    """
    parser = argparse.ArgumentParser(
        description="Drizzle 输出 (.ahpx) 验证脚本"
    )
    parser.add_argument(
        "ahpx_file",
        help=".ahpx 文件路径 (Drizzle 输出)"
    )
    parser.add_argument(
        "--input-fits", "-i",
        default=None,
        help="可选: 输入 FITS 文件路径 (用于通量守恒验证)"
    )
    parser.add_argument(
        "--output", "-o",
        default=None,
        help="输出 JSON 路径 (默认: output/pipeline_debug/<frame_name>/validate_drizzle.json)"
    )
    parser.add_argument(
        "--pixfrac",
        type=float,
        default=0.5,
        help="元数据中未找到 pixfrac 时的默认值 (默认: 0.5)"
    )
    args = parser.parse_args(argv)

    try:
        # 执行验证
        result = validate_drizzle(
            ahpx_file=args.ahpx_file,
            input_fits=args.input_fits,
            default_pixfrac=args.pixfrac,
        )

        # 推导输出路径并写入
        output_path = derive_output_path(args.ahpx_file, args.output)
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(result, f, ensure_ascii=False, indent=4)
        logger.info("验证结果已写入: %s", output_path)

        # 打印关键结果到 stdout
        print_summary = {
            "output_path": output_path,
            "ahpx_file": result["ahpx_file"],
            "overall_pass": result["overall_pass"],
            "degraded": result.get("degraded", False),
            "n_checks": len(result["checks"]),
            "n_passed": sum(1 for c in result["checks"] if c["pass"]),
            "n_failed": sum(1 for c in result["checks"] if not c["pass"]),
        }
        if result.get("statistics"):
            print_summary["statistics"] = {
                k: result["statistics"].get(k)
                for k in ("n_healpix_pixels", "nside", "pixfrac", "sum_pixels")
                if k in result["statistics"]
            }
        print(json.dumps(print_summary, ensure_ascii=False, indent=2))

        return 0 if result["overall_pass"] else 1

    except Exception as e:
        logger.error("运行时错误: %s", e, exc_info=True)
        return 2


if __name__ == "__main__":
    sys.exit(main())
