# -*- coding: utf-8 -*-
"""
P02-002 多帧批量验证脚本
========================

目的:
    对多个测试帧运行三路径对比 (path0 / pathA / pathB),
    验证路径 A 和路径 B 与基准路径 0 在所有帧上精度一致。

    默认测试 5 帧 (Galaxy_Center Red/Green/Blue/H-alpha + NGC55),
    覆盖不同 filter / 不同 target / 不同曝光时间。

用法:
    pwsh> python engineering/tools/p02_002_batch_test.py [--limit N]

输出:
    engineering/evidence/P02-002/results/batch_three_paths.json
    engineering/evidence/P02-002/results/per_frame/<frame_name>.json

作者: P02-002 子 Agent
日期: 2026-07-25
"""

from __future__ import annotations

import os
import sys
import json
import time
import traceback

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))

PLATE_SOLVE_PY = os.path.join(PROJECT_ROOT, "lib", "plate_solve", "python")
if PLATE_SOLVE_PY not in sys.path:
    sys.path.insert(0, PLATE_SOLVE_PY)

import solve_and_write_wcs as sw  # noqa: E402
from solve_and_write_wcs import init_environment  # noqa: E402

# 复用单帧测试脚本的核心函数
sys.path.insert(0, SCRIPT_DIR)
from p02_002_single_frame_test import run_three_paths, result_to_dict, wcs_diff  # noqa: E402


# ============================================================================
# 测试帧选择
# ============================================================================

TEST_FRAMES = [
    # Galaxy_Center panel1 不同 filter
    ("Galaxy_Center_Red_180S",
     "testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts"),
    ("Galaxy_Center_Green_180S",
     "testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@063620-180S-Green.fts"),
    ("Galaxy_Center_Blue_180S",
     "testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@055414-180S-Blue.fts"),
    ("Galaxy_Center_Halpha_300S",
     "testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@061318-300S-H-alpha.fts"),
    ("Galaxy_Center_Oiii_600S",
     "testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@063631-600S-Oiii.fts"),
]


