# -*- coding: utf-8 -*-
"""
BASS DR3 每帧坐标索引构建工具

原理: 归档的 files/bassmzls-dr3-ccdinfo.fits (245 MB, 429,111 行 x 79 列)
含每帧坐标等元数据, 无需下载任何图像。本工具:
  1. 若本地无 ccdinfo 文件则直连下载 (不使用代理)
  2. 按 FITS BINTABLE 固定行宽(572B) 大端解析所需列
  3. 与 index.csv 的 science 文件按文件名 join
  4. 输出 coords.csv (+ .gz), 并把 join 统计写回 index.json

用法:
  py -3.12 build_coords_index.py [--dir ..] [--data-dir ../data]
"""

from __future__ import annotations

import argparse
import csv
import gzip
import json
import os
import struct
import sys
import urllib.request
from pathlib import Path

CCDINFO_URL = "https://casdc.china-vo.org/archive/BASS/DR3/files/bassmzls-dr3-ccdinfo.fits"
CCDINFO_FILENAME = "bassmzls-dr3-ccdinfo.fits"
ROW_SIZE = 572
PRIMARY_HDR_BLOCKS = 1  # 2880B (SIMPLE + END)


def parse_headers(data: bytes):
    """返回第二个 HDU (BINTABLE) 头部结束后的数据偏移。"""
    # 主头: 从偏移 0 起数 END 卡
    off = 0
    while True:
        block = data[off : off + 2880]
        for i in range(0, 2880, 80):
            if block[i : i + 80].startswith(b"END"):
                off += 2880
                break
        else:
            off += 2880
            continue
        break
    # 第二 HDU 头
    while True:
        block = data[off : off + 2880]
        found = False
        for i in range(0, 2880, 80):
            if block[i : i + 80].startswith(b"END"):
                found = True
                break
        off += 2880
        if found:
            return off


def asc(b: bytes) -> str:
    return b.decode("ascii", errors="replace").strip().rstrip("\x00").strip()


