# -*- coding: utf-8 -*-
"""
实验脚本：CCD校准模块验证
功能：从testdata选取6帧覆盖所有滤镜/曝光组合，使用调试模式运行校准+坏点修复
用途：验证校准模块功能正确性，输出FITS和统计对比日志

选取的6帧覆盖：
  - 滤镜: Red / Green / Blue / H-alpha / Oiii（全部5种）
  - 曝光: 180s / 300s / 600s（全部3种）
  - Oiii 取两帧 600s 用于一致性观察

输出：
  - FITS: {lib}/calibration/logs/experiment/{原名}_calibrated.fits 和 _final.fits
  - 日志: {lib}/calibration/logs/experiment/experiment_summary.txt
  - 控制台: 统计对比表 + 总结
"""

from __future__ import annotations

import os
import sys
import time
import numpy as np

# ============================ 路径常量 ============================
PROJECT_ROOT = r"f:\Astro dev\Astro CS Normalization Database"
CALIB_DIR = os.path.join(
    PROJECT_ROOT, "testdata", "T4_data", "calibration files"
)
LIGHTS_DIR = os.path.join(
    PROJECT_ROOT, "testdata", "T4_data", "lights", "panel1"
)
OUTPUT_DIR = os.path.join(PROJECT_ROOT, "lib", "calibration", "logs", "experiment")
SUMMARY_FILE = os.path.join(OUTPUT_DIR, "experiment_summary.txt")

# ============================ 实验帧列表 ============================
# (文件名, 滤镜, 曝光时间s) —— 覆盖所有滤镜/曝光组合
EXPERIMENT_FRAMES = [
    ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts", "Red", 180.0),
    ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@063620-180S-Green.fts", "Green", 180.0),
    ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@055414-180S-Blue.fts", "Blue", 180.0),
    ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@061318-300S-H-alpha.fts", "H-alpha", 300.0),
    ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@063631-600S-Oiii.fts", "Oiii", 600.0),
    ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@064722-600S-Oiii.fts", "Oiii", 600.0),
]

# ============================ 模块路径注入 ============================
# 将 calibration_pipeline.py 所在目录加入 sys.path
_PIPELINE_DIR = os.path.join(PROJECT_ROOT, "lib", "calibration", "python")
if _PIPELINE_DIR not in sys.path:
    sys.path.insert(0, _PIPELINE_DIR)
# 将 astro_image_io 所在目录加入 sys.path（用于读取最终 FITS 统计）
_IO_DIR = os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "python")
if _IO_DIR not in sys.path:
    sys.path.insert(0, _IO_DIR)

from calibration_pipeline import CalibrationPipeline  # noqa: E402
from astro_image_io import ImageReader  # noqa: E402


# ============================ 工具函数 ============================

def build_master_paths(calib_dir, filter_name, exposure):
    """
    构建主校准帧路径。

    Args:
        calib_dir: 主校准帧目录
        filter_name: 滤镜名（如 "Red", "H-alpha"）
        exposure: 曝光时间（秒，浮点）

    Returns:
        (bias_path, dark_path, flat_path)
    """
    bias_path = os.path.join(calib_dir, "masterBias_BIN-1_4500x3600.xisf")
    dark_path = os.path.join(
        calib_dir, f"masterDark_BIN-1_4500x3600_EXPOSURE-{exposure:.2f}s.xisf"
    )
    flat_path = os.path.join(
        calib_dir, f"masterFlat_BIN-1_4500x3600_FILTER-{filter_name}_mono.xisf"
    )
    return bias_path, dark_path, flat_path


def compute_fits_stats(fits_path, reader):
    """
    读取 FITS 文件并计算统计信息（min/max/mean/std）。

    Args:
        fits_path: FITS 文件路径
        reader: ImageReader 实例

    Returns:
        dict: {min, max, mean, std}；读取失败返回空 dict
    """
    try:
        img = reader.read(fits_path)
        try:
            data = img.data.astype(np.float64)
        finally:
            img.close()
        return {
            "min": float(np.min(data)),
            "max": float(np.max(data)),
            "mean": float(np.mean(data)),
            "std": float(np.std(data)),
        }
    except Exception as e:
        print(f"[警告] 读取 FITS 统计失败: {fits_path} -> {e}")
        return {}


def fmt(val, width=12, prec=4):
    """格式化浮点数为定宽字符串"""
    if val is None:
        return "N/A".rjust(width)
    return f"{val:.{prec}f}".rjust(width)


def fmt_int(val, width=8):
    """格式化整数为定宽字符串"""
    if val is None:
        return "N/A".rjust(width)
    return str(int(val)).rjust(width)


# ============================ 主函数 ============================