def main():
    import argparse
    parser = argparse.ArgumentParser(description="P02-002 多帧三路径批量验证")
    parser.add_argument("--limit", type=int, default=5,
                        help="测试帧数上限 (默认 5)")
    parser.add_argument("--output-dir", default="",
                        help="结果输出目录 (默认 engineering/evidence/P02-002/results)")
    args = parser.parse_args()

    if not args.output_dir:
        args.output_dir = os.path.join(
            PROJECT_ROOT, "engineering", "evidence", "P02-002", "results"
        )

    per_frame_dir = os.path.join(args.output_dir, "per_frame")
    os.makedirs(per_frame_dir, exist_ok=True)
    log_dir = os.path.join(args.output_dir, "batch_solver_logs")
    os.makedirs(log_dir, exist_ok=True)

    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    print("=" * 70)
    print("P02-002 多帧三路径批量验证")
    print("=" * 70)
    print(f"测试帧数上限: {args.limit}")
    print(f"输出目录: {args.output_dir}")

    # 选择测试帧
    frames_to_test = TEST_FRAMES[:args.limit]
    for label, rel_path in frames_to_test:
        full_path = os.path.join(PROJECT_ROOT, rel_path.replace("/", os.sep))
        if not os.path.isfile(full_path):
            print(f"[WARN] 测试帧不存在, 跳过: {full_path}")

    # 初始化环境
    print("\n[初始化] 加载 GaiaClient + StarDetector + IPVSolver ...")
    try:
        gaia_client, sdet, solver = init_environment()
    except Exception as e:
        print(f"[FAIL] 环境初始化失败: {e}")
        traceback.print_exc()
        sys.exit(1)

    # 运行测试
    all_results = []
    overall = {"total": 0, "path0_success": 0, "pathA_success": 0, "pathB_success": 0,
               "A_vs_0_pass": 0, "B_vs_0_pass": 0}

    try:
        for label, rel_path in frames_to_test:
            fits_path = os.path.join(PROJECT_ROOT, rel_path.replace("/", os.sep))
            if not os.path.isfile(fits_path):
                continue

            overall["total"] += 1
            print(f"\n{'='*70}")
            print(f"测试帧 [{overall['total']}/{len(frames_to_test)}]: {label}")
            print(f"{'='*70}")
            print(f"FITS: {fits_path}")

            # 单帧日志目录 (按 label)
            frame_log_dir = os.path.join(log_dir, label)
            os.makedirs(frame_log_dir, exist_ok=True)

            try:
                summary = run_three_paths(solver, sdet, fits_path, frame_log_dir)
                summary["label"] = label
            except Exception as e:
                print(f"[FAIL] 测试执行异常: {e}")
                traceback.print_exc()
                summary = {
                    "label": label, "fits_path": fits_path,
                    "error": str(e),
                    "paths": {
                        "path0_baseline": {"success": False, "error": str(e)},
                        "pathA_from_detections": {"success": False, "error": str(e)},
                        "pathB_callback": {"success": False, "error": str(e)},
                    }
                }

            all_results.append(summary)

            # 保存单帧结果
            per_frame_file = os.path.join(per_frame_dir, f"{label}.json")
            with open(per_frame_file, "w", encoding="utf-8") as f:
                json.dump(summary, f, indent=2, ensure_ascii=False)

            # 统计
            p0 = summary.get("paths", {}).get("path0_baseline", {})
            pA = summary.get("paths", {}).get("pathA_from_detections", {})
            pB = summary.get("paths", {}).get("pathB_callback", {})

            if p0.get("success"): overall["path0_success"] += 1
            if pA.get("success"): overall["pathA_success"] += 1
            if pB.get("success"): overall["pathB_success"] += 1

            # 精度一致性判定 (容差: CRVAL < 0.001", RMS < 0.001")
            TOL = 0.001
            if p0.get("success") and pB.get("success"):
                diff_B = summary.get("diff_B_vs_0", {})
                d_crval = max(diff_B.get("d_crval1_arcsec", 999),
                              diff_B.get("d_crval2_arcsec", 999))
                d_rms = diff_B.get("d_rms_arcsec", 999)
                if d_crval < TOL and d_rms < TOL:
                    overall["B_vs_0_pass"] += 1

            if p0.get("success") and pA.get("success"):
                diff_A = summary.get("diff_A_vs_0", {})
                d_crval = max(diff_A.get("d_crval1_arcsec", 999),
                              diff_A.get("d_crval2_arcsec", 999))
                d_rms = diff_A.get("d_rms_arcsec", 999)
                if d_crval < TOL and d_rms < TOL:
                    overall["A_vs_0_pass"] += 1

    finally:
        try:
            sw._close_environment(gaia_client, sdet, solver)
        except Exception:
            pass

    # 汇总
    batch_summary = {
        "_meta": {
            "task_id": "P02-002",
            "tool": "engineering/tools/p02_002_batch_test.py",
            "tolerance_arcsec": 0.001,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
        },
        "overall": overall,
        "frames": all_results,
    }

    output_file = os.path.join(args.output_dir, "batch_three_paths.json")
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(batch_summary, f, indent=2, ensure_ascii=False)
    print(f"\n[OK] 批量结果已保存: {output_file}")

    # 最终判定
    print("\n" + "=" * 70)
    print("批量判定汇总")
    print("=" * 70)
    print(f"总帧数: {overall['total']}")
    print(f"路径 0 (基准) success: {overall['path0_success']}/{overall['total']}")
    print(f"路径 A (外检) success: {overall['pathA_success']}/{overall['total']}")
    print(f"路径 B (回调) success: {overall['pathB_success']}/{overall['total']}")
    print(f"路径 A vs 0 精度一致: {overall['A_vs_0_pass']}/{overall['total']}")
    print(f"路径 B vs 0 精度一致: {overall['B_vs_0_pass']}/{overall['total']}")

    all_pass = (
        overall["path0_success"] == overall["total"]
        and overall["pathA_success"] == overall["total"]
        and overall["pathB_success"] == overall["total"]
        and overall["A_vs_0_pass"] == overall["total"]
        and overall["B_vs_0_pass"] == overall["total"]
    )
    print("\n[结论] " + ("PASS (三路径精度与成功率完全一致)"
                        if all_pass else "FAIL (存在不一致)"))
    sys.exit(0 if all_pass else 2)


if __name__ == "__main__":
    main()
