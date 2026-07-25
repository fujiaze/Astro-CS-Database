# -*- coding: utf-8 -*-
"""
P05-001 Canonical 数据集登记脚本
================================
功能:
    1. 从 P02-001 manifest 中按目标天区选择代表性 canonical 帧
    2. 读取每帧 FITS header 获取元数据 (尺寸/曝光/滤镜/CCD温度/OBJECT/DATE-OBS)
    3. 从 P02-001 results 提取 plate solve 结果作为预期范围验证
    4. 计算 SHA-256 (复用 manifest)
    5. 生成 canonical_dataset.json + canonical_dataset_registry.csv

选择规则 (每目标 1-2 帧, 总计约 7-14 帧):
    - Galaxy_Center_T4: panel1 第一帧 (Red)
    - LDN43_T2: 第一帧 (Lum)
    - NGC1727_T2: 第一帧
    - NGC247_T2: 第一帧
    - NGC55_T3: 第一帧
    - NGC83_cluster_T3: 第一帧
    - Victory_Nebula_T4: mosaic1 第一帧 Lum

依赖:
    - engineering/evidence/P02-001/testdata_manifest.json
    - engineering/evidence/P02-001/results/frame_XXXX.json
    - astropy.io.fits (读取 FITS header)

作者: P05-001 子 Agent
日期: 2026-07-25
"""

from __future__ import annotations

import os
import sys
import json
import csv
import hashlib
from datetime import datetime
from typing import Optional

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# SCRIPT_DIR = engineering/evidence/P05-001, 上溯 3 级到项目根
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", ".."))

MANIFEST_PATH = os.path.join(
    PROJECT_ROOT, "engineering", "evidence", "P02-001", "testdata_manifest.json"
)
P02_RESULTS_DIR = os.path.join(
    PROJECT_ROOT, "engineering", "evidence", "P02-001", "results"
)
OUTPUT_DIR = SCRIPT_DIR  # evidence/P05-001/


# ============================================================================
# Canonical 帧选择规则
# ============================================================================
# 每个目标天区的选择函数: 输入该目标的所有帧 (list[dict]), 返回选中的帧 index 列表
# 规则: 优先选第一帧; Victory_Nebula 额外选 mosaic1 第一帧 Lum


def select_galaxy_center(frames):
    """Galaxy_Center: panel1 第一帧 Red (index 升序第一)"""
    # manifest 已按 target/panel/time 排序, 第一帧即 panel1 Red
    return [frames[0]["index"]]


def select_ldn43(frames):
    """LDN43: 第一帧 (Lum)"""
    return [frames[0]["index"]]


def select_ngc1727(frames):
    """NGC1727: 第一帧"""
    return [frames[0]["index"]]


def select_ngc247(frames):
    """NGC247: 第一帧"""
    return [frames[0]["index"]]


def select_ngc55(frames):
    """NGC55: 第一帧"""
    return [frames[0]["index"]]


def select_ngc83(frames):
    """NGC83_cluster: 第一帧"""
    return [frames[0]["index"]]


def select_victory_nebula(frames):
    """Victory_Nebula: mosaic1 第一帧 Lum (按文件名匹配 mosaic1 + Lum)"""
    # 第一帧 (整体最早)
    selected = [frames[0]["index"]]
    # 找 mosaic1 第一帧 Lum
    for fr in frames:
        fn = fr["filename"]
        if "mosaic1" in fn and fr["filter"] == "Lum":
            if fr["index"] not in selected:
                selected.append(fr["index"])
            break
    return selected


SELECTORS = {
    "Galaxy_Center": select_galaxy_center,
    "LDN43": select_ldn43,
    "NGC1727": select_ngc1727,
    "NGC247": select_ngc247,
    "NGC55": select_ngc55,
    "NGC83_cluster": select_ngc83,
    "Victory_Nebula": select_victory_nebula,
}


