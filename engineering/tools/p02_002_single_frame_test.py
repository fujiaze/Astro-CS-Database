# -*- coding: utf-8 -*-
"""
P02-002 单帧三路径验证脚本
================================

目的:
    在同一帧 FITS 上验证三条求解路径的 WCS 输出一致性:
      路径 0 (基准): ipv_solve_from_memory  (内部检测 + 内部选星 + 求解)
      路径 A       : ipv_solve_from_detections_v1  (外部检测 + 内部选星 + 求解)
      路径 B       : ipv_solve_from_memory_with_callback  (内部检测 + callback 导出 + 内部选星 + 求解)

    通过路径 B callback 拿到内部检测结果, 然后用同一组 detections 调用路径 A,
    可隔离检测与求解, 验证:
      (1) 路径 B 与路径 0 结果一致 (算法等价性, callback 无副作用)
      (2) 路径 A 用路径 B 导出的 detections 应得到与路径 0 一致的结果

    最终输出: results/single_frame_three_paths.json

依赖:
    lib/plate_solve/python/solve_and_write_wcs.py (env init)
    lib/plate_solve/python/ipv_solver.py (IPVSolver)
    lib/star_detector/python/star_detector.py (StarDetector, 仅路径 A 验证用)

用法:
    pwsh> python engineering/tools/p02_002_single_frame_test.py

作者: P02-002 子 Agent
日期: 2026-07-25
"""

from __future__ import annotations

import os
import sys
import json
import time
import traceback
from typing import Optional

# ============================================================================
# 项目根目录定位
# ============================================================================
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))

PLATE_SOLVE_PY = os.path.join(PROJECT_ROOT, "lib", "plate_solve", "python")
if PLATE_SOLVE_PY not in sys.path:
    sys.path.insert(0, PLATE_SOLVE_PY)

# solve_and_write_wcs 初始化时会设置 PATH/DLL 目录/sys.path
import solve_and_write_wcs as sw  # noqa: E402
from solve_and_write_wcs import init_environment, read_fits_header  # noqa: E402


# ============================================================================
# 工具函数
# ============================================================================

def result_to_dict(result) -> dict:
    """将 IpvWcsResult 转换为可 JSON 序列化的字典 (关键字段)"""
    ctype1 = result.ctype1.decode('utf-8', errors='ignore').rstrip('\x00')
    ctype2 = result.ctype2.decode('utf-8', errors='ignore').rstrip('\x00')
    if not ctype1:
        ctype1 = "RA---TAN-SIP" if result.sip_order > 0 else "RA---TAN"
    if not ctype2:
        ctype2 = "DEC--TAN-SIP" if result.sip_order > 0 else "DEC--TAN"
    return {
        "success": bool(result.success),
        "wcs": {
            "ctype1": ctype1,
            "ctype2": ctype2,
            "crval1": float(result.crval[0]),
            "crval2": float(result.crval[1]),
            "crpix1": float(result.crpix[0]),
            "crpix2": float(result.crpix[1]),
            "cd1_1": float(result.cd[0]),
            "cd1_2": float(result.cd[1]),
            "cd2_1": float(result.cd[2]),
            "cd2_2": float(result.cd[3]),
            "sip_order": int(result.sip_order),
            "sip_ap_order": int(result.sip_ap_order),
        },
        "n_pairs": int(result.n_pairs),
        "n_detected": int(result.n_detected),
        "n_catalog": int(result.n_catalog),
        "rms_px": float(result.rms_px),
        "rms_arcsec": float(result.rms_arcsec),
        "trans_order": int(result.trans_order),
        "best_inliers": int(result.best_inliers),
        "error_msg": result.error_msg.decode("utf-8", errors="ignore").strip(),
    }


def read_pixels_from_fits(fits_path: str):
    """读取 FITS 像素数据返回 float32 numpy 数组 (row-major, shape=[H, W])

    使用 astropy.io.fits, 自动做 uint16→float32 转换
    """
    from astropy.io import fits
    import numpy as np
    with fits.open(fits_path, mode='readonly') as hdul:
        data = hdul[0].data
    if data is None:
        raise RuntimeError("FITS 像素数据为空")
    # 确保 float32 + C-contiguous
    if data.dtype != np.float32:
        data = data.astype(np.float32)
    if not data.flags['C_CONTIGUOUS']:
        data = np.ascontiguousarray(data)
    return data


