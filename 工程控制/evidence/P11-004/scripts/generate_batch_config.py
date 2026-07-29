#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P11-004 v3.1 — 从 REPRESENTATIVE_FRAMES_ARCHIVE.json 生成批量诊断配置

生成 JSON 配置供 wcs_closure_diagnostic_v3.py --batch 使用,
每帧含 fits/output_subdir/focal_length_mm/pixel_size_um 字段。

用法:
    python generate_batch_config.py
"""
import json
import os
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[4]  # up from evidence/P11-004/scripts/
ARCHIVE_PATH = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P11-003" / "REPRESENTATIVE_FRAMES_ARCHIVE.json"
OUTPUT_PATH = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P11-004" / "scripts" / "batch_frames.json"


def main():
    with open(ARCHIVE_PATH, "r", encoding="utf-8") as f:
        archive = json.load(f)

    frames = []
    for rf in archive["representative_frames"]:
        light_path = rf["light_path"]
        # 转为绝对路径
        abs_path = PROJECT_ROOT / light_path
        if not abs_path.exists():
            print(f"WARN: frame not found: {abs_path}")
            continue
        frame = {
            "fits": str(abs_path).replace("\\", "/"),
            "output_subdir": rf["frame_id"],
            "focal_length_mm": float(rf["focal_length_mm"]),
            "pixel_size_um": float(rf["pixel_size_um"]),
        }
        frames.append(frame)
        print(f"  {rf['frame_id']}: fl={frame['focal_length_mm']}, ps={frame['pixel_size_um']}")

    config = {
        "config_version": "1.0",
        "generated_from": "REPRESENTATIVE_FRAMES_ARCHIVE.json",
        "n_frames": len(frames),
        "frames": frames,
    }
    with open(OUTPUT_PATH, "w", encoding="utf-8") as f:
        json.dump(config, f, indent=2, ensure_ascii=False)
    print(f"\nGenerated: {OUTPUT_PATH} ({len(frames)} frames)")


if __name__ == "__main__":
    main()
