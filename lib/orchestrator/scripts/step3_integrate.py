# -*- coding: utf-8 -*-
"""
Step3 光谱积分阶段 (全链路整合测试)
功能: 从 FITS 文件头读取 WCS，检测图像星点 + PSF 拟合，WCS 投影到天球坐标，
      查询 Gaia 光谱数据库匹配光谱并积分 F_syn，输出含 PSF 字段的 F_syn JSON
用途: 全链路整合测试的第 3 步，自包含流程:
      1. 从 FITS 头读 WCS (CTYPE/CRVAL/CRPIX/CD/SIP)
      2. StarDetector 检测图像星点
      3. DynamicPSF 批量 PSF 拟合
      4. WCS 将 PSF 星 (cx,cy) 投影到 (ra,dec)
      5. GaiaSpectrumClient 坐标查询匹配 BP/RP 光谱
      6. SpectrumIntegrator 积分 F_syn
      7. 输出 JSON: 每颗星含 PSF 字段 + Gaia 字段, 供 step4 复用 PSF 结果
依赖: argparse, json, logging, numpy, astropy.io.fits;
      spectrum_integrator (GaiaSpectrumClient / CurveLoader / SpectrumIntegrator / narrowband_curves);
      star_detector (StarDetector);
      dynamic_psf (DynamicPSF / DPSFFitResultPy);
      wcs_transform (WCSTransform);
      astro_image_io (ImageReader)
调用示例:
    # 宽带模式
    python step3_integrate.py --image 02_calibrated.fits \\
        --filter-name "Baader R" --qe GSENSE2020BSI --gaia-data GaiaDR3SP \\
        --mag-low 8 --mag-high 16 --output 03_fsyn.json
    # 窄带模式
    python step3_integrate.py --image 02_calibrated.fits \\
        --narrowband-center 656.3 --narrowband-bw 7.0 --narrowband-trans 0.9 \\
        --gaia-data GaiaDR3SP --mag-low 8 --mag-high 16 --output 03_fsyn.json
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import sys
import time

# ============================ OpenMP 线程数控制 ============================
# 必须在 DLL 加载前设置: step3 同时加载 star_detector / dynamic_psf /
# gaia_client / astro_image_io 四个 DLL, 统一限制 OMP 线程数避免堆冲突。
# dynamic_psf 已改为动态链接 libgcc, 与 gaia_client 共享同一 C runtime。
os.environ.setdefault("OMP_NUM_THREADS", "8")

import numpy as np

# ============================ 环境初始化 ============================

# 项目根目录: integration_test/python -> integration_test -> lib -> 项目根
_PROJECT_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))

# MinGW bin (DLL 依赖)
_MINGW_BIN = r"C:\msys64\mingw64\bin"
if _MINGW_BIN not in os.environ.get("PATH", ""):
    os.environ["PATH"] = _MINGW_BIN + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(_MINGW_BIN)
    except (OSError, FileNotFoundError):
        pass

# astro_image_io.dll 所在目录
_ASTRO_IO_DIR = os.path.join(_PROJECT_ROOT, "lib", "astro_image_io")
if _ASTRO_IO_DIR not in os.environ.get("PATH", ""):
    os.environ["PATH"] = _ASTRO_IO_DIR + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(_ASTRO_IO_DIR)
    except (OSError, FileNotFoundError):
        pass

# sys.path 设置
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "photometric_calib",
                                "spectrum_integrator", "python"))
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "photometric_calib",
                                "flux_calibrator", "python"))
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "astro_image_io", "python"))
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "star_detector", "python"))
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "dynamic_psf", "python"))

from gaia_spectrum_client import GaiaSpectrumClient  # noqa: E402
from curve_loader import CurveLoader  # noqa: E402
from integrator import SpectrumIntegrator  # noqa: E402
from narrowband_curves import make_narrowband_curve, BAADER_HA, BAADER_OIII  # noqa: E402
from star_detector import StarDetector  # noqa: E402
from dynamic_psf import DynamicPSF  # noqa: E402
from wcs_transform import WCSTransform  # noqa: E402
from astro_image_io import ImageReader  # noqa: E402

# 日志初始化
_LOG_DIR = os.path.join(_PROJECT_ROOT, "lib", "integration_test", "logs")
os.makedirs(_LOG_DIR, exist_ok=True)
logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s] [%(levelname)s] %(name)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(
            os.path.join(_LOG_DIR, "step3_integrate.log"), encoding="utf-8"),
    ],
)
logger = logging.getLogger(__name__)


# ============================ 分块星点检测 ============================

def _detect_stars_tiled(image: np.ndarray, log: logging.Logger,
                        target_tile: int = 1000,
                        overlap: int = 24) -> list[tuple[float, float]]:
    """分块星点检测, 保证全图均匀覆盖

    将图像划分为若干块, 每块独立调用 StarDetector 检测 (每块独立 maxStars=2000),
    合并后去重 (距离 < 3 像素的视为同一颗星)。

    支持任意宽高比: 块数 = ceil(W/target_tile) * ceil(H/target_tile)。

    Args:
        image: 2D numpy 数组
        log: 日志器
        target_tile: 目标块大小 (像素), 默认 1000
        overlap: 块间重叠 (像素), 默认 24 (约 4*fitRadius)

    Returns:
        list[(x, y)]: 去重后的星点坐标列表 (全局坐标)
    """
    h, w = image.shape[:2]
    log.info("分块检测: 图像 %dx%d, 目标块 %d, 重叠 %d", w, h, target_tile, overlap)

    # 计算分块边界
    n_tiles_x = max(1, (w + target_tile - 1) // target_tile)
    n_tiles_y = max(1, (h + target_tile - 1) // target_tile)
    # 实际块宽: 均分图像 (最后一块可能更宽)
    tile_w = w // n_tiles_x
    tile_h = h // n_tiles_y

    log.info("分块布局: %dx%d = %d 块, 块大小 %dx%d",
             n_tiles_x, n_tiles_y, n_tiles_x * n_tiles_y, tile_w, tile_h)

    # 导入 SDetParamsPy 用于配置每块 maxStars
    from star_detector import StarDetector, SDetParamsPy

    all_coords: list[tuple[float, float]] = []
    detector = StarDetector(params=SDetParamsPy(maxStars=2000))

    for ty in range(n_tiles_y):
        for tx in range(n_tiles_x):
            # 块边界 (带重叠)
            x0 = max(0, tx * tile_w - overlap)
            y0 = max(0, ty * tile_h - overlap)
            x1 = min(w, (tx + 1) * tile_w + overlap) if tx < n_tiles_x - 1 else w
            y1 = min(h, (ty + 1) * tile_h + overlap) if ty < n_tiles_y - 1 else h

            tile = image[y0:y1, x0:x1]
            tile_w_actual = x1 - x0
            tile_h_actual = y1 - y0

            try:
                local_coords = detector.detect(tile)
            except Exception as e:
                log.warning("块 (%d,%d) 检测失败: %s", tx, ty, e)
                continue

            # 转换为全局坐标
            for lx, ly in local_coords:
                gx = lx + x0
                gy = ly + y0
                # 过滤重叠区产生的边界星 (只在块的核心区域保留, 避免重复)
                # 核心区域: 不含 overlap 的部分
                core_x0 = tx * tile_w
                core_y0 = ty * tile_h
                core_x1 = (tx + 1) * tile_w if tx < n_tiles_x - 1 else w
                core_y1 = (ty + 1) * tile_h if ty < n_tiles_y - 1 else h
                if core_x0 <= gx < core_x1 and core_y0 <= gy < core_y1:
                    all_coords.append((float(gx), float(gy)))

            log.debug("块 (%d,%d) [%d,%d]-[%d,%d]: %d 颗 -> 全局 %d 颗",
                      tx, ty, x0, y0, x1, y1, len(local_coords), len(all_coords))

    detector.close()

    # 去重: 距离 < 3 像素的视为同一颗星 (分块边界可能重复检测)
    if len(all_coords) < 2:
        return all_coords

    coords_arr = np.array(all_coords, dtype=np.float64)
    from scipy.spatial import cKDTree
    tree = cKDTree(coords_arr)
    # 查询每颗星的最近邻 (不包括自己)
    dists, _ = tree.query(coords_arr, k=2)
    # dists[:,1] 是最近邻距离
    keep_mask = dists[:, 1] >= 3.0
    deduped = coords_arr[keep_mask]

    log.info("分块检测合并: 原始 %d -> 去重后 %d (移除 %d 重复)",
             len(all_coords), len(deduped), len(all_coords) - len(deduped))

    # 5x5 网格分布统计
    grid = np.zeros((5, 5), dtype=int)
    for i in range(5):
        for j in range(5):
            x_lo, x_hi = j * w / 5, (j + 1) * w / 5
            y_lo, y_hi = i * h / 5, (i + 1) * h / 5
            grid[i, j] = np.sum((deduped[:, 0] >= x_lo) & (deduped[:, 0] < x_hi) &
                                (deduped[:, 1] >= y_lo) & (deduped[:, 1] < y_hi))
    log.info("5x5 网格分布:\n%s", "\n".join(
        "  " + "  ".join(f"{grid[i,j]:4d}" for j in range(5)) for i in range(5)))

    return [(float(x), float(y)) for x, y in deduped]


# ============================ 空间均匀化采样 ============================

def _uniform_sample_stars(matched_pairs: list, img_w: int, img_h: int,
                          grid_n: int = 10, k_per_cell: int = 20,
                          log: logging.Logger = None) -> list:
    """对匹配星做空间均匀化采样

    将图像分成 grid_n×grid_n 网格, 每格保留最多 k_per_cell 颗 PSF flux 最大的星。
    这样可以避免银心等高密度区域星点过度集中, 保证全图均匀覆盖控制点。

    Args:
        matched_pairs: [(psf, gaia_star), ...] 匹配对列表
        img_w, img_h: 图像宽高
        grid_n: 网格划分数 (grid_n×grid_n), 默认 10
        k_per_cell: 每格最多保留的星数, 默认 20
        log: 日志器

    Returns:
        均匀化后的 matched_pairs 列表
    """
    if not matched_pairs:
        return matched_pairs

    n_before = len(matched_pairs)

    # 按网格分组
    cells: dict[tuple[int, int], list] = {}
    for psf, gaia in matched_pairs:
        gx = min(grid_n - 1, max(0, int(psf.cx / img_w * grid_n)))
        gy = min(grid_n - 1, max(0, int(psf.cy / img_h * grid_n)))
        cells.setdefault((gx, gy), []).append((psf, gaia))

    # 每格按 PSF flux 降序排序, 保留前 k_per_cell 颗
    result = []
    cell_counts = []
    for (gx, gy), pairs in sorted(cells.items()):
        # 按 PSF flux 降序 (亮星优先)
        pairs.sort(key=lambda x: float(x[0].flux), reverse=True)
        kept = pairs[:k_per_cell]
        result.extend(kept)
        cell_counts.append(len(kept))

    n_after = len(result)

    # 统计网格分布
    if log:
        grid = np.zeros((grid_n, grid_n), dtype=int)
        for (gx, gy), pairs in sorted(cells.items()):
            grid[gy, gx] = min(len(pairs), k_per_cell)
        log.info(
            "均匀化采样: %dx%d 网格, 每格上限 %d, 原始 %d -> 采样后 %d (移除 %d)",
            grid_n, grid_n, k_per_cell, n_before, n_after, n_before - n_after)
        log.info("网格星数分布 (行=y, 列=x):\n%s",
                 "\n".join("  " + "  ".join(f"{grid[i,j]:3d}" for j in range(grid_n))
                           for i in range(grid_n)))
        # 环形密度统计
        cx_c, cy_c = img_w / 2, img_h / 2
        r_max = (img_w**2 + img_h**2)**0.5 / 2
        cx_arr = np.array([p[0].cx for p in result])
        cy_arr = np.array([p[0].cy for p in result])
        r = np.sqrt((cx_arr - cx_c)**2 + (cy_arr - cy_c)**2)
        log.info("环形密度分布 (采样后):")
        for r_lo_pct, r_hi_pct in [(0,0.2),(0.2,0.4),(0.4,0.6),(0.6,0.8),(0.8,1.0)]:
            mask = (r >= r_lo_pct*r_max) & (r < r_hi_pct*r_max)
            n = mask.sum()
            area = np.pi*((r_hi_pct*r_max)**2 - (r_lo_pct*r_max)**2) / 1e6
            density = n / area if area > 0 else 0
            log.info("  r=[%.1f,%.1f): n=%d, 密度=%.1f/Mpix",
                     r_lo_pct, r_hi_pct, n, density)

    return result


# ============================ Gaia 星等驱动均匀化采样 ============================

def _uniform_sample_gaia(px_arr: np.ndarray, py_arr: np.ndarray, mag_arr: np.ndarray,
                         img_w: int, img_h: int,
                         grid_n: int = 10, k_per_cell: int = 20,
                         log: logging.Logger = None) -> np.ndarray:
    """对 Gaia 星做空间均匀化采样: 每格保留 mag 最亮的 K 颗

    将图像分成 grid_n×grid_n 网格, 每格保留 Gaia G 星等最小 (最亮) 的 k_per_cell 颗星。
    用于在 PSF 拟合前减少候选星数: 从数十万 Gaia 星降到 ~2000 颗。

    Args:
        px_arr: Gaia 星投影到像素的 x 坐标数组
        py_arr: Gaia 星投影到像素的 y 坐标数组
        mag_arr: Gaia G 星等数组
        img_w, img_h: 图像宽高
        grid_n: 网格划分数 (grid_n×grid_n), 默认 10
        k_per_cell: 每格最多保留的星数, 默认 20
        log: 日志器

    Returns:
        选中星在输入数组中的索引数组 (int64)
    """
    n = len(px_arr)
    if n == 0:
        return np.array([], dtype=int)

    cells: dict[tuple[int, int], list[int]] = {}
    for i in range(n):
        gx = min(grid_n - 1, max(0, int(px_arr[i] / img_w * grid_n)))
        gy = min(grid_n - 1, max(0, int(py_arr[i] / img_h * grid_n)))
        cells.setdefault((gx, gy), []).append(i)

    selected: list[int] = []
    for (gx, gy), indices in sorted(cells.items()):
        indices.sort(key=lambda i: mag_arr[i])
        selected.extend(indices[:k_per_cell])

    if log:
        grid = np.zeros((grid_n, grid_n), dtype=int)
        for (gx, gy), indices in sorted(cells.items()):
            grid[gy, gx] = min(len(indices), k_per_cell)
        log.info(
            "均匀化采样: %dx%d 网格, 每格上限 %d, 原始 %d -> 采样后 %d",
            grid_n, grid_n, k_per_cell, n, len(selected))
        log.info("网格星数分布 (行=y, 列=x):\n%s",
                 "\n".join("  " + "  ".join(f"{grid[i,j]:3d}" for j in range(grid_n))
                           for i in range(grid_n)))

    return np.array(selected, dtype=int)


# ============================ 动态星等上限计算 ============================

def _compute_mag_limit(ra: float, dec: float, fov_deg: float,
                       n_stars: int = 2000) -> float:
    """根据银纬和 FOV 面积动态计算星等上限

    移植自 Siril compute_mag_limit_from_position_and_fov()。
    银道面附近星密度高，自动降低星等上限；银极附近星稀疏，自动提高。

    Args:
        ra: 图像中心赤经 (度)
        dec: 图像中心赤纬 (度)
        fov_deg: FOV 直径 (度)
        n_stars: 目标星数, 默认 2000

    Returns:
        极限星等 (不低于 7.0)
    """
    import math
    DEG2RAD = math.pi / 180.0
    RAD2DEG = 180.0 / math.pi

    # RA/Dec -> 银道坐标 (ml, mb)
    l0 = 122.9320 * DEG2RAD
    a0 = 192.8595 * DEG2RAD
    d0 = 27.1284 * DEG2RAD
    ra_rad = ra * DEG2RAD
    dec_rad = dec * DEG2RAD

    ml = (l0 - math.atan2(
        math.cos(dec_rad) * math.sin(ra_rad - a0),
        math.sin(dec_rad) * math.cos(d0) - math.cos(dec_rad) * math.sin(d0) * math.cos(ra_rad - a0)
    )) * RAD2DEG
    mb = math.asin(
        math.sin(dec_rad) * math.sin(d0) + math.cos(dec_rad) * math.cos(d0) * math.cos(ra_rad - a0)
    ) * RAD2DEG
    if ml > 180.0:
        ml -= 360.0

    # FOV 球面面积 (deg^2)
    S = 2.0 * (1.0 - math.cos(0.5 * fov_deg * DEG2RAD)) * 180.0 * 180.0 / math.pi

    # 星等截距
    m0 = 11.68 + 2.66 * math.sin(abs(mb) * DEG2RAD)

    # 星等斜率
    a = 2.36 + (abs(ml) - 90.0) * 0.0073 * (1.0 if abs(ml) < 90.0 else 0.0)
    b = 0.88 - (abs(ml) - 90.0) * 0.0065 * (1.0 if abs(ml) < 90.0 else 0.0)
    s = a + b * math.sin(abs(mb) * DEG2RAD)

    # 极限星等
    limit = m0 + s * (math.log10(float(n_stars) / S) - 2.0)
    return max(limit, 7.0)


# ============================ WCS 从 FITS 头读取 ============================

def read_wcs_from_fits(fits_path: str) -> WCSTransform:
    """从 FITS 文件头读取 WCS 参数，构造 WCSTransform

    读取关键字: CTYPE1/2, CRVAL1/2, CRPIX1/2, CD1_1/CD1_2/CD2_1/CD2_2,
                A_ORDER/B_ORDER, A_i_j/B_i_j (SIP 系数, 下三角 i+j<=order)

    Args:
        fits_path: FITS 文件路径

    Returns:
        WCSTransform 对象

    Raises:
        ValueError: FITS 头缺少必要 WCS 关键字 (CRVAL/CRPIX/CD)
    """
    from astropy.io import fits

    logger.info("从 FITS 头读取 WCS: %s", fits_path)

    with fits.open(fits_path, mode='readonly') as hdul:
        header = hdul[0].header
        ctype1 = str(header.get('CTYPE1', 'RA---TAN'))
        ctype2 = str(header.get('CTYPE2', 'DEC--TAN'))
        crval1 = float(header.get('CRVAL1', 0.0))
        crval2 = float(header.get('CRVAL2', 0.0))
        crpix1 = float(header.get('CRPIX1', 0.0))
        crpix2 = float(header.get('CRPIX2', 0.0))
        cd11 = float(header.get('CD1_1', 0.0))
        cd12 = float(header.get('CD1_2', 0.0))
        cd21 = float(header.get('CD2_1', 0.0))
        cd22 = float(header.get('CD2_2', 0.0))

        # SIP 系数 (下三角: i+j <= order, 存为 A[i*6+j])
        sip_order = int(header.get('A_ORDER', 0))
        sip_a = None
        sip_b = None
        if sip_order > 0:
            sip_a = [0.0] * 36
            sip_b = [0.0] * 36
            for i in range(sip_order + 1):
                for j in range(sip_order + 1 - i):
                    key_a = 'A_%d_%d' % (i, j)
                    key_b = 'B_%d_%d' % (i, j)
                    if key_a in header:
                        sip_a[i * 6 + j] = float(header[key_a])
                    if key_b in header:
                        sip_b[i * 6 + j] = float(header[key_b])
            logger.info("SIP 系数: order=%d, 非零A=%d, 非零B=%d",
                        sip_order,
                        sum(1 for v in sip_a if v != 0.0),
                        sum(1 for v in sip_b if v != 0.0))

    # 校验必要参数
    if cd11 == 0.0 and cd12 == 0.0 and cd21 == 0.0 and cd22 == 0.0:
        raise ValueError("FITS 头缺少 CD 矩阵关键字 (CD1_1/CD1_2/CD2_1/CD2_2)")

    logger.info(
        "WCS 参数: CTYPE=(%s, %s), CRVAL=(%.6f, %.6f), CRPIX=(%.2f, %.2f), "
        "CD=[[%.6e, %.6e], [%.6e, %.6e]], SIP_ORDER=%d",
        ctype1, ctype2, crval1, crval2, crpix1, crpix2,
        cd11, cd12, cd21, cd22, sip_order)

    wcs_transform = WCSTransform(
        crpix1=crpix1, crpix2=crpix2,
        crval1=crval1, crval2=crval2,
        cd11=cd11, cd12=cd12, cd21=cd21, cd22=cd22,
        sip_order=sip_order,
        sip_a=sip_a if sip_order > 0 else None,
        sip_b=sip_b if sip_order > 0 else None,
        ctype1=ctype1, ctype2=ctype2,
    )
    logger.info("WCSTransform 构造完成 (SIP=%s)", "有" if sip_order > 0 else "无")
    return wcs_transform


# ============================ 参数解析 ============================

def parse_args() -> argparse.Namespace:
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="Step3 光谱积分: 图像星点检测+PSF+WCS投影+Gaia光谱查询+积分 -> F_syn JSON",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--image", type=str, required=True,
                        help="输入校准后图像路径 (FITS/XISF)，必填；WCS 从此文件头读取")
    parser.add_argument("--filter-name", type=str, default="",
                        help="宽带滤光片名称 (如 'Baader R')，窄带模式时留空")
    parser.add_argument("--qe", type=str, default=None,
                        help="QE 曲线名称，可选")
    parser.add_argument("--narrowband-center", type=float, default=None,
                        help="窄带滤光片中心波长 (nm)，提供时启用窄带模式")
    parser.add_argument("--narrowband-bw", type=float, default=7.0,
                        help="窄带滤光片带宽 FWHM (nm)，默认 7.0")
    parser.add_argument("--narrowband-trans", type=float, default=1.0,
                        help="窄带滤光片峰值透过率 [0,1]，默认 1.0")
    parser.add_argument("--gaia-data", type=str,
                        default=os.path.join(_PROJECT_ROOT, "GaiaDR3SP"),
                        help="Gaia 数据目录路径，默认项目根目录下的 GaiaDR3SP")
    parser.add_argument("--mag-low", type=float, default=8.0,
                        help="星等下限，默认 8.0")
    parser.add_argument("--mag-high", type=float, default=0.0,
                        help="Gaia 星等上限, 0=自动根据银纬/FOV计算 (默认 0)")
    parser.add_argument("--match-radius-px", type=float, default=3.0,
                        help="Gaia 星-PSF星 像素匹配半径, 默认 3.0 px (WCS 残差一般 <1px)")
    parser.add_argument("--max-psf-stars", type=int, default=2000,
                        help="PSF 拟合星数上限 (均匀化采样后), 默认 2000")
    parser.add_argument("--output", type=str, default="03_fsyn.json",
                        help="输出 F_syn JSON 路径，默认 03_fsyn.json")
    return parser.parse_args()


# ============================ 主流程 ============================

def main():
    """主流程: 读WCS -> Gaia查询 -> 均匀化采样 -> PSF拟合 -> 积分 -> 输出"""
    args = parse_args()
    result = {"success": False, "n_stars": 0, "filter_name": "", "error": ""}

    try:
        t0 = time.time()
        # ---- 1. 从 FITS 头读取 WCS ----
        wcs_transform = read_wcs_from_fits(args.image)

        # ---- 2. 加载图像 ----
        logger.info("加载图像: %s", args.image)
        reader = ImageReader()
        image_data = reader.read(args.image)
        image = image_data.data
        logger.info(
            "图像加载完成: 尺寸=%dx%d, dtype=%s",
            image_data.width, image_data.height, image.dtype)

        # ---- 3. 滤光片曲线加载 ----
        filter_wl = None
        filter_trans = None
        filter_name = ""

        if args.narrowband_center is not None:
            logger.info(
                "窄带模式: center=%.2f nm, bandwidth=%.2f nm, transmittance=%.3f",
                args.narrowband_center, args.narrowband_bw, args.narrowband_trans)
            filter_wl, filter_trans = make_narrowband_curve(
                args.narrowband_center, args.narrowband_bw, args.narrowband_trans)
            filter_name = "Narrowband_%snm" % args.narrowband_center
            logger.info(
                "窄带曲线生成: %d 点, 范围 %.1f~%.1f nm, 名称=%s",
                len(filter_wl), float(filter_wl[0]), float(filter_wl[-1]), filter_name)
        else:
            if not args.filter_name:
                raise ValueError("未提供 --narrowband-center 且 --filter-name 为空，"
                                 "至少需要指定一种滤光片模式")
            logger.info("宽带模式: filter_name=%s", args.filter_name)
            cl = CurveLoader()
            filter_wl, filter_trans = cl.load_filter(args.filter_name)
            filter_name = args.filter_name
            logger.info(
                "宽带曲线加载: %d 点, 范围 %.1f~%.1f nm",
                len(filter_wl), float(filter_wl[0]), float(filter_wl[-1]))

        result["filter_name"] = filter_name

        # ---- 4. QE 曲线加载 ----
        qe_wl, qe_val = (None, None)
        qe_name = args.qe
        if qe_name:
            logger.info("加载 QE 曲线: %s", qe_name)
            cl = CurveLoader()
            qe_wl, qe_val = cl.load_qe(qe_name)
            logger.info(
                "QE 曲线: %d 点, 范围 %.1f~%.1f nm",
                len(qe_wl), float(qe_wl[0]), float(qe_wl[-1]))
        else:
            logger.info("未提供 QE 曲线, Q(λ)=1")

        # ---- 5. Gaia 锥形搜索 ----
        # Gaia 驱动流程: 先查询 Gaia 星表, 投影到像素, 均匀化采样后再 PSF 拟合。
        # 相比先检测 36k 星再 PSF 拟合, 仅拟合 ~2000 颗采样星, 速度提升 ~18 倍。
        t_gaia_start = time.time()
        img_w, img_h = image_data.width, image_data.height
        ra_center_arr, dec_center_arr = wcs_transform.pixel_to_sky_batch(
            np.array([img_w / 2.0]), np.array([img_h / 2.0]))
        ra_center = float(ra_center_arr[0])
        dec_center = float(dec_center_arr[0])

        # 外接圆半径: 计算图像四个角到中心的最大球面角距离
        # 确保锥形搜索覆盖整个 FOV (包括四个角), 而非仅内接圆区域
        corner_xs = np.array([0.0, float(img_w), 0.0, float(img_w)])
        corner_ys = np.array([0.0, 0.0, float(img_h), float(img_h)])
        corner_ra, corner_dec = wcs_transform.pixel_to_sky_batch(corner_xs, corner_ys)

        from astropy.coordinates import angular_separation
        max_sep_rad = 0.0
        for i in range(4):
            sep = angular_separation(
                ra_center * np.pi / 180.0, dec_center * np.pi / 180.0,
                float(corner_ra[i]) * np.pi / 180.0, float(corner_dec[i]) * np.pi / 180.0)
            if sep > max_sep_rad:
                max_sep_rad = sep
        fov_radius_deg = float(max_sep_rad * 180.0 / np.pi)
        # 加 5% 余量防止 WCS 残差导致边缘星遗漏
        cone_radius_deg = fov_radius_deg * 1.05

        # 动态星等上限 (Siril SPCC 策略)
        if args.mag_high <= 0.0:
            fov_diag_deg = fov_radius_deg * 2.0
            mag_high = _compute_mag_limit(ra_center, dec_center, fov_diag_deg)
            logger.info("动态星等上限: %.1f (自动, FOV直径=%.2f deg)", mag_high, fov_diag_deg)
        else:
            mag_high = args.mag_high
            logger.info("固定星等上限: %.1f", mag_high)

        logger.info(
            "Gaia 锥形搜索(外接圆): center=(%.6f, %.6f), radius=%.4f deg, "
            "四角最大角距=%.4f deg, mag=[%.1f, %.1f]",
            ra_center, dec_center, cone_radius_deg, fov_radius_deg,
            args.mag_low, mag_high)

        with GaiaSpectrumClient(args.gaia_data, db_type=2) as client:
            spectrum_wl = client.get_wavelength_array()
            logger.info(
                "光谱波长: %.1f~%.1f nm (%d 点)",
                float(spectrum_wl[0]), float(spectrum_wl[-1]), len(spectrum_wl))

            gaia_stars = client.cone_search_with_spectrum(
                ra_center, dec_center, cone_radius_deg,
                args.mag_low, mag_high)
            logger.info("Gaia 锥形搜索完成: %d 颗星 (%.1fs)", len(gaia_stars), time.time() - t_gaia_start)

            if len(gaia_stars) == 0:
                logger.warning("Gaia 锥形搜索无结果，跳过积分")
                result["success"] = True
                result["n_stars"] = 0
                print(json.dumps(result, ensure_ascii=False))
                return

            # ---- 6. Gaia 星投影到像素 + 过滤图像范围 ----
            gaia_ra_arr = np.array([s.ra for s in gaia_stars], dtype=np.float64)
            gaia_dec_arr = np.array([s.dec for s in gaia_stars], dtype=np.float64)
            gaia_px, gaia_py = wcs_transform.sky_to_pixel_batch(
                gaia_ra_arr, gaia_dec_arr)

            in_img = (gaia_px >= 0) & (gaia_px < img_w) & \
                     (gaia_py >= 0) & (gaia_py < img_h)
            gaia_idx_in = np.where(in_img)[0]
            gaia_px_in = gaia_px[in_img]
            gaia_py_in = gaia_py[in_img]
            gaia_mag_in = np.array(
                [gaia_stars[i].mag_g for i in gaia_idx_in])
            logger.info(
                "Gaia 星投影到像素: 总 %d, 图像范围内 %d",
                len(gaia_stars), len(gaia_idx_in))

            if len(gaia_idx_in) == 0:
                logger.warning("图像范围内无 Gaia 星，跳过积分")
                result["success"] = True
                result["n_stars"] = 0
                print(json.dumps(result, ensure_ascii=False))
                return

            # ---- 7. PSF 拟合: 全部图像范围内 Gaia 星 ----
            # 注: Gaia 星在天球上天然均匀分布, 不需要网格均匀化采样
            # _uniform_sample_gaia() 已注释, 保留函数本体以备后用
            selected_idx = np.arange(len(gaia_idx_in), dtype=int)
            logger.info("PSF 拟合候选: %d 颗 Gaia 星 (全量, 无均匀化采样)", len(selected_idx))

            # ---- 8. PSF 拟合: 只对采样后的 Gaia 星 ----
            t_psf_start = time.time()
            cx_list = gaia_px_in[selected_idx].tolist()
            cy_list = gaia_py_in[selected_idx].tolist()
            logger.info("PSF 批量拟合开始: %d 颗 Gaia 星 (均匀化后)", len(cx_list))
            psf_results = DynamicPSF.fit_batch(image, cx_list, cy_list)
            n_psf_ok = sum(1 for r in psf_results if int(r.status) == 0)
            logger.info("PSF 拟合完成: 成功 %d / %d (%.1fs)", n_psf_ok, len(cx_list), time.time() - t_psf_start)

            if n_psf_ok == 0:
                logger.warning("PSF 拟合全部失败，跳过积分")
                result["success"] = True
                result["n_stars"] = 0
                print(json.dumps(result, ensure_ascii=False))
                return

            # ---- 9. PSF-Gaia 距离过滤 + 积分 ----
            # PSF 拟合后 (cx,cy) 可能偏离 Gaia 投影坐标 (px,py),
            # 距离超过 match_radius_px 的视为拟合到错误目标, 过滤掉。
            valid_pairs = []
            match_dists = []
            for i, psf in enumerate(psf_results):
                if int(psf.status) != 0:
                    continue
                sel = int(selected_idx[i])
                px = float(gaia_px_in[sel])
                py = float(gaia_py_in[sel])
                dist = float(np.sqrt((psf.cx - px) ** 2 + (psf.cy - py) ** 2))
                if dist <= args.match_radius_px:
                    gaia_star = gaia_stars[int(gaia_idx_in[sel])]
                    valid_pairs.append((psf, gaia_star))
                    match_dists.append(dist)

            if match_dists:
                d = np.array(match_dists)
                logger.info(
                    "PSF-Gaia 距离过滤: PSF成功 %d, 距离<=%.1fpx %d, "
                    "距离(px): median=%.2f, mean=%.2f, max=%.2f, <1px=%d(%.1f%%)",
                    n_psf_ok, args.match_radius_px, len(valid_pairs),
                    np.median(d), d.mean(), d.max(),
                    np.sum(d < 1.0), np.sum(d < 1.0) / len(d) * 100)
            else:
                logger.info("PSF-Gaia 距离过滤: PSF成功 %d, 匹配 0", n_psf_ok)

            if len(valid_pairs) == 0:
                logger.warning("PSF-Gaia 距离过滤后无有效星，跳过积分")
                result["success"] = True
                result["n_stars"] = 0
                print(json.dumps(result, ensure_ascii=False))
                return

            # ---- 10. 积分 + 合并 PSF 字段 ----
            integrator = SpectrumIntegrator(
                filter_wl, filter_trans, qe_wl, qe_val, spectrum_wl)

            output_stars = []
            for psf, matched in valid_pairs:
                f_syn = integrator.integrate_star(
                    matched.spectrum, mag_g=matched.mag_g)

                output_stars.append({
                    "source_id": int(getattr(matched, "source_id", 0)),
                    "ra": float(matched.ra),
                    "dec": float(matched.dec),
                    "mag_g": float(matched.mag_g),
                    "f_syn": float(f_syn),
                    "cx": float(psf.cx),
                    "cy": float(psf.cy),
                    "status": int(psf.status),
                    "B": float(psf.B),
                    "A": float(psf.A),
                    "sx": float(psf.sx),
                    "sy": float(psf.sy),
                    "theta": float(psf.theta),
                    "fwhm_x": float(psf.fwhm_x),
                    "fwhm_y": float(psf.fwhm_y),
                    "mad": float(psf.mad),
                    "flux": float(psf.flux),
                    "eccentricity": float(psf.eccentricity),
                })

        # ---- 11. 保存结果 ----
        output_data = {
            "filter_name": filter_name,
            "qe_name": qe_name,
            "wl_step": int(spectrum_wl[1] - spectrum_wl[0]) if len(spectrum_wl) > 1 else 2,
            "spectrum_source": "GaiaDR3SP",
            "n_stars": len(output_stars),
            "ra_center": ra_center,
            "dec_center": dec_center,
            "radius_deg": fov_radius_deg,
            "stars": output_stars,
        }

        out_dir = os.path.dirname(os.path.abspath(args.output))
        if out_dir and not os.path.isdir(out_dir):
            os.makedirs(out_dir, exist_ok=True)
        with open(args.output, "w", encoding="utf-8") as f:
            json.dump(output_data, f, ensure_ascii=False, indent=2)
        logger.info("F_syn 结果已保存: %s (%d 颗星)", args.output, len(output_stars))
        logger.info("Step3 总耗时: %.1fs", time.time() - t0)

        result["success"] = True
        result["n_stars"] = len(output_stars)

    except Exception as e:
        logger.error("光谱积分失败: %s", e, exc_info=True)
        result["error"] = str(e)

    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