def wcs_diff(r0: dict, r1: dict) -> dict:
    """计算两个 WCS 结果的关键差异

    用于验证两条路径输出是否一致
    """
    w0 = r0["wcs"]
    w1 = r1["wcs"]
    return {
        "d_crval1_arcsec": abs(w0["crval1"] - w1["crval1"]) * 3600.0,
        "d_crval2_arcsec": abs(w0["crval2"] - w1["crval2"]) * 3600.0,
        "d_crpix1_px": abs(w0["crpix1"] - w1["crpix1"]),
        "d_crpix2_px": abs(w0["crpix2"] - w1["crpix2"]),
        "d_cd11": abs(w0["cd1_1"] - w1["cd1_1"]),
        "d_cd12": abs(w0["cd1_2"] - w1["cd1_2"]),
        "d_cd21": abs(w0["cd2_1"] - w1["cd2_1"]),
        "d_cd22": abs(w0["cd2_2"] - w1["cd2_2"]),
        "d_rms_arcsec": abs(r0["rms_arcsec"] - r1["rms_arcsec"]),
        "d_n_pairs": abs(r0["n_pairs"] - r1["n_pairs"]),
        "same_success": r0["success"] == r1["success"],
        "same_trans_order": r0["trans_order"] == r1["trans_order"],
        "same_sip_order": w0["sip_order"] == w1["sip_order"],
    }


# ============================================================================
# 三路径测试主流程
# ============================================================================

