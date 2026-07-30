# -*- coding: utf-8 -*-
"""
hiss_v2.py - HISS (HEALPix Storage System) V2 读写器 (Python 参考实现)

实现冻结契约 engineering_authoritative/contracts/HISS_FORMAT_V2.md 的全部规范:
  - 固定头 24B + JSON provenance (zstd) + 分块索引 + 数据块 (每块独立 zstd) + SNR 稀疏块 + footer 48B
  - signal=float32 (禁止量化为 uint8), support=uint8 (无覆盖不得写成零)
  - 分块索引支持 O(1) 随机读取与 batch read (7 个 API)
  - CRC32 IEEE 802.3 (zlib) per-chunk + global 双层校验
  - V1→V2 迁移转换器 (纯 Python V1 读取, 不依赖 DLL)

二进制布局见契约 §4。本文件为 SSOT 契约的参考实现, 任何歧义以契约文档为准。
"""

from __future__ import annotations

import json
import logging
import os
import struct
import zlib
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Sequence, Tuple

import numpy as np
import zstandard as zstd

logger = logging.getLogger(__name__)

# ============================================================================
# 常量 (FROZEN, 契约 §5.1 / §6.2 / §9 / §10 / §11.8)
# ============================================================================

MAGIC = b"HI2S"                       # 固定头 magic (契约 §5.1)
MAGIC_TRAILER = b"HI2S"               # footer magic_trailer (契约 §9)
VERSION = 2                            # 固定头 version (FROZEN=2)
FORMAT_VERSION_STR = "HISS-V2"         # JSON format_version 字段 (契约 §5.2)

CHUNK_SIZE_DEFAULT = 4096              # 分块像素数默认值 (FROZEN, 契约 §6.1)
FIXED_HEADER_SIZE = 24                 # 固定头字节数 (FROZEN, 契约 §5.1)
CHUNK_INDEX_ENTRY_SIZE = 24            # 块索引项字节数 (FROZEN, 契约 §6.2)
FOOTER_SIZE = 48                       # footer 字节数 (FROZEN, 契约 §9)

ZSTD_LEVEL = 5                         # zstd 压缩级别 (契约 §10.1)

# codec 枚举 (契约 §10.1)
CODEC_NONE = 0
CODEC_ZSTD = 1
CODEC_LZ4 = 2
CODEC_NAME_TO_ID = {"NONE": CODEC_NONE, "ZSTD": CODEC_ZSTD, "LZ4": CODEC_LZ4}
CODEC_ID_TO_NAME = {v: k for k, v in CODEC_NAME_TO_ID.items()}

# flags 位 (契约 §5.1)
FLAG_DENSE_MODE = 0x0001

# CRC 算法标识 (FROZEN, 契约 §12.1)
CRC_ALGORITHM = "CRC32_IEEE8023"

# 错误码 (FROZEN, 契约 §11.8)
HIO_OK = 0
HIO_ERR_FILE = -1          # 文件不存在 / 无法打开
HIO_ERR_MAGIC = -2         # magic 不匹配
HIO_ERR_VERSION = -3       # version 不支持
HIO_ERR_CRC = -4           # CRC32 校验失败
HIO_ERR_CHUNK_RANGE = -5   # chunk 索引越界
HIO_ERR_CODEC = -6         # codec 不支持
HIO_ERR_JSON = -7          # JSON 解析失败 / 必填字段缺失
HIO_ERR_FOOTER = -8        # footer magic_trailer 不匹配 (文件截断)
HIO_ERR_MEM = -9           # 内存分配失败
HIO_ERR_NO_SNR = -10       # has_snr=false 但请求读 SNR

# JSON provenance 必填字段 (契约 §5.2)
REQUIRED_PROVENANCE_FIELDS = {
    "format_version", "nside", "ordering", "nested", "n_pix", "filter",
    "exposure_s", "obs_time", "pixfrac", "wcs", "drizzle", "fits_meta",
    "source", "has_snr", "chunk_size", "n_chunks", "codec", "crc_algorithm",
}


# ============================================================================
# 异常
# ============================================================================

class HissV2Error(Exception):
    """HISS V2 错误, 携带契约错误码 (§11.8)。"""

    def __init__(self, code: int, message: str):
        self.code = code
        self.message = message
        super().__init__(f"[{code}] {message}")


# ============================================================================
# 数据类
# ============================================================================

@dataclass
class HissV2SnrModel:
    """SNR 稀疏控制点模型 (契约 §8, SoA 布局)。"""
    n_points: int
    points_ra: np.ndarray   # f64 [n_points] 赤经 (度)
    points_dec: np.ndarray  # f64 [n_points] 赤纬 (度)
    points_snr: np.ndarray  # f32 [n_points] SNR 值 (A-B)/mad
    snr_phot: float         # 1/(ln10×sigma_residual)
    median_snr: float       # median(snr_psf) 归一化基准
    idw_power: float        # IDW 幂次 (默认 2.0)


@dataclass
class HissV2ChunkEntry:
    """块索引项 (契约 §6.2, 24B)。"""
    offset: int         # uint64: 压缩数据文件偏移
    comp_size: int      # uint32: 压缩后字节数
    raw_count: int      # uint32: 该块原始像素数
    crc32: int          # uint32: 压缩后数据 CRC32
    codec: int          # uint8: 0=NONE/1=ZSTD/2=LZ4
    flags: int = 0      # uint8: 块级标志 (保留=0)
    reserved: int = 0   # uint16: 保留=0


# ============================================================================
# CRC32 / zstd 辅助
# ============================================================================

def _crc32(data: bytes) -> int:
    """CRC32 IEEE 802.3 (与 zlib.crc32 一致, 契约 §12.1)。返回无符号 32 位。"""
    return zlib.crc32(data) & 0xFFFFFFFF


def _zstd_compress(data: bytes, level: int = ZSTD_LEVEL) -> bytes:
    """zstd 压缩 (契约 §10.1, level=5)。"""
    cctx = zstd.ZstdCompressor(level=level)
    return cctx.compress(data)


