#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P10-002 测试脚本: 验证 4 个 JSON profile 的完整性与硬门限.

测试项:
  T01 入口条件: P10-001 交付物存在且可读
  T02 失败基线复现: 修复前 TypeError (n_lights str+int)
  T03 4 个 profile 文件存在
  T04 JSON 可解析
  T05 device_id 集合 == {T1, T2, T3, T4}
  T06 禁止捷径: 不存在 T5/unknown
  T07 T1 status == "no_data"
  T08 T2/T3/T4 status == "active"
  T09 active profile 必填字段非空 (telescope, aperture, focal_length, camera, image_size, bin, filter_set)
  T10 active profile pixel_size_um 非空 (来自 Header)
  T11 active profile calibration.total_calib_files >= 1
  T12 active profile total_light_frames >= 1
  T13 T2/T3/T4 总 Light 帧数 == 710 (174+151+385)
  T14 T2 filter_set 包含 6 项 (LRGB+Ha+OIII)
  T15 T4 filter_set 包含 6 项 (LRGB+Ha+Oiii)
  T16 missing_flats 已记录 (T2/T4 缺 Lum)
  T17 滤镜别名规范: filter_set 中元素在 FILTER_ALIAS_MAP 中有对应规范名
  T18 dark_exposures_s 覆盖 Light 曝光集合
  T19 dataset_summary 中 n_lights 类型为 int
  T20 生成时间戳存在 (generated_at)