# ============================================================================
# FITS header 读取
# ============================================================================
def read_fits_metadata(fits_path):
    """读取 FITS header 获取元数据

    返回:
        dict: width, height, exposure, filter, ccd_temp, object, date_obs, bitpix
    """
    from astropy.io import fits

    meta = {}
    with fits.open(fits_path, mode="readonly", memmap=False) as hdul:
        header = hdul[0].header
        data = hdul[0].data
        meta["width"] = int(data.shape[1]) if data is not None else 0
        meta["height"] = int(data.shape[0]) if data is not None else 0
        meta["bitpix"] = int(header.get("BITPIX", 0))

        # EXPTIME / EXPOSURE
        meta["exposure"] = float(
            header.get("EXPTIME", header.get("EXPOSURE", 0.0))
        )

        # FILTER
        meta["filter"] = str(header.get("FILTER", ""))

        # CCD-TEMP (冷却温度)
        try:
            meta["ccd_temp"] = float(header.get("CCD-TEMP", header.get("SET-TEMP", 0.0)))
        except (ValueError, TypeError):
            meta["ccd_temp"] = 0.0

        # OBJECT
        meta["object"] = str(header.get("OBJECT", ""))

        # DATE-OBS
        meta["date_obs"] = str(header.get("DATE-OBS", ""))

    return meta


