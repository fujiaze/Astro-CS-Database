#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P10-001: 读取全部 TestData 子目录说明文档与 FITS/XISF Header,
输出 TESTDATA_EQUIPMENT_CATALOG.csv / TESTDATA_DATASET_CATALOG.csv /
     FILTER_ALIAS_MAP.json / DOCUMENT_FACT_CONFLICTS.md

设计原则 (依据 docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md):
- 递归读取每个 TestData 子文件夹的说明文档 (素材信息*.txt)
- 交叉读取 FITS (.fts) Header 与 XISF (.xisf) Header, 不得仅凭文件名猜测
- 硬门限: 只允许 T1-T4 四套规范设备 ID; 所有 Light 必须能归属其中之一

输出:
  TESTDATA_EQUIPMENT_CATALOG.csv  — 设备档案 (每套设备一行)
  TESTDATA_DATASET_CATALOG.csv   — 数据集清单 (每个 Light 目录一行)
  FILTER_ALIAS_MAP.json          — 滤镜规范名与别名映射
  DOCUMENT_FACT_CONFLICTS.md     — 文档与 Header 事实冲突报告
"""

from __future__ import annotations

import csv
import json
import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from astropy.io import fits


REPO_ROOT = Path(__file__).resolve().parents[4]  # scripts/ -> P10-001/ -> evidence/ -> engineering_v1.2/ -> repo root
TESTDATA_DIR = REPO_ROOT / "testdata"
OUT_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P10-001"
RAW_LOGS_DIR = OUT_DIR / "raw_logs"


# ---------------------------------------------------------------------------
# 文档说明解析 (素材信息*.txt)
# ---------------------------------------------------------------------------

# 设备 ID -> 设备档案 (来自说明文档的声明)
@dataclass
class EquipmentRecord:
    device_id: str              # T1/T2/T3/T4
    telescope: str = ""         # 望远镜型号
    focal_length_mm: str = ""   # 焦距
    camera: str = ""            # 相机
    mount: str = ""             # 赤道仪
    filter_set_doc: str = ""    # 说明文档声明的滤镜集合
    image_size_doc: str = ""    # 说明文档声明的图像尺寸 (若提及)
    bin_doc: str = ""           # 说明文档声明的 Bin
    gain_doc: str = ""          # 说明文档声明的 Gain
    offset_doc: str = ""        # 说明文档声明的 Offset
    temp_doc: str = ""          # 说明文档声明的温度
    light_dir: str = ""         # Light 目录 (相对路径)
    master_dir: str = ""        # Master 目录 (相对路径)
    doc_source: str = ""        # 文档来源 (相对路径)
    doc_conflict_note: str = "" # 冲突说明


@dataclass
class DatasetRecord:
    target_name: str            # 目标天体名 (Galaxy_Center / LDN43 / NGC1727 ...)
    device_id: str              # T1/T2/T3/T4
    panel_id: str = ""          # 面板 (panel1/panel2/panel3 或 "")
    filter_in_filename: str = ""  # 文件名中的滤镜名
    filter_in_header: str = ""    # FITS Header 中的滤镜名
    exposure_s: float = 0.0       # 单张曝光 (秒)
    n_lights: int = 0             # 该 (target, device, panel, filter) 组合的 Light 帧数
    light_dir: str = ""           # Light 目录 (相对路径)
    image_size_from_header: str = ""  # FITS Header 中的图像尺寸 (NAXIS1 x NAXIS2)
    bin_from_header: str = ""        # FITS Header 中的 Bin
    gain_from_header: str = ""       # FITS Header 中的 Gain
    offset_from_header: str = ""      # FITS Header 中的 Offset
    temp_from_header: str = ""        # FITS Header 中的温度
    camera_from_header: str = ""      # FITS Header 中的相机
    telescope_from_header: str = ""    # FITS Header 中的望远镜
    date_obs_from_header: str = ""    # FITS Header 中的观测日期
    filter_alias_conflict: str = ""   # 滤镜别名冲突说明
    sample_file: str = ""             # 采样文件 (相对路径, 用于 Header 交叉验证)


# ---------------------------------------------------------------------------
# 说明文档解析
# ---------------------------------------------------------------------------

# 目录名 -> 设备 ID 映射 (硬门限: 只允许 T1-T4)
def derive_device_id_from_dirname(dirname: str) -> str:
    """从目录名提取设备 ID (T1/T2/T3/T4). 未识别返回空字符串.
    支持 _T2_ 和 _T2中文 和 _T2素材 等格式."""
    m = re.search(r"_T([1-4])(?:[_\s]|$|[^a-zA-Z0-9])", dirname)
    if m:
        return f"T{m.group(1)}"
    return ""


def parse_doc_file(txt_path: Path) -> dict[str, str]:
    """解析素材信息*.txt, 返回关键字段. 不依赖精确格式, 用宽松正则."""
    text = txt_path.read_text(encoding="utf-8", errors="replace")
    info: dict[str, str] = {"raw_text": text, "doc_source": str(txt_path.relative_to(REPO_ROOT))}
    # 望远镜 (含焦距)
    m = re.search(r"望远镜[：:]\s*(.+)", text)
    if m:
        info["telescope_line"] = m.group(1).strip()
    # 从 telescope 行提取焦距数字 (如 "焦距1900mm" -> 1900)
    if "telescope_line" in info:
        fm = re.search(r"焦距\s*(\d+(?:\.\d+)?)\s*mm", info["telescope_line"])
        if fm:
            info["focal_length_mm"] = fm.group(1)
    # 相机
    m = re.search(r"相机[：:]\s*(.+)", text)
    if m:
        info["camera"] = m.group(1).strip()
    # 赤道仪
    m = re.search(r"赤道仪[：:]\s*(.+)", text)
    if m:
        info["mount"] = m.group(1).strip()
    # 滤镜
    m = re.search(r"滤镜[：:]\s*(.+)", text)
    if m:
        info["filter_set_doc"] = m.group(1).strip()
    # 单张曝光
    m = re.search(r"单张曝光[：:]\s*(.+)", text)
    if m:
        info["per_exposure_doc"] = m.group(1).strip()
    # 曝光时间 (总计)
    m = re.search(r"曝光时间[：:]\s*(.+)", text)
    if m:
        info["total_exposure_doc"] = m.group(1).strip()
    return info


# ---------------------------------------------------------------------------
# FITS Header 读取
# ---------------------------------------------------------------------------

# 关键 Header 关键字 (按优先级顺序尝试)
HEADER_KEY_CANDIDATES = {
    "filter":   ["FILTER", "FILTERS", "FILT1", "FILT2", "FILTER1", "FILTER2"],
    "exposure": ["EXPOSURE", "EXPTIME", "EXP"],
    "camera":   ["CAMERA", "INSTRUME", "DETECTOR", "CAMNAME"],
    "telescope":["TELESCOP", "TELESCOPE", "TEL"],
    "bin":       ["XBINNING", "BINNING", "BINX"],
    "gain":      ["GAIN", "EGAIN", "GAINCAP"],
    "offset":    ["OFFSET", "BLACKLEV"],
    "temp":      ["CCD-TEMP", "SET-TEMP", "CCDTEMP", "TEMP"],
    "date_obs":  ["DATE-OBS", "DATEOBS"],
    "object":    ["OBJECT", "OBJNAME", "TARGET"],
    "ra":        ["RA", "OBJCTRA", "CRVAL1"],
    "dec":       ["DEC", "OBJCTDEC", "CRVAL2"],
    "xpixsz":    ["XPIXSZ", "PIXSIZE1"],
    "ypixsz":    ["YPIXSZ", "PIXSIZE2"],
    "imagew":    ["NAXIS1", "IMAGEW"],
    "imageh":    ["NAXIS2", "IMAGEH"],
    "instrument":["INSTRUME"],
    "focallen":  ["FOCALLEN", "FOCLENGTH"],
}


def get_header_value(hdr: fits.Header, key_candidates: list[str]) -> str:
    for k in key_candidates:
        if k in hdr:
            v = hdr[k]
            if isinstance(v, (int, float)):
                return str(v)
            return str(v).strip()
    return ""


def read_fits_header(path: Path) -> dict[str, str]:
    """读取 FITS (.fts) Header, 返回关键字段."""
    try:
        with fits.open(path, memmap=True) as hdul:
            hdr = hdul[0].header
            rec: dict[str, str] = {}
            for field_name, keys in HEADER_KEY_CANDIDATES.items():
                rec[field_name] = get_header_value(hdr, keys)
            # 图像尺寸
            rec["image_size"] = f"{rec.get('imagew','')}x{rec.get('imageh','')}"
            return rec
    except Exception as e:
        return {"_error": f"{type(e).__name__}: {e}", "_path": str(path)}


# ---------------------------------------------------------------------------
# XISF Header 读取
# ---------------------------------------------------------------------------

def read_xisf_header(path: Path) -> dict[str, str]:
    """读取 XISF 文件的 XML Header.
    支持两种 XISF 变体:
      - 标准 XISF 1.0: magic(4) + header_len(4 LE) + XML
      - PixInsight 变体: magic(4) + version '0100'(4) + header_len(4 LE) + reserved(4) + XML
    自动检测格式变体.
    """
    import struct
    try:
        with open(path, "rb") as f:
            head = f.read(16)
            if head[:4] != b"XISF":
                return {"_error": f"not XISF magic: {head[:4]!r}"}
            # 检测格式变体: 字节 4-7 是否是 ASCII '0100' (版本号)
            bytes_4_7 = head[4:8]
            if bytes_4_7 == b"0100":
                # PixInsight 变体: header_len 在字节 8-11 (LE), XML 从字节 16 开始
                hdr_len = struct.unpack("<I", head[8:12])[0]
                xml_bytes = f.read(hdr_len)
            else:
                # 标准 XISF 1.0: header_len 在字节 4-7 (LE), XML 从字节 8 开始
                hdr_len = struct.unpack("<I", bytes_4_7)[0]
                xml_bytes = f.read(hdr_len)
            xml_text = xml_bytes.decode("utf-8", errors="replace")
        # 去除 namespace 声明, 使 ET.fromstring 用普通标签名 (无 namespace 前缀)
        # 这是因为 XISF XML 有 xmlns="http://www.pixinsight.com/xisf", 会导致
        # root.iter("Image") 不匹配 {namespace}Image
        xml_text_clean = re.sub(r'\s+xmlns="[^"]*"', '', xml_text, count=1)
        xml_text_clean = re.sub(r'\s+xmlns:[a-z]+="[^"]*"', '', xml_text_clean)
        xml_text_clean = re.sub(r'\s+xsi:[a-zA-Z]+="[^"]*"', '', xml_text_clean)
        root = ET.fromstring(xml_text_clean)
        rec: dict[str, str] = {"_xml_snippet": xml_text[:500]}
        # 查找 Image 元素
        for img in root.iter("Image"):
            for k, v in img.attrib.items():
                rec[f"img_{k}"] = v
            # 查找 Property 子元素
            for prop in img.iter("Property"):
                name = prop.attrib.get("name", "")
                value = prop.attrib.get("value", "")
                if name:
                    rec[name] = value
            # 查找 FITSKeyword 元素 (PixInsight 风格, 含 FITS 兼容关键字)
            for fk in img.iter("FITSKeyword"):
                name = fk.attrib.get("name", "")
                value = fk.attrib.get("value", "")
                # 去除 PixInsight 的单引号包裹
                if value.startswith("'") and value.endswith("'"):
                    value = value[1:-1]
                if name:
                    rec[name] = value
        # 提取常见字段
        out: dict[str, str] = {}
        for field_name, keys in HEADER_KEY_CANDIDATES.items():
            for k in keys:
                if k in rec:
                    out[field_name] = rec[k]
                    break
                if k.lower() in rec:
                    out[field_name] = rec[k.lower()]
                    break
        # XISF/PixInsight 常用关键字
        for k in ["Instrument", "Camera", "FocalLength", "PixelSize", "XResolution", "YResolution",
                  "Filter", "ExposureTime", "Gain", "Offset", "CCDTemperature",
                  "ObservationDate", "Object", "RA", "Dec", "IMAGETYP"]:
            if k in rec and k not in out:
                if k == "Instrument" and "camera" not in out:
                    out["camera"] = rec[k]
                elif k == "Camera" and "camera" not in out:
                    out["camera"] = rec[k]
                elif k == "Filter" and "filter" not in out:
                    out["filter"] = rec[k]
                elif k == "ExposureTime" and "exposure" not in out:
                    out["exposure"] = rec[k]
                elif k == "FocalLength" and "focallen" not in out:
                    out["focallen"] = rec[k]
                elif k == "IMAGETYP":
                    out["imagetyp"] = rec[k]
        # 图像尺寸 (从 geometry 属性)
        w = rec.get("img_geometry", "")
        if w:
            parts = w.split(":")
            if len(parts) >= 2:
                out["image_size"] = f"{parts[0]}x{parts[1]}"
                out["imagew"] = parts[0]
                out["imageh"] = parts[1]
        return out
    except Exception as e:
        return {"_error": f"{type(e).__name__}: {e}", "_path": str(path)}


# ---------------------------------------------------------------------------
# 滤镜规范名归一化
# ---------------------------------------------------------------------------

# 滤镜规范名 (大写, 用于 FILTER_ALIAS_MAP)
CANONICAL_FILTERS = {
    "LUM": ["Lum", "Luminance", "L", "LUM"],
    "RED": ["Red", "RED", "R"],
    "GREEN": ["Green", "GREEN", "G"],
    "BLUE": ["Blue", "BLUE", "B"],
    "HA": ["H-alpha", "Halpha", "Ha", "HA", "H-Alpha"],
    "OIII": ["OIII", "Oiii", "Oiii", "O3", "O-III"],
}

# 反向映射: 别名 -> 规范名
ALIAS_TO_CANONICAL: dict[str, str] = {}
for canonical, aliases in CANONICAL_FILTERS.items():
    for a in aliases:
        ALIAS_TO_CANONICAL[a.lower()] = canonical


def canonicalize_filter(raw: str) -> str:
    if not raw:
        return ""
    raw_stripped = raw.strip()
    return ALIAS_TO_CANONICAL.get(raw_stripped.lower(), raw_stripped.upper())


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------

def discover_testdata_subdirs() -> list[Path]:
    """发现 testdata/ 下的所有子目录 (排除 report/results)."""
    excluded = {"report", "results"}
    return sorted([d for d in TESTDATA_DIR.iterdir()
                   if d.is_dir() and d.name not in excluded])


def discover_calibration_dirs() -> dict[str, Path]:
    """发现 T2/T3/T4 calibration files 目录."""
    result = {}
    for d in TESTDATA_DIR.iterdir():
        if d.is_dir() and "calibration" in d.name.lower():
            m = re.search(r"T([1-4])", d.name)
            if m:
                result[f"T{m.group(1)}"] = d
    return result


def derive_panel_from_filename(fname: str) -> str:
    """从文件名提取 panel ID."""
    m = re.search(r"mosaic(\d+)", fname, re.IGNORECASE)
    if m:
        return f"panel{m.group(1)}"
    return ""


def derive_filter_from_filename(fname: str) -> str:
    """从文件名提取滤镜名 (倒数第二段, 在 -<exposure>S- 之后)."""
    # 模式: ...-180S-Red.fts 或 ...-600S-Oiii.fts
    m = re.search(r"-(\d+)S-([^.]+)\.\w+$", fname)
    if m:
        return m.group(2)
    return ""


def derive_exposure_from_filename(fname: str) -> float:
    """从文件名提取曝光时间 (秒)."""
    m = re.search(r"-(\d+)S-", fname)
    if m:
        return float(m.group(1))
    return 0.0


def main():
    log_lines: list[str] = []
    def log(msg: str):
        print(msg, flush=True)
        log_lines.append(msg)

    log("=" * 70)
    log("P10-001 extract_testdata_catalog.py 启动")
    log(f"REPO_ROOT = {REPO_ROOT}")
    log(f"TESTDATA_DIR = {TESTDATA_DIR}")
    log(f"OUT_DIR = {OUT_DIR}")
    log("=" * 70)

    # --- 阶段 1: 发现所有 TestData 子目录 ---
    subdirs = discover_testdata_subdirs()
    calib_dirs = discover_calibration_dirs()
    log(f"\n[阶段1] 发现 {len(subdirs)} 个 TestData 子目录:")
    for d in subdirs:
        log(f"  - {d.name}")
    log(f"发现 {len(calib_dirs)} 个校准目录: {list(calib_dirs.keys())}")

    # --- 阶段 2: 解析所有说明文档 ---
    equipment_records: dict[str, EquipmentRecord] = {}  # device_id -> EquipmentRecord
    dataset_records: list[DatasetRecord] = []
    doc_records: list[dict] = []  # 文档事实 (用于冲突检查)
    unknown_device_dirs: list[str] = []

    for subdir in subdirs:
        dirname = subdir.name
        device_id = derive_device_id_from_dirname(dirname)
        if not device_id:
            # 检查是否是 calibration 目录
            if "calibration" in dirname.lower():
                continue  # calibration 目录单独处理
            unknown_device_dirs.append(dirname)
            log(f"  [警告] 未识别设备 ID: {dirname}")
            continue

        # 查找说明文档
        doc_files = list(subdir.glob("素材信息*.txt")) + list(subdir.glob("*.txt"))
        if not doc_files:
            log(f"  [警告] {dirname} 无说明文档")
            doc_info = {"doc_source": "", "raw_text": ""}
        else:
            doc_path = doc_files[0]
            doc_info = parse_doc_file(doc_path)
            log(f"  [文档] {dirname} <- {doc_path.name}")

        # 查找 lights 子目录
        lights_dir = subdir / "lights"
        if not lights_dir.exists():
            # 部分目录可能直接在 subdir 下放 .fts (LDN43 是 lights/ 子目录)
            lights_dir = subdir

        # 创建/更新设备记录
        if device_id not in equipment_records:
            er = EquipmentRecord(device_id=device_id)
            er.telescope = doc_info.get("telescope_line", "")
            er.focal_length_mm = doc_info.get("focal_length_mm", "")
            er.camera = doc_info.get("camera", "")
            er.mount = doc_info.get("mount", "")
            er.filter_set_doc = doc_info.get("filter_set_doc", "")
            er.doc_source = doc_info.get("doc_source", "")
            er.light_dir = str(lights_dir.relative_to(REPO_ROOT))
            equipment_records[device_id] = er
        else:
            # 设备已存在: 追加 light_dir (分号分隔)
            er = equipment_records[device_id]
            new_light_dir = str(lights_dir.relative_to(REPO_ROOT))
            if new_light_dir not in er.light_dir:
                er.light_dir = (er.light_dir + ";" + new_light_dir) if er.light_dir else new_light_dir

        # 记录文档事实
        doc_records.append({
            "device_id": device_id,
            "target_dir": dirname,
            "doc_source": doc_info.get("doc_source", ""),
            "telescope_line": doc_info.get("telescope_line", ""),
            "camera": doc_info.get("camera", ""),
            "mount": doc_info.get("mount", ""),
            "filter_set_doc": doc_info.get("filter_set_doc", ""),
            "per_exposure_doc": doc_info.get("per_exposure_doc", ""),
            "total_exposure_doc": doc_info.get("total_exposure_doc", ""),
        })

    # --- 阶段 3: 补充校准目录到设备档案 (Master 目录) ---
    for dev_id, cdir in calib_dirs.items():
        if dev_id in equipment_records:
            equipment_records[dev_id].master_dir = str(cdir.relative_to(REPO_ROOT))
        else:
            er = EquipmentRecord(device_id=dev_id)
            er.master_dir = str(cdir.relative_to(REPO_ROOT))
            er.doc_source = "(无说明文档, 仅校准文件目录)"
            equipment_records[dev_id] = er

    # --- 阶段 4: 读取 FITS Header (每个 Light 子目录采样 1-2 个文件) ---
    log(f"\n[阶段4] 读取 FITS Header (每个 Light 子目录采样)")
    header_samples: dict[str, dict[str, str]] = {}  # 子目录 -> header 字段

    # 按 (device, target, panel, filter) 分组统计
    group_stats: dict[tuple[str, str, str, str], dict] = defaultdict(lambda: {
        "n_lights": 0, "light_dir": "", "sample_file": "", "header": {}
    })

    for subdir in subdirs:
        dirname = subdir.name
        device_id = derive_device_id_from_dirname(dirname)
        if not device_id:
            continue

        lights_dir = subdir / "lights"
        if not lights_dir.exists():
            lights_dir = subdir

        # 递归查找所有 .fts 文件
        fts_files = sorted(lights_dir.rglob("*.fts"))
        if not fts_files:
            log(f"  [警告] {dirname} 无 .fts 文件")
            continue

        target_name = dirname.split("_T")[0] if "_T" in dirname else dirname
        # 清理 target_name (如 LDN43_T2素材 -> LDN43)
        target_name = re.sub(r"_T[1-4].*$", "", target_name)

        # 按 (panel, filter) 分组
        per_group: dict[tuple[str, str], list[Path]] = defaultdict(list)
        for fts in fts_files:
            fname = fts.name
            panel = derive_panel_from_filename(fname)
            filt = derive_filter_from_filename(fname)
            per_group[(panel, filt)].append(fts)

        for (panel, filt), files in per_group.items():
            # 采样第一个文件读 Header
            sample = files[0]
            hdr = read_fits_header(sample)
            key = f"{dirname}/{panel}/{filt}"
            header_samples[key] = hdr
            rel_sample = str(sample.relative_to(REPO_ROOT))
            log(f"  [Header] {key}: filter={hdr.get('filter','')} exp={hdr.get('exposure','')} "
                f"size={hdr.get('image_size','')} camera={hdr.get('camera','')[:30]}")

            grp_key = (device_id, target_name, panel, filt)
            group_stats[grp_key]["n_lights"] = len(files)
            group_stats[grp_key]["light_dir"] = str(lights_dir.relative_to(REPO_ROOT))
            group_stats[grp_key]["sample_file"] = rel_sample
            group_stats[grp_key]["header"] = hdr

    # --- 阶段 5: 读取 XISF Header (校准文件) ---
    log(f"\n[阶段5] 读取 XISF Header (校准文件)")
    xisf_samples: dict[str, dict[str, str]] = {}
    for dev_id, cdir in calib_dirs.items():
        xisf_files = sorted(cdir.glob("*.xisf"))
        for xf in xisf_files:
            hdr = read_xisf_header(xf)
            key = f"{dev_id}/{xf.name}"
            xisf_samples[key] = hdr
            log(f"  [XISF] {key}: filter={hdr.get('filter','')} size={hdr.get('image_size','')} "
                f"error={hdr.get('_error','')}")

    # --- 阶段 6: 交叉验证文档与 Header, 收集冲突 ---
    conflicts: list[dict] = []

    # 6.1 设备级冲突: 望远镜/相机/焦距
    for dev_id, er in equipment_records.items():
        # 找一个该设备的 Header 样本
        dev_headers = [v for k, v in header_samples.items() if k.startswith(f"{dev_id}_") or f"_T{dev_id[1]}_" in k]
        if not dev_headers:
            # 尝试从 xisf
            dev_headers = [v for k, v in xisf_samples.items() if k.startswith(f"{dev_id}/")]
        if not dev_headers:
            continue
        hdr = dev_headers[0]
        # 相机冲突
        if er.camera and hdr.get("camera"):
            # 提取文档中的相机型号 (如 "FLI Microline 16200" -> "16200")
            doc_camera = er.camera
            hdr_camera = hdr["camera"]
            # 简单包含检查
            doc_num = re.search(r"\d{4,}", doc_camera)
            hdr_num = re.search(r"\d{4,}", hdr_camera)
            if doc_num and hdr_num and doc_num.group() != hdr_num.group():
                conflicts.append({
                    "type": "camera_mismatch",
                    "device_id": dev_id,
                    "field": "camera",
                    "doc_value": doc_camera,
                    "header_value": hdr_camera,
                    "source": er.doc_source,
                })
        # 图像尺寸冲突 (从校准文件名 vs Header)
        # 已在 calib_dirs 文件名中体现 (如 4096x4096 vs 4500x3600)

    # 6.2 滤镜别名冲突: 文件名 vs Header
    for grp_key, info in group_stats.items():
        device_id, target, panel, fname_filter = grp_key
        hdr = info["header"]
        hdr_filter = hdr.get("filter", "")
        if fname_filter and hdr_filter:
            canon_fname = canonicalize_filter(fname_filter)
            canon_hdr = canonicalize_filter(hdr_filter)
            if canon_fname != canon_hdr:
                conflicts.append({
                    "type": "filter_alias_mismatch",
                    "device_id": device_id,
                    "target": target,
                    "panel": panel,
                    "field": "filter",
                    "filename_value": fname_filter,
                    "header_value": hdr_filter,
                    "canonical_filename": canon_fname,
                    "canonical_header": canon_hdr,
                    "sample_file": info["sample_file"],
                })

    # 6.3 校准文件滤镜名冲突 (T2 OIII vs T3/T4 Oiii)
    calib_filter_aliases: dict[str, set[str]] = defaultdict(set)
    for dev_id, cdir in calib_dirs.items():
        for xf in cdir.glob("masterFlat_*.xisf"):
            m = re.search(r"FILTER-(.+?)_mono", xf.name)
            if m:
                calib_filter_aliases[canonicalize_filter(m.group(1))].add(m.group(1))
    for canon, aliases in calib_filter_aliases.items():
        if len(aliases) > 1:
            conflicts.append({
                "type": "calibration_filter_alias_inconsistency",
                "canonical": canon,
                "aliases": sorted(aliases),
                "note": "校准文件中同一规范滤镜使用了不同的别名拼写, 必须在 P10-004 统一",
            })

    # --- 阶段 7: 写 TESTDATA_EQUIPMENT_CATALOG.csv ---
    equip_csv = OUT_DIR / "TESTDATA_EQUIPMENT_CATALOG.csv"
    with open(equip_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow([
            "device_id", "telescope", "focal_length_mm", "camera", "mount",
            "filter_set_doc", "image_size_doc", "bin_doc", "gain_doc",
            "offset_doc", "temp_doc", "light_dir", "master_dir",
            "doc_source", "doc_conflict_note"
        ])
        for dev_id in sorted(equipment_records.keys()):
            er = equipment_records[dev_id]
            # 从 Header 样本补充 image_size
            image_size_hdr = ""
            for k, v in header_samples.items():
                if dev_id in k and v.get("image_size"):
                    image_size_hdr = v["image_size"]
                    break
            if not image_size_hdr:
                for k, v in xisf_samples.items():
                    if k.startswith(f"{dev_id}/") and v.get("image_size"):
                        image_size_hdr = v["image_size"]
                        break
            w.writerow([
                er.device_id, er.telescope, er.focal_length_mm, er.camera, er.mount,
                er.filter_set_doc, image_size_hdr or er.image_size_doc, er.bin_doc,
                er.gain_doc, er.offset_doc, er.temp_doc, er.light_dir, er.master_dir,
                er.doc_source, er.doc_conflict_note
            ])
    log(f"\n[阶段7] 写出 {equip_csv.name}")

    # --- 阶段 8: 写 TESTDATA_DATASET_CATALOG.csv ---
    ds_csv = OUT_DIR / "TESTDATA_DATASET_CATALOG.csv"
    with open(ds_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow([
            "target_name", "device_id", "panel_id", "filter_in_filename",
            "filter_in_header", "exposure_s", "n_lights", "light_dir",
            "image_size_from_header", "bin_from_header", "gain_from_header",
            "offset_from_header", "temp_from_header", "camera_from_header",
            "telescope_from_header", "date_obs_from_header",
            "filter_alias_conflict", "sample_file"
        ])
        for grp_key in sorted(group_stats.keys()):
            device_id, target, panel, fname_filter = grp_key
            info = group_stats[grp_key]
            hdr = info["header"]
            hdr_filter = hdr.get("filter", "")
            conflict_note = ""
            if fname_filter and hdr_filter:
                if canonicalize_filter(fname_filter) != canonicalize_filter(hdr_filter):
                    conflict_note = f"filename={fname_filter} vs header={hdr_filter}"
            w.writerow([
                target, device_id, panel, fname_filter, hdr_filter,
                hdr.get("exposure", ""), info["n_lights"], info["light_dir"],
                hdr.get("image_size", ""), hdr.get("bin", ""), hdr.get("gain", ""),
                hdr.get("offset", ""), hdr.get("temp", ""), hdr.get("camera", ""),
                hdr.get("telescope", ""), hdr.get("date_obs", ""),
                conflict_note, info["sample_file"]
            ])
    log(f"[阶段8] 写出 {ds_csv.name}")

    # --- 阶段 9: 写 FILTER_ALIAS_MAP.json ---
    # 收集实际观察到的别名
    observed: dict[str, set[str]] = defaultdict(set)
    for d in subdirs:
        lights_dir = d / "lights"
        if not lights_dir.exists():
            lights_dir = d
        for f in lights_dir.rglob("*.fts"):
            fname_f = derive_filter_from_filename(f.name)
            if fname_f:
                observed[canonicalize_filter(fname_f)].add(fname_f)
    for cdir in calib_dirs.values():
        for xf in cdir.glob("masterFlat_*.xisf"):
            m = re.search(r"FILTER-(.+?)_mono", xf.name)
            if m:
                observed[canonicalize_filter(m.group(1))].add(m.group(1))
    # Header 中的滤镜名
    for hdr in list(header_samples.values()) + list(xisf_samples.values()):
        hf = hdr.get("filter", "")
        if hf and not hf.startswith("_"):
            observed[canonicalize_filter(hf)].add(hf)

    alias_map = {
        "_description": "滤镜规范名与别名映射 (P10-001 冻结). 规范名=大写, 别名=实际观察到的拼写.",
        "_canonical_to_aliases": {c: sorted(a) for c, a in CANONICAL_FILTERS.items()},
        "observed_aliases_by_canonical": {
            c: sorted(a) for c, a in observed.items()
        },
        "canonical_to_preferred_alias": {
            "LUM": "Lum", "RED": "Red", "GREEN": "Green", "BLUE": "Blue",
            "HA": "H-alpha", "OIII": "OIII",
        },
    }
    alias_json = OUT_DIR / "FILTER_ALIAS_MAP.json"
    with open(alias_json, "w", encoding="utf-8") as f:
        json.dump(alias_map, f, ensure_ascii=False, indent=2)
    log(f"[阶段9] 写出 {alias_json.name}")

    # --- 阶段 10: 写 DOCUMENT_FACT_CONFLICTS.md ---
    conflicts_md = OUT_DIR / "DOCUMENT_FACT_CONFLICTS.md"
    with open(conflicts_md, "w", encoding="utf-8") as f:
        f.write("# 文档与 Header 事实冲突报告\n\n")
        f.write(f"- 任务: P10-001\n")
        f.write(f"- 生成时间: {__import__('datetime').datetime.now().isoformat()}\n")
        f.write(f"- 测试数据目录: testdata/\n")
        f.write(f"- 发现子目录数: {len(subdirs)}\n")
        f.write(f"- 设备数 (T1-T4): {len(equipment_records)}\n")
        f.write(f"- 未知设备目录: {len(unknown_device_dirs)}\n")
        if unknown_device_dirs:
            f.write(f"  - {', '.join(unknown_device_dirs)}\n")
        f.write(f"- Light 分组数 (target/device/panel/filter): {len(group_stats)}\n")
        f.write(f"- Header 采样数: FITS={len(header_samples)}, XISF={len(xisf_samples)}\n")
        f.write(f"- 冲突数: {len(conflicts)}\n\n")
        f.write("## 冲突明细\n\n")
        if not conflicts:
            f.write("无冲突.\n\n")
        for i, c in enumerate(conflicts, 1):
            f.write(f"### 冲突 {i}: {c['type']}\n\n")
            f.write(f"- 设备: {c.get('device_id', 'N/A')}\n")
            for k, v in c.items():
                if k not in ("type", "device_id"):
                    f.write(f"- {k}: {v}\n")
            f.write("\n")
        # 添加硬门限检查
        f.write("## 硬门限检查\n\n")
        f.write(f"- 只允许 T1-T4 四套规范设备 ID: {'PASS' if set(equipment_records.keys()).issubset({'T1','T2','T3','T4'}) else 'FAIL'}\n")
        f.write(f"  - 实际设备: {sorted(equipment_records.keys())}\n")
        all_lights_attributed = all(derive_device_id_from_dirname(d.name) or 'calibration' in d.name.lower()
                                    for d in subdirs)
        f.write(f"- 所有 Light 必须能归属 T1-T4: {'PASS' if all_lights_attributed else 'FAIL'}\n")
        if unknown_device_dirs:
            f.write(f"  - 未归属目录: {unknown_device_dirs}\n")
    log(f"[阶段10] 写出 {conflicts_md.name}")

    # --- 阶段 11: 写原始日志 ---
    raw_log = RAW_LOGS_DIR / "extract_testdata_catalog.log"
    raw_log.write_text("\n".join(log_lines), encoding="utf-8")
    log(f"[阶段11] 写出 {raw_log.name}")

    # --- 阶段 12: 保存原始 Header 样本 (JSON) ---
    headers_json = RAW_LOGS_DIR / "header_samples.json"
    with open(headers_json, "w", encoding="utf-8") as f:
        json.dump({
            "fits_headers": header_samples,
            "xisf_headers": xisf_samples,
            "doc_records": doc_records,
        }, f, ensure_ascii=False, indent=2, default=str)
    log(f"[阶段12] 写出 {headers_json.name}")

    log(f"\n{'=' * 70}")
    log(f"P10-001 数据提取完成")
    log(f"  设备档案: {equip_csv}")
    log(f"  数据集清单: {ds_csv}")
    log(f"  滤镜别名映射: {alias_json}")
    log(f"  冲突报告: {conflicts_md}")
    log(f"  冲突数: {len(conflicts)}")
    log(f"  硬门限: {'PASS' if not unknown_device_dirs else 'FAIL'}")
    log(f"{'=' * 70}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
