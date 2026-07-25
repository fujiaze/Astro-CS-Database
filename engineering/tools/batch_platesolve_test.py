# -*- coding: utf-8 -*-
"""
P02-001 / P02-003 PlateSolve 全量 TestData 批量测试工具
================================================

功能:
    轻量级 plate solving 测试工具, 只做:
      1. 读取 FITS 文件头 (OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ)
      2. 调用 ipv_solver.dll 的 C API
      3. 记录 WCS, 星数, RMS, 耗时

    不做 (区别于完整 stage1):
      - 不写 FITS (overwrite=False)
      - 不调用 drizzle, photometric, snr
      - 不写 star_det/gaia_cat 块

模式 (--mode):
    old    : 调用 ipv_solve (从文件读, P02-001 旧路径基线)
    path-b : 调用 ipv_solve_from_memory_with_callback (P02-003 路径B, callback 导出)
             读 FITS 像素到内存 float32, callback 内复制检测结果 (star_det v1)

依赖:
    lib/plate_solve/python/solve_and_write_wcs.py (复用环境初始化 + FITS 头读取)
    lib/plate_solve/python/ipv_solver.py (IPVSolver ctypes 绑定)

用法:
    pwsh> python engineering/tools/batch_platesolve_test.py \\
            --manifest engineering/evidence/P02-001/testdata_manifest.json \\
            --output-dir engineering/evidence/P02-001/results \\
            --repeat-first 10

    pwsh> python engineering/tools/batch_platesolve_test.py \\
            --mode path-b \\
            --manifest engineering/evidence/P02-001/testdata_manifest.json \\
            --output-dir engineering/evidence/P02-003/results

输出:
    results/frame_<index>.json        每帧单次运行结果
    results/frame_<index>_run<k>.json 前 10 帧 3 次重复性结果
    results/old_path_baseline.json    汇总基线 (mode=old)
    results/path_b_results.json       汇总基线 (mode=path-b)

作者: P02-001 子 Agent (P02-003 扩展 path-b 模式)
日期: 2026-07-25
"""

from __future__ import annotations

import os
import sys
import json
import time
import argparse
import traceback
import statistics
from datetime import datetime
from typing import Optional

# ============================================================================
# 项目根目录定位
# ============================================================================
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))

# 把 lib/plate_solve/python 加入 sys.path (复用 solve_and_write_wcs 模块)
PLATE_SOLVE_PY = os.path.join(PROJECT_ROOT, "lib", "plate_solve", "python")
if PLATE_SOLVE_PY not in sys.path:
    sys.path.insert(0, PLATE_SOLVE_PY)

# solve_and_write_wcs 初始化时会设置 PATH/DLL 目录/sys.path, import 时执行
import solve_and_write_wcs as sw  # noqa: E402
from solve_and_write_wcs import init_environment, read_fits_header, parse_ra_hms, parse_dec_dms  # noqa: E402


