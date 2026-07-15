# -*- coding: utf-8 -*-
"""
可视化报告生成器 - 为指定帧生成全套调试图(6张)
功能: 读取梯度校正产物(04_calibrated_final.fits / 04_quality_report.json /
      03_fsyn.json / 02_wcs.json / 残差CSV)，生成6张调试图用于展示梯度校准效果:
        00_original_preview.png     - 原始Light帧预览
        01_calibrated_preview.png   - 梯度校正后图像预览
        02_M_map_heatmap.png        - 乘性梯度M_map热图
        03_S_map_heatmap.png        - 加性梯度S_map热图
        04_star_match_overlay.png   - 匹配星-Gaia叠加图
        05_residual_spatial.png     - 残差空间分布图
用途: 梯度校准效果可视化、人工审核、调试分析
依赖: numpy, matplotlib; astropy.io.fits (优先) 或 astro_image_io (回退);
      可选: flux_calibrator (GradientFitter/ImageCorrector 用于重新拟合M_map/S_map)
调用:
    # 单帧模式
    python generate_report.py --fits 04_calibrated_final.fits \\
        --original original_light.fts --output report/panel1_Red
    # 批量模式 (扫描results目录, 每panel选RGB三帧)
    python generate_report.py --batch --results-dir testdata/.../results \\
        --output-base testdata/report
    # 指定数据集
    python generate_report.py --batch --results-dir testdata/.../results \\
        --output-base testdata/report --datasets panel1 panel2
注意: M_map/S_map 不保存到文件(仅内存), 本脚本从残差CSV重新拟合曲面来重建
"""

from __future__ import annotations

import argparse
import csv
import json
import logging
import os
import sys
from typing import Optional, Tuple

import numpy as np
import matplotlib
matplotlib.use("Agg")  # 非交互后端, 保证无显示环境也能保存图像
import matplotlib.pyplot as plt
from matplotlib.colors import TwoSlopeNorm

# ============================ 路径常量 ============================

# 本脚本所在目录: .../lib/integration_test/python
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# 项目根目录: integration_test/python -> integration_test -> lib -> 项目根
_PROJECT_ROOT = os.path.normpath(os.path.join(_SCRIPT_DIR, "..", "..", ".."))

# flux_calibrator 模块路径 (用于重新拟合M_map/S_map)
_GRADIENT_ESTIMATOR_PATH = os.path.join(
    _PROJECT_ROOT, "lib", "photometric_calib", "flux_calibrator", "python")
# astro_image_io 模块路径
_ASTRO_IMAGE_IO_PATH = os.path.join(_PROJECT_ROOT, "lib", "astro_image_io", "python")
# plate_solve 模块路径 (用于 Siril MTF 直方图拉伸)
_PLATE_SOLVE_PATH = os.path.join(_PROJECT_ROOT, "lib", "plate_solve", "python")

# 将模块路径加入 sys.path
for _p in (_GRADIENT_ESTIMATOR_PATH, _ASTRO_IMAGE_IO_PATH, _PLATE_SOLVE_PATH):
    if _p not in sys.path:
        sys.path.insert(0, _p)

# ============================ 日志配置 ============================

_LOG_DIR = os.path.join(
    _PROJECT_ROOT, "lib", "integration_test", "logs", "generate_report")
os.makedirs(_LOG_DIR, exist_ok=True)

logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s] [%(levelname)s] %(name)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(
            os.path.join(_LOG_DIR, "generate_report.log"), encoding="utf-8"),
    ],
)
logger = logging.getLogger(__name__)


# ============================ 中文字体配置 ============================

def setup_chinese_font() -> str:
    """配置matplotlib中文字体, 避免方块乱码

    尝试候选字体列表, 找到系统可用的非fallback中文字体。
    设置 font.sans-serif 和 axes.unicode_minus=False。

    Returns:
        实际使用的字体名称
    """
    font_candidates = ['SimHei', 'Microsoft YaHei', 'STSong', 'KaiTi', 'FangSong']
    for font in font_candidates:
        try:
            path = matplotlib.font_manager.findfont(
                matplotlib.font_manager.FontProperties(family=font))
            # findfont 在找不到时会返回 DejaVuSans 的路径, 需排除
            if 'DejaVuSans' not in path:
                matplotlib.rcParams['font.sans-serif'] = [font, 'DejaVu Sans']
                matplotlib.rcParams['axes.unicode_minus'] = False
                logger.info("中文字体配置成功: %s (路径: %s)", font, path)
                return font
        except Exception:
            continue
    # fallback: 全部候选都不可用
    matplotlib.rcParams['font.sans-serif'] = ['DejaVu Sans']
    matplotlib.rcParams['axes.unicode_minus'] = False
    logger.warning("未找到中文字体, 回退到 DejaVu Sans (中文可能显示为方块)")
    return 'DejaVu Sans'


# ============================ 图像读取与拉伸 ============================

