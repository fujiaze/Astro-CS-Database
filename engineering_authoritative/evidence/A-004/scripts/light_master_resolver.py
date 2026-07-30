#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""A-004: Light 帧 -> Bias/Dark/Flat 唯一解析与严格模式。

功能:
  1. 读取 Light 帧 FITS(.fts)/XISF(.xisf) Header (设备ID/尺寸/Bin/曝光/温度/滤镜)
  2. 滤镜名规范化 (使用 A-002 FILTER_ALIAS_MAP.json)
  3. 匹配 Bias  : 同设备 + 同尺寸 + 同 Bin
  4. 匹配 Dark  : 同设备 + 同尺寸 + 同 Bin + 曝光时间匹配 (严格=精确, 宽松=最近)
  5. 匹配 Flat  : 同设备 + 同尺寸 + 同 Bin + 同规范滤镜
  6. 严格模式 (默认): 找不到唯一匹配则 UNRESOLVED, 不可静默降级

依赖: astropy (FITS 读取), 标准库 (XISF XML 解析, threading 超时)

用法:
  # 单帧解析
  python light_master_resolver.py <light_path> [--lenient] [--output-dir DIR]
  # 批量解析 (扫描目录下所有 .fts/.fit/.xisf Light 帧)
  python light_master_resolver.py --batch <dir> [--lenient] [--output-dir DIR]
  # 运行内置 4 测试用例
  python light_master_resolver.py --self-test [--output-dir DIR]

