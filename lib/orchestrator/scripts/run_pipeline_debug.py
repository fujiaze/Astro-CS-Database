# -*- coding: utf-8 -*-
"""
run_pipeline_debug.py - 全链路调试运行器
功能: 按 DLL 调用粒度导出阶段性成果到 output/pipeline_debug/<frame_name>/
用途: 全链路调试与验证，每个 DLL 调用导出 FITS + PNG + JSON + 日志
调用: python run_pipeline_debug.py

9 个节点 (按 DLL 调用粒度):
    0_read_fits          - ImageReader.read (读取 FITS)
    1_calibrate          - ac_calibrate_frame (校准 + 坏点修复)
    2a_platesolve_solve  - ipv_solve_from_memory (WCS/SIP 求解)
    2b_platesolve_stardet - sdet_detect_ex (星点检测)
    2c_platesolve_gaia   - gaia_cone_search (Gaia 星表查询)
    3_psf_fit            - DynamicPSF.fit_batch (PSF 拟合)
    4_photometric        - pc_calibrate_simple_with_gaia (光度定标)
    5_snr                - snr_estimate (SNR 估算, 乘法模型)
    6_drizzle            - hp_drizzle_run (HEALPix Drizzle)

注意:
    - platesolve_adapter 是单一 handler, 2a/2b/2c 对应同一次 handler 调用
      中的三个 DLL 调用产物 (WCS header / star_det / gaia_cat)
    - 5_snr 节点直接调用 snr_adapter.run_snr_stage (独立函数, 不经 Orchestrator handler)
      因为 Orchestrator 构造函数暂未支持 snr_params; psf 块生命周期延至 5_snr 后清理
    - 不修改任何 adapter 代码, 只调用现有 Orchestrator 内部 handler
    - 输出目录不入 git (.gitignore 已添加 output/pipeline_debug/)
"""

from __future__ import annotations

import ctypes
import importlib.util
import json
import logging
import os
import sys
import time
import traceback
from datetime import datetime
from typing import Optional

import numpy as np

# ============================================================================
# 路径设置 (必须在导入任何项目模块之前完成)
# ============================================================================

HERE = os.path.dirname(os.path.abspath(__file__))
# scripts -> orchestrator -> lib -> 项目根目录 (上3级)
PROJECT_ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
LIB_DIR = os.path.join(PROJECT_ROOT, "lib")

# MinGW DLL 路径 (C++ 模块依赖)
MINGW_BIN = r"C:\msys64\mingw64\bin"
if os.path.isdir(MINGW_BIN) and MINGW_BIN not in os.environ.get("PATH", ""):
    os.environ["PATH"] = MINGW_BIN + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(MINGW_BIN)
    except (OSError, FileNotFoundError):
        pass

# astro_image_io.dll 目录
ASTRO_IO_DIR = os.path.join(LIB_DIR, "astro_image_io")
if ASTRO_IO_DIR not in os.environ.get("PATH", ""):
    os.environ["PATH"] = ASTRO_IO_DIR + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(ASTRO_IO_DIR)
    except (OSError, FileNotFoundError):
        pass

# Python 模块路径 (注意: sys.path.insert(0, ...) 会让列表最后一项优先级最高,
# 因此 orchestrator/python 必须放最后, 确保 import orchestrator 加载新版本
# 而非 astro_image_io/python/orchestrator.py 旧版本)
_MODULE_PATHS = [
    "astro_image_io/python",
    "calibration/python",
    "plate_solve/python",
    "plate_solve/archive/vector_method/python/python",
    "photometric_calib/flux_calibrator/python",
    "photometric_calib/python",
    "photometric_calib/spectrum_integrator/python",
    "healpix_db/healpix_drizzle",
    "dynamic_psf/python",
    "star_detector/python",
    "gaia_xpsd_client/python",
    "snr_estimator/python",
    "orchestrator/python/pipeline_adapters",
    "orchestrator/python",
]
for _module_dir in _MODULE_PATHS:
    _p = os.path.join(LIB_DIR, _module_dir)
    if os.path.isdir(_p) and _p not in sys.path:
        sys.path.insert(0, _p)


# ============================================================================
# 默认配置
# ============================================================================

# 测试帧 (相对项目根目录)
DEFAULT_FRAME_PATH = os.path.join(
    PROJECT_ROOT, "testdata", "Galaxy_Center_T4", "lights", "panel1",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts",
)

# 校准帧目录
DEFAULT_CALIB_DIR = os.path.join(PROJECT_ROOT, "testdata", "T4 calibration files")

# Gaia DR3SP 数据目录
DEFAULT_GAIA_DATA_DIR = os.path.join(PROJECT_ROOT, "GaiaDR3SP")

# 输出根目录
DEFAULT_OUTPUT_ROOT = os.path.join(PROJECT_ROOT, "output", "pipeline_debug")


# ============================================================================
# 日志初始化
# ============================================================================