def _zstd_decompress(data: bytes) -> bytes:
    """zstd 解压。"""
    dctx = zstd.ZstdDecompressor()
    return dctx.decompress(data)


# ============================================================================
# V1 纯 Python 读取器 (用于 V1→V2 迁移, 不依赖 DLL)
# 契约 §14.2/§14.3: B-002 三帧为 V1 snr_format=1 稀疏格式
# ============================================================================

_V1_MAGIC = b"HISS"

# V1 SNR 控制点: packed 20B = ra(f64) + dec(f64) + snr_psf(f32)
_V1_SNR_POINT_FMT = "<ddf"
_V1_SNR_POINT_SIZE = struct.calcsize(_V1_SNR_POINT_FMT)  # 20


def v1_read_snr_model(path: str) -> Tuple[int, bool, np.ndarray, np.ndarray,
                                          Dict[str, Any], Optional[HissV2SnrModel]]:
    """纯 Python 读取 V1 .hiss 文件 (snr_format=1 稀疏 SNR)。

    V1 布局 (HEALPIX_FORMAT_SPEC.md §2.2):
      magic[4] + uncomp_len(u32) + comp_len(u32) + zstd(JSON)
      + ipix[n_pix×u64] + pixel[n_pix×f32]
      + (has_snr && snr_format=1): n_points(u32) + points[n×20B] + 3×f64

    Returns:
        (nside, nested, ipix, pixel, meta, snr_model) 元组。
        snr_model 为 HissV2SnrModel 或 None (无 SNR 时)。
    """
    if not os.path.isfile(path):
        raise HissV2Error(HIO_ERR_FILE, f"V1 文件不存在: {path}")

    with open(path, "rb") as f:
        data = f.read()

    if len(data) < 12:
        raise HissV2Error(HIO_ERR_MAGIC, "V1 文件过短")
    if data[:4] != _V1_MAGIC:
        raise HissV2Error(HIO_ERR_MAGIC, f"V1 magic 不匹配: {data[:4]!r}")

    uncomp_len, comp_len = struct.unpack_from("<II", data, 4)
    json_start = 12
    json_end = json_start + comp_len
    if json_end > len(data):
        raise HissV2Error(HIO_ERR_JSON, "V1 JSON 头长度越界")

    json_bytes = _zstd_decompress(data[json_start:json_end])
    if len(json_bytes) != uncomp_len:
        raise HissV2Error(HIO_ERR_JSON,
                          f"V1 JSON 解压长度不一致: {len(json_bytes)} != {uncomp_len}")
    meta = json.loads(json_bytes.decode("utf-8"))

    nside = int(meta["nside"])
    nested = bool(meta["nested"])
    n_pix = int(meta["n_pix"])
    has_snr = bool(meta.get("has_snr", False))
    snr_format = int(meta.get("snr_format", 0))
    snr_n_points = int(meta.get("snr_n_points", 0))

    pos = json_end
    # ipix + pixel
    ipix = np.frombuffer(data, dtype="<u8", count=n_pix, offset=pos).astype(np.uint64)
    pos += n_pix * 8
    pixel = np.frombuffer(data, dtype="<f4", count=n_pix, offset=pos).astype(np.float32)
    pos += n_pix * 4

    snr_model: Optional[HissV2SnrModel] = None
    if has_snr and snr_format == 1:
        # n_points (u32)
        n_points = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        if snr_n_points and n_points != snr_n_points:
            logger.warning("V1 snr_n_points 不一致 (json=%d 实际=%d), 以实际为准",
                           snr_n_points, n_points)
        # points: n_points × 20B (ra f64, dec f64, snr_psf f32, packed)
        pts_bytes = data[pos:pos + n_points * _V1_SNR_POINT_SIZE]
        pos += n_points * _V1_SNR_POINT_SIZE
        # 拆为 SoA
        ra = np.empty(n_points, dtype=np.float64)
        dec = np.empty(n_points, dtype=np.float64)
        snr = np.empty(n_points, dtype=np.float32)
        for i in range(n_points):
            r, d, s = struct.unpack_from(_V1_SNR_POINT_FMT, pts_bytes, i * _V1_SNR_POINT_SIZE)
            ra[i] = r
            dec[i] = d
            snr[i] = s
        # 3 scalars: snr_phot, median_snr, idw_power (f64 × 3)
        snr_phot, median_snr, idw_power = struct.unpack_from("<ddd", data, pos)
        pos += 24
        snr_model = HissV2SnrModel(
            n_points=n_points,
            points_ra=ra,
            points_dec=dec,
            points_snr=snr,
            snr_phot=float(snr_phot),
            median_snr=float(median_snr),
            idw_power=float(idw_power),
        )

    logger.info("V1 读取完成: %s nside=%d n_pix=%d has_snr=%s snr_fmt=%s n_points=%s",
                os.path.basename(path), nside, n_pix, has_snr, snr_format,
                snr_model.n_points if snr_model else 0)
    return nside, nested, ipix, pixel, meta, snr_model


# ============================================================================
# HissV2Writer
# ============================================================================