def run_three_paths(solver, sdet, fits_path: str, log_dir: str) -> dict:
    """对单帧 FITS 运行三条求解路径

    参数:
        solver: IPVSolver 实例 (已设置 gaia/detector 句柄)
        sdet: StarDetector 实例 (用于读取像素)
        fits_path: FITS 文件路径
        log_dir: 日志目录 (每条路径独立日志)

    返回:
        dict: {
            fits_path, header, paths: {path0, pathA, pathB},
            diff_A_vs_0, diff_B_vs_0, callback_detections_count
        }
    """
    import numpy as np

    # 1. 读取 FITS 头 + 像素
    header_info = read_fits_header(fits_path)
    ra0 = header_info["ra0"]
    dec0 = header_info["dec0"]
    focal_length = header_info["focal_length"]
    pixel_size = header_info["pixel_size"]

    pixels = read_pixels_from_fits(fits_path)
    height, width = pixels.shape

    # 准备日志目录参数 (避免日志互相覆盖, 每条路径独立子目录)
    from ipv_solver import IpvParams

    def _make_params(label: str) -> IpvParams:
        p = solver.get_default_params()
        log_subdir = os.path.join(log_dir, label).encode('utf-8')
        if len(log_subdir) >= 256:
            log_subdir = log_subdir[:255]
        p.log_dir = log_subdir
        return p

    os.makedirs(os.path.join(log_dir, "path0"), exist_ok=True)
    os.makedirs(os.path.join(log_dir, "pathA"), exist_ok=True)
    os.makedirs(os.path.join(log_dir, "pathB"), exist_ok=True)

    result_summary = {
        "fits_path": fits_path,
        "header": {
            "ra0": ra0, "dec0": dec0,
            "focal_length_mm": focal_length,
            "pixel_size_um": pixel_size,
            "width": int(width), "height": int(height),
        },
        "paths": {},
    }

    # ===== 路径 0: 基准 (ipv_solve_from_memory) =====
    print(f"\n[路径 0] ipv_solve_from_memory (基准)")
    t0 = time.perf_counter()
    try:
        params0 = _make_params("path0")
        r0 = solver.solve_from_memory(
            pixels, width, height, ra0, dec0,
            focal_length, pixel_size, params0
        )
        d0 = result_to_dict(r0)
        d0["duration_sec"] = time.perf_counter() - t0
        print(f"  success={d0['success']}, n_pairs={d0['n_pairs']}, "
              f"rms_arcsec={d0['rms_arcsec']:.4f}, duration={d0['duration_sec']:.2f}s")
    except Exception as e:
        d0 = {"success": False, "error": str(e), "duration_sec": time.perf_counter() - t0}
        print(f"  异常: {e}")
    result_summary["paths"]["path0_baseline"] = d0

    # ===== 路径 B: ipv_solve_from_memory_with_callback =====
    # 先用 callback 收集 detections, 再用同一份 detections 调用路径 A
    print(f"\n[路径 B] ipv_solve_from_memory_with_callback (内部检测 + callback 导出)")
    captured_detections = None

    def detection_callback(det_arr, n_det, user_data):
        nonlocal captured_detections
        # det_arr 是 numpy [N,6] float64 的副本 (Python 包装已复制)
        captured_detections = det_arr.copy()
        print(f"  callback: 接收到 {n_det} 颗检测星")

    t0 = time.perf_counter()
    try:
        paramsB = _make_params("pathB")
        rB = solver.solve_from_memory_with_callback(
            pixels, width, height, ra0, dec0,
            focal_length, pixel_size,
            callback=detection_callback,
            user_data=None,
            params=paramsB,
        )
        dB = result_to_dict(rB)
        dB["duration_sec"] = time.perf_counter() - t0
        dB["callback_n_detections"] = (int(captured_detections.shape[0])
                                        if captured_detections is not None else 0)
        print(f"  success={dB['success']}, n_pairs={dB['n_pairs']}, "
              f"rms_arcsec={dB['rms_arcsec']:.4f}, duration={dB['duration_sec']:.2f}s")
        print(f"  callback 捕获 {dB['callback_n_detections']} 颗检测星")
    except Exception as e:
        dB = {"success": False, "error": str(e), "duration_sec": time.perf_counter() - t0}
        print(f"  异常: {e}")
    result_summary["paths"]["pathB_callback"] = dB

    # ===== 路径 A: ipv_solve_from_detections_v1 =====
    # 使用路径 B callback 捕获的 detections 作为输入
    if captured_detections is not None and captured_detections.shape[0] > 0:
        print(f"\n[路径 A] ipv_solve_from_detections_v1 (用路径 B callback 捕获的 detections)")
        t0 = time.perf_counter()
        try:
            paramsA = _make_params("pathA")
            rA = solver.solve_from_detections_v1(
                captured_detections, width, height,
                ra0, dec0, focal_length, pixel_size, paramsA
            )
            dA = result_to_dict(rA)
            dA["duration_sec"] = time.perf_counter() - t0
            dA["input_n_detections"] = int(captured_detections.shape[0])
            print(f"  success={dA['success']}, n_pairs={dA['n_pairs']}, "
                  f"rms_arcsec={dA['rms_arcsec']:.4f}, duration={dA['duration_sec']:.2f}s")
        except Exception as e:
            dA = {"success": False, "error": str(e), "duration_sec": time.perf_counter() - t0}
            print(f"  异常: {e}")
        result_summary["paths"]["pathA_from_detections"] = dA
    else:
        print(f"\n[路径 A] 跳过 (路径 B 未捕获 detections)")
        result_summary["paths"]["pathA_from_detections"] = {
            "success": False, "error": "path B callback 未捕获 detections, 无法测试路径 A"
        }

    # ===== 差异分析 =====
    if d0.get("success") and dB.get("success"):
        result_summary["diff_B_vs_0"] = wcs_diff(d0, dB)
        print(f"\n[差异] B vs 0:")
        print(f"  d_crval = ({result_summary['diff_B_vs_0']['d_crval1_arcsec']:.6f}, "
              f"{result_summary['diff_B_vs_0']['d_crval2_arcsec']:.6f}) arcsec")
        print(f"  d_rms = {result_summary['diff_B_vs_0']['d_rms_arcsec']:.6f} arcsec")
        print(f"  d_n_pairs = {result_summary['diff_B_vs_0']['d_n_pairs']}")

    if d0.get("success") and result_summary["paths"].get("pathA_from_detections", {}).get("success"):
        result_summary["diff_A_vs_0"] = wcs_diff(d0, result_summary["paths"]["pathA_from_detections"])
        print(f"\n[差异] A vs 0:")
        print(f"  d_crval = ({result_summary['diff_A_vs_0']['d_crval1_arcsec']:.6f}, "
              f"{result_summary['diff_A_vs_0']['d_crval2_arcsec']:.6f}) arcsec")
        print(f"  d_rms = {result_summary['diff_A_vs_0']['d_rms_arcsec']:.6f} arcsec")
        print(f"  d_n_pairs = {result_summary['diff_A_vs_0']['d_n_pairs']}")

    return result_summary


# ============================================================================
# 主入口
# ============================================================================

