# -*- coding: utf-8 -*-
"""
可视化校正结果 (全链路整合测试辅助工具)
功能: 读取 01_calibrated.fits(校正前) 与 04_calibrated_final.fits(校正后)，
      生成对比预览图(原图 / 校正后 / 差异 / 直方图)供人工审核。
用途: SubTask 9.5 输出校正图供用户人工审核。
依赖: numpy, matplotlib, astropy.io.fits (或回退到 astro_image_io)
调用:
    python visualize_result.py --frame-dir <results/panel1_Red> [--output <png路径>]
    # 默认输出 <frame_dir>/04_preview.png
"""

from __future__ import annotations

import os
import sys
import argparse
import logging

import numpy as np
import matplotlib
matplotlib.use("Agg")  # 非交互后端
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

# 项目根目录
_PROJECT_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "astro_image_io", "python"))

logging.basicConfig(level=logging.INFO,
                    format="[%(asctime)s] [%(levelname)s] %(message)s",
                    datefmt="%Y-%m-%d %H:%M:%S")
logger = logging.getLogger(__name__)


def _read_fits(path: str) -> np.ndarray:
    """读取 FITS 图像数据为 2D numpy 数组"""
    # 优先用 astropy.io.fits
    try:
        from astropy.io import fits
        with fits.open(path, ignore_blank=True) as hdul:
            data = hdul[0].data.astype(np.float64)
        logger.info("astropy 读取: %s, shape=%s, dtype=%s", path, data.shape, data.dtype)
        return data
    except ImportError:
        pass
    # 回退到项目 ImageReader
    from astro_image_io import ImageReader
    reader = ImageReader()
    img = reader.read(path)
    data = img.data.astype(np.float64)
    img.close()
    logger.info("ImageReader 读取: %s, shape=%s", path, data.shape)
    return data


def _percentile_stretch(data: np.ndarray, lo: float = 1.0, hi: float = 99.5) -> np.ndarray:
    """百分位拉伸归一化到 [0,1]"""
    d = np.nan_to_num(data, nan=0.0, posinf=0.0, neginf=0.0)
    v_lo, v_hi = np.percentile(d[d > 0] if np.any(d > 0) else d, [lo, hi])
    if v_hi - v_lo < 1e-9:
        v_hi = v_lo + 1.0
    stretched = np.clip((d - v_lo) / (v_hi - v_lo), 0.0, 1.0)
    return stretched


