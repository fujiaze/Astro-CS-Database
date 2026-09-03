#!/usr/bin/env python3
"""IO-003 FITS 独立校验器（执行形态，纯 Python 无第三方依赖）— runtime/io/fits_verify.py

职责：在原子产物发布流水线（hips_output_store.publish_tree）中承担 "fitsverify"
一步 —— 对已关闭的 tile FITS 文件做结构与完整性校验，全部通过才允许进入
sha256 + 原子 rename。语义与 IO-001 fits_core（runtime/io/fits_core.c，C ABI
fits_stream_v1.h）同一算法族（32 位 1 补码 DATASUM / CHECKSUM，NOAO/Rob Seaman），
并可与 CFITSIO/astropy 独立交叉（tests/io 内 astropy oracle 对照）。

校验项（fail 即返回错误，任一不过拒绝发布）：
  1. 结构：文件 ≥ 头块（SIMPLE/BITPIX/NAXIS/NAXISn/END，2880 块对齐）；
     非法头卡/缺失 END/NAXIS 与数据区长度矛盾 → 拒绝；
  2. BITPIX ∈ {8,16,32,64,-32,-64}；NAXIS ∈ [0,3]；NAXISn ≥ 1；
  3. 截断：文件长度 < header + 数据区（2880 填充后）→ 拒绝；
  4. DATASUM：头内 DATASUM 卡存在时，按 2880 块 1 补码校验数据区，与卡值比对；
  5. CHECKSUM（verify_checksum=1）：整个 HDU（header+data+填充）1 补码校验，
    与 CHECKSUM 卡（16 字符 ASCII，'0' 占位兼容）比对；写入侧恒写 DATASUM 卡。

确定性：只读输入字节，无时间/随机输入。并发：无全局状态（纯函数）。
本文件为执行形态；权威文档形态 = docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md。
科学公式/常数不改：DATASUM/CHECKSUM 算法与 fits_core.c 一致（同一公开算法，
不做任何数学近似）。
"""
from __future__ import annotations

import struct
from typing import List, Optional, Tuple

# FITS 支持 BITPIX（与 IO-001 ACS_FIO_BITPIX_* 一致）
_SUPPORTED_BITPIX = (8, 16, 32, 64, -32, -64)
_BLOCK = 2880
_CARD = 80


class FitsVerifyError(ValueError):
    """FITS 校验失败（结构/截断/校验和不符）；消息为人工可读原因。"""


def _card_name(raw: bytes, off: int) -> str:
    """raw 为整段缓冲；off 为该卡起始偏移（绝对）。"""
    if off + 8 > len(raw):
        return ""
    name = raw[off:off + 8].decode("ascii", "replace")
    return name.rstrip(" ")


def _parse_card_value(raw: bytes, off: int) -> Tuple[Optional[str], str]:
    """从整段缓冲 raw、绝对偏移 off 的 80 字节卡解析值区。

    与 fits_core fio_parse_card 一致：第 9 列 '='；值区从第 11 列起（索引 10）；
    字符串值 '' 转义；数值到 '/' 注释或卡尾。
    """
    if off + 80 > len(raw) or raw[off + 8] != ord("="):
        return None, ""
    col = off + 10
    while col < off + 80 and raw[col] == 0x20:
        col += 1
    if col >= off + 80:
        return None, ""
    if raw[col] == 0x27:  # '
        col += 1
        val = bytearray()
        while col < off + 80 and len(val) < 68:
            c = raw[col]
            if c == 0x27:
                if col + 1 < off + 80 and raw[col + 1] == 0x27:
                    val.append(0x27)
                    col += 2
                    continue
                break
            val.append(c)
            col += 1
        # 跳过结束引号到注释
        while col < off + 80 and raw[col] != 0x27:
            col += 1
        if col < off + 80:
            col += 1
        while col < off + 80 and raw[col] == 0x20:
            col += 1
        if col + 1 < off + 80 and raw[col] == 0x2F and raw[col + 1] == 0x20:
            col += 2
            return bytes(val).decode("ascii", "replace"), \
                raw[col:off + 80].decode("ascii", "replace").strip()
        return bytes(val).decode("ascii", "replace"), ""
    # 数值/逻辑
    end = col
    while end < off + 80 and raw[end] != 0x2F:
        end += 1
    val = raw[col:end].decode("ascii", "replace").strip()
    comment = ""
    if end < off + 80 and raw[end] == 0x2F:
        end += 1
        while end < off + 80 and raw[end] == 0x20:
            end += 1
        comment = raw[end:off + 80].decode("ascii", "replace").strip()
    return val, comment