# ============================================================================
# 单帧 plate solve (轻量: 不写 FITS, 不重复 sdet_detect_ex)
# ============================================================================
def solve_single_frame(solver, fits_path: str) -> dict:
    """对单帧执行 plate solving, 返回结构化结果

    参数:
        solver: IPVSolver 实例 (已设置 gaia/detector 句柄)
        fits_path: FITS 文件路径

    返回:
        dict: {
            success, duration_sec, wcs (cd/crval/crpix/ctype/sip),
            n_pairs, n_detected, n_catalog, rms_px, rms_arcsec,
            trans_order, best_inliers, error_msg
        }
    """
    t0 = time.perf_counter()

    # 1. 读取 FITS 头获取初始指向
    try:
        header_info = read_fits_header(fits_path)
        ra0 = header_info["ra0"]
        dec0 = header_info["dec0"]
        focal_length = header_info["focal_length"]
        pixel_size = header_info["pixel_size"]
    except Exception as e:
        return {
            "success": False,
            "duration_sec": time.perf_counter() - t0,
            "error": "read_fits_header 失败: %s" % str(e),
        }

    # 2. 调用 solver.solve() (ipv_solve C API)
    try:
        result = solver.solve(
            fits_path, ra0, dec0, focal_length, pixel_size
        )
    except Exception as e:
        return {
            "success": False,
            "duration_sec": time.perf_counter() - t0,
            "error": "solver.solve 异常: %s" % str(e),
            "ra0": ra0, "dec0": dec0,
            "focal_length": focal_length, "pixel_size": pixel_size,
        }

    duration = time.perf_counter() - t0

    # 3. 提取 WCS 字段
    ctype1 = result.ctype1.decode('utf-8', errors='ignore').rstrip('\x00')
    ctype2 = result.ctype2.decode('utf-8', errors='ignore').rstrip('\x00')
    if not ctype1:
        ctype1 = "RA---TAN-SIP" if result.sip_order > 0 else "RA---TAN"
    if not ctype2:
        ctype2 = "DEC--TAN-SIP" if result.sip_order > 0 else "DEC--TAN"

    err_msg = result.error_msg.decode("utf-8", errors="ignore").strip()

    return {
        "success": bool(result.success),
        "duration_sec": duration,
        "ra0": ra0, "dec0": dec0,
        "focal_length_mm": focal_length,
        "pixel_size_um": pixel_size,
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
            "sip_a": list(result.sip_a),
            "sip_b": list(result.sip_b),
            "sip_ap": list(result.sip_ap),
            "sip_bp": list(result.sip_bp),
        },
        "n_pairs": int(result.n_pairs),
        "n_detected": int(result.n_detected),
        "n_catalog": int(result.n_catalog),
        "rms_px": float(result.rms_px),
        "rms_arcsec": float(result.rms_arcsec),
        "trans_order": int(result.trans_order),
        "best_inliers": int(result.best_inliers),
        "error_msg": err_msg,
    }


# ============================================================================
# P02-003 路径 B: 从 FITS 读取像素到内存 (float32, row-major [H,W])
# ============================================================================
def read_fits_pixels(fits_path):
    """读取 FITS 像素数据为 numpy float32 数组 (row-major [H,W])

    参数:
        fits_path: FITS 文件路径

    返回:
        tuple: (pixels_float32, width, height)
            pixels_float32: numpy.ndarray shape=[H,W] dtype=float32 C-contiguous
            width: 图像宽度 (像素)
            height: 图像高度 (像素)
    """
    import numpy as np
    from astropy.io import fits

    # memmap=False: FITS 含 BZERO/BSCALE/BLANK 关键字时不支持 memmap
    with fits.open(fits_path, mode='readonly', memmap=False) as hdul:
        data = hdul[0].data
        if data is None:
            raise RuntimeError("FITS PRIMARY HDU 无数据")
        # 确保 float32 + C-contiguous + 2D
        arr = np.asarray(data, dtype=np.float32)
        if arr.ndim != 2:
            if arr.ndim == 3 and arr.shape[0] == 1:
                arr = arr[0]
            else:
                raise RuntimeError("FITS 数据非 2D: shape=%s" % (arr.shape,))
        if not arr.flags['C_CONTIGUOUS']:
            arr = np.ascontiguousarray(arr)
    height, width = arr.shape
    return arr, width, height