class HissV2Writer:
    """HISS V2 写入器 (契约 §4 二进制布局)。

    写入流程: 构建 provenance → 分块压缩数据 → 写 header+json+index+data+snr →
              计算全局 CRC32 → 写 footer。
    """

    def __init__(self, path: str):
        self._path = path

    def write(self, nside: int, nested: bool, ipix: np.ndarray, signal: np.ndarray,
              support: np.ndarray, provenance: Dict[str, Any],
              snr_model: Optional[HissV2SnrModel] = None,
              chunk_size: int = CHUNK_SIZE_DEFAULT,
              codec: str = "ZSTD",
              dense_mode: bool = False) -> int:
        """写入 V2 文件。

        Args:
            nside: HEALPix nside 参数
            nested: 是否 nested 排序
            ipix: uint64 [n_pix] 像素索引 (升序)
            signal: float32 [n_pix] 信号值 (不得量化为 uint8)
            support: uint8 [n_pix] 覆盖标记 (无覆盖不得写成零)
            provenance: provenance 字段 (wcs/drizzle/fits_meta/source/filter 等)
            snr_model: 可选 SNR 稀疏模型
            chunk_size: 分块像素数 (默认 4096)
            codec: 压缩编码 "ZSTD"/"NONE"/"LZ4"
            dense_mode: 是否稠密模式 (False=稀疏, 默认)

        Returns:
            0=成功

        Raises:
            HissV2Error: 写入失败
        """
        # ---- 参数校验 ----
        ipix = np.ascontiguousarray(ipix, dtype="<u8")
        signal = np.ascontiguousarray(signal, dtype="<f4")
        support = np.ascontiguousarray(support, dtype="<u1")
        n_pix = int(ipix.size)
        if signal.size != n_pix or support.size != n_pix:
            raise HissV2Error(HIO_ERR_FILE,
                              f"ipix/signal/support 长度不一致: {n_pix}/{signal.size}/{support.size}")
        if codec not in CODEC_NAME_TO_ID:
            raise HissV2Error(HIO_ERR_CODEC, f"不支持的 codec: {codec}")
        if n_pix > 0 and chunk_size <= 0:
            raise HissV2Error(HIO_ERR_FILE, f"chunk_size 必须为正: {chunk_size}")

        codec_id = CODEC_NAME_TO_ID[codec]
        n_chunks = (n_pix + chunk_size - 1) // chunk_size if chunk_size > 0 else 0
        if n_chunks == 0:
            n_chunks = 1  # 至少 1 个块 (空文件情况)
        flags = FLAG_DENSE_MODE if dense_mode else 0
        has_snr = snr_model is not None and snr_model.n_points > 0

        # ---- 构建 provenance JSON (契约 §5.2 必填字段) ----
        ordering = "NESTED" if nested else "RING"
        prov = dict(provenance)  # 复制, 不修改调用者
        prov.update({
            "format_version": FORMAT_VERSION_STR,
            "nside": int(nside),
            "ordering": ordering,
            "nested": bool(nested),
            "n_pix": n_pix,
            "has_snr": has_snr,
            "chunk_size": int(chunk_size),
            "n_chunks": int(n_chunks),
            "codec": codec,
            "crc_algorithm": CRC_ALGORITHM,
        })
        # 确保必填字段存在
        missing = REQUIRED_PROVENANCE_FIELDS - set(prov.keys())
        if missing:
            raise HissV2Error(HIO_ERR_JSON, f"provenance 缺少必填字段: {missing}")

        json_bytes = json.dumps(prov, ensure_ascii=False).encode("utf-8")
        json_comp = _zstd_compress(json_bytes, ZSTD_LEVEL)
        json_uncomp_len = len(json_bytes)
        json_comp_len = len(json_comp)

        # ---- 分块压缩数据 (契约 §6.3: ipix+signal+support 打包) ----
        chunk_entries: List[HissV2ChunkEntry] = []
        chunk_comp_data: List[bytes] = []
        for c in range(n_chunks):
            lo = c * chunk_size
            hi = min((c + 1) * chunk_size, n_pix)
            raw_count = hi - lo if n_pix > 0 else 0
            if raw_count == 0:
                # 空块 (n_pix=0 时仅 1 个空块)
                raw_buf = b""
            else:
                ipix_chunk = ipix[lo:hi]
                sig_chunk = signal[lo:hi]
                sup_chunk = support[lo:hi]
                raw_buf = (ipix_chunk.tobytes() + sig_chunk.tobytes() + sup_chunk.tobytes())
            # 压缩
            if codec_id == CODEC_ZSTD:
                comp = _zstd_compress(raw_buf, ZSTD_LEVEL)
            elif codec_id == CODEC_NONE:
                comp = raw_buf
            else:
                raise HissV2Error(HIO_ERR_CODEC, f"codec 未实现: {codec}")
            chunk_comp_data.append(comp)
            chunk_entries.append(HissV2ChunkEntry(
                offset=0,  # 稍后填入
                comp_size=len(comp),
                raw_count=raw_count,
                crc32=_crc32(comp),
                codec=codec_id,
            ))

        # ---- 计算各段偏移 ----
        chunk_index_offset = FIXED_HEADER_SIZE + json_comp_len
        chunk_index_size = n_chunks * CHUNK_INDEX_ENTRY_SIZE
        data_start = chunk_index_offset + chunk_index_size
        # 填入每块 offset
        cur = data_start
        for entry in chunk_entries:
            entry.offset = cur
            cur += entry.comp_size
        snr_block_offset = cur
        snr_block_bytes = b""
        if has_snr:
            snr_block_bytes = self._build_snr_block(snr_model)
        snr_block_size = len(snr_block_bytes)

        # ---- 写文件 + 增量计算全局 CRC32 (覆盖 [0, filesize-48), 契约 §12.3) ----
        # zlib.crc32 支持增量更新: crc = zlib.crc32(data, crc); 起始 crc=0
        global_crc = 0
        with open(self._path, "wb") as fp:
            # FIXED HEADER (24B)
            header = struct.pack("<4sHHIIQ", MAGIC, VERSION, flags,
                                 json_uncomp_len, json_comp_len, n_pix)
            fp.write(header)
            global_crc = zlib.crc32(header, global_crc)
            # COMPRESSED JSON
            fp.write(json_comp)
            global_crc = zlib.crc32(json_comp, global_crc)
            # CHUNK INDEX
            for entry in chunk_entries:
                idx_bytes = struct.pack("<QIIIBBH",
                                        entry.offset, entry.comp_size, entry.raw_count,
                                        entry.crc32, entry.codec, entry.flags, entry.reserved)
                fp.write(idx_bytes)
                global_crc = zlib.crc32(idx_bytes, global_crc)
            # DATA CHUNKS
            for comp in chunk_comp_data:
                fp.write(comp)
                global_crc = zlib.crc32(comp, global_crc)
            # SNR BLOCK
            if has_snr:
                fp.write(snr_block_bytes)
                global_crc = zlib.crc32(snr_block_bytes, global_crc)
            global_crc &= 0xFFFFFFFF  # 无符号化
            # FOOTER (48B) — footer 自身不参与全局 CRC32 (契约 §12.3)
            footer = struct.pack("<QQQQII4s4s",
                                 chunk_index_offset, chunk_index_size,
                                 snr_block_offset, snr_block_size,
                                 global_crc, 0, MAGIC_TRAILER, b"\x00" * 4)
            fp.write(footer)

        logger.info("V2 写入完成: %s nside=%d n_pix=%d n_chunks=%d has_snr=%s size=%d",
                    os.path.basename(self._path), nside, n_pix, n_chunks, has_snr,
                    os.path.getsize(self._path))
        return HIO_OK

    @staticmethod
    def _build_snr_block(model: HissV2SnrModel) -> bytes:
        """构建 SNR 稀疏块 (契约 §8.2, SoA 三通道分块压缩)。"""
        n = int(model.n_points)
        out = struct.pack("<I", n)
        # points_ra (f64)
        ra_bytes = np.ascontiguousarray(model.points_ra, dtype="<f8").tobytes()
        out += _compress_snr_channel(ra_bytes)
        # points_dec (f64)
        dec_bytes = np.ascontiguousarray(model.points_dec, dtype="<f8").tobytes()
        out += _compress_snr_channel(dec_bytes)
        # points_snr (f32)
        snr_bytes = np.ascontiguousarray(model.points_snr, dtype="<f4").tobytes()
        out += _compress_snr_channel(snr_bytes)
        # 3 scalars
        out += struct.pack("<ddd", float(model.snr_phot), float(model.median_snr),
                           float(model.idw_power))
        return out


