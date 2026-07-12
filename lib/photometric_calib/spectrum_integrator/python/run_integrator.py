# -*- coding: utf-8 -*-
"""
光谱积分器命令行入口 (Spectrum Integrator CLI)
功能: 使用 Gaia DR3SP 真实 BP/RP 光谱，结合滤光片透过率与可选 CCD QE，
      批量计算参考星合成流量 F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ，输出 JSON 结果。
用途: 测光定标流程的参考星理论流量计算入口，支持锥形搜索 Gaia 光谱库、
      按滤光片/QE 配置积分、列出可用曲线、运行时注入窄带方波滤光片曲线。
依赖: argparse, logging, numpy, gaia_spectrum_client, curve_loader, integrator,
      narrowband_curves (integration_test 模块)
调用示例:
    python run_integrator.py --list-filters
    python run_integrator.py --list-qe
    python run_integrator.py --ra 266.4168 --dec -28.9833 --radius 0.5 \
        --mag-low 8 --mag-high 16 --filter "Baader R" --qe "GSENSE2020BSI" \
        --output f_syn_results.json
    # 窄带方波滤光片运行时注入 (跳过 CurveLoader, 用 --filter 值作结果标签)
    python run_integrator.py --ra 266.4168 --dec -28.9833 --radius 0.5 \
        --filter "H-alpha" --narrowband-center 656.3 --narrowband-bw 7.0 \
        --narrowband-trans 1.0 --output f_syn_ha.json
"""

from __future__ import annotations

import argparse
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

# 确保能导入 integration_test 模块下的 narrowband_curves
_INTEGRATION_TEST_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "integration_test", "python")
)
if _INTEGRATION_TEST_DIR not in sys.path:
    sys.path.insert(0, _INTEGRATION_TEST_DIR)

from gaia_spectrum_client import GaiaSpectrumClient  # noqa: E402
from curve_loader import CurveLoader  # noqa: E402
from integrator import SpectrumIntegrator  # noqa: E402
from narrowband_curves import make_narrowband_curve  # noqa: E402


def _default_gaia_data() -> str:
    """从当前脚本位置回溯到项目根目录，返回 GaiaDR3SP 默认路径"""
    project_root = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..")
    )
    return os.path.join(project_root, "GaiaDR3SP")


def parse_args() -> argparse.Namespace:
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="光谱积分器: 使用 Gaia DR3SP BP/RP 光谱计算合成流量 F_syn",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--ra", type=float, help="中心赤经 (度)，必填")
    parser.add_argument("--dec", type=float, help="中心赤纬 (度)，必填")
    parser.add_argument("--radius", type=float, default=0.5, help="搜索半径 (度)，默认 0.5")
    parser.add_argument("--mag-low", type=float, default=8.0, help="星等下限，默认 8.0")
    parser.add_argument("--mag-high", type=float, default=16.0, help="星等上限，默认 16.0")
    parser.add_argument("--filter", type=str, help="滤光片名称，必填 (可用 --list-filters 查看)")
    parser.add_argument("--qe", type=str, default=None, help="QE 曲线名称，可选")
    parser.add_argument(
        "--gaia-data", type=str, default=_default_gaia_data(),
        help="Gaia 数据目录路径，默认项目根目录下的 GaiaDR3SP",
    )
    parser.add_argument("--output", type=str, default="f_syn_results.json",
                        help="输出 JSON 文件路径，默认 f_syn_results.json")
    parser.add_argument("--list-filters", action="store_true", help="列出所有可用滤光片名称并退出")
    parser.add_argument("--list-qe", action="store_true", help="列出所有可用QE曲线名称并退出")
    parser.add_argument(
        "--narrowband-center", type=float, default=None,
        help="窄带滤光片中心波长 (nm)，提供时跳过 CurveLoader 并生成方波曲线",
    )
    parser.add_argument(
        "--narrowband-bw", type=float, default=7.0,
        help="窄带滤光片带宽 (nm)，默认 7.0",
    )
    parser.add_argument(
        "--narrowband-trans", type=float, default=1.0,
        help="窄带滤光片带内透过率 [0,1]，默认 1.0",
    )
    args = parser.parse_args()

    # 列表模式不校验必填参数
    if not args.list_filters and not args.list_qe:
        if args.ra is None:
            parser.error("--ra 为必填参数")
        if args.dec is None:
            parser.error("--dec 为必填参数")
        if not args.filter:
            parser.error("--filter 为必填参数")
    return args


