#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""B-001 自检: JSON 格式 + 路径存在性 + CSV 完整性."""
import json
import os
import csv

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
B001 = os.path.join(ROOT, "engineering_authoritative", "evidence", "B-001")

print("=" * 60)
print("B-001 自检")
print("=" * 60)

# 1. JSON 配置自检
print("\n[1] Stage1 配置 JSON 格式 + 路径存在性")
configs = ["stage1_config_T2_Red.json", "stage1_config_T3_Red.json", "stage1_config_T4_Red.json"]
all_ok = True
for c in configs:
    p = os.path.join(B001, "configs", c)
    with open(p, "r", encoding="utf-8") as f:
        d = json.load(f)
    fid = d["frame"]["id"]
    filt = d["frame"]["filter"]
    print(f"  [OK] {c} JSON 解析成功 (frame.id={fid}, filter={filt})")
    for key in ("light_path",):
        rel = d["frame"][key]
        full = os.path.join(ROOT, rel)
        ok = os.path.exists(full)
        print(f"    [{'OK' if ok else 'MISSING'}] {key}: {rel}")
        if not ok:
            all_ok = False
    for key in ("master_bias_path", "master_dark_path", "master_flat_path"):
        rel = d["calibration"][key]
        full = os.path.join(ROOT, rel)
        ok = os.path.exists(full)
        print(f"    [{'OK' if ok else 'MISSING'}] {key}: {rel}")
        if not ok:
            all_ok = False

# 2. REPRESENTATIVE_FRAMES.csv 自检
print("\n[2] REPRESENTATIVE_FRAMES.csv")
csv_path = os.path.join(B001, "REPRESENTATIVE_FRAMES.csv")
with open(csv_path, "r", encoding="utf-8-sig") as f:
    rows = list(csv.DictReader(f))
print(f"  行数: {len(rows)} (预期 3)")
for r in rows:
    print(f"  [OK] {r['frame_id']}: {r['device_id']} {r['filter']} {r['exposure_s']}s status={r['a004_resolution_status']}")

# 3. A-004 验证结果 CSV 自检
print("\n[3] A-004 验证结果 representative_resolution.csv")
res_csv = os.path.join(B001, "results", "representative_resolution.csv")
with open(res_csv, "r", encoding="utf-8-sig") as f:
    rrows = list(csv.DictReader(f))
print(f"  行数: {len(rrows)} (预期 3)")
for r in rrows:
    print(f"  [OK] {r['frame_id']}: {r['resolution_status']} bias={r['bias_status']} dark={r['dark_status']} flat={r['flat_status']}")

# 4. 曝光时间匹配 Dark 自检
print("\n[4] 曝光时间匹配 Dark 自检")
for c in configs:
    p = os.path.join(B001, "configs", c)
    with open(p, "r", encoding="utf-8") as f:
        d = json.load(f)
    exp = d["frame"]["exposure_s"]
    dark = d["calibration"]["master_dark_path"]
    dark_exp_str = os.path.basename(dark)
    # 从文件名提取 EXPOSURE-XXX.00s
    import re
    m = re.search(r"EXPOSURE-([\d.]+)s", dark_exp_str)
    dark_exp = float(m.group(1)) if m else 0
    match = abs(exp - dark_exp) < 0.01
    print(f"  [{'OK' if match else 'FAIL'}] {d['frame']['id']}: Light={exp}s Dark={dark_exp}s")

print("\n" + "=" * 60)
print(f"自检结果: {'全部通过' if all_ok else '存在问题'}")
print("=" * 60)