def parse_rows(data: bytes, data_offset: int) -> list[dict]:
    n = (len(data) - data_offset) // ROW_SIZE
    rows = []
    for i in range(n):
        r = data[data_offset + i * ROW_SIZE : data_offset + (i + 1) * ROW_SIZE]
        if len(r) < ROW_SIZE:
            break
        rows.append(
            {
                "fitsname": asc(r[0:69]),
                "date": asc(r[69:79]),
                "time": asc(r[79:94]),
                "ccd": struct.unpack_from(">q", r, 94)[0],
                "filter": asc(r[132:142]),
                "exptime": struct.unpack_from(">f", r, 142)[0],
                "airmass": struct.unpack_from(">f", r, 146)[0],
                "pixscale": struct.unpack_from(">f", r, 178)[0],
                "ra_obs": struct.unpack_from(">d", r, 182)[0],
                "dec_obs": struct.unpack_from(">d", r, 190)[0],
                "ra_lb": struct.unpack_from(">d", r, 198)[0],
                "dec_lb": struct.unpack_from(">d", r, 206)[0],
                "ra_lt": struct.unpack_from(">d", r, 214)[0],
                "dec_lt": struct.unpack_from(">d", r, 222)[0],
                "ra_rt": struct.unpack_from(">d", r, 230)[0],
                "dec_rt": struct.unpack_from(">d", r, 238)[0],
                "ra_rb": struct.unpack_from(">d", r, 246)[0],
                "dec_rb": struct.unpack_from(">d", r, 254)[0],
                "ra": struct.unpack_from(">d", r, 262)[0],
                "dec": struct.unpack_from(">d", r, 270)[0],
                "seeing": struct.unpack_from(">f", r, 290)[0],
                "ccdzpt": struct.unpack_from(">f", r, 298)[0],
                "zptrms": struct.unpack_from(">f", r, 302)[0],
                "nmatch": struct.unpack_from(">i", r, 306)[0],
                "ra_rms": struct.unpack_from(">f", r, 390)[0],
                "dec_rms": struct.unpack_from(">f", r, 394)[0],
                "mjd": struct.unpack_from(">d", r, 438)[0],
                "imq": struct.unpack_from(">q", r, 446)[0],
                "survey": asc(r[462:472]),
                "pass": r[472],
                "filename": asc(r[541:572]),
            }
        )
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description="BASS DR3 坐标索引")
    parser.add_argument("--dir", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--data-dir", type=Path, default=None)
    args = parser.parse_args()

    base: Path = args.dir.resolve()
    data_dir: Path = (args.data_dir or (base / "data")).resolve()
    data_dir.mkdir(parents=True, exist_ok=True)
    for key in ("HTTP_PROXY", "HTTPS_PROXY", "http_proxy", "https_proxy",
                "ALL_PROXY", "all_proxy", "NO_PROXY", "no_proxy"):
        os.environ.pop(key, None)

    ccdinfo_path = data_dir / CCDINFO_FILENAME
    if not ccdinfo_path.exists():
        print(f"[download] {CCDINFO_URL} -> {ccdinfo_path}")
        req = urllib.request.Request(
            CCDINFO_URL, headers={"User-Agent": "AstroCS-BASS-Index/1.0"}
        )
        with urllib.request.urlopen(req, timeout=300) as resp, open(
            ccdinfo_path, "wb"
        ) as fh:
            while True:
                chunk = resp.read(1 << 20)
                if not chunk:
                    break
                fh.write(chunk)
    size = ccdinfo_path.stat().st_size
    print(f"[ccdinfo] {ccdinfo_path} size={size/1e6:.1f} MB")

    data = ccdinfo_path.read_bytes()
    data_offset = parse_headers(data)
    print(f"[parse] data_offset={data_offset}")
    rows = parse_rows(data, data_offset)
    print(f"[parse] rows={len(rows)}")

    # join: index.csv science 文件 (保留日期目录维度的全部条目, 含归档重复项)
    csv_path = base / "index.csv"
    science = []  # (filename, date)
    with open(csv_path, encoding="utf-8-sig", newline="") as fh:
        for r in csv.DictReader(fh):
            if r["k"] == "science":
                science.append((r["f"], r["d"]))
    print(f"[join] science entries in index={len(science)}")

    by_name: dict[str, dict] = {}
    dup = 0
    for r in rows:
        fn = r["filename"]
        if fn in by_name:
            dup += 1
        by_name[fn] = r
    print(f"[join] ccdinfo unique filename={len(by_name)} duplicates={dup}")

    matched = unmatched = 0
    out_cols = [
        "f", "d", "ra", "dec", "ra_obs", "dec_obs",
        "ra_lb", "dec_lb", "ra_lt", "dec_lt", "ra_rt", "dec_rt",
        "ra_rb", "dec_rb",
        "pxl", "see", "am", "exp", "filt", "mjd", "zpt", "zpt_rms",
        "astr_ra_rms", "astr_dec_rms", "imq", "date", "time", "survey", "pass",
    ]
    out_rows = []
    for fn, d in science:
        info = by_name.get(fn)
        if info is None:
            unmatched += 1
            continue
        matched += 1
        out_rows.append(
            {
                "f": fn,
                "d": d,
                "ra": round(info["ra"], 6),
                "dec": round(info["dec"], 6),
                "ra_obs": round(info["ra_obs"], 6),
                "dec_obs": round(info["dec_obs"], 6),
                "ra_lb": round(info["ra_lb"], 6),
                "dec_lb": round(info["dec_lb"], 6),
                "ra_lt": round(info["ra_lt"], 6),
                "dec_lt": round(info["dec_lt"], 6),
                "ra_rt": round(info["ra_rt"], 6),
                "dec_rt": round(info["dec_rt"], 6),
                "ra_rb": round(info["ra_rb"], 6),
                "dec_rb": round(info["dec_rb"], 6),
                "pxl": round(info["pixscale"], 6),
                "see": round(info["seeing"], 4),
                "am": round(info["airmass"], 4),
                "exp": round(info["exptime"], 2),
                "filt": info["filter"],
                "mjd": round(info["mjd"], 5),
                "zpt": round(info["ccdzpt"], 4),
                "zpt_rms": round(info["zptrms"], 4),
                "astr_ra_rms": round(info["ra_rms"], 4),
                "astr_dec_rms": round(info["dec_rms"], 4),
                "imq": info["imq"],
                "date": info["date"],
                "time": info["time"],
                "survey": info["survey"],
                "pass": int(info["pass"]),
            }
        )

    out_path = base / "coords.csv"
    with open(out_path, "w", newline="", encoding="utf-8-sig") as fh:
        writer = csv.DictWriter(fh, fieldnames=out_cols)
        writer.writeheader()
        writer.writerows(out_rows)
    with open(out_path, "rb") as fsrc, gzip.open(
        str(out_path) + ".gz", "wb", compresslevel=9
    ) as fdst:
        fdst.write(fsrc.read())

    print(f"[out] {out_path} rows={len(out_rows)} ({out_path.stat().st_size/1e6:.1f} MB)")
    print(
        f"[out] {out_path}.gz ({(Path(str(out_path)+'.gz').stat().st_size/1e6):.2f} MB)"
    )
    print(f"[join] matched={matched} unmatched={unmatched}")

    # 写回 index.json 汇总
    index_path = base / "index.json"
    index = json.loads(index_path.read_text(encoding="utf-8"))
    index["coords"] = {
        "source": CCDINFO_URL,
        "source_file": str(ccdinfo_path.relative_to(base)),
        "rows_ccdinfo": len(rows),
        "matched": matched,
        "unmatched": unmatched,
        "file": "coords.csv",
        "gz": "coords.csv.gz",
    }
    index_path.write_text(
        json.dumps(index, ensure_ascii=False, separators=(",", ":")), encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
