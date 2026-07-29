#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P10-003 测试脚本: 验证 CALIBRATION_MASTER_INVENTORY.csv 的完整性与硬门限.

测试项:
  T01 入口条件: P10-001 交付物存在
  T02 CSV 文件存在且可读
  T03 行数 == 27 (T2:9 + T3:9 + T4:9)
  T04 device_id 集合 ⊆ {T1, T2, T3, T4}
  T05 master_type 集合 ⊆ {Bias, Dark, Flat} (无 Unknown)
  T06 每个文件有 SHA-256 (64 字符 hex)
  T07 每个文件有 file_size_bytes > 0
  T08 每行有 scanned_at 时间戳
  T09 T2/T3/T4 各有 1 个 Bias
  T10 T2 有 3 个 Dark (600/1200/1800s)
  T11 T3 有 2 个 Dark (600/1200s)
  T12 T4 有 3 个 Dark (180/300/600s)
  T13 T2 有 5 个 Flat (Blue/Green/H-alpha/OIII/Red, 缺 Lum)
  T14 T3 有 6 个 Flat (Blue/Green/H-alpha/Lum/Oiii/Red, 完整)
  T15 T4 有 5 个 Flat (Blue/Green/H-alpha/Oiii/Red, 缺 Lum)
  T16 所有 bin == 1
  T17 所有 instrument == FLI
  T18 文件名 vs Header 一致性 (filter/exposure/bin/image_size)
  T19 SHA-256 唯一性 (无重复 hash)
  T20 禁止捷径: 无 Unknown 类型, 无文件缺失判定

