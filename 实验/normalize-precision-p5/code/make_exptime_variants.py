#!/usr/bin/env python3
"""构造 EXPTIME 变体输入（P0 复现用）: 复制真实亮帧并原地改写 EXPTIME 卡。

判据输入（Issue #7 A.3）: 有效 / 缺失 / 0 / 负 / 非数。
- 只改 FITS 头里的那 80 字节卡（像素与其余头不动, 偏移不变）;
- 缺失 = 换成 COMMENT 卡（同长度）;
- 输出写到 run/P0-EXPTIME/inputs/（run/ 不入库; 复现命令见 Issue 报告）。

用法: python3 make_exptime_variants.py <src.fts> <out_dir>
"""
from __future__ import annotations

import os
import sys

CARD = 80


def card(keyword: str, value: str, comment: str = "") -> bytes:
    body = "%-8s= %20s" % (keyword, value)
    if comment:
        body += " / " + comment
    return body.ljust(CARD)[:CARD].encode("ascii")


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    src, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)
    with open(src, "rb") as f:
        data = bytearray(f.read())
    # 找到 EXPTIME 卡
    off = None
    for i in range(0, min(len(data), 2880 * 6), CARD):
        c = bytes(data[i:i + CARD])
        if c.startswith(b"EXPTIME "):
            off = i
            break
        if c.startswith(b"END"):
            break
    if off is None:
        print("EXPTIME card not found in %s" % src)
        return 1
    original = bytes(data[off:off + CARD])   # 原卡原样保留（valid 变体不得改数值）
    variants = {
        "valid": original,
        "missing": b"COMMENT  EXPTIME card removed by P0-EXPTIME repro harness".ljust(CARD),
        "zero": card("EXPTIME", "0.00000000000E+000", "[sec] zero exposure (repro)"),
        "negative": card("EXPTIME", "-6.00000000000E+002", "[sec] negative (repro)"),
        "nan": card("EXPTIME", "NAN", "[sec] non-finite (repro)"),
    }
    base = os.path.splitext(os.path.basename(src))[0]
    for name, c in variants.items():
        out = os.path.join(out_dir, "%s.EXPTIME-%s.fts" % (base, name))
        d = bytearray(data)
        d[off:off + CARD] = c
        with open(out, "wb") as f:
            f.write(bytes(d))
        print("wrote %s (%s)" % (out, c.decode("ascii", "replace").strip()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
