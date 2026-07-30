# -*- coding: utf-8 -*-
"""
hiss_v2_inspector.py - HISS V2 文件检查器 (Inspector)

对 V2 HISS 文件做结构化检查并输出人类可读报告:
  - 固定头 (magic, version, flags, json_uncomp_len, json_comp_len, n_pix)
  - JSON provenance 全部字段
  - 块索引 (每块 ipix 范围 / offset / comp_size / raw_count / crc32 / codec)
  - SNR 块 (n_points, 3 个标量 snr_phot/median_snr/idw_power)
  - 全局 CRC32 校验 (覆盖 [0, filesize-48))
  - per-chunk CRC32 校验 (每块压缩数据)
  - 文件大小一致性 (契约 §13.10)

可直接作为模块导入, 也可 CLI 运行:
  python hiss_v2_inspector.py <file.hiss2> [-o report.txt]
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import struct
import sys
import zlib
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

# 复用 hiss_v2 的常量与解析逻辑, 避免重复实现导致行为分叉
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hiss_v2 as hv  # noqa: E402

logger = logging.getLogger(__name__)


# ============================================================================
# 报告数据类
# ============================================================================

@dataclass
class ChunkInspection:
    """单块检查结果。"""
    index: int
    offset: int
    comp_size: int
    raw_count: int
    crc32_stored: int
    crc32_actual: int
    crc32_ok: bool
    codec: int
    codec_name: str
    flags: int
    reserved: int
    first_ipix: Optional[int] = None
    last_ipix: Optional[int] = None
    decompress_ok: bool = True
    decompress_error: str = ""


@dataclass
class SnrInspection:
    """SNR 块检查结果。"""
    has_snr: bool
    offset: int
    size: int
    n_points: int = 0
    snr_phot: float = 0.0
    median_snr: float = 0.0
    idw_power: float = 0.0
    ra_bytes: int = 0
    dec_bytes: int = 0
    snr_bytes: int = 0
    parse_ok: bool = True
    parse_error: str = ""


@dataclass
class InspectionReport:
    """完整检查报告。"""
    path: str
    filesize: int

    # 固定头
    magic: bytes = b""
    magic_ok: bool = False
    version: int = 0
    version_ok: bool = False
    flags: int = 0
    dense_mode: bool = False
    json_uncomp_len: int = 0
    json_comp_len: int = 0
    n_pix_header: int = 0

    # JSON provenance
    provenance: Dict[str, Any] = field(default_factory=dict)
    provenance_ok: bool = False
    provenance_missing: List[str] = field(default_factory=list)
    format_version_ok: bool = False

    # 块索引
    n_chunks_json: int = 0
    n_chunks_index: int = 0
    chunk_index_offset: int = 0
    chunk_index_size: int = 0
    chunks: List[ChunkInspection] = field(default_factory=list)
    n_pix_sum_raw: int = 0
    n_pix_consistent: bool = False

    # SNR
    snr: SnrInspection = field(default_factory=lambda: SnrInspection(False, 0, 0))

    # Footer
    footer_chunk_index_offset: int = 0
    footer_chunk_index_size: int = 0
    footer_snr_block_offset: int = 0
    footer_snr_block_size: int = 0
    footer_global_crc32: int = 0
    footer_magic_trailer: bytes = b""
    footer_magic_ok: bool = False

    # 全局 CRC32
    global_crc32_stored: int = 0
    global_crc32_actual: int = 0
    global_crc32_ok: bool = False

    # 文件大小一致性 (契约 §13.10)
    expected_filesize: int = 0
    filesize_ok: bool = False

    # 整体结论
    all_ok: bool = False
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)


# ============================================================================
# Inspector
# ============================================================================

class HissV2Inspector:
    """HISS V2 文件检查器。

    直接解析二进制 (不依赖 HissV2Reader 的异常路径), 以便在文件损坏时
    仍能输出尽可能完整的结构报告, 并标注每项校验通过/失败。
    """

    def __init__(self, path: str):
        self._path = path

    def inspect(self) -> InspectionReport:
        """执行完整检查, 返回 InspectionReport。"""
        report = InspectionReport(path=self._path, filesize=0)

        if not os.path.isfile(self._path):
            report.errors.append(f"文件不存在: {self._path}")
            return report

        with open(self._path, "rb") as f:
            raw = f.read()
        report.filesize = len(raw)

        # ---- 1. 固定头 (契约 §5.1) ----
        self._inspect_fixed_header(raw, report)

        # 固定头损坏时后续无法继续
        if not report.magic_ok or len(raw) < hv.FIXED_HEADER_SIZE:
            report.all_ok = False
            return report

        # ---- 2. JSON provenance (契约 §5.2) ----
        self._inspect_provenance(raw, report)

        # ---- 3. Footer (契约 §9) ----
        self._inspect_footer(raw, report)

        # ---- 4. 块索引 (契约 §6.2) ----
        # 仅当 footer 解析成功时才尝试, 否则用 JSON 中的 n_chunks
        self._inspect_chunk_index(raw, report)

        # ---- 5. SNR 块 (契约 §8) ----
        self._inspect_snr_block(raw, report)

        # ---- 6. 全局 CRC32 (契约 §12.3) ----
        self._inspect_global_crc(raw, report)

        # ---- 7. 文件大小一致性 (契约 §13.10) ----
        self._inspect_filesize(raw, report)

        # ---- 汇总 ----
        report.all_ok = (
            report.magic_ok and report.version_ok and report.provenance_ok
            and report.format_version_ok and report.footer_magic_ok
            and report.global_crc32_ok and report.filesize_ok
            and report.n_pix_consistent
            and all(c.crc32_ok and c.decompress_ok for c in report.chunks)
            and report.snr.parse_ok
            and len(report.errors) == 0
        )
        return report

    # --------------------------------------------------------------------

    def _inspect_fixed_header(self, raw: bytes, r: InspectionReport):
        if len(raw) < hv.FIXED_HEADER_SIZE:
            r.errors.append(f"文件过短 ({len(raw)}B < {hv.FIXED_HEADER_SIZE}B), 无法读取固定头")
            return
        magic, version, flags, json_uncomp_len, json_comp_len, n_pix = struct.unpack_from(
            "<4sHHIIQ", raw, 0)
        r.magic = magic
        r.magic_ok = (magic == hv.MAGIC)
        if not r.magic_ok:
            r.errors.append(f"magic 不匹配: {magic!r} (期望 {hv.MAGIC!r})")
        r.version = version
        r.version_ok = (version == hv.VERSION)
        if not r.version_ok:
            r.errors.append(f"version 不支持: {version} (期望 {hv.VERSION})")
        r.flags = flags
        r.dense_mode = bool(flags & hv.FLAG_DENSE_MODE)
        r.json_uncomp_len = json_uncomp_len
        r.json_comp_len = json_comp_len
        r.n_pix_header = int(n_pix)

    def _inspect_provenance(self, raw: bytes, r: InspectionReport):
        json_start = hv.FIXED_HEADER_SIZE
        json_end = json_start + r.json_comp_len
        if json_end > len(raw):
            r.errors.append(f"JSON 头长度越界: json_comp_len={r.json_comp_len} 文件大小={len(raw)}")
            return
        try:
            json_bytes = hv._zstd_decompress(raw[json_start:json_end])
        except Exception as e:
            r.errors.append(f"JSON 头 zstd 解压失败: {e}")
            return
        if len(json_bytes) != r.json_uncomp_len:
            r.errors.append(
                f"JSON 解压长度不一致: {len(json_bytes)} != json_uncomp_len={r.json_uncomp_len}")
            return
        try:
            prov = json.loads(json_bytes.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            r.errors.append(f"JSON 解析失败: {e}")
            return
        r.provenance = prov
        r.provenance_ok = True
        # 必填字段 (契约 §5.2)
        missing = hv.REQUIRED_PROVENANCE_FIELDS - set(prov.keys())
        r.provenance_missing = sorted(missing)
        if missing:
            r.errors.append(f"provenance 缺少必填字段: {sorted(missing)}")
            r.provenance_ok = False
        # format_version
        fv = prov.get("format_version")
        r.format_version_ok = (fv == hv.FORMAT_VERSION_STR)
        if not r.format_version_ok:
            r.errors.append(f"format_version 不匹配: {fv!r} (期望 {hv.FORMAT_VERSION_STR!r})")
        r.n_chunks_json = int(prov.get("n_chunks", 0))

    def _inspect_footer(self, raw: bytes, r: InspectionReport):
        if len(raw) < hv.FOOTER_SIZE:
            r.errors.append(f"文件过短 ({len(raw)}B < {hv.FOOTER_SIZE}B), 无法读取 footer")
            return
        footer_off = len(raw) - hv.FOOTER_SIZE
        (cio, cis, sbo, sbs, gcrc, _resv, mtrail, _pad) = struct.unpack_from(
            "<QQQQII4s4s", raw, footer_off)
        r.footer_chunk_index_offset = cio
        r.footer_chunk_index_size = cis
        r.footer_snr_block_offset = sbo
        r.footer_snr_block_size = sbs
        r.footer_global_crc32 = gcrc
        r.footer_magic_trailer = mtrail
        r.footer_magic_ok = (mtrail == hv.MAGIC_TRAILER)
        if not r.footer_magic_ok:
            r.errors.append(f"footer magic_trailer 不匹配: {mtrail!r} (文件截断)")

    def _inspect_chunk_index(self, raw: bytes, r: InspectionReport):
        # 用 footer 给出的偏移优先; 若 footer 损坏, 回退到 header+json 推算
        if r.footer_magic_ok and r.footer_chunk_index_size > 0:
            cio = r.footer_chunk_index_offset
            cis = r.footer_chunk_index_size
        else:
            cio = hv.FIXED_HEADER_SIZE + r.json_comp_len
            cis = r.n_chunks_json * hv.CHUNK_INDEX_ENTRY_SIZE
        r.chunk_index_offset = cio
        r.chunk_index_size = cis

        n_chunks = cis // hv.CHUNK_INDEX_ENTRY_SIZE if cis > 0 else r.n_chunks_json
        r.n_chunks_index = n_chunks

        if cio + cis > len(raw):
            r.errors.append(f"块索引表越界: cio={cio} cis={cis} filesize={len(raw)}")
            return

        for i in range(n_chunks):
            pos = cio + i * hv.CHUNK_INDEX_ENTRY_SIZE
            (offset, comp_size, raw_count, crc32, codec, cflags, reserved) = struct.unpack_from(
                "<QIIIBBH", raw, pos)
            entry = ChunkInspection(
                index=i, offset=offset, comp_size=comp_size, raw_count=raw_count,
                crc32_stored=crc32, crc32_actual=0, crc32_ok=False,
                codec=codec, codec_name=hv.CODEC_ID_TO_NAME.get(codec, f"UNKNOWN({codec})"),
                flags=cflags, reserved=reserved,
            )
            # per-chunk CRC32 校验
            if offset + comp_size > len(raw):
                entry.decompress_ok = False
                entry.decompress_error = f"块数据越界: offset={offset} comp_size={comp_size}"
                r.errors.append(f"chunk {i}: {entry.decompress_error}")
                r.chunks.append(entry)
                continue
            comp_data = raw[offset:offset + comp_size]
            entry.crc32_actual = hv._crc32(comp_data)
            entry.crc32_ok = (entry.crc32_actual == crc32)
            if not entry.crc32_ok:
                r.errors.append(
                    f"chunk {i}: per-chunk CRC32 失败 stored={crc32:#010x} actual={entry.crc32_actual:#010x}")

            # 解压以获取 ipix 范围 (契约 §6.4 块间全局升序)
            try:
                if codec == hv.CODEC_ZSTD:
                    raw_buf = hv._zstd_decompress(comp_data)
                elif codec == hv.CODEC_NONE:
                    raw_buf = comp_data
                else:
                    entry.decompress_ok = False
                    entry.decompress_error = f"不支持的 codec: {codec}"
                    r.warnings.append(f"chunk {i}: {entry.decompress_error}")
                    r.chunks.append(entry)
                    continue
                expected = raw_count * 13
                if len(raw_buf) != expected:
                    entry.decompress_ok = False
                    entry.decompress_error = f"解压长度不一致: {len(raw_buf)} != {expected}"
                    r.errors.append(f"chunk {i}: {entry.decompress_error}")
                    r.chunks.append(entry)
                    continue
                if raw_count > 0:
                    ipix = np.frombuffer(raw_buf, dtype="<u8", count=raw_count, offset=0)
                    entry.first_ipix = int(ipix[0])
                    entry.last_ipix = int(ipix[-1])
            except Exception as e:
                entry.decompress_ok = False
                entry.decompress_error = f"解压异常: {e}"
                r.errors.append(f"chunk {i}: {entry.decompress_error}")

            r.chunks.append(entry)

        # n_pix 一致性 (契约 §13.6): header == json == sum(raw_count)
        r.n_pix_sum_raw = sum(c.raw_count for c in r.chunks)
        r.n_pix_consistent = (
            r.n_pix_header == int(r.provenance.get("n_pix", -1)) == r.n_pix_sum_raw
        )
        if not r.n_pix_consistent:
            r.errors.append(
                f"n_pix 不一致: header={r.n_pix_header} json={r.provenance.get('n_pix')} "
                f"sum(raw_count)={r.n_pix_sum_raw}")

    def _inspect_snr_block(self, raw: bytes, r: InspectionReport):
        has_snr = bool(r.provenance.get("has_snr", False))
        if r.footer_magic_ok:
            off = r.footer_snr_block_offset
            size = r.footer_snr_block_size
        else:
            off = 0
            size = 0
        snr = SnrInspection(has_snr=has_snr, offset=off, size=size)
        if not has_snr or off == 0 or size == 0:
            r.snr = snr
            return
        if off + size > len(raw):
            snr.parse_ok = False
            snr.parse_error = f"SNR 块越界: offset={off} size={size} filesize={len(raw)}"
            r.errors.append(f"SNR: {snr.parse_error}")
            r.snr = snr
            return
        try:
            pos = off
            n_points = struct.unpack_from("<I", raw, pos)[0]
            pos += 4
            snr.n_points = n_points
            # 三通道
            ra_raw, ra_consumed = hv._decompress_snr_channel(raw, pos)
            pos += ra_consumed
            dec_raw, dec_consumed = hv._decompress_snr_channel(raw, pos)
            pos += dec_consumed
            snr_raw, snr_consumed = hv._decompress_snr_channel(raw, pos)
            pos += snr_consumed
            snr.ra_bytes = len(ra_raw)
            snr.dec_bytes = len(dec_raw)
            snr.snr_bytes = len(snr_raw)
            # 3 标量
            snr.snr_phot, snr.median_snr, snr.idw_power = struct.unpack_from("<ddd", raw, pos)
        except Exception as e:
            snr.parse_ok = False
            snr.parse_error = f"SNR 解析异常: {e}"
            r.errors.append(f"SNR: {snr.parse_error}")
        r.snr = snr

    def _inspect_global_crc(self, raw: bytes, r: InspectionReport):
        if len(raw) < hv.FOOTER_SIZE:
            return
        footer_off = len(raw) - hv.FOOTER_SIZE
        r.global_crc32_stored = r.footer_global_crc32
        r.global_crc32_actual = hv._crc32(raw[:footer_off])
        r.global_crc32_ok = (r.global_crc32_actual == r.footer_global_crc32)
        if not r.global_crc32_ok:
            r.errors.append(
                f"全局 CRC32 校验失败: stored={r.footer_global_crc32:#010x} "
                f"actual={r.global_crc32_actual:#010x}")

    def _inspect_filesize(self, raw: bytes, r: InspectionReport):
        if not r.footer_magic_ok or not r.chunks:
            return
        expected = (r.chunk_index_offset + r.chunk_index_size
                    + sum(c.comp_size for c in r.chunks)
                    + r.snr.size + hv.FOOTER_SIZE)
        r.expected_filesize = expected
        r.filesize_ok = (expected == r.filesize)
        if not r.filesize_ok:
            r.errors.append(f"文件大小不一致: expected={expected} actual={r.filesize}")


# ============================================================================
# 报告格式化
# ============================================================================

def format_report(r: InspectionReport) -> str:
    """格式化报告为人类可读字符串。"""
    lines: List[str] = []
    sep = "=" * 72

    lines.append(sep)
    lines.append("HISS V2 Inspector Report")
    lines.append(sep)
    lines.append(f"文件: {r.path}")
    lines.append(f"文件大小: {r.filesize} B")
    lines.append(f"整体结论: {'PASS ✓' if r.all_ok else 'FAIL ✗'}")
    lines.append("")

    # ---- 固定头 ----
    lines.append("-" * 72)
    lines.append("[1] FIXED HEADER (24B, 契约 §5.1)")
    lines.append("-" * 72)
    lines.append(f"  magic            = {r.magic!r}   {'✓' if r.magic_ok else '✗'} (期望 {hv.MAGIC!r})")
    lines.append(f"  version          = {r.version}    {'✓' if r.version_ok else '✗'} (期望 {hv.VERSION})")
    lines.append(f"  flags            = {r.flags:#06x}  dense_mode={r.dense_mode}")
    lines.append(f"  json_uncomp_len  = {r.json_uncomp_len}")
    lines.append(f"  json_comp_len    = {r.json_comp_len}")
    lines.append(f"  n_pix (header)   = {r.n_pix_header}")
    lines.append("")

    # ---- JSON provenance ----
    lines.append("-" * 72)
    lines.append("[2] JSON PROVENANCE (契约 §5.2)")
    lines.append("-" * 72)
    if r.provenance_ok:
        lines.append(f"  解析状态: ✓  必填字段齐全: {'✓' if not r.provenance_missing else '✗'}")
        lines.append(f"  format_version = {r.provenance.get('format_version')!r}  "
                     f"{'✓' if r.format_version_ok else '✗'} (期望 {hv.FORMAT_VERSION_STR!r})")
        if r.provenance_missing:
            lines.append(f"  缺失字段: {r.provenance_missing}")
        lines.append(f"  provenance 全部字段 ({len(r.provenance)} 项):")
        for k in sorted(r.provenance.keys()):
            v = r.provenance[k]
            # 复杂对象截断显示
            vs = _format_value(v)
            lines.append(f"    {k} = {vs}")
    else:
        lines.append(f"  解析状态: ✗  (见 errors)")
    lines.append("")

    # ---- 块索引 ----
    lines.append("-" * 72)
    lines.append("[3] CHUNK INDEX (契约 §6.2, 24B/项)")
    lines.append("-" * 72)
    lines.append(f"  chunk_index_offset = {r.chunk_index_offset}")
    lines.append(f"  chunk_index_size   = {r.chunk_index_size}")
    lines.append(f"  n_chunks (json)    = {r.n_chunks_json}")
    lines.append(f"  n_chunks (index)   = {r.n_chunks_index}")
    lines.append(f"  n_pix 一致性: header={r.n_pix_header} json={r.provenance.get('n_pix')} "
                 f"sum(raw_count)={r.n_pix_sum_raw}  {'✓' if r.n_pix_consistent else '✗'}")
    lines.append("")
    if r.chunks:
        lines.append(f"  {'idx':>4} {'offset':>10} {'comp_size':>10} {'raw_count':>10} "
                     f"{'crc32(stored)':>14} {'crc32(actual)':>14} {'ok':>3} "
                     f"{'codec':>6} {'first_ipix':>12} {'last_ipix':>12}")
        for c in r.chunks:
            lines.append(
                f"  {c.index:>4} {c.offset:>10} {c.comp_size:>10} {c.raw_count:>10} "
                f"{c.crc32_stored:#014x} {c.crc32_actual:#014x} {'✓' if c.crc32_ok else '✗':>3} "
                f"{c.codec_name:>6} "
                f"{c.first_ipix if c.first_ipix is not None else '-':>12} "
                f"{c.last_ipix if c.last_ipix is not None else '-':>12}")
            if not c.decompress_ok:
                lines.append(f"        解压失败: {c.decompress_error}")
    lines.append("")

    # ---- SNR 块 ----
    lines.append("-" * 72)
    lines.append("[4] SNR SPARSE BLOCK (契约 §8)")
    lines.append("-" * 72)
    s = r.snr
    lines.append(f"  has_snr        = {s.has_snr}")
    lines.append(f"  offset         = {s.offset}")
    lines.append(f"  size           = {s.size}")
    if s.has_snr:
        lines.append(f"  n_points       = {s.n_points}")
        lines.append(f"  ra_bytes       = {s.ra_bytes}  (期望 n_points×8 = {s.n_points * 8})")
        lines.append(f"  dec_bytes      = {s.dec_bytes}  (期望 n_points×8 = {s.n_points * 8})")
        lines.append(f"  snr_bytes      = {s.snr_bytes}  (期望 n_points×4 = {s.n_points * 4})")
        lines.append(f"  snr_phot       = {s.snr_phot}")
        lines.append(f"  median_snr     = {s.median_snr}")
        lines.append(f"  idw_power      = {s.idw_power}")
        lines.append(f"  解析状态       = {'✓' if s.parse_ok else '✗'}")
        if not s.parse_ok:
            lines.append(f"  错误           = {s.parse_error}")
    lines.append("")

    # ---- Footer ----
    lines.append("-" * 72)
    lines.append("[5] FOOTER (48B, 契约 §9)")
    lines.append("-" * 72)
    lines.append(f"  chunk_index_offset = {r.footer_chunk_index_offset}")
    lines.append(f"  chunk_index_size   = {r.footer_chunk_index_size}")
    lines.append(f"  snr_block_offset   = {r.footer_snr_block_offset}")
    lines.append(f"  snr_block_size     = {r.footer_snr_block_size}")
    lines.append(f"  global_crc32       = {r.footer_global_crc32:#010x}")
    lines.append(f"  magic_trailer      = {r.footer_magic_trailer!r}  "
                 f"{'✓' if r.footer_magic_ok else '✗'} (期望 {hv.MAGIC_TRAILER!r})")
    lines.append("")

    # ---- 全局 CRC32 ----
    lines.append("-" * 72)
    lines.append("[6] GLOBAL CRC32 (契约 §12.3, 覆盖 [0, filesize-48))")
    lines.append("-" * 72)
    lines.append(f"  stored  = {r.global_crc32_stored:#010x}")
    lines.append(f"  actual  = {r.global_crc32_actual:#010x}")
    lines.append(f"  一致    = {'✓' if r.global_crc32_ok else '✗'}")
    lines.append("")

    # ---- 文件大小 ----
    lines.append("-" * 72)
    lines.append("[7] FILE SIZE (契约 §13.10)")
    lines.append("-" * 72)
    lines.append(f"  expected = {r.expected_filesize}")
    lines.append(f"  actual   = {r.filesize}")
    lines.append(f"  一致     = {'✓' if r.filesize_ok else '✗'}")
    lines.append("")

    # ---- 错误/警告 ----
    if r.errors:
        lines.append("-" * 72)
        lines.append(f"ERRORS ({len(r.errors)}):")
        lines.append("-" * 72)
        for e in r.errors:
            lines.append(f"  ✗ {e}")
    if r.warnings:
        lines.append("-" * 72)
        lines.append(f"WARNINGS ({len(r.warnings)}):")
        lines.append("-" * 72)
        for w in r.warnings:
            lines.append(f"  ! {w}")

    lines.append(sep)
    return "\n".join(lines)


def _format_value(v: Any) -> str:
    """格式化 provenance 值用于报告显示。"""
    if isinstance(v, str):
        return repr(v)
    if isinstance(v, (int, float, bool)) or v is None:
        return str(v)
    try:
        s = json.dumps(v, ensure_ascii=False)
        if len(s) > 80:
            return s[:77] + "..."
        return s
    except Exception:
        return str(v)


# ============================================================================
# CLI
# ============================================================================

def inspect_file(path: str) -> InspectionReport:
    """检查单个文件, 返回报告。"""
    return HissV2Inspector(path).inspect()


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="HISS V2 文件检查器")
    parser.add_argument("path", help="V2 HISS 文件路径 (.hiss2)")
    parser.add_argument("-o", "--output", default=None, help="报告输出文件 (默认 stdout)")
    parser.add_argument("--json", action="store_true", help="输出 JSON 格式报告")
    args = parser.parse_args(argv)

    report = inspect_file(args.path)
    if args.json:
        out = _report_to_dict(report)
        text = json.dumps(out, ensure_ascii=False, indent=2)
    else:
        text = format_report(report)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(text)
        print(f"报告已写入: {args.output}")
    else:
        print(text)

    return 0 if report.all_ok else 1


def _report_to_dict(r: InspectionReport) -> Dict[str, Any]:
    """将报告转为可 JSON 序列化的 dict。"""
    return {
        "path": r.path,
        "filesize": r.filesize,
        "all_ok": r.all_ok,
        "fixed_header": {
            "magic": r.magic.decode("latin-1", errors="replace"),
            "magic_ok": r.magic_ok,
            "version": r.version,
            "version_ok": r.version_ok,
            "flags": r.flags,
            "dense_mode": r.dense_mode,
            "json_uncomp_len": r.json_uncomp_len,
            "json_comp_len": r.json_comp_len,
            "n_pix_header": r.n_pix_header,
        },
        "provenance": r.provenance,
        "provenance_ok": r.provenance_ok,
        "provenance_missing": r.provenance_missing,
        "format_version_ok": r.format_version_ok,
        "chunk_index": {
            "offset": r.chunk_index_offset,
            "size": r.chunk_index_size,
            "n_chunks_json": r.n_chunks_json,
            "n_chunks_index": r.n_chunks_index,
            "n_pix_sum_raw": r.n_pix_sum_raw,
            "n_pix_consistent": r.n_pix_consistent,
            "chunks": [
                {
                    "index": c.index, "offset": c.offset, "comp_size": c.comp_size,
                    "raw_count": c.raw_count,
                    "crc32_stored": c.crc32_stored, "crc32_actual": c.crc32_actual,
                    "crc32_ok": c.crc32_ok,
                    "codec": c.codec_name, "flags": c.flags, "reserved": c.reserved,
                    "first_ipix": c.first_ipix, "last_ipix": c.last_ipix,
                    "decompress_ok": c.decompress_ok,
                    "decompress_error": c.decompress_error,
                } for c in r.chunks
            ],
        },
        "snr": {
            "has_snr": r.snr.has_snr, "offset": r.snr.offset, "size": r.snr.size,
            "n_points": r.snr.n_points,
            "snr_phot": r.snr.snr_phot, "median_snr": r.snr.median_snr,
            "idw_power": r.snr.idw_power,
            "ra_bytes": r.snr.ra_bytes, "dec_bytes": r.snr.dec_bytes,
            "snr_bytes": r.snr.snr_bytes,
            "parse_ok": r.snr.parse_ok, "parse_error": r.snr.parse_error,
        },
        "footer": {
            "chunk_index_offset": r.footer_chunk_index_offset,
            "chunk_index_size": r.footer_chunk_index_size,
            "snr_block_offset": r.footer_snr_block_offset,
            "snr_block_size": r.footer_snr_block_size,
            "global_crc32": r.footer_global_crc32,
            "magic_trailer": r.footer_magic_trailer.decode("latin-1", errors="replace"),
            "magic_ok": r.footer_magic_ok,
        },
        "global_crc32": {
            "stored": r.global_crc32_stored, "actual": r.global_crc32_actual,
            "ok": r.global_crc32_ok,
        },
        "filesize": {
            "expected": r.expected_filesize, "actual": r.filesize,
            "ok": r.filesize_ok,
        },
        "errors": r.errors,
        "warnings": r.warnings,
    }


if __name__ == "__main__":
    sys.exit(main())
