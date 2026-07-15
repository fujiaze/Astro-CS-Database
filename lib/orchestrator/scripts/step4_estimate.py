# -*- coding: utf-8 -*-
"""
Step4 梯度估算阶段 (全链路整合测试)
功能: 调用 GradientEstimator 梯度拟合 + 图像校正，输出校正 FITS + 质量报告
用途: 全链路整合测试的第 4 步，直接从 FITS 文件头读取 WCS 参数构造 WCSTransform，
      加载 step3 输出的 F_syn JSON (含 PSF 拟合结果) 与校准后图像，
      执行星-图匹配、乘性/加性梯度曲面拟合、图像校正归一化，
      输出最终校正 FITS 图像、质量报告 JSON 与残差 CSV
注意: WCS 直接从 FITS 文件头读取 (CTYPE/CRVAL/CRPIX/CD/SIP)，不依赖中间 JSON 文件
依赖: argparse, json, logging, numpy, shutil, astropy.io.fits;
      flux_calibrator (GradientEstimator / WCSTransform);
      astro_image_io (ImageReader / FITSWriter);
      dynamic_psf (DPSFFitResultPy)
调用示例:
    python step4_estimate.py --image 02_calibrated.fits --fsyn 03_fsyn.json \\
        --output 04_calibrated_final.fits \\
        --report 04_quality_report.json \\
        --match-radius 3.0 --outlier-sigma 3.0 --max-order 5
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import shutil
import sys

# ============================ OpenMP 线程数控制 ============================
# 必须在 DLL 加载前设置: step4 同时加载 flux_calibrator / astro_image_io /
# dynamic_psf 等多个 DLL, 统一限制 OMP 线程数避免堆冲突。
os.environ.setdefault("OMP_NUM_THREADS", "8")

import numpy as np

# 项目根目录: integration_test/python -> integration_test -> lib -> 项目根
_PROJECT_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "photometric_calib",
                                "flux_calibrator", "python"))
# astro_image_io 路径 (GradientEstimator 内部需要)
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "astro_image_io", "python"))
# dynamic_psf 路径 (加载 step3 输出的 PSF 结果)
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "dynamic_psf", "python"))

from estimator import GradientEstimator  # noqa: E402
from wcs_transform import WCSTransform  # noqa: E402
from astro_image_io import ImageReader, FITSWriter  # noqa: E402
from dynamic_psf import DPSFFitResultPy  # noqa: E402

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
            os.path.join(_LOG_DIR, "step4_estimate.log"), encoding="utf-8"),
    ],
)
logger = logging.getLogger(__name__)


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


def load_fsyn_with_psf(json_path: str):
    """从 step3 输出的 F_syn JSON 加载 Gaia 星列表和 PSF 拟合结果

    step3 输出的 JSON 中每颗星同时包含 Gaia 字段 (ra/dec/mag_g/f_syn/source_id)
    和 PSF 字段 (cx/cy/status/B/A/flux/fwhm_x/fwhm_y 等)。

    Args:
        json_path: step3 输出的 F_syn JSON 路径

    Returns:
        tuple: (gaia_stars, psf_results)
            - gaia_stars: list[dict], 每项含 ra/dec/mag_g/f_syn/source_id (f_syn>0 且 status==0)
            - psf_results: list[DPSFFitResultPy], PSF 拟合成功的星 (status==0)
    """
    logger.info("加载 F_syn + PSF 结果: %s", json_path)
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    raw_stars = data.get("stars", [])
    gaia_stars = []
    psf_results = []
    n_invalid_fsyn = 0
    n_psf_failed = 0

    for s in raw_stars:
        status = int(s.get("status", -1))
        f_syn = float(s.get("f_syn", 0.0))

        # PSF 拟合失败的星跳过
        if status != 0:
            n_psf_failed += 1
            continue

        # 构造 DPSFFitResultPy
        psf_results.append(DPSFFitResultPy(
            status=status,
            B=float(s.get("B", 0.0)),
            A=float(s.get("A", 0.0)),
            cx=float(s.get("cx", 0.0)),
            cy=float(s.get("cy", 0.0)),
            sx=float(s.get("sx", 0.0)),
            sy=float(s.get("sy", 0.0)),
            theta=float(s.get("theta", 0.0)),
            fwhm_x=float(s.get("fwhm_x", 0.0)),
            fwhm_y=float(s.get("fwhm_y", 0.0)),
            mad=float(s.get("mad", 0.0)),
            flux=float(s.get("flux", 0.0)),
            eccentricity=float(s.get("eccentricity", 0.0)),
        ))

        # f_syn <= 0 的星不加入 gaia_stars (无法用于梯度拟合)
        if f_syn <= 0.0:
            n_invalid_fsyn += 1
            continue

        gaia_stars.append({
            "ra": float(s["ra"]),
            "dec": float(s["dec"]),
            "mag_g": float(s["mag_g"]),
            "f_syn": f_syn,
            "source_id": int(s["source_id"]),
        })

    filter_name = data.get("filter_name", "未知")
    logger.info(
        "F_syn+PSF 加载完成: 滤光片=%s, 原始=%d, PSF成功=%d, "
        "f_syn>0=%d, PSF失败=%d, f_syn无效=%d",
        filter_name, len(raw_stars), len(psf_results), len(gaia_stars),
        n_psf_failed, n_invalid_fsyn)
    return gaia_stars, psf_results


def parse_args() -> argparse.Namespace:
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="Step4 梯度估算: 梯度拟合 + 图像校正 -> 校正 FITS + 质量报告",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--image", type=str, required=True,
                        help="输入校准后图像路径 (FITS/XISF)，必填；WCS 从此文件头读取")
    parser.add_argument("--fsyn", type=str, required=True,
                        help="F_syn JSON 文件路径 (step3 输出，含 PSF 拟合结果)，必填")
    parser.add_argument("--output", type=str, default="04_calibrated_final.fits",
                        help="输出校正后 FITS 路径，默认 04_calibrated_final.fits")
    parser.add_argument("--report", type=str, default="04_quality_report.json",
                        help="输出质量报告 JSON 路径，默认 04_quality_report.json")
    parser.add_argument("--match-radius", type=float, default=3.0,
                        help="星-图匹配半径 (像素)，默认 3.0")
    parser.add_argument("--outlier-sigma", type=float, default=3.0,
                        help="离群点 sigma 阈值，默认 3.0")
    parser.add_argument("--max-order", type=int, default=5,
                        help="多项式最大阶数，默认 5")
    return parser.parse_args()


def main():
    """主流程: 从FITS头读WCS -> 加载F_syn+PSF -> 加载图像 -> 梯度校正 -> 输出"""
    args = parse_args()
    result = {"success": False, "n_matched": 0, "scale_factor": 0.0, "error": ""}

    try:
        # ---- 1. 从 FITS 文件头读取 WCS ----
        wcs_transform = read_wcs_from_fits(args.image)

        # ---- 2. 加载 F_syn + PSF 结果 ----
        gaia_stars, psf_results = load_fsyn_with_psf(args.fsyn)

        if len(gaia_stars) == 0:
            raise ValueError("F_syn JSON 中无有效星 (f_syn>0 且 PSF status==0)")
        if len(psf_results) == 0:
            raise ValueError("F_syn JSON 中无 PSF 拟合成功的星")

        # ---- 3. 加载图像 ----
        logger.info("加载图像: %s", args.image)
        reader = ImageReader()
        image_data = reader.read(args.image)
        image = image_data.data  # numpy 2D
        logger.info(
            "图像加载完成: 尺寸=%dx%d, dtype=%s",
            image_data.width, image_data.height, image.dtype)

        # ---- 4. 梯度校正 (传入 psf_results 避免重复检测) ----
        log_dir = os.path.dirname(os.path.abspath(args.output))
        if log_dir:
            os.makedirs(log_dir, exist_ok=True)
        logger.info("残差 CSV 输出目录: %s", log_dir)

        estimator = GradientEstimator(
            log_dir=log_dir,
            match_radius_px=args.match_radius,
            outlier_sigma=args.outlier_sigma,
            max_order=args.max_order,
        )
        logger.info(
            "开始梯度校正: match_radius=%.2f px, outlier_sigma=%.2f, max_order=%d, "
            "Gaia星=%d, PSF星=%d",
            args.match_radius, args.outlier_sigma, args.max_order,
            len(gaia_stars), len(psf_results))

        cal_result = estimator.calibrate(
            image, gaia_stars, wcs_transform, psf_results=psf_results)
        n_matched = cal_result["n_matched"]
        scale_factor = cal_result["scale_factor"]
        logger.info(
            "梯度校正完成: n_matched=%d, n_excluded=%d, scale=%.6e",
            n_matched, cal_result["n_excluded"], scale_factor)

        # ---- 5. 输出校正后 FITS ----
        logger.info("写入校正后 FITS: %s", args.output)
        writer = FITSWriter()
        writer.write(cal_result["image_calibrated"], args.output)
        logger.info(
            "校正图像写入完成: dtype=%s, 范围=[%.4f, %.4f]",
            cal_result["image_calibrated"].dtype,
            float(cal_result["image_calibrated"].min()),
            float(cal_result["image_calibrated"].max()))

        # ---- 6. 输出质量报告 JSON ----
        logger.info("写入质量报告: %s", args.report)
        report_dir = os.path.dirname(os.path.abspath(args.report))
        if report_dir:
            os.makedirs(report_dir, exist_ok=True)
        with open(args.report, "w", encoding="utf-8") as f:
            json.dump(cal_result["quality_report"], f, ensure_ascii=False, indent=2)
        logger.info("质量报告写入完成")

        # ---- 7. 残差 CSV 重命名 ----
        # estimator 内部在 log_dir 下生成 mult_residuals.csv 和 add_residuals.csv
        # 重命名为 04_residuals_mult.csv 和 04_residuals_add.csv
        mult_src = os.path.join(log_dir, "mult_residuals.csv")
        add_src = os.path.join(log_dir, "add_residuals.csv")
        mult_dst = os.path.join(log_dir, "04_residuals_mult.csv")
        add_dst = os.path.join(log_dir, "04_residuals_add.csv")
        if os.path.isfile(mult_src):
            shutil.copy2(mult_src, mult_dst)
            logger.info("乘性残差 CSV: %s -> %s", mult_src, mult_dst)
        if os.path.isfile(add_src):
            shutil.copy2(add_src, add_dst)
            logger.info("加性残差 CSV: %s -> %s", add_src, add_dst)

        result["success"] = True
        result["n_matched"] = int(n_matched)
        result["scale_factor"] = float(scale_factor)

    except Exception as e:
        logger.error("梯度估算失败: %s", e, exc_info=True)
        result["error"] = str(e)

    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