# ============================================================================
# P02-003 路径 B: 单帧 plate solve (callback 导出检测结果)
# ============================================================================
def solve_single_frame_path_b(solver, fits_path):
    """对单帧执行 plate solving (路径B: ipv_solve_from_memory_with_callback)

    参数:
        solver: IPVSolver 实例 (已设置 gaia/detector 句柄)
        fits_path: FITS 文件路径

    返回:
        dict: 同 solve_single_frame, 额外含 callback_n_detected
    """
    import numpy as np

    t0 = time.perf_counter()

    # 1. 读取 FITS 头获取初始指向
    try:
        header_info = read_fits_header(fits_path)
        ra0 = header_info["ra0"]
        dec0 = header_info["dec0"]
        focal_length = header_info["focal_length"]
        pixel_size = header_info["pixel_size"]
    except Exception as e:
        return {
            "success": False,
            "duration_sec": time.perf_counter() - t0,
            "error": "read_fits_header 失败: %s" % str(e),
        }

    # 2. 读取 FITS 像素到内存 (float32)
    try:
        pixels, width, height = read_fits_pixels(fits_path)
    except Exception as e:
        return {
            "success": False,
            "duration_sec": time.perf_counter() - t0,
            "error": "read_fits_pixels 失败: %s" % str(e),
            "ra0": ra0, "dec0": dec0,
            "focal_length_mm": focal_length, "pixel_size_um": pixel_size,
        }

    # 3. callback 上下文 (记录检测结果)
    cb_state = {"n_detected": 0, "copied": False, "detections": None}

    def on_detections(detections_arr, n_det, user_data):
        """路径B callback: 复制检测结果 (callback 返回后源指针失效)"""
        cb_state["n_detected"] = int(n_det)
        if detections_arr is not None and n_det > 0:
            # detections_arr 已是 numpy 数组副本 (ipv_solver.py 的 _trampoline 已 copy)
            cb_state["detections"] = detections_arr
            cb_state["copied"] = True

    # 4. 调用 solver.solve_from_memory_with_callback (路径B)
    try:
        result = solver.solve_from_memory_with_callback(
            pixels, width, height,
            ra0, dec0, focal_length, pixel_size,
            callback=on_detections, user_data=None,
        )
    except Exception as e:
        return {
            "success": False,
            "duration_sec": time.perf_counter() - t0,
            "error": "solver.solve_from_memory_with_callback 异常: %s" % str(e),
            "ra0": ra0, "dec0": dec0,
            "focal_length_mm": focal_length, "pixel_size_um": pixel_size,
            "callback_n_detected": cb_state["n_detected"],
        }

    duration = time.perf_counter() - t0

    # 5. 提取 WCS 字段 (与 solve_single_frame 一致)
    ctype1 = result.ctype1.decode('utf-8', errors='ignore').rstrip('\x00')
    ctype2 = result.ctype2.decode('utf-8', errors='ignore').rstrip('\x00')
    if not ctype1:
        ctype1 = "RA---TAN-SIP" if result.sip_order > 0 else "RA---TAN"
    if not ctype2:
        ctype2 = "DEC--TAN-SIP" if result.sip_order > 0 else "DEC--TAN"

    err_msg = result.error_msg.decode("utf-8", errors="ignore").strip()

    return {
        "success": bool(result.success),
        "duration_sec": duration,
        "ra0": ra0, "dec0": dec0,
        "focal_length_mm": focal_length,
        "pixel_size_um": pixel_size,
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
            "sip_a": list(result.sip_a),
            "sip_b": list(result.sip_b),
            "sip_ap": list(result.sip_ap),
            "sip_bp": list(result.sip_bp),
        },
        "n_pairs": int(result.n_pairs),
        "n_detected": int(result.n_detected),
        "n_catalog": int(result.n_catalog),
        "rms_px": float(result.rms_px),
        "rms_arcsec": float(result.rms_arcsec),
        "trans_order": int(result.trans_order),
        "best_inliers": int(result.best_inliers),
        "error_msg": err_msg,
        "callback_n_detected": cb_state["n_detected"],
        "callback_copied": bool(cb_state["copied"]),
    }