def _init_logger(log_dir: str) -> logging.Logger:
    """初始化脚本主日志

    参数:
        log_dir: 日志目录 (output/pipeline_debug/<frame_name>/logs/)

    返回:
        logging.Logger 实例
    """
    os.makedirs(log_dir, exist_ok=True)
    log_file = os.path.join(
        log_dir, "run_pipeline_debug_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log"
    )
    formatter = logging.Formatter(
        "[%(asctime)s] [%(levelname)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )

    lg = logging.getLogger("run_pipeline_debug")
    lg.setLevel(logging.DEBUG)
    lg.propagate = False
    if not lg.handlers:
        fh = logging.FileHandler(log_file, encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(formatter)
        lg.addHandler(fh)

        ch = logging.StreamHandler()
        ch.setLevel(logging.INFO)
        ch.setFormatter(formatter)
        lg.addHandler(ch)

    lg.info("run_pipeline_debug 日志初始化完成: %s", log_file)
    return lg


# ============================================================================
# 参数类加载 (与 test_orchestrator_e2e.py 一致)
# ============================================================================

def _load_module(module_name: str, file_path: str):
    """从文件路径加载 Python 模块 (避免命名冲突)"""
    if module_name in sys.modules:
        return sys.modules[module_name]
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = mod
    spec.loader.exec_module(mod)
    return mod


def _load_params_classes():
    """加载各阶段参数类

    返回: (CalibrateParams, PlateSolveParams, PhotometricParams, DrizzleParams)
    """
    classes = {}
    adapters = [
        ("CalibrateParams", "calibrate_adapter.py",
         os.path.join(LIB_DIR, "orchestrator", "python", "pipeline_adapters",
                      "calibrate_adapter.py")),
        ("PlateSolveParams", "platesolve_adapter.py",
         os.path.join(LIB_DIR, "orchestrator", "python", "pipeline_adapters",
                      "platesolve_adapter.py")),
        ("PhotometricParams", "photometric_adapter.py",
         os.path.join(LIB_DIR, "orchestrator", "python", "pipeline_adapters",
                      "photometric_adapter.py")),
        ("DrizzleParams", "drizzle_adapter.py",
         os.path.join(LIB_DIR, "orchestrator", "python", "pipeline_adapters",
                      "drizzle_adapter.py")),
    ]
    for cls_name, mod_suffix, path in adapters:
        try:
            mod = _load_module("_debug_" + mod_suffix.replace(".", "_"), path)
            classes[cls_name] = getattr(mod, cls_name)
        except Exception as e:
            print(f"[FAIL] 加载 {cls_name} 失败: {e}")
    return classes


# ============================================================================
# 图像统计与 WCS 辅助
# ============================================================================

def _compute_stats(arr: np.ndarray) -> dict:
    """计算 numpy 数组通用统计 (mean/std/min/max/shape/dtype)"""
    if arr is None:
        return {"exists": False}
    if not isinstance(arr, np.ndarray):
        arr = np.asarray(arr)
    stats = {
        "exists": True,
        "shape": list(arr.shape),
        "dtype": str(arr.dtype),
        "size": int(arr.size),
    }
    if arr.size > 0 and np.issubdtype(arr.dtype, np.number):
        stats["mean"] = float(np.mean(arr))
        stats["std"] = float(np.std(arr))
        stats["min"] = float(np.min(arr))
        stats["max"] = float(np.max(arr))
        stats["median"] = float(np.median(arr))
    return stats


def _build_wcs_from_frame(frame) -> Optional[object]:
    """从 frame header KV 块构建 astropy.wcs.WCS 对象

    返回: WCS 对象, 失败返回 None
    """
    try:
        from astropy.wcs import WCS
        wcs = WCS(naxis=2)
        ctype1 = frame.kv_get("header", "CTYPE1")
        ctype2 = frame.kv_get("header", "CTYPE2")
        if ctype1:
            wcs.wcs.ctype = [ctype1, ctype2 or "DEC--TAN"]
        wcs.wcs.crval = [
            frame.kv_get_double("header", "CRVAL1", 0.0),
            frame.kv_get_double("header", "CRVAL2", 0.0),
        ]
        wcs.wcs.crpix = [
            frame.kv_get_double("header", "CRPIX1", 0.0),
            frame.kv_get_double("header", "CRPIX2", 0.0),
        ]
        wcs.wcs.cd = np.array([
            [frame.kv_get_double("header", "CD1_1", 0.0),
             frame.kv_get_double("header", "CD1_2", 0.0)],
            [frame.kv_get_double("header", "CD2_1", 0.0),
             frame.kv_get_double("header", "CD2_2", 0.0)],
        ])
        # SIP 系数 (如果存在)
        a_order_str = frame.kv_get("header", "A_ORDER")
        if a_order_str:
            try:
                a_order = int(a_order_str)
                if a_order > 0:
                    wcs.sip = None  # 简化: 仅用 CD 矩阵做近似转换
            except (ValueError, TypeError):
                pass
        return wcs
    except Exception:
        return None


def _radec_to_xy(wcs, ra_arr: np.ndarray, dec_arr: np.ndarray):
    """ra/dec (度) -> x/y 像素坐标 (0-indexed)"""
    if wcs is None or ra_arr.size == 0:
        return np.array([]), np.array([])
    try:
        coords = np.column_stack([ra_arr, dec_arr])
        xy = wcs.world_to_pixel_values(coords)
        return xy[0], xy[1]
    except Exception:
        # fallback: 用 CRVAL/CRPIX/CD 矩阵手动计算 (一阶近似, 忽略 SIP)
        try:
            crval1 = wcs.wcs.crval[0]
            crval2 = wcs.wcs.crval[1]
            crpix1 = wcs.wcs.crpix[0]
            crpix2 = wcs.wcs.crpix[1]
            cd = wcs.wcs.cd
            # 球面切平面投影近似 (TAN 一阶)
            dra = (ra_arr - crval1) * np.cos(np.radians(crval2))
            ddec = dec_arr - crval2
            det = cd[0, 0] * cd[1, 1] - cd[0, 1] * cd[1, 0]
            if det == 0:
                return np.array([]), np.array([])
            # 逆 CD 矩阵: [x; y] = CD^-1 * [dra; ddec] + [crpix1; crpix2]
            inv_cd = np.array([[cd[1, 1], -cd[0, 1]], [-cd[1, 0], cd[0, 0]]]) / det
            xy = inv_cd @ np.vstack([dra, ddec])
            x = xy[0] + crpix1 - 1  # FITS CRPIX 是 1-indexed
            y = xy[1] + crpix2 - 1
            return x, y
        except Exception:
            return np.array([]), np.array([])


# ============================================================================
# 导出函数: FITS / PNG / JSON / LOG
# ============================================================================

def _export_fits(frame, stage_name: str, output_dir: str, logger: logging.Logger):
    """导出 FITS: data 块作为 PRIMARY HDU, header 块 KV 写入 header

    参数:
        frame: PipelineFramePy
        stage_name: 阶段名 (如 "0_read_fits")
        output_dir: 输出目录
        logger: 日志器
    """
    from astropy.io import fits

    # FITS 关键字最长 8 字符, 超长关键字映射表
    _FITS_KEY_MAP = {
        "FRAME_TYPE": "FRAMTYPE",
    }

    def _fit_key(k: str) -> str:
        """将超长关键字映射到 <=8 字符"""
        if len(k) <= 8:
            return k
        if k in _FITS_KEY_MAP:
            return _FITS_KEY_MAP[k]
        # 通用缩写: 截断到 8 字符
        return k[:8]

    fits_path = os.path.join(output_dir, f"{stage_name}.fits")
    try:
        # 1. 取 data 块 (FLOAT32 [H, W])
        pixels = frame.get_block_data("data")
        if pixels is None:
            logger.warning("[%s] data 块不存在, 跳过 FITS 导出", stage_name)
            return None

        data = np.ascontiguousarray(pixels, dtype=np.float32)
        if data.ndim != 2:
            data = data.reshape(-1, data.shape[-1]) if data.ndim > 2 else data

        # 2. 构建 astropy FITS Header (从 header KV 块)
        hdr = fits.Header()
        header_kv = frame.get_block_kv("header")
        if header_kv:
            # WCS 关键字优先级 (FITS 标准)
            wcs_keys_priority = [
                "CTYPE1", "CTYPE2", "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2",
                "CD1_1", "CD1_2", "CD2_1", "CD2_2", "RADESYS", "EQUINOX",
                "A_ORDER", "B_ORDER", "AP_ORDER", "BP_ORDER",
            ]
            # 先写优先级关键字
            for k in wcs_keys_priority:
                if k in header_kv:
                    val = header_kv[k]
                    try:
                        val_num = float(val)
                        hdr[_fit_key(k)] = val_num
                    except (ValueError, TypeError):
                        hdr[_fit_key(k)] = val
            # SIP 系数 (A_i_j / B_i_j / AP_i_j / BP_i_j, 形如 "A_0_0", "B_1_2")
            # 排除 A_ORDER/B_ORDER/AP_ORDER/BP_ORDER (阶数, 非系数)
            sip_order_keys = {"A_ORDER", "B_ORDER", "AP_ORDER", "BP_ORDER"}
            for k, v in header_kv.items():
                if k in sip_order_keys:
                    continue
                if any(k.startswith(p) for p in ("A_", "B_", "AP_", "BP_")):
                    try:
                        hdr[_fit_key(k)] = float(v)
                    except (ValueError, TypeError):
                        hdr[_fit_key(k)] = v
            # 其他关键字 (EXPTIME/FILTER/GAIN/OBJECT/DATE-OBS 等)
            skip_prefixes = ("CTYPE", "CRVAL", "CRPIX", "CD", "A_", "B_", "AP_", "BP_")
            for k, v in header_kv.items():
                if k in wcs_keys_priority:
                    continue
                if k in sip_order_keys:
                    continue
                if any(k.startswith(p) for p in skip_prefixes):
                    continue
                if k in ("RADESYS", "EQUINOX"):
                    continue
                if k == "SOURCE_PATH":
                    continue  # 字符串太长会破坏 FITS header
                try:
                    val_num = float(v)
                    # 区分 int/float
                    if val_num == int(val_num) and "e" not in v.lower() and "." not in v:
                        hdr[_fit_key(k)] = int(val_num)
                    else:
                        hdr[_fit_key(k)] = val_num
                except (ValueError, TypeError):
                    try:
                        hdr[_fit_key(k)] = v[:68]  # FITS 关键字值限长 68 字符
                    except Exception:
                        pass

        # 3. 确保 EXTEND 卡片存在 (FITS 标准, validate_fits_format 检查)
        # 注意: PrimaryHDU 构造时可能移除 EXTEND, 需在构造后设置
        # 4. 写入 FITS
        hdu = fits.PrimaryHDU(data=data, header=hdr)
        hdu.header["EXTEND"] = True
        hdu.writeto(fits_path, overwrite=True, checksum=True)
        logger.info("[%s] FITS 导出: %s (shape=%s, header keys=%d)",
                    stage_name, fits_path, data.shape, len(hdr))
        return fits_path
    except Exception as e:
        logger.error("[%s] FITS 导出失败: %s", stage_name, e, exc_info=True)
        return None


def _export_png(frame, stage_name: str, output_dir: str, logger: logging.Logger,
                overlays: Optional[list] = None, title_suffix: str = ""):
    """导出 PNG: imshow + colorbar + title, percentile 拉伸 (vmin=1%, vmax=99%)

    参数:
        frame: PipelineFramePy
        stage_name: 阶段名
        output_dir: 输出目录
        logger: 日志器
        overlays: 标注列表, 每项为 dict:
            {
                "x": np.ndarray, "y": np.ndarray,  # 像素坐标
                "label": str, "color": str, "markersize": int,
                "text": Optional[np.ndarray],  # 每点的文本标注 (如 FWHM)
            }
        title_suffix: 标题后缀
    """
    import matplotlib
    matplotlib.use("Agg")  # 非交互后端
    import matplotlib.pyplot as plt

    png_path = os.path.join(output_dir, f"{stage_name}.png")
    try:
        pixels = frame.get_block_data("data")
        if pixels is None:
            logger.warning("[%s] data 块不存在, 跳过 PNG 导出", stage_name)
            return None

        data = np.asarray(pixels, dtype=np.float32)
        if data.ndim != 2:
            logger.warning("[%s] data 块非 2D (shape=%s), 跳过 PNG", stage_name, data.shape)
            return None

        # percentile 拉伸
        finite = data[np.isfinite(data)]
        if finite.size > 0:
            vmin, vmax = np.percentile(finite, [1, 99])
        else:
            vmin, vmax = 0.0, 1.0

        # 绘图
        fig, ax = plt.subplots(figsize=(12, 8), dpi=100)
        im = ax.imshow(data, cmap="gray", origin="lower", vmin=vmin, vmax=vmax,
                       interpolation="nearest", aspect="equal")
        cbar = plt.colorbar(im, ax=ax, shrink=0.8, pad=0.02)
        cbar.set_label("Pixel Value")

        title = stage_name + (f"  {title_suffix}" if title_suffix else "")
        title += f"\nshape={data.shape}, dtype={data.dtype}, "
        title += f"vmin={vmin:.2f} (1%), vmax={vmax:.2f} (99%)"
        ax.set_title(title, fontsize=10)
        ax.set_xlabel("X (pixel)")
        ax.set_ylabel("Y (pixel)")

        # 添加标注
        if overlays:
            for ov in overlays:
                x = np.asarray(ov.get("x", []))
                y = np.asarray(ov.get("y", []))
                if x.size == 0:
                    continue
                color = ov.get("color", "red")
                label = ov.get("label", "")
                markersize = ov.get("markersize", 4)
                ax.scatter(x, y, s=markersize ** 2, c=color, marker="o",
                           edgecolors="white", linewidths=0.3, alpha=0.8,
                           label=label)
                # 文本标注 (如 FWHM 值)
                text = ov.get("text")
                if text is not None and len(text) == len(x):
                    for xi, yi, ti in zip(x, y, text):
                        if ti:
                            ax.text(xi, yi, str(ti), fontsize=6,
                                    color=color, ha="left", va="bottom")
            ax.legend(loc="upper right", fontsize=8, framealpha=0.7)

        plt.tight_layout()
        plt.savefig(png_path, bbox_inches="tight", dpi=100)
        plt.close(fig)
        logger.info("[%s] PNG 导出: %s (overlays=%d)",
                    stage_name, png_path, len(overlays) if overlays else 0)
        return png_path
    except Exception as e:
        logger.error("[%s] PNG 导出失败: %s", stage_name, e, exc_info=True)
        try:
            plt.close("all")
        except Exception:
            pass
        return None


def _export_json(frame, stage_name: str, output_dir: str, logger: logging.Logger,
                 stage_stats: Optional[dict] = None, ahpx_path: Optional[str] = None,
                 elapsed: Optional[float] = None):
    """导出 JSON: 通用统计 (mean/std/min/max/shape/dtype) + 阶段特定统计

    参数:
        frame: PipelineFramePy
        stage_name: 阶段名
        output_dir: 输出目录
        logger: 日志器
        stage_stats: 阶段特定统计 dict
        ahpx_path: .ahpx 文件路径 (drizzle 阶段)
        elapsed: 耗时 (秒)
    """
    json_path = os.path.join(output_dir, f"{stage_name}.json")
    try:
        # 收集所有块的状态
        blocks_info = {}
        for name in frame.list_blocks():
            info = frame.get_block_info(name)
            if not info:
                continue
            block_info = {
                "type": _block_type_name(info["type"]),
                "count": int(info["count"]),
                "dims": list(info["dims"]),
                "n_dims": int(info["n_dims"]),
                "description": info.get("description", ""),
            }
            # 数值块: 添加统计
            if info["type"] in (0, 1, 2, 3):  # FLOAT32/FLOAT64/INT32/INT64
                data = frame.get_block_data(name)
                block_info["stats"] = _compute_stats(data)
            elif info["type"] == 5:  # KV 块
                kv = frame.get_block_kv(name)
                block_info["kv"] = kv or {}
            elif info["type"] == 4:  # STRING 块
                block_info["value"] = frame.get_block_string(name) or ""
            blocks_info[name] = block_info

        out = {
            "stage": stage_name,
            "timestamp": datetime.now().isoformat(),
            "blocks": blocks_info,
            "stage_stats": stage_stats or {},
        }
        # PSF 阶段: 导出 psf 数组 (validate_psf_quality.py 期望顶层 psf 字段,
        # 每行 [status, B, flux, cx, cy, fwhm])
        if stage_name == "3_psf_fit":
            try:
                psf_data = frame.get_block_data("psf")
                if psf_data is not None and isinstance(psf_data, np.ndarray) and psf_data.size > 0:
                    # 确保形状为 [N, 9] (status,B,flux,cx,cy,fwhm,A,mad,eccentricity)
                    psf_arr = np.asarray(psf_data, dtype=np.float64)
                    if psf_arr.ndim == 2 and psf_arr.shape[1] >= 6:
                        out["psf"] = psf_arr.tolist()
                        logger.info("[%s] psf 数组导出: %d 行, %d 列",
                                    stage_name, psf_arr.shape[0], psf_arr.shape[1])
                    else:
                        logger.warning("[%s] psf 块形状异常: %s, 跳过 psf 数组导出",
                                       stage_name, psf_arr.shape)
                else:
                    logger.warning("[%s] psf 块数据为空或不存在, 跳过 psf 数组导出", stage_name)
            except Exception as e_psf:
                logger.error("[%s] psf 数组导出失败: %s", stage_name, e_psf, exc_info=True)
        if ahpx_path:
            out["ahpx_path"] = ahpx_path
            out["ahpx_exists"] = os.path.isfile(ahpx_path) if ahpx_path else False
            if os.path.isfile(ahpx_path):
                out["ahpx_size_bytes"] = os.path.getsize(ahpx_path)
        if elapsed is not None:
            out["elapsed_sec"] = float(elapsed)

        with open(json_path, "w", encoding="utf-8") as f:
            json.dump(out, f, ensure_ascii=False, indent=2, default=_json_default)
        logger.info("[%s] JSON 导出: %s (blocks=%d)",
                    stage_name, json_path, len(blocks_info))
        return json_path
    except Exception as e:
        logger.error("[%s] JSON 导出失败: %s", stage_name, e, exc_info=True)
        return None


def _export_log(frame, stage_name: str, output_dir: str, logger: logging.Logger,
                stage_stats: Optional[dict] = None, elapsed: Optional[float] = None,
                note: str = ""):
    """导出阶段日志 (人类可读的文本日志)

    参数:
        frame: PipelineFramePy
        stage_name: 阶段名
        output_dir: 输出目录
        logger: 日志器
        stage_stats: 阶段特定统计
        elapsed: 耗时 (秒)
        note: 附加说明
    """
    log_path = os.path.join(output_dir, f"{stage_name}.log")
    try:
        lines = []
        lines.append(f"=== {stage_name} ===")
        lines.append(f"Time: {datetime.now().isoformat()}")
        if elapsed is not None:
            lines.append(f"Elapsed: {elapsed:.3f} s")
        if note:
            lines.append(f"Note: {note}")
        lines.append("")
        lines.append("--- Blocks ---")
        for name in frame.list_blocks():
            info = frame.get_block_info(name)
            if info:
                type_name = _block_type_name(info["type"])
                lines.append(
                    f"  - {name}: type={type_name}, dims={info['dims']}, "
                    f"count={info['count']}, desc={info.get('description', '')}"
                )
                # KV 块: 列出所有键值
                if info["type"] == 5:
                    kv = frame.get_block_kv(name)
                    if kv:
                        for k, v in kv.items():
                            lines.append(f"      {k} = {v}")
        lines.append("")
        if stage_stats:
            lines.append("--- Stage Stats ---")
            for k, v in stage_stats.items():
                lines.append(f"  {k}: {v}")
        lines.append("")
        with open(log_path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines))
        logger.info("[%s] LOG 导出: %s", stage_name, log_path)
        return log_path
    except Exception as e:
        logger.error("[%s] LOG 导出失败: %s", stage_name, e, exc_info=True)
        return None


def _block_type_name(type_id: int) -> str:
    """块类型 ID -> 名称"""
    return {
        0: "FLOAT32", 1: "FLOAT64", 2: "INT32",
        3: "INT64", 4: "STRING", 5: "KV", 6: "RAW",
    }.get(type_id, f"TYPE_{type_id}")


def _json_default(obj):
    """JSON 序列化 fallback (处理 numpy 类型)"""
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, (np.floating,)):
        return float(obj)
    if isinstance(obj, np.ndarray):
        return obj.tolist()
    return str(obj)


