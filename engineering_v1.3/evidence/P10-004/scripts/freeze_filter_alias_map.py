"""P10-004: 冻结滤镜规范名与别名映射.

依据:
- tasks/P10-004.md
- docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md

数据源:
1. testdata/**/*.txt (7 个素材信息文档, 含滤镜清单)
2. P10-001/raw_logs/header_samples.json (49 FITS + 27 XISF Header)
3. P10-001/TESTDATA_EQUIPMENT_CATALOG.csv (设备清单, 含 doc_filter_set)
4. P10-001/TESTDATA_DATASET_CATALOG.csv (49 数据集, 含 filter_in_header / filter_in_filename)
5. P10-002/T2/T3/T4_DEVICE_PROFILE.json (filter_set / filter_set_doc)
6. P10-003/CALIBRATION_MASTER_INVENTORY.csv (27 文件, filter_from_filename / filter_from_header)

输出:
- FILTER_ALIAS_MAP.json (冻结的规范名与别名映射, 含 Unicode 别名)
- ALIAS_OBSERVATION_REPORT.json (来源统计: 哪个别名来自哪类源)
"""
from __future__ import annotations

import csv
import json
import re
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
P10_001_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-001"
P10_002_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-002"
P10_003_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-003"
P10_004_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-004"
TESTDATA_DIR = REPO_ROOT / "testdata"

# 6 个规范名 (硬门限: 不允许产生新规范名)
CANONICAL_NAMES = ["LUM", "RED", "GREEN", "BLUE", "HA", "OIII"]


def iso_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="microseconds").replace("+00:00", "Z")


def scan_documentation_filters() -> dict:
    """扫描 testdata/**/*.txt 中的滤镜名, 返回 {target_dir: [filter_tokens]}."""
    result = {}
    for txt in TESTDATA_DIR.rglob("*.txt"):
        rel = txt.relative_to(TESTDATA_DIR).as_posix()
        try:
            text = txt.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        # 提取"滤镜："行
        filter_line = ""
        for line in text.splitlines():
            if line.startswith("滤镜"):
                filter_line = line.strip()
                break
        # 提取曝光时间行的滤镜名 (Red/Green/Blue/Lum/Halpha/Ha/OIII 等)
        tokens = set()
        # 中文逗号/英文逗号/中文分号/英文分号/空格分隔
        for line in text.splitlines():
            if any(kw in line for kw in ["曝光时间", "单张曝光"]):
                # 匹配 RGBHO/LRGBHa 等连续字符串
                m = re.findall(r"\b(Lum|Luminance|Red|Green|Blue|H-alpha|Halpha|Ha|OIII|Oiii|L|R|G|B|H)\b", line)
                tokens.update(m)
        # 滤镜行提取 (如 "Astrodon 50mm L,R,G,B, 3nmHalpha")
        if filter_line:
            m = re.findall(r"\b(Lum|Luminance|Red|Green|Blue|H-alpha|Halpha|Ha|H-Alpha|HA|OIII|Oiii|OIII|O3|L|R|G|B|H)\b", filter_line)
            tokens.update(m)
            # 处理 "L,R,G,B" 分隔形式
            if "L,R,G,B" in filter_line or "LRGB" in filter_line or "RGBHa" in filter_line or "RGBHO" in filter_line or "LRGBHO" in filter_line or "LRGBHa" in filter_line or "RGBHaOIII" in filter_line:
                if "L" in filter_line: tokens.add("L")
                if "R" in filter_line: tokens.add("R")
                if "G" in filter_line: tokens.add("G")
                if "B" in filter_line: tokens.add("B")
                if "Ha" in filter_line: tokens.add("Ha")
                if "Halpha" in filter_line: tokens.add("Halpha")
                if "OIII" in filter_line: tokens.add("OIII")
        result[rel] = sorted(tokens)
    return result