def _read_fits(path: str) -> np.ndarray:
    """读取FITS图像数据为2D numpy数组(float64)

    优先使用 astropy.io.fits, 不可用时回退到项目 ImageReader。
    """
    # 优先用 astropy.io.fits
    try:
        from astropy.io import fits
        with fits.open(path, ignore_blank=True) as hdul:
            data = hdul[0].data.astype(np.float64)
        logger.info("astropy读取FITS: %s, shape=%s", path, data.shape)
        return data
    except ImportError:
        pass
    # 回退到项目 ImageReader
    try:
        from astro_image_io import ImageReader
        reader = ImageReader()
        img = reader.read(path)
        data = img.data.astype(np.float64)
        try:
            img.close()
        except Exception:
            pass
        logger.info("ImageReader读取FITS: %s, shape=%s", path, data.shape)
        return data
    except Exception as e:
        raise RuntimeError(f"读取FITS失败({path}): {e}")


def _log_stretch(data: np.ndarray) -> np.ndarray:
    """对数拉伸: log(1 + data), 归一化到 [0, 1]

    适用于天文图像动态范围压缩, 突出暗部细节。
    """
    d = np.nan_to_num(data, nan=0.0, posinf=0.0, neginf=0.0)
    d_positive = np.where(d > 0, d, 0.0)
    stretched = np.log1p(d_positive)
    vmax = float(stretched.max())
    if vmax > 1e-9:
        stretched = stretched / vmax
    return stretched


def _siril_mtf_stretch(data: np.ndarray) -> np.ndarray:
    """Siril MTF 直方图拉伸 (灰度显示用)

    复用 lib/plate_solve/python/visualize_reproject.py 的 siril_autostretch。
    算法: MAD + 1.4826 -> shadows=-2.8σ -> MTF(midtones, target_bg=0.25)
    参考: siril-1.4.3/src/filters/mtf.c find_linked_midtones_balance()

    适配任意 float 输入: 先按百分位映射到 [0, 65535] uint16, 再调用 siril_autostretch。
    返回 uint8 灰度图像 (可直接用 cmap="gray" 显示)。
    """
    from visualize_reproject import siril_autostretch

    d = np.nan_to_num(data, nan=0.0, posinf=0.0, neginf=0.0)
    # 按百分位映射到 [0, 65535] uint16 (兼容 float 归一化后的 04_calibrated_final.fits)
    mask = d > 0
    if np.any(mask):
        v_lo, v_hi = np.percentile(d[mask], [0.5, 99.7])
    else:
        v_lo, v_hi = np.percentile(d, [0.5, 99.7])
    if v_hi - v_lo < 1e-9:
        v_hi = v_lo + 1.0
    d_u16 = np.clip((d - v_lo) / (v_hi - v_lo) * 65535.0, 0, 65535).astype(np.uint16)
    # Siril Auto Stretch
    return siril_autostretch(d_u16)


def _percentile_stretch(data: np.ndarray, lo: float = 1.0, hi: float = 99.5) -> np.ndarray:
    """百分位拉伸归一化到 [0, 1]"""
    d = np.nan_to_num(data, nan=0.0, posinf=0.0, neginf=0.0)
    mask = d > 0
    if np.any(mask):
        v_lo, v_hi = np.percentile(d[mask], [lo, hi])
    else:
        v_lo, v_hi = np.percentile(d, [lo, hi])
    if v_hi - v_lo < 1e-9:
        v_hi = v_lo + 1.0
    return np.clip((d - v_lo) / (v_hi - v_lo), 0.0, 1.0)