# ============================================================================
# 主入口
# ============================================================================
def main():
    parser = argparse.ArgumentParser(
        description="P02-001/P02-003 PlateSolve 全量 TestData 批量测试工具",
    )
    parser.add_argument(
        "--manifest", required=True,
        help="testdata_manifest.json 路径",
    )
    parser.add_argument(
        "--output-dir", required=True,
        help="结果输出目录 (results/)",
    )
    parser.add_argument(
        "--mode", choices=["old", "path-b"], default="old",
        help="求解模式: old=ipv_solve(文件) / path-b=ipv_solve_from_memory_with_callback(路径B)",
    )
    parser.add_argument(
        "--repeat-first", type=int, default=10,
        help="前 N 帧重复运行 3 次 (默认 10)",
    )
    parser.add_argument(
        "--limit", type=int, default=0,
        help="仅运行前 N 帧 (0=全部, 用于调试)",
    )
    parser.add_argument(
        "--target", default="",
        help="仅运行指定 target_name (空=全部)",
    )
    args = parser.parse_args()

    # Windows 控制台 UTF-8
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    os.makedirs(args.output_dir, exist_ok=True)

    # 加载 manifest
    with open(args.manifest, "r", encoding="utf-8") as f:
        manifest = json.load(f)
    frames = manifest["frames"]
    if args.target:
        frames = [fr for fr in frames if fr["target_name"] == args.target]
    if args.limit > 0:
        frames = frames[:args.limit]

    # 选择求解函数 (P02-003 路径 B)
    if args.mode == "path-b":
        solve_fn = solve_single_frame_path_b
        mode_label = "路径B (callback 导出)"
        summary_filename = "path_b_results.json"
        task_id = "P02-003"
        task_name = "PlateSolve 全量 TestData 路径B (callback 导出) (v1.1 开发包)"
    else:
        solve_fn = solve_single_frame
        mode_label = "旧路径 (ipv_solve)"
        summary_filename = "old_path_baseline.json"
        task_id = "P02-001"
        task_name = "PlateSolve 全量 TestData 旧路径基线 (v1.1 开发包)"

    n_total = len(frames)
    print("=" * 60)
    print("P02-003 PlateSolve 全量 TestData 批量测试 [%s]" % mode_label)
    print("=" * 60)
    print("模式: %s (--mode %s)" % (mode_label, args.mode))
    print("总帧数: %d" % n_total)
    print("前 %d 帧重复 3 次" % args.repeat_first)
    print("输出目录: %s" % args.output_dir)
    print("=" * 60)

    # 初始化环境 (GaiaClient + StarDetector + IPVSolver)
    print("[INIT] 初始化环境 (GaiaClient + StarDetector + IPVSolver)...")
    t_init0 = time.perf_counter()
    gaia_client, sdet, solver = init_environment()
    t_init = time.perf_counter() - t_init0
    print("[INIT] 环境初始化完成 (%.2fs)" % t_init)

    # Gaia 缓存预热 (避免首帧冷缓存影响统计)
    # 实际: 不同天区首帧会触发冷缓存, 这是正常现象, 我们记录实际耗时

    # 全量运行
    all_results = []
    repeat_results = {}  # index -> [result1, result2, result3]

    t_total_start = time.perf_counter()
    for i, fr in enumerate(frames, 1):
        idx = fr["index"]
        fits_path = os.path.join(PROJECT_ROOT, fr["filepath"])

        # 单次运行
        result = solve_fn(solver, fits_path)
        result["index"] = idx
        result["case_id"] = fr["case_id"]
        result["target_name"] = fr["target_name"]
        result["filename"] = fr["filename"]
        result["filter"] = fr["filter"]
        result["exposure"] = fr["exposure"]
        result["panel"] = fr["panel"]
        result["sha256"] = fr["sha256"]
        result["run_number"] = 1

        # 写单帧结果
        out_frame = os.path.join(args.output_dir, "frame_%04d.json" % idx)
        with open(out_frame, "w", encoding="utf-8") as f:
            json.dump(result, f, ensure_ascii=False, indent=2, default=str)

        all_results.append(result)

        # 前 N 帧再跑 2 次 (总共 3 次)
        if i <= args.repeat_first:
            runs = [result]
            for run_idx in (2, 3):
                r2 = solve_fn(solver, fits_path)
                r2["index"] = idx
                r2["case_id"] = fr["case_id"]
                r2["target_name"] = fr["target_name"]
                r2["filename"] = fr["filename"]
                r2["filter"] = fr["filter"]
                r2["exposure"] = fr["exposure"]
                r2["panel"] = fr["panel"]
                r2["sha256"] = fr["sha256"]
                r2["run_number"] = run_idx
                runs.append(r2)

                # 写单次重复结果
                out_rep = os.path.join(args.output_dir, "frame_%04d_run%d.json" % (idx, run_idx))
                with open(out_rep, "w", encoding="utf-8") as f:
                    json.dump(r2, f, ensure_ascii=False, indent=2, default=str)

            repeat_results[idx] = runs

        # 进度日志
        elapsed = time.perf_counter() - t_total_start
        n_success = sum(1 for r in all_results if r["success"])
        if i % 10 == 0 or i <= args.repeat_first or i == n_total:
            avg_dur = elapsed / i
            eta = avg_dur * (n_total - i)
            status = "OK" if result["success"] else "FAIL"
            print("[%4d/%d] %s %s dur=%.2fs rms=%.3f\" npairs=%d  | succ=%d/%d avg=%.2fs eta=%.0fs" % (
                i, n_total, fr["target_name"][:14], status,
                result.get("duration_sec", 0),
                result.get("rms_arcsec", 0) if result["success"] else 0,
                result.get("n_pairs", 0),
                n_success, i, avg_dur, eta,
            ))

    t_total = time.perf_counter() - t_total_start

    # ============================================================================
    # 汇总统计
    # ============================================================================
    n_success = sum(1 for r in all_results if r["success"])
    n_fail = n_total - n_success
    success_rate = n_success / n_total if n_total > 0 else 0.0

    # RMS 分布 (仅成功帧)
    rms_arcsec_list = [r["rms_arcsec"] for r in all_results if r["success"]]
    rms_px_list = [r["rms_px"] for r in all_results if r["success"]]
    n_pairs_list = [r["n_pairs"] for r in all_results if r["success"]]
    duration_list = [r["duration_sec"] for r in all_results if r["success"]]

    def stats(arr):
        if not arr:
            return {"count": 0, "min": 0, "max": 0, "mean": 0, "median": 0, "p90": 0, "p99": 0}
        s = sorted(arr)
        n = len(s)
        return {
            "count": n,
            "min": s[0],
            "max": s[-1],
            "mean": statistics.mean(s),
            "median": statistics.median(s),
            "p90": s[int(n * 0.9)] if n >= 10 else s[-1],
            "p99": s[int(n * 0.99)] if n >= 100 else s[-1],
        }

    # 失败帧列表
    fail_frames = []
    for r in all_results:
        if not r["success"]:
            fail_frames.append({
                "index": r["index"],
                "case_id": r["case_id"],
                "target_name": r["target_name"],
                "filename": r["filename"],
                "filter": r["filter"],
                "duration_sec": r.get("duration_sec", 0),
                "error": r.get("error", "") or r.get("error_msg", ""),
            })

    # 按 target 分组
    by_target = {}
    for r in all_results:
        t = r["target_name"]
        if t not in by_target:
            by_target[t] = {"total": 0, "success": 0, "fail": 0, "rms_arcsec": [], "duration": []}
        by_target[t]["total"] += 1
        if r["success"]:
            by_target[t]["success"] += 1
            by_target[t]["rms_arcsec"].append(r["rms_arcsec"])
            by_target[t]["duration"].append(r["duration_sec"])
        else:
            by_target[t]["fail"] += 1
    for t, v in by_target.items():
        v["success_rate"] = v["success"] / v["total"] if v["total"] > 0 else 0
        v["rms_arcsec_stats"] = stats(v["rms_arcsec"])
        v["duration_stats"] = stats(v["duration"])
        del v["rms_arcsec"]
        del v["duration"]

    # 按 filter 分组
    by_filter = {}
    for r in all_results:
        flt = r["filter"] or "unknown"
        if flt not in by_filter:
            by_filter[flt] = {"total": 0, "success": 0, "fail": 0}
        by_filter[flt]["total"] += 1
        if r["success"]:
            by_filter[flt]["success"] += 1
        else:
            by_filter[flt]["fail"] += 1
    for flt, v in by_filter.items():
        v["success_rate"] = v["success"] / v["total"] if v["total"] > 0 else 0

    # 重复性分析 (前 N 帧的 3 次运行)
    repeatability = []
    for idx, runs in repeat_results.items():
        if len(runs) < 3:
            continue
        succ_runs = [r for r in runs if r["success"]]
        if len(succ_runs) < 3:
            repeatability.append({
                "index": idx,
                "filename": runs[0]["filename"],
                "target_name": runs[0]["target_name"],
                "success_count": len(succ_runs),
                "wcs_diff_ra_deg": None,
                "wcs_diff_dec_deg": None,
                "wcs_diff_rms_arcsec": None,
                "durations": [r["duration_sec"] for r in runs],
                "rms_arcsec_runs": [r.get("rms_arcsec", 0) for r in runs],
                "note": "成功次数 < 3, 无法计算 WCS 重复性",
            })
            continue

        # WCS 中心差异
        crval1s = [r["wcs"]["crval1"] for r in succ_runs]
        crval2s = [r["wcs"]["crval2"] for r in succ_runs]
        rmss = [r["rms_arcsec"] for r in succ_runs]
        durs = [r["duration_sec"] for r in succ_runs]

        repeatability.append({
            "index": idx,
            "filename": runs[0]["filename"],
            "target_name": runs[0]["target_name"],
            "success_count": len(succ_runs),
            "wcs_diff_ra_deg": max(crval1s) - min(crval1s),
            "wcs_diff_dec_deg": max(crval2s) - min(crval2s),
            "wcs_diff_rms_arcsec": statistics.stdev(rmss) if len(rmss) > 1 else 0,
            "rms_arcsec_mean": statistics.mean(rmss),
            "rms_arcsec_std": statistics.stdev(rmss) if len(rmss) > 1 else 0,
            "duration_mean": statistics.mean(durs),
            "duration_std": statistics.stdev(durs) if len(durs) > 1 else 0,
            "durations": durs,
            "rms_arcsec_runs": rmss,
        })

    # ============================================================================
    # 写汇总文件
    # ============================================================================
    summary = {
        "_meta": {
            "task_id": task_id,
            "task_name": task_name,
            "phase": "P02",
            "gate": "G2",
            "mode": args.mode,
            "mode_label": mode_label,
            "commit_base": manifest["_meta"]["commit_base"],
            "manifest_sha256": manifest["_meta"]["manifest_sha256"],
            "total_frames": n_total,
            "repeat_first_n": args.repeat_first,
            "started_at": datetime.fromtimestamp(t_total_start).strftime("%Y-%m-%dT%H:%M:%S+08:00"),
            "ended_at": datetime.now().strftime("%Y-%m-%dT%H:%M:%S+08:00"),
            "total_duration_sec": t_total,
            "env_init_sec": t_init,
            "ipv_solver_dll": os.path.join(PROJECT_ROOT, "build", "artifacts", "ipv_solver.dll"),
            "scope": "轻量 plate solving: read FITS header + ipv_solve (无 stage1 全流程)",
            "no_modification": "不修改 FITS 文件 (overwrite=False), 不调用 drizzle/photometric/snr",
            "tool": "engineering/tools/batch_platesolve_test.py",
        },
        "overall": {
            "total": n_total,
            "success": n_success,
            "fail": n_fail,
            "success_rate": success_rate,
            "rms_arcsec_stats": stats(rms_arcsec_list),
            "rms_px_stats": stats(rms_px_list),
            "n_pairs_stats": stats(n_pairs_list),
            "duration_stats": stats(duration_list),
        },
        "by_target": by_target,
        "by_filter": by_filter,
        "repeatability_first_n": repeatability,
        "fail_frames": fail_frames,
        "all_frames_summary": [
            {
                "index": r["index"],
                "case_id": r["case_id"],
                "target_name": r["target_name"],
                "filename": r["filename"],
                "filter": r["filter"],
                "success": r["success"],
                "duration_sec": r.get("duration_sec", 0),
                "rms_arcsec": r.get("rms_arcsec", 0) if r["success"] else 0,
                "rms_px": r.get("rms_px", 0) if r["success"] else 0,
                "n_pairs": r.get("n_pairs", 0),
                "n_detected": r.get("n_detected", 0),
                "n_catalog": r.get("n_catalog", 0),
                "trans_order": r.get("trans_order", 0) if r["success"] else 0,
                "best_inliers": r.get("best_inliers", 0) if r["success"] else 0,
                "crval1": r["wcs"]["crval1"] if r["success"] and "wcs" in r else None,
                "crval2": r["wcs"]["crval2"] if r["success"] and "wcs" in r else None,
                "sip_order": r["wcs"]["sip_order"] if r["success"] and "wcs" in r else 0,
                "callback_n_detected": r.get("callback_n_detected", None),
                "callback_copied": r.get("callback_copied", None),
            }
            for r in all_results
        ],
    }

    out_summary = os.path.join(args.output_dir, "..", summary_filename)
    out_summary = os.path.normpath(out_summary)
    with open(out_summary, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2, default=str)
    print("=" * 60)
    print("汇总基线已写入: %s" % out_summary)
    print("=" * 60)

    # 摘要打印
    print("")
    print("=" * 60)
    print("PlateSolve 全量基线 [%s] - 摘要" % mode_label)
    print("=" * 60)
    print("总帧数: %d" % n_total)
    print("成功: %d  失败: %d  成功率: %.2f%%" % (n_success, n_fail, success_rate * 100))
    print("总耗时: %.1fs (%.1fmin)" % (t_total, t_total / 60))
    print("平均耗时: %.2fs/帧" % (statistics.mean(duration_list) if duration_list else 0))
    print("")
    print("RMS (角秒) 分布:")
    rs = summary["overall"]["rms_arcsec_stats"]
    print("  count=%d, min=%.3f, median=%.3f, mean=%.3f, p90=%.3f, p99=%.3f, max=%.3f" % (
        rs["count"], rs["min"], rs["median"], rs["mean"], rs["p90"], rs["p99"], rs["max"]
    ))
    print("")
    print("按目标天区:")
    for t, v in sorted(by_target.items()):
        print("  %-20s total=%3d success=%3d fail=%3d rate=%.1f%% median_rms=%.3f\" avg_dur=%.2fs" % (
            t, v["total"], v["success"], v["fail"], v["success_rate"] * 100,
            v["rms_arcsec_stats"]["median"], v["duration_stats"]["mean"],
        ))
    print("")
    print("按滤镜:")
    for flt, v in sorted(by_filter.items()):
        print("  %-10s total=%3d success=%3d fail=%3d rate=%.1f%%" % (
            flt, v["total"], v["success"], v["fail"], v["success_rate"] * 100,
        ))
    print("")
    print("前 %d 帧重复性 (3 次运行):" % args.repeat_first)
    for rp in repeatability:
        if rp["success_count"] >= 3:
            print("  [%d] %s: dRA=%.6f° dDec=%.6f° rms_std=%.4f\" dur_std=%.3fs" % (
                rp["index"], rp["filename"][:40],
                rp["wcs_diff_ra_deg"], rp["wcs_diff_dec_deg"],
                rp["wcs_diff_rms_arcsec"], rp["duration_std"],
            ))
        else:
            print("  [%d] %s: 成功 %d/3, %s" % (
                rp["index"], rp["filename"][:40], rp["success_count"], rp.get("note", "")
            ))
    print("=" * 60)

    return 0 if n_fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
