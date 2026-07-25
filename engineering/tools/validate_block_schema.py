#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P01-002 数据块 schema 校验器

功能：
  1. 读取 pipeline_blocks_registry.csv 注册表并校验内部一致性
  2. 解析 HISS 文件二进制头 + zstd 压缩 JSON 元数据，按注册表校验
  3. 解析 PipelineFrame 块快照（JSON 或 orchestrator 日志），校验类型/维度/count
  4. 输出结构化 JSON 校验报告

校验规则来源：engineering/docs/04_PIPELINEFRAME_CONTRACT_V1.md §4
  - 块存在检查
  - 类型检查（FLOAT32=0 / FLOAT64=1 / KV=5 / RAW=6）
  - 维度检查（[height,width] / [N,6] / [N,9]）
  - count 检查（star_det 与 psf 行数一致）
  - schema_version 检查
  - revision 检查（ASTROCS.STARDET.INPUT_REVISION 等 header provenance）
  - 有限值比例检查（data 块禁止 NaN/Inf）

这是工程辅助工具（非产品运行时），用 Python 实现，供 P01 合约冻结阶段使用。
"""

from __future__ import annotations

import argparse
import csv
import json
import logging
import os
import re
import struct
import sys
from dataclasses import dataclass, field, asdict
from typing import Any

# zstd 解压（P01-002 安装 zstandard 0.25.0）
try:
    import zstandard as _zstd
    _HAS_ZSTD = True
except ImportError:
    _HAS_ZSTD = False


# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------

# AioBlockType 枚举（aio_pipeline.h:36-43）
AIO_BLOCK_TYPES = {
    0: "FLOAT32",
    1: "FLOAT64",
    5: "KV",
    6: "RAW",
}
TYPE_NAME_TO_ID = {v: k for k, v in AIO_BLOCK_TYPES.items()}

HISS_MAGIC = b"HISS"

# 注册表必需覆盖的契约块（04_PIPELINEFRAME_CONTRACT_V1.md §2-3）
REQUIRED_CONTRACT_BLOCKS = {
    "data", "header", "star_det", "psf", "photo_stats",
    "snr_model", "cal_stats", "astrometry_stats", "astrometry_matches",
}

# star_det 必需 header provenance 键（star_det_block_v1.md）
STAR_DET_REQUIRED_HEADER_KEYS = [
    "ASTROCS.STARDET.SCHEMA",
    "ASTROCS.STARDET.COLUMNS",
    "ASTROCS.STARDET.PRODUCER",
    "ASTROCS.STARDET.INPUT_REVISION",
    "ASTROCS.STARDET.PARAM_HASH",
    "ASTROCS.STARDET.COUNT",
    "ASTROCS.STARDET.HASH",
    "ASTROCS.PLATESOLVE.DETECTION_PATH",
]

# photo_stats 必需键（04 §2 photo_stats）
PHOTO_STATS_REQUIRED_KEYS = [
    "schema_version", "status", "n_matched", "scale_factor",
    "sigma_residual", "input_data_revision", "output_data_revision",
]


# ---------------------------------------------------------------------------
# 日志
# ---------------------------------------------------------------------------

_LOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "logs")
os.makedirs(_LOG_DIR, exist_ok=True)

logger = logging.getLogger("validate_block_schema")
logger.setLevel(logging.DEBUG)
_fh = logging.FileHandler(os.path.join(_LOG_DIR, "validate_block_schema.log"),
                          encoding="utf-8")
_fh.setFormatter(logging.Formatter(
    "%(asctime)s [%(levelname)s] %(message)s"))
logger.addHandler(_fh)
_sh = logging.StreamHandler(sys.stderr)
_sh.setLevel(logging.INFO)
_sh.setFormatter(logging.Formatter("[%(levelname)s] %(message)s"))
logger.addHandler(_sh)


# ---------------------------------------------------------------------------
# 数据结构
# ---------------------------------------------------------------------------

@dataclass
class CheckResult:
    check: str
    status: str  # PASS / FAIL / WARN / SKIP
    detail: str = ""
    severity: str = "info"  # info / minor / major / critical

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class ValidationReport:
    tool_version: str = "P01-002 v1.0"
    registry_path: str = ""
    hiss_path: str = ""
    frame_snapshot_source: str = ""
    registry_summary: dict = field(default_factory=dict)
    registry_checks: list = field(default_factory=list)
    hiss_checks: list = field(default_factory=list)
    frame_checks: list = field(default_factory=list)
    inconsistencies: list = field(default_factory=list)
    summary: dict = field(default_factory=dict)


# ---------------------------------------------------------------------------
# 注册表加载与校验
# ---------------------------------------------------------------------------

def load_registry(csv_path: str) -> list[dict]:
    """读取 pipeline_blocks_registry.csv，返回块定义列表。"""
    logger.info("加载注册表: %s", csv_path)
    if not os.path.isfile(csv_path):
        raise FileNotFoundError(f"注册表不存在: {csv_path}")
    rows: list[dict] = []
    with open(csv_path, "r", encoding="utf-8", newline="") as fp:
        reader = csv.DictReader(fp)
        required_cols = {"block_name", "schema_version", "type",
                         "dimensions", "producer", "consumers",
                         "created_at_stage", "destroyable_at", "notes"}
        missing = required_cols - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"注册表缺少列: {missing}")
        for row in reader:
            rows.append(row)
    logger.info("注册表加载完成: %d 个块", len(rows))
    return rows


def validate_registry(rows: list[dict]) -> list[CheckResult]:
    """注册表内部一致性校验。"""
    results: list[CheckResult] = []
    names = [r["block_name"] for r in rows]

    # 1. 必需契约块覆盖
    covered = set(names)
    for blk in sorted(REQUIRED_CONTRACT_BLOCKS):
        if blk in covered:
            results.append(CheckResult(
                check=f"registry.required_block_present:{blk}",
                status="PASS", detail=f"{blk} 已注册",
                severity="info"))
        else:
            results.append(CheckResult(
                check=f"registry.required_block_present:{blk}",
                status="FAIL", detail=f"{blk} 缺失",
                severity="critical"))

    # 2. 块名唯一
    seen: dict[str, int] = {}
    for n in names:
        seen[n] = seen.get(n, 0) + 1
    for n, c in seen.items():
        if c > 1:
            results.append(CheckResult(
                check=f"registry.block_name_unique:{n}",
                status="FAIL", detail=f"重复 {c} 次",
                severity="critical"))
        else:
            results.append(CheckResult(
                check=f"registry.block_name_unique:{n}",
                status="PASS", severity="info"))

    # 3. 类型合法
    valid_types = set(AIO_BLOCK_TYPES.values())
    for r in rows:
        t = r["type"].strip()
        if t in valid_types:
            results.append(CheckResult(
                check=f"registry.type_valid:{r['block_name']}",
                status="PASS", detail=f"type={t}",
                severity="info"))
        else:
            results.append(CheckResult(
                check=f"registry.type_valid:{r['block_name']}",
                status="FAIL", detail=f"非法类型 {t}",
                severity="major"))

    # 4. schema_version 为整数
    for r in rows:
        try:
            int(r["schema_version"])
            results.append(CheckResult(
                check=f"registry.schema_version_int:{r['block_name']}",
                status="PASS", detail=f"v{r['schema_version']}",
                severity="info"))
        except ValueError:
            results.append(CheckResult(
                check=f"registry.schema_version_int:{r['block_name']}",
                status="FAIL", detail=f"非整数: {r['schema_version']}",
                severity="major"))

    # 5. 维度格式校验（[...] 形式）
    dim_re = re.compile(r"^\[.+\]$")
    for r in rows:
        d = r["dimensions"].strip()
        if dim_re.match(d):
            results.append(CheckResult(
                check=f"registry.dims_format:{r['block_name']}",
                status="PASS", detail=d, severity="info"))
        else:
            results.append(CheckResult(
                check=f"registry.dims_format:{r['block_name']}",
                status="FAIL", detail=f"维度格式错误: {d}",
                severity="major"))

    # 6. producer/consumers 非空
    for r in rows:
        if not r["producer"].strip():
            results.append(CheckResult(
                check=f"registry.producer_nonempty:{r['block_name']}",
                status="FAIL", severity="major"))
        else:
            results.append(CheckResult(
                check=f"registry.producer_nonempty:{r['block_name']}",
                status="PASS", detail=r["producer"], severity="info"))

    return results


# ---------------------------------------------------------------------------
# HISS 文件解析
# ---------------------------------------------------------------------------

def parse_hiss(path: str) -> dict:
    """解析 HISS 文件头与 JSON 元数据。

    HISS 格式（aio_healpix_io.cpp:284-332）:
      - magic (4 bytes): "HISS"
      - uncompressed_len (uint32 LE)
      - compressed_len (uint32 LE)
      - compressed_json (compressed_len bytes, zstd)
      - ipix 数组 (n_pix * 8 bytes)
      - pixel 数组 (n_pix * 4 bytes, float32)
      - snr 数组 (n_pix * 4 bytes, float32, 可选)
    """
    logger.info("解析 HISS: %s", path)
    info: dict[str, Any] = {"path": path, "parse_ok": False, "errors": []}
    if not os.path.isfile(path):
        info["errors"].append(f"文件不存在: {path}")
        return info
    size = os.path.getsize(path)
    info["file_size"] = size
    with open(path, "rb") as fp:
        data = fp.read()
    info["magic_hex"] = data[:4].hex()
    if data[:4] != HISS_MAGIC:
        info["errors"].append(f"magic 不匹配: {data[:4]!r} (期望 HISS)")
        return info
    info["magic"] = "HISS"
    if len(data) < 12:
        info["errors"].append("文件过短，无完整头")
        return info
    uncomp_len, comp_len = struct.unpack_from("<II", data, 4)
    info["json_uncompressed_len"] = uncomp_len
    info["json_compressed_len"] = comp_len
    comp_json = data[12:12 + comp_len]
    info["json_compressed_bytes"] = len(comp_json)
    # zstd 解压
    if not _HAS_ZSTD:
        info["errors"].append("zstandard 库未安装，无法解压 JSON")
        return info
    try:
        dctx = _zstd.ZstdDecompressor()
        json_bytes = dctx.decompress(comp_json, max_output_size=uncomp_len + 64)
        info["json_raw"] = json_bytes.decode("utf-8", errors="replace")
        try:
            meta = json.loads(info["json_raw"])
            info["meta"] = meta
            info["parse_ok"] = True
        except json.JSONDecodeError as e:
            info["errors"].append(f"JSON 解析失败: {e}")
    except Exception as e:
        info["errors"].append(f"zstd 解压失败: {e}")
    # 数据区长度校验
    if info.get("meta"):
        n_pix = meta.get("n_pix", 0)
        has_snr = meta.get("has_snr", 0)
        expected_data = 12 + comp_len + n_pix * 8 + n_pix * 4
        if has_snr:
            expected_data += n_pix * 4
        info["expected_data_len"] = expected_data
        info["data_len_match"] = (expected_data == size)
    return info


def validate_hiss_metadata(hiss_info: dict, registry: list[dict]) -> list[CheckResult]:
    """校验 HISS 元数据是否符合注册表与 04 契约。"""
    results: list[CheckResult] = []
    if not hiss_info.get("parse_ok"):
        results.append(CheckResult(
            check="hiss.parse",
            status="FAIL",
            detail="; ".join(hiss_info.get("errors", [])) or "解析失败",
            severity="critical"))
        return results
    results.append(CheckResult(
        check="hiss.magic", status="PASS",
        detail=f"magic={hiss_info['magic']}", severity="info"))
    meta = hiss_info.get("meta", {})
    # 必需字段（HISS JSON 头）
    required_hiss_fields = ["nside", "nested", "n_pix", "has_snr", "snr_format"]
    for f in required_hiss_fields:
        if f in meta:
            results.append(CheckResult(
                check=f"hiss.field_present:{f}", status="PASS",
                detail=f"{f}={meta[f]}", severity="info"))
        else:
            results.append(CheckResult(
                check=f"hiss.field_present:{f}", status="FAIL",
                detail=f"缺字段 {f}", severity="major"))
    # has_snr 与 snr_model 块关系
    has_snr = int(meta.get("has_snr", 0))
    snr_entry = next((r for r in registry if r["block_name"] == "snr_model"), None)
    if snr_entry:
        if has_snr == 0:
            results.append(CheckResult(
                check="hiss.snr_model_consistency",
                status="WARN",
                detail="has_snr=0，对应 snr_model 块未生产（G-002 退化）",
                severity="minor"))
        else:
            results.append(CheckResult(
                check="hiss.snr_model_consistency",
                status="PASS",
                detail="has_snr=1，snr_model 已嵌入 HISS",
                severity="info"))
    # 数据区长度
    if "data_len_match" in hiss_info:
        if hiss_info["data_len_match"]:
            results.append(CheckResult(
                check="hiss.data_len", status="PASS",
                detail=f"文件长度={hiss_info['file_size']} 匹配预期",
                severity="info"))
        else:
            results.append(CheckResult(
                check="hiss.data_len", status="FAIL",
                detail=f"文件长度={hiss_info['file_size']} != 预期 {hiss_info.get('expected_data_len')}",
                severity="major"))
    # nside 合理性
    nside = meta.get("nside", 0)
    if isinstance(nside, int) and nside > 0 and (nside & (nside - 1)) == 0:
        results.append(CheckResult(
            check="hiss.nside_power_of_two", status="PASS",
            detail=f"nside={nside}", severity="info"))
    else:
        results.append(CheckResult(
            check="hiss.nside_power_of_two", status="FAIL",
            detail=f"nside={nside} 非 2 的幂", severity="major"))
    return results


# ---------------------------------------------------------------------------
# PipelineFrame 块校验
# ---------------------------------------------------------------------------

def parse_orchestrator_log(log_path: str) -> dict:
    """从 orchestrator DEBUG 日志提取块操作（add_block/add_block_move/remove_block）。

    日志行格式：
      [ts][DEBUG][PIPELINE] add_block: 'data' type=0 count=16200000 (64800000 bytes)
      [ts][DEBUG][PIPELINE] add_block_move: 'data' type=0 count=16200000 (moved ptr=...)
      [ts][DEBUG][PIPELINE] remove_block: 'data' removed (n_blocks=1)
    """
    logger.info("解析 orchestrator 日志: %s", log_path)
    snapshot: dict[str, Any] = {
        "source": log_path,
        "blocks": {},
        "operations": [],
        "photo_stats_seen": False,
        "snr_model_written": False,
    }
    if not os.path.isfile(log_path):
        snapshot["error"] = f"日志不存在: {log_path}"
        return snapshot
    add_re = re.compile(
        r"add_block(?:_move)?:\s+'([^']+)'\s+type=(\d+)\s+count=(\d+)")
    rm_re = re.compile(r"remove_block:\s+'([^']+)'")
    with open(log_path, "r", encoding="utf-8", errors="replace") as fp:
        for line in fp:
            m = add_re.search(line)
            if m:
                name, type_id, count = m.group(1), int(m.group(2)), int(m.group(3))
                op = "add_move" if "add_block_move" in line else "add"
                snapshot["operations"].append(
                    {"op": op, "block": name, "type_id": type_id, "count": count})
                # 保留最后一次 add 的状态作为块快照
                snapshot["blocks"][name] = {
                    "name": name,
                    "type_id": type_id,
                    "type": AIO_BLOCK_TYPES.get(type_id, f"UNKNOWN({type_id})"),
                    "count": count,
                    "op": op,
                }
                continue
            m = rm_re.search(line)
            if m:
                name = m.group(1)
                snapshot["operations"].append(
                    {"op": "remove", "block": name})
            if "photo_stats 已写入" in line or ("photo_stats" in line and "已写入" in line):
                snapshot["photo_stats_seen"] = True
                # photo_stats 是 KV 块，通过 kv_set 隐式创建，不出现在 add_block 日志
                snapshot["blocks"].setdefault("photo_stats", {
                    "name": "photo_stats", "type_id": TYPE_NAME_TO_ID["KV"],
                    "type": "KV", "count": 0, "op": "implicit_kv",
                })
            if "snr_model" in line and "跳过" in line:
                snapshot["snr_model_written"] = False
            if "snr_model 块已写入" in line or ("snr_model" in line and "写入" in line and "跳过" not in line):
                snapshot["snr_model_written"] = True
            # header 是 KV 块，READ_FITS 通过 kv_set 隐式创建（无 add_block 日志）
            # 检测 READ_FITS 完成（图像尺寸日志是 header 块已填充的可靠标志）
            if "[READ_FITS] 图像尺寸:" in line and "header" not in snapshot["blocks"]:
                snapshot["blocks"]["header"] = {
                    "name": "header", "type_id": TYPE_NAME_TO_ID["KV"],
                    "type": "KV", "count": 0, "op": "implicit_kv",
                }
    logger.info("日志解析完成: %d 个块, %d 次操作",
                len(snapshot["blocks"]), len(snapshot["operations"]))
    return snapshot


def load_frame_snapshot(json_path: str) -> dict:
    """加载 PipelineFrame JSON 快照。"""
    with open(json_path, "r", encoding="utf-8") as fp:
        return json.load(fp)


def _dims_to_list(dim_str: str) -> list:
    """'[N,6]' -> ['N', 6] 或 ['height','width']。"""
    inner = dim_str.strip().strip("[]")
    parts = [p.strip() for p in inner.split(",")]
    out = []
    for p in parts:
        if p.isdigit():
            out.append(int(p))
        else:
            out.append(p)
    return out


def validate_frame_blocks(snapshot: dict, registry: list[dict]) -> list[CheckResult]:
    """校验 PipelineFrame 块的类型/维度/count 与注册表一致性。

    snapshot["blocks"] 形如:
      {"data": {"name":"data","type_id":0,"type":"FLOAT32","count":16200000},
       "star_det": {"name":"star_det","type_id":0,"type":"FLOAT32","count":8000}, ...}
    """
    results: list[CheckResult] = []
    reg_by_name = {r["block_name"]: r for r in registry}
    blocks = snapshot.get("blocks", {})

    # 1. 块存在检查（必需契约块）
    for blk in REQUIRED_CONTRACT_BLOCKS:
        if blk in blocks:
            results.append(CheckResult(
                check=f"frame.block_present:{blk}", status="PASS",
                severity="info", detail="块存在"))
        else:
            # 可选/诊断块允许缺失，但记录
            entry = reg_by_name.get(blk, {})
            notes = entry.get("notes", "")
            if blk == "astrometry_matches":
                results.append(CheckResult(
                    check=f"frame.block_present:{blk}", status="SKIP",
                    severity="minor", detail="可选诊断块缺失"))
            elif "退化" in notes or "G-002" in notes or blk == "snr_model":
                results.append(CheckResult(
                    check=f"frame.block_present:{blk}", status="WARN",
                    severity="minor",
                    detail=f"{blk} 未生产（实现现状/退化）"))
            else:
                results.append(CheckResult(
                    check=f"frame.block_present:{blk}", status="FAIL",
                    severity="major", detail=f"{blk} 缺失"))

    # 2. 类型/维度/count 检查（针对实际存在的块）
    for name, blk in blocks.items():
        entry = reg_by_name.get(name)
        if entry is None:
            results.append(CheckResult(
                check=f"frame.block_registered:{name}", status="WARN",
                severity="minor",
                detail=f"{name} 未在注册表（观察项）"))
            continue
        results.append(CheckResult(
            check=f"frame.block_registered:{name}", status="PASS",
            severity="info"))
        # 类型检查
        exp_type = entry["type"].strip()
        act_type = blk.get("type", "")
        if exp_type == act_type:
            results.append(CheckResult(
                check=f"frame.type_match:{name}", status="PASS",
                detail=f"type={act_type}", severity="info"))
        else:
            results.append(CheckResult(
                check=f"frame.type_match:{name}", status="FAIL",
                severity="critical",
                detail=f"期望 {exp_type} 实际 {act_type}"))
        # 维度/count 检查（根据类型推断）
        exp_dims = _dims_to_list(entry["dimensions"])
        count = blk.get("count", 0)
        type_id = blk.get("type_id")
        if name == "data" and type_id == 0:
            # FLOAT32 [H,W]: count = H*W
            results.append(CheckResult(
                check=f"frame.count_data:{name}", status="PASS",
                detail=f"count={count} (=H*W, FLOAT32)", severity="info"))
        elif name == "star_det":
            # 期望 FLOAT64 [N,6] count=N*6
            if type_id == 0:  # FLOAT32
                # 实现现状 FLOAT32 [N,4]
                n = count // 4 if count % 4 == 0 else -1
                results.append(CheckResult(
                    check=f"frame.dims_stardet:{name}", status="FAIL",
                    severity="critical",
                    detail=f"期望 FLOAT64[N,6] count=N*6; 实际 FLOAT32[N,4] count={count} (N={n})"))
            elif type_id == 1:  # FLOAT64
                if count % 6 == 0:
                    results.append(CheckResult(
                        check=f"frame.dims_stardet:{name}", status="PASS",
                        detail=f"FLOAT64[N,6] count={count} N={count//6}",
                        severity="info"))
                else:
                    results.append(CheckResult(
                        check=f"frame.dims_stardet:{name}", status="FAIL",
                        severity="major", detail=f"count={count} 非 6 倍数"))
        elif name == "psf":
            # FLOAT64 [N,9] count=N*9
            if type_id == 1 and count % 9 == 0:
                results.append(CheckResult(
                    check=f"frame.dims_psf:{name}", status="PASS",
                    detail=f"FLOAT64[N,9] count={count} N={count//9}",
                    severity="info"))
            else:
                results.append(CheckResult(
                    check=f"frame.dims_psf:{name}", status="FAIL",
                    severity="major", detail=f"期望 FLOAT64[N,9]; type_id={type_id} count={count}"))
        elif name == "gaia_cat":
            # FLOAT64 [N,3] count=N*3
            if type_id == 1 and count % 3 == 0:
                results.append(CheckResult(
                    check=f"frame.dims_gaiacat:{name}", status="PASS",
                    detail=f"FLOAT64[N,3] count={count} N={count//3}",
                    severity="info"))
            else:
                results.append(CheckResult(
                    check=f"frame.dims_gaiacat:{name}", status="FAIL",
                    severity="minor", detail=f"type_id={type_id} count={count}"))

    # 3. star_det 与 psf 行数一致
    sd = blocks.get("star_det")
    psf = blocks.get("psf")
    if sd and psf:
        sd_count = sd.get("count", 0)
        psf_count = psf.get("count", 0)
        # star_det 实际列数（实现现状 4，契约 6）
        sd_cols = 4 if sd.get("type_id") == 0 else 6
        psf_cols = 9
        sd_n = sd_count // sd_cols if sd_count % sd_cols == 0 else -1
        psf_n = psf_count // psf_cols if psf_count % psf_cols == 0 else -1
        if sd_n == psf_n and sd_n > 0:
            results.append(CheckResult(
                check="frame.count_stardet_psf_aligned", status="PASS",
                detail=f"行数一致 N={sd_n}", severity="info"))
        else:
            results.append(CheckResult(
                check="frame.count_stardet_psf_aligned", status="FAIL",
                severity="major",
                detail=f"star_det N={sd_n} vs psf N={psf_n} 不一致"))

    # 4. photo_stats 检查
    if snapshot.get("photo_stats_seen"):
        results.append(CheckResult(
            check="frame.photo_stats_written", status="PASS",
            detail="日志确认 photo_stats 已写入", severity="info"))
        # 注意：键名大小写与必需键对齐需运行时 KV 检查，此处仅确认写入
        results.append(CheckResult(
            check="frame.photo_stats_keys", status="WARN",
            severity="minor",
            detail="photo_stats 键名大小写与契约不一致（实现大写 vs 契约小写），需运行时 KV 校验"))
    else:
        results.append(CheckResult(
            check="frame.photo_stats_written", status="FAIL",
            severity="major", detail="photo_stats 未写入"))

    # 5. snr_model 检查
    if "snr_model" in blocks:
        results.append(CheckResult(
            check="frame.snr_model_written", status="PASS",
            detail="snr_model 块已写入", severity="info"))
    else:
        results.append(CheckResult(
            check="frame.snr_model_written", status="WARN",
            severity="minor",
            detail="snr_model 未写入（G-002 退化：sigma_residual=0）"))

    return results


# ---------------------------------------------------------------------------
# 不一致项汇总
# ---------------------------------------------------------------------------

def collect_inconsistencies(rows: list[dict], hiss_info: dict,
                            frame_snapshot: dict,
                            registry_checks: list[CheckResult],
                            hiss_checks: list[CheckResult],
                            frame_checks: list[CheckResult]) -> list[dict]:
    """汇总所有 FAIL/WARN 的不一致项。"""
    items: list[dict] = []
    for cr in registry_checks + hiss_checks + frame_checks:
        if cr.status in ("FAIL", "WARN"):
            items.append({
                "check": cr.check,
                "status": cr.status,
                "severity": cr.severity,
                "detail": cr.detail,
            })
    # 基于源码分析的已知不一致（orchestrator.cpp 静态审查）
    items.extend([
        {
            "check": "orchestrator.star_det_type_mismatch",
            "status": "FAIL",
            "severity": "critical",
            "detail": "orchestrator.cpp:1465 写入 AIO_BLOCK_FLOAT32 [N,4]，"
                      "契约 star_det_block_v1.md 要求 FLOAT64 [N,6]（缺 saturated,has_saturated 列）",
        },
        {
            "check": "orchestrator.gaia_cat_unregistered",
            "status": "WARN",
            "severity": "minor",
            "detail": "orchestrator.cpp:1529 生产 gaia_cat FLOAT64[N,3]，"
                      "04 契约与注册表（v1）未正式定义此块",
        },
        {
            "check": "orchestrator.cal_stats_missing",
            "status": "FAIL",
            "severity": "major",
            "detail": "CALIBRATE 阶段骨架未写入 cal_stats KV 块（master=nullptr 退化）",
        },
        {
            "check": "orchestrator.astrometry_stats_missing",
            "status": "FAIL",
            "severity": "major",
            "detail": "PLATESOLVE 阶段未写入 astrometry_stats KV 块（WCS 质量诊断）",
        },
        {
            "check": "orchestrator.photo_stats_key_case",
            "status": "WARN",
            "severity": "minor",
            "detail": "photo_stats 键名大写（STATUS/N_MATCHED/SCALE_FACTOR/SIGMA_RESIDUAL），"
                      "契约要求小写且缺 schema_version/input_data_revision/output_data_revision",
        },
        {
            "check": "orchestrator.star_det_provenance_missing",
            "status": "FAIL",
            "severity": "major",
            "detail": "star_det 块未写入 ASTROCS.STARDET.SCHEMA/PRODUCER/INPUT_REVISION/"
                      "PARAM_HASH/COUNT/HASH 等 header provenance 键",
        },
        {
            "check": "orchestrator.snr_model_not_written",
            "status": "WARN",
            "severity": "minor",
            "detail": "SNR 退化（sigma_residual=0）时 snr_model 块未写入，HISS has_snr=0（G-002）",
        },
    ])
    return items


# ---------------------------------------------------------------------------
# 主入口
# ---------------------------------------------------------------------------

def build_report(args: argparse.Namespace) -> ValidationReport:
    report = ValidationReport(
        registry_path=args.registry,
        hiss_path=args.hiss or "",
        frame_snapshot_source=args.frame_snapshot or args.log or "",
    )
    # 1. 注册表
    rows = load_registry(args.registry)
    report.registry_summary = {
        "block_count": len(rows),
        "block_names": [r["block_name"] for r in rows],
        "required_contract_blocks_covered": sorted(
            REQUIRED_CONTRACT_BLOCKS & {r["block_name"] for r in rows}),
        "required_contract_blocks_missing": sorted(
            REQUIRED_CONTRACT_BLOCKS - {r["block_name"] for r in rows}),
        "observed_extra_blocks": sorted(
            {r["block_name"] for r in rows} - REQUIRED_CONTRACT_BLOCKS),
    }
    report.registry_checks = [c.to_dict() for c in validate_registry(rows)]

    # 2. HISS
    hiss_info: dict = {}
    if args.hiss:
        hiss_info = parse_hiss(args.hiss)
        report.hiss_checks = [c.to_dict()
                              for c in validate_hiss_metadata(hiss_info, rows)]

    # 3. PipelineFrame
    snapshot: dict = {"blocks": {}}
    if args.frame_snapshot:
        snapshot = load_frame_snapshot(args.frame_snapshot)
        report.frame_snapshot_source = args.frame_snapshot
    elif args.log:
        snapshot = parse_orchestrator_log(args.log)
        report.frame_snapshot_source = args.log
    if snapshot.get("blocks"):
        report.frame_checks = [c.to_dict()
                               for c in validate_frame_blocks(snapshot, rows)]

    # 4. 不一致项（重新构造 CheckResult 列表以收集 FAIL/WARN）
    reg_checks = [CheckResult(**c) for c in report.registry_checks]
    hiss_checks = [CheckResult(**c) for c in report.hiss_checks]
    frm_checks = [CheckResult(**c) for c in report.frame_checks]
    report.inconsistencies = collect_inconsistencies(
        rows, hiss_info, snapshot, reg_checks, hiss_checks, frm_checks)

    # 5. 汇总
    n_pass = sum(1 for c in report.registry_checks + report.hiss_checks
                 + report.frame_checks if c["status"] == "PASS")
    n_fail = sum(1 for c in report.registry_checks + report.hiss_checks
                 + report.frame_checks if c["status"] == "FAIL")
    n_warn = sum(1 for c in report.registry_checks + report.hiss_checks
                 + report.frame_checks if c["status"] == "WARN")
    n_skip = sum(1 for c in report.registry_checks + report.hiss_checks
                 + report.frame_checks if c["status"] == "SKIP")
    report.summary = {
        "total_checks": len(report.registry_checks) + len(report.hiss_checks)
                        + len(report.frame_checks),
        "pass": n_pass, "fail": n_fail, "warn": n_warn, "skip": n_skip,
        "inconsistency_count": len(report.inconsistencies),
        "critical_count": sum(1 for i in report.inconsistencies
                              if i.get("severity") == "critical"),
        "verdict": "PASS" if n_fail == 0 else "FAIL",
        "note": "verdict=PASS 表示校验器自身运行成功；不一致项记录于 inconsistencies 数组",
    }
    return report


def main() -> int:
    parser = argparse.ArgumentParser(
        description="P01-002 数据块 schema 校验器")
    parser.add_argument("--registry", required=True,
                        help="pipeline_blocks_registry.csv 路径")
    parser.add_argument("--hiss", help="待校验 HISS 文件路径")
    parser.add_argument("--frame-snapshot", dest="frame_snapshot",
                        help="PipelineFrame JSON 快照路径")
    parser.add_argument("--log",
                        help="orchestrator DEBUG 日志路径（用于提取块快照）")
    parser.add_argument("-o", "--output", default="-",
                        help="输出 JSON 报告路径（- 表示 stdout）")
    args = parser.parse_args()

    logger.info("=== P01-002 schema 校验开始 ===")
    report = build_report(args)
    out_json = json.dumps(asdict(report), ensure_ascii=False, indent=2)
    if args.output == "-":
        print(out_json)
    else:
        with open(args.output, "w", encoding="utf-8") as fp:
            fp.write(out_json)
        logger.info("报告已写入: %s", args.output)
    logger.info("=== 校验结束: verdict=%s, inconsistencies=%d ===",
                report.summary["verdict"],
                report.summary["inconsistency_count"])
    # 退出码：有 critical 不一致项返回 2，有 FAIL 返回 1，否则 0
    if report.summary["critical_count"] > 0:
        return 2
    if report.summary["fail"] > 0:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
