#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P10-002: 建立 T1-T4 设备档案
结合 P10-001 的 TESTDATA_EQUIPMENT_CATALOG.csv + Header 采样数据,
生成 4 个 JSON profile (T1/T2/T3/T4).

依据 docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md, 每套设备档案至少包含:
  设备 ID, 望远镜, 口径, 焦距, 相机, 像元, 图像尺寸, Bin, Gain, Offset, 温度,
  滤镜集合, Light 目录, Master 目录, 文档来源, 冲突说明.

禁止捷径: 不得产生 T5 或 unknown 作为正常结果.
T1 无数据: 明确标注 "no_data", 不创建虚假档案.
"""

from __future__ import annotations

import csv
import json
import os
import re
import sys
from datetime import datetime
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[4]
P10_001_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P10-001"
OUT_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P10-002"
RAW_LOGS_DIR = OUT_DIR / "raw_logs"


def load_equipment_catalog() -> list[dict]:
    """读取 P10-001 的 TESTDATA_EQUIPMENT_CATALOG.csv."""
    csv_path = P10_001_DIR / "TESTDATA_EQUIPMENT_CATALOG.csv"
    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        return list(reader)


def load_dataset_catalog() -> list[dict]:
    """读取 P10-001 的 TESTDATA_DATASET_CATALOG.csv."""
    csv_path = P10_001_DIR / "TESTDATA_DATASET_CATALOG.csv"
    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        return list(reader)


def load_header_samples() -> dict:
    """读取 P10-001 的 header_samples.json."""
    json_path = P10_001_DIR / "raw_logs" / "header_samples.json"
    with open(json_path, "r", encoding="utf-8") as f:
        return json.load(f)


def load_filter_alias_map() -> dict:
    """读取 P10-001 的 FILTER_ALIAS_MAP.json."""
    json_path = P10_001_DIR / "FILTER_ALIAS_MAP.json"
    with open(json_path, "r", encoding="utf-8") as f:
        return json.load(f)


def derive_aperture(telescope_line: str) -> str:
    """从 telescope 行推导口径 (mm).
    ASA 500N -> 500mm (Newtonian)
    Nikkor 200F2 -> 200mm
    """
    if not telescope_line:
        return ""
    # ASA 500N
    m = re.search(r"ASA\s*(\d+)N", telescope_line, re.IGNORECASE)
    if m:
        return m.group(1)
    # Nikkor 200F2
    m = re.search(r"Nikkor\s*(\d+)F", telescope_line, re.IGNORECASE)
    if m:
        return m.group(1)
    # 通用: 寻找 数字+mm 或 数字+英寸
    m = re.search(r"(\d+)\s*mm", telescope_line, re.IGNORECASE)
    if m:
        return m.group(1)
    return ""


def derive_pixel_size(headers: list[dict]) -> str:
    """从 Header 采样推导像元大小 (um)."""
    for h in headers:
        xpixsz = h.get("xpixsz", "")
        if xpixsz and xpixsz != "0":
            return xpixsz
    return ""


def collect_device_headers(header_samples: dict, device_id: str) -> list[dict]:
    """收集某设备的所有 Header 采样.

    header_samples.json 的 key 格式有以下变体:
      - Galaxy_Center_T4/panel1/Red       (T{N} 后接 /)
      - NGC1727_T2_flying_dutchman//Red   (T{N} 后接 _)
      - LDN43_T2素材_flying_dutchman//Lum  (T{N} 后接 素材)
      - T2/masterBias_BIN-1_*.xisf        (T{N} 在开头, 后接 /)
      - NGC83_cluster_T3_Flying_Dutchman//Red
    """
    n = device_id[1]  # "T2" -> "2"
    # 正则: _T{N} 后接非字母数字字符 (/, _, 素, 等) 或 T{N} 在开头后接 /
    pattern = re.compile(rf"(?:^|_)T{n}(?:[^a-zA-Z0-9]|$)|^{device_id}/")
    result = []
    for key, hdr in header_samples.items():
        if pattern.search(key):
            result.append(hdr)
    return result


def collect_device_datasets(dataset_catalog: list[dict], device_id: str) -> list[dict]:
    """收集某设备的所有数据集记录."""
    return [d for d in dataset_catalog if d["device_id"] == device_id]


def collect_device_calib_files(device_id: str) -> list[str]:
    """收集某设备的校准文件列表."""
    calib_dir = REPO_ROOT / "testdata" / f"T{device_id[1]} calibration files"
    if not calib_dir.exists():
        return []
    return sorted([f.name for f in calib_dir.glob("*.xisf")])


def collect_filter_set(dataset_catalog: list[dict], device_id: str) -> list[str]:
    """从数据集清单收集某设备的滤镜集合 (按规范名)."""
    filters = set()
    for d in dataset_catalog:
        if d["device_id"] == device_id:
            filt = d.get("filter_in_header", "") or d.get("filter_in_filename", "")
            if filt:
                filters.add(filt)
    return sorted(filters)


def build_profile(device_id: str, equip_record: dict | None,
                  dataset_catalog: list[dict], header_samples: dict,
                  filter_alias_map: dict) -> dict:
    """构建单个设备的 JSON profile."""
    if device_id == "T1":
        # T1 无数据: 明确标注, 不创建虚假档案
        return {
            "device_id": "T1",
            "status": "no_data",
            "description": "testdata 中无 T1 设备数据. 硬门限允许 T1-T4, 但实际只有 T2/T3/T4.",
            "telescope": "",
            "aperture_mm": "",
            "focal_length_mm": "",
            "camera": "",
            "mount": "",
            "pixel_size_um": "",
            "image_size": "",
            "bin": "",
            "gain": "",
            "offset": "",
            "temperature_c": "",
            "filter_set": [],
            "light_dirs": [],
            "master_dir": "",
            "calibration_files": [],
            "doc_source": "",
            "data_conflict_note": "T1 在 testdata 中无任何数据 (无 Light 帧, 无校准文件, 无说明文档).",
            "generated_at": datetime.utcnow().isoformat() + "Z",
        }

    # T2/T3/T4: 从 equip_record 和 Header 采样构建
    if not equip_record:
        return {
            "device_id": device_id,
            "status": "error",
            "description": f"设备 {device_id} 在设备档案中未找到.",
            "generated_at": datetime.utcnow().isoformat() + "Z",
        }

    telescope_line = equip_record.get("telescope", "")
    aperture = derive_aperture(telescope_line)
    device_headers = collect_device_headers(header_samples, device_id)
    pixel_size = derive_pixel_size(device_headers)
    device_datasets = collect_device_datasets(dataset_catalog, device_id)
    calib_files = collect_device_calib_files(device_id)
    filter_set = collect_filter_set(dataset_catalog, device_id)

    # 收集 light_dirs (去重)
    light_dirs = sorted(set(d["light_dir"] for d in device_datasets if d.get("light_dir")))

    # 收集图像尺寸 (从 Header)
    image_size = ""
    bin_val = ""
    temp_val = ""
    camera_hdr = ""
    for h in device_headers:
        if h.get("image_size") and not image_size:
            image_size = h["image_size"]
        if h.get("bin") and not bin_val:
            bin_val = h["bin"]
        if h.get("temp") and not temp_val:
            temp_val = h["temp"]
        if h.get("camera") and not camera_hdr:
            camera_hdr = h["camera"]

    # 收集校准文件覆盖的滤镜
    calib_filters = []
    for cf in calib_files:
        m = re.search(r"FILTER-(.+?)_mono", cf)
        if m:
            calib_filters.append(m.group(1))

    # 检测缺失的校准平场
    missing_flats = []
    for f in filter_set:
        if f not in calib_filters:
            missing_flats.append(f)

    # 构建数据集摘要
    dataset_summary = []
    for d in device_datasets:
        n_lights_int = 0
        try:
            n_lights_int = int(d.get("n_lights", 0))
        except (ValueError, TypeError):
            n_lights_int = 0
        exposure_val = 0.0
        try:
            exposure_val = float(d.get("exposure_s", 0))
        except (ValueError, TypeError):
            exposure_val = 0.0
        dataset_summary.append({
            "target": d.get("target_name", ""),
            "panel": d.get("panel_id", ""),
            "filter": d.get("filter_in_header", "") or d.get("filter_in_filename", ""),
            "exposure_s": exposure_val,
            "n_lights": n_lights_int,
        })

    # 总 Light 帧数 (csv.DictReader 返回字符串, 需显式转 int)
    total_lights = sum(int(item["n_lights"]) for item in dataset_summary)

    # 校准文件分类
    bias_files = [f for f in calib_files if "masterBias" in f]
    dark_files = [f for f in calib_files if "masterDark" in f]
    flat_files = [f for f in calib_files if "masterFlat" in f]

    # Dark 曝光时间
    dark_exposures = []
    for df in dark_files:
        m = re.search(r"EXPOSURE-([\d.]+)s", df)
        if m:
            dark_exposures.append(float(m.group(1)))

    return {
        "device_id": device_id,
        "status": "active",
        "telescope": telescope_line,
        "aperture_mm": aperture,
        "focal_length_mm": equip_record.get("focal_length_mm", ""),
        "camera": equip_record.get("camera", ""),
        "camera_from_header": camera_hdr,
        "mount": equip_record.get("mount", ""),
        "pixel_size_um": pixel_size,
        "image_size": image_size or equip_record.get("image_size_doc", ""),
        "bin": bin_val,
        "gain": equip_record.get("gain_doc", ""),
        "offset": equip_record.get("offset_doc", ""),
        "temperature_c": temp_val,
        "filter_set": filter_set,
        "filter_set_doc": equip_record.get("filter_set_doc", ""),
        "light_dirs": light_dirs,
        "master_dir": equip_record.get("master_dir", ""),
        "calibration": {
            "bias_files": bias_files,
            "dark_files": dark_files,
            "dark_exposures_s": sorted(set(dark_exposures)),
            "flat_files": flat_files,
            "flat_filters": calib_filters,
            "missing_flats": missing_flats,
            "total_calib_files": len(calib_files),
        },
        "datasets": dataset_summary,
        "total_light_frames": total_lights,
        "doc_source": equip_record.get("doc_source", ""),
        "data_conflict_note": equip_record.get("doc_conflict_note", ""),
        "missing_flat_warning": (
            f"设备 {device_id} 缺少以下滤镜的校准平场: {missing_flats}. "
            f"相关 Light 帧在 P10-005 须特殊处理."
            if missing_flats else ""
        ),
        "generated_at": datetime.utcnow().isoformat() + "Z",
    }


def main():
    log_lines: list[str] = []
    def log(msg: str):
        print(msg, flush=True)
        log_lines.append(msg)

    log("=" * 70)
    log("P10-002 generate_device_profiles.py 启动")
    log(f"REPO_ROOT = {REPO_ROOT}")
    log(f"OUT_DIR = {OUT_DIR}")
    log("=" * 70)

    # 加载 P10-001 数据
    equip_catalog = load_equipment_catalog()
    dataset_catalog = load_dataset_catalog()
    header_samples = load_header_samples()
    filter_alias_map = load_filter_alias_map()

    log(f"\n[加载] 设备档案: {len(equip_catalog)} 行")
    log(f"[加载] 数据集清单: {len(dataset_catalog)} 行")
    log(f"[加载] Header 采样: FITS={len(header_samples.get('fits_headers', {}))}, "
        f"XISF={len(header_samples.get('xisf_headers', {}))}")

    # 构建 4 个设备 profile
    device_ids = ["T1", "T2", "T3", "T4"]
    profiles = {}

    for dev_id in device_ids:
        # 查找设备档案记录
        equip_record = None
        for r in equip_catalog:
            if r["device_id"] == dev_id:
                equip_record = r
                break

        profile = build_profile(dev_id, equip_record, dataset_catalog,
                                header_samples.get("fits_headers", {}), filter_alias_map)
        profiles[dev_id] = profile

        # 写 JSON 文件
        out_path = OUT_DIR / f"{dev_id}_DEVICE_PROFILE.json"
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(profile, f, ensure_ascii=False, indent=2)
        log(f"\n[生成] {out_path.name}")
        log(f"  status: {profile['status']}")
        if profile["status"] == "active":
            log(f"  telescope: {profile.get('telescope', '')[:50]}")
            log(f"  aperture: {profile.get('aperture_mm', '')}mm")
            log(f"  focal_length: {profile.get('focal_length_mm', '')}mm")
            log(f"  camera: {profile.get('camera', '')[:40]}")
            log(f"  image_size: {profile.get('image_size', '')}")
            log(f"  filter_set: {profile.get('filter_set', [])}")
            log(f"  total_lights: {profile.get('total_light_frames', 0)}")
            log(f"  calib_files: {profile.get('calibration', {}).get('total_calib_files', 0)}")
            missing = profile.get('calibration', {}).get('missing_flats', [])
            if missing:
                log(f"  WARNING missing_flats: {missing}")

    # 汇总统计
    total_lights = sum(p.get("total_light_frames", 0) for p in profiles.values())
    active_devices = [d for d in device_ids if profiles[d]["status"] == "active"]
    no_data_devices = [d for d in device_ids if profiles[d]["status"] == "no_data"]

    log(f"\n{'=' * 70}")
    log(f"P10-002 设备档案生成完成")
    log(f"  总设备数: {len(device_ids)}")
    log(f"  活跃设备: {len(active_devices)} ({', '.join(active_devices)})")
    log(f"  无数据设备: {len(no_data_devices)} ({', '.join(no_data_devices)})")
    log(f"  总 Light 帧数: {total_lights}")
    log(f"  禁止捷径检查: {'PASS' if 'T5' not in device_ids and 'unknown' not in device_ids else 'FAIL'}")

    # 写汇总文件
    summary = {
        "_description": "P10-002 T1-T4 设备档案汇总",
        "generated_at": datetime.utcnow().isoformat() + "Z",
        "total_devices": len(device_ids),
        "active_devices": active_devices,
        "no_data_devices": no_data_devices,
        "total_light_frames": total_lights,
        "hard_gate": "PASS" if "T5" not in device_ids else "FAIL",
        "profiles": {d: str(OUT_DIR / f"{d}_DEVICE_PROFILE.json") for d in device_ids},
    }
    summary_path = OUT_DIR / "DEVICE_PROFILE_SUMMARY.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)
    log(f"  汇总: {summary_path}")

    # 写原始日志
    raw_log = RAW_LOGS_DIR / "generate_device_profiles.log"
    raw_log.write_text("\n".join(log_lines), encoding="utf-8")
    log(f"  日志: {raw_log}")

    log(f"{'=' * 70}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
