#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P10-003: 盘点全部主校准帧 (Master Bias/Dark/Flat)

依据 docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md:
- 扫描所有 Master Bias/Dark/Flat 文件
- 读取 Header (XISF XML) + 文件名属性
- 计算每个文件的 SHA-256
- 输出 CALIBRATION_MASTER_INVENTORY.csv

禁止捷径: 不得因当前 resolver 找不到就判定文件缺失.
若文件存在但 Header 解析失败, 仍须记录文件名和 hash, 标注 "header_parse_error".

输入:
  testdata/T{N} calibration files/  (3 个目录, T2/T3/T4)
  engineering_v1.2/evidence/P10-001/raw_logs/header_samples.json (Header 采样已有)

输出:
  CALIBRATION_MASTER_INVENTORY.csv - 每行一个 Master 文件
"""

from __future__ import annotations

import csv
import hashlib
import json
import os
import re
import sys
import xml.etree.ElementTree as ET
from datetime import datetime
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
TESTDATA_DIR = REPO_ROOT / "testdata"
P10_001_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P10-001"
OUT_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P10-003"
RAW_LOGS_DIR = OUT_DIR / "raw_logs"


def sha256_file(path: Path, chunk_size: int = 1024 * 1024) -> str:
    """计算文件 SHA-256."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest().upper()


def parse_xisf_header(path: Path) -> dict:
    """读取 XISF 文件的 XML Header (复用 P10-001 的解析逻辑).

    支持两种 XISF 变体:
      - 标准 XISF 1.0: magic(4) + header_len(4 LE) + XML
      - PixInsight 变体: magic(4) + version '0100'(4) + header_len(4 LE) + reserved(4) + XML
    """
    import struct
    try:
        with open(path, "rb") as f:
            head = f.read(16)
            if head[:4] != b"XISF":
                return {"_error": f"not XISF magic: {head[:4]!r}"}
            bytes_4_7 = head[4:8]
            if bytes_4_7 == b"0100":
                hdr_len = struct.unpack("<I", head[8:12])[0]
                xml_bytes = f.read(hdr_len)
            else:
                hdr_len = struct.unpack("<I", bytes_4_7)[0]
                xml_bytes = f.read(hdr_len)
            xml_text = xml_bytes.decode("utf-8", errors="replace")
        # 去 namespace
        xml_text_clean = re.sub(r'\s+xmlns="[^"]*"', '', xml_text, count=1)
        xml_text_clean = re.sub(r'\s+xmlns:[a-z]+="[^"]*"', '', xml_text_clean)
        xml_text_clean = re.sub(r'\s+xsi:[a-zA-Z]+="[^"]*"', '', xml_text_clean)
        root = ET.fromstring(xml_text_clean)
        rec: dict = {"_xml_snippet": xml_text[:500]}
        for img in root.iter("Image"):
            for k, v in img.attrib.items():
                rec[f"img_{k}"] = v
            for prop in img.iter("Property"):
                name = prop.attrib.get("name", "")
                value = prop.attrib.get("value", "")
                if name:
                    rec[name] = value
            for fk in img.iter("FITSKeyword"):
                name = fk.attrib.get("name", "")
                value = fk.attrib.get("value", "")
                if value.startswith("'") and value.endswith("'"):
                    value = value[1:-1]
                if name:
                    rec[name] = value
        return rec
    except Exception as e:
        return {"_error": f"{type(e).__name__}: {e}"}


def derive_master_type(filename: str) -> str:
    """从文件名推导 Master 类型 (Bias/Dark/Flat)."""
    fn_lower = filename.lower()
    if "masterbias" in fn_lower or "master_bias" in fn_lower or "master-bias" in fn_lower:
        return "Bias"
    if "masterdark" in fn_lower or "master_dark" in fn_lower or "master-dark" in fn_lower:
        return "Dark"
    if "masterflat" in fn_lower or "master_flat" in fn_lower or "master-flat" in fn_lower:
        return "Flat"
    return "Unknown"


def derive_device_id(path: Path) -> str:
    """从路径推导设备 ID."""
    # 路径形如 testdata/T2 calibration files/...
    parts = path.parts
    for p in parts:
        m = re.search(r"T([1-4])", p)
        if m and "calibration" in p.lower():
            return f"T{m.group(1)}"
    return ""


def derive_filter_from_filename(filename: str) -> str:
    """从文件名提取滤镜 (Flat)."""
    m = re.search(r"FILTER-([^-_]+?)_mono", filename, re.IGNORECASE)
    if m:
        return m.group(1)
    return ""


