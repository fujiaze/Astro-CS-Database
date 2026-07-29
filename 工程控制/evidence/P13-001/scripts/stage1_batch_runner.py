#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P13-001 — Stage1 全 TestData 批处理与恢复入口

功能：
  - 扫描 testdata 得到 710 帧清单（按 DATASETS.md 规则）
  - 对每帧调用 orchestrator.exe stage1
  - 缓存绑定 commit/orchestrator.exe/config/filters/qe_curves/input_fits hash
  - 断点恢复：batch_state.json 持久化每帧状态
  - 超时保护：subprocess timeout
  - 分类报告：CSV + JSON + by_device/target/filter 统计

入口子命令：
  scan             扫描 testdata 得到帧清单
  run              运行批处理（自动跳过已缓存且 hash 匹配的 PASS 帧）
  resume           等同 run（断点恢复是默认行为）
  status           查看批处理进度
  report           仅从 batch_state.json 重新生成报告
  cache-list       列出 cache 目录
  cache-clear      清空 cache 目录

用法示例：
  python stage1_batch_runner.py scan
  python stage1_batch_runner.py run --limit 5
  python stage1_batch_runner.py run --device T4 --target Galaxy_Center
  python stage1_batch_runner.py run --fresh
  python stage1_batch_runner.py status
  python stage1_batch_runner.py report

设计原则（P13-001 禁止捷径）：
  缓存必须绑定 commit + orchestrator.exe + config + filters.json + qe_curves.json
  + input_fits + gaia_data_dir 七元 hash。任一变更则缓存失效，强制重跑。

依赖：
  - orchestrator.exe (lib/orchestrator/cpp/)
  - stage1_config_T2/T3/T4.json (engineering_v1.3/evidence/P12-004/scripts/)
  - testdata/ASCII junctions (LDN43_T2_flying_dutchman 等已建立)
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# ============================================================================
# 常量
# ============================================================================

PROJECT_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
ORCH_EXE = PROJECT_ROOT / "lib" / "orchestrator" / "cpp" / "orchestrator.exe"
STAGE1_CONFIG_BASE = PROJECT_ROOT / "lib" / "orchestrator" / "configs" / "stage1_config.json"
DEVICE_CONFIG_DIR = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P12-004" / "scripts"
FILTERS_JSON = PROJECT_ROOT / "lib" / "photometric_calib" / "data" / "response_curves" / "filters.json"
QE_CURVES_JSON = PROJECT_ROOT / "lib" / "photometric_calib" / "data" / "response_curves" / "qe_curves.json"
GAIA_DATA_DIR = "GaiaDR3SP"
EVIDENCE_DIR = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P13-001"
SCRIPTS_DIR = EVIDENCE_DIR / "scripts"
RAW_LOGS_DIR = EVIDENCE_DIR / "raw_logs"
REPORTS_DIR = EVIDENCE_DIR / "reports"
CACHE_DIR = EVIDENCE_DIR / "cache"
BATCH_STATE_FILE = EVIDENCE_DIR / "batch_state.json"

# 按设备选择 stage1_config_<device>.json
DEVICE_CONFIG = {
    "T2": DEVICE_CONFIG_DIR / "stage1_config_T2.json",
    "T3": DEVICE_CONFIG_DIR / "stage1_config_T3.json",
    "T4": DEVICE_CONFIG_DIR / "stage1_config_T4.json",
}

# 数据集目录 → 设备映射（DATASETS.md）
DATASET_DEVICE_MAP = {
    "Victory_Nebula_T4_Flying_Dutchman": "T4",
    "Galaxy_Center_T4": "T4",
    "NGC55_T3_flying_dutchman": "T3",
    "NGC247_T2_flying_dutchman": "T2",
    "NGC1727_T2_flying_dutchman": "T2",
    "NGC83_cluster_T3_Flying_Dutchman": "T3",
    "LDN43_T2_flying_dutchman": "T2",  # ASCII junction (绕过中文路径)
}

# 单帧默认超时（秒），与 P12-004 一致
DEFAULT_TIMEOUT_S = 600

# 滤镜规范化（与 orchestrator map_filter_name 对齐）
FILTER_CANONICAL_MAP = {
    "lum": "LUM", "red": "RED", "green": "GREEN", "blue": "BLUE",
    "h-alpha": "HA", "halpha": "HA", "ha": "HA",
    "oiii": "OIII", "oiii.fts": "OIII",
}

BROADBAND_FILTERS = {"LUM", "RED", "GREEN", "BLUE"}
NARROWBAND_FILTERS = {"HA", "OIII"}


# ============================================================================
# 数据结构
# ============================================================================

@dataclass
class FrameRecord:
    """扫描得到的单帧记录"""
    dataset: str           # 数据集目录名 (如 "Galaxy_Center_T4")
    device: str            # T2/T3/T4
    target: str            # 目标简称 (如 "Galaxy_Center")
    filter_canonical: str  # LUM/RED/GREEN/BLUE/HA/OIII
    filter_alias: str      # 传给 orchestrator 的滤镜名 (如 "H-alpha", "OIII")
    frame_name: str        # .fts 文件名
    fits_path: str         # 相对 PROJECT_ROOT 的路径
    panel: str             # panel1/panel2/panel3 (Galaxy_Center 用)，其他为 ""

    @property
    def frame_id(self) -> str:
        """帧唯一 ID（用于 cache 文件名），格式: <device>_<target>_<filter>_<basename>"""
        bn = Path(self.frame_name).stem
        return f"{self.device}_{self.target}_{self.filter_canonical}_{bn}"

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class FrameResult:
    """单帧运行结果"""
    frame_id: str
    dataset: str
    device: str
    target: str
    filter_canonical: str
    filter_class: str  # Broadband / Narrowband / Unknown
    frame_name: str
    fits_path: str
    status: str = "PENDING"  # PENDING/RUNNING/PASS/FAIL/STAGE1_ERROR/TIMEOUT/SKIPPED
    exit_code: int = -1
    elapsed_s: float = 0.0
    hash_key: str = ""
    cache_hit: bool = False

    # PhotometricDiag 关键字段（photo_stats KV 解析）
    spectrum_rows_total: int = 0
    valid_fsyn: int = 0
    psf_valid: int = 0
    fit_used: int = 0
    scale_factor: float = 0.0
    sigma_residual: float = 0.0
    unique_matches: int = 0
    n_matched: int = 0
    has_snr: int = 0
    snr_n_points: int = 0
    hiss_path: str = ""
    hiss_sha256: str = ""

    failure_category: str = ""
    notes: str = ""