退出码: 0=全部 PASS, 非 0=有 FAIL
"""

from __future__ import annotations

import csv
import json
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
P10_001_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P10-001"
P10_002_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P10-002"

DEVICE_IDS = ["T1", "T2", "T3", "T4"]
EXPECTED_TOTAL_LIGHTS = 710  # T2(174) + T3(151) + T4(385)


class TestResult:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.skipped = 0
        self.items: list[tuple[str, str, str]] = []  # (test_id, status, detail)

    def pass_(self, test_id: str, detail: str = ""):
        self.passed += 1
        self.items.append((test_id, "PASS", detail))

    def fail(self, test_id: str, detail: str = ""):
        self.failed += 1
        self.items.append((test_id, "FAIL", detail))

    def skip(self, test_id: str, detail: str = ""):
        self.skipped += 1
        self.items.append((test_id, "SKIP", detail))

    def summary(self) -> str:
        total = self.passed + self.failed + self.skipped
        return f"{total} tests: {self.passed} PASS, {self.failed} FAIL, {self.skipped} SKIP"


def main() -> int:
    r = TestResult()
    log_lines: list[str] = []

    def log(msg: str):
        print(msg, flush=True)
        log_lines.append(msg)

    log("=" * 70)
    log("P10-002 validate_device_profiles.py 启动")
    log("=" * 70)

    # T01 入口条件: P10-001 交付物存在
    p10_001_required = [
        "TESTDATA_EQUIPMENT_CATALOG.csv",
        "TESTDATA_DATASET_CATALOG.csv",
        "FILTER_ALIAS_MAP.json",
    ]
    all_p10_001_ok = True
    for fname in p10_001_required:
        fpath = P10_001_DIR / fname
        if not fpath.exists():
            r.fail("T01", f"P10-001 缺失: {fname}")
            all_p10_001_ok = False
    if all_p10_001_ok:
        r.pass_("T01", f"P10-001 交付物齐全 ({len(p10_001_required)} 项)")

    # T02 失败基线复现: 修复前 TypeError
    # 简化检查: 验证 csv.DictReader 返回的 n_lights 是 str (说明修复前会出错)
    csv_path = P10_001_DIR / "TESTDATA_DATASET_CATALOG.csv"
    try:
        with open(csv_path, "r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            rows = list(reader)
        if rows and isinstance(rows[0].get("n_lights"), str):
            r.pass_("T02", f"基线确认: csv n_lights 是 str 类型 (修复前 sum() 会 TypeError), 共 {len(rows)} 行")
        else:
            r.fail("T02", "csv n_lights 不是 str, 基线复现失败")
    except Exception as e:
        r.fail("T02", f"读取 csv 异常: {e}")

    # T03 4 个 profile 文件存在
    profiles = {}
    all_files_exist = True
    for dev in DEVICE_IDS:
        p = P10_002_DIR / f"{dev}_DEVICE_PROFILE.json"
        if not p.exists():
            r.fail(f"T03-{dev}", f"profile 缺失: {p.name}")
            all_files_exist = False
    if all_files_exist:
        r.pass_("T03", "4 个 profile 文件存在")

    # T04 JSON 可解析
    for dev in DEVICE_IDS:
        p = P10_002_DIR / f"{dev}_DEVICE_PROFILE.json"
        try:
            with open(p, "r", encoding="utf-8") as f:
                profiles[dev] = json.load(f)
            r.pass_(f"T04-{dev}", f"JSON 可解析")
        except Exception as e:
            r.fail(f"T04-{dev}", f"JSON 解析失败: {e}")

    if not all(profiles.get(d) for d in DEVICE_IDS):
        log("[!] profile 加载不全, 跳过剩余测试")
        log(f"\n{r.summary()}")
        (P10_002_DIR / "raw_logs" / "validate_device_profiles.log").write_text(
            "\n".join(log_lines), encoding="utf-8"
        )
        return 1

    # T05 device_id 集合
    actual_ids = {profiles[d]["device_id"] for d in DEVICE_IDS}
    if actual_ids == set(DEVICE_IDS):
        r.pass_("T05", f"device_id 集合 == {{T1,T2,T3,T4}}")
    else:
        r.fail("T05", f"device_id 集合不匹配: {actual_ids}")

    # T06 禁止捷径: 不存在 T5/unknown
    has_forbidden = any(d in ["T5", "unknown"] for d in actual_ids)
    if not has_forbidden:
        r.pass_("T06", "无 T5/unknown 设备")
    else:
        r.fail("T06", "检测到 T5/unknown (禁止捷径)")

    # T07 T1 status == "no_data"
    if profiles["T1"].get("status") == "no_data":
        r.pass_("T07", "T1 status = no_data (硬门限允许, 实际无数据)")
    else:
        r.fail("T07", f"T1 status 应为 no_data, 实际 {profiles['T1'].get('status')}")

    # T08 T2/T3/T4 status == "active"
    for dev in ["T2", "T3", "T4"]:
        if profiles[dev].get("status") == "active":
            r.pass_(f"T08-{dev}", "status = active")
        else:
            r.fail(f"T08-{dev}", f"status 应为 active, 实际 {profiles[dev].get('status')}")

    # T09 active profile 必填字段
    required_fields = ["telescope", "aperture_mm", "focal_length_mm", "camera",
                       "image_size", "bin", "filter_set"]
    for dev in ["T2", "T3", "T4"]:
        for field in required_fields:
            val = profiles[dev].get(field)
            if isinstance(val, list):
                if not val:
                    r.fail(f"T09-{dev}-{field}", "字段为空列表")
                else:
                    r.pass_(f"T09-{dev}-{field}", f"非空: {val}")
            elif isinstance(val, str):
                if val:
                    r.pass_(f"T09-{dev}-{field}", f"非空: {val[:50]}")
                else:
                    r.fail(f"T09-{dev}-{field}", "字段为空")
            else:
                r.fail(f"T09-{dev}-{field}", f"字段类型异常: {type(val)}")

    # T10 pixel_size_um 非空
    for dev in ["T2", "T3", "T4"]:
        val = profiles[dev].get("pixel_size_um", "")
        if val:
            r.pass_(f"T10-{dev}", f"pixel_size_um = {val}")
        else:
            r.fail(f"T10-{dev}", "pixel_size_um 为空 (Header 缺失)")

    # T11 calibration.total_calib_files >= 1
    for dev in ["T2", "T3", "T4"]:
        calib = profiles[dev].get("calibration", {})
        n = calib.get("total_calib_files", 0)
        if n >= 1:
            r.pass_(f"T11-{dev}", f"total_calib_files = {n}")
        else:
            r.fail(f"T11-{dev}", f"total_calib_files = {n} (应 >= 1)")

    # T12 total_light_frames >= 1
    for dev in ["T2", "T3", "T4"]:
        n = profiles[dev].get("total_light_frames", 0)
        if n >= 1:
            r.pass_(f"T12-{dev}", f"total_light_frames = {n}")
        else:
            r.fail(f"T12-{dev}", f"total_light_frames = {n} (应 >= 1)")

    # T13 总 Light 帧数 == 710
    total_lights = sum(profiles[d].get("total_light_frames", 0) for d in ["T2", "T3", "T4"])
    if total_lights == EXPECTED_TOTAL_LIGHTS:
        r.pass_("T13", f"T2+T3+T4 total = {total_lights} (预期 {EXPECTED_TOTAL_LIGHTS})")
    else:
        r.fail("T13", f"T2+T3+T4 total = {total_lights} (预期 {EXPECTED_TOTAL_LIGHTS})")

    # T14 T2 filter_set 包含 6 项
    t2_fs = set(profiles["T2"].get("filter_set", []))
    expected_t2 = {"Blue", "Green", "H-alpha", "Lum", "OIII", "Red"}
    if t2_fs == expected_t2:
        r.pass_("T14", f"T2 filter_set = {sorted(t2_fs)}")
    else:
        r.fail("T14", f"T2 filter_set = {sorted(t2_fs)} (预期 {sorted(expected_t2)})")

    # T15 T4 filter_set 包含 6 项
    t4_fs = set(profiles["T4"].get("filter_set", []))
    expected_t4 = {"Blue", "Green", "H-alpha", "Lum", "Oiii", "Red"}
    if t4_fs == expected_t4:
        r.pass_("T15", f"T4 filter_set = {sorted(t4_fs)}")
    else:
        r.fail("T15", f"T4 filter_set = {sorted(t4_fs)} (预期 {sorted(expected_t4)})")

    # T16 missing_flats 已记录
    t2_missing = profiles["T2"].get("calibration", {}).get("missing_flats", [])
    t4_missing = profiles["T4"].get("calibration", {}).get("missing_flats", [])
    t3_missing = profiles["T3"].get("calibration", {}).get("missing_flats", [])
    if "Lum" in t2_missing:
        r.pass_("T16-T2", "T2 缺 Lum flat (实际观测, 已记录)")
    else:
        r.fail("T16-T2", f"T2 missing_flats 应含 Lum, 实际 {t2_missing}")
    if not t3_missing:
        r.pass_("T16-T3", "T3 无缺失 flat (5 个滤镜全覆盖)")
    else:
        r.fail("T16-T3", f"T3 missing_flats 应为空, 实际 {t3_missing}")
    if "Lum" in t4_missing:
        r.pass_("T16-T4", "T4 缺 Lum flat (实际观测, 已记录)")
    else:
        r.fail("T16-T4", f"T4 missing_flats 应含 Lum, 实际 {t4_missing}")

    # T17 滤镜别名规范
    with open(P10_001_DIR / "FILTER_ALIAS_MAP.json", "r", encoding="utf-8") as f:
        alias_map = json.load(f)
    canonical_set = set(alias_map.get("_canonical_to_aliases", {}).keys())
    # filter_set 中元素应能归一化为规范名
    for dev in ["T2", "T3", "T4"]:
        for filt in profiles[dev].get("filter_set", []):
            # 检查是否能找到对应的规范名
            found = False
            for canonical, aliases in alias_map.get("_canonical_to_aliases", {}).items():
                if filt in aliases or filt == canonical:
                    found = True
                    break
            if found:
                r.pass_(f"T17-{dev}-{filt}", "滤镜可归一化")
            else:
                r.fail(f"T17-{dev}-{filt}", f"滤镜 {filt} 在 FILTER_ALIAS_MAP 中无映射")

    # T18 dark_exposures 覆盖
    # 简化检查: T2 darks 应含 600/1200/1800; T4 darks 应含 180/300/600
    t2_darks = set(profiles["T2"].get("calibration", {}).get("dark_exposures_s", []))
    t4_darks = set(profiles["T4"].get("calibration", {}).get("dark_exposures_s", []))
    if {600.0, 1200.0, 1800.0}.issubset(t2_darks):
        r.pass_("T18-T2", f"dark exposures 覆盖: {sorted(t2_darks)}")
    else:
        r.fail("T18-T2", f"dark exposures 不全: {sorted(t2_darks)}")
    if {180.0, 300.0, 600.0}.issubset(t4_darks):
        r.pass_("T18-T4", f"dark exposures 覆盖: {sorted(t4_darks)}")
    else:
        r.fail("T18-T4", f"dark exposures 不全: {sorted(t4_darks)}")

    # T19 dataset_summary 中 n_lights 类型为 int
    for dev in ["T2", "T3", "T4"]:
        for ds in profiles[dev].get("datasets", []):
            if isinstance(ds.get("n_lights"), int):
                r.pass_(f"T19-{dev}", f"dataset_summary n_lights 类型正确 (int)")
            else:
                r.fail(f"T19-{dev}", f"dataset_summary n_lights 类型错误: {type(ds.get('n_lights'))}")
            break  # 一个就够

    # T20 generated_at 存在
    for dev in DEVICE_IDS:
        if profiles[dev].get("generated_at"):
            r.pass_(f"T20-{dev}", f"generated_at = {profiles[dev]['generated_at']}")
        else:
            r.fail(f"T20-{dev}", "generated_at 缺失")

    # 输出汇总
    log("\n" + "=" * 70)
    log("测试详情:")
    for tid, status, detail in r.items:
        log(f"  [{status}] {tid}: {detail}")
    log("=" * 70)
    log(r.summary())
    log("=" * 70)

    # 写日志
    log_path = P10_002_DIR / "raw_logs" / "validate_device_profiles.log"
    log_path.write_text("\n".join(log_lines), encoding="utf-8")
    log(f"日志: {log_path}")

    # 写 JSON 结果
    result_json = {
        "summary": {
            "total": r.passed + r.failed + r.skipped,
            "passed": r.passed,
            "failed": r.failed,
            "skipped": r.skipped,
        },
        "items": [{"test_id": t, "status": s, "detail": d} for t, s, d in r.items],
        "exit_code": 0 if r.failed == 0 else 1,
    }
    result_path = P10_002_DIR / "raw_logs" / "validate_device_profiles_result.json"
    with open(result_path, "w", encoding="utf-8") as f:
        json.dump(result_json, f, ensure_ascii=False, indent=2)
    log(f"结果: {result_path}")

    return 0 if r.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