def derive_exposure_from_filename(filename: str) -> str:
    """从文件名提取曝光 (Dark)."""
    m = re.search(r"EXPOSURE-([\d.]+)s", filename, re.IGNORECASE)
    if m:
        return m.group(1)
    return ""


def derive_bin_from_filename(filename: str) -> str:
    """从文件名提取 Bin."""
    m = re.search(r"BIN-(\d+)", filename, re.IGNORECASE)
    if m:
        return m.group(1)
    return ""


def derive_image_size_from_filename(filename: str) -> str:
    """从文件名提取图像尺寸."""
    m = re.search(r"(\d+)x(\d+)", filename)
    if m:
        return f"{m.group(1)}x{m.group(2)}"
    return ""


def get_header_value(rec: dict, keys: list[str]) -> str:
    """从 Header 记录中按候选 key 顺序取值."""
    for k in keys:
        if k in rec:
            v = rec[k]
            if isinstance(v, (int, float)):
                return str(v)
            return str(v).strip()
        # 大小写不敏感
        for rk, rv in rec.items():
            if rk.lower() == k.lower():
                if isinstance(rv, (int, float)):
                    return str(rv)
                return str(rv).strip()
    return ""


# 关键 Header 关键字 (按优先级)
HEADER_KEYS = {
    "filter":   ["FILTER", "FILTERS", "FILT1", "FILT2"],
    "exposure": ["EXPOSURE", "EXPTIME", "EXP", "ExposureTime"],
    "bin":      ["XBINNING", "BINNING", "BINX"],
    "temp":     ["CCD-TEMP", "SET-TEMP", "CCDTEMP", "TEMP", "CCDTemperature"],
    "imagew":   ["NAXIS1", "IMAGEW"],
    "imageh":   ["NAXIS2", "IMAGEH"],
    "xpixsz":   ["XPIXSZ", "PIXSIZE1"],
    "instrument": ["INSTRUME", "Instrument"],
}