def _downsample(data: np.ndarray, factor: int) -> np.ndarray:
    """整数因子降采样(均值)"""
    if factor <= 1:
        return data
    h, w = data.shape
    h2, w2 = (h // factor) * factor, (w // factor) * factor
    d = data[:h2, :w2].reshape(h2 // factor, factor, w2 // factor, factor)
    return d.mean(axis=(1, 3))


def generate_preview(frame_dir: str, output_path: str | None = None) -> str:
    """生成校正前后对比预览图

    Args:
        frame_dir: 帧输出目录(含 01_calibrated.fits 和 04_calibrated_final.fits)
        output_path: 输出 PNG 路径(可选,默认 <frame_dir>/04_preview.png)

    Returns:
        输出 PNG 路径
    """
    before_path = os.path.join(frame_dir, "01_calibrated.fits")
    after_path = os.path.join(frame_dir, "04_calibrated_final.fits")
    report_path = os.path.join(frame_dir, "04_quality_report.json")

    if not os.path.isfile(before_path):
        raise FileNotFoundError(f"校正前图像不存在: {before_path}")
    if not os.path.isfile(after_path):
        raise FileNotFoundError(f"校正后图像不存在: {after_path}")

    if output_path is None:
        output_path = os.path.join(frame_dir, "04_preview.png")

    # 读取图像
    logger.info("读取校正前图像: %s", before_path)
    before = _read_fits(before_path)
    logger.info("读取校正后图像: %s", after_path)
    after = _read_fits(after_path)

    # 降采样(4500x3600 -> 约 900x720, factor=5)
    factor = max(1, max(before.shape) // 1000)
    logger.info("降采样因子: %d", factor)
    before_ds = _downsample(before, factor)
    after_ds = _downsample(after, factor)
    diff_ds = after_ds - before_ds

    # 拉伸
    before_norm = _percentile_stretch(before_ds)
    after_norm = _percentile_stretch(after_ds)
    # 差异图用对称百分位
    diff_lo, diff_hi = np.percentile(diff_ds, [1.0, 99.0])
    abs_max = max(abs(diff_lo), abs(diff_hi), 1e-9)
    diff_norm = np.clip(diff_ds / abs_max, -1.0, 1.0)

    # 读取质量报告
    import json
    qr = {}
    if os.path.isfile(report_path):
        with open(report_path, "r", encoding="utf-8") as f:
            qr = json.load(f)

    # 绘图
    fig = plt.figure(figsize=(16, 10))
    gs = GridSpec(2, 3, figure=fig, hspace=0.25, wspace=0.2)

    # 原图
    ax1 = fig.add_subplot(gs[0, 0])
    ax1.imshow(before_norm, cmap="gray", origin="lower")
    ax1.set_title("Before (01_calibrated)", fontsize=11)
    ax1.axis("off")

    # 校正后
    ax2 = fig.add_subplot(gs[0, 1])
    ax2.imshow(after_norm, cmap="gray", origin="lower")
    ax2.set_title("After (04_calibrated_final)", fontsize=11)
    ax2.axis("off")

    # 差异
    ax3 = fig.add_subplot(gs[0, 2])
    ax3.imshow(diff_norm, cmap="RdBu_r", origin="lower", vmin=-1, vmax=1)
    ax3.set_title("Difference (After - Before)", fontsize=11)
    ax3.axis("off")

    # 直方图(对数y轴)
    ax4 = fig.add_subplot(gs[1, :])
    b_flat = before_ds[before_ds > 0].ravel()
    a_flat = after_ds[after_ds > 0].ravel()
    bins = np.linspace(
        np.percentile(np.concatenate([b_flat, a_flat]), 0.5),
        np.percentile(np.concatenate([b_flat, a_flat]), 99.5),
        200)
    ax4.hist(b_flat, bins=bins, alpha=0.5, color="blue", label="Before", density=True)
    ax4.hist(a_flat, bins=bins, alpha=0.5, color="red", label="After", density=True)
    ax4.set_yscale("log")
    ax4.set_xlabel("ADU", fontsize=10)
    ax4.set_ylabel("Density (log)", fontsize=10)
    ax4.legend(fontsize=9)
    ax4.set_title("Pixel Value Distribution", fontsize=11)

    # 标题与质量信息
    frame_id = os.path.basename(frame_dir)
    title = f"Gradient Calibration Preview - {frame_id}"
    if qr:
        title += (f"\nmatched={qr.get('n_matched', '?')}  "
                  f"scale={qr.get('scale_factor', 0):.6f}  "
                  f"mult_order={qr.get('mult_order', '?')}  "
                  f"mult_res_std={qr.get('mult_residual_std', 0):.4f}  "
                  f"add_order={qr.get('add_order', '?')}  "
                  f"add_res_std={qr.get('add_residual_std', 0):.2f}")
    fig.suptitle(title, fontsize=12, y=0.98)

    plt.savefig(output_path, dpi=120, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    logger.info("预览图已保存: %s", output_path)
    return output_path


def main():
    parser = argparse.ArgumentParser(
        description="生成校正前后对比预览图",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--frame-dir", required=True,
                        help="帧输出目录(含 01_calibrated.fits 和 04_calibrated_final.fits)")
    parser.add_argument("--output", default=None,
                        help="输出 PNG 路径(默认 <frame_dir>/04_preview.png)")
    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    out = generate_preview(args.frame_dir, args.output)
    print(f"预览图: {out}")


if __name__ == "__main__":
    main()
