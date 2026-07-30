#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""B-001: 验证 3 个代表帧 (T2/T3/T4 Red) 的 Bias/Dark/Flat 解析状态。

调用 A-004 light_master_resolver 对每个代表帧进行严格模式解析，
确认全部 RESOLVED 后输出 CSV + JSON 证据。

用法:
  python verify_representative_frames.py [--output-dir DIR]

退出码: 0=全部 RESOLVED, 1=存在 UNRESOLVED, 2=运行错误
"""
from __future__ import annotations

import csv
import json
import os
import sys
from datetime import datetime

# 复用 A-004 解析器
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "..", ".."))
A004_SCRIPTS = os.path.join(PROJECT_ROOT, "engineering_authoritative", "evidence", "A-004", "scripts")
sys.path.insert(0, A004_SCRIPTS)

from light_master_resolver import (  # noqa: E402
    resolve_light,
    load_master_inventory,
    load_filter_map,
    setup_logger,
    result_to_dict,
)

# ---------------------------------------------------------------------------
# 代表帧定义 (B-001 任务: 每设备选 1 帧 Red, 避开 UNRESOLVED 的 Lum)
# ---------------------------------------------------------------------------
REPRESENTATIVE_FRAMES = [
    {
        "frame_id": "T2_RED_LDN43",
        "device_id": "T2",
        "target": "LDN43",
        "filter": "Red",
        "exposure_s": 1200,
        "light_path": os.path.join(
            PROJECT_ROOT,
            "testdata", "LDN43_T2素材_flying_dutchman", "lights",
            "LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts",
        ),
        "selection_reason": "LDN43 暗星云, 1200s 曝光, A-004 自测用例 #1 已验证 RESOLVED; T2 有完整 Red 校准链",
    },
    {
        "frame_id": "T3_RED_NGC55",
        "device_id": "T3",
        "target": "NGC55",
        "filter": "Red",
        "exposure_s": 600,
        "light_path": os.path.join(
            PROJECT_ROOT,
            "testdata", "NGC55_T3_flying_dutchman", "lights",
            "NGC55_T3_flying_dutchman-20250703@080546-600S-Red.fts",
        ),
        "selection_reason": "NGC55 星系, 600s 曝光, 与 A-004 自测 T3 Lum 帧同一天拍摄; T3 有完整 Red 校准链",
    },
    {
        "frame_id": "T4_RED_GalaxyCenter_panel1",
        "device_id": "T4",
        "target": "Galaxy_Center",
        "panel": "panel1",
        "filter": "Red",
        "exposure_s": 180,
        "light_path": os.path.join(
            PROJECT_ROOT,
            "testdata", "Galaxy_Center_T4", "lights", "panel1",
            "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts",
        ),
        "selection_reason": "银心 panel1, 180s 曝光, A-004 自测用例 #4 已验证 RESOLVED; Gate F/G 核心数据",
    },
]


def main():
    import argparse
    parser = argparse.ArgumentParser(description="B-001 代表帧 A-004 解析验证")
    parser.add_argument(
        "--output-dir",
        default=os.path.join(SCRIPT_DIR, "..", "results"),
        help="输出目录",
    )
    args = parser.parse_args()
    output_dir = os.path.abspath(args.output_dir)
    os.makedirs(output_dir, exist_ok=True)

    log_file = os.path.join(output_dir, f"b001_verify_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log")
    logger = setup_logger(log_file=log_file)
    logger.info("=" * 70)
    logger.info("B-001 代表帧 A-004 解析验证启动")
    logger.info("  代表帧数量: %d", len(REPRESENTATIVE_FRAMES))
    logger.info("  输出目录: %s", output_dir)
    logger.info("  日志文件: %s", log_file)

    # 加载 A-002 证据
    masters = load_master_inventory(logger)
    filter_map = load_filter_map(logger)

    # 文件存在性检查
    logger.info("=" * 70)
    logger.info("步骤 1: 文件存在性检查")
    all_exist = True
    for f in REPRESENTATIVE_FRAMES:
        exists = os.path.exists(f["light_path"])
        tag = "OK" if exists else "MISSING"
        logger.info("  [%s] %s -> %s", tag, f["frame_id"], f["light_path"])
        if not exists:
            all_exist = False
    if not all_exist:
        logger.error("存在缺失文件, 终止")
        return 2

    # A-004 严格模式解析
    logger.info("=" * 70)
    logger.info("步骤 2: A-004 严格模式解析 (strict=True)")
    results = []
    for f in REPRESENTATIVE_FRAMES:
        logger.info("-" * 60)
        logger.info("解析: %s (%s %s %ss)", f["frame_id"], f["device_id"], f["filter"], f["exposure_s"])
        r = resolve_light(f["light_path"], masters, filter_map, logger, strict=True)
        # 附加 B-001 元数据
        d = result_to_dict(r)
        d["frame_id"] = f["frame_id"]
        d["target"] = f["target"]
        d["panel"] = f.get("panel", "")
        d["selection_reason"] = f["selection_reason"]
        results.append(d)

    # 输出 JSON
    json_path = os.path.join(output_dir, "representative_resolution.json")
    with open(json_path, "w", encoding="utf-8") as fh:
        json.dump(results, fh, indent=2, ensure_ascii=False, default=str)
    logger.info("JSON 结果已保存: %s", json_path)

    # 输出 CSV
    csv_path = os.path.join(output_dir, "representative_resolution.csv")
    fields = [
        "frame_id", "device_id", "target", "panel",
        "sensor_size", "binning", "exposure_s",
        "filter_raw", "filter_canonical", "ccd_temp_c",
        "bias_status", "bias_master", "bias_reason",
        "dark_status", "dark_master", "dark_reason",
        "flat_status", "flat_master", "flat_reason",
        "resolution_status", "resolution_note", "missing",
    ]
    with open(csv_path, "w", encoding="utf-8-sig", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(fields)
        for d in results:
            bias = d.get("bias", {})
            dark = d.get("dark", {})
            flat = d.get("flat", {})
            w.writerow([
                d.get("frame_id", ""), d.get("device_id", ""), d.get("target", ""), d.get("panel", ""),
                d.get("sensor_size", ""), d.get("binning", ""), d.get("exposure_s", ""),
                d.get("filter_raw", ""), d.get("filter_canonical", ""), d.get("ccd_temp_c", ""),
                bias.get("status", ""), bias.get("master_id", ""), bias.get("reason", ""),
                dark.get("status", ""), dark.get("master_id", ""), dark.get("reason", ""),
                flat.get("status", ""), flat.get("master_id", ""), flat.get("reason", ""),
                d.get("resolution_status", ""), d.get("resolution_note", ""),
                "|".join(d.get("missing", [])),
            ])
    logger.info("CSV 结果已保存: %s", csv_path)

    # 汇总
    total = len(results)
    resolved = sum(1 for r in results if r.get("resolution_status") == "RESOLVED")
    unresolved = total - resolved
    logger.info("=" * 70)
    logger.info("验证汇总: %d/%d RESOLVED", resolved, total)
    for d in results:
        tag = "RESOLVED" if d.get("resolution_status") == "RESOLVED" else "UNRESOLVED"
        logger.info("  [%s] %s (%s %s): %s", tag, d["frame_id"], d["device_id"], d["filter_canonical"], d["resolution_note"])

    return 0 if unresolved == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