# ============================================================================
# 阶段成果导出 (统一入口)
# ============================================================================

def export_stage_output(frame, stage_name: str, output_dir: str,
                        logger: logging.Logger,
                        stage_stats: Optional[dict] = None,
                        overlays: Optional[list] = None,
                        ahpx_path: Optional[str] = None,
                        elapsed: Optional[float] = None,
                        note: str = "",
                        export_fits: bool = True,
                        export_png: bool = True,
                        title_suffix: str = ""):
    """导出阶段性成果: FITS + PNG + JSON + 日志

    参数:
        frame: PipelineFramePy
        stage_name: 阶段名 (如 "0_read_fits")
        output_dir: 输出目录
        logger: 日志器
        stage_stats: 阶段特定统计 dict
        overlays: PNG 标注列表
        ahpx_path: .ahpx 文件路径 (drizzle 阶段)
        elapsed: 耗时 (秒)
        note: 附加说明 (写入日志)
        export_fits: 是否导出 FITS
        export_png: 是否导出 PNG
        title_suffix: PNG 标题后缀
    """
    logger.info("=" * 60)
    logger.info("[%s] 开始导出阶段性成果", stage_name)
    logger.info("=" * 60)

    if export_fits:
        _export_fits(frame, stage_name, output_dir, logger)
    if export_png:
        _export_png(frame, stage_name, output_dir, logger,
                    overlays=overlays, title_suffix=title_suffix)
    _export_json(frame, stage_name, output_dir, logger,
                 stage_stats=stage_stats, ahpx_path=ahpx_path, elapsed=elapsed)
    _export_log(frame, stage_name, output_dir, logger,
                stage_stats=stage_stats, elapsed=elapsed, note=note)