def main():
    """
    实验主流程：
      1. 创建输出目录
      2. 创建 CalibrationPipeline(mode="debug")
      3. 对每帧构建主帧路径并调用 pipeline.run()
      4. 收集结果，读取最终 FITS 计算坏点修复后统计
      5. 输出统计对比表到控制台和 experiment_summary.txt
    """
    # Windows 控制台强制 UTF-8，避免中文路径/日志乱码
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    print("=" * 80)
    print("实验脚本：CCD校准模块验证（调试模式）")
    print("=" * 80)
    print(f"项目根目录: {PROJECT_ROOT}")
    print(f"主校准帧目录: {CALIB_DIR}")
    print(f"Light帧目录: {LIGHTS_DIR}")
    print(f"输出目录: {OUTPUT_DIR}")
    print(f"实验帧数: {len(EXPERIMENT_FRAMES)}")
    print("=" * 80)

    # 1. 创建输出目录
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"[步骤1] 输出目录已创建: {OUTPUT_DIR}")

    # 2. 创建管线（调试模式，16线程）
    print("[步骤2] 创建 CalibrationPipeline(mode=debug, max_workers=16)")
    pipeline = CalibrationPipeline(mode="debug", max_workers=16)
    reader = ImageReader()

    # 3. 逐帧运行校准+坏点修复
    results = []
    n_total = len(EXPERIMENT_FRAMES)
    n_success = 0
    t_start = time.time()

    for i, (fname, filt, exposure) in enumerate(EXPERIMENT_FRAMES, 1):
        light_path = os.path.join(LIGHTS_DIR, fname)
        print("\n" + "#" * 80)
        print(f"# 处理帧 {i}/{n_total}: {fname}")
        print(f"# 滤镜={filt}, 曝光={exposure}s")
        print("#" * 80)

        # 检查 Light 帧是否存在
        if not os.path.isfile(light_path):
            print(f"[错误] Light 帧不存在: {light_path}")
            results.append({
                "filename": fname, "filter": filt, "exposure": exposure,
                "success": False, "error": "Light帧不存在",
                "original": {}, "calibrated": {}, "final": {},
                "hot_pixels": None, "cold_pixels": None,
                "mean_change_pct": None,
            })
            continue

        # 构建主帧路径
        bias_path, dark_path, flat_path = build_master_paths(CALIB_DIR, filt, exposure)
        print(f"  Master Bias : {bias_path}")
        print(f"  Master Dark : {dark_path}")
        print(f"  Master Flat : {flat_path}")

        # 校验主帧文件存在性
        for label, p in [("Bias", bias_path), ("Dark", dark_path), ("Flat", flat_path)]:
            if not os.path.isfile(p):
                print(f"  [警告] Master {label} 不存在: {p}")

        # 调用管线（显式传入主帧路径，不使用 calibration_dir 自动匹配）
        t_frame = time.time()
        result = pipeline.run(
            light_path, OUTPUT_DIR,
            master_bias=bias_path, master_dark=dark_path, master_flat=flat_path,
            calibration_dir=None,
            dark_optimization=False,
            cc_method="median",
            hot_sigma=5.0, cold_sigma=5.0,
            max_structure_size=4,
            enable_cosmetic_correction=True,
        )
        t_frame_cost = time.time() - t_frame

        # 提取校准前后统计（来自 calibrator 内部计算）
        cal_stats = result.get("calibrated_stats", {})
        original_stats = cal_stats.get("before", {})
        calibrated_stats = cal_stats.get("after", {})

        # 提取坏点修复统计
        cc_stats = result.get("cc_stats", {})
        hot_pixels = cc_stats.get("hot_pixels") if cc_stats.get("success") else None
        cold_pixels = cc_stats.get("cold_pixels") if cc_stats.get("success") else None

        # 读取最终 FITS 计算坏点修复后的完整统计
        # debug 模式: cc 成功输出 _final.fits，失败则回退到 _calibrated.fits
        final_stats = {}
        final_fits_path = result.get("output_path")
        if final_fits_path and os.path.isfile(final_fits_path):
            final_stats = compute_fits_stats(final_fits_path, reader)

        # 计算均值变化百分比（校准前 -> 坏点修复后）
        mean_change_pct = None
        orig_mean = original_stats.get("mean")
        final_mean = final_stats.get("mean")
        if orig_mean is not None and final_mean is not None and orig_mean != 0:
            mean_change_pct = (final_mean - orig_mean) / abs(orig_mean) * 100.0

        success = result.get("success", False)
        if success:
            n_success += 1

        print(f"  结果: {'成功' if success else '失败'}  耗时: {t_frame_cost:.2f}s")
        if not success:
            print(f"  错误: {result.get('error', '未知')}")

        results.append({
            "filename": fname,
            "filter": filt,
            "exposure": exposure,
            "success": success,
            "error": result.get("error"),
            "original": original_stats,
            "calibrated": calibrated_stats,
            "final": final_stats,
            "hot_pixels": hot_pixels,
            "cold_pixels": cold_pixels,
            "mean_change_pct": mean_change_pct,
            "calibrated_path": result.get("calibrated_path"),
            "final_path": final_fits_path,
            "cost_sec": t_frame_cost,
        })

    t_total = time.time() - t_start

    # 4. 生成统计对比表
    print("\n\n" + "=" * 80)
    print("实验总结")
    print("=" * 80)
    print(f"总帧数: {n_total}  成功: {n_success}  失败: {n_total - n_success}  总耗时: {t_total:.2f}s")

    summary_lines = []
    summary_lines.append("=" * 80)
    summary_lines.append("CCD校准模块验证实验 - 统计对比报告")
    summary_lines.append(f"生成时间: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    summary_lines.append(f"总帧数: {n_total}  成功: {n_success}  失败: {n_total - n_success}  总耗时: {t_total:.2f}s")
    summary_lines.append("=" * 80)

    for i, r in enumerate(results, 1):
        summary_lines.append("")
        summary_lines.append("-" * 80)
        summary_lines.append(
            f"帧 {i}/{n_total}: {r['filename']}"
        )
        summary_lines.append(
            f"  滤镜={r['filter']}, 曝光={r['exposure']}s, "
            f"结果={'成功' if r['success'] else '失败'}, 耗时={r.get('cost_sec', 0):.2f}s"
        )
        if not r["success"]:
            summary_lines.append(f"  错误: {r.get('error', '未知')}")
            continue

        orig = r["original"]
        cal = r["calibrated"]
        fin = r["final"]

        # 统计对比表（原始 / 校准后 / 坏点修复后）
        summary_lines.append("")
        summary_lines.append(
            f"  {'阶段':<12} {'min':>12} {'max':>12} {'mean':>12} {'std':>12}"
        )
        summary_lines.append(
            f"  {'-'*12} {'-'*12} {'-'*12} {'-'*12} {'-'*12}"
        )
        summary_lines.append(
            f"  {'原始':<12} {fmt(orig.get('min'))} {fmt(orig.get('max'))} "
            f"{fmt(orig.get('mean'))} {fmt(orig.get('std'))}"
        )
        summary_lines.append(
            f"  {'校准后':<12} {fmt(cal.get('min'))} {fmt(cal.get('max'))} "
            f"{fmt(cal.get('mean'))} {fmt(cal.get('std'))}"
        )
        summary_lines.append(
            f"  {'坏点修复后':<12} {fmt(fin.get('min'))} {fmt(fin.get('max'))} "
            f"{fmt(fin.get('mean'))} {fmt(fin.get('std'))}"
        )
        summary_lines.append("")
        summary_lines.append(
            f"  热像素数: {fmt_int(r.get('hot_pixels'))}    "
            f"冷像素数: {fmt_int(r.get('cold_pixels'))}    "
            f"均值变化: {fmt(r.get('mean_change_pct'), 10, 2)}%"
        )
        if r.get("calibrated_path"):
            summary_lines.append(f"  校准FITS: {r['calibrated_path']}")
        if r.get("final_path"):
            summary_lines.append(f"  最终FITS: {r['final_path']}")

    # 滤镜/曝光分组汇总
    summary_lines.append("")
    summary_lines.append("=" * 80)
    summary_lines.append("按滤镜/曝光分组汇总")
    summary_lines.append("=" * 80)
    summary_lines.append(
        f"  {'帧':<6} {'滤镜':<10} {'曝光(s)':>8} {'结果':<6} "
        f"{'原始mean':>12} {'校准mean':>12} {'最终mean':>12} "
        f"{'热像素':>8} {'冷像素':>8} {'变化%':>10}"
    )
    summary_lines.append(
        f"  {'-'*6} {'-'*10} {'-'*8} {'-'*6} "
        f"{'-'*12} {'-'*12} {'-'*12} {'-'*8} {'-'*8} {'-'*10}"
    )
    for i, r in enumerate(results, 1):
        orig = r["original"]
        cal = r["calibrated"]
        fin = r["final"]
        summary_lines.append(
            f"  {i:<6} {r['filter']:<10} {r['exposure']:>8.1f} "
            f"{'成功' if r['success'] else '失败':<6} "
            f"{fmt(orig.get('mean'))} {fmt(cal.get('mean'))} {fmt(fin.get('mean'))} "
            f"{fmt_int(r.get('hot_pixels'))} {fmt_int(r.get('cold_pixels'))} "
            f"{fmt(r.get('mean_change_pct'), 10, 2)}"
        )

    summary_lines.append("")
    summary_lines.append("=" * 80)
    summary_lines.append("实验完成")
    summary_lines.append("=" * 80)

    # 打印到控制台
    summary_text = "\n".join(summary_lines)
    print()
    print(summary_text)

    # 写入 experiment_summary.txt（UTF-8）
    with open(SUMMARY_FILE, "w", encoding="utf-8") as f:
        f.write(summary_text + "\n")
    print(f"\n统计报告已写入: {SUMMARY_FILE}")

    return 0 if n_success == n_total else 1


if __name__ == "__main__":
    sys.exit(main())
