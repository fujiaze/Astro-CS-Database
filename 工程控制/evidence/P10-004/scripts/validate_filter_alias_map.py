"""P10-004: 验证 FILTER_ALIAS_MAP.json 完整性与 Unicode/大小写归一化.

测试覆盖:
1. 入口条件 (P10-001 + P10-002 交付物)
2. FILTER_ALIAS_MAP.json 存在且 JSON 可解析
3. 6 个规范名正确 (LUM/RED/GREEN/BLUE/HA/OIII)
4. 禁止捷径: 每个规范名对应不同物理滤镜 (不合并)
5. 全部观察到的别名可归一化
6. 大小写归一化 (LUM/Lum/lum/LUMINANCE)
7. Unicode 希腊字母 alpha (Hα, hα, α, HΑ)
8. Unicode 下标 3 (O₃, o₃) 和罗马数字 III (OⅢ)
9. 全角字符归一化 (ＯＩＩＩ)
10. 别名 -> 规范名 -> 别名 往返校验
11. 规范名 -> 首选别名 / 文档拼写
12. 反向映射: 规范名 -> 别名 列表完整
13. 与 P10-001 FILTER_ALIAS_MAP.json 兼容性
14. 与 P10-002/P10-003 观察到的滤镜名交叉验证
"""
from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
P10_001_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-001"
P10_002_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-002"
P10_003_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-003"
P10_004_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-004"

CANONICAL_NAMES = ["LUM", "RED", "GREEN", "BLUE", "HA", "OIII"]


class TestResult:
    def __init__(self):
        self.items = []
        self.passed = 0
        self.failed = 0

    def pass_(self, test_id: str, detail: str):
        self.items.append({"test_id": test_id, "status": "PASS", "detail": detail})
        self.passed += 1
        print(f"  [PASS] {test_id}: {detail}", flush=True)

    def fail(self, test_id: str, detail: str):
        self.items.append({"test_id": test_id, "status": "FAIL", "detail": detail})
        self.failed += 1
        print(f"  [FAIL] {test_id}: {detail}", flush=True)

    def summary(self) -> str:
        return f"\n{self.passed + self.failed} tests: {self.passed} PASS, {self.failed} FAIL"


def normalize_alias(alias_map: dict, alias: str) -> str | None:
    """从 alias_map 提取归一化函数."""
    if not alias:
        return None
    key = alias.strip()
    if not key:
        return None
    a2c = alias_map.get("alias_to_canonical", {})
    if key in a2c:
        return a2c[key]
    upper = key.upper()
    if upper in a2c:
        return a2c[upper]
    return None


