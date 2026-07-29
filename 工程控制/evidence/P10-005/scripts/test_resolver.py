"""P10-005: Light-to-Master resolver unit tests.

覆盖:
- match_bias: unique / missing / ambiguous
- match_dark: exact / closest_longer / fallback_longest / missing / ambiguous_exact
- match_flat: unique / missing (no filter / no master) / ambiguous
- normalize_filter: 直接映射 / 大小写归一 / 未知别名
- load_calibration_masters: 27 个 master 加载 + image_size 统一字段
- 端到端: 710 Light 帧解析, resolved=587, unresolved=123 (全部 missing_lum_flat)

禁止捷径验证:
- 不得 first-match (multiple matches 必须标记 ambiguous)
- 不得静默选择歧义 (必须输出选择理由)
- 缺失 Flat 必须标记 missing_<filter>_flat, 不得选其他滤镜
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

# Add scripts dir to path
SCRIPTS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS_DIR))

from resolve_light_to_master import (
    load_filter_alias_map,
    normalize_filter,
    load_calibration_masters,
    match_bias,
    match_dark,
    match_flat,
    derive_device_id,
    derive_target_panel_filter,
)

REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
P10_004_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-004"
P10_005_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-005"


# ========== Test Helpers ==========

def make_master(master_type: str, device_id: str, bin_int: int, image_size: str,
                filter_canonical: str | None = None, exposure_s: float = 0.0,
                file_name: str = "test.xisf") -> dict:
    """构造测试用 master dict."""
    return {
        "file_name": file_name,
        "device_id": device_id,
        "master_type": master_type,
        "bin_int": bin_int,
        "image_size": image_size,
        "canonical_filter": filter_canonical,
        "exposure_s": exposure_s,
    }


# ========== normalize_filter Tests ==========

def test_normalize_filter_direct_mapping():
    """测试 1: 直接映射 (alias 在 map 中)."""
    fmap = load_filter_alias_map()
    assert normalize_filter(fmap, "Lum") == "LUM"
    assert normalize_filter(fmap, "Red") == "RED"
    assert normalize_filter(fmap, "Blue") == "BLUE"
    assert normalize_filter(fmap, "Green") == "GREEN"
    assert normalize_filter(fmap, "H-alpha") == "HA"
    assert normalize_filter(fmap, "OIII") == "OIII"
    assert normalize_filter(fmap, "Oiii") == "OIII"  # 别名冲突已解决
    print("PASS test_normalize_filter_direct_mapping")


def test_normalize_filter_case_insensitive():
    """测试 2: 大小写归一 (LUM/lum/Lum)."""
    fmap = load_filter_alias_map()
    assert normalize_filter(fmap, "LUM") == "LUM"
    assert normalize_filter(fmap, "lum") == "LUM"
    assert normalize_filter(fmap, "RED") == "RED"
    assert normalize_filter(fmap, "red") == "RED"
    print("PASS test_normalize_filter_case_insensitive")


def test_normalize_filter_unknown():
    """测试 3: 未知别名返回 None."""
    fmap = load_filter_alias_map()
    assert normalize_filter(fmap, "UnknownFilter") is None
    assert normalize_filter(fmap, "") is None
    assert normalize_filter(fmap, None) is None
    print("PASS test_normalize_filter_unknown")


# ========== match_bias Tests ==========

def test_match_bias_unique():
    """测试 4: Bias 唯一匹配."""
    masters = [
        make_master("Bias", "T4", 1, "4500x3600", file_name="bias_t4.xisf"),
        make_master("Bias", "T2", 1, "4096x4096", file_name="bias_t2.xisf"),
    ]
    m, status, reason = match_bias(masters, "T4", 1, "4500x3600")
    assert status == "unique"
    assert m["file_name"] == "bias_t4.xisf"
    assert "T4" in reason and "4500x3600" in reason
    print("PASS test_match_bias_unique")


def test_match_bias_missing():
    """测试 5: Bias 缺失 (无匹配设备)."""
    masters = [
        make_master("Bias", "T2", 1, "4096x4096", file_name="bias_t2.xisf"),
    ]
    m, status, reason = match_bias(masters, "T4", 1, "4500x3600")
    assert status == "missing"
    assert m is None
    assert "T4" in reason and "4500x3600" in reason
    print("PASS test_match_bias_missing")


def test_match_bias_ambiguous():
    """测试 6: Bias 歧义 (多个匹配, 选第一个并标记)."""
    masters = [
        make_master("Bias", "T4", 1, "4500x3600", file_name="bias_t4_a.xisf"),
        make_master("Bias", "T4", 1, "4500x3600", file_name="bias_t4_b.xisf"),
    ]
    m, status, reason = match_bias(masters, "T4", 1, "4500x3600")
    assert status == "ambiguous"
    assert m is not None
    assert "multiple Bias matches (2)" in reason
    print("PASS test_match_bias_ambiguous")


# ========== match_dark Tests ==========

def test_match_dark_exact():
    """测试 7: Dark 精确匹配 (Light exposure == Dark exposure)."""
    masters = [
        make_master("Dark", "T4", 1, "4500x3600", exposure_s=180.0, file_name="dark_180s.xisf"),
        make_master("Dark", "T4", 1, "4500x3600", exposure_s=300.0, file_name="dark_300s.xisf"),
    ]
    m, status, reason = match_dark(masters, "T4", 1, "4500x3600", 180.0)
    assert status == "exact"
    assert m["file_name"] == "dark_180s.xisf"
    assert "exact exposure match" in reason
    print("PASS test_match_dark_exact")


def test_match_dark_closest_longer():
    """测试 8: Dark 选择 >= Light exposure 的最接近 dark."""
    masters = [
        make_master("Dark", "T4", 1, "4500x3600", exposure_s=300.0, file_name="dark_300s.xisf"),
        make_master("Dark", "T4", 1, "4500x3600", exposure_s=600.0, file_name="dark_600s.xisf"),
    ]
    # Light exposure = 240s, 没有 240s 的 dark, 选 >= 240s 的最接近 = 300s
    m, status, reason = match_dark(masters, "T4", 1, "4500x3600", 240.0)
    assert status == "closest_longer"
    assert m["file_name"] == "dark_300s.xisf"
    assert "closest dark >= Light exposure" in reason
    print("PASS test_match_dark_closest_longer")


def test_match_dark_fallback_longest():
    """测试 9: Dark 兜底 (全部 dark < Light exposure, 选最长)."""
    masters = [
        make_master("Dark", "T4", 1, "4500x3600", exposure_s=180.0, file_name="dark_180s.xisf"),
        make_master("Dark", "T4", 1, "4500x3600", exposure_s=300.0, file_name="dark_300s.xisf"),
    ]
    # Light exposure = 600s, 全部 dark < 600s, 选最长 = 300s
    m, status, reason = match_dark(masters, "T4", 1, "4500x3600", 600.0)
    assert status == "fallback_longest"
    assert m["file_name"] == "dark_300s.xisf"
    assert "fallback" in reason and "longest dark" in reason
    print("PASS test_match_dark_fallback_longest")


def test_match_dark_missing():
    """测试 10: Dark 缺失 (无候选)."""
    masters = [
        make_master("Dark", "T2", 1, "4096x4096", exposure_s=600.0, file_name="dark_t2.xisf"),
    ]
    m, status, reason = match_dark(masters, "T4", 1, "4500x3600", 180.0)
    assert status == "missing"
    assert m is None
    assert "T4" in reason
    print("PASS test_match_dark_missing")


def test_match_dark_ambiguous_exact():
    """测试 11: Dark 歧义 (多个精确匹配)."""
    masters = [
        make_master("Dark", "T4", 1, "4500x3600", exposure_s=180.0, file_name="dark_a.xisf"),
        make_master("Dark", "T4", 1, "4500x3600", exposure_s=180.0, file_name="dark_b.xisf"),
    ]
    m, status, reason = match_dark(masters, "T4", 1, "4500x3600", 180.0)
    assert status == "ambiguous_exact"
    assert m is not None
    assert "multiple exact dark matches (2)" in reason
    print("PASS test_match_dark_ambiguous_exact")


# ========== match_flat Tests ==========

def test_match_flat_unique():
    """测试 12: Flat 唯一匹配 (设备 + Bin + 尺寸 + 滤镜)."""
    masters = [
        make_master("Flat", "T4", 1, "4500x3600", filter_canonical="RED", file_name="flat_red.xisf"),
        make_master("Flat", "T4", 1, "4500x3600", filter_canonical="BLUE", file_name="flat_blue.xisf"),
    ]
    m, status, reason = match_flat(masters, "T4", 1, "4500x3600", "RED")
    assert status == "unique"
    assert m["file_name"] == "flat_red.xisf"
    assert "RED" in reason
    print("PASS test_match_flat_unique")


def test_match_flat_missing_no_filter():
    """测试 13: Flat 缺失 (Light 无滤镜规范名)."""
    masters = [
        make_master("Flat", "T4", 1, "4500x3600", filter_canonical="RED", file_name="flat_red.xisf"),
    ]
    m, status, reason = match_flat(masters, "T4", 1, "4500x3600", "")
    assert status == "missing"
    assert m is None
    assert "no canonical filter" in reason
    print("PASS test_match_flat_missing_no_filter")


def test_match_flat_missing_no_master():
    """测试 14: Flat 缺失 (无对应滤镜 master, 如 T2 缺 Lum flat).

    禁止捷径: 不得选其他滤镜 flat 作替代.
    """
    masters = [
        make_master("Flat", "T2", 1, "4096x4096", filter_canonical="RED", file_name="flat_red.xisf"),
        make_master("Flat", "T2", 1, "4096x4096", filter_canonical="BLUE", file_name="flat_blue.xisf"),
    ]
    # T2 没有 Lum flat, 必须返回 missing, 不得选 Red/Blue flat
    m, status, reason = match_flat(masters, "T2", 1, "4096x4096", "LUM")
    assert status == "missing"
    assert m is None
    assert "missing_lum_flat" in reason
    print("PASS test_match_flat_missing_no_master")


def test_match_flat_ambiguous():
    """测试 15: Flat 歧义 (多个匹配)."""
    masters = [
        make_master("Flat", "T4", 1, "4500x3600", filter_canonical="RED", file_name="flat_red_a.xisf"),
        make_master("Flat", "T4", 1, "4500x3600", filter_canonical="RED", file_name="flat_red_b.xisf"),
    ]
    m, status, reason = match_flat(masters, "T4", 1, "4500x3600", "RED")
    assert status == "ambiguous"
    assert m is not None
    assert "multiple Flat matches (2)" in reason
    print("PASS test_match_flat_ambiguous")


# ========== load_calibration_masters Tests ==========

def test_load_masters_count():
    """测试 16: 加载 27 个 master."""
    fmap = load_filter_alias_map()
    masters = load_calibration_masters(fmap)
    assert len(masters) == 27, f"Expected 27 masters, got {len(masters)}"
    print("PASS test_load_masters_count")


def test_load_masters_image_size_unified():
    """测试 17: image_size 统一字段 (header 优先, filename 兜底).

    P10-003 的 image_size_from_header 全为空 (XISF NAXIS1/NAXIS2 未提取),
    故 image_size 必须从 image_size_from_filename 兜底.
    """
    fmap = load_filter_alias_map()
    masters = load_calibration_masters(fmap)
    # T2/T3 master: 4096x4096
    # T4 master: 4500x3600
    t2_bias = [m for m in masters if m["device_id"] == "T2" and m["master_type"] == "Bias"][0]
    t4_bias = [m for m in masters if m["device_id"] == "T4" and m["master_type"] == "Bias"][0]
    assert t2_bias["image_size"] == "4096x4096", f"T2 Bias image_size: {t2_bias['image_size']!r}"
    assert t4_bias["image_size"] == "4500x3600", f"T4 Bias image_size: {t4_bias['image_size']!r}"
    print("PASS test_load_masters_image_size_unified")


def test_load_masters_canonical_filter():
    """测试 18: canonical_filter 正确归一化 (Lum -> LUM, H-alpha -> HA, Oiii -> OIII)."""
    fmap = load_filter_alias_map()
    masters = load_calibration_masters(fmap)
    # T3 Lum flat -> LUM
    t3_lum = [m for m in masters if m["device_id"] == "T3" and m["master_type"] == "Flat"
              and m.get("canonical_filter") == "LUM"]
    assert len(t3_lum) == 1, f"T3 LUM flat count: {len(t3_lum)}"
    # T2 H-alpha flat -> HA
    t2_ha = [m for m in masters if m["device_id"] == "T2" and m["master_type"] == "Flat"
             and m.get("canonical_filter") == "HA"]
    assert len(t2_ha) == 1, f"T2 HA flat count: {len(t2_ha)}"
    # T4 Oiii flat -> OIII
    t4_oiii = [m for m in masters if m["device_id"] == "T4" and m["master_type"] == "Flat"
               and m.get("canonical_filter") == "OIII"]
    assert len(t4_oiii) == 1, f"T4 OIII flat count: {len(t4_oiii)}"
    print("PASS test_load_masters_canonical_filter")


# ========== derive_device_id Tests ==========

def test_derive_device_id():
    """测试 19: 从路径推导设备 ID."""
    assert derive_device_id(Path("testdata/Galaxy_Center_T4/lights/panel1/test.fts")) == "T4"
    assert derive_device_id(Path("testdata/LDN43_T2素材_flying_dutchman/lights/test.fts")) == "T2"
    assert derive_device_id(Path("testdata/NGC1727_T2_flying_dutchman/lights/test.fts")) == "T2"
    print("PASS test_derive_device_id")


# ========== End-to-End Validation ==========

def test_e2e_resolution_csv():
    """测试 20: 端到端验证 LIGHT_TO_MASTER_RESOLUTION.csv.

    预期:
    - 总 Light 帧: 710
    - Resolved: 587 (T2/T3/T4 的非 Lum 帧)
    - Unresolved: 123 (T2 Lum 25 + T4 Lum 98, 全部 missing_lum_flat)
    - Bias 状态: 全部 unique
    - Dark 状态: 全部 exact
    - Flat 状态: 587 unique + 123 missing
    - 歧义: 587 NONE + 123 FLAT_MISSING
    """
    import csv
    csv_path = P10_005_DIR / "LIGHT_TO_MASTER_RESOLUTION.csv"
    with open(csv_path, "r", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    # 总数
    assert len(rows) == 710, f"Total lights: {len(rows)} (expected 710)"

    # Resolved / Unresolved
    resolved = sum(1 for r in rows if r["resolved"] == "YES")
    unresolved = sum(1 for r in rows if r["resolved"] == "NO")
    assert resolved == 587, f"Resolved: {resolved} (expected 587)"
    assert unresolved == 123, f"Unresolved: {unresolved} (expected 123)"

    # Bias 状态 (全部 unique)
    bias_statuses = {r["bias_status"] for r in rows}
    assert bias_statuses == {"unique"}, f"Bias statuses: {bias_statuses}"

    # Dark 状态 (全部 exact)
    dark_statuses = {r["dark_status"] for r in rows}
    assert dark_statuses == {"exact"}, f"Dark statuses: {dark_statuses}"

    # Flat 状态 (587 unique + 123 missing)
    flat_unique = sum(1 for r in rows if r["flat_status"] == "unique")
    flat_missing = sum(1 for r in rows if r["flat_status"] == "missing")
    assert flat_unique == 587, f"Flat unique: {flat_unique} (expected 587)"
    assert flat_missing == 123, f"Flat missing: {flat_missing} (expected 123)"

    # 歧义分类
    amb_none = sum(1 for r in rows if r["ambiguity"] == "NONE")
    amb_flat_missing = sum(1 for r in rows if r["ambiguity"] == "FLAT_MISSING")
    assert amb_none == 587, f"Ambiguity NONE: {amb_none} (expected 587)"
    assert amb_flat_missing == 123, f"Ambiguity FLAT_MISSING: {amb_flat_missing} (expected 123)"

    # 123 unresolved 全部是 missing_lum_flat
    unresolved_rows = [r for r in rows if r["resolved"] == "NO"]
    for r in unresolved_rows:
        assert "missing_lum_flat" in r["flat_match_reason"], f"Unexpected reason: {r['flat_match_reason']}"
        assert r["filter_canonical"] == "LUM", f"Unexpected filter: {r['filter_canonical']}"
        assert r["device_id"] in ("T2", "T4"), f"Unexpected device: {r['device_id']}"

    # T2 Lum unresolved 数 (LDN43 10 + NGC247 15 = 25)
    t2_lum_unresolved = sum(1 for r in unresolved_rows if r["device_id"] == "T2")
    assert t2_lum_unresolved == 25, f"T2 Lum unresolved: {t2_lum_unresolved} (expected 25)"

    # T4 Lum unresolved 数 (Victory_Nebula 49 + 49 = 98)
    t4_lum_unresolved = sum(1 for r in unresolved_rows if r["device_id"] == "T4")
    assert t4_lum_unresolved == 98, f"T4 Lum unresolved: {t4_lum_unresolved} (expected 98)"

    print("PASS test_e2e_resolution_csv")


def test_e2e_no_silent_fallback():
    """测试 21: 禁止捷径 - 不得静默选择其他滤镜 flat.

    验证: 所有 missing_lum_flat 的 Light 都没有 flat_master (空字符串),
    不得静默选 Red/Blue/Green/H-alpha/OIII flat 作替代.
    """
    import csv
    csv_path = P10_005_DIR / "LIGHT_TO_MASTER_RESOLUTION.csv"
    with open(csv_path, "r", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    unresolved = [r for r in rows if r["resolved"] == "NO"]
    for r in unresolved:
        assert r["flat_master"] == "", f"Silent fallback detected: {r['light_path']} -> {r['flat_master']}"
        assert r["flat_status"] == "missing", f"Non-missing status: {r['flat_status']}"
    print("PASS test_e2e_no_silent_fallback")


def test_e2e_summary_json():
    """测试 22: RESOLUTION_SUMMARY.json 与 CSV 一致."""
    import csv
    csv_path = P10_005_DIR / "LIGHT_TO_MASTER_RESOLUTION.csv"
    json_path = P10_005_DIR / "RESOLUTION_SUMMARY.json"

    with open(csv_path, "r", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    with open(json_path, "r", encoding="utf-8") as f:
        summary = json.load(f)

    assert summary["total_lights"] == len(rows), f"total_lights mismatch"
    assert summary["resolved"] == sum(1 for r in rows if r["resolved"] == "YES")
    assert summary["unresolved"] == sum(1 for r in rows if r["resolved"] == "NO")
    assert summary["bias_status_breakdown"]["unique"] == 710
    assert summary["dark_status_breakdown"]["exact"] == 710
    assert summary["flat_status_breakdown"]["unique"] == 587
    assert summary["flat_status_breakdown"]["missing"] == 123
    assert summary["ambiguity_breakdown"]["NONE"] == 587
    assert summary["ambiguity_breakdown"]["FLAT_MISSING"] == 123
    print("PASS test_e2e_summary_json")


# ========== Forbidden Shortcut Tests ==========

def test_no_first_match_for_ambiguous():
    """测试 23: 禁止 first-match - 多个匹配必须标记 ambiguous (不是 unique)."""
    # 构造 2 个相同条件的 Bias
    masters = [
        make_master("Bias", "T4", 1, "4500x3600", file_name="bias_a.xisf"),
        make_master("Bias", "T4", 1, "4500x3600", file_name="bias_b.xisf"),
    ]
    m, status, reason = match_bias(masters, "T4", 1, "4500x3600")
    # 必须标记 ambiguous, 不得 first-match 后标记 unique
    assert status == "ambiguous", f"Expected ambiguous, got {status}"
    assert "multiple Bias matches (2)" in reason
    print("PASS test_no_first_match_for_ambiguous")


# ========== Main ==========

def main() -> int:
    print("=" * 70)
    print("P10-005 test_resolver.py 启动")
    print("=" * 70)

    tests = [
        # normalize_filter
        test_normalize_filter_direct_mapping,
        test_normalize_filter_case_insensitive,
        test_normalize_filter_unknown,
        # match_bias
        test_match_bias_unique,
        test_match_bias_missing,
        test_match_bias_ambiguous,
        # match_dark
        test_match_dark_exact,
        test_match_dark_closest_longer,
        test_match_dark_fallback_longest,
        test_match_dark_missing,
        test_match_dark_ambiguous_exact,
        # match_flat
        test_match_flat_unique,
        test_match_flat_missing_no_filter,
        test_match_flat_missing_no_master,
        test_match_flat_ambiguous,
        # load_calibration_masters
        test_load_masters_count,
        test_load_masters_image_size_unified,
        test_load_masters_canonical_filter,
        # derive_device_id
        test_derive_device_id,
        # End-to-End
        test_e2e_resolution_csv,
        test_e2e_no_silent_fallback,
        test_e2e_summary_json,
        # Forbidden shortcuts
        test_no_first_match_for_ambiguous,
    ]

    passed = 0
    failed = 0
    for test in tests:
        try:
            test()
            passed += 1
        except AssertionError as e:
            print(f"FAIL {test.__name__}: {e}")
            failed += 1
        except Exception as e:
            print(f"ERROR {test.__name__}: {type(e).__name__}: {e}")
            failed += 1

    print("\n" + "=" * 70)
    print(f"汇总: PASS={passed}, FAIL={failed}, TOTAL={passed + failed}")
    print("=" * 70)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