@dataclass
class BatchState:
    """批处理状态（持久化到 batch_state.json）"""
    version: int = 1
    created_at: str = ""
    updated_at: str = ""
    git_commit: str = ""
    orchestrator_sha256: str = ""
    total_frames: int = 0
    completed: int = 0
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    timeouts: int = 0
    errors: int = 0
    frames: Dict[str, Dict[str, Any]] = field(default_factory=dict)
    # frames[frame_id] = {status, exit_code, elapsed_s, hash_key, completed_at, ...}


# ============================================================================
# 工具函数
# ============================================================================

def sha256_file(path: Path) -> str:
    """计算文件 SHA256（大文件分块）"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def git_commit_hash() -> str:
    """获取当前 git HEAD commit hash"""
    try:
        proc = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=str(PROJECT_ROOT),
            capture_output=True, text=True, timeout=10,
        )
        if proc.returncode == 0:
            return proc.stdout.strip()
    except Exception:
        pass
    return "UNKNOWN"


def filter_class_of(filter_canonical: str) -> str:
    if filter_canonical in BROADBAND_FILTERS:
        return "Broadband"
    if filter_canonical in NARROWBAND_FILTERS:
        return "Narrowband"
    return "Unknown"


def canonical_filter_from_filename(frame_name: str) -> Tuple[str, str]:
    """从 .fts 文件名解析规范滤镜名和传给 orchestrator 的别名

    Returns:
        (filter_canonical, filter_alias)
        filter_canonical: LUM/RED/GREEN/BLUE/HA/OIII
        filter_alias: 传给 orchestrator --filter 的字符串（与 map_filter_name 对齐）
    """
    name_lower = frame_name.lower()
    # 注意顺序：先匹配 h-alpha（避免被 -alpha 后缀干扰），再匹配 oiii
    if "-h-alpha" in name_lower or "_h-alpha" in name_lower or "-halpha" in name_lower:
        return "HA", "H-alpha"
    if "-oiii" in name_lower or "_oiii" in name_lower:
        return "OIII", "OIII"
    if "-lum" in name_lower or "_lum" in name_lower:
        return "LUM", "Lum"
    if "-red" in name_lower or "_red" in name_lower:
        return "RED", "Red"
    if "-green" in name_lower or "_green" in name_lower:
        return "GREEN", "Green"
    if "-blue" in name_lower or "_blue" in name_lower:
        return "BLUE", "Blue"
    return "UNKNOWN", ""


def parse_stage1_stdout(stdout: str) -> Dict[str, str]:
    """解析 orchestrator stage1 stdout JSON 的 photo_stats 字段"""
    photo_stats: Dict[str, str] = {}
    try:
        idx = stdout.find("{")
        if idx < 0:
            return photo_stats
        json_text = stdout[idx:]
        last = json_text.rfind("}")
        if last < 0:
            return photo_stats
        json_text = json_text[: last + 1]
        obj = json.loads(json_text)
        ps = obj.get("photo_stats", {})
        if isinstance(ps, dict):
            for k, v in ps.items():
                photo_stats[k] = str(v)
        # 顺便解析 output_ahpx_path
        if "output_ahpx_path" in obj:
            photo_stats["_output_ahpx_path"] = str(obj.get("output_ahpx_path", ""))
    except (json.JSONDecodeError, ValueError):
        pass
    return photo_stats


def to_int(val: Any, default: int = 0) -> int:
    try:
        return int(float(val))
    except (TypeError, ValueError):
        return default


def to_float(val: Any, default: float = 0.0) -> float:
    try:
        return float(val)
    except (TypeError, ValueError):
        return default


# ============================================================================
# 扫描器：testdata → FrameRecord 列表
# ============================================================================

def scan_testdata() -> List[FrameRecord]:
    """扫描 testdata 目录，按 DATASETS.md 规则得到 710 帧清单

    规则：
      - Victory_Nebula_T4_Flying_Dutchman: lights/<bn>.fts (扁平)
      - Galaxy_Center_T4: lights/panel<N>/<filter>/<bn>.fts (嵌套)
      - 其他 5 个: lights/<bn>.fts (扁平)
      - 仅扫描 .fts 文件（忽略 .xisf 校准帧、子目录中的诊断日志）
    """
    frames: List[FrameRecord] = []
    testdata = PROJECT_ROOT / "testdata"

    for dataset_dir_name, device in DATASET_DEVICE_MAP.items():
        dataset_dir = testdata / dataset_dir_name
        if not dataset_dir.is_dir():
            continue

        # 推导 target 简称：去除 _T<N>_<flying_dutchman> 等后缀
        target = dataset_dir_name
        # Victory_Nebula_T4_Flying_Dutchman → Victory_Nebula
        target = re.sub(r"_T[234]_.*$", "", target)
        # Galaxy_Center_T4 → Galaxy_Center
        target = re.sub(r"_T[234]$", "", target)
        # NGC83_cluster_T3_Flying_Dutchman → NGC83
        target = re.sub(r"_cluster.*$", "", target)

        lights_dir = dataset_dir / "lights"
        if not lights_dir.is_dir():
            continue

        if dataset_dir_name == "Galaxy_Center_T4":
            # 实际结构：panel<N>/<bn>.fts（panel 下直接是 .fts 文件，无 filter 子目录）
            # 滤镜从文件名解析（-Red/-Green/-Blue/-H-alpha/-Oiii 后缀）
            for panel_dir in sorted(lights_dir.iterdir()):
                if not panel_dir.is_dir() or not panel_dir.name.startswith("panel"):
                    continue
                panel_name = panel_dir.name
                for fts in sorted(panel_dir.glob("*.fts")):
                    fc, fa = canonical_filter_from_filename(fts.name)
                    if fc == "UNKNOWN":
                        continue
                    frames.append(FrameRecord(
                        dataset=dataset_dir_name,
                        device=device,
                        target=target,
                        filter_canonical=fc,
                        filter_alias=fa,
                        frame_name=fts.name,
                        fits_path=str(fts.relative_to(PROJECT_ROOT)),
                        panel=panel_name,
                    ))
        else:
            # 扁平：lights/<bn>.fts
            for fts in sorted(lights_dir.glob("*.fts")):
                fc, fa = canonical_filter_from_filename(fts.name)
                if fc == "UNKNOWN":
                    continue
                frames.append(FrameRecord(
                    dataset=dataset_dir_name,
                    device=device,
                    target=target,
                    filter_canonical=fc,
                    filter_alias=fa,
                    frame_name=fts.name,
                    fits_path=str(fts.relative_to(PROJECT_ROOT)),
                    panel="",
                ))

    return frames


# ============================================================================
# Hash 缓存（绑定 commit/config/input hash，禁止捷径）
# ============================================================================

def compute_frame_hash_key(rec: FrameRecord, commit: str, orch_sha: str) -> str:
    """计算单帧缓存 hash key

    hash_key = sha256(concat(
        git_commit_hash,
        orchestrator.exe_sha256,
        stage1_config_<device>.json_sha256,
        filters.json_sha256,
        qe_curves.json_sha256,
        input_fits_sha256,
        gaia_data_dir_name,
    ))

    任一变更则 hash_key 变化，强制重跑该帧。
    """
    config_path = DEVICE_CONFIG.get(rec.device)
    config_sha = sha256_file(config_path) if config_path and config_path.exists() else "MISSING"
    filters_sha = sha256_file(FILTERS_JSON) if FILTERS_JSON.exists() else "MISSING"
    qe_sha = sha256_file(QE_CURVES_JSON) if QE_CURVES_JSON.exists() else "MISSING"
    fits_abs = PROJECT_ROOT / rec.fits_path
    fits_sha = sha256_file(fits_abs) if fits_abs.exists() else "MISSING"

    parts = [
        commit,
        orch_sha,
        config_sha,
        filters_sha,
        qe_sha,
        fits_sha,
        GAIA_DATA_DIR,
    ]
    h = hashlib.sha256()
    for p in parts:
        h.update(p.encode("utf-8"))
        h.update(b"|")
    return h.hexdigest().upper()


def cache_path_for(frame_id: str) -> Path:
    """cache/<frame_id>.json"""
    safe = re.sub(r"[^A-Za-z0-9_.\-]", "_", frame_id)
    return CACHE_DIR / f"{safe}.json"


def cache_load(frame_id: str) -> Optional[Dict[str, Any]]:
    p = cache_path_for(frame_id)
    if not p.exists():
        return None
    try:
        with open(p, "r", encoding="utf-8") as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return None


def cache_save(frame_id: str, data: Dict[str, Any]) -> None:
    p = cache_path_for(frame_id)
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_suffix(".tmp")
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    os.replace(tmp, p)


def cache_clear_all() -> int:
    """清空 cache 目录，返回删除文件数"""
    if not CACHE_DIR.exists():
        return 0
    n = 0
    for f in CACHE_DIR.glob("*.json"):
        try:
            f.unlink()
            n += 1
        except OSError:
            pass
    return n


# ============================================================================
# 断点状态（batch_state.json）
# ============================================================================

def state_load() -> BatchState:
    if not BATCH_STATE_FILE.exists():
        return BatchState()
    try:
        with open(BATCH_STATE_FILE, "r", encoding="utf-8") as f:
            obj = json.load(f)
        s = BatchState(
            version=obj.get("version", 1),
            created_at=obj.get("created_at", ""),
            updated_at=obj.get("updated_at", ""),
            git_commit=obj.get("git_commit", ""),
            orchestrator_sha256=obj.get("orchestrator_sha256", ""),
            total_frames=obj.get("total_frames", 0),
            completed=obj.get("completed", 0),
            passed=obj.get("passed", 0),
            failed=obj.get("failed", 0),
            skipped=obj.get("skipped", 0),
            timeouts=obj.get("timeouts", 0),
            errors=obj.get("errors", 0),
            frames=obj.get("frames", {}),
        )
        return s
    except (json.JSONDecodeError, OSError):
        return BatchState()


def state_save(state: BatchState) -> None:
    state.updated_at = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime())
    BATCH_STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
    tmp = BATCH_STATE_FILE.with_suffix(".tmp")
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(asdict(state), f, ensure_ascii=False, indent=2)
    os.replace(tmp, BATCH_STATE_FILE)


def state_clear() -> None:
    if BATCH_STATE_FILE.exists():
        BATCH_STATE_FILE.unlink()


def state_update_frame(state: BatchState, frame_id: str, result: FrameResult) -> None:
    """更新单帧状态到 batch_state.json"""
    state.frames[frame_id] = {
        "status": result.status,
        "exit_code": result.exit_code,
        "elapsed_s": round(result.elapsed_s, 3),
        "hash_key": result.hash_key,
        "cache_hit": result.cache_hit,
        "completed_at": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()),
        "failure_category": result.failure_category,
        "fit_used": result.fit_used,
        "scale_factor": result.scale_factor,
        "sigma_residual": result.sigma_residual,
        "has_snr": result.has_snr,
        "snr_n_points": result.snr_n_points,
    }
    # 重算汇总
    state.completed = sum(1 for v in state.frames.values() if v.get("status") in ("PASS", "FAIL", "STAGE1_ERROR", "TIMEOUT", "SKIPPED"))
    state.passed = sum(1 for v in state.frames.values() if v.get("status") == "PASS")
    state.failed = sum(1 for v in state.frames.values() if v.get("status") == "FAIL")
    state.skipped = sum(1 for v in state.frames.values() if v.get("status") == "SKIPPED")
    state.timeouts = sum(1 for v in state.frames.values() if v.get("status") == "TIMEOUT")
    state.errors = sum(1 for v in state.frames.values() if v.get("status") == "STAGE1_ERROR")
    state_save(state)


# ============================================================================
# Runner：调用 orchestrator stage1
# ============================================================================

def make_default_state(total_frames: int, commit: str, orch_sha: str) -> BatchState:
    return BatchState(
        version=1,
        created_at=time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()),
        updated_at=time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()),
        git_commit=commit,
        orchestrator_sha256=orch_sha,
        total_frames=total_frames,
    )


def run_single_frame(rec: FrameRecord, commit: str, orch_sha: str,
                     timeout_s: int, force: bool = False,
                     state: Optional[BatchState] = None) -> FrameResult:
    """对单帧运行 orchestrator stage1，带缓存/断点"""
    result = FrameResult(
        frame_id=rec.frame_id,
        dataset=rec.dataset,
        device=rec.device,
        target=rec.target,
        filter_canonical=rec.filter_canonical,
        filter_class=filter_class_of(rec.filter_canonical),
        frame_name=rec.frame_name,
        fits_path=rec.fits_path,
        status="PENDING",
    )
    result.hash_key = compute_frame_hash_key(rec, commit, orch_sha)

    fits_abs = PROJECT_ROOT / rec.fits_path
    if not fits_abs.exists():
        result.status = "STAGE1_ERROR"
        result.exit_code = -1
        result.failure_category = "STAGE1_ERROR"
        result.notes = f"FITS 文件不存在: {fits_abs}"
        return result

    # 缓存检查（非 force 模式）
    if not force:
        cached = cache_load(rec.frame_id)
        if cached and cached.get("hash_key") == result.hash_key and cached.get("status") == "PASS":
            # 缓存命中，跳过执行
            result.status = "SKIPPED"
            result.cache_hit = True
            result.exit_code = cached.get("exit_code", 0)
            result.elapsed_s = cached.get("elapsed_s", 0.0)
            result.fit_used = cached.get("fit_used", 0)
            result.scale_factor = cached.get("scale_factor", 0.0)
            result.sigma_residual = cached.get("sigma_residual", 0.0)
            result.unique_matches = cached.get("unique_matches", 0)
            result.n_matched = cached.get("n_matched", 0)
            result.has_snr = cached.get("has_snr", 0)
            result.snr_n_points = cached.get("snr_n_points", 0)
            result.hiss_path = cached.get("hiss_path", "")
            result.hiss_sha256 = cached.get("hiss_sha256", "")
            result.notes = "cache hit (hash 匹配，跳过)"
            return result

    # 输出目录（用 hash_key 前 16 字符；frame_id 可达 80 字符，直接做目录名会使 .hiss 路径超过 Windows MAX_PATH=260，导致 hio_fopen_utf8 创建文件失败 rc=-2）
    out_dir = RAW_LOGS_DIR / result.hash_key[:16]
    out_dir.mkdir(parents=True, exist_ok=True)
    hiss_path = out_dir / f"{rec.frame_id}.hiss"
    log_file = out_dir / "stage1.log"
    (out_dir / "frame_id.txt").write_text(rec.frame_id, encoding="utf-8")

    config_path = DEVICE_CONFIG.get(rec.device)
    if not config_path or not config_path.exists():
        result.status = "STAGE1_ERROR"
        result.exit_code = -1
        result.failure_category = "STAGE1_ERROR"
        result.notes = f"stage1_config_{rec.device}.json 不存在"
        return result

    cmd = [
        str(ORCH_EXE),
        "stage1",
        "--frame", str(fits_abs),
        "--output", str(hiss_path),
        "--gaia-data", GAIA_DATA_DIR,
        "--filter", rec.filter_alias,
        "--config", str(config_path),
        "--log-level", "INFO",
    ]

    print(f"  [{rec.frame_id}] 运行 stage1 ...", flush=True)

    t0 = time.time()
    try:
        with open(log_file, "w", encoding="utf-8", errors="replace") as lf:
            lf.write("=== COMMAND ===\n")
            lf.write(" ".join(cmd) + "\n")
            lf.write(f"\n=== HASH_KEY ===\n{result.hash_key}\n")
            lf.write(f"\n=== TIMEOUT ===\n{timeout_s}s\n\n")
            lf.flush()
            proc = subprocess.run(
                cmd,
                cwd=str(PROJECT_ROOT),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=timeout_s,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
            lf.write("=== EXIT CODE ===\n")
            lf.write(str(proc.returncode) + "\n")
            lf.write("\n=== STDOUT ===\n")
            lf.write(proc.stdout or "")
            lf.write("\n=== STDERR ===\n")
            lf.write(proc.stderr or "")
        result.exit_code = proc.returncode
        result.elapsed_s = time.time() - t0

        if proc.returncode != 0:
            result.status = "STAGE1_ERROR"
            err_tail = (proc.stderr or "").strip().splitlines()
            err_msg = err_tail[-1] if err_tail else f"exit_code={proc.returncode}"
            result.failure_category = "STAGE1_ERROR"
            result.notes = f"exit_code={proc.returncode}: {err_msg[:200]}"
        else:
            result.status = "PASS"  # 暂定，下面 Gate 检查

        # 解析 stdout 拿 photo_stats
        photo_stats = parse_stage1_stdout(proc.stdout or "")
        result.spectrum_rows_total = to_int(photo_stats.get("SPECTRUM_ROWS_TOTAL"))
        result.valid_fsyn = to_int(photo_stats.get("VALID_FSYN"))
        result.psf_valid = to_int(photo_stats.get("PSF_VALID"))
        result.fit_used = to_int(photo_stats.get("FIT_USED"))
        result.scale_factor = to_float(photo_stats.get("SCALE_FACTOR"))
        result.sigma_residual = to_float(photo_stats.get("SIGMA_RESIDUAL"))
        result.unique_matches = to_int(photo_stats.get("UNIQUE_MATCHES"))
        result.n_matched = to_int(photo_stats.get("N_MATCHED"))

        # HISS 路径
        result.hiss_path = photo_stats.get("_output_ahpx_path", str(hiss_path))
        if Path(result.hiss_path).exists():
            result.hiss_sha256 = sha256_file(Path(result.hiss_path))
            # 简单 inspect：has_snr + n_points（避免依赖 aio_healpix_io）
            try:
                import struct
                import zstandard as zstd
                with open(result.hiss_path, "rb") as hf:
                    magic = hf.read(4)
                    if magic == b"HISS":
                        uncomp_len = struct.unpack("<I", hf.read(4))[0]
                        comp_len = struct.unpack("<I", hf.read(4))[0]
                        comp_json = hf.read(comp_len)
                        json_str = zstd.ZstdDecompressor().decompress(comp_json, uncomp_len).decode("utf-8")
                        header = json.loads(json_str)
                        result.has_snr = 1 if header.get("has_snr", False) else 0
                        result.snr_n_points = int(header.get("snr_n_points", 0))
            except Exception as e:
                result.notes = (result.notes + f" | HISS inspect 失败: {e}").strip(" |")

        # Gate 检查（与 P12-004 一致）
        if result.status == "PASS":
            gate_pass, cat, notes = classify_gate(result)
            if not gate_pass:
                result.status = "FAIL"
                result.failure_category = cat
                result.notes = (result.notes + (" | " if result.notes else "") + notes).strip(" |")

        print(f"  [{rec.frame_id}] exit={result.exit_code} elapsed={result.elapsed_s:.1f}s "
              f"fit_used={result.fit_used} scale={result.scale_factor:.4g} "
              f"sigma={result.sigma_residual:.4g} has_snr={result.has_snr} → {result.status}",
              flush=True)

    except subprocess.TimeoutExpired:
        result.status = "TIMEOUT"
        result.elapsed_s = timeout_s
        result.exit_code = -1
        result.failure_category = "TIMEOUT"
        result.notes = f"超时 (>{timeout_s}s)"
        print(f"  [{rec.frame_id}] TIMEOUT (>{timeout_s}s)", flush=True)
    except Exception as e:
        result.status = "STAGE1_ERROR"
        result.elapsed_s = time.time() - t0
        result.exit_code = -2
        result.failure_category = "STAGE1_ERROR"
        result.notes = f"异常: {type(e).__name__}: {e}"
        print(f"  [{rec.frame_id}] 异常: {e}", flush=True)

    # 写入缓存（无论 PASS/FAIL，都记录 hash_key + 结果，便于后续审计）
    # 但仅 PASS 才会被后续跳过；FAIL/STAGE1_ERROR/TIMEOUT 仍会重跑
    cache_data = {
        "frame_id": result.frame_id,
        "hash_key": result.hash_key,
        "status": result.status,
        "exit_code": result.exit_code,
        "elapsed_s": round(result.elapsed_s, 3),
        "fit_used": result.fit_used,
        "scale_factor": result.scale_factor,
        "sigma_residual": result.sigma_residual,
        "unique_matches": result.unique_matches,
        "n_matched": result.n_matched,
        "has_snr": result.has_snr,
        "snr_n_points": result.snr_n_points,
        "hiss_path": result.hiss_path,
        "hiss_sha256": result.hiss_sha256,
        "failure_category": result.failure_category,
        "notes": result.notes,
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()),
    }
    cache_save(result.frame_id, cache_data)

    # 更新 batch_state
    if state is not None:
        state_update_frame(state, result.frame_id, result)

    return result


def classify_gate(r: FrameResult) -> Tuple[bool, str, str]:
    """Gate 检查（与 P12-004 一致）

    Returns: (gate_pass, failure_category, notes)
    """
    if r.filter_canonical in BROADBAND_FILTERS:
        fit_threshold = 20
    elif r.filter_canonical in NARROWBAND_FILTERS:
        fit_threshold = 8
    else:
        return False, "UNKNOWN_FILTER", f"未知滤镜类别: {r.filter_canonical}"

    if r.fit_used < fit_threshold:
        return False, "INSUFFICIENT_STARS", f"fit_used={r.fit_used} < 阈值 {fit_threshold} ({r.filter_class})"

    if not (r.sigma_residual == r.sigma_residual and r.sigma_residual > 0.0):
        return False, "ZERO_SIGMA", f"sigma_residual={r.sigma_residual} (非正或非有限)"

    if not (r.scale_factor > 0.0):
        return False, "INVALID_SCALE", f"scale_factor={r.scale_factor} 必须 > 0"

    return True, "", ""


# ============================================================================
# 过滤器
# ============================================================================

def filter_frames(frames: List[FrameRecord], args: argparse.Namespace) -> List[FrameRecord]:
    """按 CLI 参数过滤帧列表"""
    out = frames
    if getattr(args, "device", None):
        devices = set(d.upper() for d in args.device)
        out = [f for f in out if f.device.upper() in devices]
    if getattr(args, "target", None):
        targets = set(t.lower() for t in args.target)
        out = [f for f in out if f.target.lower() in targets]
    if getattr(args, "filter", None):
        filts = set(fl.upper() for fl in args.filter)
        out = [f for f in out if f.filter_canonical.upper() in filts]
    if getattr(args, "dataset", None):
        datasets = set(d for d in args.dataset)
        out = [f for f in out if f.dataset in datasets]
    if getattr(args, "limit", None) and args.limit > 0:
        out = out[: args.limit]
    return out


# ============================================================================
# 报告生成
# ============================================================================

def write_batch_results_csv(results: List[FrameResult], path: Path) -> None:
    columns = [
        "frame_id", "dataset", "device", "target", "filter_canonical", "filter_class",
        "frame_name", "panel", "fits_path",
        "status", "exit_code", "elapsed_s", "cache_hit",
        "fit_used", "scale_factor", "sigma_residual", "unique_matches", "n_matched",
        "has_snr", "snr_n_points", "hiss_sha256",
        "failure_category", "notes",
    ]
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(columns)
        for r in results:
            # panel 需要从 frame_id 之外拿，但 FrameResult 没存 panel，从 fits_path 推
            panel = ""
            if "/panel" in r.fits_path.replace("\\", "/"):
                m = re.search(r"panel(\d)", r.fits_path)
                panel = f"panel{m.group(1)}" if m else ""
            w.writerow([
                r.frame_id, r.dataset, r.device, r.target,
                r.filter_canonical, r.filter_class, r.frame_name, panel, r.fits_path,
                r.status, r.exit_code, f"{r.elapsed_s:.3f}", r.cache_hit,
                r.fit_used, f"{r.scale_factor:.6g}", f"{r.sigma_residual:.6g}",
                r.unique_matches, r.n_matched,
                r.has_snr, r.snr_n_points, r.hiss_sha256,
                r.failure_category, r.notes,
            ])


def write_batch_summary_json(results: List[FrameResult], path: Path, commit: str, orch_sha: str) -> None:
    by_status: Dict[str, int] = {}
    by_category: Dict[str, int] = {}
    by_device: Dict[str, Dict[str, int]] = {}
    by_dataset: Dict[str, Dict[str, int]] = {}
    by_filter: Dict[str, Dict[str, int]] = {}

    for r in results:
        by_status[r.status] = by_status.get(r.status, 0) + 1
        cat = r.failure_category or ("PASS" if r.status == "PASS" else r.status)
        by_category[cat] = by_category.get(cat, 0) + 1

        for key, bucket in ((r.device, by_device), (r.dataset, by_dataset), (r.filter_canonical, by_filter)):
            if key not in bucket:
                bucket[key] = {"total": 0, "pass": 0, "fail": 0, "error": 0, "timeout": 0, "skipped": 0}
            bucket[key]["total"] += 1
            if r.status == "PASS":
                bucket[key]["pass"] += 1
            elif r.status == "FAIL":
                bucket[key]["fail"] += 1
            elif r.status == "STAGE1_ERROR":
                bucket[key]["error"] += 1
            elif r.status == "TIMEOUT":
                bucket[key]["timeout"] += 1
            elif r.status == "SKIPPED":
                bucket[key]["skipped"] += 1

    summary = {
        "_description": "P13-001 Stage1 批处理汇总",
        "_generated_at": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()),
        "_hash_binding": {
            "git_commit": commit,
            "orchestrator_sha256": orch_sha,
            "hash_components": [
                "git_commit_hash", "orchestrator.exe_sha256",
                "stage1_config_<device>.json_sha256", "filters.json_sha256",
                "qe_curves.json_sha256", "input_fits_sha256", "gaia_data_dir_name",
            ],
        },
        "total_frames": len(results),
        "by_status": by_status,
        "by_failure_category": by_category,
        "by_device": by_device,
        "by_dataset": by_dataset,
        "by_filter": by_filter,
        "pass_rate_pct": round(
            by_status.get("PASS", 0) / max(len(results), 1) * 100, 2
        ),
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)


def write_failure_classification_json(results: List[FrameResult], path: Path) -> None:
    failed = [r for r in results if r.status in ("FAIL", "STAGE1_ERROR", "TIMEOUT")]
    obj = {
        "_description": "P13-001 失败帧分类",
        "_categories": [
            "INSUFFICIENT_STARS - fit_used 不足",
            "ZERO_SIGMA - sigma_residual 非正",
            "INVALID_SCALE - scale_factor <= 0",
            "STAGE1_ERROR - orchestrator 运行失败",
            "TIMEOUT - 超时",
        ],
        "total_failed": len(failed),
        "total_passed": sum(1 for r in results if r.status == "PASS"),
        "total_skipped": sum(1 for r in results if r.status == "SKIPPED"),
        "failures": [
            {
                "frame_id": r.frame_id,
                "device": r.device,
                "target": r.target,
                "filter_canonical": r.filter_canonical,
                "frame_name": r.frame_name,
                "category": r.failure_category,
                "status": r.status,
                "exit_code": r.exit_code,
                "fit_used": r.fit_used,
                "scale_factor": r.scale_factor,
                "sigma_residual": r.sigma_residual,
                "elapsed_s": round(r.elapsed_s, 3),
                "notes": r.notes,
            }
            for r in failed
        ],
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)


def generate_reports(results: List[FrameResult], commit: str, orch_sha: str) -> Dict[str, Path]:
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)
    csv_path = REPORTS_DIR / "batch_results.csv"
    json_path = REPORTS_DIR / "batch_summary.json"
    fail_path = REPORTS_DIR / "failure_classification.json"
    write_batch_results_csv(results, csv_path)
    write_batch_summary_json(results, json_path, commit, orch_sha)
    write_failure_classification_json(results, fail_path)
    return {"csv": csv_path, "json": json_path, "fail": fail_path}


# ============================================================================
# CLI 子命令
# ============================================================================

def cmd_scan(args: argparse.Namespace) -> int:
    """扫描 testdata 得到帧清单"""
    frames = scan_testdata()
    print(f"[scan] testdata 共 {len(frames)} 帧")
    by_device: Dict[str, int] = {}
    by_dataset: Dict[str, int] = {}
    by_filter: Dict[str, int] = {}
    for f in frames:
        by_device[f.device] = by_device.get(f.device, 0) + 1
        by_dataset[f.dataset] = by_dataset.get(f.dataset, 0) + 1
        by_filter[f.filter_canonical] = by_filter.get(f.filter_canonical, 0) + 1
    print("  按设备:", by_device)
    print("  按数据集:", by_dataset)
    print("  按滤镜:", by_filter)

    # 落盘
    out = REPORTS_DIR / "frame_inventory.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump({
            "_description": "P13-001 testdata 帧清单",
            "_generated_at": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()),
            "total_frames": len(frames),
            "by_device": by_device,
            "by_dataset": by_dataset,
            "by_filter": by_filter,
            "frames": [fr.to_dict() for fr in frames],
        }, f, ensure_ascii=False, indent=2)
    print(f"  清单已写入: {out}")
    return 0


def cmd_run(args: argparse.Namespace) -> int:
    """运行批处理（默认断点恢复）"""
    # 前置检查
    if not ORCH_EXE.exists():
        print(f"[ERROR] orchestrator.exe 不存在: {ORCH_EXE}")
        return 2
    for dev, cfg in DEVICE_CONFIG.items():
        if not cfg.exists():
            print(f"[ERROR] {cfg} 不存在")
            return 2

    commit = git_commit_hash()
    orch_sha = sha256_file(ORCH_EXE)

    if args.fresh:
        # 清空 cache 和 state
        n = cache_clear_all()
        state_clear()
        print(f"[run] --fresh: 已清空 cache ({n} 文件) + batch_state.json")

    frames = scan_testdata()
    frames = filter_frames(frames, args)
    if not frames:
        print("[run] 过滤后无帧可运行")
        return 1

    print(f"[run] 共 {len(frames)} 帧待处理")
    print(f"  git_commit: {commit}")
    print(f"  orchestrator_sha256: {orch_sha[:16]}...")
    print(f"  timeout: {args.timeout}s/帧")
    print(f"  dry_run: {args.dry_run}")

    # 加载或初始化 batch_state
    state = state_load()
    if not state.created_at or state.git_commit != commit or state.orchestrator_sha256 != orch_sha:
        # 新批次或环境变更，重置 state（保留 cache）
        state = make_default_state(len(frames), commit, orch_sha)
        state_save(state)
        print("[run] 初始化 batch_state.json (新批次或环境变更)")
    else:
        if state.total_frames != len(frames):
            old_total = state.total_frames
            state.total_frames = len(frames)
            state_save(state)
            print(f"[run] 更新 total_frames={len(frames)} (原值 {old_total}，cache 保留)")
        print(f"[run] 加载已有 batch_state.json: {state.completed}/{state.total_frames} 已完成")

    results: List[FrameResult] = []
    t_batch_start = time.time()

    for i, rec in enumerate(frames, 1):
        print(f"\n=== [{i}/{len(frames)}] {rec.frame_id} ===", flush=True)

        if args.dry_run:
            # 干跑：仅打印，不执行
            r = FrameResult(
                frame_id=rec.frame_id, dataset=rec.dataset, device=rec.device,
                target=rec.target, filter_canonical=rec.filter_canonical,
                filter_class=filter_class_of(rec.filter_canonical),
                frame_name=rec.frame_name, fits_path=rec.fits_path,
                status="PENDING", hash_key=compute_frame_hash_key(rec, commit, orch_sha),
            )
            r.status = "SKIPPED"
            r.notes = "dry_run (未执行)"
            results.append(r)
            continue

        r = run_single_frame(rec, commit, orch_sha, args.timeout, force=args.fresh, state=state)
        results.append(r)

        # 中途进度保存
        if i % 5 == 0 or i == len(frames):
            elapsed = time.time() - t_batch_start
            done = i
            print(f"\n[progress] {done}/{len(frames)} ({done/len(frames)*100:.1f}%) "
                  f"batch_elapsed={elapsed:.1f}s", flush=True)

    # 生成报告
    reports = generate_reports(results, commit, orch_sha)

    # 汇总打印
    elapsed = time.time() - t_batch_start
    by_status: Dict[str, int] = {}
    for r in results:
        by_status[r.status] = by_status.get(r.status, 0) + 1
    print("\n" + "=" * 70)
    print(f"[run] 批处理完成: {len(results)} 帧, 总耗时 {elapsed:.1f}s")
    print(f"  by_status: {by_status}")
    print(f"  reports:")
    for k, v in reports.items():
        print(f"    {k}: {v}")

    # 返回码：全部 PASS 或 SKIPPED → 0；有 FAIL/ERROR/TIMEOUT → 1
    has_failure = any(r.status in ("FAIL", "STAGE1_ERROR", "TIMEOUT") for r in results)
    return 1 if has_failure else 0


def cmd_status(args: argparse.Namespace) -> int:
    """查看批处理进度"""
    if not BATCH_STATE_FILE.exists():
        print("[status] batch_state.json 不存在")
        return 1
    state = state_load()
    print(f"[status] batch_state.json")
    print(f"  version: {state.version}")
    print(f"  created_at: {state.created_at}")
    print(f"  updated_at: {state.updated_at}")
    print(f"  git_commit: {state.git_commit}")
    print(f"  orchestrator_sha256: {state.orchestrator_sha256[:16]}...")
    print(f"  total_frames: {state.total_frames}")
    print(f"  completed: {state.completed}")
    print(f"  passed: {state.passed}")
    print(f"  failed: {state.failed}")
    print(f"  skipped: {state.skipped}")
    print(f"  timeouts: {state.timeouts}")
    print(f"  errors: {state.errors}")
    if state.total_frames > 0:
        pct = state.completed / state.total_frames * 100
        print(f"  progress: {pct:.1f}%")
    return 0


def cmd_report(args: argparse.Namespace) -> int:
    """从 batch_state.json 重新生成报告"""
    if not BATCH_STATE_FILE.exists():
        print("[report] batch_state.json 不存在")
        return 1
    state = state_load()

    # 从 cache 重建 results
    results: List[FrameResult] = []
    for frame_id, fdata in state.frames.items():
        cached = cache_load(frame_id) or {}
        r = FrameResult(
            frame_id=frame_id,
            dataset=cached.get("dataset", ""),
            device=cached.get("device", ""),
            target=cached.get("target", ""),
            filter_canonical=cached.get("filter_canonical", ""),
            filter_class=cached.get("filter_class", ""),
            frame_name=cached.get("frame_name", ""),
            fits_path=cached.get("fits_path", ""),
            status=fdata.get("status", "UNKNOWN"),
            exit_code=fdata.get("exit_code", -1),
            elapsed_s=fdata.get("elapsed_s", 0.0),
            cache_hit=fdata.get("cache_hit", False),
            fit_used=fdata.get("fit_used", 0),
            scale_factor=fdata.get("scale_factor", 0.0),
            sigma_residual=fdata.get("sigma_residual", 0.0),
            unique_matches=cached.get("unique_matches", 0),
            n_matched=cached.get("n_matched", 0),
            has_snr=fdata.get("has_snr", 0),
            snr_n_points=fdata.get("snr_n_points", 0),
            hiss_sha256=cached.get("hiss_sha256", ""),
            failure_category=fdata.get("failure_category", ""),
        )
        results.append(r)

    reports = generate_reports(results, state.git_commit, state.orchestrator_sha256)
    print(f"[report] 从 batch_state.json 重新生成 {len(results)} 帧报告")
    for k, v in reports.items():
        print(f"  {k}: {v}")
    return 0


def cmd_cache_list(args: argparse.Namespace) -> int:
    """列出 cache 目录"""
    if not CACHE_DIR.exists():
        print("[cache-list] cache 目录不存在")
        return 0
    files = sorted(CACHE_DIR.glob("*.json"))
    print(f"[cache-list] {len(files)} 个缓存文件")
    total_size = 0
    for f in files:
        sz = f.stat().st_size
        total_size += sz
        print(f"  {f.name}  {sz}B")
    print(f"  total: {total_size}B")
    return 0


def cmd_cache_clear(args: argparse.Namespace) -> int:
    """清空 cache 目录"""
    n = cache_clear_all()
    print(f"[cache-clear] 删除 {n} 个缓存文件")
    return 0


# ============================================================================
# CLI 入口
# ============================================================================

def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        prog="stage1_batch_runner.py",
        description="P13-001 — Stage1 全 TestData 批处理与恢复入口",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例：
  scan                        扫描 testdata 得到帧清单
  run --limit 5               小规模冒烟测试（前 5 帧）
  run --device T4             仅运行 T4 设备帧
  run --target Galaxy_Center  仅运行 Galaxy_Center 数据集帧
  run --filter HA             仅运行 HA 滤镜帧
  run --fresh                 清空 cache + state 重跑
  run --dry-run               仅打印 hash_key 不执行
  run --timeout 900            单帧超时设为 900s
  status                      查看批处理进度
  report                      从 batch_state.json 重新生成报告
  cache-list                  列出 cache 目录
  cache-clear                 清空 cache 目录

缓存策略（禁止捷径）：
  每帧缓存绑定 7 元 hash:
    git_commit + orchestrator.exe_sha256 + stage1_config_<device>.json_sha256
    + filters.json_sha256 + qe_curves.json_sha256 + input_fits_sha256
    + gaia_data_dir_name
  任一变更则该帧缓存失效，强制重跑。
  仅 PASS 帧会被跳过；FAIL/STAGE1_ERROR/TIMEOUT 仍会重跑。
""",
    )
    sub = ap.add_subparsers(dest="cmd", required=True, metavar="<command>")

    p_scan = sub.add_parser("scan", help="扫描 testdata 得到帧清单")
    p_scan.set_defaults(func=cmd_scan)

    p_run = sub.add_parser("run", help="运行批处理（默认断点恢复）")
    p_run.add_argument("--device", nargs="+", help="按设备过滤 (T2 T3 T4)")
    p_run.add_argument("--target", nargs="+", help="按目标过滤 (Galaxy_Center NGC55 ...)")
    p_run.add_argument("--filter", nargs="+", help="按滤镜过滤 (LUM RED GREEN BLUE HA OIII)")
    p_run.add_argument("--dataset", nargs="+", help="按数据集目录过滤")
    p_run.add_argument("--limit", type=int, default=0, help="限制帧数 (0=全部)")
    p_run.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT_S, help=f"单帧超时秒数 (默认 {DEFAULT_TIMEOUT_S})")
    p_run.add_argument("--fresh", action="store_true", help="清空 cache + state 重跑")
    p_run.add_argument("--dry-run", action="store_true", help="仅打印 hash_key 不执行")
    p_run.set_defaults(func=cmd_run)

    p_status = sub.add_parser("status", help="查看批处理进度")
    p_status.set_defaults(func=cmd_status)

    p_report = sub.add_parser("report", help="从 batch_state.json 重新生成报告")
    p_report.set_defaults(func=cmd_report)

    p_clist = sub.add_parser("cache-list", help="列出 cache 目录")
    p_clist.set_defaults(func=cmd_cache_list)

    p_cclear = sub.add_parser("cache-clear", help="清空 cache 目录")
    p_cclear.set_defaults(func=cmd_cache_clear)

    return ap


def main(argv: Optional[List[str]] = None) -> int:
    ap = build_parser()
    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
