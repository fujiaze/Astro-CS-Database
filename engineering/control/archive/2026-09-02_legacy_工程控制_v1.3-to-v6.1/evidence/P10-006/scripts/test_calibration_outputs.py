#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P10-006 测试套件: 校准输出独立验证.

覆盖维度:
- contract: 交付物文件存在/格式/字段完整性
- unit: CSV 数据合法性 (无 NaN/Inf, 尺寸一致, 坏点判定正确)
- e2e: 端到端验证 (16 个代表帧全部 PASS, 100% 通过率)
- forbidden_shortcut: 禁止捷径 (无空数据/无虚构/无尺寸不一致)

23 项测试 (与 P10-005 测试规模一致).
"""
from __future__ import annotations

import csv
import json
import os
import sys
from pathlib import Path

import numpy as np

REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
P10_006_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-006"
CSV_PATH = P10_006_DIR / "REPRESENTATIVE_CALIBRATION_REPORT.csv"
JSON_PATH = P10_006_DIR / "CALIBRATION_VALIDATION_SUMMARY.json"
CALIBRATED_DIR = P10_006_DIR / "calibrated"

EXPECTED_REPRESENTATIVES = [
    ("T2", "BLUE"), ("T2", "GREEN"), ("T2", "HA"), ("T2", "OIII"), ("T2", "RED"),
    ("T3", "BLUE"), ("T3", "GREEN"), ("T3", "HA"), ("T3", "LUM"),
    ("T3", "OIII"), ("T3", "RED"),
    ("T4", "BLUE"), ("T4", "GREEN"), ("T4", "HA"), ("T4", "OIII"), ("T4", "RED"),
]
EXPECTED_TOTAL = 16


_passed = 0
_failed = 0


def check(cond: bool, name: str, detail: str = "") -> None:
    global _passed, _failed
    if cond:
        _passed += 1
        print(f"  [PASS] {name}", flush=True)
    else:
        _failed += 1
        print(f"  [FAIL] {name} {detail}", flush=True)


def load_csv() -> list[dict]:
    with open(CSV_PATH, "r", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def load_json() -> dict:
    with open(JSON_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


# ============ contract 测试 ============

def test_csv_exists() -> None:
    check(CSV_PATH.exists(), "CSV 文件存在", str(CSV_PATH))


def test_json_exists() -> None:
    check(JSON_PATH.exists(), "JSON 文件存在", str(JSON_PATH))


def test_csv_row_count() -> None:
    rows = load_csv()
    check(len(rows) == EXPECTED_TOTAL, f"CSV 行数 = {EXPECTED_TOTAL}", f"actual={len(rows)}")


def test_csv_fields_complete() -> None:
    rows = load_csv()
    required = ["device_id", "filter_canonical", "target", "exposure_s", "image_size",
                "light_path", "bias_master", "dark_master", "flat_master",
                "light_shape", "calibrated_shape",
                "calibrated_min", "calibrated_max", "calibrated_mean", "calibrated_std", "calibrated_median",
                "nan_count", "inf_count", "saturated_light_count", "extreme_calibrated_count",
                "total_pixels", "bad_pixel_count", "bad_pixel_ratio",
                "passed", "failure_reasons", "output_fits"]
    for r in rows:
        for f in required:
            if f not in r:
                check(False, "CSV 字段完整", f"missing field {f}")
                return
    check(True, "CSV 字段完整")


def test_json_summary_fields() -> None:
    s = load_json()
    required = ["_description", "generated_at", "expected_total", "actual_total",
                "success_count", "failure_count", "pass_rate",
                "by_device", "by_filter", "representatives"]
    for f in required:
        if f not in s:
            check(False, "JSON 汇总字段完整", f"missing {f}")
            return
    check(True, "JSON 汇总字段完整")


# ============ unit 测试 ============

def test_all_combinations_present() -> None:
    rows = load_csv()
    found = {(r["device_id"], r["filter_canonical"]) for r in rows}
    missing = [k for k in EXPECTED_REPRESENTATIVES if k not in found]
    check(not missing, "全部 16 个 (device, filter) 组合存在", f"missing={missing}")


def test_no_extra_combinations() -> None:
    rows = load_csv()
    found = {(r["device_id"], r["filter_canonical"]) for r in rows}
    extra = [k for k in found if k not in EXPECTED_REPRESENTATIVES]
    check(not extra, "无多余组合", f"extra={extra}")


def test_t1_not_present() -> None:
    rows = load_csv()
    t1_rows = [r for r in rows if r["device_id"] == "T1"]
    check(not t1_rows, "T1 无数据未参与", f"T1 rows={len(t1_rows)}")


def test_t2_lum_not_present() -> None:
    rows = load_csv()
    t2_lum = [r for r in rows if r["device_id"] == "T2" and r["filter_canonical"] == "LUM"]
    check(not t2_lum, "T2 缺 Lum Flat 未参与", f"T2 LUM rows={len(t2_lum)}")


def test_t4_lum_not_present() -> None:
    rows = load_csv()
    t4_lum = [r for r in rows if r["device_id"] == "T4" and r["filter_canonical"] == "LUM"]
    check(not t4_lum, "T4 缺 Lum Flat 未参与", f"T4 LUM rows={len(t4_lum)}")


def test_t3_lum_present() -> None:
    rows = load_csv()
    t3_lum = [r for r in rows if r["device_id"] == "T3" and r["filter_canonical"] == "LUM"]
    check(len(t3_lum) == 1, "T3 有 Lum Flat 参与", f"T3 LUM rows={len(t3_lum)}")


def test_no_nan_in_calibrated() -> None:
    rows = load_csv()
    for r in rows:
        if int(r["nan_count"]) != 0:
            check(False, "校准后无 NaN", f"{r['device_id']}/{r['filter_canonical']} nan={r['nan_count']}")
            return
    check(True, "校准后无 NaN")


def test_no_inf_in_calibrated() -> None:
    rows = load_csv()
    for r in rows:
        if int(r["inf_count"]) != 0:
            check(False, "校准后无 Inf", f"{r['device_id']}/{r['filter_canonical']} inf={r['inf_count']}")
            return
    check(True, "校准后无 Inf")


def test_size_consistency() -> None:
    rows = load_csv()
    for r in rows:
        light_shape = r["light_shape"]
        cal_shape = r["calibrated_shape"]
        if light_shape != cal_shape:
            check(False, "Light/Calibrated 尺寸一致",
                  f"{r['device_id']}/{r['filter_canonical']} light={light_shape} calibrated={cal_shape}")
            return
    check(True, "Light/Calibrated 尺寸一致")


def test_no_extreme_bad_pixel_ratio() -> None:
    rows = load_csv()
    for r in rows:
        ratio = float(r["bad_pixel_ratio"])
        if ratio >= 0.01:
            check(False, "坏点比例 < 1%", f"{r['device_id']}/{r['filter_canonical']} ratio={ratio}")
            return
    check(True, "坏点比例 < 1%")


def test_calibrated_stats_finite() -> None:
    rows = load_csv()
    for r in rows:
        try:
            mean = float(r["calibrated_mean"])
            std = float(r["calibrated_std"])
            if not (np.isfinite(mean) and np.isfinite(std)):
                check(False, "统计量有限", f"{r['device_id']}/{r['filter_canonical']} mean={mean} std={std}")
                return
        except (ValueError, TypeError):
            check(False, "统计量可解析", f"{r['device_id']}/{r['filter_canonical']}")
            return
    check(True, "统计量有限")


def test_calibrated_mean_positive() -> None:
    """所有代表帧校准后 mean 应 > 0 (天体信号 + 背景大于暗噪声)."""
    rows = load_csv()
    for r in rows:
        mean = float(r["calibrated_mean"])
        if mean <= 0:
            check(False, "校准后 mean > 0", f"{r['device_id']}/{r['filter_canonical']} mean={mean}")
            return
    check(True, "校准后 mean > 0")


# ============ e2e 测试 ============

def test_all_passed() -> None:
    rows = load_csv()
    failed = [r for r in rows if r["passed"] != "YES"]
    check(not failed, "全部 16 帧通过", f"failed={len(failed)}: {[r['device_id']+'/'+r['filter_canonical'] for r in failed]}")


def test_json_pass_rate_100() -> None:
    s = load_json()
    check(abs(s["pass_rate"] - 100.0) < 0.01, "JSON 通过率 100%", f"pass_rate={s['pass_rate']}")


def test_json_success_count() -> None:
    s = load_json()
    check(s["success_count"] == EXPECTED_TOTAL, f"success_count = {EXPECTED_TOTAL}",
          f"actual={s['success_count']}")


def test_calibrated_fits_exist() -> None:
    """校准后 FITS 文件应全部存在 (16 个)."""
    rows = load_csv()
    missing = []
    for r in rows:
        out = r.get("output_fits", "")
        if not out:
            missing.append(f"{r['device_id']}/{r['filter_canonical']} (empty path)")
            continue
        abs_path = REPO_ROOT / out
        if not abs_path.exists():
            missing.append(f"{r['device_id']}/{r['filter_canonical']} (missing: {out})")
    check(not missing, "全部 16 个校准后 FITS 存在", f"missing={missing[:3]}")


# ============ forbidden_shortcut 测试 ============

def test_no_shortcut_skip_t1() -> None:
    """禁止捷径: 不得伪造 T1 数据."""
    rows = load_csv()
    t1 = [r for r in rows if r["device_id"] == "T1"]
    check(not t1, "无 T1 伪造数据")


def test_no_shortcut_substitute_lum_flat() -> None:
    """禁止捷径: T2/T4 缺 Lum Flat 不得用其他滤镜 flat 替代."""
    rows = load_csv()
    t2_lum = [r for r in rows if r["device_id"] in ("T2", "T4") and r["filter_canonical"] == "LUM"]
    check(not t2_lum, "无 T2/T4 Lum 替代")


def test_no_shortcut_no_fictitious_data() -> None:
    """禁止捷径: 所有 light_path 应指向真实存在的文件."""
    rows = load_csv()
    missing = []
    for r in rows:
        p = REPO_ROOT / r["light_path"]
        if not p.exists():
            missing.append(r["light_path"])
    check(not missing, "全部 Light 路径真实存在", f"missing={missing[:3]}")


def test_no_shortcut_master_files_exist() -> None:
    """禁止捷径: 所有 Bias/Dark/Flat master 文件应真实存在."""
    rows = load_csv()
    missing = []
    for r in rows:
        for field in ("bias_master", "dark_master", "flat_master"):
            p = REPO_ROOT / r[field]
            if not p.exists():
                missing.append(f"{r['device_id']}/{r['filter_canonical']} {field}: {r[field]}")
    check(not missing, "全部 Master 文件真实存在", f"missing={missing[:3]}")


def main() -> int:
    print("=" * 70, flush=True)
    print("P10-006 test_calibration_outputs.py 启动", flush=True)
    print("=" * 70, flush=True)

    print("\n[contract 测试]", flush=True)
    test_csv_exists()
    test_json_exists()
    test_csv_row_count()
    test_csv_fields_complete()
    test_json_summary_fields()

    print("\n[unit 测试]", flush=True)
    test_all_combinations_present()
    test_no_extra_combinations()
    test_t1_not_present()
    test_t2_lum_not_present()
    test_t4_lum_not_present()
    test_t3_lum_present()
    test_no_nan_in_calibrated()
    test_no_inf_in_calibrated()
    test_size_consistency()
    test_no_extreme_bad_pixel_ratio()
    test_calibrated_stats_finite()
    test_calibrated_mean_positive()

    print("\n[e2e 测试]", flush=True)
    test_all_passed()
    test_json_pass_rate_100()
    test_json_success_count()
    test_calibrated_fits_exist()

    print("\n[forbidden_shortcut 测试]", flush=True)
    test_no_shortcut_skip_t1()
    test_no_shortcut_substitute_lum_flat()
    test_no_shortcut_no_fictitious_data()
    test_no_shortcut_master_files_exist()

    print("\n" + "=" * 70, flush=True)
    print(f"汇总: {_passed} PASS, {_failed} FAIL", flush=True)
    print("=" * 70, flush=True)

    return 0 if _failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