退出码: 0=全部 RESOLVED, 1=存在 UNRESOLVED, 2=运行错误
"""
from __future__ import annotations

import argparse
import csv
import json
import logging
import os
import struct
import sys
import threading
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field, asdict
from datetime import datetime
from typing import Optional

# ---------------------------------------------------------------------------
# 路径常量
# ---------------------------------------------------------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# scripts -> A-004 -> evidence -> engineering_authoritative -> PROJECT_ROOT (4 级)
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "..", ".."))
A002_DIR = os.path.join(PROJECT_ROOT, "engineering_authoritative", "evidence", "A-002")
A004_DIR = os.path.join(PROJECT_ROOT, "engineering_authoritative", "evidence", "A-004")
LOG_DIR = os.path.join(A004_DIR, "logs")
RESULT_DIR = os.path.join(A004_DIR, "results")

EQUIPMENT_CATALOG = os.path.join(A002_DIR, "TESTDATA_EQUIPMENT_CATALOG.csv")
FILTER_ALIAS_MAP = os.path.join(A002_DIR, "FILTER_ALIAS_MAP.json")
MASTER_INVENTORY = os.path.join(A002_DIR, "CALIBRATION_MASTER_INVENTORY.csv")

# 设备 ID -> 校准目录 (用于 Master 文件存在性校验)
MASTER_DIRS = {
    "T2": os.path.join(PROJECT_ROOT, "testdata", "T2 calibration files"),
    "T3": os.path.join(PROJECT_ROOT, "testdata", "T3 calibration files"),
    "T4": os.path.join(PROJECT_ROOT, "testdata", "T4 calibration files"),
}

# 超时常量
HEADER_READ_TIMEOUT_S = 30
# Dark 曝光精确匹配容差 (秒) — Master 命名用 1200.00s, Light header 用 1200.0
DARK_EXPOSURE_TOLERANCE_S = 0.01

# ---------------------------------------------------------------------------
# 日志配置
# ---------------------------------------------------------------------------


def setup_logger(name: str = "A004", log_file: Optional[str] = None) -> logging.Logger:
    """配置日志: 同时输出到 stderr 和文件."""
    logger = logging.getLogger(name)
    logger.setLevel(logging.DEBUG)
    logger.handlers.clear()
    fmt = logging.Formatter("[%(asctime)s] [%(levelname)s] %(message)s", "%Y-%m-%d %H:%M:%S")
    sh = logging.StreamHandler(sys.stderr)
    sh.setLevel(logging.INFO)
    sh.setFormatter(fmt)
    logger.addHandler(sh)
    if log_file:
        os.makedirs(os.path.dirname(log_file), exist_ok=True)
        fh = logging.FileHandler(log_file, encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(fmt)
        logger.addHandler(fh)
    return logger


# ---------------------------------------------------------------------------
# 数据结构
# ---------------------------------------------------------------------------


@dataclass
class MasterRecord:
    """Master 帧清单中的一条记录."""
    master_id: str
    device_id: str
    master_type: str  # Bias / Dark / Flat
    file_name: str
    file_path: str
    sensor_size: str
    binning: str
    exposure_s: Optional[float]
    filter_canonical: Optional[str]
    ccd_temp_c: Optional[str]
    imagetyp: str
    match_key: str


@dataclass
class LightHeader:
    """从 Light 帧 Header 解析的关键字段."""
    file_path: str
    file_name: str
    format: str  # "FITS" / "XISF"
    device_id: str  # 从路径推导 (FITS header INSTRUME=FLI 无法区分 T2/T3/T4)
    instrume: str
    imagetyp: str
    sensor_size: str  # "WxH"
    binning: str  # "1x1"
    exposure_s: float
    filter_raw: str
    filter_canonical: str
    ccd_temp_c: Optional[str]
    set_temp: Optional[str]
    gain: Optional[str]
    offset: Optional[str]
    focal_len: Optional[str]
    object_name: Optional[str]
    date_obs: Optional[str]
    naxis1: int
    naxis2: int
    read_error: Optional[str] = None
    raw_header: dict = field(default_factory=dict)


@dataclass
class MatchResult:
    """单个 Master 类型的匹配结果."""
    status: str  # "exact" / "not_found" / "ambiguous" / "nearest_degraded"
    master_id: Optional[str] = None
    master_path: Optional[str] = None
    reason: str = ""
    candidates_considered: int = 0
    nearest_exposure: Optional[float] = None  # Dark 降级时记录最近曝光


@dataclass
class ResolutionResult:
    """单帧 Light 的完整解析结果."""
    light_path: str
    light_file: str
    device_id: str
    sensor_size: str
    binning: str
    exposure_s: float
    filter_raw: str
    filter_canonical: str
    ccd_temp_c: Optional[str]
    bias: MatchResult
    dark: MatchResult
    flat: MatchResult
    resolution_status: str  # "RESOLVED" / "UNRESOLVED"
    resolution_note: str
    missing: list  # 缺失项列表
    timestamp: str
    read_error: Optional[str] = None


# ---------------------------------------------------------------------------
# Header 读取 (带超时)
# ---------------------------------------------------------------------------


class _HeaderReadWorker(threading.Thread):
    """在子线程中读取 FITS/XISF header, 供主线程 join(timeout) 实现超时."""

    def __init__(self, path: str, fmt: str):
        super().__init__(daemon=True)
        self.path = path
        self.fmt = fmt
        self.result: Optional[dict] = None
        self.error: Optional[str] = None

    def run(self):
        try:
            if self.fmt == "FITS":
                self.result = _read_fits_header_internal(self.path)
            else:
                self.result = _read_xisf_header_internal(self.path)
        except Exception as e:
            self.error = f"{type(e).__name__}: {e}"


def _read_fits_header_internal(path: str) -> dict:
    """用 astropy 读取 FITS header."""
    from astropy.io import fits
    h = fits.getheader(path, ext=0)
    return {k: str(h[k]) for k in h.keys() if k}


def _read_xisf_header_internal(path: str) -> dict:
    """读取 XISF header (纯 Python XML 解析, 复用 A-002 逻辑).

    XISF 格式: [0-3]='XISF' [4-7]=version [8-11]=uint32 LE xml_len [12-15]=reserved [16+]=XML
    """
    with open(path, "rb") as f:
        magic = f.read(4)
        if magic != b"XISF":
            raise ValueError(f"not XISF magic: {magic!r}")
        f.read(4)  # version
        xml_len = struct.unpack("<I", f.read(4))[0]
        f.read(4)  # reserved
        raw = f.read(xml_len)
    xml_str = raw.decode("utf-8", errors="ignore")
    start = xml_str.find("<?xml")
    end = xml_str.rfind("</xisf>")
    if start >= 0 and end > start:
        xml_str = xml_str[start:end + 7]
    elif start >= 0:
        xml_str = xml_str[start:]
    header = {}
    try:
        root = ET.fromstring(xml_str)
        ns = ""
        if root.tag.startswith("{"):
            ns = root.tag[1:root.tag.index("}")]
        nsmap = {"x": ns} if ns else {}
        img = root.find("x:Image", nsmap) if ns else root.find("Image")
        if img is not None:
            for kw in (img.findall("x:FITSKeyword", nsmap) if ns else img.findall("FITSKeyword")):
                name = kw.get("name")
                value = kw.get("value")
                if name:
                    header[name] = value
            # geometry 属性也记录 (含 width/height)
            for attr_name, attr_val in img.attrib.items():
                header[f"_geo_{attr_name}"] = attr_val
    except Exception as e:
        raise ValueError(f"XISF XML parse error: {e}")
    return header


def read_header_with_timeout(path: str, logger: logging.Logger, timeout: int = HEADER_READ_TIMEOUT_S) -> tuple[Optional[dict], Optional[str], str]:
    """带超时读取 header, 返回 (header_dict, error, format)."""
    ext = os.path.splitext(path)[1].lower()
    if ext in (".fts", ".fit", ".fits"):
        fmt = "FITS"
    elif ext == ".xisf":
        fmt = "XISF"
    else:
        return None, f"unsupported extension: {ext}", ""
    worker = _HeaderReadWorker(path, fmt)
    worker.start()
    worker.join(timeout=timeout)
    if worker.is_alive():
        return None, f"header read timeout after {timeout}s ({fmt})", fmt
    if worker.error:
        return None, worker.error, fmt
    return worker.result, None, fmt


# ---------------------------------------------------------------------------
# 设备 ID 推导 & 滤镜规范化
# ---------------------------------------------------------------------------


def derive_device_id(path: str, logger: logging.Logger) -> str:
    """从文件路径推导设备 ID (T2/T3/T4).

    FITS header INSTRUME=FLI 无法区分 T2/T3/T4, 必须从路径推导.
    路径线索: 目录名含 "_T2" / "_T3" / "_T4" (后跟非字母数字, 含中文, 如 "_T2素材")
    或 "T2 calibration files" 等.
    """
    import re
    norm = path.replace("\\", "/")
    # 匹配 _T2 / _T3 / _T4 后跟非字母数字 (含中文/下划线/斜杠/空格)
    # 不匹配 _T20 _T2x 等 (T后数字后若跟字母数字则跳过)
    m = re.search(r"[_/]T([234])(?![a-zA-Z0-9])", norm)
    if m:
        return f"T{m.group(1)}"
    # 匹配 /T2 calibration files 等
    m2 = re.search(r"/T([234])\s+calibration", norm)
    if m2:
        return f"T{m2.group(1)}"
    logger.warning("无法从路径推导设备 ID: %s", path)
    return "UNKNOWN"


def normalize_filter(filter_raw: str, filter_map: dict, logger: logging.Logger) -> str:
    """使用 FILTER_ALIAS_MAP.json 规范化滤镜名."""
    if not filter_raw:
        return "UNKNOWN"
    raw = filter_raw.strip()
    # 1. 精确匹配 canonical_name
    for canon, info in filter_map.items():
        if canon.startswith("_"):
            continue
        if info.get("canonical_name") == raw:
            return info["canonical_name"]
    # 2. 匹配 aliases (大小写敏感, 因为 H-alpha vs Ha 是不同写法)
    for canon, info in filter_map.items():
        if canon.startswith("_"):
            continue
        if raw in info.get("aliases", []):
            return info["canonical_name"]
    # 3. 大小写不敏感回退
    raw_lower = raw.lower()
    for canon, info in filter_map.items():
        if canon.startswith("_"):
            continue
        for alias in info.get("aliases", []):
            if alias.lower() == raw_lower:
                return info["canonical_name"]
    logger.warning("滤镜名无法规范化: %s (不在 FILTER_ALIAS_MAP)", raw)
    return raw  # 返回原名, 严格模式下会导致 Flat 不匹配


# ---------------------------------------------------------------------------
# Master 清单加载
# ---------------------------------------------------------------------------


def load_master_inventory(logger: logging.Logger) -> list[MasterRecord]:
    """加载 CALIBRATION_MASTER_INVENTORY.csv."""
    records = []
    with open(MASTER_INVENTORY, "r", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        for row in reader:
            exp = row.get("exposure_s", "").strip()
            records.append(MasterRecord(
                master_id=row["master_id"],
                device_id=row["device_id"],
                master_type=row["master_type"],
                file_name=row["file_name"],
                file_path=row["file_path"],
                sensor_size=row["sensor_size"],
                binning=row["binning"],
                exposure_s=float(exp) if exp else None,
                filter_canonical=row.get("filter_canonical", "").strip() or None,
                ccd_temp_c=row.get("ccd_temp_c", "").strip() or None,
                imagetyp=row.get("imagetyp", ""),
                match_key=row.get("match_key", ""),
            ))
    logger.info("加载 Master 清单: %d 条记录", len(records))
    return records


def load_filter_map(logger: logging.Logger) -> dict:
    """加载 FILTER_ALIAS_MAP.json."""
    with open(FILTER_ALIAS_MAP, "r", encoding="utf-8") as f:
        data = json.load(f)
    n = sum(1 for k in data if not k.startswith("_"))
    logger.info("加载滤镜别名映射: %d 个规范滤镜", n)
    return data


# ---------------------------------------------------------------------------
# 匹配逻辑
# ---------------------------------------------------------------------------


def match_bias(device: str, size: str, binning: str, masters: list[MasterRecord], logger: logging.Logger) -> MatchResult:
    """匹配 Bias: 同设备 + 同尺寸 + 同 Bin."""
    candidates = [
        m for m in masters
        if m.master_type == "Bias"
        and m.device_id == device
        and m.sensor_size == size
        and m.binning == binning
    ]
    if len(candidates) == 0:
        return MatchResult(status="not_found", reason=f"无 Bias 匹配 {device}|{size}|{binning}", candidates_considered=0)
    if len(candidates) > 1:
        ids = [c.master_id for c in candidates]
        return MatchResult(status="ambiguous", reason=f"Bias 匹配多个: {ids}", candidates_considered=len(candidates))
    c = candidates[0]
    return MatchResult(status="exact", master_id=c.master_id, master_path=c.file_path, reason="唯一匹配", candidates_considered=1)


def match_dark(device: str, size: str, binning: str, exposure: float, masters: list[MasterRecord],
               strict: bool, logger: logging.Logger) -> MatchResult:
    """匹配 Dark: 同设备 + 同尺寸 + 同 Bin + 曝光匹配.

    严格模式: 曝光精确匹配 (容差 0.01s), 否则 not_found.
    宽松模式: 精确优先, 无精确则取最近 Dark (标注 nearest_degraded).
    """
    candidates = [
        m for m in masters
        if m.master_type == "Dark"
        and m.device_id == device
        and m.sensor_size == size
        and m.binning == binning
        and m.exposure_s is not None
    ]
    if len(candidates) == 0:
        return MatchResult(status="not_found", reason=f"无 Dark 匹配 {device}|{size}|{binning}", candidates_considered=0)

    # 精确匹配
    exact = [c for c in candidates if abs(c.exposure_s - exposure) < DARK_EXPOSURE_TOLERANCE_S]
    if len(exact) == 1:
        c = exact[0]
        return MatchResult(status="exact", master_id=c.master_id, master_path=c.file_path,
                           reason=f"曝光精确匹配 ({c.exposure_s}s vs {exposure}s)", candidates_considered=len(candidates))
    if len(exact) > 1:
        ids = [c.master_id for c in exact]
        return MatchResult(status="ambiguous", reason=f"Dark 曝光匹配多个: {ids}", candidates_considered=len(exact))

    # 无精确匹配
    if strict:
        # 严格模式: 不降级, 记录最近 Dark 供参考
        nearest = min(candidates, key=lambda c: abs(c.exposure_s - exposure))
        return MatchResult(
            status="not_found",
            reason=f"严格模式: 无精确曝光匹配 (Light={exposure}s, 最近 Dark={nearest.exposure_s}s, 差={abs(nearest.exposure_s - exposure):.2f}s)",
            candidates_considered=len(candidates),
            nearest_exposure=nearest.exposure_s,
        )
    # 宽松模式: 取最近 Dark (降级)
    nearest = min(candidates, key=lambda c: abs(c.exposure_s - exposure))
    logger.warning("Dark 宽松降级: Light=%ss -> 使用最近 Dark=%ss (差=%.2fs)", exposure, nearest.exposure_s, abs(nearest.exposure_s - exposure))
    return MatchResult(
        status="nearest_degraded",
        master_id=nearest.master_id,
        master_path=nearest.file_path,
        reason=f"宽松降级: 最近 Dark ({nearest.exposure_s}s vs Light {exposure}s, 差={abs(nearest.exposure_s - exposure):.2f}s)",
        candidates_considered=len(candidates),
        nearest_exposure=nearest.exposure_s,
    )


def match_flat(device: str, size: str, binning: str, filter_canonical: str, masters: list[MasterRecord],
               logger: logging.Logger) -> MatchResult:
    """匹配 Flat: 同设备 + 同尺寸 + 同 Bin + 同规范滤镜.

    严格模式: 滤镜必须精确匹配规范名, 不可用其他滤镜替代 (如 Red 替代 Lum).
    """
    candidates = [
        m for m in masters
        if m.master_type == "Flat"
        and m.device_id == device
        and m.sensor_size == size
        and m.binning == binning
        and m.filter_canonical == filter_canonical
    ]
    if len(candidates) == 0:
        # 检查是否有同设备其他滤镜的 Flat (用于报告, 不使用)
        other_flats = [m for m in masters if m.master_type == "Flat" and m.device_id == device and m.sensor_size == size and m.binning == binning]
        other_filters = sorted(set(m.filter_canonical for m in other_flats if m.filter_canonical))
        hint = f" (同设备有 Flat 滤镜: {other_filters})" if other_filters else ""
        return MatchResult(
            status="not_found",
            reason=f"无 Flat 匹配 {device}|{size}|{binning}|{filter_canonical}{hint}",
            candidates_considered=0,
        )
    if len(candidates) > 1:
        ids = [c.master_id for c in candidates]
        return MatchResult(status="ambiguous", reason=f"Flat 匹配多个: {ids}", candidates_considered=len(candidates))
    c = candidates[0]
    return MatchResult(status="exact", master_id=c.master_id, master_path=c.file_path,
                       reason=f"滤镜匹配 ({filter_canonical})", candidates_considered=1)


# ---------------------------------------------------------------------------
# 核心解析
# ---------------------------------------------------------------------------


def parse_light_header(path: str, filter_map: dict, logger: logging.Logger) -> LightHeader:
    """读取 Light 帧 header 并解析关键字段."""
    header, err, fmt = read_header_with_timeout(path, logger)
    fname = os.path.basename(path)

    if err:
        logger.error("读取 header 失败: %s -> %s", fname, err)
        return LightHeader(
            file_path=path, file_name=fname, format=fmt or "UNKNOWN",
            device_id="UNKNOWN", instrume="?", imagetyp="?",
            sensor_size="?", binning="?", exposure_s=0.0,
            filter_raw="", filter_canonical="UNKNOWN",
            ccd_temp_c=None, set_temp=None, gain=None, offset=None,
            focal_len=None, object_name=None, date_obs=None,
            naxis1=0, naxis2=0, read_error=err, raw_header={},
        )

    def _get(key, default=""):
        v = header.get(key, default)
        return v if v else default

    naxis1 = int(_get("NAXIS1", "0") or "0")
    naxis2 = int(_get("NAXIS2", "0") or "0")
    sensor_size = f"{naxis1}x{naxis2}" if naxis1 and naxis2 else "?"

    xbin = _get("XBINNING", "1")
    ybin = _get("YBINNING", "1")
    binning = f"{xbin}x{ybin}" if xbin and ybin else "1x1"

    try:
        exposure = float(_get("EXPTIME", "0") or "0")
    except ValueError:
        exposure = 0.0

    filter_raw = _get("FILTER", "")
    filter_canonical = normalize_filter(filter_raw, filter_map, logger)

    device_id = derive_device_id(path, logger)

    return LightHeader(
        file_path=path, file_name=fname, format=fmt,
        device_id=device_id,
        instrume=_get("INSTRUME", "?"),
        imagetyp=_get("IMAGETYP", "?"),
        sensor_size=sensor_size,
        binning=binning,
        exposure_s=exposure,
        filter_raw=filter_raw,
        filter_canonical=filter_canonical,
        ccd_temp_c=_get("CCD-TEMP", "") or None,
        set_temp=_get("SET-TEMP", "") or None,
        gain=_get("GAIN", "") or None,
        offset=_get("OFFSET", "") or None,
        focal_len=_get("FOCALLEN", "") or None,
        object_name=_get("OBJECT", "") or None,
        date_obs=_get("DATE-OBS", "") or None,
        naxis1=naxis1, naxis2=naxis2,
        raw_header=header,
    )


def resolve_light(path: str, masters: list[MasterRecord], filter_map: dict,
                  logger: logging.Logger, strict: bool = True) -> ResolutionResult:
    """解析单帧 Light -> Bias/Dark/Flat."""
    logger.info("=" * 70)
    logger.info("解析 Light 帧: %s (严格模式=%s)", os.path.basename(path), strict)

    lh = parse_light_header(path, filter_map, logger)

    if lh.read_error:
        result = ResolutionResult(
            light_path=path, light_file=lh.file_name,
            device_id=lh.device_id, sensor_size=lh.sensor_size,
            binning=lh.binning, exposure_s=lh.exposure_s,
            filter_raw=lh.filter_raw, filter_canonical=lh.filter_canonical,
            ccd_temp_c=lh.ccd_temp_c,
            bias=MatchResult(status="not_found", reason="header 读取失败"),
            dark=MatchResult(status="not_found", reason="header 读取失败"),
            flat=MatchResult(status="not_found", reason="header 读取失败"),
            resolution_status="UNRESOLVED",
            resolution_note=f"header 读取失败: {lh.read_error}",
            missing=["Bias", "Dark", "Flat"],
            timestamp=datetime.now().isoformat(),
            read_error=lh.read_error,
        )
        logger.error("解析失败 (header 读取错误): %s", lh.read_error)
        return result

    logger.info("  设备=%s 尺寸=%s Bin=%s 曝光=%.1fs 滤镜=%s->%s 温度=%s",
                lh.device_id, lh.sensor_size, lh.binning, lh.exposure_s,
                lh.filter_raw, lh.filter_canonical, lh.ccd_temp_c)

    # 匹配 Bias / Dark / Flat
    bias_r = match_bias(lh.device_id, lh.sensor_size, lh.binning, masters, logger)
    dark_r = match_dark(lh.device_id, lh.sensor_size, lh.binning, lh.exposure_s, masters, strict, logger)
    flat_r = match_flat(lh.device_id, lh.sensor_size, lh.binning, lh.filter_canonical, masters, logger)

    logger.info("  Bias:  status=%s id=%s reason=%s", bias_r.status, bias_r.master_id, bias_r.reason)
    logger.info("  Dark:  status=%s id=%s reason=%s", dark_r.status, dark_r.master_id, dark_r.reason)
    logger.info("  Flat:  status=%s id=%s reason=%s", flat_r.status, flat_r.master_id, flat_r.reason)

    # 严格模式: 三者必须 exact, 否则 UNRESOLVED
    missing = []
    if bias_r.status != "exact":
        missing.append("Bias")
    if dark_r.status != "exact":
        missing.append("Dark")
    if flat_r.status != "exact":
        missing.append("Flat")

    resolved = len(missing) == 0
    status = "RESOLVED" if resolved else "UNRESOLVED"
    note = "全部校准帧匹配" if resolved else f"缺失: {', '.join(missing)}"

    result = ResolutionResult(
        light_path=path, light_file=lh.file_name,
        device_id=lh.device_id, sensor_size=lh.sensor_size,
        binning=lh.binning, exposure_s=lh.exposure_s,
        filter_raw=lh.filter_raw, filter_canonical=lh.filter_canonical,
        ccd_temp_c=lh.ccd_temp_c,
        bias=bias_r, dark=dark_r, flat=flat_r,
        resolution_status=status, resolution_note=note,
        missing=missing,
        timestamp=datetime.now().isoformat(),
    )
    logger.info("  => %s: %s", status, note)
    return result


# ---------------------------------------------------------------------------
# 输出
# ---------------------------------------------------------------------------


def result_to_dict(r: ResolutionResult) -> dict:
    """转 dict 用于 JSON 输出."""
    d = asdict(r)
    return d


def save_json(results: list[ResolutionResult], path: str, logger: logging.Logger):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump([result_to_dict(r) for r in results], f, indent=2, ensure_ascii=False, default=str)
    logger.info("JSON 结果已保存: %s (%d 帧)", path, len(results))


def save_csv(results: list[ResolutionResult], path: str, logger: logging.Logger):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fields = [
        "light_file", "device_id", "sensor_size", "binning", "exposure_s",
        "filter_raw", "filter_canonical", "ccd_temp_c",
        "bias_status", "bias_master", "bias_reason",
        "dark_status", "dark_master", "dark_reason",
        "flat_status", "flat_master", "flat_reason",
        "resolution_status", "resolution_note", "missing",
    ]
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(fields)
        for r in results:
            w.writerow([
                r.light_file, r.device_id, r.sensor_size, r.binning, r.exposure_s,
                r.filter_raw, r.filter_canonical, r.ccd_temp_c or "",
                r.bias.status, r.bias.master_id or "", r.bias.reason,
                r.dark.status, r.dark.master_id or "", r.dark.reason,
                r.flat.status, r.flat.master_id or "", r.flat.reason,
                r.resolution_status, r.resolution_note, "|".join(r.missing),
            ])
    logger.info("CSV 结果已保存: %s (%d 帧)", path, len(results))


def save_unresolved_report(results: list[ResolutionResult], path: str, logger: logging.Logger):
    """生成 UNRESOLVED 帧 Markdown 报告."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    unresolved = [r for r in results if r.resolution_status == "UNRESOLVED"]
    total = len(results)
    resolved = total - len(unresolved)
    coverage = (resolved / total * 100) if total else 0

    lines = []
    lines.append("# A-004 Light 帧 -> Master 解析报告\n")
    lines.append(f"**生成时间**: {datetime.now().isoformat()}\n")
    lines.append(f"**任务**: A-004 实现 Light 到 Bias/Dark/Flat 唯一解析与严格模式\n")
    lines.append(f"**严格模式**: 默认开启 (不可静默降级)\n\n")
    lines.append("## 1. 总览\n")
    lines.append(f"| 指标 | 值 |\n|------|-----|\n")
    lines.append(f"| 解析帧总数 | {total} |\n")
    lines.append(f"| 已解析 (RESOLVED) | {resolved} |\n")
    lines.append(f"| 未解析 (UNRESOLVED) | {len(unresolved)} |\n")
    lines.append(f"| 覆盖率 | {coverage:.1f}% |\n\n")

    if unresolved:
        lines.append("## 2. UNRESOLVED 帧清单\n\n")
        lines.append("| Light 帧 | 设备 | 尺寸 | Bin | 曝光(s) | 滤镜 | 缺失项 | Bias状态 | Dark状态 | Flat状态 | 说明 |\n")
        lines.append("|----------|------|------|-----|---------|------|--------|----------|----------|----------|------|\n")
        for r in unresolved:
            lines.append(f"| {r.light_file} | {r.device_id} | {r.sensor_size} | {r.binning} | {r.exposure_s} | {r.filter_raw}->{r.filter_canonical} | {', '.join(r.missing)} | {r.bias.status} | {r.dark.status} | {r.flat.status} | {r.resolution_note} |\n")

        lines.append("\n## 3. UNRESOLVED 原因分析\n\n")
        # 按缺失项分类
        missing_flat = [r for r in unresolved if "Flat" in r.missing]
        missing_dark = [r for r in unresolved if "Dark" in r.missing]
        missing_bias = [r for r in unresolved if "Bias" in r.missing]
        if missing_flat:
            lines.append(f"### 3.1 缺失 Flat ({len(missing_flat)} 帧)\n\n")
            for r in missing_flat:
                lines.append(f"- **{r.light_file}** ({r.device_id}, {r.filter_canonical}, {r.exposure_s}s): {r.flat.reason}\n")
        if missing_dark:
            lines.append(f"\n### 3.2 缺失 Dark ({len(missing_dark)} 帧)\n\n")
            for r in missing_dark:
                lines.append(f"- **{r.light_file}** ({r.device_id}, {r.exposure_s}s): {r.dark.reason}\n")
        if missing_bias:
            lines.append(f"\n### 3.3 缺失 Bias ({len(missing_bias)} 帧)\n\n")
            for r in missing_bias:
                lines.append(f"- **{r.light_file}** ({r.device_id}): {r.bias.reason}\n")
        if r.read_error:
            lines.append(f"\n### 3.4 Header 读取错误\n\n")
            for r in unresolved:
                if r.read_error:
                    lines.append(f"- **{r.light_file}**: {r.read_error}\n")
    else:
        lines.append("## 2. 全部 RESOLVED\n\n无 UNRESOLVED 帧。\n")

    lines.append("\n## 4. 已知问题 (不修复, 仅记录)\n")
    lines.append("- T2 Lum Flat 缺失: LDN43_T2 和 NGC247_T2 的 Lum 帧标记为 UNRESOLVED (T2 无 Lum Flat)\n")
    lines.append("- T4 Lum Flat 缺失: Victory_T4 的 Lum 帧标记为 UNRESOLVED (T4 无 Lum Flat)\n")
    lines.append("- 这些是真实缺失, 解析器明确报告, 不可静默降级 (如用 Red Flat 替代 Lum Flat)\n")

    with open(path, "w", encoding="utf-8") as f:
        f.writelines(lines)
    logger.info("UNRESOLVED 报告已保存: %s", path)