# ============================================================================
# SHA-256 重算 (验证 manifest 的 sha256)
# ============================================================================
def compute_sha256(path, chunk_size=1024 * 1024):
    """计算文件 SHA-256"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest().upper()


# ============================================================================
# 从 P02-001 results 读取 plate solve 结果
# ============================================================================
def load_p02_result(index):
    """从 P02-001 results/frame_XXXX.json 读取 plate solve 结果"""
    path = os.path.join(P02_RESULTS_DIR, "frame_%04d.json" % index)
    if not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


# ============================================================================
# 定义预期数值范围
# ============================================================================
def build_expected_ranges(p02_result):
    """基于 P02-001 plate solve 结果定义预期数值范围

    返回:
        dict: expected_platesolve_success, expected_rms_range, expected_n_pairs_range,
              expected_psf_valid, expected_n_matched_range, expected_snr_has_snr,
              expected_hiss_size_kb
    """
    if p02_result is None:
        return {
            "expected_platesolve_success": "unknown",
            "expected_rms_range": "unknown",
            "expected_n_pairs_range": "unknown",
            "expected_psf_valid": "unknown",
            "expected_n_matched_range": "unknown",
            "expected_snr_has_snr": "unknown",
            "expected_hiss_size_kb": "unknown",
            "actual_rms_arcsec": None,
            "actual_n_pairs": None,
            "actual_success": None,
        }

    success = bool(p02_result.get("success", False))
    rms_arcsec = float(p02_result.get("rms_arcsec", 0.0))
    n_pairs = int(p02_result.get("n_pairs", 0))

    # 预期 RMS 范围 (放宽到 < 1.0" 任务要求, 实际 P02-001 大多 < 0.5")
    if success and rms_arcsec > 0:
        rms_lo = 0.0
        rms_hi = max(1.0, rms_arcsec * 2.0)  # 上限取 max(1.0", 2x实际值)
        expected_rms_range = "[0.00, %.3f]" % rms_hi
    else:
        expected_rms_range = "[0.00, 1.000]"

    # 预期 n_pairs 范围 (任务要求 > 10, 实际 P02-001 大多 > 30)
    if success and n_pairs > 0:
        np_lo = 10
        np_hi = max(50, n_pairs * 2)
        expected_n_pairs_range = "[%d, %d]" % (np_lo, np_hi)
    else:
        expected_n_pairs_range = "[10, 1000]"

    return {
        "expected_platesolve_success": "true" if success else "false",
        "expected_rms_range": expected_rms_range,
        "expected_n_pairs_range": expected_n_pairs_range,
        "expected_psf_valid": "true",  # PSF 参数非 NaN (骨架退化时可能为 0)
        "expected_n_matched_range": "[0, 5000]",  # G-002 缺口, 可能为 0
        "expected_snr_has_snr": "0_or_1",  # 骨架退化 has_snr=0, P03-004 修复后 has_snr=1
        "expected_hiss_size_kb": ">10",  # HISS 文件 > 10KB
        "actual_rms_arcsec": rms_arcsec,
        "actual_n_pairs": n_pairs,
        "actual_success": success,
    }


# ============================================================================
# 主流程
# ============================================================================
def main():
    # Windows 控制台 UTF-8
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    print("=" * 70)
    print("P05-001 Canonical 数据集登记")
    print("=" * 70)

    # 1. 加载 P02-001 manifest
    print("[1/5] 加载 P02-001 manifest...")
    with open(MANIFEST_PATH, "r", encoding="utf-8") as f:
        manifest = json.load(f)
    print("      manifest 总帧数: %d" % manifest["_meta"]["total_frames"])
    print("      manifest_sha256: %s" % manifest["_meta"]["manifest_sha256"])

    # 2. 按目标天区分组
    print("[2/5] 按目标天区分组并选择 canonical 帧...")
    by_target = {}
    for fr in manifest["frames"]:
        t = fr["target_name"]
        if t not in by_target:
            by_target[t] = []
        by_target[t].append(fr)

    for t, frames in sorted(by_target.items()):
        print("      %s: %d 帧" % (t, len(frames)))

    # 3. 选择 canonical 帧
    selected_frames = []
    for target_name, selector in SELECTORS.items():
        if target_name not in by_target:
            print("      [WARN] 目标天区 %s 不在 manifest 中" % target_name)
            continue
        frames = by_target[target_name]
        indices = selector(frames)
        for idx in indices:
            fr = next(f for f in frames if f["index"] == idx)
            selected_frames.append(fr)
            print(
                "      [SELECT] %s index=%d %s"
                % (target_name, idx, fr["filename"])
            )

    print("      总计选择 %d 个 canonical 帧" % len(selected_frames))

    # 4. 读取每帧元数据 + SHA-256 + P02-001 结果
    print("[3/5] 读取 canonical 帧元数据 + 验证 SHA-256 + 加载 P02-001 结果...")
    canonical_entries = []
    for i, fr in enumerate(selected_frames, 1):
        fits_path = os.path.join(PROJECT_ROOT, fr["filepath"])
        print(
            "      [%d/%d] %s"
            % (i, len(selected_frames), fr["filename"])
        )

        # 读取 FITS header
        meta = read_fits_metadata(fits_path)

        # 验证 SHA-256 (重算一次确认 manifest 准确)
        sha256_actual = compute_sha256(fits_path)
        sha256_manifest = fr["sha256"]
        sha256_match = sha256_actual == sha256_manifest
        if not sha256_match:
            print(
                "      [WARN] SHA-256 不匹配! manifest=%s actual=%s"
                % (sha256_manifest[:16], sha256_actual[:16])
            )

        # 加载 P02-001 plate solve 结果
        p02_result = load_p02_result(fr["index"])

        # 构建预期范围
        expected = build_expected_ranges(p02_result)

        # 文件大小
        size_bytes = int(fr["size_bytes"])

        entry = {
            "dataset_id": "P05-001-C%03d" % i,
            "canonical_index": i,
            "p02_001_index": fr["index"],
            "case_id": fr["case_id"],
            "target": fr["target_name"],
            "target_full": fr["target_full"],
            "panel": fr["panel"],
            "filename": fr["filename"],
            "frame_path": fr["filepath"],
            "frame_path_absolute": fits_path,
            "sha256": sha256_actual,
            "sha256_manifest": sha256_manifest,
            "sha256_match": sha256_match,
            "size_bytes": size_bytes,
            "size_mb": round(size_bytes / (1024 * 1024), 2),
            "width": meta["width"],
            "height": meta["height"],
            "bitpix": meta["bitpix"],
            "exposure": meta["exposure"],
            "filter": meta["filter"],
            "ccd_temp": meta["ccd_temp"],
            "object": meta["object"],
            "date_obs": meta["date_obs"],
            # P02-001 实际 plate solve 结果
            "p02_001_success": expected["actual_success"],
            "p02_001_rms_arcsec": expected["actual_rms_arcsec"],
            "p02_001_n_pairs": expected["actual_n_pairs"],
            # 预期数值范围
            "expected_platesolve_success": expected["expected_platesolve_success"],
            "expected_rms_range": expected["expected_rms_range"],
            "expected_n_pairs_range": expected["expected_n_pairs_range"],
            "expected_psf_valid": expected["expected_psf_valid"],
            "expected_n_matched_range": expected["expected_n_matched_range"],
            "expected_snr_has_snr": expected["expected_snr_has_snr"],
            "expected_hiss_size_kb": expected["expected_hiss_size_kb"],
        }
        canonical_entries.append(entry)

    # 5. 生成 canonical_dataset.json
    print("[4/5] 生成 canonical_dataset.json...")
    dataset = {
        "_meta": {
            "task_id": "P05-001",
            "task_name": "真实参考数据集登记 (v1.1 开发包)",
            "phase": "P05",
            "gate": "G5",
            "commit_base": _get_git_commit(),
            "generated_at": datetime.now().strftime("%Y-%m-%dT%H:%M:%S+08:00"),
            "manifest_source": "engineering/evidence/P02-001/testdata_manifest.json",
            "manifest_sha256": manifest["_meta"]["manifest_sha256"],
            "total_canonical_frames": len(canonical_entries),
            "selection_rule": (
                "每个目标天区选 1-2 帧代表性帧; "
                "Galaxy_Center=panel1 第一帧 Red; "
                "LDN43/NGC1727/NGC247/NGC55/NGC83=第一帧; "
                "Victory_Nebula=第一帧 + mosaic1 第一帧 Lum"
            ),
            "expected_ranges_basis": (
                "P02-001 plate solve 实际结果 + 任务规范 (RMS<1.0\", n_pairs>10); "
                "PSF 有效参数非 NaN; "
                "测光 n_matched 可能为 0 (G-002 缺口); "
                "SNR has_snr 可能为 0 (骨架退化, P03-004 已修复); "
                "HISS 文件 > 10KB"
            ),
            "scope": "数据集登记任务, 不修改业务源码",
        },
        "by_target": {},
        "frames": canonical_entries,
    }
    # by_target 索引
    for entry in canonical_entries:
        t = entry["target"]
        if t not in dataset["by_target"]:
            dataset["by_target"][t] = []
        dataset["by_target"][t].append(entry["dataset_id"])

    out_json = os.path.join(OUTPUT_DIR, "canonical_dataset.json")
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(dataset, f, ensure_ascii=False, indent=2, default=str)
    print("      已写入: %s" % out_json)

    # 6. 生成 canonical_dataset_registry.csv
    print("[5/5] 生成 canonical_dataset_registry.csv...")
    out_csv = os.path.join(
        PROJECT_ROOT, "engineering", "contracts", "canonical_dataset_registry.csv"
    )
    fieldnames = [
        "dataset_id",
        "target",
        "frame_path",
        "sha256",
        "size_bytes",
        "width",
        "height",
        "exposure",
        "filter",
        "ccd_temp",
        "object",
        "date_obs",
        "expected_platesolve_success",
        "expected_rms_range",
        "expected_n_pairs_range",
        "p02_001_rms_arcsec",
        "p02_001_n_pairs",
    ]
    with open(out_csv, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        for entry in canonical_entries:
            writer.writerow(entry)
    print("      已写入: %s" % out_csv)

    # 摘要
    print("")
    print("=" * 70)
    print("Canonical 数据集登记完成 - 摘要")
    print("=" * 70)
    print("总帧数: %d" % len(canonical_entries))
    print("目标天区数: %d" % len(dataset["by_target"]))
    print("")
    print("%-20s %-50s %-8s %-8s %-8s %-8s" % (
        "Target", "Filename", "Filter", "Exp", "W×H", "SHA256(前8)"
    ))
    print("-" * 110)
    for e in canonical_entries:
        print("%-20s %-50s %-8s %-6ss %dx%d %s" % (
            e["target"], e["filename"][:50], e["filter"], e["exposure"],
            e["width"], e["height"], e["sha256"][:8]
        ))
    print("")
    print("P02-001 plate solve 结果:")
    print("%-20s %-8s %-12s %-10s" % ("Target", "Success", "RMS(\")", "n_pairs"))
    print("-" * 60)
    for e in canonical_entries:
        print("%-20s %-8s %-12.4f %-10d" % (
            e["target"], e["p02_001_success"],
            e["p02_001_rms_arcsec"] or 0, e["p02_001_n_pairs"] or 0
        ))
    print("=" * 70)

    return 0


def _get_git_commit():
    """获取当前 git commit hash"""
    import subprocess
    try:
        r = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            capture_output=True, text=True, cwd=PROJECT_ROOT, timeout=5,
        )
        return r.stdout.strip()
    except Exception:
        return "unknown"


if __name__ == "__main__":
    sys.exit(main())
