#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P10-006: T1-T4 真实校准代表帧验证

依据:
- tasks/P10-006.md
- docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md

目标:
- 每套设备 (T2/T3/T4) 和滤镜类选 1 个代表帧, 实际执行校准
- 验证尺寸/统计/坏点 (NaN/Inf/saturated)
- T1 无数据 (跳过); T2/T4 缺 Lum Flat (跳过 Lum 代表帧)
- 总代表帧数 = T2 (5 滤镜) + T3 (6 滤镜) + T4 (5 滤镜) = 16

校准公式 (与 lib/calibration/python/calibrator.py 一致):
- Calibrated = (Light - Dark) / NormalizedFlat
- Dark 已含 Bias (P10-003 masterDark 由 dark 帧堆叠且未减 Bias)
- Flat 归一化到 median=1.0, 最小值裁剪 0.1

输出:
- REPRESENTATIVE_CALIBRATION_REPORT.csv - 每行一个代表帧校准结果
- CALIBRATION_VALIDATION_SUMMARY.json - 汇总统计
- calibrated/<device>_<filter>_calibrated.fits - 校准后 FITS (代表帧示例)
"""
from __future__ import annotations

import csv
import json
import os
import sys
import time
import traceback
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

# ---- 加载项目统一 IO 接口 ----
REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
AIO_DIR = REPO_ROOT / "lib" / "astro_image_io" / "python"
CALIB_DIR = REPO_ROOT / "lib" / "calibration" / "python"
for p in (str(AIO_DIR), str(CALIB_DIR)):
    if p not in sys.path:
        sys.path.insert(0, p)

from astro_image_io import ImageReader  # noqa: E402

P10_001_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-001"
P10_002_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-002"
P10_003_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-003"
P10_004_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-004"
P10_005_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-005"
P10_006_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-006"
RAW_LOGS_DIR = P10_006_DIR / "raw_logs"
CALIBRATED_DIR = P10_006_DIR / "calibrated"
TESTDATA_DIR = REPO_ROOT / "testdata"

# T2/T4 缺 Lum Flat (P10-002 已记录, P10-005 123 unresolved)
# T3 全部 6 滤镜均有 Flat
EXPECTED_REPRESENTATIVES = [
    # (device, canonical_filter)
    ("T2", "BLUE"), ("T2", "GREEN"), ("T2", "HA"), ("T2", "OIII"), ("T2", "RED"),
    ("T3", "BLUE"), ("T3", "GREEN"), ("T3", "HA"), ("T3", "LUM"),
    ("T3", "OIII"), ("T3", "RED"),
    ("T4", "BLUE"), ("T4", "GREEN"), ("T4", "HA"), ("T4", "OIII"), ("T4", "RED"),
]
EXPECTED_TOTAL = len(EXPECTED_REPRESENTATIVES)  # 16


def iso_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="microseconds").replace("+00:00", "Z")


def log_print(msg: str, log_file: Path | None = None) -> None:
    print(msg, flush=True)
    if log_file is not None:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(msg + "\n")


def load_filter_alias_map() -> dict:
    with open(P10_004_DIR / "FILTER_ALIAS_MAP.json", "r", encoding="utf-8") as f:
        return json.load(f)


def normalize_filter(fmap: dict, alias: str) -> str | None:
    if not alias:
        return None
    key = alias.strip()
    if not key:
        return None
    a2c = fmap.get("alias_to_canonical", {})
    if key in a2c:
        return a2c[key]
    upper = key.upper()
    if upper in a2c:
        return a2c[upper]
    return None


def select_representative_frames() -> list[dict]:
    """从 P10-005 LIGHT_TO_MASTER_RESOLUTION.csv 中, 每个 (device, canonical_filter) 选第一个 resolved=YES 的帧."""
    csv_path = P10_005_DIR / "LIGHT_TO_MASTER_RESOLUTION.csv"
    with open(csv_path, "r", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    selected: dict[tuple[str, str], dict] = {}
    for r in rows:
        if r["resolved"] != "YES":
            continue
        key = (r["device_id"], r["filter_canonical"])
        if key in EXPECTED_REPRESENTATIVES and key not in selected:
            selected[key] = r
    return list(selected.values())


def load_master_image(reader: ImageReader, master_rel_path: str) -> np.ndarray:
    """加载 master 文件 (.xisf) 返回 float32 数组."""
    abs_path = REPO_ROOT / master_rel_path
    if not abs_path.exists():
        raise FileNotFoundError(f"Master not found: {abs_path}")
    with reader.read_xisf(str(abs_path)) as img:
        return img.data.astype(np.float32, copy=True)


def calibrate_representative(
    reader: ImageReader,
    light_path: str,
    bias_path: str,
    dark_path: str,
    flat_path: str,
) -> tuple[np.ndarray, dict]:
    """对单个代表帧执行标准校准.

    公式: Calibrated = (Light - Dark) / NormalizedFlat
    Dark 已含 Bias, 直接减 Dark 即可.
    Flat 归一化到 median=1.0, 最小值裁剪 0.1.
    """
    stats: dict = {}

    # 1. 读 Light
    abs_light = REPO_ROOT / light_path
    if not abs_light.exists():
        raise FileNotFoundError(f"Light not found: {abs_light}")
    with reader.read(str(abs_light)) as img:
        light = img.data.astype(np.float32, copy=True)
        light_shape = img.shape
    stats["light_shape"] = list(light_shape)
    stats["light_min"] = float(np.min(light))
    stats["light_max"] = float(np.max(light))
    stats["light_mean"] = float(np.mean(light))
    stats["light_std"] = float(np.std(light))
    stats["light_median"] = float(np.median(light))

    # 2. 读 Bias (用于尺寸/统计校验, 不参与公式但保留加载)
    bias = load_master_image(reader, bias_path)
    stats["bias_shape"] = list(bias.shape)
    stats["bias_mean"] = float(np.mean(bias))
    stats["bias_median"] = float(np.median(bias))

    # 3. 读 Dark (含 Bias)
    dark = load_master_image(reader, dark_path)
    stats["dark_shape"] = list(dark.shape)
    stats["dark_mean"] = float(np.mean(dark))
    stats["dark_median"] = float(np.median(dark))

    # 4. 读 Flat
    flat = load_master_image(reader, flat_path)
    stats["flat_shape"] = list(flat.shape)
    stats["flat_mean"] = float(np.mean(flat))
    stats["flat_median"] = float(np.median(flat))

    # 5. 尺寸一致性检查
    if light.shape != bias.shape:
        raise ValueError(f"Light/Bias shape mismatch: {light.shape} vs {bias.shape}")
    if light.shape != dark.shape:
        raise ValueError(f"Light/Dark shape mismatch: {light.shape} vs {dark.shape}")
    if light.shape != flat.shape:
        raise ValueError(f"Light/Flat shape mismatch: {light.shape} vs {flat.shape}")

    # 6. Flat 归一化 (median=1.0, 最小值裁剪 0.1)
    flat_median = float(np.median(flat))
    if flat_median <= 0:
        raise ValueError(f"Flat median <= 0: {flat_median}")
    flat_norm = flat / flat_median
    flat_norm = np.maximum(flat_norm, 0.1).astype(np.float32)
    stats["flat_norm_median"] = float(np.median(flat_norm))
    stats["flat_norm_min"] = float(np.min(flat_norm))
    stats["flat_norm_max"] = float(np.max(flat_norm))

    # 7. 标准校准: Calibrated = (Light - Dark) / NormalizedFlat
    calibrated = (light - dark) / flat_norm
    calibrated = calibrated.astype(np.float32)

    stats["calibrated_shape"] = list(calibrated.shape)
    stats["calibrated_min"] = float(np.min(calibrated))
    stats["calibrated_max"] = float(np.max(calibrated))
    stats["calibrated_mean"] = float(np.mean(calibrated))
    stats["calibrated_std"] = float(np.std(calibrated))
    stats["calibrated_median"] = float(np.median(calibrated))

    # 8. 坏点检查
    nan_count = int(np.sum(np.isnan(calibrated)))
    inf_count = int(np.sum(np.isinf(calibrated)))
    # 饱和 (Light 接近 16-bit 上限 65535)
    saturated_light = int(np.sum(light >= 65530))
    # 极端值 (校准后 abs > 1e6, 通常是 flat 异常导致)
    extreme_count = int(np.sum(np.abs(calibrated) > 1e6))
    stats["nan_count"] = nan_count
    stats["inf_count"] = inf_count
    stats["saturated_light_count"] = saturated_light
    stats["extreme_calibrated_count"] = extreme_count

    # 9. 坏点判定 (NaN/Inf 必须 0, extreme < 1% of pixels)
    total_pixels = calibrated.size
    bad_pixel_count = nan_count + inf_count + extreme_count
    stats["total_pixels"] = int(total_pixels)
    stats["bad_pixel_count"] = int(bad_pixel_count)
    stats["bad_pixel_ratio"] = float(bad_pixel_count / total_pixels) if total_pixels > 0 else 0.0

    return calibrated, stats


def write_calibrated_fits(reader: ImageReader, calibrated: np.ndarray, out_path: Path, light_path: str) -> None:
    """写校准后 FITS (float32, 调用项目统一 FITSWriter)."""
    from astro_image_io import FITSWriter, FITSKeywordPy  # type: ignore
    writer = FITSWriter()
    keywords = [
        FITSKeywordPy(name="CALIBRAT", value="TRUE", comment="P10-006 representative calibration"),
        FITSKeywordPy(name="SRCFRAME", value=os.path.basename(light_path), comment="source light frame"),
    ]
    # FITSWriter.write 签名: write(image_data, path, keywords=None, float_sample=True)
    writer.write(calibrated, str(out_path), keywords=keywords, float_sample=True)


def main() -> int:
    log_file = RAW_LOGS_DIR / "validate_representative_calibration.log"
    if log_file.exists():
        log_file.unlink()

    log_print("=" * 70, log_file)
    log_print("P10-006 validate_representative_calibration.py 启动", log_file)
    log_print(f"时间: {iso_now()}", log_file)
    log_print("=" * 70, log_file)

    # 阶段 1: 加载数据
    log_print("\n[阶段 1] 加载数据...", log_file)
    fmap = load_filter_alias_map()
    log_print(f"  滤镜别名映射: {len(fmap.get('alias_to_canonical', {}))} 别名", log_file)

    # 阶段 2: 选择代表帧
    log_print("\n[阶段 2] 选择代表帧...", log_file)
    representatives = select_representative_frames()
    log_print(f"  实际选中代表帧: {len(representatives)} (预期 {EXPECTED_TOTAL})", log_file)

    if len(representatives) < EXPECTED_TOTAL:
        found_keys = {(r["device_id"], r["filter_canonical"]) for r in representatives}
        missing = [k for k in EXPECTED_REPRESENTATIVES if k not in found_keys]
        log_print(f"  缺失组合: {missing}", log_file)

    # 阶段 3: 执行校准
    log_print("\n[阶段 3] 执行代表帧校准...", log_file)
    reader = ImageReader()
    results: list[dict] = []
    by_device_filter: dict[tuple[str, str], dict] = {}
    success_count = 0
    failure_count = 0

    for i, r in enumerate(representatives, 1):
        device = r["device_id"]
        filter_canonical = r["filter_canonical"]
        light_path = r["light_path"]
        bias_path = r["bias_master"]
        dark_path = r["dark_master"]
        flat_path = r["flat_master"]
        target = r["target"]
        exposure = float(r["exposure_s"])
        image_size = r["image_size"]

        log_print(f"\n  [{i}/{len(representatives)}] {device}/{filter_canonical} ({target}, {exposure}s, {image_size})", log_file)
        log_print(f"    Light: {light_path}", log_file)
        log_print(f"    Bias:  {bias_path}", log_file)
        log_print(f"    Dark:  {dark_path}", log_file)
        log_print(f"    Flat:  {flat_path}", log_file)

        t0 = time.time()
        try:
            calibrated, stats = calibrate_representative(
                reader, light_path, bias_path, dark_path, flat_path
            )
            elapsed = time.time() - t0
            log_print(f"    耗时: {elapsed:.2f}s", log_file)
            log_print(f"    校准后统计: min={stats['calibrated_min']:.2f}, max={stats['calibrated_max']:.2f}, "
                       f"mean={stats['calibrated_mean']:.2f}, std={stats['calibrated_std']:.2f}, "
                       f"median={stats['calibrated_median']:.2f}", log_file)
            log_print(f"    坏点: NaN={stats['nan_count']}, Inf={stats['inf_count']}, "
                       f"Saturated={stats['saturated_light_count']}, Extreme={stats['extreme_calibrated_count']}, "
                       f"Total={stats['total_pixels']}", log_file)

            # 判定通过/失败
            bad_pixel_threshold = 0.01  # 1% 允许极端值
            size_ok = (stats["light_shape"] == stats["bias_shape"] == stats["dark_shape"] == stats["flat_shape"])
            nan_ok = (stats["nan_count"] == 0)
            inf_ok = (stats["inf_count"] == 0)
            extreme_ok = (stats["bad_pixel_ratio"] < bad_pixel_threshold)
            stats_ok = (np.isfinite(stats["calibrated_mean"]) and np.isfinite(stats["calibrated_std"]))
            passed = size_ok and nan_ok and inf_ok and extreme_ok and stats_ok

            stats["passed"] = bool(passed)
            stats["size_ok"] = bool(size_ok)
            stats["nan_ok"] = bool(nan_ok)
            stats["inf_ok"] = bool(inf_ok)
            stats["extreme_ok"] = bool(extreme_ok)
            stats["stats_ok"] = bool(stats_ok)
            stats["elapsed_s"] = float(elapsed)

            if passed:
                success_count += 1
                log_print(f"    结果: PASS", log_file)
            else:
                failure_count += 1
                reasons = []
                if not size_ok: reasons.append("size_mismatch")
                if not nan_ok: reasons.append("nan_present")
                if not inf_ok: reasons.append("inf_present")
                if not extreme_ok: reasons.append("extreme_pixel_overflow")
                if not stats_ok: reasons.append("non_finite_stats")
                stats["failure_reasons"] = ",".join(reasons)
                log_print(f"    结果: FAIL ({stats['failure_reasons']})", log_file)

            # 写校准后 FITS (代表帧示例)
            out_fits = CALIBRATED_DIR / f"{device}_{filter_canonical}_calibrated.fits"
            try:
                write_calibrated_fits(reader, calibrated, out_fits, light_path)
                stats["output_fits"] = str(out_fits.relative_to(REPO_ROOT).as_posix())
                log_print(f"    写出: {out_fits.relative_to(REPO_ROOT).as_posix()}", log_file)
            except Exception as e:
                log_print(f"    写出失败: {e}", log_file)
                stats["output_fits"] = ""

            # 收集结果
            row = {
                "device_id": device,
                "filter_canonical": filter_canonical,
                "target": target,
                "exposure_s": exposure,
                "image_size": image_size,
                "light_path": light_path,
                "bias_master": bias_path,
                "dark_master": dark_path,
                "flat_master": flat_path,
                "light_shape": "x".join(str(s) for s in stats["light_shape"]),
                "light_min": stats["light_min"],
                "light_max": stats["light_max"],
                "light_mean": stats["light_mean"],
                "light_std": stats["light_std"],
                "light_median": stats["light_median"],
                "bias_mean": stats["bias_mean"],
                "bias_median": stats["bias_median"],
                "dark_mean": stats["dark_mean"],
                "dark_median": stats["dark_median"],
                "flat_mean": stats["flat_mean"],
                "flat_median": stats["flat_median"],
                "flat_norm_min": stats["flat_norm_min"],
                "flat_norm_max": stats["flat_norm_max"],
                "calibrated_shape": "x".join(str(s) for s in stats["calibrated_shape"]),
                "calibrated_min": stats["calibrated_min"],
                "calibrated_max": stats["calibrated_max"],
                "calibrated_mean": stats["calibrated_mean"],
                "calibrated_std": stats["calibrated_std"],
                "calibrated_median": stats["calibrated_median"],
                "nan_count": stats["nan_count"],
                "inf_count": stats["inf_count"],
                "saturated_light_count": stats["saturated_light_count"],
                "extreme_calibrated_count": stats["extreme_calibrated_count"],
                "total_pixels": stats["total_pixels"],
                "bad_pixel_count": stats["bad_pixel_count"],
                "bad_pixel_ratio": stats["bad_pixel_ratio"],
                "elapsed_s": stats["elapsed_s"],
                "passed": "YES" if stats["passed"] else "NO",
                "failure_reasons": stats.get("failure_reasons", ""),
                "output_fits": stats.get("output_fits", ""),
            }
            results.append(row)
            by_device_filter[(device, filter_canonical)] = stats

        except Exception as e:
            elapsed = time.time() - t0
            failure_count += 1
            tb = traceback.format_exc()
            log_print(f"    异常: {type(e).__name__}: {e}", log_file)
            log_print(tb, log_file)
            results.append({
                "device_id": device, "filter_canonical": filter_canonical, "target": target,
                "exposure_s": exposure, "image_size": image_size,
                "light_path": light_path, "bias_master": bias_path,
                "dark_master": dark_path, "flat_master": flat_path,
                "passed": "NO", "failure_reasons": f"exception: {type(e).__name__}: {e}",
                "elapsed_s": elapsed,
            })

    # 阶段 4: 写 CSV
    log_print(f"\n[阶段 4] 写入 REPRESENTATIVE_CALIBRATION_REPORT.csv...", log_file)
    out_csv = P10_006_DIR / "REPRESENTATIVE_CALIBRATION_REPORT.csv"
    fields = ["device_id", "filter_canonical", "target", "exposure_s", "image_size",
              "light_path", "bias_master", "dark_master", "flat_master",
              "light_shape", "light_min", "light_max", "light_mean", "light_std", "light_median",
              "bias_mean", "bias_median", "dark_mean", "dark_median",
              "flat_mean", "flat_median", "flat_norm_min", "flat_norm_max",
              "calibrated_shape", "calibrated_min", "calibrated_max", "calibrated_mean",
              "calibrated_std", "calibrated_median",
              "nan_count", "inf_count", "saturated_light_count", "extreme_calibrated_count",
              "total_pixels", "bad_pixel_count", "bad_pixel_ratio", "elapsed_s",
              "passed", "failure_reasons", "output_fits"]
    with open(out_csv, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        w.writerows(results)
    log_print(f"  [写入] {out_csv} ({len(results)} 行)", log_file)

    # 阶段 5: 写 JSON 汇总
    log_print(f"\n[阶段 5] 写入 CALIBRATION_VALIDATION_SUMMARY.json...", log_file)
    summary = {
        "_description": "P10-006 Representative Calibration Validation Summary",
        "generated_at": iso_now(),
        "expected_total": EXPECTED_TOTAL,
        "actual_total": len(results),
        "success_count": success_count,
        "failure_count": failure_count,
        "pass_rate": (success_count / len(results) * 100) if results else 0.0,
        "by_device": defaultdict(int),
        "by_filter": defaultdict(int),
        "representatives": results,
    }
    for r in results:
        summary["by_device"][r["device_id"]] += 1
        summary["by_filter"][r["filter_canonical"]] += 1
    summary["by_device"] = dict(summary["by_device"])
    summary["by_filter"] = dict(summary["by_filter"])

    out_json = P10_006_DIR / "CALIBRATION_VALIDATION_SUMMARY.json"
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2, default=str)
    log_print(f"  [写入] {out_json}", log_file)

    # 阶段 6: 汇总打印
    log_print("\n" + "=" * 70, log_file)
    log_print("汇总:", log_file)
    log_print(f"  预期代表帧数: {EXPECTED_TOTAL}", log_file)
    log_print(f"  实际校准数: {len(results)}", log_file)
    log_print(f"  成功: {success_count}", log_file)
    log_print(f"  失败: {failure_count}", log_file)
    log_print(f"  通过率: {summary['pass_rate']:.1f}%", log_file)
    log_print(f"\n  按设备:", log_file)
    for d, c in sorted(summary["by_device"].items()):
        log_print(f"    {d}: {c}", log_file)
    log_print(f"\n  按滤镜:", log_file)
    for f_, c in sorted(summary["by_filter"].items()):
        log_print(f"    {f_}: {c}", log_file)
    log_print("=" * 70, log_file)

    # 退出码: 全部通过 = 0
    return 0 if (success_count == len(results) and len(results) == EXPECTED_TOTAL) else 1


if __name__ == "__main__":
    raise SystemExit(main())
