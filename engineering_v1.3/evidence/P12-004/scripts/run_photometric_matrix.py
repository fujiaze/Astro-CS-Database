#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P12-004: T1-T4 与滤镜类别测光矩阵验证脚本

功能:
  - 对 16 帧代表帧 (T2/T3/T4 × LUM/RED/GREEN/BLUE/HA/OIII) 运行 orchestrator stage1
  - 收集 PhotometricDiag 诊断字段 (20 字段)
  - 检查 Gate (Broadband/LRGB fit_used >= 20, 窄带 >= 8)
  - 输出 PHOTOMETRY_MATRIX.csv + photometric_diag_summary.json + failure_classification.json

约束:
  - 不修改测试代码或 DLL
  - 单帧 timeout 600 秒
  - 失败的帧记录原因并继续其他帧
  - UTF-8 编码
"""

from __future__ import annotations

import csv
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# ============================================================================
# 配置
# ============================================================================

PROJECT_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
ORCH_EXE = PROJECT_ROOT / "lib" / "orchestrator" / "cpp" / "orchestrator.exe"
STAGE1_CONFIG_BASE = PROJECT_ROOT / "lib" / "orchestrator" / "configs" / "stage1_config.json"
GAIA_DATA = "GaiaDR3SP"
EVIDENCE_DIR = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P12-004"
RAW_LOGS_DIR = EVIDENCE_DIR / "raw_logs"
REPORTS_DIR = EVIDENCE_DIR / "reports"
SCRIPTS_DIR = EVIDENCE_DIR / "scripts"

# P12-005: 按设备选择校准目录
DEVICE_CALIB_DIR = {
    "T2": "testdata/T2 calibration files",
    "T3": "testdata/T3 calibration files",
    "T4": "testdata/T4 calibration files",
}


def make_device_config(device: str) -> Path:
    """P12-005: 根据设备生成对应的 stage1 config (覆盖 calibration_dir)"""
    import json as _json
    base = _json.loads(STAGE1_CONFIG_BASE.read_text(encoding="utf-8"))
    base["calibration_dir"] = DEVICE_CALIB_DIR.get(device, base["calibration_dir"])
    out = SCRIPTS_DIR / f"stage1_config_{device}.json"
    out.write_text(_json.dumps(base, indent=2, ensure_ascii=False), encoding="utf-8")
    return out

PER_FRAME_TIMEOUT_S = 600

# Broadband/LRGB Gate: fit_used >= 20
BROADBAND_GATE_FIT_USED = 20
# 窄带 Gate: fit_used >= 8
NARROWBAND_GATE_FIT_USED = 8
# scale_factor 合理范围 (P12-005 修复: 移除 0.01 下限, 接受 > 0 即可)
# 项目规则: scale_factor 无下限约束, 仅要求 > 0
SCALE_FACTOR_MIN = 0.0  # 排除 <= 0, 接受任意正数
SCALE_FACTOR_MAX = 1.0e9  # 实际不限制上限

# 滤镜类别
BROADBAND_FILTERS = {"LUM", "RED", "GREEN", "BLUE"}
NARROWBAND_FILTERS = {"HA", "OIII"}

# 16 帧代表帧清单
# (device, filter_canonical, filter_preferred_alias, target, fits_path)
FRAMES: List[Tuple[str, str, str, str, str]] = [
    # T4 (5 帧, Galaxy_Center panel1)
    ("T4", "RED",   "Red",     "Galaxy_Center",
     r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts"),
    ("T4", "GREEN", "Green",   "Galaxy_Center",
     r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@063620-180S-Green.fts"),
    ("T4", "BLUE",  "Blue",    "Galaxy_Center",
     r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@055414-180S-Blue.fts"),
    ("T4", "HA",    "H-alpha", "Galaxy_Center",
     r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@061318-300S-H-alpha.fts"),
    ("T4", "OIII",  "OIII",    "Galaxy_Center",
     r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@063631-600S-Oiii.fts"),
    # T2 (5 帧) — P12-005: 使用 ASCII junction (LDN43_T2_flying_dutchman) 绕过中文路径 bug
    ("T2", "RED",   "Red",     "LDN43",
     r"testdata\LDN43_T2_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts"),
    ("T2", "GREEN", "Green",   "LDN43",
     r"testdata\LDN43_T2_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@034804-1200S-Green.fts"),
    ("T2", "BLUE",  "Blue",    "LDN43",
     r"testdata\LDN43_T2_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@040855-1200S-Blue.fts"),
    ("T2", "HA",    "H-alpha", "LDN43",
     r"testdata\LDN43_T2_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@042947-1200S-H-alpha.fts"),
    ("T2", "OIII",  "OIII",    "NGC1727",
     r"testdata\NGC1727_T2_flying_dutchman\lights\NGC1727_RGBHO_T2_flying_dutchman-20251031@075259-1800S-OIII.fts"),
    # T3 (6 帧, NGC55)
    ("T3", "RED",   "Red",     "NGC55",
     r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts"),
    ("T3", "GREEN", "Green",   "NGC55",
     r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@075153-600S-Green.fts"),
    ("T3", "BLUE",  "Blue",    "NGC55",
     r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@080333-600S-Blue.fts"),
    ("T3", "HA",    "H-alpha", "NGC55",
     r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@081412-1200S-H-alpha.fts"),
    ("T3", "OIII",  "OIII",    "NGC55",
     r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@083458-1200S-Oiii.fts"),
    ("T3", "LUM",   "Lum",     "NGC55",
     r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts"),
]


# ============================================================================
# 数据结构
# ============================================================================

@dataclass
class FrameResult:
    """单帧测光结果"""
    device: str
    filter_class: str  # Broadband / Narrowband
    filter_canonical: str  # LUM/RED/GREEN/BLUE/HA/OIII
    filter_alias: str  # 传给 orchestrator 的滤镜名
    target: str
    frame_name: str  # FITS 文件名
    fits_path: str
    status: str  # PASS / FAIL / STAGE1_ERROR / TIMEOUT
    exit_code: int = -1
    elapsed_s: float = 0.0

    # PhotometricDiag 20 字段
    spectrum_rows_total: int = 0
    valid_fsyn: int = 0
    gaia_projected_in_frame: int = 0
    psf_total: int = 0
    psf_valid: int = 0
    spatial_candidates: int = 0
    unique_matches: int = 0
    rejected_ambiguous: int = 0
    rejected_distance: int = 0
    rejected_quality: int = 0
    fit_used: int = 0
    robust_iterations: int = 0
    scale_factor: float = 0.0
    sigma_residual: float = 0.0
    r_median: float = 0.0
    r_p90: float = 0.0
    r_max: float = 0.0
    match_distance_median: float = 0.0
    match_distance_p90: float = 0.0
    match_distance_max: float = 0.0

    # n_matched (来自 photo_stats.N_MATCHED)
    n_matched: int = 0

    # Gate 检查
    gate_pass: bool = False
    failure_category: str = ""  # INSUFFICIENT_STARS / ZERO_SIGMA / INVALID_SCALE / STAGE1_ERROR / TIMEOUT / ""
    notes: str = ""

    def to_dict(self) -> Dict[str, Any]:
        d = asdict(self)
        # CSV 友好的 filter_class 列
        return d


# ============================================================================
# 工具函数
# ============================================================================

def filter_class_of(filter_canonical: str) -> str:
    """返回滤镜类别: Broadband / Narrowband"""
    if filter_canonical in BROADBAND_FILTERS:
        return "Broadband"
    if filter_canonical in NARROWBAND_FILTERS:
        return "Narrowband"
    return "Unknown"


def parse_stage1_stdout(stdout: str) -> Dict[str, str]:
    """解析 orchestrator stage1 stdout JSON 的 photo_stats 字段

    orchestrator stage1 (非 --request 模式) 通过 output_json_result 输出 JSON:
    {
      "success": true/false,
      "frame_name": "...",
      "timings": [...],
      "wcs_fields": {...},
      "photo_stats": { KEY: VALUE, ... },
      "output_ahpx_path": "...",
      "error_msg": "..."
    }
    """
    photo_stats: Dict[str, str] = {}
    try:
        # stdout 可能含日志前缀, 找到第一个 '{' 开始的 JSON
        idx = stdout.find("{")
        if idx < 0:
            return photo_stats
        json_text = stdout[idx:]
        # 找到最后一个 '}'
        last = json_text.rfind("}")
        if last < 0:
            return photo_stats
        json_text = json_text[: last + 1]
        obj = json.loads(json_text)
        ps = obj.get("photo_stats", {})
        if isinstance(ps, dict):
            for k, v in ps.items():
                photo_stats[k] = str(v)
    except (json.JSONDecodeError, ValueError) as e:
        # 解析失败, 返回空
        pass
    return photo_stats


def to_int(val: str, default: int = 0) -> int:
    try:
        return int(float(val))
    except (TypeError, ValueError):
        return default


def to_float(val: str, default: float = 0.0) -> float:
    try:
        return float(val)
    except (TypeError, ValueError):
        return default


def classify_failure(result: FrameResult) -> Tuple[bool, str, str]:
    """检查 Gate 并分类失败原因

    Returns:
        (gate_pass, failure_category, notes)
    """
    if result.status in ("STAGE1_ERROR", "TIMEOUT"):
        return False, result.status, result.notes

    fit_used = result.fit_used
    sigma = result.sigma_residual
    scale = result.scale_factor
    fc = result.filter_canonical

    # 1. fit_used 阈值
    if fc in BROADBAND_FILTERS:
        threshold = BROADBAND_GATE_FIT_USED
    elif fc in NARROWBAND_FILTERS:
        threshold = NARROWBAND_GATE_FIT_USED
    else:
        return False, "UNKNOWN_FILTER", f"未知滤镜类别: {fc}"

    if fit_used < threshold:
        return False, "INSUFFICIENT_STARS", (
            f"fit_used={fit_used} < 阈值 {threshold} ({result.filter_class})"
        )

    # 2. sigma_residual 必须有限且 > 0
    if not (sigma == sigma and sigma > 0.0):  # NaN check + > 0
        return False, "ZERO_SIGMA", f"sigma_residual={sigma} (非正或非有限)"

    # 3. scale_factor 必须 > 0 (P12-005 修复: 移除下限, 接受任意正值)
    if not (scale > SCALE_FACTOR_MIN and scale <= SCALE_FACTOR_MAX):
        return False, "INVALID_SCALE", (
            f"scale_factor={scale} 必须 > 0 (P12-005: 移除 0.01 下限)"
        )

    return True, "", ""


# ============================================================================
# 主流程
# ============================================================================

def run_single_frame(device: str, filter_canonical: str, filter_alias: str,
                     target: str, fits_rel_path: str) -> FrameResult:
    """对单帧运行 orchestrator stage1 并收集诊断"""
    filter_cls = filter_class_of(filter_canonical)
    fits_abs = PROJECT_ROOT / fits_rel_path
    frame_name = os.path.basename(fits_rel_path)

    # 输出目录: raw_logs/<device>_<filter_canonical>_<target>/
    sub_dir_name = f"{device}_{filter_canonical}_{target}"
    out_dir = RAW_LOGS_DIR / sub_dir_name
    out_dir.mkdir(parents=True, exist_ok=True)

    # 输出 .hiss 路径
    hiss_path = out_dir / f"{sub_dir_name}.hiss"

    result = FrameResult(
        device=device,
        filter_class=filter_cls,
        filter_canonical=filter_canonical,
        filter_alias=filter_alias,
        target=target,
        frame_name=frame_name,
        fits_path=str(fits_abs),
        status="PENDING",
    )

    if not fits_abs.exists():
        result.status = "STAGE1_ERROR"
        result.exit_code = -1
        result.failure_category = "STAGE1_ERROR"
        result.notes = f"FITS 文件不存在: {fits_abs}"
        return result

    # 构造 orchestrator stage1 命令 — P12-005: 按设备生成 config (覆盖 calibration_dir)
    device_config = make_device_config(device)
    cmd = [
        str(ORCH_EXE),
        "stage1",
        "--frame", str(fits_abs),
        "--output", str(hiss_path),
        "--gaia-data", GAIA_DATA,
        "--filter", filter_alias,
        "--config", str(device_config),
        "--log-level", "INFO",
    ]

    log_file = out_dir / "stage1.log"

    print(f"[P12-004] 运行 {device}/{filter_canonical}/{target}: {frame_name}")
    print(f"  输出目录: {out_dir}")

    t0 = time.time()
    try:
        with open(log_file, "w", encoding="utf-8", errors="replace") as lf:
            proc = subprocess.run(
                cmd,
                cwd=str(PROJECT_ROOT),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=PER_FRAME_TIMEOUT_S,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
            # 写日志
            lf.write("=== COMMAND ===\n")
            lf.write(" ".join(cmd) + "\n")
            lf.write("\n=== EXIT CODE ===\n")
            lf.write(str(proc.returncode) + "\n")
            lf.write("\n=== STDOUT ===\n")
            lf.write(proc.stdout or "")
            lf.write("\n=== STDERR ===\n")
            lf.write(proc.stderr or "")
        result.exit_code = proc.returncode
        result.elapsed_s = time.time() - t0

        if proc.returncode != 0:
            result.status = "STAGE1_ERROR"
            # 从 stderr 末尾提取错误信息
            err_tail = (proc.stderr or "").strip().splitlines()
            err_msg = err_tail[-1] if err_tail else f"exit_code={proc.returncode}"
            result.notes = f"exit_code={proc.returncode}: {err_msg[:200]}"
        else:
            result.status = "PASS"  # 暂定, 后面 Gate 检查可能改为 FAIL

        # 解析 stdout JSON 拿 photo_stats
        photo_stats = parse_stage1_stdout(proc.stdout or "")
        result.spectrum_rows_total = to_int(photo_stats.get("SPECTRUM_ROWS_TOTAL"))
        result.valid_fsyn = to_int(photo_stats.get("VALID_FSYN"))
        result.gaia_projected_in_frame = to_int(photo_stats.get("GAIA_IN_FRAME"))
        result.psf_total = to_int(photo_stats.get("PSF_TOTAL"))
        result.psf_valid = to_int(photo_stats.get("PSF_VALID"))
        result.spatial_candidates = to_int(photo_stats.get("SPATIAL_CANDIDATES"))
        result.unique_matches = to_int(photo_stats.get("UNIQUE_MATCHES"))
        result.rejected_ambiguous = to_int(photo_stats.get("REJECTED_AMBIGUOUS"))
        result.rejected_distance = to_int(photo_stats.get("REJECTED_DISTANCE"))
        result.rejected_quality = to_int(photo_stats.get("REJECTED_QUALITY"))
        result.fit_used = to_int(photo_stats.get("FIT_USED"))
        result.robust_iterations = to_int(photo_stats.get("ROBUST_ITERATIONS"))
        result.scale_factor = to_float(photo_stats.get("SCALE_FACTOR"))
        result.sigma_residual = to_float(photo_stats.get("SIGMA_RESIDUAL"))
        result.r_median = to_float(photo_stats.get("R_MEDIAN"))
        result.r_p90 = to_float(photo_stats.get("R_P90"))
        result.r_max = to_float(photo_stats.get("R_MAX"))
        result.match_distance_median = to_float(photo_stats.get("MATCH_DIST_MEDIAN"))
        result.match_distance_p90 = to_float(photo_stats.get("MATCH_DIST_P90"))
        result.match_distance_max = to_float(photo_stats.get("MATCH_DIST_MAX"))
        result.n_matched = to_int(photo_stats.get("N_MATCHED"))

        # 若 stdout 未拿到 photo_stats, 尝试读取 photometry_report.json
        if not photo_stats and result.status == "PASS":
            report_json = out_dir / "photometry_report.json"
            if report_json.exists():
                try:
                    with open(report_json, "r", encoding="utf-8") as f:
                        rj = json.load(f)
                    result.fit_used = int(rj.get("fit_used", 0))
                    result.scale_factor = float(rj.get("scale_factor", 0.0))
                    result.sigma_residual = float(rj.get("sigma_residual", 0.0))
                    result.unique_matches = int(rj.get("unique_matches", 0))
                    result.psf_valid = int(rj.get("psf_valid", 0))
                    result.valid_fsyn = int(rj.get("valid_fsyn", 0))
                    result.gaia_projected_in_frame = int(rj.get("gaia_in_frame", 0))
                    result.spectrum_rows_total = int(rj.get("spectrum_rows_total", 0))
                    result.psf_total = int(rj.get("psf_total", 0))
                    result.spatial_candidates = int(rj.get("spatial_candidates", 0))
                    result.rejected_ambiguous = int(rj.get("rejected_ambiguous", 0))
                    result.rejected_distance = int(rj.get("rejected_distance", 0))
                    result.rejected_quality = int(rj.get("rejected_quality", 0))
                    result.robust_iterations = int(rj.get("robust_iterations", 0))
                    result.r_median = float(rj.get("r_median", 0.0))
                    result.r_p90 = float(rj.get("r_p90", 0.0))
                    result.r_max = float(rj.get("r_max", 0.0))
                    md = rj.get("match_distance", {})
                    result.match_distance_median = float(md.get("median", 0.0))
                    result.match_distance_p90 = float(md.get("p90", 0.0))
                    result.match_distance_max = float(md.get("max", 0.0))
                    result.n_matched = result.unique_matches
                    result.notes = (result.notes + " [从 photometry_report.json 解析]").strip()
                except (json.JSONDecodeError, OSError) as e:
                    result.notes = f"解析 photometry_report.json 失败: {e}"

        # Gate 检查
        if result.status == "PASS":
            gate_pass, category, notes = classify_failure(result)
            result.gate_pass = gate_pass
            result.failure_category = category
            result.notes = (result.notes + (" | " if result.notes else "") + notes).strip(" |")
            if not gate_pass:
                result.status = "FAIL"
        else:
            result.gate_pass = False
            result.failure_category = result.status

        print(f"  exit_code={result.exit_code}, elapsed={result.elapsed_s:.1f}s, "
              f"fit_used={result.fit_used}, scale={result.scale_factor:.4g}, "
              f"sigma={result.sigma_residual:.4g}, gate={result.gate_pass}")

    except subprocess.TimeoutExpired:
        result.status = "TIMEOUT"
        result.elapsed_s = PER_FRAME_TIMEOUT_S
        result.exit_code = -1
        result.gate_pass = False
        result.failure_category = "TIMEOUT"
        result.notes = f"超时 (>{PER_FRAME_TIMEOUT_S}s)"
        print(f"  TIMEOUT (>{PER_FRAME_TIMEOUT_S}s)")
    except Exception as e:
        result.status = "STAGE1_ERROR"
        result.elapsed_s = time.time() - t0
        result.exit_code = -2
        result.gate_pass = False
        result.failure_category = "STAGE1_ERROR"
        result.notes = f"异常: {type(e).__name__}: {e}"
        print(f"  异常: {e}")

    return result


def write_photometry_matrix_csv(results: List[FrameResult], path: Path) -> None:
    """生成 PHOTOMETRY_MATRIX.csv"""
    columns = [
        "device", "filter_class", "filter_name", "target", "frame_name",
        "status", "fit_used", "scale_factor", "sigma_residual", "n_matched",
        "spectrum_rows_total", "valid_fsyn", "gaia_projected_in_frame",
        "psf_valid", "unique_matches", "gate_pass", "failure_category",
        "notes",
    ]
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(columns)
        for r in results:
            w.writerow([
                r.device,
                r.filter_class,
                r.filter_canonical,
                r.target,
                r.frame_name,
                r.status,
                r.fit_used,
                f"{r.scale_factor:.6g}",
                f"{r.sigma_residual:.6g}",
                r.n_matched,
                r.spectrum_rows_total,
                r.valid_fsyn,
                r.gaia_projected_in_frame,
                r.psf_valid,
                r.unique_matches,
                "PASS" if r.gate_pass else "FAIL",
                r.failure_category,
                r.notes,
            ])


def write_diag_summary_json(results: List[FrameResult], path: Path) -> None:
    """生成 photometric_diag_summary.json"""
    summary = {
        "_description": "P12-004 测光诊断汇总 (16 帧代表帧)",
        "_generated_at": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()),
        "_gate_criteria": {
            "broadband_fit_used_min": BROADBAND_GATE_FIT_USED,
            "narrowband_fit_used_min": NARROWBAND_GATE_FIT_USED,
            "scale_factor_range": [SCALE_FACTOR_MIN, SCALE_FACTOR_MAX],
            "sigma_residual_must_be_positive_finite": True,
        },
        "total_frames": len(results),
        "gate_pass_count": sum(1 for r in results if r.gate_pass),
        "gate_fail_count": sum(1 for r in results if not r.gate_pass),
        "failure_category_counts": {},
        "by_filter_class": {},
        "frames": [],
    }
    for r in results:
        cat = r.failure_category or ("PASS" if r.gate_pass else "FAIL")
        summary["failure_category_counts"][cat] = (
            summary["failure_category_counts"].get(cat, 0) + 1
        )
        fc_key = r.filter_class
        if fc_key not in summary["by_filter_class"]:
            summary["by_filter_class"][fc_key] = {
                "total": 0, "pass": 0, "fail": 0,
            }
        summary["by_filter_class"][fc_key]["total"] += 1
        if r.gate_pass:
            summary["by_filter_class"][fc_key]["pass"] += 1
        else:
            summary["by_filter_class"][fc_key]["fail"] += 1

        summary["frames"].append({
            "device": r.device,
            "filter_class": r.filter_class,
            "filter_name": r.filter_canonical,
            "target": r.target,
            "frame_name": r.frame_name,
            "status": r.status,
            "gate_pass": r.gate_pass,
            "failure_category": r.failure_category,
            "exit_code": r.exit_code,
            "elapsed_s": round(r.elapsed_s, 3),
            "diag": {
                "spectrum_rows_total": r.spectrum_rows_total,
                "valid_fsyn": r.valid_fsyn,
                "gaia_projected_in_frame": r.gaia_projected_in_frame,
                "psf_total": r.psf_total,
                "psf_valid": r.psf_valid,
                "spatial_candidates": r.spatial_candidates,
                "unique_matches": r.unique_matches,
                "rejected_ambiguous": r.rejected_ambiguous,
                "rejected_distance": r.rejected_distance,
                "rejected_quality": r.rejected_quality,
                "fit_used": r.fit_used,
                "robust_iterations": r.robust_iterations,
                "scale_factor": r.scale_factor,
                "sigma_residual": r.sigma_residual,
                "r_median": r.r_median,
                "r_p90": r.r_p90,
                "r_max": r.r_max,
                "match_distance_median": r.match_distance_median,
                "match_distance_p90": r.match_distance_p90,
                "match_distance_max": r.match_distance_max,
                "n_matched": r.n_matched,
            },
            "notes": r.notes,
        })
    with open(path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)


def write_failure_classification_json(results: List[FrameResult], path: Path) -> None:
    """生成 failure_classification.json"""
    failed = [r for r in results if not r.gate_pass]
    obj = {
        "_description": "P12-004 失败帧分类",
        "_categories": [
            "INSUFFICIENT_STARS - fit_used 不足",
            "ZERO_SIGMA - sigma_residual = 0 或非有限",
            "INVALID_SCALE - scale_factor 超范围",
            "STAGE1_ERROR - orchestrator 运行失败",
            "TIMEOUT - 超时",
        ],
        "total_failed": len(failed),
        "total_passed": len(results) - len(failed),
        "failures": [
            {
                "device": r.device,
                "filter_name": r.filter_canonical,
                "target": r.target,
                "frame_name": r.frame_name,
                "category": r.failure_category,
                "fit_used": r.fit_used,
                "scale_factor": r.scale_factor,
                "sigma_residual": r.sigma_residual,
                "exit_code": r.exit_code,
                "notes": r.notes,
            }
            for r in failed
        ],
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)


def main() -> int:
    # 确保目录存在
    RAW_LOGS_DIR.mkdir(parents=True, exist_ok=True)
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)

    # 前置检查
    if not ORCH_EXE.exists():
        print(f"[ERROR] orchestrator.exe 不存在: {ORCH_EXE}")
        return 2
    if not STAGE1_CONFIG_BASE.exists():
        print(f"[ERROR] stage1_config.json 不存在: {STAGE1_CONFIG_BASE}")
        return 2

    print(f"[P12-004] 共 {len(FRAMES)} 帧代表帧")
    print(f"[P12-004] orchestrator: {ORCH_EXE}")
    print(f"[P12-004] 配置基线: {STAGE1_CONFIG_BASE} (按设备动态生成)")
    print(f"[P12-004] timeout: {PER_FRAME_TIMEOUT_S}s/帧")
    print()

    results: List[FrameResult] = []
    for i, (device, filt_can, filt_alias, target, fits_path) in enumerate(FRAMES, 1):
        print(f"=== [{i}/{len(FRAMES)}] {device}/{filt_can}/{target} ===")
        r = run_single_frame(device, filt_can, filt_alias, target, fits_path)
        results.append(r)
        print()

    # 生成报告
    matrix_csv = REPORTS_DIR / "PHOTOMETRY_MATRIX.csv"
    summary_json = REPORTS_DIR / "photometric_diag_summary.json"
    failure_json = REPORTS_DIR / "failure_classification.json"

    write_photometry_matrix_csv(results, matrix_csv)
    write_diag_summary_json(results, summary_json)
    write_failure_classification_json(results, failure_json)

    # 汇总打印
    n_pass = sum(1 for r in results if r.gate_pass)
    n_fail = len(results) - n_pass
    print("=" * 60)
    print(f"[P12-004] 测光矩阵测试完成")
    print(f"  总帧数: {len(results)}")
    print(f"  Gate PASS: {n_pass}")
    print(f"  Gate FAIL: {n_fail}")
    print(f"  Gate 通过率: {n_pass / len(results) * 100:.1f}%")

    # 按类别统计
    cat_counts: Dict[str, int] = {}
    for r in results:
        cat = r.failure_category or ("PASS" if r.gate_pass else "FAIL")
        cat_counts[cat] = cat_counts.get(cat, 0) + 1
    print(f"  分类统计: {cat_counts}")

    # 按滤镜类别统计
    bb = [r for r in results if r.filter_class == "Broadband"]
    nb = [r for r in results if r.filter_class == "Narrowband"]
    bb_pass = sum(1 for r in bb if r.gate_pass)
    nb_pass = sum(1 for r in nb if r.gate_pass)
    print(f"  Broadband (LUM/RED/GREEN/BLUE): {bb_pass}/{len(bb)} PASS")
    print(f"  Narrowband (HA/OIII):           {nb_pass}/{len(nb)} PASS")

    print()
    print(f"  PHOTOMETRY_MATRIX.csv:           {matrix_csv}")
    print(f"  photometric_diag_summary.json:   {summary_json}")
    print(f"  failure_classification.json:     {failure_json}")

    # 全部失败则提示阻塞
    if n_pass == 0:
        print()
        print("[P12-004] 警告: 所有 16 帧 Gate 失败, 建议生成 BLOCKED_REPORT.md")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