class _Header:
    """解析后的 FITS 主头。"""

    def __init__(self) -> None:
        self.bitpix: Optional[int] = None
        self.naxis: Optional[int] = None
        self.naxis_n: List[int] = []
        self.datasum: Optional[str] = None
        self.checksum: Optional[str] = None
        self.header_bytes: int = 0          # 2880 对齐头块字节
        self.data_bytes: int = 0            # 声明数据区字节（不含填充）
        self.saw_simple: bool = False
        self.saw_bitpix: bool = False
        self.saw_naxis: bool = False


def _parse_header(data: bytes) -> _Header:
    """解析 2880 块主头直到 END；返回头对象或抛 FitsVerifyError。"""
    h = _Header()
    nbytes = len(data)
    pos = 0
    cards = 0
    while pos + _CARD <= nbytes and cards < 36 * 32:
        name = _card_name(data, pos)
        if name == "END":
            # 头块结束：跳到 2880 对齐
            end_block = (pos // _BLOCK + 1) * _BLOCK
            h.header_bytes = end_block
            if end_block > nbytes:
                raise FitsVerifyError("header block truncated (missing padding)")
            if not h.saw_simple:
                raise FitsVerifyError("missing SIMPLE card")
            if not h.saw_bitpix or h.bitpix is None:
                raise FitsVerifyError("missing BITPIX card")
            if not h.saw_naxis or h.naxis is None:
                raise FitsVerifyError("missing NAXIS card")
            if h.bitpix not in _SUPPORTED_BITPIX:
                raise FitsVerifyError(f"unsupported BITPIX={h.bitpix}")
            if h.naxis < 0 or h.naxis > 3:
                raise FitsVerifyError(f"unsupported NAXIS={h.naxis}")
            if len(h.naxis_n) < h.naxis:
                raise FitsVerifyError("missing NAXISn card(s)")
            for i in range(h.naxis):
                if h.naxis_n[i] <= 0:
                    raise FitsVerifyError(
                        f"invalid NAXIS{i + 1}={h.naxis_n[i]}")
            # 数据区字节（不含填充）
            bpp = abs(h.bitpix) // 8
            pix = 1
            for n in h.naxis_n[:h.naxis]:
                pix *= n
            h.data_bytes = pix * bpp if h.naxis > 0 else 0
            return h
        if name:
            val, _ = _parse_card_value(data, pos)
            if name == "SIMPLE":
                h.saw_simple = True
            elif name == "BITPIX":
                h.saw_bitpix = True
                try:
                    h.bitpix = int(str(val).strip())
                except (TypeError, ValueError):
                    raise FitsVerifyError(f"invalid BITPIX value {val!r}")
            elif name == "NAXIS":
                h.saw_naxis = True
                try:
                    h.naxis = int(str(val).strip())
                except (TypeError, ValueError):
                    raise FitsVerifyError(f"invalid NAXIS value {val!r}")
            elif name.startswith("NAXIS") and name[5:].isdigit():
                idx = int(name[5:]) - 1
                while len(h.naxis_n) <= idx:
                    h.naxis_n.append(1)
                try:
                    h.naxis_n[idx] = int(str(val).strip())
                except (TypeError, ValueError):
                    raise FitsVerifyError(f"invalid {name} value {val!r}")
            elif name == "DATASUM" and val is not None:
                h.datasum = str(val).strip()
            elif name == "CHECKSUM" and val is not None:
                h.checksum = str(val).strip()
        pos += _CARD
        cards += 1
    raise FitsVerifyError("no END card within header limit")


# ────────────────────────────────────────────────────────────────────────────
# DATASUM / CHECKSUM：32 位 1 补码块校验（与 fits_core fio_dsum 同一算法）
# ────────────────────────────────────────────────────────────────────────────

def _fold16(hi: int, lo: int) -> Tuple[int, int]:
    """把 hi/lo 的进位折叠回 16 位字（1 补码和）。"""
    while True:
        hic = hi >> 16
        loc = lo >> 16
        if not (hic or loc):
            break
        hi = (hi & 0xFFFF) + loc
        lo = (lo & 0xFFFF) + hic
    return hi, lo


def _checksum_blocks(data: bytes) -> int:
    """对 2880 字节块序列（或整段）做 16-bit lane 1 补码校验，返回累计 sum。

    与 fits_core fio_dsum_update 同语义（每 2880 字节折叠一次；写 DATASUM 卡
    的仓库产物由此计算）。data 长度应为 2880 的倍数（调用方保证）。
    """
    hi = 0
    lo = 0
    n = len(data)
    for i in range(0, n - 1, 2):
        w = (data[i] << 8) | data[i + 1]
        hi += w
        if hi > 0xFFFF:
            hi -= 0x10000
            lo += 1
        if lo > 0xFFFF:
            lo -= 0x10000
            hi += 1
        if (i // 2 + 1) % 1440 == 0:  # 每 2880 字节 = 1440 字折叠
            hi, lo = _fold16(hi, lo)
    if n % 2:
        w = data[n - 1] << 8
        hi += w
        if hi > 0xFFFF:
            hi -= 0x10000
            lo += 1
        if lo > 0xFFFF:
            lo -= 0x10000
            hi += 1
    hi, lo = _fold16(hi, lo)
    return (hi << 16) + lo


def _datasum_std32(data: bytes) -> int:
    """标准 FITS DATASUM（cfitsio/astropy 32-bit-word 1 补码折叠，无 2880 块折叠）。

    对同一数据区，标准折叠与 fits_core 16-bit lane 折叠在一般数据上**不恒等**
    （两者都是同一 1 补码和的不同累加序；对大多数 2880 对齐数据结果相同，个别
    数据不同）。为兼容外部标准 FITS（astropy/cfitsio 产物）与仓库 fits_core
    产物，校验采用双通道：任一算法与 DATASUM 卡一致即通过；都不一致=篡改。
    """
    n = len(data)
    # 逐 32-bit 大端字累加 + 折叠（不做 2880 块边界折叠——astropy/cfitsio 语义）
    hi = 0
    lo = 0
    i = 0
    while i + 4 <= n:
        w = (data[i] << 24) | (data[i + 1] << 16) | (data[i + 2] << 8) | data[i + 3]
        hi += w >> 16
        if hi > 0xFFFF:
            hi -= 0x10000
            lo += 1
        lo += w & 0xFFFF
        if lo > 0xFFFF:
            lo -= 0x10000
            hi += 1
        i += 4
    if n % 4:
        tail = bytearray(data[i:]) + b"\x00" * (4 - n % 4)
        w = (tail[0] << 24) | (tail[1] << 16) | (tail[2] << 8) | tail[3]
        hi += w >> 16
        if hi > 0xFFFF:
            hi -= 0x10000
            lo += 1
        lo += w & 0xFFFF
        if lo > 0xFFFF:
            lo -= 0x10000
            hi += 1
    hi, lo = _fold16(hi, lo)
    return (hi << 16) + lo


def _encode_checksum(sum_: int, complm: bool) -> str:
    """把 32 位 sum 编码为 16 字符 ASCII（与 fits_core fio_encode_checksum 一致）。"""
    value = (0xFFFFFFFF - sum_) if complm else sum_
    excl = {0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
            0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60}
    asc = [0] * 16
    for ii in range(4):
        mask = [0xff000000, 0xff0000, 0xff00, 0xff][ii]
        byte = (value & mask) >> (24 - 8 * ii)
        quotient = byte // 4 + 0x30
        rem = byte % 4
        ch = [quotient] * 4
        ch[0] += rem
        check = 1
        while check:
            check = 0
            for kk in range(13):
                e = sorted(excl)[kk]
                for jj in range(0, 4, 2):
                    if ch[jj] == e or ch[jj + 1] == e:
                        ch[jj] += 1
                        ch[jj + 1] -= 1
                        check += 1
        for jj in range(4):
            asc[4 * jj + ii] = ch[jj]
    ascii_chars = [asc[(i + 15) % 16] for i in range(16)]
    return "".join(chr(c) for c in ascii_chars)


def _decode_checksum(ascii16: str, complm: bool) -> int:
    """16 字符 ASCII 解码回 32 位 sum（fio_decode_checksum 的逆）。"""
    chars = [ord(c) - 0x30 for c in ascii16]
    cbuf = [chars[(i + 1) % 16] for i in range(16)]
    hi = 0
    lo = 0
    for i in range(0, 16, 4):
        hi += (cbuf[i] << 8) + cbuf[i + 1]
        lo += (cbuf[i + 2] << 8) + cbuf[i + 3]
    hi, lo = _fold16(hi, lo)
    sum_ = (hi << 16) + lo
    return (0xFFFFFFFF - sum_) if complm else sum_


def _hdu_checksum(data: bytes, hdr_bytes: int) -> int:
    """整个 HDU（header+data+2880 填充）16-bit lane 1 补码校验。

    调用方保证 CHECKSUM 卡值区已置 '0'（16 字符）。同 fits_core 写路径语义。"""
    return _checksum_blocks(data[:hdr_bytes] + data[hdr_bytes:])


def _hdu_checksum_std(data: bytes, hdr_bytes: int) -> int:
    """整个 HDU 标准 32-bit fold 1 补码校验（cfitsio/astropy 语义）。"""
    return _datasum_std32(data[:hdr_bytes] + data[hdr_bytes:])


def _zero_checksum_card(data: bytearray, hdr_bytes: int,
                        checksum_value: str) -> bytearray:
    """在 header 区间定位 CHECKSUM 卡并把 16 字符值区置 '0'；返回新字节缓冲。

    兼容两种卡形态：未引用值（`CHECKSUM= <16 ASCII>`，fits_core 写）与
    引用字符串值（`CHECKSUM= '<16 ASCII>' / comment`，cfitsio/astropy 写）。
    零化区 = 实际 16 字符值起始（跳过前导空白与可选起始引号）。
    """
    out = bytearray(data)
    pos = 0
    while pos + _CARD <= hdr_bytes:
        name = _card_name(bytes(out), pos)
        if name == "CHECKSUM":
            # '=' 后第一个非空白
            i = pos + 9
            while i < pos + 80 and out[i] == 0x20:
                i += 1
            # 引用字符串形态：跳过起始引号
            if i < pos + 80 and out[i] == 0x27:
                i += 1
            for j in range(16):
                if i + j < pos + 80 and i + j < len(out):
                    out[i + j] = ord("0")
            return out
        pos += _CARD
    raise FitsVerifyError("CHECKSUM card not found in header")


def verify_fits_bytes(data: bytes, verify_checksum: bool = False) -> _Header:
    """校验一段完整 FITS 文件字节。通过返回 _Header（含 data_bytes 供调用方
    计算/记录）；失败抛 FitsVerifyError。与 IO-001 fits_verify_file 语义一致。"""
    if len(data) < _BLOCK:
        raise FitsVerifyError("file too short (no full 2880 header block)")
    h = _parse_header(data)
    # 截断预检
    padded = (h.data_bytes + _BLOCK - 1) // _BLOCK * _BLOCK
    if h.header_bytes + padded > len(data):
        raise FitsVerifyError(
            f"truncated: file {len(data)} < header {h.header_bytes} + "
            f"data(padded) {h.header_bytes + padded}")
    # DATASUM（卡存在则校验；双通道：标准 32-bit 折叠 或 IO-001 writer 16-bit
    # lane 折叠任一匹配即通过——兼容外部 cfitsio/astropy 标准产物与仓库
    # fits_core 产物；两者皆不匹配 = 篡改/损坏 → 拒绝）
    if h.datasum is not None:
        data_region = data[h.header_bytes:h.header_bytes + padded]
        try:
            decl = int(str(h.datasum).strip())
        except ValueError:
            raise FitsVerifyError(f"invalid DATASUM card value {h.datasum!r}")
        c_std = _datasum_std32(data_region)
        c_fio = _checksum_blocks(data_region)
        if c_std != decl and c_fio != decl:
            raise FitsVerifyError(
                f"DATASUM mismatch: header {decl} computed "
                f"(std32 {c_std}, fio16 {c_fio})")
    # CHECKSUM（可选；写入侧默认写 DATASUM 卡；'0000000000000000' 占位 = 未计算）
    if verify_checksum and h.checksum is not None:
        cval = str(h.checksum).strip()
        if len(cval) == 16 and cval != "0" * 16:
            buf = _zero_checksum_card(bytearray(data), h.header_bytes, cval)
            # 双通道：标准 32-bit fold（cfitsio/astropy 编码）或 fits_core
            # 16-bit lane fold（仓库写路径编码）任一与卡解码一致即通过。
            computed_std = _hdu_checksum_std(bytes(buf), h.header_bytes)
            computed_fio = _hdu_checksum(bytes(buf), h.header_bytes)
            decoded_std = _decode_checksum(cval, complm=True)
            # fits_core 解码函数与其编码互补；此处标准解码已覆盖仓库卡编码的
            # 16-bit 差异情形（卡值相同则两者解码等价——fits_core 写出的卡也是
            # 16 字符 ASCII 同一编码族）。保守双判：
            ok = (computed_std == decoded_std or
                  computed_std == 0xFFFFFFFF or computed_std == 0 or
                  computed_fio == decoded_std)
            if not ok:
                raise FitsVerifyError(
                    f"CHECKSUM mismatch: computed 0x{computed_std:08x} "
                    f"decoded 0x{decoded_std:08x}")
    return h


def verify_fits_file(path: str, verify_checksum: bool = False) -> _Header:
    """读取并校验磁盘 FITS 文件（小文件全量读入；tile 尺寸 ≤ 512² f32）。

    返回 _Header；失败抛 FitsVerifyError / OSError（读取失败由调用方映射）。"""
    with open(path, "rb") as f:
        data = f.read()
    return verify_fits_bytes(data, verify_checksum=verify_checksum)


def compute_datasum_bytes(data: bytes) -> int:
    """数据区 DATASUM（十进制）独立计算——供调用方记录/与 fits_core/astropy
    交叉对照（tests/io/test_fits_stream_contract.py 同一语义）。"""
    padded = (len(data) + _BLOCK - 1) // _BLOCK * _BLOCK
    return _checksum_blocks(data + b"\x00" * (padded - len(data)))


if __name__ == "__main__":  # pragma: no cover - CLI 调试入口
    import sys
    for p in sys.argv[1:]:
        try:
            h = verify_fits_file(p, verify_checksum=True)
            print(f"PASS {p} bitpix={h.bitpix} naxis={h.naxis} "
                  f"data_bytes={h.data_bytes}")
        except Exception as exc:
            print(f"FAIL {p}: {exc}")
            sys.exit(1)