# ============================================================================
# 阶段特定统计收集
# ============================================================================

def _collect_cal_stats(frame, logger: logging.Logger) -> dict:
    """CALIBRATE 后: dark_k, 校准前后 mean 对比"""
    stats = {}
    dark_k = frame.kv_get("cal_stats", "DARK_K")
    if dark_k is not None:
        stats["dark_k"] = dark_k
    pixels = frame.get_block_data("data")
    if pixels is not None:
        s = _compute_stats(pixels)
        stats["after_mean"] = s.get("mean")
        stats["after_std"] = s.get("std")
        stats["after_min"] = s.get("min")
        stats["after_max"] = s.get("max")
    return stats


def _collect_platesolve_stats(frame, logger: logging.Logger) -> dict:
    """PLATESOLVE 后: WCS 字段 (CD/CRVAL/CRPIX/SIP order)"""
    stats = {}
    wcs_keys = [
        "CTYPE1", "CTYPE2", "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2",
        "CD1_1", "CD1_2", "CD2_1", "CD2_2", "RADESYS", "EQUINOX",
        "A_ORDER", "B_ORDER", "AP_ORDER", "BP_ORDER",
    ]
    for k in wcs_keys:
        v = frame.kv_get("header", k)
        if v is not None:
            stats[k] = v
    # 像素尺度 (从 CD 矩阵推算)
    try:
        cd11 = frame.kv_get_double("header", "CD1_1", 0.0)
        cd12 = frame.kv_get_double("header", "CD1_2", 0.0)
        cd21 = frame.kv_get_double("header", "CD2_1", 0.0)
        cd22 = frame.kv_get_double("header", "CD2_2", 0.0)
        det = abs(cd11 * cd22 - cd12 * cd21)
        if det > 0:
            pixel_scale_deg = float(np.sqrt(det))
            stats["pixel_scale_deg"] = f"{pixel_scale_deg:.6e}"
            stats["pixel_scale_arcsec"] = f"{pixel_scale_deg * 3600.0:.4f}"
    except Exception:
        pass
    return stats


def _collect_stardet_stats(frame, logger: logging.Logger) -> dict:
    """星点检测后: n_detected (star_det 块行数) + flux 分布"""
    stats = {}
    star_det = frame.get_block_data("star_det")
    if star_det is not None and star_det.size > 0:
        n = star_det.shape[0]
        stats["n_detected"] = int(n)
        if star_det.shape[1] >= 3:
            flux = star_det[:, 2]
            stats["flux_mean"] = float(np.mean(flux))
            stats["flux_median"] = float(np.median(flux))
            stats["flux_min"] = float(np.min(flux))
            stats["flux_max"] = float(np.max(flux))
            stats["flux_std"] = float(np.std(flux))
        if star_det.shape[1] >= 4:
            mag = star_det[:, 3]
            stats["mag_min"] = float(np.min(mag))
            stats["mag_max"] = float(np.max(mag))
            stats["mag_median"] = float(np.median(mag))
    else:
        stats["n_detected"] = 0
    return stats


def _collect_gaia_stats(frame, logger: logging.Logger) -> dict:
    """Gaia 查询后: n_catalog (gaia_cat 块行数) + mag 分布"""
    stats = {}
    gaia_cat = frame.get_block_data("gaia_cat")
    if gaia_cat is not None and gaia_cat.size > 0:
        n = gaia_cat.shape[0]
        stats["n_catalog"] = int(n)
        if gaia_cat.shape[1] >= 3:
            mag = gaia_cat[:, 2]
            stats["mag_min"] = float(np.min(mag))
            stats["mag_max"] = float(np.max(mag))
            stats["mag_median"] = float(np.median(mag))
    else:
        stats["n_catalog"] = 0
    return stats