def main():
    """主流程: 解析参数 -> 加载曲线 -> 锥形搜索 -> 批量积分 -> 保存结果"""
    args = parse_args()

    # ---- 列出可用曲线 ----
    if args.list_filters or args.list_qe:
        cl = CurveLoader()
        if args.list_filters:
            print("可用滤光片:")
            for name in cl.list_filters():
                print(f"  {name}")
        if args.list_qe:
            print("可用QE曲线:")
            for name in cl.list_qe():
                print(f"  {name}")
        return

    # ---- 1. 加载滤光片/QE 曲线 ----
    # 窄带模式: 提供 --narrowband-center 时跳过 CurveLoader，生成方波曲线
    # 宽带模式: 走 CurveLoader.load_filter(name) 路径
    qe_wl, qe_val = (None, None)
    cl = CurveLoader()
    if args.narrowband_center is not None:
        logger.info(
            "窄带方波注入模式: filter标签='%s', center=%.2f nm, bw=%.2f nm, trans=%.3f",
            args.filter, args.narrowband_center, args.narrowband_bw, args.narrowband_trans,
        )
        filter_wl, filter_trans = make_narrowband_curve(
            args.narrowband_center, args.narrowband_bw, args.narrowband_trans,
        )
        logger.info(
            "窄带曲线 '%s': %d 点, 波长范围 %.1f~%.1f nm",
            args.filter, len(filter_wl), float(filter_wl[0]), float(filter_wl[-1]),
        )
    else:
        logger.info("加载曲线: filter=%s, qe=%s", args.filter, args.qe or "无")
        filter_wl, filter_trans = cl.load_filter(args.filter)
        logger.info(
            "滤光片 '%s': %d 点, 波长范围 %.1f~%.1f nm",
            args.filter, len(filter_wl), float(filter_wl[0]), float(filter_wl[-1]),
        )
    if args.qe:
        qe_wl, qe_val = cl.load_qe(args.qe)
        logger.info(
            "QE曲线 '%s': %d 点, 波长范围 %.1f~%.1f nm",
            args.qe, len(qe_wl), float(qe_wl[0]), float(qe_wl[-1]),
        )

    # ---- 2. 初始化 Gaia 客户端，锥形搜索 ----
    logger.info(
        "锥形搜索: RA=%.6f, Dec=%.6f, r=%.4f, mag=[%.1f, %.1f], gaia_data=%s",
        args.ra, args.dec, args.radius, args.mag_low, args.mag_high, args.gaia_data,
    )
    with GaiaSpectrumClient(args.gaia_data, db_type=2) as client:
        spectrum_wl = client.get_wavelength_array()
        logger.info(
            "光谱波长: %.1f~%.1f nm (%d 点)",
            float(spectrum_wl[0]), float(spectrum_wl[-1]), len(spectrum_wl),
        )
        stars = client.cone_search_with_spectrum(
            args.ra, args.dec, args.radius, args.mag_low, args.mag_high)

        # ---- 3. 创建积分器，批量积分 ----
        integrator = SpectrumIntegrator(
            filter_wl, filter_trans, qe_wl, qe_val, spectrum_wl)
        results = integrator.integrate_batch(stars)

    # ---- 4. 保存结果 ----
    integrator.save_results(
        results, args.output, args.filter, args.qe,
        args.ra, args.dec, args.radius)

    print(f"完成: {len(results)} 颗星, 结果已保存到 {args.output}")


if __name__ == "__main__":
    main()
