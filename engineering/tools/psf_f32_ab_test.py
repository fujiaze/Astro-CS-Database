# -*- coding: utf-8 -*-
"""
P02-005 PSF float32 API 单帧 A/B 验证工具
==========================================

功能:
    1. 读取真实 FITS 帧 (float32 或 uint16)
    2. 用 numpy 局部最大值检测生成 star_det v1 [N,6] 坐标
    3. 调用 dpsf_fit_batch_f32 (新 float32 API)
    4. 调用 dpsf_fit_batch (旧 uint16 API, 含 0-65535 clip)
    5. 对比两者 PSF 参数 (B/A/cx/cy/sx/sy/theta/fwhm_x/fwhm_y)
    6. 验证新 API 输出有效 (非 NaN, 合理范围)

依赖:
    astropy (FITS 读取)
    numpy
    lib/dynamic_psf/dynamic_psf.dll (新构建, 含 dpsf_fit_batch_f32)

用法:
    pwsh> python engineering/tools/psf_f32_ab_test.py
    pwsh> python engineering/tools/psf_f32_ab_test.py --fits <path> --n-stars 50

输出:
    engineering/evidence/P02-005/psf_f32_ab_result.json  (结构化结果)
    stdout 摘要

作者: P02-005 子 Agent
日期: 2026-07-25
"""

from __future__ import annotations

import os
import sys
import json
import math
import time
import argparse
from datetime import datetime

import numpy as np

# ============================================================================
# 项目根目录定位
# ============================================================================
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))

# 把 lib/dynamic_psf/python 加入 sys.path
DPSF_PY = os.path.join(PROJECT_ROOT, "lib", "dynamic_psf", "python")
if DPSF_PY not in sys.path:
    sys.path.insert(0, DPSF_PY)

# mingw64 DLL 依赖路径
MINGW_BIN = r"C:\msys64\mingw64\bin"
if os.path.isdir(MINGW_BIN):
    os.environ["PATH"] = MINGW_BIN + ";" + os.environ.get("PATH", "")
    try:
        os.add_dll_directory(MINGW_BIN)
    except OSError:
        pass

from dynamic_psf import (
    DynamicPSF, DPSFFitParamsPy,
    DPSF_FIT_OK, DPSF_FIT_STATUS_NAMES,
)

# ============================================================================
# FITS 读取
# ============================================================================
def read_fits_image(fits_path: str):
    """读取 FITS 主 HDU 图像数据, 返回 (data_float32, header, original_dtype)"""
    from astropy.io import fits
    with fits.open(fits_path, memmap=False) as hdul:
        hdu = hdul[0]
        data = hdu.data
        header = hdu.header
    original_dtype = data.dtype
    # 转 float32 (不 clip)
    data_f32 = np.asarray(data, dtype=np.float32)
    return data_f32, header, original_dtype


