# -*- coding: utf-8 -*-
"""
梯度估算器命令行入口 (Gradient Estimator CLI)
功能: 读取天文图像(FITS/XISF)与光谱积分器输出的 F_syn JSON，结合 WCS 构造，
      调用 GradientEstimator 完成乘性/加性梯度曲面拟合与图像校正，输出校正后
      FITS 图像、质量报告 JSON 与残差 CSV。
用途: photometric_calib 双程序架构的定标端入口；上游输入为 run_integrator.py
      生成的 F_syn JSON，本脚本完成星-图匹配、梯度拟合、图像归一化全流程。
依赖: argparse, logging, numpy, json;
      同目录 fsyn_loader / estimator / wcs_transform;
      astro_image_io (ImageReader / FITSWriter, 封装 C++ DLL)
调用示例:
    python run_estimator.py --image img.fits --fsyn f_syn.json \\
        --output calibrated.fits
    python run_estimator.py --image img.fits --fsyn f_syn.json \\
        --wcs-json wcs.json --output cal.fits --report qr.json \\
        --match-radius 3.0 --outlier-sigma 3.0 --max-order 5
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import sys

# 日志初始化
logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s] [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger(__name__)

# 确保能导入同目录下的依赖模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# 导入 astro_image_io (回溯3级: gradient_estimator/python -> photometric_calib -> lib -> astro_image_io/python)
_AIO_PATH = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "..",
    "astro_image_io", "python"))
if _AIO_PATH not in sys.path:
    sys.path.insert(0, _AIO_PATH)

from fsyn_loader import FSynLoader  # noqa: E402
from estimator import GradientEstimator  # noqa: E402
from wcs_transform import WCSTransform  # noqa: E402
from astro_image_io import ImageReader, FITSWriter  # noqa: E402


def parse_args() -> argparse.Namespace:
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="梯度估算器: 基于匹配星拟合空间梯度并校正天文图像",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--image", type=str, required=True,
                        help="输入图像路径 (FITS 或 XISF)，必填")
    parser.add_argument("--fsyn", type=str, required=True,
                        help="F_syn JSON 文件路径 (光谱积分器输出)，必填")
    parser.add_argument("--output", type=str, default="calibrated.fits",
                        help="输出校正后 FITS 图像路径，默认 calibrated.fits")
    parser.add_argument("--report", type=str, default="quality_report.json",
                        help="输出质量报告 JSON 路径，默认 quality_report.json")
    parser.add_argument("--residual-dir", type=str, default="logs",
                        help="残差 CSV 输出目录，默认 logs/")
    parser.add_argument("--match-radius", type=float, default=3.0,
                        help="星-图匹配半径 (像素)，默认 3.0")
    parser.add_argument("--outlier-sigma", type=float, default=3.0,
                        help="离群点 sigma 阈值，默认 3.0")
    parser.add_argument("--max-order", type=int, default=5,
                        help="多项式最大阶数，默认 5")
    parser.add_argument("--wcs-json", type=str, default=None,
                        help="WCS JSON 文件路径 (plate_solve 结果)，可选；图像无 WCS 时使用")
    return parser.parse_args()


def build_wcs(image_data, wcs_json_path=None) -> WCSTransform:
    """从图像 FITS 头或 WCS JSON 构造 WCSTransform

    Args:
        image_data: ImageData 对象 (astro_image_io)，提供 has_wcs / wcs 属性
        wcs_json_path: 可选，plate_solve 结果 JSON 路径；图像无 WCS 时使用

    Returns:
        WCSTransform 对象

    Raises:
        ValueError: 图像无 WCS 且未提供 wcs_json_path
        FileNotFoundError: wcs_json_path 文件不存在
        KeyError: WCS JSON 缺少必需字段
    """
    if image_data.has_wcs:
        wcs = image_data.wcs
        logger.info(
            "从图像 FITS 头构造 WCS: CRVAL=(%.6f, %.6f), CRPIX=(%.2f, %.2f)",
            wcs.crval1, wcs.crval2, wcs.crpix1, wcs.crpix2)
        return WCSTransform(
            crpix1=wcs.crpix1, crpix2=wcs.crpix2,
            crval1=wcs.crval1, crval2=wcs.crval2,
            cd11=wcs.cd1_1, cd12=wcs.cd1_2,
            cd21=wcs.cd2_1, cd22=wcs.cd2_2,
        )
    elif wcs_json_path:
        logger.info("图像无 WCS，从 JSON 加载: %s", wcs_json_path)
        with open(wcs_json_path, "r", encoding="utf-8") as f:
            wcs_data = json.load(f)
        logger.info(
            "WCS JSON: CRVAL=(%.6f, %.6f), CRPIX=(%.2f, %.2f)",
            float(wcs_data["crval1"]), float(wcs_data["crval2"]),
            float(wcs_data["crpix1"]), float(wcs_data["crpix2"]))
        return WCSTransform(
            crpix1=wcs_data["crpix1"], crpix2=wcs_data["crpix2"],
            crval1=wcs_data["crval1"], crval2=wcs_data["crval2"],
            cd11=wcs_data["cd1_1"], cd12=wcs_data["cd1_2"],
            cd21=wcs_data["cd2_1"], cd22=wcs_data["cd2_2"],
        )
    else:
        raise ValueError("图像无 WCS 且未提供 --wcs-json 参数")


def main():
    """主流程: 解析参数 -> 加载图像 -> 加载F_syn -> 构造WCS -> 梯度校正 -> 输出"""
    args = parse_args()

    # ---- 1. 加载图像 ----
    logger.info("加载图像: %s", args.image)
    reader = ImageReader()
    image_data = reader.read(args.image)
    image = image_data.data  # numpy 2D
    logger.info(
        "图像加载完成: 尺寸=%dx%d, dtype=%s, 有WCS=%s",
        image_data.width, image_data.height, image.dtype, image_data.has_wcs)

    # ---- 2. 加载 F_syn JSON ----
    logger.info("加载 F_syn 结果: %s", args.fsyn)
    gaia_stars = FSynLoader.load(args.fsyn)
    logger.info("F_syn 加载完成: 有效星=%d 颗", len(gaia_stars))

    # ---- 3. 构造 WCS ----
    wcs_transform = build_wcs(image_data, args.wcs_json)

    # ---- 4. 创建估算器并校准 ----
    log_dir = args.residual_dir
    os.makedirs(log_dir, exist_ok=True)
    estimator = GradientEstimator(
        log_dir=log_dir,
        match_radius_px=args.match_radius,
        outlier_sigma=args.outlier_sigma,
        max_order=args.max_order,
    )
    logger.info(
        "开始梯度校正: match_radius=%.2f px, outlier_sigma=%.2f, max_order=%d",
        args.match_radius, args.outlier_sigma, args.max_order)
    result = estimator.calibrate(image, gaia_stars, wcs_transform)

    # ---- 5. 输出校正后 FITS ----
    logger.info("写入校正后 FITS: %s", args.output)
    writer = FITSWriter()
    writer.write(result["image_calibrated"], args.output)

    # ---- 6. 输出质量报告 JSON ----
    logger.info("写入质量报告: %s", args.report)
    report_dir = os.path.dirname(os.path.abspath(args.report))
    os.makedirs(report_dir, exist_ok=True)
    with open(args.report, "w", encoding="utf-8") as f:
        json.dump(result["quality_report"], f, ensure_ascii=False, indent=2)

    # ---- 7. 残差 CSV (estimator 内部已写入 log_dir) ----
    logger.info("残差 CSV 输出目录: %s", log_dir)

    print(f"校准完成: {result['n_matched']} 颗星匹配, scale={result['scale_factor']:.6e}")
    print(f"校正图像: {args.output}")
    print(f"质量报告: {args.report}")


if __name__ == "__main__":
    main()