# ---------------------------------------------------------------------------
# 批量解析
# ---------------------------------------------------------------------------


def batch_resolve(light_paths: list[str], masters: list[MasterRecord], filter_map: dict,
                  logger: logging.Logger, strict: bool = True) -> list[ResolutionResult]:
    results = []
    for i, p in enumerate(light_paths, 1):
        logger.info("[%d/%d] 处理: %s", i, len(light_paths), p)
        r = resolve_light(p, masters, filter_map, logger, strict=strict)
        results.append(r)
    return results


def scan_lights(directory: str, logger: logging.Logger) -> list[str]:
    """扫描目录下所有 Light 帧 (.fts/.fit/.xisf)."""
    import glob
    exts = ("*.fts", "*.fit", "*.fits", "*.xisf")
    files = []
    for ext in exts:
        files.extend(glob.glob(os.path.join(directory, "**", ext), recursive=True))
    files = sorted(set(files))
    logger.info("扫描目录 %s: 找到 %d 个 Light 帧", directory, len(files))
    return files


# ---------------------------------------------------------------------------
# 内置自测
# ---------------------------------------------------------------------------


SELF_TEST_CASES = [
    {
        "name": "T2 Red (应 RESOLVED, T2 有 Red Flat)",
        "path": r"testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts",
        "expect_status": "RESOLVED",
    },
    {
        "name": "T2 Lum (应 UNRESOLVED, T2 无 Lum Flat)",
        "path": r"testdata\LDN43_T2素材_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts",
        "expect_status": "UNRESOLVED",
    },
    {
        "name": "T3 Lum (应 RESOLVED, T3 有 Lum Flat)",
        "path": r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts",
        "expect_status": "RESOLVED",
    },
    {
        "name": "T4 Red (应 RESOLVED, T4 有 Red Flat)",
        "path": r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts",
        "expect_status": "RESOLVED",
    },
]