# ============================================================================
# 简单局部最大值检测 (不依赖 star_detector, 仅用于测试生成坐标)
# ============================================================================
def detect_local_maxima(image: np.ndarray, n_stars: int = 50,
                        threshold_sigma: float = 5.0,
                        min_distance: int = 10):
    """用 numpy 做简单的局部最大值检测, 返回 star_det v1 [N,6] 数组

    返回: detections np.ndarray shape=(N,6) dtype=float64
          列: [0]=x_px [1]=y_px [2]=flux [3]=mag [4]=saturated [5]=has_saturated
    """
    # 背景估计 (中位 + MAD)
    med = float(np.median(image))
    mad = float(np.median(np.abs(image - med))) * 1.4826
    if mad < 1e-6:
        mad = float(np.std(image))
    threshold = med + threshold_sigma * mad

    # 局部最大值: 用最大值滤波 (简单实现, 避免依赖 scipy)
    h, w = image.shape
    # 下采样以加速 (每 min_distance 像素取一个候选)
    candidates = []
    step = max(1, min_distance // 2)
    for y in range(step, h - step, step):
        for x in range(step, w - step, step):
            val = float(image[y, x])
            if val < threshold:
                continue
            # 检查是否为局部最大值
            patch = image[max(0, y - step):min(h, y + step + 1),
                          max(0, x - step):min(w, x + step + 1)]
            if val >= float(patch.max()):
                candidates.append((val, x, y))

    # 按亮度排序, 取前 n_stars
    candidates.sort(key=lambda c: -c[0])
    candidates = candidates[:n_stars]

    # 构造 star_det v1 [N,6]
    detections = np.zeros((len(candidates), 6), dtype=np.float64)
    for i, (val, x, y) in enumerate(candidates):
        flux = max(val - med, 1.0)
        mag = -2.5 * math.log10(flux) if flux > 0 else 99.0
        saturated = 1 if val > 60000 else 0
        detections[i, 0] = float(x)   # x_px
        detections[i, 1] = float(y)   # y_px
        detections[i, 2] = flux       # flux
        detections[i, 3] = mag        # mag
        detections[i, 4] = saturated  # saturated
        detections[i, 5] = saturated  # has_saturated

    return detections


# ============================================================================
# 主测试流程
# ============================================================================
def run_ab_test(fits_path: str, n_stars: int = 50, fit_radius: int = 8,
                dll_path: str | None = None) -> dict:
    """对单帧 FITS 执行 float32 vs uint16 PSF A/B 测试"""

    if dll_path is None:
        dll_path = os.path.join(PROJECT_ROOT, "lib", "dynamic_psf", "dynamic_psf.dll")

    result = {
        "fits_path": os.path.relpath(fits_path, PROJECT_ROOT),
        "n_stars_requested": n_stars,
        "fit_radius": fit_radius,
        "timestamp": datetime.now().isoformat(),
    }

    # 1. 读取 FITS
    t0 = time.perf_counter()
    img_f32, header, orig_dtype = read_fits_image(fits_path)
    t_read = time.perf_counter() - t0
    h, w = img_f32.shape
    result["image"] = {
        "width": int(w),
        "height": int(h),
        "original_dtype": str(orig_dtype),
        "read_sec": round(t_read, 3),
        "min": float(img_f32.min()),
        "max": float(img_f32.max()),
        "median": float(np.median(img_f32)),
    }

    # 2. 检测星点 (生成 star_det v1)
    t0 = time.perf_counter()
    detections = detect_local_maxima(img_f32, n_stars=n_stars)
    t_detect = time.perf_counter() - t0
    result["detections"] = {
        "count": int(detections.shape[0]),
        "schema": "star_det_v1:FLOAT64[N,6]",
        "detect_sec": round(t_detect, 3),
    }

    if detections.shape[0] == 0:
        result["error"] = "未检测到星点"
        return result

    # 3. 调用 dpsf_fit_batch_f32 (新 float32 API)
    params = DPSFFitParamsPy(fitRadius=fit_radius, maxIter=200, tolerance=1e-8)
    t0 = time.perf_counter()
    psf_f32, n_valid_f32 = DynamicPSF.fit_batch_f32(
        img_f32, detections, params=params, dll_path=dll_path)
    t_f32 = time.perf_counter() - t0
    result["f32_api"] = {
        "n_valid": int(n_valid_f32),
        "n_total": int(detections.shape[0]),
        "success_rate": round(n_valid_f32 / detections.shape[0], 4),
        "elapsed_sec": round(t_f32, 3),
    }

    # 4. 调用 dpsf_fit_batch (旧 uint16 API, 含 0-65535 clip)
    img_u16 = np.clip(img_f32, 0, 65535).astype(np.uint16)
    cx_list = detections[:, 0].tolist()
    cy_list = detections[:, 1].tolist()
    t0 = time.perf_counter()
    results_u16 = DynamicPSF.fit_batch(
        img_u16, cx_list, cy_list, params=params, dll_path=dll_path)
    t_u16 = time.perf_counter() - t0
    n_valid_u16 = sum(1 for r in results_u16 if r.status == DPSF_FIT_OK)
    result["u16_api"] = {
        "n_valid": int(n_valid_u16),
        "n_total": int(detections.shape[0]),
        "success_rate": round(n_valid_u16 / detections.shape[0], 4),
        "elapsed_sec": round(t_u16, 3),
    }

    # 5. 逐星对比 (只对比两者都成功的星点)
    psf_u16 = np.zeros((len(results_u16), 9), dtype=np.float64)
    for i, r in enumerate(results_u16):
        if r.status == DPSF_FIT_OK:
            psf_u16[i, 0] = r.B
            psf_u16[i, 1] = r.A
            psf_u16[i, 2] = r.cx
            psf_u16[i, 3] = r.cy
            psf_u16[i, 4] = r.sx
            psf_u16[i, 5] = r.sy
            psf_u16[i, 6] = r.theta
            psf_u16[i, 7] = r.fwhm_x
            psf_u16[i, 8] = r.fwhm_y
        else:
            psf_u16[i, :] = float("nan")

    # 对比统计
    field_names = ["B", "A", "cx", "cy", "sx", "sy", "theta", "fwhm_x", "fwhm_y"]
    both_ok_mask = ~np.isnan(psf_f32[:, 0]) & ~np.isnan(psf_u16[:, 0])
    n_both = int(both_ok_mask.sum())

    comparison = {
        "n_both_ok": n_both,
        "fields": {},
    }
    if n_both > 0:
        for k, name in enumerate(field_names):
            f32_vals = psf_f32[both_ok_mask, k]
            u16_vals = psf_u16[both_ok_mask, k]
            diff = f32_vals - u16_vals
            abs_rel_diff = np.abs(diff) / (np.abs(u16_vals) + 1e-30)
            comparison["fields"][name] = {
                "f32_mean": round(float(np.mean(f32_vals)), 6),
                "u16_mean": round(float(np.mean(u16_vals)), 6),
                "diff_mean": round(float(np.mean(diff)), 6),
                "diff_abs_mean": round(float(np.mean(np.abs(diff))), 6),
                "abs_rel_diff_mean": round(float(np.mean(abs_rel_diff)), 6),
                "abs_rel_diff_max": round(float(np.max(abs_rel_diff)), 6),
            }
    result["comparison"] = comparison

    # 6. 验证新 API 输出有效性 (非 NaN, 合理范围)
    valid_mask = ~np.isnan(psf_f32[:, 0])
    validity = {
        "n_valid_non_nan": int(valid_mask.sum()),
        "checks": [],
    }
    if valid_mask.sum() > 0:
        v = psf_f32[valid_mask]
        checks = [
            ("A > 0",             bool(np.all(v[:, 1] > 0))),
            ("sx > 0.3",          bool(np.all(v[:, 4] > 0.3))),
            ("sy > 0.3",          bool(np.all(v[:, 5] > 0.3))),
            ("fwhm_x > 0",        bool(np.all(v[:, 7] > 0))),
            ("fwhm_y > 0",        bool(np.all(v[:, 8] > 0))),
            ("fwhm_x < 50",       bool(np.all(v[:, 7] < 50))),
            ("fwhm_y < 50",       bool(np.all(v[:, 8] < 50))),
            ("B finite",          bool(np.all(np.isfinite(v[:, 0])))),
            ("cx in image",       bool(np.all((v[:, 2] >= 0) & (v[:, 2] < w)))),
            ("cy in image",       bool(np.all((v[:, 3] >= 0) & (v[:, 3] < h)))),
        ]
        validity["checks"] = [{"name": n, "pass": p} for n, p in checks]
        validity["all_pass"] = bool(all(p for _, p in checks))
    result["f32_validity"] = validity

    # 7. 摘要输出 (前 5 颗星)
    summary_rows = []
    n_show = min(5, detections.shape[0])
    for i in range(n_show):
        row = {"star_idx": i, "cx": float(detections[i, 0]), "cy": float(detections[i, 1])}
        if not np.isnan(psf_f32[i, 0]):
            row["f32"] = {
                "B": round(float(psf_f32[i, 0]), 3),
                "A": round(float(psf_f32[i, 1]), 3),
                "fwhm_x": round(float(psf_f32[i, 7]), 4),
                "fwhm_y": round(float(psf_f32[i, 8]), 4),
            }
        else:
            row["f32"] = None
        if not np.isnan(psf_u16[i, 0]):
            row["u16"] = {
                "B": round(float(psf_u16[i, 0]), 3),
                "A": round(float(psf_u16[i, 1]), 3),
                "fwhm_x": round(float(psf_u16[i, 7]), 4),
                "fwhm_y": round(float(psf_u16[i, 8]), 4),
            }
        else:
            row["u16"] = None
        summary_rows.append(row)
    result["summary_first_5"] = summary_rows

    return result


def main():
    parser = argparse.ArgumentParser(description="P02-005 PSF float32 API A/B 测试")
    parser.add_argument("--fits", default=None,
                        help="FITS 文件路径 (默认使用 testdata 首帧)")
    parser.add_argument("--n-stars", type=int, default=50,
                        help="检测的星点数 (默认 50)")
    parser.add_argument("--fit-radius", type=int, default=8,
                        help="拟合半径 (默认 8)")
    parser.add_argument("--output", default=None,
                        help="输出 JSON 路径")
    args = parser.parse_args()

    # 默认 FITS 文件
    if args.fits is None:
        args.fits = os.path.join(
            PROJECT_ROOT, "testdata", "Galaxy_Center_T4", "lights", "panel1",
            "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts")

    if not os.path.exists(args.fits):
        print(f"ERROR: FITS 文件不存在: {args.fits}", file=sys.stderr)
        sys.exit(1)

    print(f"==== P02-005 PSF float32 API A/B 测试 ====")
    print(f"FITS: {args.fits}")
    print(f"n_stars: {args.n_stars}, fit_radius: {args.fit_radius}")
    print()

    result = run_ab_test(args.fits, n_stars=args.n_stars,
                         fit_radius=args.fit_radius)

    # 摘要输出
    print("---- 图像信息 ----")
    print(f"  尺寸: {result['image']['width']}x{result['image']['height']}")
    print(f"  原始 dtype: {result['image']['original_dtype']}")
    print(f"  像素范围: [{result['image']['min']:.1f}, {result['image']['max']:.1f}]")
    print(f"  中位值: {result['image']['median']:.1f}")
    print()
    print("---- 星点检测 ----")
    print(f"  检测到: {result['detections']['count']} 颗")
    print()
    print("---- float32 API (新) ----")
    f32 = result["f32_api"]
    print(f"  有效拟合: {f32['n_valid']}/{f32['n_total']} ({f32['success_rate']*100:.1f}%)")
    print(f"  耗时: {f32['elapsed_sec']:.3f}s")
    print()
    print("---- uint16 API (旧, 含 clip) ----")
    u16 = result["u16_api"]
    print(f"  有效拟合: {u16['n_valid']}/{u16['n_total']} ({u16['success_rate']*100:.1f}%)")
    print(f"  耗时: {u16['elapsed_sec']:.3f}s")
    print()
    print("---- A/B 对比 ----")
    cmp = result["comparison"]
    print(f"  两者都成功的星点: {cmp['n_both_ok']}")
    if cmp["n_both_ok"] > 0:
        print(f"  {'字段':<10} {'f32_mean':>12} {'u16_mean':>12} {'|diff|_mean':>12} {'rel_diff_max':>14}")
        for name, st in cmp["fields"].items():
            print(f"  {name:<10} {st['f32_mean']:>12.4f} {st['u16_mean']:>12.4f} "
                  f"{st['diff_abs_mean']:>12.4f} {st['abs_rel_diff_max']:>14.6f}")
    print()
    print("---- 新 API 输出有效性 ----")
    v = result["f32_validity"]
    print(f"  非 NaN 拟合数: {v['n_valid_non_nan']}")
    for chk in v["checks"]:
        mark = "PASS" if chk["pass"] else "FAIL"
        print(f"  [{mark}] {chk['name']}")
    print(f"  总体: {'PASS' if v.get('all_pass') else 'FAIL'}")
    print()
    print("---- 前 5 颗星摘要 ----")
    for row in result["summary_first_5"]:
        f32_str = "NaN"
        if row["f32"]:
            f32_str = f"B={row['f32']['B']:.1f} A={row['f32']['A']:.1f} fwhm=({row['f32']['fwhm_x']:.2f},{row['f32']['fwhm_y']:.2f})"
        u16_str = "NaN"
        if row["u16"]:
            u16_str = f"B={row['u16']['B']:.1f} A={row['u16']['A']:.1f} fwhm=({row['u16']['fwhm_x']:.2f},{row['u16']['fwhm_y']:.2f})"
        print(f"  star[{row['star_idx']}] ({row['cx']:.1f},{row['cy']:.1f})")
        print(f"    f32: {f32_str}")
        print(f"    u16: {u16_str}")

    # 写 JSON
    if args.output is None:
        out_dir = os.path.join(PROJECT_ROOT, "engineering", "evidence", "P02-005")
        os.makedirs(out_dir, exist_ok=True)
        args.output = os.path.join(out_dir, "psf_f32_ab_result.json")
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)
    print()
    print(f"JSON 结果已写入: {args.output}")

    # 退出码: 新 API 有效性全部通过且有效拟合数 > 0 则 PASS
    ok = (result.get("f32_validity", {}).get("all_pass", False)
          and result["f32_api"]["n_valid"] > 0)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