def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="P02-002 单帧三路径验证 (path0/A/B 精度一致性)"
    )
    parser.add_argument(
        "--fits", default="",
        help="FITS 文件路径 (默认使用 Galaxy_Center 第一帧 Red)"
    )
    parser.add_argument(
        "--output-dir", default="",
        help="结果输出目录 (默认 engineering/evidence/P02-002/results)"
    )
    args = parser.parse_args()

    # 默认测试帧: Galaxy_Center 第一帧 Red (与 P02-001 frame_0001 一致)
    if not args.fits:
        args.fits = os.path.join(
            PROJECT_ROOT, "testdata", "Galaxy_Center_T4", "lights", "panel1",
            "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts"
        )

    if not os.path.isabs(args.output_dir) or not args.output_dir:
        args.output_dir = os.path.join(
            PROJECT_ROOT, "engineering", "evidence", "P02-002", "results"
        )

    os.makedirs(args.output_dir, exist_ok=True)

    # 日志目录 (C++ 求解器内部日志)
    log_dir = os.path.join(args.output_dir, "solver_logs")
    os.makedirs(log_dir, exist_ok=True)

    # Windows 控制台 UTF-8
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    print("=" * 70)
    print("P02-002 单帧三路径验证")
    print("=" * 70)
    print(f"FITS: {args.fits}")
    print(f"输出: {args.output_dir}")

    if not os.path.isfile(args.fits):
        print(f"[FAIL] FITS 文件不存在: {args.fits}")
        sys.exit(1)

    # 初始化环境
    print("\n[初始化] 加载 GaiaClient + StarDetector + IPVSolver ...")
    try:
        gaia_client, sdet, solver = init_environment()
    except Exception as e:
        print(f"[FAIL] 环境初始化失败: {e}")
        traceback.print_exc()
        sys.exit(1)

    # 运行三路径测试
    try:
        summary = run_three_paths(solver, sdet, args.fits, log_dir)
    except Exception as e:
        print(f"[FAIL] 测试执行失败: {e}")
        traceback.print_exc()
        sys.exit(1)
    finally:
        # 关闭环境
        try:
            sw._close_environment(gaia_client, sdet, solver)
        except Exception:
            pass

    # 保存结果
    output_file = os.path.join(args.output_dir, "single_frame_three_paths.json")
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    print(f"\n[OK] 结果已保存: {output_file}")

    # 判定
    print("\n" + "=" * 70)
    print("判定")
    print("=" * 70)

    paths = summary.get("paths", {})
    p0 = paths.get("path0_baseline", {})
    pA = paths.get("pathA_from_detections", {})
    pB = paths.get("pathB_callback", {})

    ok_0 = p0.get("success", False)
    ok_A = pA.get("success", False)
    ok_B = pB.get("success", False)

    print(f"路径 0 (基准): success={ok_0}, rms={p0.get('rms_arcsec', 'N/A')}")
    print(f"路径 A (外检): success={ok_A}, rms={pA.get('rms_arcsec', 'N/A')}")
    print(f"路径 B (回调): success={ok_B}, rms={pB.get('rms_arcsec', 'N/A')}")

    if ok_0 and ok_B:
        diff_B = summary.get("diff_B_vs_0", {})
        d_crval = max(diff_B.get("d_crval1_arcsec", 999),
                      diff_B.get("d_crval2_arcsec", 999))
        d_rms = diff_B.get("d_rms_arcsec", 999)
        # 容差: CRVAL 差异 < 0.001" (微秒级浮点误差), RMS 差异 < 0.001"
        b_pass = (d_crval < 0.001 and d_rms < 0.001)
        print(f"\n路径 B vs 0: d_crval={d_crval:.6f} arcsec, d_rms={d_rms:.6f} arcsec "
              f"-> {'PASS' if b_pass else 'FAIL'}")
    else:
        b_pass = False
        print(f"\n路径 B vs 0: 跳过 (有失败路径)")

    if ok_0 and ok_A:
        diff_A = summary.get("diff_A_vs_0", {})
        d_crval = max(diff_A.get("d_crval1_arcsec", 999),
                      diff_A.get("d_crval2_arcsec", 999))
        d_rms = diff_A.get("d_rms_arcsec", 999)
        a_pass = (d_crval < 0.001 and d_rms < 0.001)
        print(f"路径 A vs 0: d_crval={d_crval:.6f} arcsec, d_rms={d_rms:.6f} arcsec "
              f"-> {'PASS' if a_pass else 'FAIL'}")
    else:
        a_pass = False
        print(f"路径 A vs 0: 跳过 (有失败路径)")

    print("\n" + "=" * 70)
    if b_pass and a_pass:
        print("[结论] 三路径精度一致性验证 PASS")
        sys.exit(0)
    else:
        print("[结论] 三路径精度一致性验证 FAIL (需要排查)")
        sys.exit(2)


if __name__ == "__main__":
    main()