退出码: 0=全部 PASS, 非 0=有 FAIL
"""

from __future__ import annotations

import csv
import json
import sys
from pathlib import Path
from collections import defaultdict


REPO_ROOT = Path(__file__).resolve().parents[4]
P10_001_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P10-001"
P10_003_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P10-003"


class TestResult:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.items: list[tuple[str, str, str]] = []

    def pass_(self, test_id: str, detail: str = ""):
        self.passed += 1
        self.items.append((test_id, "PASS", detail))

    def fail(self, test_id: str, detail: str = ""):
        self.failed += 1
        self.items.append((test_id, "FAIL", detail))

    def summary(self) -> str:
        total = self.passed + self.failed
        return f"{total} tests: {self.passed} PASS, {self.failed} FAIL"


def main() -> int:
    r = TestResult()
    log_lines: list[str] = []

    def log(msg: str):
        print(msg, flush=True)
        log_lines.append(msg)

    log("=" * 70)
    log("P10-003 validate_calibration_master_inventory.py 启动")
    log("=" * 70)

    # T01 入口条件
    p10_001_required = ["TESTDATA_EQUIPMENT_CATALOG.csv", "FILTER_ALIAS_MAP.json"]
    all_ok = all((P10_001_DIR / f).exists() for f in p10_001_required)
    if all_ok:
        r.pass_("T01", f"P10-001 交付物齐全 ({len(p10_001_required)} 项)")
    else:
        r.fail("T01", "P10-001 交付物缺失")

    # T02 CSV 存在且可读
    csv_path = P10_003_DIR / "CALIBRATION_MASTER_INVENTORY.csv"
    rows = []
    if csv_path.exists():
        try:
            with open(csv_path, "r", encoding="utf-8") as f:
                rows = list(csv.DictReader(f))
            r.pass_("T02", f"CSV 可读, {len(rows)} 行")
        except Exception as e:
            r.fail("T02", f"CSV 读取异常: {e}")
    else:
        r.fail("T02", f"CSV 不存在: {csv_path}")
        log(f"[!] CSV 不存在, 终止")
        log(r.summary())
        return 1

    # T03 行数 == 27
    if len(rows) == 27:
        r.pass_("T03", f"行数 = 27 (T2:9 + T3:9 + T4:9)")
    else:
        r.fail("T03", f"行数 = {len(rows)} (预期 27)")

    # T04 device_id 集合
    dev_ids = {r_["device_id"] for r_ in rows}
    if dev_ids <= {"T1", "T2", "T3", "T4"}:
        r.pass_("T04", f"device_id 集合 = {sorted(dev_ids)} (⊆ T1-T4)")
    else:
        r.fail("T04", f"device_id 含越界值: {dev_ids}")

    # T05 master_type 集合
    types = {r_["master_type"] for r_ in rows}
    if types <= {"Bias", "Dark", "Flat"} and "Unknown" not in types:
        r.pass_("T05", f"master_type 集合 = {sorted(types)} (无 Unknown)")
    else:
        r.fail("T05", f"master_type 含 Unknown 或越界: {types}")

    # T06 SHA-256 (64 hex chars)
    sha_ok = all(len(r_["sha256"]) == 64 and all(c in "0123456789ABCDEFabcdef" for c in r_["sha256"]) for r_ in rows)
    if sha_ok:
        r.pass_("T06", f"全部 {len(rows)} 行有合法 SHA-256 (64 hex)")
    else:
        bad = [r_["file_name"] for r_ in rows if len(r_["sha256"]) != 64]
        r.fail("T06", f"SHA-256 异常文件: {bad}")

    # T07 file_size_bytes > 0
    size_ok = all(int(r_["file_size_bytes"]) > 0 for r_ in rows)
    if size_ok:
        r.pass_("T07", "全部文件 file_size_bytes > 0")
    else:
        r.fail("T07", "存在 file_size_bytes <= 0")

    # T08 scanned_at 存在
    ts_ok = all(r_["scanned_at"] for r_ in rows)
    if ts_ok:
        r.pass_("T08", "全部行有 scanned_at 时间戳")
    else:
        r.fail("T08", "存在 scanned_at 为空")

    # T09 每设备 1 个 Bias
    by_dev_type = defaultdict(lambda: defaultdict(int))
    for r_ in rows:
        by_dev_type[r_["device_id"]][r_["master_type"]] += 1
    bias_ok = all(by_dev_type[d]["Bias"] == 1 for d in ["T2", "T3", "T4"])
    if bias_ok:
        r.pass_("T09", "T2/T3/T4 各有 1 个 Bias")
    else:
        r.fail("T09", f"Bias 分布异常: {dict({d: dict(by_dev_type[d]) for d in ['T2','T3','T4']})}")

    # T10 T2 Dark (600/1200/1800s)
    t2_darks = [r_ for r_ in rows if r_["device_id"] == "T2" and r_["master_type"] == "Dark"]
    t2_exp = sorted([float(r_["exposure_from_header"]) for r_ in t2_darks])
    if t2_exp == [600.0, 1200.0, 1800.0]:
        r.pass_("T10", f"T2 Dark exposures = {t2_exp}")
    else:
        r.fail("T10", f"T2 Dark exposures = {t2_exp} (预期 [600, 1200, 1800])")

    # T11 T3 Dark (600/1200s)
    t3_darks = [r_ for r_ in rows if r_["device_id"] == "T3" and r_["master_type"] == "Dark"]
    t3_exp = sorted([float(r_["exposure_from_header"]) for r_ in t3_darks])
    if t3_exp == [600.0, 1200.0]:
        r.pass_("T11", f"T3 Dark exposures = {t3_exp}")
    else:
        r.fail("T11", f"T3 Dark exposures = {t3_exp} (预期 [600, 1200])")

    # T12 T4 Dark (180/300/600s)
    t4_darks = [r_ for r_ in rows if r_["device_id"] == "T4" and r_["master_type"] == "Dark"]
    t4_exp = sorted([float(r_["exposure_from_header"]) for r_ in t4_darks])
    if t4_exp == [180.0, 300.0, 600.0]:
        r.pass_("T12", f"T4 Dark exposures = {t4_exp}")
    else:
        r.fail("T12", f"T4 Dark exposures = {t4_exp} (预期 [180, 300, 600])")

    # T13 T2 Flat (Blue/Green/H-alpha/OIII/Red, 缺 Lum)
    t2_flats = [r_["filter_from_header"] or r_["filter_from_filename"]
                for r_ in rows if r_["device_id"] == "T2" and r_["master_type"] == "Flat"]
    t2_flat_set = set(t2_flats)
    expected_t2 = {"Blue", "Green", "H-alpha", "OIII", "Red"}
    if t2_flat_set == expected_t2 and len(t2_flats) == 5:
        r.pass_("T13", f"T2 Flats = {sorted(t2_flat_set)} (5 项, 缺 Lum)")
    else:
        r.fail("T13", f"T2 Flats = {sorted(t2_flat_set)} (预期 {sorted(expected_t2)})")

    # T14 T3 Flat (完整 6 项)
    t3_flats = [r_["filter_from_header"] or r_["filter_from_filename"]
                for r_ in rows if r_["device_id"] == "T3" and r_["master_type"] == "Flat"]
    t3_flat_set = set(t3_flats)
    expected_t3 = {"Blue", "Green", "H-alpha", "Lum", "Oiii", "Red"}
    if t3_flat_set == expected_t3 and len(t3_flats) == 6:
        r.pass_("T14", f"T3 Flats = {sorted(t3_flat_set)} (6 项, 完整)")
    else:
        r.fail("T14", f"T3 Flats = {sorted(t3_flat_set)} (预期 {sorted(expected_t3)})")

    # T15 T4 Flat (Blue/Green/H-alpha/Oiii/Red, 缺 Lum)
    t4_flats = [r_["filter_from_header"] or r_["filter_from_filename"]
                for r_ in rows if r_["device_id"] == "T4" and r_["master_type"] == "Flat"]
    t4_flat_set = set(t4_flats)
    expected_t4 = {"Blue", "Green", "H-alpha", "Oiii", "Red"}
    if t4_flat_set == expected_t4 and len(t4_flats) == 5:
        r.pass_("T15", f"T4 Flats = {sorted(t4_flat_set)} (5 项, 缺 Lum)")
    else:
        r.fail("T15", f"T4 Flats = {sorted(t4_flat_set)} (预期 {sorted(expected_t4)})")

    # T16 bin == 1
    bins = set(r_["bin_from_header"] or r_["bin_from_filename"] for r_ in rows)
    if bins == {"1"}:
        r.pass_("T16", f"全部 bin = 1 (集合 = {sorted(bins)})")
    else:
        r.fail("T16", f"bin 集合 = {sorted(bins)} (预期 [1])")

    # T17 instrument == FLI
    instruments = set(r_["instrument_from_header"] for r_ in rows if r_["instrument_from_header"])
    if instruments == {"FLI"}:
        r.pass_("T17", f"全部 instrument = FLI")
    else:
        r.fail("T17", f"instrument 集合 = {sorted(instruments)} (预期 [FLI])")

    # T18 文件名 vs Header 一致性
    mismatch_count = 0
    for r_ in rows:
        for f in ["filter_match", "exposure_match", "bin_match", "image_size_match"]:
            if r_[f] == "NO":
                mismatch_count += 1
    if mismatch_count == 0:
        r.pass_("T18", f"全部 {len(rows)} 行 4 项 match 全为 YES (0 个 NO)")
    else:
        r.fail("T18", f"存在 {mismatch_count} 个 NO")

    # T19 SHA-256 唯一性
    shas = [r_["sha256"] for r_ in rows]
    if len(set(shas)) == len(shas):
        r.pass_("T19", f"全部 {len(shas)} 个 SHA-256 唯一 (无重复)")
    else:
        dup = [s for s in shas if shas.count(s) > 1]
        r.fail("T19", f"SHA-256 重复: {set(dup)}")

    # T20 禁止捷径: 无 Unknown, 无文件缺失
    unknown_count = sum(1 for r_ in rows if r_["master_type"] == "Unknown")
    parse_err_count = sum(1 for r_ in rows if r_["header_parse_error"])
    if unknown_count == 0 and parse_err_count == 0:
        r.pass_("T20", f"无 Unknown 类型, 无 Header 解析错误 (禁止捷径 PASS)")
    else:
        r.fail("T20", f"Unknown={unknown_count}, parse_errors={parse_err_count}")

    # 输出汇总
    log("\n" + "=" * 70)
    log("测试详情:")
    for tid, status, detail in r.items:
        log(f"  [{status}] {tid}: {detail}")
    log("=" * 70)
    log(r.summary())
    log("=" * 70)

    # 写日志
    log_path = P10_003_DIR / "raw_logs" / "validate_calibration_master_inventory.log"
    log_path.write_text("\n".join(log_lines), encoding="utf-8")
    log(f"日志: {log_path}")

    # 写 JSON 结果
    result_json = {
        "summary": {"total": r.passed + r.failed, "passed": r.passed, "failed": r.failed},
        "items": [{"test_id": t, "status": s, "detail": d} for t, s, d in r.items],
        "exit_code": 0 if r.failed == 0 else 1,
    }
    result_path = P10_003_DIR / "raw_logs" / "validate_calibration_master_inventory_result.json"
    with open(result_path, "w", encoding="utf-8") as f:
        json.dump(result_json, f, ensure_ascii=False, indent=2)
    log(f"结果: {result_path}")

    return 0 if r.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