def _compress_snr_channel(raw: bytes) -> bytes:
    """SNR 单通道压缩块: [comp_len u32][raw_len u32][zstd 数据] (契约 §8.3)。"""
    raw_len = len(raw)
    if raw_len == 0:
        return struct.pack("<II", 0, 0)
    comp = _zstd_compress(raw, ZSTD_LEVEL)
    return struct.pack("<II", len(comp), raw_len) + comp


# ============================================================================
# HissV2Reader
# ============================================================================

class HissV2Reader:
    """HISS V2 读取器 (支持 7 个 batch read API, 契约 §11)。

    首次打开时读取固定头 + JSON + footer + chunk index, 并执行契约 §13 校验。
    数据块按需读取 (lazy), 解压结果缓存以便重复访问。
    """

    def __init__(self, path: str, verify_global_crc: bool = True):
        """打开 V2 文件并读取元数据。

        Args:
            path: .hiss2 / .hiss (V2) 文件路径
            verify_global_crc: 是否校验全局 CRC32 (默认 True)

        Raises:
            HissV2Error: 校验失败 (携带错误码)
        """
        self._path = path
        if not os.path.isfile(path):
            raise HissV2Error(HIO_ERR_FILE, f"文件不存在: {path}")

        with open(path, "rb") as f:
            self._raw = f.read()
        self._filesize = len(self._raw)

        # ---- 1. Magic 校验 (契约 §13.1) ----
        if len(self._raw) < FIXED_HEADER_SIZE:
            raise HissV2Error(HIO_ERR_MAGIC, "文件过短, 无法读取固定头")
        magic, version, flags, json_uncomp_len, json_comp_len, n_pix = struct.unpack_from(
            "<4sHHIIQ", self._raw, 0)
        if magic != MAGIC:
            raise HissV2Error(HIO_ERR_MAGIC, f"magic 不匹配: {magic!r} (期望 {MAGIC!r})")
        # ---- 2. Version 校验 (契约 §13.2) ----
        if version != VERSION:
            raise HissV2Error(HIO_ERR_VERSION, f"version 不支持: {version} (期望 {VERSION})")

        self._version = version
        self._flags = flags
        self._dense_mode = bool(flags & FLAG_DENSE_MODE)
        self._n_pix = int(n_pix)

        # ---- 3. JSON provenance (契约 §13.4) ----
        json_start = FIXED_HEADER_SIZE
        json_end = json_start + json_comp_len
        if json_end > self._filesize:
            raise HissV2Error(HIO_ERR_JSON, "JSON 头长度越界")
        json_bytes = _zstd_decompress(self._raw[json_start:json_end])
        if len(json_bytes) != json_uncomp_len:
            raise HissV2Error(HIO_ERR_JSON,
                              f"JSON 解压长度不一致: {len(json_bytes)} != {json_uncomp_len}")
        try:
            self._provenance = json.loads(json_bytes.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            raise HissV2Error(HIO_ERR_JSON, f"JSON 解析失败: {e}") from e
        # ---- 5. JSON 必填字段 (契约 §13.5) ----
        missing = REQUIRED_PROVENANCE_FIELDS - set(self._provenance.keys())
        if missing:
            raise HissV2Error(HIO_ERR_JSON, f"provenance 缺少必填字段: {missing}")
        if self._provenance.get("format_version") != FORMAT_VERSION_STR:
            raise HissV2Error(HIO_ERR_VERSION,
                              f"format_version 不匹配: {self._provenance.get('format_version')}")

        self._nside = int(self._provenance["nside"])
        self._nested = bool(self._provenance["nested"])
        self._chunk_size = int(self._provenance["chunk_size"])
        self._n_chunks = int(self._provenance["n_chunks"])
        self._has_snr = bool(self._provenance["has_snr"])
        self._codec = self._provenance["codec"]

        # ---- 3. Footer magic (契约 §13.3) ----
        if self._filesize < FOOTER_SIZE:
            raise HissV2Error(HIO_ERR_FOOTER, "文件过短, 无法读取 footer")
        footer_off = self._filesize - FOOTER_SIZE
        (chunk_index_offset, chunk_index_size, snr_block_offset, snr_block_size,
         global_crc32, _reserved, magic_trailer, _pad) = struct.unpack_from(
            "<QQQQII4s4s", self._raw, footer_off)
        if magic_trailer != MAGIC_TRAILER:
            raise HissV2Error(HIO_ERR_FOOTER,
                              f"footer magic_trailer 不匹配: {magic_trailer!r} (文件截断)")

        self._chunk_index_offset = chunk_index_offset
        self._chunk_index_size = chunk_index_size
        self._snr_block_offset = snr_block_offset
        self._snr_block_size = snr_block_size
        self._global_crc32 = global_crc32

        # ---- 7. 全局 CRC32 (契约 §13.7) ----
        if verify_global_crc:
            calc_crc = _crc32(self._raw[:footer_off])
            if calc_crc != global_crc32:
                raise HissV2Error(HIO_ERR_CRC,
                                  f"全局 CRC32 校验失败: 计算值={calc_crc:#010x} "
                                  f"存储值={global_crc32:#010x}")

        # ---- 读取块索引表 ----
        self._chunk_entries: List[HissV2ChunkEntry] = []
        idx_pos = chunk_index_offset
        if chunk_index_offset + chunk_index_size > self._filesize:
            raise HissV2Error(HIO_ERR_JSON, "块索引表越界")
        for i in range(self._n_chunks):
            (offset, comp_size, raw_count, crc32, codec, cflags, reserved) = struct.unpack_from(
                "<QIIIBBH", self._raw, idx_pos + i * CHUNK_INDEX_ENTRY_SIZE)
            self._chunk_entries.append(HissV2ChunkEntry(
                offset=offset, comp_size=comp_size, raw_count=raw_count,
                crc32=crc32, codec=codec, flags=cflags, reserved=reserved))

        # ---- 6. n_pix 一致性 (契约 §13.6) ----
        sum_raw = sum(e.raw_count for e in self._chunk_entries)
        if sum_raw != n_pix or int(self._provenance["n_pix"]) != n_pix:
            raise HissV2Error(HIO_ERR_JSON,
                              f"n_pix 不一致: header={n_pix} json={self._provenance['n_pix']} "
                              f"sum(raw_count)={sum_raw}")

        # ---- 10. 文件大小一致性 (契约 §13.10) ----
        expected_size = (chunk_index_offset + chunk_index_size +
                         sum(e.comp_size for e in self._chunk_entries) +
                         snr_block_size + FOOTER_SIZE)
        if expected_size != self._filesize:
            raise HissV2Error(HIO_ERR_CRC,
                              f"文件大小不一致: expected={expected_size} actual={self._filesize}")

        # 解压缓存
        self._chunk_cache: Dict[int, Tuple[np.ndarray, np.ndarray, np.ndarray]] = {}
        self._chunk_bounds_cache: Dict[int, Tuple[int, int]] = {}

        logger.info("V2 打开成功: %s nside=%d n_pix=%d n_chunks=%d has_snr=%s size=%d",
                    os.path.basename(path), self._nside, self._n_pix, self._n_chunks,
                    self._has_snr, self._filesize)

    # ---- 属性 ----
    @property
    def path(self) -> str: return self._path
    @property
    def version(self) -> int: return self._version
    @property
    def flags(self) -> int: return self._flags
    @property
    def dense_mode(self) -> bool: return self._dense_mode
    @property
    def nside(self) -> int: return self._nside
    @property
    def nested(self) -> bool: return self._nested
    @property
    def n_pix(self) -> int: return self._n_pix
    @property
    def n_chunks(self) -> int: return self._n_chunks
    @property
    def chunk_size(self) -> int: return self._chunk_size
    @property
    def has_snr(self) -> bool: return self._has_snr
    @property
    def provenance(self) -> Dict[str, Any]: return self._provenance
    @property
    def filesize(self) -> int: return self._filesize

    # ========================================================================
    # 内部: 块解压 + CRC 校验
    # ========================================================================

    def _decompress_chunk(self, chunk_idx: int) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """解压第 chunk_idx 块, 校验 per-chunk CRC32, 返回 (ipix, signal, support)。

        契约 §13.8: 读取某块时校验该块压缩数据 CRC32。
        契约 §6.3: 解压缓冲 = [ipix u64][signal f32][support u8]。
        """
        if chunk_idx < 0 or chunk_idx >= self._n_chunks:
            raise HissV2Error(HIO_ERR_CHUNK_RANGE,
                              f"chunk 索引越界: {chunk_idx} (n_chunks={self._n_chunks})")
        if chunk_idx in self._chunk_cache:
            return self._chunk_cache[chunk_idx]

        entry = self._chunk_entries[chunk_idx]
        comp_data = self._raw[entry.offset:entry.offset + entry.comp_size]
        if len(comp_data) != entry.comp_size:
            raise HissV2Error(HIO_ERR_CRC, f"chunk {chunk_idx} 数据长度不足")
        # per-chunk CRC32 (契约 §13.8)
        if _crc32(comp_data) != entry.crc32:
            raise HissV2Error(HIO_ERR_CRC, f"chunk {chunk_idx} CRC32 校验失败 (文件损坏)")

        # 解压
        if entry.codec == CODEC_ZSTD:
            raw = _zstd_decompress(comp_data)
        elif entry.codec == CODEC_NONE:
            raw = comp_data
        else:
            raise HissV2Error(HIO_ERR_CODEC, f"chunk {chunk_idx} 不支持的 codec: {entry.codec}")

        raw_count = entry.raw_count
        expected = raw_count * 13  # 8 + 4 + 1
        if len(raw) != expected:
            raise HissV2Error(HIO_ERR_CRC,
                              f"chunk {chunk_idx} 解压长度不一致: {len(raw)} != {expected}")

        pos = 0
        ipix = np.frombuffer(raw, dtype="<u8", count=raw_count, offset=pos).astype(np.uint64)
        pos += raw_count * 8
        signal = np.frombuffer(raw, dtype="<f4", count=raw_count, offset=pos).astype(np.float32)
        pos += raw_count * 4
        support = np.frombuffer(raw, dtype="<u1", count=raw_count, offset=pos).astype(np.uint8)

        result = (ipix, signal, support)
        self._chunk_cache[chunk_idx] = result
        return result

    def _chunk_bounds(self, chunk_idx: int) -> Tuple[int, int]:
        """返回第 chunk_idx 块的 (first_ipix, last_ipix), 用于二分查找。"""
        if chunk_idx in self._chunk_bounds_cache:
            return self._chunk_bounds_cache[chunk_idx]
        ipix, _, _ = self._decompress_chunk(chunk_idx)
        if ipix.size == 0:
            bounds = (0, -1)  # 空块
        else:
            bounds = (int(ipix[0]), int(ipix[-1]))
        self._chunk_bounds_cache[chunk_idx] = bounds
        return bounds

    def _find_chunks_for_ipix_range(self, ipix_lo: int, ipix_hi: int) -> Optional[Tuple[int, int]]:
        """二分定位 ipix 区间 [ipix_lo, ipix_hi] 覆盖的块范围 [first, last]。

        利用块间全局 ipix 升序 (契约 §6.4):
          - first = 第一个 last_ipix >= ipix_lo 的块
          - last  = 最后一个 first_ipix <= ipix_hi 的块
        若无重叠返回 None。仅解压 O(log n_chunks) 个块用于边界判断。
        """
        n = self._n_chunks
        if n == 0:
            return None
        # 找 first: 第一个 last_ipix >= ipix_lo
        lo_c, hi_c = 0, n - 1
        first = n
        while lo_c <= hi_c:
            mid = (lo_c + hi_c) // 2
            _, last_ipix = self._chunk_bounds(mid)
            if last_ipix < ipix_lo:
                lo_c = mid + 1
            else:
                first = mid
                hi_c = mid - 1
        # 找 last: 最后一个 first_ipix <= ipix_hi
        lo_c, hi_c = 0, n - 1
        last = -1
        while lo_c <= hi_c:
            mid = (lo_c + hi_c) // 2
            first_ipix, _ = self._chunk_bounds(mid)
            if first_ipix > ipix_hi:
                hi_c = mid - 1
            else:
                last = mid
                lo_c = mid + 1
        if first > last or first >= n:
            return None
        return (first, last)

    # ========================================================================
    # batch read API (契约 §11, 7 个)
    # ========================================================================

    def read_provenance(self) -> Dict[str, Any]:
        """§11.1 读 provenance (仅头部, 不读数据)。"""
        return {
            "version": self._version,
            "flags": self._flags,
            "n_pix": self._n_pix,
            "provenance": dict(self._provenance),
            "chunk_size": self._chunk_size,
            "n_chunks": self._n_chunks,
        }

    def read_chunk(self, chunk_idx: int) -> Tuple[int, np.ndarray, np.ndarray, np.ndarray]:
        """§11.2 读单个块, 返回 (raw_count, ipix, signal, support)。"""
        ipix, signal, support = self._decompress_chunk(chunk_idx)
        return (int(ipix.size), ipix, signal, support)

    def read_chunks(self, chunk_indices: Sequence[int]) -> Tuple[int, np.ndarray,
                                                                 np.ndarray, np.ndarray]:
        """§11.3 批量读多个块, 返回拼接后的 (total_count, ipix, signal, support)。

        按 chunk 顺序拼接 (全局 ipix 升序)。
        """
        ipix_list, sig_list, sup_list = [], [], []
        total = 0
        for idx in chunk_indices:
            ipix, signal, support = self._decompress_chunk(idx)
            ipix_list.append(ipix)
            sig_list.append(signal)
            sup_list.append(support)
            total += ipix.size
        if total == 0:
            return (0, np.empty(0, dtype=np.uint64),
                    np.empty(0, dtype=np.float32), np.empty(0, dtype=np.uint8))
        return (total,
                np.concatenate(ipix_list) if ipix_list else np.empty(0, dtype=np.uint64),
                np.concatenate(sig_list) if sig_list else np.empty(0, dtype=np.float32),
                np.concatenate(sup_list) if sup_list else np.empty(0, dtype=np.uint8))

    def read_ipix_range(self, ipix_lo: int, ipix_hi: int) -> Tuple[int, np.ndarray,
                                                                    np.ndarray, np.ndarray]:
        """§11.4 按 ipix 区间读取 (范围查询, 仅读取必要块)。"""
        if ipix_lo > ipix_hi:
            return (0, np.empty(0, dtype=np.uint64),
                    np.empty(0, dtype=np.float32), np.empty(0, dtype=np.uint8))
        crange = self._find_chunks_for_ipix_range(int(ipix_lo), int(ipix_hi))
        if crange is None:
            return (0, np.empty(0, dtype=np.uint64),
                    np.empty(0, dtype=np.float32), np.empty(0, dtype=np.uint8))
        first, last = crange
        ipix_list, sig_list, sup_list = [], [], []
        for c in range(first, last + 1):
            ipix, signal, support = self._decompress_chunk(c)
            if ipix.size == 0:
                continue
            mask = (ipix >= ipix_lo) & (ipix <= ipix_hi)
            if mask.any():
                ipix_list.append(ipix[mask])
                sig_list.append(signal[mask])
                sup_list.append(support[mask])
        if not ipix_list:
            return (0, np.empty(0, dtype=np.uint64),
                    np.empty(0, dtype=np.float32), np.empty(0, dtype=np.uint8))
        return (int(sum(x.size for x in ipix_list)),
                np.concatenate(ipix_list), np.concatenate(sig_list), np.concatenate(sup_list))

    def read_leaf(self, leaf_ipix: int) -> Tuple[int, np.ndarray, np.ndarray, np.ndarray]:
        """§11.5 按 nside=64 子叶读取 (nested 位运算 ipix>>shift 定位)。

        nside<64 时返回错误 (契约 §11.5)。
        """
        if self._nside < 64:
            raise HissV2Error(HIO_ERR_CHUNK_RANGE,
                              f"nside={self._nside} < 64, 不支持子叶读取")
        if not self._nested:
            raise HissV2Error(HIO_ERR_CHUNK_RANGE,
                              "read_leaf 仅支持 nested 排序")
        # shift = 2 * (log2(nside) - log2(64)) = 2 * (log2(nside) - 6)
        nside_log2 = int(round(np.log2(self._nside)))
        shift = 2 * (nside_log2 - 6)
        # 子叶 ipix 范围: [leaf_ipix << shift, (leaf_ipix+1) << shift - 1]
        ipix_lo = int(leaf_ipix) << shift
        ipix_hi = ((int(leaf_ipix) + 1) << shift) - 1
        return self.read_ipix_range(ipix_lo, ipix_hi)

    def read_snr_model(self) -> HissV2SnrModel:
        """§11.6 读 SNR 稀疏模型。has_snr=false 时报错 (-10)。"""
        if not self._has_snr:
            raise HissV2Error(HIO_ERR_NO_SNR, "has_snr=false, 无 SNR 块")
        if self._snr_block_offset == 0 or self._snr_block_size == 0:
            raise HissV2Error(HIO_ERR_NO_SNR, "SNR 块偏移/大小为 0")
        pos = self._snr_block_offset
        n_points = struct.unpack_from("<I", self._raw, pos)[0]
        pos += 4
        # 三通道 (每通道 8B 头 + 数据, 用返回的消耗字节数推进)
        ra_raw, ra_consumed = _decompress_snr_channel(self._raw, pos)
        pos += ra_consumed
        dec_raw, dec_consumed = _decompress_snr_channel(self._raw, pos)
        pos += dec_consumed
        snr_raw, snr_consumed = _decompress_snr_channel(self._raw, pos)
        pos += snr_consumed
        # 3 scalars
        snr_phot, median_snr, idw_power = struct.unpack_from("<ddd", self._raw, pos)

        model = HissV2SnrModel(
            n_points=int(n_points),
            points_ra=np.frombuffer(ra_raw, dtype="<f8").astype(np.float64),
            points_dec=np.frombuffer(dec_raw, dtype="<f8").astype(np.float64),
            points_snr=np.frombuffer(snr_raw, dtype="<f4").astype(np.float32),
            snr_phot=float(snr_phot),
            median_snr=float(median_snr),
            idw_power=float(idw_power),
        )
        logger.info("SNR 模型读取: n_points=%d snr_phot=%.4f median=%.4f idw_power=%.2f",
                    n_points, snr_phot, median_snr, idw_power)
        return model

    def read_all(self) -> Tuple[int, np.ndarray, np.ndarray, np.ndarray,
                                Optional[HissV2SnrModel], Dict[str, Any]]:
        """§11.7 整文件读取 (= read_chunks(0..n_chunks-1) + read_snr_model)。"""
        all_chunks = list(range(self._n_chunks))
        total, ipix, signal, support = self.read_chunks(all_chunks)
        snr_model = None
        if self._has_snr:
            snr_model = self.read_snr_model()
        return (total, ipix, signal, support, snr_model, dict(self._provenance))

    def close(self):
        """清理解压缓存。"""
        self._chunk_cache.clear()
        self._chunk_bounds_cache.clear()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False


def _decompress_snr_channel(raw: bytes, pos: int) -> Tuple[bytes, int]:
    """解压 SNR 单通道 (契约 §8.3): [comp_len u32][raw_len u32][data]。

    Returns:
        (decompressed_bytes, total_bytes_consumed) — total_bytes_consumed = 8 + 实际数据字节数。
    """
    comp_len, raw_len = struct.unpack_from("<II", raw, pos)
    data_start = pos + 8
    if comp_len == 0:
        # 未压缩, 直接读 raw_len 字节
        return (raw[data_start:data_start + raw_len], 8 + raw_len)
    comp = raw[data_start:data_start + comp_len]
    out = _zstd_decompress(comp)
    if len(out) != raw_len:
        raise HissV2Error(HIO_ERR_CRC,
                          f"SNR 通道解压长度不一致: {len(out)} != {raw_len}")
    return (out, 8 + comp_len)


# ============================================================================
# 模块级函数 API (匹配契约 §11 C 风格签名, 返回 (code, result))
# ============================================================================

def hiss2_read_provenance(path: str) -> Tuple[int, Optional[Dict[str, Any]]]:
    """§11.1 读 provenance。Returns (code, result_dict 或 None)。"""
    try:
        with HissV2Reader(path, verify_global_crc=False) as r:
            return (HIO_OK, r.read_provenance())
    except HissV2Error as e:
        logger.warning("hiss2_read_provenance 失败: %s", e)
        return (e.code, None)


def hiss2_read_chunk(path: str, chunk_idx: int) -> Tuple[int, Optional[Dict[str, Any]]]:
    """§11.2 读单个块。Returns (code, {raw_count, ipix, signal, support} 或 None)。"""
    try:
        with HissV2Reader(path) as r:
            raw_count, ipix, signal, support = r.read_chunk(chunk_idx)
            return (HIO_OK, {"raw_count": raw_count, "ipix": ipix,
                             "signal": signal, "support": support})
    except HissV2Error as e:
        return (e.code, None)


def hiss2_read_chunks(path: str, chunk_indices: Sequence[int]) -> Tuple[int, Optional[Dict[str, Any]]]:
    """§11.3 批量读多个块。"""
    try:
        with HissV2Reader(path) as r:
            total, ipix, signal, support = r.read_chunks(chunk_indices)
            return (HIO_OK, {"total_count": total, "ipix": ipix,
                             "signal": signal, "support": support})
    except HissV2Error as e:
        return (e.code, None)


def hiss2_read_ipix_range(path: str, ipix_lo: int, ipix_hi: int) -> Tuple[int, Optional[Dict[str, Any]]]:
    """§11.4 按 ipix 区间读取。"""
    try:
        with HissV2Reader(path) as r:
            count, ipix, signal, support = r.read_ipix_range(ipix_lo, ipix_hi)
            return (HIO_OK, {"count": count, "ipix": ipix,
                             "signal": signal, "support": support})
    except HissV2Error as e:
        return (e.code, None)


def hiss2_read_leaf(path: str, leaf_ipix: int) -> Tuple[int, Optional[Dict[str, Any]]]:
    """§11.5 按 nside=64 子叶读取。"""
    try:
        with HissV2Reader(path) as r:
            count, ipix, signal, support = r.read_leaf(leaf_ipix)
            return (HIO_OK, {"count": count, "ipix": ipix,
                             "signal": signal, "support": support})
    except HissV2Error as e:
        return (e.code, None)


def hiss2_read_snr_model(path: str) -> Tuple[int, Optional[HissV2SnrModel]]:
    """§11.6 读 SNR 稀疏模型。"""
    try:
        with HissV2Reader(path) as r:
            return (HIO_OK, r.read_snr_model())
    except HissV2Error as e:
        return (e.code, None)


def hiss2_read_all(path: str) -> Tuple[int, Optional[Dict[str, Any]]]:
    """§11.7 整文件读取。"""
    try:
        with HissV2Reader(path) as r:
            total, ipix, signal, support, snr_model, prov = r.read_all()
            return (HIO_OK, {"n_pix": total, "ipix": ipix, "signal": signal,
                             "support": support, "snr_model": snr_model,
                             "provenance": prov})
    except HissV2Error as e:
        return (e.code, None)


# ============================================================================
# V1 → V2 转换器 (契约 §14.2)
# ============================================================================

def v1_to_v2_converter(v1_path: str, v2_path: str,
                       chunk_size: int = CHUNK_SIZE_DEFAULT) -> int:
    """将 V1 .hiss 转换为 V2 格式 (契约 §14.2 迁移步骤)。

    迁移规则:
      1. 读 V1 (snr_format=1 稀疏): ipix/pixel/snr_model/meta
      2. support[i]=1 (所有存储像素均为覆盖像素, 契约 §14.2 步骤2)
      3. provenance 补全 format_version/ordering/chunk_size/n_chunks/codec/crc_algorithm/has_snr
      4. SNR AoS → SoA (snr_psf → points_snr, 语义不变, 契约 §14.2 步骤4)
      5. 写 V2: 分块压缩 + CRC32 + footer

    Returns:
        0=成功
    """
    nside, nested, ipix, pixel, meta, snr_model = v1_read_snr_model(v1_path)
    n_pix = int(ipix.size)

    # support: 全 1 (契约 §14.2, V1 所有存储像素均为覆盖像素)
    support = np.ones(n_pix, dtype=np.uint8)

    # signal: V1 pixel 即为 V2 signal (float32)
    signal = pixel.astype(np.float32)

    # provenance: V1 meta 已含 wcs/drizzle/source/fits_meta/filter/exposure_s/obs_time/pixfrac
    # 补全的字段由 Writer 自动填入 (format_version/ordering/nested/n_pix/has_snr/
    # chunk_size/n_chunks/codec/crc_algorithm), 此处只透传 V1 已有字段
    provenance = {k: v for k, v in meta.items()
                  if k in {"filter", "exposure_s", "obs_time", "pixfrac",
                           "wcs", "drizzle", "fits_meta", "source"}}
    # 确保关键 provenance 字段存在 (V1 可能缺 source/drizzle)
    if "source" not in provenance:
        provenance["source"] = {"fits_path": "", "frame_id":
                                os.path.splitext(os.path.basename(v1_path))[0],
                                "n_source_pixels": 0}
    if "drizzle" not in provenance:
        provenance["drizzle"] = {"n_healpix_pixels": n_pix,
                                 "n_source_pixels": 0, "elapsed_sec": 0.0,
                                 "pixfrac": float(meta.get("pixfrac", 1.0))}

    # SNR 模型: V1 AoS 已在 v1_read_snr_model 中转为 SoA (HissV2SnrModel)
    writer = HissV2Writer(v2_path)
    return writer.write(nside=nside, nested=nested, ipix=ipix, signal=signal,
                        support=support, provenance=provenance,
                        snr_model=snr_model, chunk_size=chunk_size, codec="ZSTD")


# ============================================================================
# 模块自测
# ============================================================================

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
    print("=" * 60)
    print("hiss_v2.py 模块自测 (常量与依赖检查)")
    print("=" * 60)
    print(f"  MAGIC={MAGIC!r}  VERSION={VERSION}  FORMAT_VERSION_STR={FORMAT_VERSION_STR}")
    print(f"  FIXED_HEADER_SIZE={FIXED_HEADER_SIZE}  CHUNK_INDEX_ENTRY_SIZE={CHUNK_INDEX_ENTRY_SIZE}"
          f"  FOOTER_SIZE={FOOTER_SIZE}")
    print(f"  CHUNK_SIZE_DEFAULT={CHUNK_SIZE_DEFAULT}  ZSTD_LEVEL={ZSTD_LEVEL}")
    print(f"  CRC_ALGORITHM={CRC_ALGORITHM}")
    print(f"  zlib.crc32(b'123456789')={_crc32(b'123456789')} (期望 0xCBF43926)")
    assert _crc32(b"123456789") == 0xCBF43926, "CRC32 实现错误"
    # zstd 往返
    src = b"hello HISS v2 " * 100
    assert _zstd_decompress(_zstd_compress(src)) == src, "zstd 往返失败"
    print("[OK] CRC32 校验通过 (0xCBF43926)")
    print("[OK] zstd 往返一致")
    print("[OK] 依赖就绪: zstandard / zlib / numpy")