def _collect_psf_stats(frame, logger: logging.Logger) -> dict:
    """PSF 拟合后: n_stars, n_ok, fwhm 中位数/标准差, flux 分布"""
    stats = {}
    psf = frame.get_block_data("psf")
    if psf is None or psf.size == 0:
        stats["n_stars"] = 0
        return stats
    n = psf.shape[0]
    stats["n_stars"] = int(n)
    status = psf[:, 0]
    n_ok = int(np.sum(status == 0))
    stats["n_ok"] = n_ok
    stats["n_failed"] = int(n - n_ok)
    if psf.shape[1] >= 3:
        flux = psf[:, 2]
        ok_flux = flux[status == 0]
        if ok_flux.size > 0:
            stats["flux_mean"] = float(np.mean(ok_flux))
            stats["flux_median"] = float(np.median(ok_flux))
            stats["flux_std"] = float(np.std(ok_flux))
    if psf.shape[1] >= 6:
        fwhm = psf[:, 5]
        ok_fwhm = fwhm[status == 0]
        if ok_fwhm.size > 0:
            stats["fwhm_median"] = float(np.median(ok_fwhm))
            stats["fwhm_mean"] = float(np.mean(ok_fwhm))
            stats["fwhm_std"] = float(np.std(ok_fwhm))
            stats["fwhm_min"] = float(np.min(ok_fwhm))
            stats["fwhm_max"] = float(np.max(ok_fwhm))
    return stats


def _collect_photo_stats(frame, logger: logging.Logger) -> dict:
    """PHOTOMETRIC 后: n_matched, scale_factor, sigma_residual"""
    stats = {}
    n_matched = frame.kv_get("photo_stats", "N_MATCHED")
    if n_matched is not None:
        stats["n_matched"] = n_matched
    scale = frame.kv_get("photo_stats", "SCALE_FACTOR")
    if scale is not None:
        stats["scale_factor"] = scale
    sigma_residual = frame.kv_get("photo_stats", "SIGMA_RESIDUAL")
    if sigma_residual is not None:
        stats["sigma_residual"] = sigma_residual
    pixels = frame.get_block_data("data")
    if pixels is not None:
        s = _compute_stats(pixels)
        stats["after_mean"] = s.get("mean")
        stats["after_std"] = s.get("std")
    return stats


def _collect_drizzle_stats(frame, ahpx_path: Optional[str],
                            logger: logging.Logger) -> dict:
    """DRIZZLE 后: n_source_pixels, n_healpix_pixels, .ahpx 路径"""
    stats = {}
    if ahpx_path:
        stats["ahpx_path"] = ahpx_path
        stats["ahpx_exists"] = os.path.isfile(ahpx_path)
        if os.path.isfile(ahpx_path):
            stats["ahpx_size_bytes"] = os.path.getsize(ahpx_path)
    # weight 块统计
    if frame.has_block("weight"):
        weight = frame.get_block_data("weight")
        if weight is not None:
            s = _compute_stats(weight)
            stats["weight_mean"] = s.get("mean")
            stats["weight_std"] = s.get("std")
            stats["weight_min"] = s.get("min")
            stats["weight_max"] = s.get("max")
    return stats


# ============================================================================
# 主流程
# ============================================================================

def _find_output_ahpx(frame, output_dir: str) -> Optional[str]:
    """从 frame 的 SOURCE_PATH 推导 .ahpx 输出路径并验证存在"""
    try:
        source_path = frame.kv_get("header", "SOURCE_PATH")
        if source_path:
            basename = os.path.splitext(os.path.basename(source_path))[0]
        else:
            basename = "drizzle_output"
        ahpx_path = os.path.join(output_dir, basename + ".ahpx")
        if os.path.isfile(ahpx_path):
            return ahpx_path
        return ahpx_path  # 返回路径即使不存在, 便于日志记录
    except Exception:
        return None


def _call_handler(handler, frame, stage_name: str, logger: logging.Logger) -> bool:
    """调用 handler 并处理错误 (从 Orchestrator._call_handler 复制而来)

    返回: True=成功, False=失败
    """
    err_buf = ctypes.create_string_buffer(512)
    ret = handler(frame.c_frame, None, err_buf, 512)
    if ret != 0:
        err_msg = err_buf.value.decode("utf-8", errors="replace").strip()
        if not err_msg:
            err_msg = "(无错误详情, 请查看模块日志)"
        logger.error("[%s] handler 调用失败 (code=%d): %s", stage_name, ret, err_msg)
        return False
    return True