def main() -> int:
    log_lines: list[str] = []
    def log(msg: str):
        print(msg, flush=True)
        log_lines.append(msg)

    log("=" * 70)
    log("P10-003 extract_calibration_master_inventory.py 启动")
    log(f"REPO_ROOT = {REPO_ROOT}")
    log(f"OUT_DIR = {OUT_DIR}")
    log("=" * 70)

    # 发现所有 calibration 目录
    calib_dirs = sorted([d for d in TESTDATA_DIR.iterdir()
                         if d.is_dir() and "calibration" in d.name.lower()])
    log(f"\n[发现] 校准目录: {len(calib_dirs)}")
    for d in calib_dirs:
        log(f"  {d.name}")

    # 收集所有 .xisf 文件
    inventory_rows = []
    for calib_dir in calib_dirs:
        device_id = derive_device_id(calib_dir)
        if not device_id:
            log(f"[WARNING] 无法识别设备 ID: {calib_dir.name}")
            continue
        xisf_files = sorted(list(calib_dir.glob("*.xisf")))
        log(f"\n[扫描] {device_id}: {len(xisf_files)} 个 XISF 文件")
        for xisf in xisf_files:
            rel_path = str(xisf.relative_to(REPO_ROOT))
            log(f"  处理: {xisf.name}")

            # SHA-256
            sha = sha256_file(xisf)
            file_size = xisf.stat().st_size

            # 文件名属性
            master_type = derive_master_type(xisf.name)
            filter_fn = derive_filter_from_filename(xisf.name)
            exposure_fn = derive_exposure_from_filename(xisf.name)
            bin_fn = derive_bin_from_filename(xisf.name)
            image_size_fn = derive_image_size_from_filename(xisf.name)

            # Header 属性
            hdr = parse_xisf_header(xisf)
            header_error = ""
            if "_error" in hdr:
                header_error = hdr["_error"]
                log(f"    [HEADER ERROR] {header_error}")

            filter_hdr = get_header_value(hdr, HEADER_KEYS["filter"])
            exposure_hdr = get_header_value(hdr, HEADER_KEYS["exposure"])
            bin_hdr = get_header_value(hdr, HEADER_KEYS["bin"])
            temp_hdr = get_header_value(hdr, HEADER_KEYS["temp"])
            imagew_hdr = get_header_value(hdr, HEADER_KEYS["imagew"])
            imageh_hdr = get_header_value(hdr, HEADER_KEYS["imageh"])
            xpixsz_hdr = get_header_value(hdr, HEADER_KEYS["xpixsz"])
            instrument_hdr = get_header_value(hdr, HEADER_KEYS["instrument"])

            image_size_hdr = ""
            if imagew_hdr and imageh_hdr:
                image_size_hdr = f"{imagew_hdr}x{imageh_hdr}"

            # 优先使用 Header 值, 缺失时回退到文件名解析
            row = {
                "file_path": rel_path,
                "file_name": xisf.name,
                "device_id": device_id,
                "master_type": master_type,
                "file_size_bytes": file_size,
                "sha256": sha,
                # 文件名属性
                "filter_from_filename": filter_fn,
                "exposure_from_filename": exposure_fn,
                "bin_from_filename": bin_fn,
                "image_size_from_filename": image_size_fn,
                # Header 属性 (优先)
                "filter_from_header": filter_hdr,
                "exposure_from_header": exposure_hdr,
                "bin_from_header": bin_hdr,
                "image_size_from_header": image_size_hdr,
                "temp_from_header": temp_hdr,
                "pixel_size_from_header": xpixsz_hdr,
                "instrument_from_header": instrument_hdr,
                # 一致性检查
                "filter_match": "YES" if (filter_fn == filter_hdr or not filter_fn or not filter_hdr) else "NO",
                "exposure_match": "YES" if (exposure_fn == exposure_hdr or not exposure_fn or not exposure_hdr) else "NO",
                "bin_match": "YES" if (bin_fn == bin_hdr or not bin_fn or not bin_hdr) else "NO",
                "image_size_match": "YES" if (image_size_fn == image_size_hdr or not image_size_fn or not image_size_hdr) else "NO",
                # 错误
                "header_parse_error": header_error,
                # 元数据
                "scanned_at": datetime.utcnow().isoformat() + "Z",
            }
            inventory_rows.append(row)

            log(f"    type={master_type}, filter={filter_hdr or filter_fn}, "
                f"exp={exposure_hdr or exposure_fn}, bin={bin_hdr or bin_fn}, "
                f"size={image_size_hdr or image_size_fn}, sha={sha[:16]}...")

    # 写 CSV
    csv_path = OUT_DIR / "CALIBRATION_MASTER_INVENTORY.csv"
    fieldnames = [
        "file_path", "file_name", "device_id", "master_type",
        "file_size_bytes", "sha256",
        "filter_from_filename", "exposure_from_filename", "bin_from_filename", "image_size_from_filename",
        "filter_from_header", "exposure_from_header", "bin_from_header", "image_size_from_header",
        "temp_from_header", "pixel_size_from_header", "instrument_from_header",
        "filter_match", "exposure_match", "bin_match", "image_size_match",
        "header_parse_error", "scanned_at",
    ]
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(inventory_rows)
    log(f"\n[输出] {csv_path}")
    log(f"  总行数: {len(inventory_rows)}")

    # 汇总统计
    by_device = {}
    by_type = {}
    parse_errors = 0
    for r in inventory_rows:
        d = r["device_id"]
        t = r["master_type"]
        by_device[d] = by_device.get(d, 0) + 1
        by_type[t] = by_type.get(t, 0) + 1
        if r["header_parse_error"]:
            parse_errors += 1

    log(f"\n[统计] 按设备:")
    for d, n in sorted(by_device.items()):
        log(f"  {d}: {n} 个文件")
    log(f"[统计] 按类型:")
    for t, n in sorted(by_type.items()):
        log(f"  {t}: {n} 个文件")
    log(f"[统计] Header 解析错误: {parse_errors}")

    # 禁止捷径检查
    forbidden_types = [t for t in by_type if t == "Unknown"]
    log(f"[硬门限] Unknown 类型: {'FAIL' if forbidden_types else 'PASS'}")

    # 写汇总 JSON
    summary = {
        "_description": "P10-003 主校准帧盘点汇总",
        "generated_at": datetime.utcnow().isoformat() + "Z",
        "total_files": len(inventory_rows),
        "by_device": by_device,
        "by_type": by_type,
        "header_parse_errors": parse_errors,
        "hard_gate_unknown_type": "FAIL" if forbidden_types else "PASS",
        "csv_path": str(csv_path.relative_to(REPO_ROOT)),
    }
    summary_path = OUT_DIR / "CALIBRATION_MASTER_INVENTORY_SUMMARY.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)
    log(f"[输出] {summary_path}")

    # 写原始日志
    raw_log = RAW_LOGS_DIR / "extract_calibration_master_inventory.log"
    raw_log.write_text("\n".join(log_lines), encoding="utf-8")
    log(f"[日志] {raw_log}")

    log(f"\n{'=' * 70}")
    log(f"P10-003 完成. 共 {len(inventory_rows)} 个 Master 文件已盘点.")
    log(f"{'=' * 70}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