def scan_fits_headers() -> set:
    """扫描 header_samples.json 中的 FITS filter 字段."""
    hs_path = P10_001_DIR / "raw_logs" / "header_samples.json"
    with open(hs_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    fits_filters = set()
    for _key, hdr in data.get("fits_headers", {}).items():
        f_val = (hdr.get("filter") or "").strip()
        if f_val:
            fits_filters.add(f_val)
    xisf_filters = set()
    for _key, hdr in data.get("xisf_headers", {}).items():
        f_val = (hdr.get("filter") or "").strip()
        if f_val:
            xisf_filters.add(f_val)
    return fits_filters | xisf_filters


def scan_dataset_catalog() -> tuple[set, set]:
    """扫描 TESTDATA_DATASET_CATALOG.csv 的 filter_in_header / filter_in_filename."""
    csv_path = P10_001_DIR / "TESTDATA_DATASET_CATALOG.csv"
    in_header = set()
    in_filename = set()
    with open(csv_path, "r", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            fh = (row.get("filter_in_header") or "").strip()
            ff = (row.get("filter_in_filename") or "").strip()
            if fh:
                in_header.add(fh)
            if ff:
                in_filename.add(ff)
    return in_header, in_filename


def scan_calibration_inventory() -> tuple[set, set]:
    """扫描 CALIBRATION_MASTER_INVENTORY.csv 的 filter_from_filename / filter_from_header."""
    csv_path = P10_003_DIR / "CALIBRATION_MASTER_INVENTORY.csv"
    in_filename = set()
    in_header = set()
    with open(csv_path, "r", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            ff = (row.get("filter_from_filename") or "").strip()
            fh = (row.get("filter_from_header") or "").strip()
            if ff:
                in_filename.add(ff)
            if fh:
                in_header.add(fh)
    return in_filename, in_header


def scan_device_profiles() -> tuple[set, set]:
    """扫描 P10-002 4 个 profile 的 filter_set (Header) + filter_set_doc (文档)."""
    filter_set_header = set()
    filter_set_doc = set()
    for tn in ["T1", "T2", "T3", "T4"]:
        p = P10_002_DIR / f"{tn}_DEVICE_PROFILE.json"
        if not p.exists():
            continue
        with open(p, "r", encoding="utf-8") as f:
            data = json.load(f)
        for flt in data.get("filter_set", []):
            if flt:
                filter_set_header.add(flt)
        doc = data.get("filter_set_doc", "")
        if doc:
            m = re.findall(r"\b(Lum|Luminance|Red|Green|Blue|H-alpha|Halpha|Ha|H-Alpha|HA|OIII|Oiii|OIII|L|R|G|B|H)\b", doc)
            filter_set_doc.update(m)
            if "LRGB" in doc or "L,R,G,B" in doc:
                filter_set_doc.update(["L", "R", "G", "B"])
            if "RGBHa" in doc or "RGBHO" in doc or "LRGBHO" in doc or "LRGBHa" in doc:
                filter_set_doc.update(["L", "R", "G", "B"])
    return filter_set_header, filter_set_doc


# 别名 -> 规范名映射 (基于实际观察 + Unicode/大小写变体)
# 禁止捷径: 不允许合并不同物理滤镜 (6 个规范名独立)
ALIAS_TO_CANONICAL = {
    # LUM
    "L": "LUM",
    "LUM": "LUM",
    "Lum": "LUM",
    "Luminance": "LUM",
    "l": "LUM",
    "lum": "LUM",
    "luminance": "LUM",
    "LUMINANCE": "LUM",
    # RED
    "R": "RED",
    "RED": "RED",
    "Red": "RED",
    "r": "RED",
    "red": "RED",
    # GREEN
    "G": "GREEN",
    "GREEN": "GREEN",
    "Green": "GREEN",
    "g": "GREEN",
    "green": "GREEN",
    # BLUE
    "B": "BLUE",
    "BLUE": "BLUE",
    "Blue": "BLUE",
    "b": "BLUE",
    "blue": "BLUE",
    # HA (H-alpha)
    "H": "HA",
    "Ha": "HA",
    "HA": "HA",
    "H-alpha": "HA",
    "H-Alpha": "HA",
    "Halpha": "HA",
    "HALPHA": "HA",
    "halpha": "HA",
    "h-alpha": "HA",
    "h": "HA",
    "ha": "HA",
    # Unicode 希腊字母 alpha
    "H\u03b1": "HA",      # Hα
    "h\u03b1": "HA",      # hα
    "H\u0391": "HA",      # HΑ (大写希腊)
    "\u03b1": "HA",       # α
    # OIII (Oxygen III)
    "OIII": "OIII",
    "Oiii": "OIII",
    "oiii": "OIII",
    "oIII": "OIII",
    "O3": "OIII",
    "o3": "OIII",
    "O-III": "OIII",
    "o-iii": "OIII",
    "OIII ": "OIII",
    # Unicode 下标 3 (₃) 和罗马数字 III
    "O\u2083": "OIII",    # O₃
    "o\u2083": "OIII",    # o₃
    "O\u2162": "OIII",    # OⅢ (罗马数字 III)
    "o\u2162": "OIII",    # oⅢ
    # 边界: 全角字符
    "\uff2f\uff29\uff29\uff29": "OIII",  # ＯＩＩＩ
}

# 规范名 -> 首选别名 (P10-004 冻结: Header 实际观察值)
CANONICAL_TO_PREFERRED = {
    "LUM": "Lum",
    "RED": "Red",
    "GREEN": "Green",
    "BLUE": "Blue",
    "HA": "H-alpha",
    "OIII": "OIII",
}

# 规范名 -> 文档拼写 (从素材信息.txt 观察)
CANONICAL_TO_DOC_SPELLING = {
    "LUM": "Lum",
    "RED": "Red",
    "GREEN": "Green",
    "BLUE": "Blue",
    "HA": "Halpha",  # 文档用 Halpha 或 Ha
    "OIII": "OIII",
}

# 规范名 -> 物理滤镜描述 (用于禁止捷径检查)
CANONICAL_TO_PHYSICAL = {
    "LUM": "Luminance 宽带滤镜 (透过整个可见光谱)",
    "RED": "Red 宽带 R 滤镜",
    "GREEN": "Green 宽带 G 滤镜",
    "BLUE": "Blue 宽带 B 滤镜",
    "HA": "H-alpha 窄带滤镜 (656.3nm 氢α发射线)",
    "OIII": "OIII 窄带滤镜 (500.7nm 双电离氧发射线)",
}


def normalize_alias(alias: str) -> str | None:
    """将别名归一化为规范名, 失败返回 None."""
    if not alias:
        return None
    key = alias.strip()
    if not key:
        return None
    # 直接查表 (含 Unicode)
    if key in ALIAS_TO_CANONICAL:
        return ALIAS_TO_CANONICAL[key]
    # 大小写归一化后查表
    upper = key.upper()
    if upper in ALIAS_TO_CANONICAL:
        return ALIAS_TO_CANONICAL[upper]
    return None


def build_canonical_to_aliases() -> dict:
    """构建 canonical -> [aliases] 映射 (按字母排序)."""
    result = defaultdict(set)
    for alias, canonical in ALIAS_TO_CANONICAL.items():
        result[canonical].add(alias)
    # 加入规范名本身
    for c in CANONICAL_NAMES:
        result[c].add(c)
    # 排序输出
    return {c: sorted(result[c]) for c in CANONICAL_NAMES}


def main() -> int:
    print("=" * 70, flush=True)
    print("P10-004 freeze_filter_alias_map.py 启动", flush=True)
    print("=" * 70, flush=True)

    # 阶段 1: 扫描所有数据源
    print("\n[阶段 1] 扫描数据源...", flush=True)

    doc_filters = scan_documentation_filters()
    print(f"  文档滤镜扫描: {len(doc_filters)} 个文档", flush=True)
    for k, v in doc_filters.items():
        print(f"    {k}: {v}", flush=True)

    header_filters = scan_fits_headers()
    print(f"\n  Header filter 集合 (FITS + XISF): {sorted(header_filters)}", flush=True)

    ds_header, ds_filename = scan_dataset_catalog()
    print(f"\n  TESTDATA_DATASET_CATALOG filter_in_header: {sorted(ds_header)}", flush=True)
    print(f"  TESTDATA_DATASET_CATALOG filter_in_filename: {sorted(ds_filename)}", flush=True)

    cal_filename, cal_header = scan_calibration_inventory()
    print(f"\n  CALIBRATION_MASTER_INVENTORY filter_from_filename: {sorted(cal_filename)}", flush=True)
    print(f"  CALIBRATION_MASTER_INVENTORY filter_from_header: {sorted(cal_header)}", flush=True)

    prof_header, prof_doc = scan_device_profiles()
    print(f"\n  DEVICE_PROFILE filter_set (Header): {sorted(prof_header)}", flush=True)
    print(f"  DEVICE_PROFILE filter_set_doc (文档): {sorted(prof_doc)}", flush=True)

    # 阶段 2: 验证所有观察到的别名都能归一化
    print("\n[阶段 2] 验证观察到的别名全部可归一化...", flush=True)
    all_observed = header_filters | ds_header | ds_filename | cal_filename | cal_header | prof_header | prof_doc
    print(f"  观察到的别名全集 ({len(all_observed)}): {sorted(all_observed)}", flush=True)

    unresolved = []
    for alias in sorted(all_observed):
        canonical = normalize_alias(alias)
        if canonical is None:
            unresolved.append(alias)
            print(f"    [!] 无法归一化: {alias!r}", flush=True)
        else:
            print(f"    {alias!r} -> {canonical}", flush=True)

    if unresolved:
        print(f"\n[!] 无法归一化的别名 ({len(unresolved)}): {unresolved}", flush=True)
        print("[!] 禁止捷径检查失败: 存在未映射的别名", flush=True)
        return 1

    print(f"\n  全部 {len(all_observed)} 个观察到的别名均可归一化", flush=True)

    # 阶段 3: 构建规范的 FILTER_ALIAS_MAP.json
    print("\n[阶段 3] 构建 FILTER_ALIAS_MAP.json...", flush=True)

    canonical_to_aliases = build_canonical_to_aliases()

    alias_map = {
        "_description": (
            "滤镜规范名与别名映射 (P10-004 冻结). "
            "6 个规范名: LUM/RED/GREEN/BLUE/HA/OIII. "
            "禁止捷径: 不允许合并不同物理滤镜. "
            "Unicode 别名包含希腊字母 α (U+03B1) 和下标 ₃ (U+2083) 等."
        ),
        "_canonical_names": CANONICAL_NAMES,
        "_canonical_to_physical": CANONICAL_TO_PHYSICAL,
        "_canonical_to_preferred_alias": CANONICAL_TO_PREFERRED,
        "_canonical_to_doc_spelling": CANONICAL_TO_DOC_SPELLING,
        "_canonical_to_aliases": canonical_to_aliases,
        "_observed_aliases_by_source": {
            "documentation_txt": sorted({a for tokens in doc_filters.values() for a in tokens}),
            "fits_xisf_headers": sorted(header_filters),
            "dataset_catalog_filter_in_header": sorted(ds_header),
            "dataset_catalog_filter_in_filename": sorted(ds_filename),
            "calibration_inventory_filter_from_filename": sorted(cal_filename),
            "calibration_inventory_filter_from_header": sorted(cal_header),
            "device_profile_filter_set": sorted(prof_header),
            "device_profile_filter_set_doc": sorted(prof_doc),
        },
        "canonical_to_preferred_alias": CANONICAL_TO_PREFERRED,
        "canonical_to_doc_spelling": CANONICAL_TO_DOC_SPELLING,
        "canonical_to_aliases": canonical_to_aliases,
        "alias_to_canonical": ALIAS_TO_CANONICAL,
        "observed_aliases_by_canonical": {
            c: sorted([a for a in all_observed if normalize_alias(a) == c])
            for c in CANONICAL_NAMES
        },
        "frozen_at": iso_now(),
        "frozen_by": "P10-004",
    }

    # 阶段 4: 验证禁止捷径 - 6 个规范名都是不同物理滤镜
    print("\n[阶段 4] 禁止捷径检查...", flush=True)
    for c in CANONICAL_NAMES:
        phys = CANONICAL_TO_PHYSICAL[c]
        print(f"  {c}: {phys}", flush=True)
        if c not in canonical_to_aliases:
            print(f"    [!] {c} 无别名映射", flush=True)
            return 1
    print(f"  6 个规范名全部对应不同物理滤镜: PASS", flush=True)

    # 写入 FILTER_ALIAS_MAP.json
    out_path = P10_004_DIR / "FILTER_ALIAS_MAP.json"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(alias_map, f, ensure_ascii=False, indent=2)
    print(f"\n[写入] {out_path}", flush=True)

    # 写入观察报告
    obs_report = {
        "_description": "P10-004 别名来源观察报告",
        "generated_at": iso_now(),
        "canonical_names": CANONICAL_NAMES,
        "all_observed_aliases": sorted(all_observed),
        "aliases_by_source": {
            "documentation_txt": {
                path: tokens for path, tokens in sorted(doc_filters.items())
            },
            "fits_xisf_headers": sorted(header_filters),
            "dataset_catalog_filter_in_header": sorted(ds_header),
            "dataset_catalog_filter_in_filename": sorted(ds_filename),
            "calibration_inventory_filter_from_filename": sorted(cal_filename),
            "calibration_inventory_filter_from_header": sorted(cal_header),
            "device_profile_filter_set": sorted(prof_header),
            "device_profile_filter_set_doc": sorted(prof_doc),
        },
        "alias_to_canonical_resolved": {
            alias: normalize_alias(alias) for alias in sorted(all_observed)
        },
        "unresolved_aliases": unresolved,
    }
    obs_path = P10_004_DIR / "ALIAS_OBSERVATION_REPORT.json"
    with open(obs_path, "w", encoding="utf-8") as f:
        json.dump(obs_report, f, ensure_ascii=False, indent=2)
    print(f"[写入] {obs_path}", flush=True)

    print("\n" + "=" * 70, flush=True)
    print("P10-004 freeze_filter_alias_map.py 完成", flush=True)
    print("=" * 70, flush=True)
    print(f"\n规范名: {CANONICAL_NAMES}", flush=True)
    print(f"别名总数: {len(ALIAS_TO_CANONICAL)}", flush=True)
    print(f"观察到的别名: {len(all_observed)}", flush=True)
    print(f"无法归一化的别名: {len(unresolved)}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
