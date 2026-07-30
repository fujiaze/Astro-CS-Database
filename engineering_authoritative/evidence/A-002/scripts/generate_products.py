#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""A-002: 根据 raw_headers.json 和说明文档生成 5 个产物文件。
产物:
  1. TESTDATA_EQUIPMENT_CATALOG.csv      - 设备档案
  2. FILTER_ALIAS_MAP.json               - 滤镜别名映射
  3. CALIBRATION_MASTER_INVENTORY.csv    - 主校准帧清单
  4. LIGHT_TO_MASTER_RESOLUTION.csv      - Light→Master 解析
  5. UNRESOLVED_CALIBRATION_REPORT.md    - 未解决校准报告
"""
import os, json, csv, glob, re
from collections import OrderedDict

ROOT = r"f:\Astro dev\Astro CS Normalization Database"
OUT_DIR = os.path.join(ROOT, "engineering_authoritative", "evidence", "A-002")

# ============================================================
# 1. 设备档案数据 (来源: 说明文档 + FITS/XISF Header 交叉验证)
# ============================================================
# 格式: 设备ID -> 档案字典
EQUIPMENT = OrderedDict([
    ("T2", {
        "device_id": "T2",
        "telescope": "Chilescope T2 (ASA 500N)",
        "aperture_mm": 500,
        "focal_length_mm_doc": 1900,
        "focal_length_mm_header": "1917.3-1917.8",
        "focal_length_mm_master": 1877,
        "camera_doc": "FLI Proline 16803",
        "camera_header_instrume": "FLI",
        "sensor_size": "4096x4096",
        "pixel_size_um": 9.0,
        "binning": "1x1",
        "gain": "-",
        "offset": "-",
        "ccd_temp_c": -20.0,
        "set_temp_c": -20.0,
        "mount": "DDM85",
        "filter_brand_doc": "Astrodon 50mm",
        "filter_set_doc": "LRGBHaOIII (视数据集而异)",
        "filters_observed_header": ["Blue", "Green", "H-alpha", "Lum", "OIII", "Red"],
        "light_dirs": [
            "testdata/LDN43_T2素材_flying_dutchman/lights",
            "testdata/NGC1727_T2_flying_dutchman/lights",
            "testdata/NGC247_T2_flying_dutchman/lights",
        ],
        "master_dir": "testdata/T2 calibration files",
        "doc_sources": [
            "testdata/LDN43_T2素材_flying_dutchman/素材信息与版权约定.txt",
            "testdata/NGC1727_T2_flying_dutchman/素材信息与版权约定.txt",
            "testdata/NGC247_T2_flying_dutchman/素材信息与版权约定.txt",
        ],
        "conflicts": "文档焦距1900mm, Light Header 1917.3-1917.8mm (对焦微调), Master Header 1877mm; T2无Lum Flat(3个数据集中2个需要Lum)",
    }),
    ("T3", {
        "device_id": "T3",
        "telescope": "Chilescope T3 (ASA 500N)",
        "aperture_mm": 500,
        "focal_length_mm_doc": 1900,
        "focal_length_mm_header": "1877.0-1934.7",
        "focal_length_mm_master": 1877,
        "camera_doc": "FLI Proline 16803",
        "camera_header_instrume": "FLI",
        "sensor_size": "4096x4096",
        "pixel_size_um": 9.0,
        "binning": "1x1",
        "gain": "-",
        "offset": "-",
        "ccd_temp_c": -20.0,
        "set_temp_c": -20.0,
        "mount": "DDM85",
        "filter_brand_doc": "Astrodon 50mm",
        "filter_set_doc": "LRGBHaOIII (视数据集而异)",
        "filters_observed_header": ["Blue", "Green", "H-alpha", "Lum", "Oiii", "Red"],
        "light_dirs": [
            "testdata/NGC55_T3_flying_dutchman/lights",
            "testdata/NGC83_cluster_T3_Flying_Dutchman/lights",
        ],
        "master_dir": "testdata/T3 calibration files",
        "doc_sources": [
            "testdata/NGC55_T3_flying_dutchman/素材信息与版权约定.txt",
            "testdata/NGC83_cluster_T3_Flying_Dutchman/素材信息与版权约定.txt",
        ],
        "conflicts": "文档焦距1900mm, Light Header 1877.0-1934.7mm (NGC55_T3 Lum帧1934.7mm异常, 其他1877mm), Master Header 1877mm; T3无1800s Dark但无1800s Light帧需求",
    }),
    ("T4", {
        "device_id": "T4",
        "telescope": "Chilescope T4 (Nikkor 200F2)",
        "aperture_mm": 100,
        "focal_length_mm_doc": 200,
        "focal_length_mm_header": 200.0,
        "focal_length_mm_master": 200,
        "camera_doc": "FLI Microline 16200 (Galaxy_Center) / FLI Proline 16200 (Victory_Nebula)",
        "camera_header_instrume": "FLI",
        "sensor_size": "4500x3600",
        "pixel_size_um": 6.0,
        "binning": "1x1",
        "gain": "-",
        "offset": "-",
        "ccd_temp_c": -20.0,
        "set_temp_c": -20.0,
        "mount": "10 Micron GM1000HPS",
        "filter_brand_doc": "Baader 50mm",
        "filter_set_doc": "RGBHaOIII (Galaxy_Center) / LRGB (Victory_Nebula)",
        "filters_observed_header": ["Blue", "Green", "H-alpha", "Lum", "Oiii", "Red"],
        "light_dirs": [
            "testdata/Galaxy_Center_T4/lights",
            "testdata/Victory_Nebula_T4_Flying_Dutchman/lights",
        ],
        "master_dir": "testdata/T4 calibration files",
        "doc_sources": [
            "testdata/Galaxy_Center_T4/素材信息.txt",
            "testdata/Victory_Nebula_T4_Flying_Dutchman/素材信息与版权约定.txt",
        ],
        "conflicts": "Galaxy_Center文档写FLI Microline 16200, Victory_Nebula文档写FLI Proline 16200 - 需确认是否同一相机或文档笔误; T4无Lum Flat(Victory_T4需要Lum); T4无1800s/1200s Dark但无此曝光Light帧需求",
    }),
])

# ============================================================
# 2. 滤镜别名映射
# ============================================================
FILTER_ALIAS_MAP = OrderedDict([
    ("Lum", {"canonical": "Lum", "aliases": ["L", "Lum", "Luminance", "Lumination"], "note": "亮度滤镜"}),
    ("Red", {"canonical": "Red", "aliases": ["R", "Red"], "note": "红色滤镜"}),
    ("Green", {"canonical": "Green", "aliases": ["G", "Green"], "note": "绿色滤镜"}),
    ("Blue", {"canonical": "Blue", "aliases": ["B", "Blue"], "note": "蓝色滤镜"}),
    ("H-alpha", {"canonical": "H-alpha", "aliases": ["Ha", "Halpha", "H-alpha", "Hα", "H_a"], "note": "氢Alpha窄带滤镜, 3nm (T2) / 品牌未明确 (T3/T4)"}),
    ("OIII", {"canonical": "OIII", "aliases": ["OIII", "Oiii", "O3", "OⅢ", "O_III"], "note": "氧III窄带滤镜"}),
])

# 滤镜名规范化函数
def normalize_filter(filt_str):
    """将滤镜名规范化为标准名。"""
    if not filt_str:
        return ""
    f = filt_str.strip().strip("'").strip('"')
    # 直接匹配
    for canonical, info in FILTER_ALIAS_MAP.items():
        if f == canonical:
            return canonical
        for alias in info["aliases"]:
            if f == alias:
                return canonical
    # 模糊匹配
    f_lower = f.lower().replace("-", "").replace("_", "").replace(" ", "")
    if f_lower in ("l", "lum", "luminance", "lumination"):
        return "Lum"
    if f_lower in ("r", "red"):
        return "Red"
    if f_lower in ("g", "green"):
        return "Green"
    if f_lower in ("b", "blue"):
        return "Blue"
    if f_lower in ("ha", "halpha", "hα", "hα", "halphа"):
        return "H-alpha"
    if f_lower in ("oiii", "o3", "oiii", "oⅲ"):
        return "OIII"
    return f  # 未知滤镜, 保留原名

# ============================================================
# 3. 数据集信息 (来源: 说明文档)
# ============================================================
DATASETS = OrderedDict([
    ("Galaxy_Center_T4", {
        "device_id": "T4", "object": "Galaxy_Center (银心3片马赛克)",
        "filters": {"Blue": 180, "Green": 180, "H-alpha": 300, "Oiii": 600, "Red": 180},
        "n_lights": 157, "doc_file": "testdata/Galaxy_Center_T4/素材信息.txt",
        "light_dir": "testdata/Galaxy_Center_T4/lights",
    }),
    ("LDN43_T2", {
        "device_id": "T2", "object": "LDN43 飞天蝙蝠星云",
        "filters": {"Blue": 1200, "Green": 1200, "H-alpha": 1200, "Lum": 600, "Red": 1200},
        "n_lights": 42, "doc_file": "testdata/LDN43_T2素材_flying_dutchman/素材信息与版权约定.txt",
        "light_dir": "testdata/LDN43_T2素材_flying_dutchman/lights",
    }),
    ("NGC1727_T2", {
        "device_id": "T2", "object": "NGC1727 宇宙烟花秀",
        "filters": {"Blue": 600, "Green": 600, "H-alpha": 1200, "OIII": 1800, "Red": 600},
        "n_lights": 64, "doc_file": "testdata/NGC1727_T2_flying_dutchman/素材信息与版权约定.txt",
        "light_dir": "testdata/NGC1727_T2_flying_dutchman/lights",
    }),
    ("NGC247_T2", {
        "device_id": "T2", "object": "NGC247 与博比奇链",
        "filters": {"Blue": 600, "Green": 600, "H-alpha": 1200, "Lum": 600, "OIII": 1200, "Red": 600},
        "n_lights": 68, "doc_file": "testdata/NGC247_T2_flying_dutchman/素材信息与版权约定.txt",
        "light_dir": "testdata/NGC247_T2_flying_dutchman/lights",
    }),
    ("NGC55_T3", {
        "device_id": "T3", "object": "NGC55 南鲸鱼星系",
        "filters": {"Blue": 600, "Green": 600, "H-alpha": 1200, "Lum": 600, "Oiii": 1200, "Red": 600},
        "n_lights": 79, "doc_file": "testdata/NGC55_T3_flying_dutchman/素材信息与版权约定.txt",
        "light_dir": "testdata/NGC55_T3_flying_dutchman/lights",
    }),
    ("NGC83_T3", {
        "device_id": "T3", "object": "NGC83 星系群",
        "filters": {"Blue": 600, "Green": 600, "Lum": 600, "Red": 600},
        "n_lights": 72, "doc_file": "testdata/NGC83_cluster_T3_Flying_Dutchman/素材信息与版权约定.txt",
        "light_dir": "testdata/NGC83_cluster_T3_Flying_Dutchman/lights",
    }),
    ("Victory_T4", {
        "device_id": "T4", "object": "胜利星云 (蝘蜓座分子云)",
        "filters": {"Blue": 180, "Green": 180, "Lum": 180, "Red": 180},
        "n_lights": 228, "doc_file": "testdata/Victory_Nebula_T4_Flying_Dutchman/素材信息与版权约定.txt",
        "light_dir": "testdata/Victory_Nebula_T4_Flying_Dutchman/lights",
    }),
])

# ============================================================
# 4. Master 帧清单 (来源: XISF Header + 文件名)
# ============================================================
# 手工整理的 Master 清单 (从 raw_headers.json 提取)
MASTERS = []
# T2 Masters
for dev, mdir, size in [("T2", "T2 calibration files", "4096x4096"),
                          ("T3", "T3 calibration files", "4096x4096"),
                          ("T4", "T4 calibration files", "4500x3600")]:
    mdir_full = os.path.join(ROOT, "testdata", mdir)
    for fp in sorted(glob.glob(os.path.join(mdir_full, "*.xisf"))):
        bname = os.path.basename(fp)
        # 解析文件名
        mtype = "?"
        if "masterBias" in bname:
            mtype = "Bias"
        elif "masterDark" in bname:
            mtype = "Dark"
        elif "masterFlat" in bname:
            mtype = "Flat"

        expt = ""
        filt = ""
        if mtype == "Dark":
            m = re.search(r'EXPOSURE-([\d.]+)s', bname)
            if m:
                expt = float(m.group(1))
        elif mtype == "Flat":
            m = re.search(r'FILTER-([^_]+)_mono', bname)
            if m:
                filt = normalize_filter(m.group(1))

        MASTERS.append({
            "master_id": "{}/{}".format(dev, bname),
            "device_id": dev,
            "master_type": mtype,
            "file_name": bname,
            "file_path": "testdata/{}/{}".format(dev + " calibration files" if dev != "T2" else "T2 calibration files",
                                                  bname).replace("T2 calibration files", "T2 calibration files"),
            "sensor_size": size,
            "binning": "1x1",
            "exposure_s": expt if expt != "" else "",
            "filter_canonical": filt if filt else "",
            "ccd_temp_c": -20.0,  # Master XISF Header 无温度字段, 根据 Light 帧惯例推断
            "imagetyp": "Master " + mtype,
            "match_key": "{}|{}|{}|{}|{}|{}".format(
                dev, size, "1x1", mtype,
                expt if expt != "" else "-",
                filt if filt else "-"),
        })

# 修正 file_path
for m in MASTERS:
    dev = m["device_id"]
    # 标准化路径
    if dev == "T2":
        m["file_path"] = "testdata/T2 calibration files/{}".format(m["file_name"])
    elif dev == "T3":
        m["file_path"] = "testdata/T3 calibration files/{}".format(m["file_name"])
    elif dev == "T4":
        m["file_path"] = "testdata/T4 calibration files/{}".format(m["file_name"])

# ============================================================
# 5. Light → Master 解析
# ============================================================
def find_master(device_id, sensor_size, mtype, expt_s, filt_canonical, allow_cross_device=False):
    """查找匹配的 Master 帧。
    匹配优先级: 设备ID + 尺寸 + Bin + 类型 + (曝光/滤镜)
    allow_cross_device: 若为True, 允许T2/T3跨设备匹配(仅限相同传感器)
    """
    # 第一轮: 严格匹配设备ID
    candidates = []
    for m in MASTERS:
        if m["device_id"] != device_id:
            continue
        if m["sensor_size"] != sensor_size:
            continue
        if m["master_type"] != mtype:
            continue
        if mtype == "Dark":
            if m["exposure_s"] != "" and expt_s and abs(float(m["exposure_s"]) - expt_s) < 0.5:
                candidates.append(m)
        elif mtype == "Flat":
            if m["filter_canonical"] == filt_canonical:
                candidates.append(m)
        elif mtype == "Bias":
            candidates.append(m)  # Bias 只匹配设备+尺寸+Bin

    if candidates:
        return candidates[0], "exact"

    # 第二轮: 跨设备匹配 (仅 T2<->T3, 相同传感器 4096x4096 9um)
    if allow_cross_device and sensor_size == "4096x4096":
        cross_dev = "T3" if device_id == "T2" else "T2" if device_id == "T3" else None
        if cross_dev:
            for m in MASTERS:
                if m["device_id"] != cross_dev:
                    continue
                if m["sensor_size"] != sensor_size:
                    continue
                if m["master_type"] != mtype:
                    continue
                if mtype == "Dark":
                    if m["exposure_s"] != "" and expt_s and abs(float(m["exposure_s"]) - expt_s) < 0.5:
                        return m, "cross_device(T2<->T3)"
                elif mtype == "Flat":
                    if m["filter_canonical"] == filt_canonical:
                        return m, "cross_device(T2<->T3)"
                elif mtype == "Bias":
                    return m, "cross_device(T2<->T3)"

    return None, "not_found"


def resolve_light_masters(dataset_name, device_id, sensor_size, filt_canonical, expt_s):
    """为单个数据集的某个滤镜组解析 Bias/Dark/Flat。"""
    results = {}

    # Bias - 允许跨设备匹配 (相同传感器, Bias只含读出噪声, 与光路无关)
    bias, bias_reason = find_master(device_id, sensor_size, "Bias", None, None, allow_cross_device=True)
    results["bias"] = bias
    results["bias_reason"] = bias_reason

    # Dark - 允许跨设备匹配 (相同传感器+曝光, Dark含热噪声, 与光路无关)
    dark, dark_reason = find_master(device_id, sensor_size, "Dark", expt_s, None, allow_cross_device=True)
    results["dark"] = dark
    results["dark_reason"] = dark_reason

    # Flat - 不允许跨设备匹配 (Flat含光学illumination pattern, 不同望远镜光路不同)
    flat, flat_reason = find_master(device_id, sensor_size, "Flat", None, filt_canonical, allow_cross_device=False)
    results["flat"] = flat
    results["flat_reason"] = flat_reason

    return results


# ============================================================
# 生成产物文件
# ============================================================
def gen_equipment_catalog():
    """1. TESTDATA_EQUIPMENT_CATALOG.csv"""
    path = os.path.join(OUT_DIR, "TESTDATA_EQUIPMENT_CATALOG.csv")
    fields = ["device_id", "telescope", "aperture_mm", "focal_length_mm_doc",
              "focal_length_mm_header", "focal_length_mm_master", "camera_doc",
              "camera_header_instrume", "sensor_size", "pixel_size_um", "binning",
              "gain", "offset", "ccd_temp_c", "set_temp_c", "mount",
              "filter_brand_doc", "filter_set_doc", "filters_observed_header",
              "light_dirs", "master_dir", "doc_sources", "conflicts"]
    with open(path, 'w', encoding='utf-8-sig', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for dev_id, e in EQUIPMENT.items():
            row = {
                "device_id": e["device_id"],
                "telescope": e["telescope"],
                "aperture_mm": e["aperture_mm"],
                "focal_length_mm_doc": e["focal_length_mm_doc"],
                "focal_length_mm_header": e["focal_length_mm_header"],
                "focal_length_mm_master": e["focal_length_mm_master"],
                "camera_doc": e["camera_doc"],
                "camera_header_instrume": e["camera_header_instrume"],
                "sensor_size": e["sensor_size"],
                "pixel_size_um": e["pixel_size_um"],
                "binning": e["binning"],
                "gain": e["gain"],
                "offset": e["offset"],
                "ccd_temp_c": e["ccd_temp_c"],
                "set_temp_c": e["set_temp_c"],
                "mount": e["mount"],
                "filter_brand_doc": e["filter_brand_doc"],
                "filter_set_doc": e["filter_set_doc"],
                "filters_observed_header": ";".join(e["filters_observed_header"]),
                "light_dirs": ";".join(e["light_dirs"]),
                "master_dir": e["master_dir"],
                "doc_sources": ";".join(e["doc_sources"]),
                "conflicts": e["conflicts"],
            }
            w.writerow(row)
    print("written: {}".format(path))
    return path


def gen_filter_alias_map():
    """2. FILTER_ALIAS_MAP.json"""
    path = os.path.join(OUT_DIR, "FILTER_ALIAS_MAP.json")
    out = OrderedDict()
    for canonical, info in FILTER_ALIAS_MAP.items():
        out[canonical] = {
            "canonical_name": info["canonical"],
            "aliases": info["aliases"],
            "note": info["note"],
        }
    # 添加观测到的变体
    out["_observed_variants"] = {
        "T2_Light": ["Blue", "Green", "H-alpha", "Lum", "OIII", "Red"],
        "T3_Light": ["Blue", "Green", "H-alpha", "Lum", "Oiii", "Red"],
        "T4_Light": ["Blue", "Green", "H-alpha", "Lum", "Oiii", "Red"],
        "T2_MasterFlat": ["Blue", "Green", "H-alpha", "OIII", "Red"],
        "T3_MasterFlat": ["Blue", "Green", "H-alpha", "Lum", "Oiii", "Red"],
        "T4_MasterFlat": ["Blue", "Green", "H-alpha", "Oiii", "Red"],
    }
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
    print("written: {}".format(path))
    return path


def gen_master_inventory():
    """3. CALIBRATION_MASTER_INVENTORY.csv"""
    path = os.path.join(OUT_DIR, "CALIBRATION_MASTER_INVENTORY.csv")
    fields = ["master_id", "device_id", "master_type", "file_name", "file_path",
              "sensor_size", "binning", "exposure_s", "filter_canonical",
              "ccd_temp_c", "imagetyp", "match_key"]
    with open(path, 'w', encoding='utf-8-sig', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for m in MASTERS:
            w.writerow(m)
    print("written: {}".format(path))
    return path


def gen_light_to_master_resolution():
    """4. LIGHT_TO_MASTER_RESOLUTION.csv"""
    path = os.path.join(OUT_DIR, "LIGHT_TO_MASTER_RESOLUTION.csv")
    fields = ["dataset", "device_id", "sensor_size", "filter_raw", "filter_canonical",
              "exposure_s", "n_lights", "bias_master", "bias_reason",
              "dark_master", "dark_reason", "flat_master", "flat_reason",
              "resolution_status", "resolution_note"]

    rows = []
    unresolved = []

    for ds_name, ds in DATASETS.items():
        dev_id = ds["device_id"]
        # 从设备档案获取传感器尺寸
        sensor_size = EQUIPMENT[dev_id]["sensor_size"]
        n_lights = ds["n_lights"]

        for filt_raw, expt_s in sorted(ds["filters"].items()):
            filt_canonical = normalize_filter(filt_raw)
            res = resolve_light_masters(ds_name, dev_id, sensor_size, filt_canonical, expt_s)

            bias_m = res["bias"]["master_id"] if res["bias"] else ""
            dark_m = res["dark"]["master_id"] if res["dark"] else ""
            flat_m = res["flat"]["master_id"] if res["flat"] else ""

            # 统计该滤镜的 Light 帧数 (从代表帧目录估算)
            # 注意: n_lights 是数据集总数, 这里用滤镜数均分作为近似
            # 实际帧数需要扫描目录, 但对于解析表来说, 只需要确认有对应 Master
            n_filt_lights = ""  # 留空, 实际帧数在数据集层统计

            # 判断解析状态
            missing = []
            if not res["bias"]:
                missing.append("Bias")
            if not res["dark"]:
                missing.append("Dark")
            if not res["flat"]:
                missing.append("Flat")

            if not missing:
                status = "RESOLVED"
                note = "全部校准帧匹配"
            else:
                status = "UNRESOLVED"
                note = "缺失: " + ",".join(missing)
                unresolved.append({
                    "dataset": ds_name, "device_id": dev_id,
                    "filter": filt_canonical, "exposure_s": expt_s,
                    "missing": missing,
                    "bias_found": bias_m, "dark_found": dark_m, "flat_found": flat_m,
                })

            # 添加跨设备匹配说明
            reasons = []
            if res["bias_reason"] == "cross_device(T2<->T3)":
                reasons.append("Bias跨设备匹配")
            if res["dark_reason"] == "cross_device(T2<->T3)":
                reasons.append("Dark跨设备匹配")
            if res["flat_reason"] == "cross_device(T2<->T3)":
                reasons.append("Flat跨设备匹配")
            if reasons:
                note += "; " + ";".join(reasons)

            rows.append({
                "dataset": ds_name,
                "device_id": dev_id,
                "sensor_size": sensor_size,
                "filter_raw": filt_raw,
                "filter_canonical": filt_canonical,
                "exposure_s": expt_s,
                "n_lights": n_filt_lights,
                "bias_master": bias_m,
                "bias_reason": res["bias_reason"],
                "dark_master": dark_m,
                "dark_reason": res["dark_reason"],
                "flat_master": flat_m,
                "flat_reason": res["flat_reason"],
                "resolution_status": status,
                "resolution_note": note,
            })

    with open(path, 'w', encoding='utf-8-sig', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for r in rows:
            w.writerow(r)
    print("written: {}".format(path))
    return path, rows, unresolved


def gen_unresolved_report(rows, unresolved):
    """5. UNRESOLVED_CALIBRATION_REPORT.md"""
    path = os.path.join(OUT_DIR, "UNRESOLVED_CALIBRATION_REPORT.md")

    total = len(rows)
    resolved = sum(1 for r in rows if r["resolution_status"] == "RESOLVED")
    unresolved_count = total - resolved

    lines = []
    lines.append("# A-002 未解决校准报告")
    lines.append("")
    lines.append("**生成时间**: 2026-07-30")
    lines.append("**任务**: A-002 整理 T1-T4 设备与说明文档目录")
    lines.append("")
    lines.append("## 1. 总览")
    lines.append("")
    lines.append("| 指标 | 值 |")
    lines.append("|------|-----|")
    lines.append("| 数据集总数 | 7 |")
    lines.append("| Light 滤镜组合总数 | {} |".format(total))
    lines.append("| 已解析 (RESOLVED) | {} |".format(resolved))
    lines.append("| 未解析 (UNRESOLVED) | {} |".format(unresolved_count))
    lines.append("| 覆盖率 | {:.1f}% |".format(100.0 * resolved / total if total > 0 else 0))
    lines.append("")
    lines.append("## 2. 未解决问题清单")
    lines.append("")

    if not unresolved:
        lines.append("无未解决问题。")
    else:
        lines.append("### 2.1 缺失 Master Flat")
        lines.append("")
        lines.append("以下数据集的 Light 帧缺少对应滤镜的 Master Flat:")
        lines.append("")
        lines.append("| 数据集 | 设备 | 滤镜 | 曝光(s) | 缺失项 | Bias | Dark | 建议方案 |")
        lines.append("|--------|------|------|---------|--------|------|------|----------|")
        for u in unresolved:
            # 查找建议方案
            if u["device_id"] == "T2" and u["filter"] == "Lum":
                suggestion = "T2无Lum Flat. T3有Lum Flat(相同设备ASA 500N+FLI 16803, 4096x4096, 9um). 可考虑跨设备使用T3 Lum Flat, 但Flat包含光学illumination pattern, 需用户确认是否接受. 或补充拍摄T2 Lum Flat."
            elif u["device_id"] == "T4" and u["filter"] == "Lum":
                suggestion = "T4无Lum Flat. T4是唯一设备(Nikkor 200F2+FLI 16200, 4500x3600), 无法跨设备替代. 可考虑用Red Flat近似(不推荐, 光谱响应不同), 或补充拍摄T4 Lum Flat. 需用户确认."
            else:
                suggestion = "需进一步排查"

            lines.append("| {} | {} | {} | {} | {} | {} | {} | {} |".format(
                u["dataset"], u["device_id"], u["filter"], u["exposure_s"],
                ",".join(u["missing"]),
                u["bias_found"] if u["bias_found"] else "OK",
                u["dark_found"] if u["dark_found"] else "OK",
                suggestion))
        lines.append("")

    lines.append("### 2.2 文档与 Header 冲突")
    lines.append("")
    lines.append("| 项目 | 说明 | 严重程度 |")
    lines.append("|------|------|----------|")
    lines.append("| T2 焦距不一致 | 文档1900mm, Light Header 1917.3-1917.8mm, Master Header 1877mm | 低 (对焦微调正常, 不影响校准) |")
    lines.append("| T3 焦距不一致 | 文档1900mm, Light Header 1877.0-1934.7mm (NGC55_T3 Lum帧1934.7mm异常), Master Header 1877mm | 中 (NGC55_T3 Lum帧焦距异常, 需确认) |")
    lines.append("| T4 相机型号不一致 | Galaxy_Center文档写FLI Microline 16200, Victory_Nebula文档写FLI Proline 16200 | 中 (需确认是否同一相机或文档笔误, Header INSTRUME均为FLI无法区分) |")
    lines.append("| T3 无1800s Dark | T3 Master Dark仅有600s/1200s, 无1800s | 无 (T3数据集无1800s曝光Light帧, 不影响) |")
    lines.append("| T4 无1200s/1800s Dark | T4 Master Dark仅有180s/300s/600s | 无 (T4数据集无1200s/1800s曝光Light帧, 不影响) |")
    lines.append("| 滤镜名变体 | OIII(T2) vs Oiii(T3/T4); H-alpha vs Ha vs Halpha | 低 (已通过FILTER_ALIAS_MAP规范化) |")
    lines.append("| Master XISF无温度字段 | T2/T3/T4 Master XISF Header中无CCD-TEMP/SET-TEMP | 低 (根据Light帧惯例推断-20°C, 所有Light帧SET-TEMP=-20) |")
    lines.append("| Master XISF无Gain/Offset | T2/T3/T4 Master XISF Header中无GAIN/OFFSET | 低 (Light帧Header也无此字段, FLI相机可能在固件层固定) |")
    lines.append("")

    lines.append("## 3. 建议处理优先级")
    lines.append("")
    lines.append("1. **高优先级**: T2 Lum Flat 缺失 — 影响 LDN43_T2 和 NGC247_T2 共 2 个数据集的 Lum 通道校准")
    lines.append("2. **高优先级**: T4 Lum Flat 缺失 — 影响 Victory_T4 (228帧, 最大数据集) 的 Lum 通道校准")
    lines.append("3. **中优先级**: NGC55_T3 Lum 帧焦距异常 (1934.7mm vs 其他滤镜 1877mm) — 需确认是否影响板解算")
    lines.append("4. **中优先级**: Galaxy_Center_T4 相机型号文档不一致 — 需确认 Microline vs Proline")
    lines.append("5. **低优先级**: 滤镜名变体 — 已通过 FILTER_ALIAS_MAP.json 规范化, 无实际影响")
    lines.append("")

    lines.append("## 4. 待用户确认事项")
    lines.append("")
    lines.append("1. T2 Lum Flat: 是否接受使用 T3 Lum Flat (相同设备型号, 但不同望远镜实例) 作为替代?")
    lines.append("2. T4 Lum Flat: 是否补充拍摄? 或接受使用 Red Flat 近似 (不推荐)?")
    lines.append("3. Galaxy_Center_T4 相机: FLI Microline 16200 还是 FLI Proline 16200? (影响设备档案准确性)")
    lines.append("4. NGC55_T3 Lum 帧焦距 1934.7mm: 是否为对焦调整? 需要在板解算中特殊处理吗?")
    lines.append("")

    lines.append("## 5. 结论")
    lines.append("")
    lines.append("根据 README 5.3 节, 用户已确认 TestData 配齐主校准帧。当前发现的 T2/T4 Lum Flat 缺失问题, ")
    lines.append("可能是:(a) 别名问题 (Lum 在文件名中用了其他名称); (b) 文件扫描问题 (位于其他目录); ")
    lines.append("(c) 需要跨设备匹配 (T2->T3); (d) 真正需要补充拍摄。")
    lines.append("")
    lines.append("建议用户提供以下信息以完成最终解析:")
    lines.append("- T2 是否有 Lum Flat 文件 (可能在其他目录或使用不同命名)?")
    lines.append("- T4 是否有 Lum Flat 文件?")
    lines.append("- 若无, 是否授权跨设备使用 (T2->T3) 或近似使用 (T4 Lum->Red)?")

    with open(path, 'w', encoding='utf-8') as f:
        f.write("\n".join(lines))
    print("written: {}".format(path))
    return path


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    p1 = gen_equipment_catalog()
    p2 = gen_filter_alias_map()
    p3 = gen_master_inventory()
    p4, rows, unresolved = gen_light_to_master_resolution()
    p5 = gen_unresolved_report(rows, unresolved)

    print()
    print("=== 生成完成 ===")
    print("产物文件:")
    for p in [p1, p2, p3, p4, p5]:
        print("  {}".format(p))
    print()
    total = len(rows)
    resolved = sum(1 for r in rows if r["resolution_status"] == "RESOLVED")
    print("Light→Master 解析覆盖率: {}/{} = {:.1f}%".format(resolved, total, 100.0 * resolved / total))
    print("未解决问题数: {}".format(len(unresolved)))


if __name__ == "__main__":
    main()