def main() -> int:
    print("=" * 70, flush=True)
    print("P10-004 validate_filter_alias_map.py 启动", flush=True)
    print("=" * 70, flush=True)
    print("\n测试详情:", flush=True)

    r = TestResult()

    # T01 入口条件: P10-001 交付物
    p10_001_required = ["TESTDATA_EQUIPMENT_CATALOG.csv", "TESTDATA_DATASET_CATALOG.csv", "FILTER_ALIAS_MAP.json"]
    all_ok = all((P10_001_DIR / f).exists() for f in p10_001_required)
    if all_ok:
        r.pass_("T01", f"P10-001 交付物齐全 ({len(p10_001_required)} 项)")
    else:
        r.fail("T01", "P10-001 交付物缺失")

    # T02 入口条件: P10-002 交付物
    p10_002_required = ["DEVICE_PROFILE_SUMMARY.json", "T2_DEVICE_PROFILE.json", "T3_DEVICE_PROFILE.json", "T4_DEVICE_PROFILE.json"]
    all_ok = all((P10_002_DIR / f).exists() for f in p10_002_required)
    if all_ok:
        r.pass_("T02", f"P10-002 交付物齐全 ({len(p10_002_required)} 项)")
    else:
        r.fail("T02", "P10-002 交付物缺失")

    # T03 入口条件: P10-003 交付物
    p10_003_required = ["CALIBRATION_MASTER_INVENTORY.csv"]
    all_ok = all((P10_003_DIR / f).exists() for f in p10_003_required)
    if all_ok:
        r.pass_("T03", f"P10-003 交付物齐全 ({len(p10_003_required)} 项)")
    else:
        r.fail("T03", "P10-003 交付物缺失")

    # T04 FILTER_ALIAS_MAP.json 存在且可解析
    fmap_path = P10_004_DIR / "FILTER_ALIAS_MAP.json"
    fmap = None
    if fmap_path.exists():
        try:
            with open(fmap_path, "r", encoding="utf-8") as f:
                fmap = json.load(f)
            r.pass_("T04", "FILTER_ALIAS_MAP.json 可解析")
        except Exception as e:
            r.fail("T04", f"JSON 解析失败: {e}")
    else:
        r.fail("T04", f"文件不存在: {fmap_path}")

    if fmap is None:
        print(f"\n{r.summary()}", flush=True)
        return 1

    # T05 6 个规范名正确
    actual_canonical = fmap.get("_canonical_names", [])
    if actual_canonical == CANONICAL_NAMES:
        r.pass_("T05", f"_canonical_names = {CANONICAL_NAMES}")
    else:
        r.fail("T05", f"_canonical_names 错误: {actual_canonical} != {CANONICAL_NAMES}")

    # T06 禁止捷径: 每个规范名对应不同物理滤镜 (6 个独立物理滤镜)
    c2p = fmap.get("_canonical_to_physical", {})
    if len(c2p) == 6 and all(c in c2p for c in CANONICAL_NAMES):
        # 验证物理描述不重复 (即不合并)
        descs = list(c2p.values())
        unique_descs = set(descs)
        if len(unique_descs) == 6:
            r.pass_("T06", f"6 个规范名全部对应不同物理滤镜 (禁止合并 PASS)")
        else:
            r.fail("T06", f"物理描述有重复: {descs}")
    else:
        r.fail("T06", f"_canonical_to_physical 字段错误: {c2p}")

    # T07 alias_to_canonical 字段存在
    a2c = fmap.get("alias_to_canonical", {})
    if a2c:
        r.pass_("T07", f"alias_to_canonical 字段存在, {len(a2c)} 个别名")
    else:
        r.fail("T07", "alias_to_canonical 字段缺失或为空")

    # T08 canonical_to_aliases 字段存在且覆盖 6 个规范名
    c2a = fmap.get("canonical_to_aliases", {})
    if all(c in c2a for c in CANONICAL_NAMES):
        r.pass_("T08", f"canonical_to_aliases 覆盖全部 6 个规范名")
    else:
        r.fail("T08", f"canonical_to_aliases 缺失规范名: {set(CANONICAL_NAMES) - set(c2a.keys())}")

    # T09 大小写归一化测试
    case_tests = [
        ("L", "LUM"), ("l", "LUM"), ("LUM", "LUM"), ("Lum", "LUM"), ("lum", "LUM"),
        ("Luminance", "LUM"), ("luminance", "LUM"), ("LUMINANCE", "LUM"),
        ("R", "RED"), ("r", "RED"), ("RED", "RED"), ("Red", "RED"), ("red", "RED"),
        ("G", "GREEN"), ("g", "GREEN"), ("GREEN", "GREEN"), ("Green", "GREEN"), ("green", "GREEN"),
        ("B", "BLUE"), ("b", "BLUE"), ("BLUE", "BLUE"), ("Blue", "BLUE"), ("blue", "BLUE"),
        ("H", "HA"), ("h", "HA"), ("HA", "HA"), ("Ha", "HA"), ("ha", "HA"),
        ("H-alpha", "HA"), ("h-alpha", "HA"), ("H-Alpha", "HA"), ("Halpha", "HA"),
        ("halpha", "HA"), ("HALPHA", "HA"),
        ("OIII", "OIII"), ("Oiii", "OIII"), ("oiii", "OIII"), ("oIII", "OIII"),
        ("O3", "OIII"), ("o3", "OIII"), ("O-III", "OIII"), ("o-iii", "OIII"),
    ]
    case_failures = []
    for alias, expected in case_tests:
        actual = normalize_alias(fmap, alias)
        if actual != expected:
            case_failures.append(f"{alias!r} -> {actual} (期望 {expected})")
    if not case_failures:
        r.pass_("T09", f"大小写归一化 {len(case_tests)} 项全 PASS")
    else:
        r.fail("T09", f"大小写归一化失败 {len(case_failures)} 项: {case_failures[:3]}")

    # T10 Unicode 希腊字母 alpha 测试
    greek_alpha_tests = [
        ("H\u03b1", "HA"),      # Hα
        ("h\u03b1", "HA"),      # hα
        ("H\u0391", "HA"),      # HΑ (大写希腊)
        ("\u03b1", "HA"),       # α
    ]
    greek_failures = []
    for alias, expected in greek_alpha_tests:
        actual = normalize_alias(fmap, alias)
        if actual != expected:
            greek_failures.append(f"{alias!r} ({hex(ord(alias[-1]))}) -> {actual} (期望 {expected})")
    if not greek_failures:
        r.pass_("T10", f"Unicode 希腊字母 α (U+03B1) {len(greek_alpha_tests)} 项全 PASS")
    else:
        r.fail("T10", f"Unicode 希腊字母归一化失败: {greek_failures}")

    # T11 Unicode 下标 3 (₃ U+2083) 和罗马数字 III (Ⅲ U+2162) 测试
    unicode_oiii_tests = [
        ("O\u2083", "OIII"),    # O₃
        ("o\u2083", "OIII"),    # o₃
        ("O\u2162", "OIII"),    # OⅢ
        ("o\u2162", "OIII"),    # oⅢ
    ]
    uni_failures = []
    for alias, expected in unicode_oiii_tests:
        actual = normalize_alias(fmap, alias)
        if actual != expected:
            uni_failures.append(f"{alias!r} -> {actual} (期望 {expected})")
    if not uni_failures:
        r.pass_("T11", f"Unicode 下标 ₃ (U+2083) + 罗马数字 Ⅲ (U+2162) {len(unicode_oiii_tests)} 项全 PASS")
    else:
        r.fail("T11", f"Unicode OIII 归一化失败: {uni_failures}")

    # T12 全角字符归一化
    fullwidth = "\uff2f\uff29\uff29\uff29"  # ＯＩＩＩ
    actual = normalize_alias(fmap, fullwidth)
    if actual == "OIII":
        r.pass_("T12", f"全角字符 ＯＩＩＩ -> OIII PASS")
    else:
        r.fail("T12", f"全角字符归一化失败: ＯＩＩＩ -> {actual}")

    # T13 往返校验: alias -> canonical -> alias 列表包含原 alias
    c2a_map = fmap.get("canonical_to_aliases", {})
    round_trip_failures = []
    for alias, canonical in a2c.items():
        if canonical in c2a_map:
            if alias not in c2a_map[canonical]:
                round_trip_failures.append(f"{alias!r} -> {canonical}, 但 {canonical} 的别名列表不含 {alias!r}")
    if not round_trip_failures:
        r.pass_("T13", f"往返校验 {len(a2c)} 个别名全 PASS")
    else:
        r.fail("T13", f"往返校验失败 {len(round_trip_failures)} 项: {round_trip_failures[:3]}")

    # T14 规范名 -> 首选别名
    c2pref = fmap.get("canonical_to_preferred_alias", {})
    expected_pref = {"LUM": "Lum", "RED": "Red", "GREEN": "Green", "BLUE": "Blue", "HA": "H-alpha", "OIII": "OIII"}
    if c2pref == expected_pref:
        r.pass_("T14", f"canonical_to_preferred_alias 正确 ({len(c2pref)} 项)")
    else:
        r.fail("T14", f"canonical_to_preferred_alias 错误: {c2pref} != {expected_pref}")

    # T15 规范名 -> 文档拼写
    c2doc = fmap.get("canonical_to_doc_spelling", {})
    expected_doc = {"LUM": "Lum", "RED": "Red", "GREEN": "Green", "BLUE": "Blue", "HA": "Halpha", "OIII": "OIII"}
    if c2doc == expected_doc:
        r.pass_("T15", f"canonical_to_doc_spelling 正确 ({len(c2doc)} 项)")
    else:
        r.fail("T15", f"canonical_to_doc_spelling 错误: {c2doc} != {expected_doc}")

    # T16 与 P10-001 FILTER_ALIAS_MAP.json 兼容性 (旧 alias 全部能归一化)
    p10_001_fmap_path = P10_001_DIR / "FILTER_ALIAS_MAP.json"
    if p10_001_fmap_path.exists():
        with open(p10_001_fmap_path, "r", encoding="utf-8") as f:
            old_fmap = json.load(f)
        old_c2a = old_fmap.get("_canonical_to_aliases", {})
        all_old_aliases = set()
        for c, aliases in old_c2a.items():
            all_old_aliases.update(aliases)
        compat_failures = []
        for alias in all_old_aliases:
            actual = normalize_alias(fmap, alias)
            if actual is None:
                compat_failures.append(alias)
        if not compat_failures:
            r.pass_("T16", f"与 P10-001 兼容: {len(all_old_aliases)} 个旧别名全部可归一化")
        else:
            r.fail("T16", f"P10-001 旧别名无法归一化: {compat_failures}")
    else:
        r.fail("T16", "P10-001 FILTER_ALIAS_MAP.json 不存在")

    # T17 与 P10-002 device profile filter_set 交叉验证
    profile_filters = set()
    for tn in ["T1", "T2", "T3", "T4"]:
        p = P10_002_DIR / f"{tn}_DEVICE_PROFILE.json"
        if not p.exists():
            continue
        with open(p, "r", encoding="utf-8") as f:
            data = json.load(f)
        for flt in data.get("filter_set", []):
            if flt:
                profile_filters.add(flt)
    cross_failures = []
    for flt in profile_filters:
        if normalize_alias(fmap, flt) is None:
            cross_failures.append(flt)
    if not cross_failures:
        r.pass_("T17", f"与 P10-002 profile filter_set 交叉验证: {len(profile_filters)} 个全可归一化")
    else:
        r.fail("T17", f"P10-002 profile filter_set 无法归一化: {cross_failures}")

    # T18 与 P10-003 CALIBRATION_MASTER_INVENTORY filter 交叉验证
    csv_path = P10_003_DIR / "CALIBRATION_MASTER_INVENTORY.csv"
    cal_filters = set()
    with open(csv_path, "r", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            ff = (row.get("filter_from_filename") or "").strip()
            fh = (row.get("filter_from_header") or "").strip()
            if ff:
                cal_filters.add(ff)
            if fh:
                cal_filters.add(fh)
    cal_failures = []
    for flt in cal_filters:
        if normalize_alias(fmap, flt) is None:
            cal_failures.append(flt)
    if not cal_failures:
        r.pass_("T18", f"与 P10-003 inventory filter 交叉验证: {len(cal_filters)} 个全可归一化")
    else:
        r.fail("T18", f"P10-003 inventory filter 无法归一化: {cal_failures}")

    # T19 observed_aliases_by_canonical 字段完整
    obs_by_c = fmap.get("observed_aliases_by_canonical", {})
    obs_failures = []
    for c in CANONICAL_NAMES:
        if c not in obs_by_c:
            obs_failures.append(c)
        elif not obs_by_c[c]:
            obs_failures.append(f"{c} (空列表)")
    if not obs_failures:
        total_obs = sum(len(v) for v in obs_by_c.values())
        r.pass_("T19", f"observed_aliases_by_canonical 覆盖 6 个规范名, 共 {total_obs} 个观察别名")
    else:
        r.fail("T19", f"observed_aliases_by_canonical 缺失/空: {obs_failures}")

    # T20 禁止捷径: OIII/Oiii 不能合并到不同规范名 (必须都映射到 OIII)
    oiii_aliases = ["OIII", "Oiii", "oiii", "O3", "O-III", "O\u2083", "O\u2162"]
    oiii_failures = []
    for alias in oiii_aliases:
        actual = normalize_alias(fmap, alias)
        if actual != "OIII":
            oiii_failures.append(f"{alias!r} -> {actual} (期望 OIII)")
    if not oiii_failures:
        r.pass_("T20", f"禁止捷径检查: OIII 别名 {len(oiii_aliases)} 项全部映射到 OIII (不合并)")
    else:
        r.fail("T20", f"OIII 别名合并检查失败: {oiii_failures}")

    # T21 frozen_at 字段存在
    frozen_at = fmap.get("frozen_at", "")
    if frozen_at and frozen_at.startswith("2026-"):
        r.pass_("T21", f"frozen_at 字段存在: {frozen_at}")
    else:
        r.fail("T21", f"frozen_at 字段错误: {frozen_at}")

    # T22 frozen_by 字段 = P10-004
    frozen_by = fmap.get("frozen_by", "")
    if frozen_by == "P10-004":
        r.pass_("T22", f"frozen_by = P10-004")
    else:
        r.fail("T22", f"frozen_by 错误: {frozen_by}")

    # T23 ALIAS_OBSERVATION_REPORT.json 存在
    obs_path = P10_004_DIR / "ALIAS_OBSERVATION_REPORT.json"
    if obs_path.exists():
        try:
            with open(obs_path, "r", encoding="utf-8") as f:
                obs = json.load(f)
            n_obs = len(obs.get("all_observed_aliases", []))
            n_unresolved = len(obs.get("unresolved_aliases", []))
            if n_unresolved == 0:
                r.pass_("T23", f"ALIAS_OBSERVATION_REPORT.json 可解析, {n_obs} 个观察别名, 0 unresolved")
            else:
                r.fail("T23", f"ALIAS_OBSERVATION_REPORT.json unresolved={n_unresolved}")
        except Exception as e:
            r.fail("T23", f"ALIAS_OBSERVATION_REPORT.json 解析失败: {e}")
    else:
        r.fail("T23", f"ALIAS_OBSERVATION_REPORT.json 不存在")

    # 输出汇总
    print("\n" + "=" * 70, flush=True)
    print(r.summary(), flush=True)
    print("=" * 70, flush=True)

    # 写入结果 JSON
    result = {
        "summary": {"total": r.passed + r.failed, "passed": r.passed, "failed": r.failed},
        "items": r.items,
        "exit_code": 0 if r.failed == 0 else 1,
    }
    result_path = P10_004_DIR / "raw_logs" / "validate_filter_alias_map_result.json"
    with open(result_path, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    print(f"日志: {P10_004_DIR / 'raw_logs' / 'validate_filter_alias_map.log'}", flush=True)
    print(f"结果: {result_path}", flush=True)

    return 0 if r.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
