# -*- coding: utf-8 -*-
"""
test_contract.py - P12-001 契约测试: photometry_report.json vs schema

功能:
  1. 加载 contracts/photometry_report.schema.json
  2. 加载 output/p12_001_test/photometry_report.json
  3. jsonschema.validate 验证
  4. 额外验证 P12-001 诊断字段齐全 (17 个 diag 字段)

依赖: jsonschema>=4.0
"""

from __future__ import annotations

import json
import os
import sys

import jsonschema

# ---- 路径 ----
# 本脚本位于 engineering_v1.3/evidence/P12-001/scripts/
# 向上 3 级到 engineering_v1.3/, 再向上 1 级到项目根
_ENG_DIR = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", ".."
))
_PROJECT_ROOT = os.path.normpath(os.path.join(_ENG_DIR, ".."))
SCHEMA_PATH = os.path.join(
    _ENG_DIR, "contracts", "photometry_report.schema.json"
)
REPORT_PATH = os.path.join(
    _PROJECT_ROOT, "output", "p12_001_test", "photometry_report.json"
)

# P12-001 诊断字段 (除 schema required 外的额外字段)
P12_001_DIAG_FIELDS = [
    "spectrum_rows_total",
    "psf_total",
    "spatial_candidates",
    "rejected_ambiguous",
    "rejected_distance",
    "rejected_quality",
    "robust_iterations",
    "r_median",
    "r_p90",
    "r_max",
]


def test_schema_validation():
    """1. jsonschema 契约验证"""
    print("=" * 60)
    print("[契约测试1] photometry_report.json vs schema")
    print("=" * 60)

    with open(SCHEMA_PATH, "r", encoding="utf-8") as f:
        schema = json.load(f)
    with open(REPORT_PATH, "r", encoding="utf-8") as f:
        report = json.load(f)

    print(f"  Schema: {SCHEMA_PATH}")
    print(f"  Report: {REPORT_PATH}")
    print(f"  Schema required fields: {schema.get('required', [])}")

    try:
        jsonschema.validate(instance=report, schema=schema)
        print("  [PASS] jsonschema.validate 通过")
        return True
    except jsonschema.ValidationError as e:
        print(f"  [FAIL] jsonschema 验证失败: {e.message}")
        print(f"  错误路径: {' -> '.join(str(p) for p in e.absolute_path)}")
        return False


def test_required_fields_present():
    """2. schema required 字段齐全"""
    print("\n" + "=" * 60)
    print("[契约测试2] required 字段齐全性")
    print("=" * 60)

    with open(SCHEMA_PATH, "r", encoding="utf-8") as f:
        schema = json.load(f)
    with open(REPORT_PATH, "r", encoding="utf-8") as f:
        report = json.load(f)

    required = schema.get("required", [])
    all_ok = True
    for field in required:
        present = field in report
        print(f"  [{'PASS' if present else 'FAIL'}] {field}: "
              f"{'存在' if present else '缺失'}")
        if not present:
            all_ok = False

    print(f"  [{'PASS' if all_ok else 'FAIL'}] required 字段齐全")
    return all_ok


def test_p12_001_diag_fields():
    """3. P12-001 诊断字段齐全 (17 个)"""
    print("\n" + "=" * 60)
    print("[契约测试3] P12-001 诊断字段齐全性 (17 个 diag 字段)")
    print("=" * 60)

    with open(REPORT_PATH, "r", encoding="utf-8") as f:
        report = json.load(f)

    all_ok = True
    for field in P12_001_DIAG_FIELDS:
        present = field in report
        print(f"  [{'PASS' if present else 'FAIL'}] {field}: "
              f"{'存在' if present else '缺失'}")
        if not present:
            all_ok = False

    print(f"  [{'PASS' if all_ok else 'FAIL'}] P12-001 诊断字段齐全")
    return all_ok


def test_field_types():
    """4. 字段类型正确性"""
    print("\n" + "=" * 60)
    print("[契约测试4] 字段类型正确性")
    print("=" * 60)

    with open(REPORT_PATH, "r", encoding="utf-8") as f:
        report = json.load(f)

    checks = []
    # frame: string
    checks.append(("frame is string", isinstance(report.get("frame"), str)))
    # valid_fsyn: integer
    checks.append(("valid_fsyn is int",
                   isinstance(report.get("valid_fsyn"), int)))
    # gaia_in_frame: integer
    checks.append(("gaia_in_frame is int",
                   isinstance(report.get("gaia_in_frame"), int)))
    # psf_valid: integer
    checks.append(("psf_valid is int",
                   isinstance(report.get("psf_valid"), int)))
    # unique_matches: integer
    checks.append(("unique_matches is int",
                   isinstance(report.get("unique_matches"), int)))
    # fit_used: integer
    checks.append(("fit_used is int",
                   isinstance(report.get("fit_used"), int)))
    # scale_factor: number or null
    sf = report.get("scale_factor")
    checks.append(("scale_factor is number/null",
                   sf is None or isinstance(sf, (int, float))))
    # sigma_residual: number or null
    sr = report.get("sigma_residual")
    checks.append(("sigma_residual is number/null",
                   sr is None or isinstance(sr, (int, float))))
    # status: enum
    checks.append(("status in [PASS/FAIL/SKIPPED]",
                   report.get("status") in ("PASS", "FAIL", "SKIPPED")))
    # match_distance: object
    checks.append(("match_distance is object",
                   isinstance(report.get("match_distance"), dict)))

    all_ok = True
    for desc, ok in checks:
        print(f"  [{'PASS' if ok else 'FAIL'}] {desc}")
        if not ok:
            all_ok = False

    print(f"  [{'PASS' if all_ok else 'FAIL'}] 字段类型正确")
    return all_ok


def test_match_distance_subfields():
    """5. match_distance 子字段 (median/p90/max) 齐全"""
    print("\n" + "=" * 60)
    print("[契约测试5] match_distance 子字段齐全性")
    print("=" * 60)

    with open(REPORT_PATH, "r", encoding="utf-8") as f:
        report = json.load(f)

    md = report.get("match_distance", {})
    required_sub = ["median", "p90", "max"]
    all_ok = True
    for sub in required_sub:
        present = sub in md
        print(f"  [{'PASS' if present else 'FAIL'}] match_distance.{sub}: "
              f"{'存在' if present else '缺失'}")
        if not present:
            all_ok = False

    print(f"  [{'PASS' if all_ok else 'FAIL'}] match_distance 子字段齐全")
    return all_ok


if __name__ == "__main__":
    print("=" * 60)
    print("P12-001 契约测试: photometry_report.json vs schema")
    print("=" * 60)

    results = []
    results.append(("schema 验证", test_schema_validation()))
    results.append(("required 字段齐全", test_required_fields_present()))
    results.append(("P12-001 诊断字段齐全", test_p12_001_diag_fields()))
    results.append(("字段类型正确", test_field_types()))
    results.append(("match_distance 子字段", test_match_distance_subfields()))

    print("\n" + "=" * 60)
    print("契约测试汇总:")
    n_pass = sum(1 for _, ok in results if ok)
    for name, ok in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")
    print(f"\n总计: {n_pass}/{len(results)} 通过")
    print("=" * 60)

    sys.exit(0 if n_pass == len(results) else 1)