def main():
    """主入口: 按 9 个节点顺序执行全链路调试导出"""
    # ---- 解析参数 ----
    import argparse
    parser = argparse.ArgumentParser(
        description="全链路调试运行器 - 按 DLL 调用粒度导出阶段性成果"
    )
    parser.add_argument(
        "--frame", default=DEFAULT_FRAME_PATH,
        help=f"测试帧 FITS 路径 (默认: {DEFAULT_FRAME_PATH})",
    )
    parser.add_argument(
        "--calib-dir", default=DEFAULT_CALIB_DIR,
        help=f"校准帧目录 (默认: {DEFAULT_CALIB_DIR})",
    )
    parser.add_argument(
        "--gaia-dir", default=DEFAULT_GAIA_DATA_DIR,
        help=f"Gaia DR3SP 数据目录 (默认: {DEFAULT_GAIA_DATA_DIR})",
    )
    parser.add_argument(
        "--output-root", default=DEFAULT_OUTPUT_ROOT,
        help=f"输出根目录 (默认: {DEFAULT_OUTPUT_ROOT})",
    )
    parser.add_argument(
        "--skip-calibrate", action="store_true",
        help="跳过校准阶段 (无 master 帧)",
    )
    args = parser.parse_args()

    # ---- 验证输入文件 ----
    print("=" * 70)
    print("全链路调试运行器 (run_pipeline_debug)")
    print("=" * 70)
    print(f"Python: {sys.executable}")
    print(f"项目根目录: {PROJECT_ROOT}")
    print(f"测试帧: {args.frame}")
    print(f"校准帧目录: {args.calib_dir}")
    print(f"Gaia DR3SP 目录: {args.gaia_dir}")
    print(f"输出根目录: {args.output_root}")
    print("=" * 70)

    if not os.path.isfile(args.frame):
        print(f"[FAIL] 测试帧不存在: {args.frame}")
        sys.exit(1)
    if not os.path.isdir(args.gaia_dir):
        print(f"[FAIL] Gaia 数据目录不存在: {args.gaia_dir}")
        sys.exit(1)

    # ---- 准备输出目录 ----
    frame_basename = os.path.splitext(os.path.basename(args.frame))[0]
    output_dir = os.path.join(args.output_root, frame_basename)
    log_dir = os.path.join(output_dir, "logs")
    os.makedirs(log_dir, exist_ok=True)

    logger = _init_logger(log_dir)
    logger.info("输出目录: %s", output_dir)
    logger.info("日志目录: %s", log_dir)

    # ---- 加载参数类 ----
    logger.info("-" * 40)
    logger.info("加载参数类")
    logger.info("-" * 40)
    classes = _load_params_classes()
    CalibrateParams = classes.get("CalibrateParams")
    PlateSolveParams = classes.get("PlateSolveParams")
    PhotometricParams = classes.get("PhotometricParams")
    DrizzleParams = classes.get("DrizzleParams")

    # ---- 构造阶段参数 ----
    logger.info("-" * 40)
    logger.info("构造阶段参数")
    logger.info("-" * 40)

    # 校准: 加载 masterDark/masterFlat/masterBias (XISF 格式)
    calib_params = None
    if not args.skip_calibrate and CalibrateParams is not None:
        try:
            from astro_image_io import ImageReader
            reader = ImageReader()
            master_dark_path = os.path.join(
                args.calib_dir, "masterDark_BIN-1_4500x3600_EXPOSURE-180.00s.xisf")
            master_flat_path = os.path.join(
                args.calib_dir, "masterFlat_BIN-1_4500x3600_FILTER-Red_mono.xisf")
            master_bias_path = os.path.join(
                args.calib_dir, "masterBias_BIN-1_4500x3600.xisf")

            master_dark = None
            master_flat = None
            master_bias = None
            if os.path.isfile(master_dark_path):
                img = reader.read(master_dark_path)
                master_dark = np.ascontiguousarray(img.data, dtype=np.float32)
                img.close()
                logger.info("masterDark 加载: %s, shape=%s", master_dark_path, master_dark.shape)
            else:
                logger.warning("masterDark 不存在: %s", master_dark_path)

            if os.path.isfile(master_flat_path):
                img = reader.read(master_flat_path)
                master_flat = np.ascontiguousarray(img.data, dtype=np.float32)
                img.close()
                logger.info("masterFlat 加载: %s, shape=%s", master_flat_path, master_flat.shape)
            else:
                logger.warning("masterFlat 不存在: %s", master_flat_path)

            if os.path.isfile(master_bias_path):
                img = reader.read(master_bias_path)
                master_bias = np.ascontiguousarray(img.data, dtype=np.float32)
                img.close()
                logger.info("masterBias 加载: %s, shape=%s", master_bias_path, master_bias.shape)
            else:
                logger.warning("masterBias 不存在: %s", master_bias_path)

            calib_params = CalibrateParams(
                master_bias=master_bias,
                master_dark=master_dark,
                master_flat=master_flat,
                dark_exposure=180.0,  # 与 masterDark 曝光一致
                dark_optimization=False,
                enable_cosmetic_correction=True,
                cc_method="median",
                hot_sigma=5.0,
                cold_sigma=5.0,
                max_structure_size=4,
            )
            logger.info("[OK] calib_params 构造完成")
        except Exception as e:
            logger.error("calib_params 构造失败: %s", e, exc_info=True)
            calib_params = None
    else:
        logger.info("[SKIP] 校准阶段 (skip_calibrate=%s, CalibrateParams=%s)",
                    args.skip_calibrate, CalibrateParams is not None)

    # 解析: env=None 触发 init_environment
    solve_params = None
    if PlateSolveParams is not None:
        try:
            solve_params = PlateSolveParams()  # env=None, gaia_mag_high=18.0
            logger.info("[OK] solve_params=PlateSolveParams(env=None) [内存接口]")
        except Exception as e:
            logger.error("PlateSolveParams 构造失败: %s", e, exc_info=True)

    # 光度: gaia_client 由 Orchestrator 创建
    photo_params = None
    if PhotometricParams is not None:
        try:
            photo_params = PhotometricParams(
                log_dir=log_dir,
                mag_min=8.0,
                mag_max=16.0,
            )
            logger.info("[OK] photo_params=PhotometricParams(log_dir=%s)", log_dir)
        except Exception as e:
            logger.error("PhotometricParams 构造失败: %s", e, exc_info=True)

    # Drizzle: nside=8192 (与 e2e 测试一致)
    drizzle_params = None
    if DrizzleParams is not None:
        try:
            drizzle_output_dir = os.path.join(output_dir, "drizzle")
            os.makedirs(drizzle_output_dir, exist_ok=True)
            drizzle_params = DrizzleParams(
                nside=8192, nested=True, pixfrac=0.8,
                output_dir=drizzle_output_dir,
            )
            logger.info("[OK] drizzle_params=DrizzleParams(nside=8192, output_dir=%s)",
                        drizzle_output_dir)
        except Exception as e:
            logger.error("DrizzleParams 构造失败: %s", e, exc_info=True)

    # SNR: 创建 SNREstimator 实例 (5_snr 节点直接调用 run_snr_stage, 不经 Orchestrator handler)
    snr_est = None
    try:
        from snr_adapter import run_snr_stage, SNRParams
        from snr_estimator import SNREstimator
        snr_est = SNREstimator()
        logger.info("[OK] snr_est=SNREstimator() (snr_estimator.dll 已加载)")
    except Exception as e:
        logger.warning("SNREstimator 构造失败 (5_snr 节点将被跳过): %s", e, exc_info=True)
        snr_est = None

    # ---- 创建 Orchestrator ----
    logger.info("-" * 40)
    logger.info("创建 Orchestrator")
    logger.info("-" * 40)
    try:
        # 直接从文件路径加载 Orchestrator, 避免 astro_image_io/python/orchestrator.py 旧版本冲突
        if 'orchestrator' in sys.modules:
            old_file = getattr(sys.modules['orchestrator'], '__file__', '?')
            logger.warning("orchestrator 已在 sys.modules: %s, 将删除并重新加载", old_file)
            del sys.modules['orchestrator']
        orchestrator_path = os.path.join(LIB_DIR, "orchestrator", "python", "orchestrator.py")
        orch_mod = _load_module("orchestrator", orchestrator_path)
        Orchestrator = orch_mod.Orchestrator
        logger.info("Orchestrator 加载自: %s", orchestrator_path)
        orch = Orchestrator(
            calib_params=calib_params,
            solve_params=solve_params,
            photo_params=photo_params,
            drizzle_params=drizzle_params,
            log_dir=log_dir,
            gaia_data_dir=args.gaia_dir,
        )
        logger.info("[OK] Orchestrator 创建成功")
    except Exception as e:
        logger.error("Orchestrator 创建失败: %s", e, exc_info=True)
        traceback.print_exc()
        sys.exit(1)

    # ---- 全链路执行 ----
    timings = {}
    t_global_start = time.time()

    try:
        # ====================================================================
        # 节点 0: 0_read_fits - ImageReader.read
        # ====================================================================
        logger.info("=" * 70)
        logger.info("节点 0: 0_read_fits (ImageReader.read)")
        logger.info("=" * 70)
        t0 = time.time()
        frame = orch._read_fits_to_frame(args.frame)
        elapsed = time.time() - t0
        timings["0_read_fits"] = elapsed
        logger.info("[0_read_fits] 完成, 耗时=%.3fs", elapsed)
        orch._print_block_status(frame, "0_read_fits 后")

        export_stage_output(
            frame, "0_read_fits", output_dir, logger,
            stage_stats={"source_path": args.frame},
            elapsed=elapsed,
            note="读取 FITS 文件, data 块 + header KV 块",
        )

        # ====================================================================
        # 节点 1: 1_calibrate - ac_calibrate_frame (含坏点修复)
        # ====================================================================
        if orch._calib_handler is not None:
            logger.info("=" * 70)
            logger.info("节点 1: 1_calibrate (ac_calibrate_frame)")
            logger.info("=" * 70)
            # 记录校准前 mean
            before_pixels = frame.get_block_data("data")
            before_mean = float(np.mean(before_pixels)) if before_pixels is not None else None

            t0 = time.time()
            ok = _call_handler(orch._calib_handler, frame, "1_calibrate", logger)
            elapsed = time.time() - t0
            timings["1_calibrate"] = elapsed
            logger.info("[1_calibrate] 完成, ok=%s, 耗时=%.3fs", ok, elapsed)

            if ok:
                cal_stats = _collect_cal_stats(frame, logger)
                if before_mean is not None:
                    cal_stats["before_mean"] = before_mean
                    after_pixels = frame.get_block_data("data")
                    if after_pixels is not None:
                        cal_stats["after_mean"] = float(np.mean(after_pixels))
                        cal_stats["mean_delta"] = cal_stats["after_mean"] - before_mean

                export_stage_output(
                    frame, "1_calibrate", output_dir, logger,
                    stage_stats=cal_stats,
                    elapsed=elapsed,
                    note="校准 (masterDark/Flat/Bias) + 坏点修复",
                    title_suffix=f"DARK_K={cal_stats.get('dark_k', 'N/A')}",
                )
            else:
                logger.error("[1_calibrate] 失败, 终止")
                raise RuntimeError("校准失败")
        else:
            logger.warning("[SKIP] 1_calibrate (orch._calib_handler is None)")

        # ====================================================================
        # 节点 2a/2b/2c: PLATESOLVE (一次 handler 调用, 三个 DLL 调用)
        # ====================================================================
        if orch._solve_handler is not None:
            logger.info("=" * 70)
            logger.info("节点 2a/2b/2c: PLATESOLVE (ipv_solve + sdet_detect + gaia_cone)")
            logger.info("=" * 70)
            t0 = time.time()
            ok = _call_handler(orch._solve_handler, frame, "PLATESOLVE", logger)
            elapsed = time.time() - t0
            timings["2_platesolve_total"] = elapsed
            logger.info("[PLATESOLVE] 完成, ok=%s, 耗时=%.3fs", ok, elapsed)

            if ok:
                # 节点 2a: 2a_platesolve_solve - WCS/SIP 注入 header
                logger.info("-" * 40)
                logger.info("节点 2a: 2a_platesolve_solve (ipv_solve_from_memory)")
                logger.info("-" * 40)
                solve_stats = _collect_platesolve_stats(frame, logger)
                export_stage_output(
                    frame, "2a_platesolve_solve", output_dir, logger,
                    stage_stats=solve_stats,
                    elapsed=elapsed,  # 共享总耗时
                    note="WCS/SIP 注入 header 块 (CRVAL/CRPIX/CD/SIP)",
                    title_suffix=(
                        f"CRVAL=({solve_stats.get('CRVAL1', '?')}, "
                        f"{solve_stats.get('CRVAL2', '?')})"
                    ),
                )

                # 节点 2b: 2b_platesolve_stardet - sdet_detect_ex
                logger.info("-" * 40)
                logger.info("节点 2b: 2b_platesolve_stardet (sdet_detect_ex)")
                logger.info("-" * 40)
                stardet_stats = _collect_stardet_stats(frame, logger)
                # 标注星点位置
                overlays_2b = []
                star_det = frame.get_block_data("star_det")
                if star_det is not None and star_det.size > 0:
                    overlays_2b.append({
                        "x": np.asarray(star_det[:, 0]),
                        "y": np.asarray(star_det[:, 1]),
                        "label": f"star_det ({star_det.shape[0]})",
                        "color": "red",
                        "markersize": 3,
                    })
                export_stage_output(
                    frame, "2b_platesolve_stardet", output_dir, logger,
                    stage_stats=stardet_stats,
                    overlays=overlays_2b,
                    elapsed=elapsed,
                    note="星点检测: star_det 块 FLOAT32[N,4] (x,y,flux,mag)",
                    title_suffix=f"n_detected={stardet_stats.get('n_detected', 0)}",
                )

                # 节点 2c: 2c_platesolve_gaia - gaia_cone_search
                logger.info("-" * 40)
                logger.info("节点 2c: 2c_platesolve_gaia (gaia_cone_search)")
                logger.info("-" * 40)
                gaia_stats = _collect_gaia_stats(frame, logger)
                # 标注 Gaia 星位置 (ra/dec -> x/y)
                overlays_2c = []
                gaia_cat = frame.get_block_data("gaia_cat")
                if gaia_cat is not None and gaia_cat.size > 0:
                    wcs = _build_wcs_from_frame(frame)
                    ra_arr = np.asarray(gaia_cat[:, 0])
                    dec_arr = np.asarray(gaia_cat[:, 1])
                    x_arr, y_arr = _radec_to_xy(wcs, ra_arr, dec_arr)
                    if x_arr.size > 0:
                        overlays_2c.append({
                            "x": x_arr,
                            "y": y_arr,
                            "label": f"gaia_cat ({gaia_cat.shape[0]})",
                            "color": "cyan",
                            "markersize": 3,
                        })
                export_stage_output(
                    frame, "2c_platesolve_gaia", output_dir, logger,
                    stage_stats=gaia_stats,
                    overlays=overlays_2c,
                    elapsed=elapsed,
                    note="Gaia 星表锥形查询: gaia_cat 块 FLOAT64[N,3] (ra,dec,mag)",
                    title_suffix=f"n_catalog={gaia_stats.get('n_catalog', 0)}",
                )

                # platesolve 后清理 weight 块 (与 orchestrator.run_single 一致)
                if frame.has_block("weight"):
                    frame.remove_block("weight")
                    logger.info("[清理] 丢弃 weight 块")
            else:
                logger.error("[PLATESOLVE] 失败, 终止")
                raise RuntimeError("解析失败")
        else:
            logger.warning("[SKIP] PLATESOLVE (orch._solve_handler is None)")

        # ====================================================================
        # 节点 3: 3_psf_fit - DynamicPSF.fit_batch
        # ====================================================================
        if orch._psf_handler is not None and frame.has_block("star_det"):
            logger.info("=" * 70)
            logger.info("节点 3: 3_psf_fit (DynamicPSF.fit_batch)")
            logger.info("=" * 70)
            t0 = time.time()
            ok = _call_handler(orch._psf_handler, frame, "3_psf_fit", logger)
            elapsed = time.time() - t0
            timings["3_psf_fit"] = elapsed
            logger.info("[3_psf_fit] 完成, ok=%s, 耗时=%.3fs", ok, elapsed)

            if ok:
                psf_stats = _collect_psf_stats(frame, logger)
                # 标注 PSF 拟合的星 + FWHM
                overlays_3 = []
                psf = frame.get_block_data("psf")
                if psf is not None and psf.size > 0:
                    # 只标注成功的 (status=0)
                    status = psf[:, 0]
                    mask_ok = (status == 0)
                    cx = psf[:, 3]
                    cy = psf[:, 4]
                    fwhm = psf[:, 5]
                    # 用 FWHM 文本 (每 5 个标一个, 避免太密集)
                    text = np.array(
                        [f"fwhm={f:.1f}" if (mask_ok[i] and i % 5 == 0) else ""
                         for i, f in enumerate(fwhm)]
                    )
                    overlays_3.append({
                        "x": cx[mask_ok],
                        "y": cy[mask_ok],
                        "label": f"PSF ok ({int(np.sum(mask_ok))}/{psf.shape[0]})",
                        "color": "yellow",
                        "markersize": 5,
                        "text": text[mask_ok],
                    })
                export_stage_output(
                    frame, "3_psf_fit", output_dir, logger,
                    stage_stats=psf_stats,
                    overlays=overlays_3,
                    elapsed=elapsed,
                    note="PSF 拟合: psf 块 FLOAT64[N,9] (status,B,flux,cx,cy,fwhm,A,mad,eccentricity)",
                    title_suffix=(
                        f"n_ok={psf_stats.get('n_ok', 0)}/{psf_stats.get('n_stars', 0)}, "
                        f"fwhm_med={psf_stats.get('fwhm_median', '?')}"
                    ),
                )
            else:
                logger.error("[3_psf_fit] 失败, 终止")
                raise RuntimeError("PSF 拟合失败")
        else:
            logger.warning("[SKIP] 3_psf_fit (handler=%s, star_det=%s)",
                           orch._psf_handler is not None, frame.has_block("star_det"))

        # ====================================================================
        # 节点 4: 4_photometric - pc_calibrate_simple_with_gaia
        # ====================================================================
        if orch._photo_handler is not None:
            logger.info("=" * 70)
            logger.info("节点 4: 4_photometric (pc_calibrate_simple_with_gaia)")
            logger.info("=" * 70)
            before_pixels = frame.get_block_data("data")
            before_mean = float(np.mean(before_pixels)) if before_pixels is not None else None

            t0 = time.time()
            ok = _call_handler(orch._photo_handler, frame, "4_photometric", logger)
            elapsed = time.time() - t0
            timings["4_photometric"] = elapsed
            logger.info("[4_photometric] 完成, ok=%s, 耗时=%.3fs", ok, elapsed)

            if ok:
                photo_stats = _collect_photo_stats(frame, logger)
                if before_mean is not None:
                    photo_stats["before_mean"] = before_mean
                    after_pixels = frame.get_block_data("data")
                    if after_pixels is not None:
                        photo_stats["after_mean"] = float(np.mean(after_pixels))
                        photo_stats["mean_ratio"] = (
                            photo_stats["after_mean"] / before_mean
                            if before_mean != 0 else None
                        )

                export_stage_output(
                    frame, "4_photometric", output_dir, logger,
                    stage_stats=photo_stats,
                    elapsed=elapsed,
                    note="光度定标 (Gaia BP/RP 光谱积分 + 全局 scale 校正)",
                    title_suffix=(
                        f"n_matched={photo_stats.get('n_matched', '?')}, "
                        f"scale={photo_stats.get('scale_factor', '?')}, "
                        f"sigma={photo_stats.get('sigma_residual', '?')}"
                    ),
                )

                # photometric 后清理 star_det/gaia_cat 块 (psf 块保留至 5_snr 后清理, 供 SNR 估算用)
                for name in ["star_det", "gaia_cat"]:
                    if frame.has_block(name):
                        frame.remove_block(name)
                        logger.info("[清理] 丢弃 %s 块", name)
            else:
                logger.error("[4_photometric] 失败, 终止")
                raise RuntimeError("光度校准失败")
        else:
            logger.warning("[SKIP] 4_photometric (orch._photo_handler is None)")

        # ====================================================================
        # 节点 5: 5_snr - snr_estimate (SNR 估算, 乘法模型)
        # ====================================================================
        if snr_est is not None:
            logger.info("=" * 70)
            logger.info("节点 5: 5_snr (snr_estimate, 乘法模型)")
            logger.info("=" * 70)
            t0 = time.time()
            ret = run_snr_stage(frame, snr_est=snr_est, log_dir=log_dir)
            elapsed = time.time() - t0
            timings["5_snr"] = elapsed
            ok = (ret == 0)
            logger.info("[5_snr] 完成, ret=%d, ok=%s, 耗时=%.3fs", ret, ok, elapsed)

            if ok:
                # 收集 snr 块统计
                snr_stats = {}
                snr_arr = frame.get_block_data("snr")
                if snr_arr is not None:
                    snr_stats = _compute_stats(snr_arr)
                    sigma_res = frame.kv_get("photo_stats", "SIGMA_RESIDUAL")
                    snr_stats["sigma_residual"] = sigma_res
                    if snr_arr.size > 0 and np.issubdtype(snr_arr.dtype, np.number):
                        snr_stats["snr_median"] = float(np.median(snr_arr))
                        snr_stats["snr_mean"] = float(np.mean(snr_arr))

                export_stage_output(
                    frame, "5_snr", output_dir, logger,
                    stage_stats=snr_stats,
                    elapsed=elapsed,
                    note="SNR 估算: SNR=SNR_phot×(SNR_psf/median), 输出 snr 块 float32[H,W]",
                    title_suffix=(
                        f"snr_med={snr_stats.get('snr_median', '?')}, "
                        f"sigma={snr_stats.get('sigma_residual', '?')}"
                    ),
                )

                # 5_snr 后清理 psf 块 (SNR 估算已完成, psf 不再需要)
                if frame.has_block("psf"):
                    frame.remove_block("psf")
                    logger.info("[清理] 丢弃 psf 块 (5_snr 后)")
            else:
                logger.error("[5_snr] 失败, 终止")
                raise RuntimeError("SNR 估算失败")
        else:
            logger.warning("[SKIP] 5_snr (snr_est is None, SNR 未初始化)")
            # 即使跳过 SNR, 也清理残留 psf 块
            if frame.has_block("psf"):
                frame.remove_block("psf")
                logger.info("[清理] 丢弃 psf 块 (5_snr 跳过)")

        # ====================================================================
        # 节点 6: 6_drizzle - hp_drizzle_run
        # ====================================================================
        if orch._drizzle_handler is not None:
            logger.info("=" * 70)
            logger.info("节点 6: 6_drizzle (hp_drizzle_run)")
            logger.info("=" * 70)
            t0 = time.time()
            ok = _call_handler(orch._drizzle_handler, frame, "6_drizzle", logger)
            elapsed = time.time() - t0
            timings["6_drizzle"] = elapsed
            logger.info("[6_drizzle] 完成, ok=%s, 耗时=%.3fs", ok, elapsed)

            if ok:
                # 找 .ahpx 文件路径
                ahpx_path = _find_output_ahpx(
                    frame, os.path.join(output_dir, "drizzle"))

                drizzle_stats = _collect_drizzle_stats(frame, ahpx_path, logger)
                export_stage_output(
                    frame, "6_drizzle", output_dir, logger,
                    stage_stats=drizzle_stats,
                    ahpx_path=ahpx_path,
                    elapsed=elapsed,
                    note="HEALPix Drizzle: 输出 .ahpx 文件",
                    title_suffix=(
                        f"ahpx={'存在' if drizzle_stats.get('ahpx_exists') else '不存在'}"
                    ),
                )
            else:
                logger.error("[6_drizzle] 失败, 终止")
                raise RuntimeError("Drizzle 失败")
        else:
            logger.warning("[SKIP] 6_drizzle (orch._drizzle_handler is None)")

        # ---- 全链路完成 ----
        t_total = time.time() - t_global_start
        timings["total"] = t_total
        logger.info("=" * 70)
        logger.info("全链路调试完成! 总耗时=%.3fs", t_total)
        logger.info("=" * 70)

    except Exception as e:
        logger.error("全链路执行失败: %s", e, exc_info=True)
        timings["error"] = str(e)
        raise
    finally:
        # 清理 frame
        try:
            if 'frame' in dir():
                orch._cleanup_frame(frame)
                frame.close()
        except Exception:
            pass
        # 释放 Orchestrator 资源
        try:
            orch.close()
        except Exception:
            pass

    # ---- 导出 timing.json ----
    timing_path = os.path.join(output_dir, "timing.json")
    try:
        timing_out = {
            "frame": args.frame,
            "frame_basename": frame_basename,
            "timestamp": datetime.now().isoformat(),
            "timings": timings,
            "total_elapsed_sec": float(timings.get("total", 0.0)),
        }
        with open(timing_path, "w", encoding="utf-8") as f:
            json.dump(timing_out, f, ensure_ascii=False, indent=2, default=_json_default)
        logger.info("timing.json 导出: %s", timing_path)
    except Exception as e:
        logger.error("timing.json 导出失败: %s", e, exc_info=True)

    # ---- 控制台打印汇总 ----
    print()
    print("=" * 70)
    print("全链路调试汇总")
    print("=" * 70)
    print(f"{'节点':<30} {'耗时(s)':<12}")
    print("-" * 42)
    for k, v in timings.items():
        if k == "total":
            continue
        if isinstance(v, (int, float)):
            print(f"{k:<30} {v:<12.3f}")
    print("-" * 42)
    print(f"{'total':<30} {timings.get('total', 0.0):<12.3f}")
    print()
    print(f"输出目录: {output_dir}")
    print(f"timing.json: {timing_path}")
    print("=" * 70)


if __name__ == "__main__":
    main()