def _downsample(data: np.ndarray, factor: int) -> np.ndarray:
    """整数因子降采样(均值), 用于大图预览加速"""
    if factor <= 1:
        return data
    h, w = data.shape
    h2, w2 = (h // factor) * factor, (w // factor) * factor
    d = data[:h2, :w2].reshape(h2 // factor, factor, w2 // factor, factor)
    return d.mean(axis=(1, 3))


# ============================ 梯度图重建 ============================

def _load_quality_report(gradient_result_dir: str) -> dict:
    """加载质量报告JSON (04_quality_report.json)"""
    report_path = os.path.join(gradient_result_dir, "04_quality_report.json")
    if not os.path.isfile(report_path):
        logger.warning("质量报告不存在: %s", report_path)
        return {}
    with open(report_path, "r", encoding="utf-8") as f:
        qr = json.load(f)
    logger.info("质量报告加载: %s (n_matched=%s, mult_skipped=%s, add_skipped=%s)",
                report_path, qr.get("n_matched"), qr.get("mult_skipped"),
                qr.get("add_skipped"))
    return qr


def _read_residuals_csv(csv_path: str) -> Optional[dict]:
    """读取残差CSV, 返回 {x, y, observed, fitted, weight} 数组

    支持 mult_residuals.csv (observed_r/fitted_r) 和 add_residuals.csv (observed_b/fitted_b)
    """
    if not os.path.isfile(csv_path):
        logger.warning("残差CSV不存在: %s", csv_path)
        return None
    x_list, y_list, obs_list, fit_list, w_list = [], [], [], [], []
    with open(csv_path, "r", encoding="utf-8", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if not header:
            return None
        # 列名: x, y, observed_r/observed_b, fitted_r/fitted_b, weight
        for row in reader:
            if len(row) < 5:
                continue
            try:
                x_list.append(float(row[0]))
                y_list.append(float(row[1]))
                obs_list.append(float(row[2]))
                fit_list.append(float(row[3]))
                w_list.append(float(row[4]))
            except (ValueError, IndexError):
                continue
    if not x_list:
        return None
    return {
        "x": np.array(x_list, dtype=np.float64),
        "y": np.array(y_list, dtype=np.float64),
        "observed": np.array(obs_list, dtype=np.float64),
        "fitted": np.array(fit_list, dtype=np.float64),
        "weight": np.array(w_list, dtype=np.float64),
    }


def _rebuild_gradient_maps(gradient_result_dir: str, img_w: int, img_h: int,
                           quality_report: dict) -> Tuple[np.ndarray, np.ndarray]:
    """重建M_map和S_map (从残差CSV重新拟合曲面)

    策略:
        1. 尝试加载 M_map.npy / S_map.npy (如果之前保存过)
        2. 否则从残差CSV重新拟合:
           - mult_skipped=True -> M_map=1.0 (平坦)
           - add_skipped=True -> S_map=0.0 (平坦)
           - 未跳过 -> GradientFitter.fit_multiplicative/fit_additive 重新拟合
        3. 用 ImageCorrector.evaluate_gradient_maps 评估全图

    Returns:
        (M_map, S_map): 均为 (img_h, img_w) float32 数组
    """
    # 1. 尝试加载 .npy 文件
    m_npy = os.path.join(gradient_result_dir, "M_map.npy")
    s_npy = os.path.join(gradient_result_dir, "S_map.npy")
    if os.path.isfile(m_npy) and os.path.isfile(s_npy):
        M_map = np.load(m_npy)
        S_map = np.load(s_npy)
        logger.info("从npy加载梯度图: M_map shape=%s, S_map shape=%s",
                    M_map.shape, S_map.shape)
        return M_map.astype(np.float32), S_map.astype(np.float32)

    # 2. 从残差CSV重新拟合
    mult_skipped = bool(quality_report.get("mult_skipped", False))
    add_skipped = bool(quality_report.get("add_skipped", False))
    max_order = int(quality_report.get("mult_order", 5) or 5)

    logger.info("重建梯度图: mult_skipped=%s, add_skipped=%s, max_order=%d",
                mult_skipped, add_skipped, max_order)

    # 导入梯度拟合模块
    try:
        from gradient_fitter import GradientFitter, GradientSurface
        from image_corrector import ImageCorrector
    except ImportError as e:
        logger.error("无法导入flux_calibrator模块: %s", e)
        # 回退: 返回平坦曲面
        return (np.ones((img_h, img_w), dtype=np.float32),
                np.zeros((img_h, img_w), dtype=np.float32))

    def _identity_surface() -> GradientSurface:
        """恒等曲面: r=0 -> M=1; s=0 -> S=0"""
        return GradientSurface(
            order=1, coeffs=np.zeros(3, dtype=np.float64),
            loocv_error=float("inf"), residual_median=0.0,
            residual_std=0.0, n_used=0, n_rejected=0)

    mult_surface = _identity_surface()
    add_surface = _identity_surface()
    fitter = GradientFitter()

    # 乘性曲面: 从 mult_residuals.csv 重新拟合
    if not mult_skipped:
        # 查找残差CSV (优先 04_residuals_mult.csv, 回退 mult_residuals.csv)
        mult_csv = os.path.join(gradient_result_dir, "04_residuals_mult.csv")
        if not os.path.isfile(mult_csv):
            mult_csv = os.path.join(gradient_result_dir, "mult_residuals.csv")
        mult_data = _read_residuals_csv(mult_csv)
        if mult_data is not None and len(mult_data["x"]) >= 6:
            logger.info("重新拟合乘性曲面: %d 个点", len(mult_data["x"]))
            mult_surface = fitter.fit_multiplicative(
                mult_data["x"], mult_data["y"], mult_data["observed"],
                img_w, img_h, max_order=max_order)
            logger.info("乘性曲面拟合: order=%d, n_used=%d",
                        mult_surface.order, mult_surface.n_used)
        else:
            logger.warning("乘性残差数据不足, 使用恒等曲面")
    else:
        logger.info("乘性校正已跳过(R²<0.02), M_map=1.0")

    # 加性曲面: 从 add_residuals.csv 重新拟合
    if not add_skipped:
        add_csv = os.path.join(gradient_result_dir, "04_residuals_add.csv")
        if not os.path.isfile(add_csv):
            add_csv = os.path.join(gradient_result_dir, "add_residuals.csv")
        add_data = _read_residuals_csv(add_csv)
        if add_data is not None and len(add_data["x"]) >= 6:
            logger.info("重新拟合加性曲面: %d 个点", len(add_data["x"]))
            add_surface = fitter.fit_additive(
                add_data["x"], add_data["y"], add_data["observed"],
                img_w, img_h, max_order=max_order)
            logger.info("加性曲面拟合: order=%d, n_used=%d",
                        add_surface.order, add_surface.n_used)
        else:
            logger.warning("加性残差数据不足, 使用恒等曲面")
    else:
        logger.info("加性校正已跳过(R²<0.02), S_map=0.0")

    # 评估全图梯度图
    corrector = ImageCorrector()
    M_map, S_map = corrector.evaluate_gradient_maps(
        mult_surface, add_surface, img_w, img_h, fitter)
    logger.info("梯度图重建完成: M_map范围=[%.4f, %.4f], S_map范围=[%.4f, %.4f]",
                float(M_map.min()), float(M_map.max()),
                float(S_map.min()), float(S_map.max()))
    return M_map, S_map


# ============================ 匹配星/Gaia星位置加载 ============================

def _load_match_stars(gradient_result_dir: str) -> Optional[dict]:
    """从残差CSV加载匹配星像素坐标(PSF检测星位置)

    Returns:
        {x, y} numpy数组, 或 None
    """
    mult_csv = os.path.join(gradient_result_dir, "04_residuals_mult.csv")
    if not os.path.isfile(mult_csv):
        mult_csv = os.path.join(gradient_result_dir, "mult_residuals.csv")
    data = _read_residuals_csv(mult_csv)
    if data is None:
        return None
    return {"x": data["x"], "y": data["y"]}


def _load_gaia_stars_pixel(fsyn_path: str, wcs_path: str,
                           img_w: int, img_h: int) -> Optional[dict]:
    """加载匹配星(Gaia)像素坐标

    从 03_fsyn.json 读取每颗星的 cx/cy (PSF 检测像素坐标, step3 已写入)。
    不再需要 WCS 转换 (WCS 已在 step3 内部用于投影, 这里直接复用 PSF 像素位置)。

    Args:
        fsyn_path: 03_fsyn.json 路径
        wcs_path: (已废弃, 保留参数兼容)
        img_w, img_h: 图像尺寸 (用于过滤越界星)

    Returns:
        {x, y, mag_g} numpy数组, 或 None
    """
    if not os.path.isfile(fsyn_path):
        logger.warning("F_syn JSON不存在: %s", fsyn_path)
        return None

    with open(fsyn_path, "r", encoding="utf-8") as f:
        fsyn_data = json.load(f)
    stars = fsyn_data.get("stars", [])
    if not stars:
        logger.warning("F_syn中无星: %s", fsyn_path)
        return None

    x_list, y_list, mag_list = [], [], []
    for s in stars:
        try:
            x = float(s.get("cx", 0.0))
            y = float(s.get("cy", 0.0))
            mag = float(s.get("mag_g", 15.0))
            # 过滤超出图像范围的星
            if -50 <= x <= img_w + 50 and -50 <= y <= img_h + 50:
                x_list.append(x)
                y_list.append(y)
                mag_list.append(mag)
        except (ValueError, TypeError):
            continue

    if not x_list:
        logger.warning("F_syn中无有效像素坐标的星")
        return None

    logger.info("匹配星加载: %d颗 -> 有效像素坐标%d颗", len(stars), len(x_list))
    return {
        "x": np.array(x_list, dtype=np.float64),
        "y": np.array(y_list, dtype=np.float64),
        "mag_g": np.array(mag_list, dtype=np.float64),
    }


# ============================ 6张图生成函数 ============================

def plot_original_preview(original_path: str, output_dir: str,
                          frame_name: str) -> str:
    """图1: 原始Light帧预览

    读取原始Light帧FITS数据, Siril MTF直方图拉伸, 灰度显示。
    """
    output_path = os.path.join(output_dir, "00_original_preview.png")
    logger.info("[图1] 生成原始预览: %s -> %s", original_path, output_path)

    data = _read_fits(original_path)
    factor = max(1, max(data.shape) // 1200)
    data_ds = _downsample(data, factor)
    stretched = _siril_mtf_stretch(data_ds)

    fig, ax = plt.subplots(figsize=(10, 8))
    ax.imshow(stretched, cmap="gray", origin="lower", vmin=0, vmax=255)
    ax.set_title(f"{frame_name} - 校准前 (Siril MTF)", fontsize=14)
    ax.axis("off")
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    logger.info("[图1] 保存完成: %s", output_path)
    return output_path


def plot_calibrated_preview(calibrated_path: str, output_dir: str,
                            frame_name: str) -> str:
    """图2: 梯度校正后图像预览

    读取04_calibrated_final.fits, Siril MTF直方图拉伸, 灰度显示。
    """
    output_path = os.path.join(output_dir, "01_calibrated_preview.png")
    logger.info("[图2] 生成校正后预览: %s -> %s", calibrated_path, output_path)

    data = _read_fits(calibrated_path)
    factor = max(1, max(data.shape) // 1200)
    data_ds = _downsample(data, factor)
    stretched = _siril_mtf_stretch(data_ds)

    fig, ax = plt.subplots(figsize=(10, 8))
    ax.imshow(stretched, cmap="gray", origin="lower", vmin=0, vmax=255)
    ax.set_title(f"{frame_name} - 梯度校正后 (Siril MTF)", fontsize=14)
    ax.axis("off")
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    logger.info("[图2] 保存完成: %s", output_path)
    return output_path


def plot_M_map_heatmap(M_map: np.ndarray, output_dir: str,
                       mult_skipped: bool, r_squared: float) -> str:
    """图3: M_map乘性梯度热图

    RdBu_r彩色映射, 中心=1.0。如果R²<0.02跳过校正, 显示平坦曲面并标注。
    """
    output_path = os.path.join(output_dir, "02_M_map_heatmap.png")
    logger.info("[图3] 生成M_map热图 -> %s", output_path)

    # 降采样大图加速
    factor = max(1, max(M_map.shape) // 800)
    M_ds = _downsample(M_map, factor)

    fig, ax = plt.subplots(figsize=(10, 8))
    # 中心化色标: vmin=1- delta, vmax=1+delta
    delta = max(abs(float(M_ds.min()) - 1.0), abs(float(M_ds.max()) - 1.0), 0.01)
    vmin = 1.0 - delta
    vmax = 1.0 + delta
    im = ax.imshow(M_ds, cmap="RdBu_r", origin="lower",
                   vmin=vmin, vmax=vmax)
    cbar = plt.colorbar(im, ax=ax, shrink=0.8)
    cbar.set_label("M_map (乘性梯度)", fontsize=11)

    if mult_skipped:
        ax.set_title(f"乘性梯度 M_map\n(R²={r_squared:.4f} < 0.02, 跳过校正, M_map=1.0)",
                     fontsize=13, color="red")
    else:
        ax.set_title(f"乘性梯度 M_map (R²={r_squared:.4f})", fontsize=13)
    ax.set_xlabel("X (像素)", fontsize=10)
    ax.set_ylabel("Y (像素)", fontsize=10)
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    logger.info("[图3] 保存完成: %s (M_map范围=[%.4f, %.4f])",
                output_path, float(M_map.min()), float(M_map.max()))
    return output_path


def plot_S_map_heatmap(S_map: np.ndarray, output_dir: str,
                       add_skipped: bool, r_squared: float) -> str:
    """图4: S_map加性梯度热图

    RdBu_r彩色映射, 中心=0。如果跳过校正, 显示平坦曲面并标注。
    """
    output_path = os.path.join(output_dir, "03_S_map_heatmap.png")
    logger.info("[图4] 生成S_map热图 -> %s", output_path)

    factor = max(1, max(S_map.shape) // 800)
    S_ds = _downsample(S_map, factor)

    fig, ax = plt.subplots(figsize=(10, 8))
    # 中心化色标: 中心=0
    abs_max = max(abs(float(S_ds.min())), abs(float(S_ds.max())), 0.1)
    norm = TwoSlopeNorm(vmin=-abs_max, vcenter=0.0, vmax=abs_max)
    im = ax.imshow(S_ds, cmap="RdBu_r", origin="lower", norm=norm)
    cbar = plt.colorbar(im, ax=ax, shrink=0.8)
    cbar.set_label("S_map (加性梯度, ADU)", fontsize=11)

    if add_skipped:
        ax.set_title(f"加性梯度 S_map\n(R²={r_squared:.4f} < 0.02, 跳过校正, S_map=0.0)",
                     fontsize=13, color="red")
    else:
        ax.set_title(f"加性梯度 S_map (R²={r_squared:.4f})", fontsize=13)
    ax.set_xlabel("X (像素)", fontsize=10)
    ax.set_ylabel("Y (像素)", fontsize=10)
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    logger.info("[图4] 保存完成: %s (S_map范围=[%.4f, %.4f])",
                output_path, float(S_map.min()), float(S_map.max()))
    return output_path


def plot_star_match_overlay(calibrated_path: str, match_stars: Optional[dict],
                            gaia_stars: Optional[dict], output_dir: str) -> str:
    """图5: 匹配星-Gaia叠加图

    读取校准后FITS图像, 红圈=PSF星, 蓝十字=Gaia星。
    """
    output_path = os.path.join(output_dir, "04_star_match_overlay.png")
    logger.info("[图5] 生成匹配星叠加图 -> %s", output_path)

    data = _read_fits(calibrated_path)
    factor = max(1, max(data.shape) // 1200)
    data_ds = _downsample(data, factor)
    stretched = _percentile_stretch(data_ds)

    fig, ax = plt.subplots(figsize=(12, 9))
    ax.imshow(stretched, cmap="gray", origin="lower")

    n_psf = 0
    n_gaia = 0

    # PSF匹配星: 红圈
    if match_stars is not None and len(match_stars["x"]) > 0:
        x = match_stars["x"] / factor
        y = match_stars["y"] / factor
        ax.scatter(x, y, s=40, facecolors='none', edgecolors='red',
                   linewidths=1.2, label=f'PSF星 (n={len(x)})')
        n_psf = len(x)

    # Gaia星: 蓝十字
    if gaia_stars is not None and len(gaia_stars["x"]) > 0:
        x = gaia_stars["x"] / factor
        y = gaia_stars["y"] / factor
        ax.scatter(x, y, s=30, marker='x', color='cyan',
                   linewidths=0.8, label=f'Gaia星 (n={len(x)})')
        n_gaia = len(x)

    n_total = max(n_psf, n_gaia)
    ax.set_title(f"匹配星叠加 (n={n_total})", fontsize=14)
    ax.set_xlabel("X (像素)", fontsize=10)
    ax.set_ylabel("Y (像素)", fontsize=10)
    ax.legend(loc='upper right', fontsize=10, framealpha=0.8)
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    logger.info("[图5] 保存完成: %s (PSF星=%d, Gaia星=%d)",
                output_path, n_psf, n_gaia)
    return output_path


def plot_residual_spatial(gradient_result_dir: str, output_dir: str) -> str:
    """图6: 残差空间分布图

    从mult_residuals.csv读取残差数据, x-y散点图, 颜色编码残差值。
    残差 = observed_r - fitted_r
    """
    output_path = os.path.join(output_dir, "05_residual_spatial.png")
    logger.info("[图6] 生成残差空间分布图 -> %s", output_path)

    # 查找残差CSV
    mult_csv = os.path.join(gradient_result_dir, "04_residuals_mult.csv")
    if not os.path.isfile(mult_csv):
        mult_csv = os.path.join(gradient_result_dir, "mult_residuals.csv")
    data = _read_residuals_csv(mult_csv)

    if data is None or len(data["x"]) == 0:
        logger.warning("无残差数据, 生成空图")
        fig, ax = plt.subplots(figsize=(10, 8))
        ax.text(0.5, 0.5, "无残差数据", ha='center', va='center',
                fontsize=16, transform=ax.transAxes)
        ax.set_title("残差空间分布 (无数据)", fontsize=14)
        plt.savefig(output_path, dpi=150, bbox_inches="tight", facecolor="white")
        plt.close(fig)
        return output_path

    x = data["x"]
    y = data["y"]
    residual = data["observed"] - data["fitted"]

    fig, ax = plt.subplots(figsize=(10, 8))
    abs_max = max(abs(float(residual.min())), abs(float(residual.max())), 0.001)
    norm = TwoSlopeNorm(vmin=-abs_max, vcenter=0.0, vmax=abs_max)
    sc = ax.scatter(x, y, c=residual, cmap="RdBu_r", norm=norm,
                    s=30, edgecolors='gray', linewidths=0.3, alpha=0.85)
    cbar = plt.colorbar(sc, ax=ax, shrink=0.8)
    cbar.set_label("残差 (observed_r - fitted_r)", fontsize=11)
    ax.set_title(f"残差空间分布 (n={len(x)}, std={float(residual.std()):.4f})",
                 fontsize=13)
    ax.set_xlabel("X (像素)", fontsize=10)
    ax.set_ylabel("Y (像素)", fontsize=10)
    ax.set_aspect('equal', adjustable='box')
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    logger.info("[图6] 保存完成: %s (n=%d, 残差std=%.4f)",
                output_path, len(x), float(residual.std()))
    return output_path


# ============================ 主函数: 单帧报告生成 ============================

def generate_full_report(calibrated_fits_path: str,
                         original_light_path: str,
                         output_dir: str,
                         gradient_result_dir: Optional[str] = None) -> dict:
    """为单帧生成全套调试图(6张)

    Args:
        calibrated_fits_path: 校准后FITS路径 (04_calibrated_final.fits)
        original_light_path: 原始Light帧路径
        output_dir: 输出目录
        gradient_result_dir: 梯度校正产物目录 (含04_quality_report.json等),
            为None时用calibrated_fits_path所在目录

    Returns:
        dict: {success, output_dir, plots: [...], error}
    """
    result = {"success": False, "output_dir": output_dir, "plots": [], "error": ""}

    try:
        # 确定梯度产物目录
        if gradient_result_dir is None:
            gradient_result_dir = os.path.dirname(os.path.abspath(calibrated_fits_path))
        logger.info("梯度产物目录: %s", gradient_result_dir)

        os.makedirs(output_dir, exist_ok=True)

        # 帧名: 用产物目录名
        frame_name = os.path.basename(gradient_result_dir)
        logger.info("帧名: %s", frame_name)

        # 配置中文字体
        setup_chinese_font()

        # 加载质量报告
        qr = _load_quality_report(gradient_result_dir)
        mult_skipped = bool(qr.get("mult_skipped", False))
        add_skipped = bool(qr.get("add_skipped", False))
        mult_r2 = float(qr.get("mult_r_squared", 0.0))
        add_r2 = float(qr.get("add_r_squared", 0.0))

        # 读取校准后图像尺寸 (用于重建梯度图)
        cal_data = _read_fits(calibrated_fits_path)
        img_h, img_w = cal_data.shape
        logger.info("校准后图像尺寸: %dx%d", img_w, img_h)

        # 重建M_map/S_map
        M_map, S_map = _rebuild_gradient_maps(
            gradient_result_dir, img_w, img_h, qr)

        # 加载匹配星/Gaia星位置
        match_stars = _load_match_stars(gradient_result_dir)
        fsyn_path = os.path.join(gradient_result_dir, "03_fsyn.json")
        wcs_path = os.path.join(gradient_result_dir, "02_wcs.json")
        gaia_stars = _load_gaia_stars_pixel(fsyn_path, wcs_path, img_w, img_h)

        # 生成6张图
        plots = []

        # 图1: 原始预览
        try:
            p = plot_original_preview(original_light_path, output_dir, frame_name)
            plots.append(p)
        except Exception as e:
            logger.error("[图1] 生成失败: %s", e)

        # 图2: 校正后预览
        try:
            p = plot_calibrated_preview(calibrated_fits_path, output_dir, frame_name)
            plots.append(p)
        except Exception as e:
            logger.error("[图2] 生成失败: %s", e)

        # 图3: M_map热图
        try:
            p = plot_M_map_heatmap(M_map, output_dir, mult_skipped, mult_r2)
            plots.append(p)
        except Exception as e:
            logger.error("[图3] 生成失败: %s", e)

        # 图4: S_map热图
        try:
            p = plot_S_map_heatmap(S_map, output_dir, add_skipped, add_r2)
            plots.append(p)
        except Exception as e:
            logger.error("[图4] 生成失败: %s", e)

        # 图5: 匹配星叠加
        try:
            p = plot_star_match_overlay(calibrated_fits_path, match_stars,
                                        gaia_stars, output_dir)
            plots.append(p)
        except Exception as e:
            logger.error("[图5] 生成失败: %s", e)

        # 图6: 残差空间分布
        try:
            p = plot_residual_spatial(gradient_result_dir, output_dir)
            plots.append(p)
        except Exception as e:
            logger.error("[图6] 生成失败: %s", e)

        result["success"] = True
        result["plots"] = plots
        logger.info("报告生成完成: %s (%d张图)", output_dir, len(plots))

    except Exception as e:
        logger.error("报告生成失败: %s", e, exc_info=True)
        result["error"] = str(e)

    return result


# ============================ 批量报告生成 ============================

def _find_original_light(frame_id: str, results_dir: str,
                         project_root: str) -> Optional[str]:
    """从test_config.json查找帧的原始Light路径

    在results_dir的上级目录搜索test_config.json, 匹配frame_id获取light_path。
    """
    # 向上查找 test_config.json
    search_dir = results_dir
    for _ in range(5):
        cfg_path = os.path.join(search_dir, "test_config.json")
        if os.path.isfile(cfg_path):
            try:
                with open(cfg_path, "r", encoding="utf-8") as f:
                    cfg = json.load(f)
                for frame in cfg.get("frames", []):
                    if frame.get("id") == frame_id:
                        light_rel = frame.get("light_path", "")
                        if light_rel:
                            light_abs = os.path.join(
                                cfg.get("project_root", project_root), light_rel)
                            if os.path.isfile(light_abs):
                                return light_abs
            except Exception as e:
                logger.warning("读取test_config.json失败: %s", e)
            break
        search_dir = os.path.dirname(search_dir)
        if not search_dir or search_dir == os.path.dirname(search_dir):
            break
    return None


def generate_batch_reports(results_dir: str, output_base_dir: str,
                           datasets: Optional[list] = None) -> dict:
    """批量报告生成

    递归扫描 results_dir 下所有含 04_calibrated_final.fits 的帧目录,
    为每帧生成 6 张调试图。

    支持两种目录结构:
      - <dataset>/<panel>/<filter>/<frame>/04_calibrated_final.fits
      - <dataset>/<filter>/<frame>/04_calibrated_final.fits

    Args:
        results_dir: results 目录
        output_base_dir: 输出根目录 (如 testdata/report)
        datasets: 指定数据集名称列表, 默认全部

    Returns:
        dict: {success, n_frames, n_success, n_fail, details: [...]}
    """
    result = {"success": False, "n_frames": 0, "n_success": 0,
              "n_fail": 0, "details": []}

    if not os.path.isdir(results_dir):
        logger.error("results目录不存在: %s", results_dir)
        result["error"] = f"results目录不存在: {results_dir}"
        return result

    logger.info("批量报告生成: results_dir=%s, output_base=%s, datasets=%s",
                results_dir, output_base_dir, datasets)

    # 递归扫描所有含 04_calibrated_final.fits 的帧目录
    frames = []
    for root, dirs, files in os.walk(results_dir):
        if "04_calibrated_final.fits" in files:
            frames.append({
                "frame_dir": root,
                "calibrated": os.path.join(root, "04_calibrated_final.fits"),
            })

    if not frames:
        logger.warning("未找到任何含04_calibrated_final.fits的帧目录")
        result["error"] = "未找到任何帧目录"
        return result

    # 按数据集过滤
    if datasets:
        filtered = []
        for fr in frames:
            for ds in datasets:
                if ds in fr["frame_dir"]:
                    filtered.append(fr)
                    break
        frames = filtered
        logger.info("过滤后帧数: %d", len(frames))

    total = len(frames)
    n_success = 0
    n_fail = 0

    for i, fr in enumerate(sorted(frames, key=lambda x: x["frame_dir"]), 1):
        frame_dir = fr["frame_dir"]
        calibrated = fr["calibrated"]

        # 帧名: 用帧目录名
        frame_name = os.path.basename(frame_dir)
        logger.info("[%d/%d] 处理帧: %s", i, total, frame_name)

        # 原始图像: 用 01_calibrated.fits 作为"校准前"图像
        original = os.path.join(frame_dir, "01_calibrated.fits")
        if not os.path.isfile(original):
            logger.warning("[%s] 01_calibrated.fits 不存在, 跳过", frame_name)
            n_fail += 1
            result["details"].append({
                "frame_id": frame_name, "success": False,
                "error": "01_calibrated.fits 不存在"})
            continue

        # 输出目录: output_base/<帧名>/
        out_dir = os.path.join(output_base_dir, frame_name)

        r = generate_full_report(calibrated, original, out_dir,
                                 gradient_result_dir=frame_dir)
        if r["success"]:
            n_success += 1
        else:
            n_fail += 1
        result["details"].append({
            "frame_id": frame_name,
            "success": r["success"],
            "output_dir": out_dir,
            "n_plots": len(r["plots"]),
            "error": r.get("error", ""),
        })

    result["success"] = True
    result["n_frames"] = total
    result["n_success"] = n_success
    result["n_fail"] = n_fail
    logger.info("批量报告完成: 共%d帧, 成功%d, 失败%d", total, n_success, n_fail)
    return result


# ============================ 命令行入口 ============================

def parse_args() -> argparse.Namespace:
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="可视化报告生成器: 为梯度校准帧生成6张调试图",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "单帧模式:\n"
            "  python generate_report.py --fits 04_calibrated_final.fits \\\n"
            "      --original original_light.fts --output report/panel1_Red\n\n"
            "批量模式:\n"
            "  python generate_report.py --batch \\\n"
            "      --results-dir testdata/.../results \\\n"
            "      --output-base testdata/report --datasets panel1 panel2\n"
        ),
    )
    # 单帧模式参数
    parser.add_argument("--fits", type=str, default=None,
                        help="单帧模式: 校准后FITS文件路径 (04_calibrated_final.fits)")
    parser.add_argument("--original", type=str, default=None,
                        help="单帧模式: 原始Light帧路径")
    parser.add_argument("--output", type=str, default=None,
                        help="单帧模式: 输出目录")
    parser.add_argument("--gradient-dir", type=str, default=None,
                        help="单帧模式: 梯度产物目录 (含04_quality_report.json等), "
                             "默认用--fits所在目录")
    # 批量模式参数
    parser.add_argument("--batch", action="store_true",
                        help="批量模式: 扫描results目录")
    parser.add_argument("--results-dir", type=str, default=None,
                        help="批量模式: results目录 (含帧子目录)")
    parser.add_argument("--output-base", type=str, default=None,
                        help="批量模式: 输出根目录")
    parser.add_argument("--datasets", type=str, nargs="*", default=None,
                        help="批量模式: 指定panel列表 (如 panel1 panel2), 默认全部")
    return parser.parse_args()


def main():
    """命令行入口"""
    # 确保stdout用UTF-8输出 (Windows下避免GBK乱码)
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    args = parse_args()

    if args.batch:
        # ---- 批量模式 ----
        if not args.results_dir:
            logger.error("批量模式需要 --results-dir")
            print(json.dumps({"success": False, "error": "需要 --results-dir"},
                             ensure_ascii=False))
            return
        if not args.output_base:
            logger.error("批量模式需要 --output-base")
            print(json.dumps({"success": False, "error": "需要 --output-base"},
                             ensure_ascii=False))
            return

        results_dir = os.path.abspath(args.results_dir)
        output_base = os.path.abspath(args.output_base)
        r = generate_batch_reports(results_dir, output_base, args.datasets)
        print(json.dumps(r, ensure_ascii=False, indent=2))

    else:
        # ---- 单帧模式 ----
        if not args.fits:
            logger.error("单帧模式需要 --fits")
            print(json.dumps({"success": False, "error": "需要 --fits"},
                             ensure_ascii=False))
            return
        if not args.original:
            logger.error("单帧模式需要 --original")
            print(json.dumps({"success": False, "error": "需要 --original"},
                             ensure_ascii=False))
            return
        if not args.output:
            logger.error("单帧模式需要 --output")
            print(json.dumps({"success": False, "error": "需要 --output"},
                             ensure_ascii=False))
            return

        calibrated = os.path.abspath(args.fits)
        original = os.path.abspath(args.original)
        output = os.path.abspath(args.output)
        grad_dir = os.path.abspath(args.gradient_dir) if args.gradient_dir else None

        r = generate_full_report(calibrated, original, output, grad_dir)
        print(json.dumps(r, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