def run_self_test(masters: list[MasterRecord], filter_map: dict, logger: logging.Logger,
                  output_dir: str, strict: bool = True) -> list[ResolutionResult]:
    """运行内置 4 测试用例."""
    logger.info("=" * 70)
    logger.info("运行内置自测: %d 个用例", len(SELF_TEST_CASES))
    results = []
    all_pass = True
    for tc in SELF_TEST_CASES:
        full_path = os.path.join(PROJECT_ROOT, tc["path"])
        logger.info("\n[TEST] %s", tc["name"])
        if not os.path.exists(full_path):
            logger.error("  测试文件不存在: %s", full_path)
            all_pass = False
            continue
        r = resolve_light(full_path, masters, filter_map, logger, strict=strict)
        results.append(r)
        passed = (r.resolution_status == tc["expect_status"])
        tag = "PASS" if passed else "FAIL"
        if not passed:
            all_pass = False
        logger.info("  [%s] 期望=%s 实际=%s", tag, tc["expect_status"], r.resolution_status)

    logger.info("\n%s", "=" * 70)
    logger.info("自测结果: %s", "全部 PASS" if all_pass else "存在 FAIL")
    return results


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(
        description="A-004 Light 帧 -> Bias/Dark/Flat 唯一解析 (严格模式默认)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    g = parser.add_mutually_exclusive_group(required=True)
    g.add_argument("light_path", nargs="?", help="单帧 Light 路径")
    g.add_argument("--batch", metavar="DIR", help="批量扫描目录")
    g.add_argument("--self-test", action="store_true", help="运行内置 4 测试用例")
    parser.add_argument("--lenient", action="store_true", help="宽松模式 (Dark 允许最近曝光降级, 默认严格)")
    parser.add_argument("--output-dir", default=RESULT_DIR, help=f"输出目录 (默认 {RESULT_DIR})")
    args = parser.parse_args()

    strict = not args.lenient
    output_dir = args.output_dir
    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(LOG_DIR, exist_ok=True)

    log_file = os.path.join(LOG_DIR, f"a004_resolver_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log")
    logger = setup_logger(log_file=log_file)
    logger.info("A-004 Light Master Resolver 启动")
    logger.info("  严格模式: %s", strict)
    logger.info("  输出目录: %s", output_dir)
    logger.info("  日志文件: %s", log_file)

    # 加载 A-002 证据
    masters = load_master_inventory(logger)
    filter_map = load_filter_map(logger)

    results: list[ResolutionResult] = []

    if args.self_test:
        results = run_self_test(masters, filter_map, logger, output_dir, strict=strict)
    elif args.batch:
        light_paths = scan_lights(args.batch, logger)
        if not light_paths:
            logger.error("目录下无 Light 帧: %s", args.batch)
            return 2
        results = batch_resolve(light_paths, masters, filter_map, logger, strict=strict)
    else:
        full_path = os.path.abspath(args.light_path)
        if not os.path.exists(full_path):
            logger.error("文件不存在: %s", full_path)
            return 2
        r = resolve_light(full_path, masters, filter_map, logger, strict=strict)
        results = [r]

    # 保存输出
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    json_path = os.path.join(output_dir, f"resolution_result_{ts}.json")
    csv_path = os.path.join(output_dir, f"resolution_result_{ts}.csv")
    report_path = os.path.join(output_dir, "UNRESOLVED_REPORT.md")
    save_json(results, json_path, logger)
    save_csv(results, csv_path, logger)
    save_unresolved_report(results, report_path, logger)

    # 统计
    total = len(results)
    resolved = sum(1 for r in results if r.resolution_status == "RESOLVED")
    unresolved = total - resolved
    coverage = (resolved / total * 100) if total else 0
    logger.info("=" * 70)
    logger.info("解析覆盖率: %d/%d = %.1f%%", resolved, total, coverage)
    logger.info("UNRESOLVED: %d 帧", unresolved)
    if unresolved:
        logger.info("UNRESOLVED 帧列表:")
        for r in results:
            if r.resolution_status == "UNRESOLVED":
                logger.info("  - %s (%s, %s): %s", r.light_file, r.device_id, r.filter_canonical, r.resolution_note)

    return 1 if unresolved > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
