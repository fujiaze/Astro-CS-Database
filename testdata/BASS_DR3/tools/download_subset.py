# -*- coding: utf-8 -*-
"""BASS DR3 子集下载工具 (可按需只下 science / weight / od)

特性:
  - 直连, 不使用代理
  - 从 index.csv / coords.csv 选择子集:
      --dates     日期 (逗号分隔, 支持 20150107-20150110 范围)
      --pointing  指向前缀 (如 p7030g0031)
      --box       "ra_min,dec_min,ra_max,dec_max" (用 coords.csv 中心坐标过滤, science 才可用)
      --kinds     science,weight,od (默认 science)
  - --funpack: 下载后用 MSYS2 funpack.exe 解压 .fits.fz -> .fits
    (主线 astro_image_io 目前不支持 fpack .fz, 需先转换)
  - --list: 只列出匹配文件, 不下载

用法:
  py -3.12 download_subset.py --dates 20151111 --out ../downloads
  py -3.12 download_subset.py --pointing p7030g0031 --kinds science,weight,od --funpack
  py -3.12 download_subset.py --box "36.5,47.8,37.2,48.4" --limit 50
"""

from __future__ import annotations

import argparse
import csv
import os
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import requests

ROOT_URL = "https://casdc.china-vo.org/archive/BASS/DR3/single_image/"
FUNPACK = r"C:\msys64\mingw64\bin\funpack.exe"


def build_session() -> requests.Session:
    s = requests.Session()
    s.trust_env = False
    s.proxies = {"http": None, "https": None}
    s.headers["User-Agent"] = "AstroCS-BASS-Downloader/1.0"
    return s


def parse_dates(spec: str) -> set[str]:
    out: set[str] = set()
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-", 1)
            a, b = int(a), int(b)
            out.update(str(d) for d in range(a, b + 1))
        else:
            out.add(part)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="BASS DR3 子集下载")
    ap.add_argument("--dir", type=Path, default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--kinds", default="science")
    ap.add_argument("--dates", default=None, help="如 20151111 或 20151111,20151112 或 20150107-20150110")
    ap.add_argument("--pointing", default=None, help="指向前缀")
    ap.add_argument("--box", default=None, help="ra_min,dec_min,ra_max,dec_max")
    ap.add_argument("--list", action="store_true", help="只列出, 不下载")
    ap.add_argument("--funpack", action="store_true", help="下载后 funpack 转 .fits")
    ap.add_argument("--workers", type=int, default=6)
    ap.add_argument("--limit", type=int, default=0, help="最多下载 N 个 (测试用)")
    args = ap.parse_args()

    for key in ("HTTP_PROXY", "HTTPS_PROXY", "http_proxy", "https_proxy",
                "ALL_PROXY", "all_proxy", "NO_PROXY", "no_proxy"):
        os.environ.pop(key, None)

    base: Path = args.dir.resolve()
    kinds = set(args.kinds.split(","))
    dates = parse_dates(args.dates) if args.dates else None

    # 1. 加载索引
    entries = []
    with open(base / "index.csv", encoding="utf-8-sig", newline="") as fh:
        for r in csv.DictReader(fh):
            if r["k"] not in kinds:
                continue
            if dates and r["d"] not in dates:
                continue
            if args.pointing and not r["p"].startswith(args.pointing):
                continue
            entries.append(r)

    # 2. 天区过滤 (science 才配坐标; 非 science 带 box 时报错)
    if args.box:
        if "science" not in kinds:
            print("error: --box 需要 kinds 含 science")
            return 2
        ra0, dec0, ra1, dec1 = (float(x) for x in args.box.split(","))
        coords = {}
        with open(base / "coords.csv", encoding="utf-8-sig", newline="") as fh:
            for r in csv.DictReader(fh):
                coords[(r["f"], r["d"])] = (float(r["ra"]), float(r["dec"]))
        kept = []
        for e in entries:
            c = coords.get((e["f"], e["d"]))
            if not c:
                continue
            ra, dec = c
            if ra0 <= ra <= ra1 and dec0 <= dec <= dec1:
                kept.append(e)
        entries = kept

    if args.limit:
        entries = entries[: args.limit]

    print(f"[select] matched={len(entries)} kinds={sorted(kinds)}")
    if args.list:
        for e in entries:
            print(f"{e['d']}/{e['f']} {e['s']} {e['k']}")
        return 0

    out_dir: Path = args.out.resolve()
    session = build_session()
    ok = fail = 0

    def one(entry: dict) -> tuple[bool, str]:
        rel = Path(entry["d"]) / entry["f"]
        dest = out_dir / rel
        if dest.exists() and dest.stat().st_size == int(entry["s"]):
            return True, f"skip {rel}"
        dest.parent.mkdir(parents=True, exist_ok=True)
        url = ROOT_URL + f"{entry['d']}/{entry['f']}"
        with session.get(url, stream=True, timeout=(20, 120)) as resp:
            resp.raise_for_status()
            with open(dest, "wb") as fh:
                for chunk in resp.iter_content(1 << 20):
                    fh.write(chunk)
        if dest.stat().st_size != int(entry["s"]):
            dest.unlink(missing_ok=True)
            return False, f"size mismatch {rel}"
        if args.funpack and entry["f"].endswith(".fits.fz"):
            out_fits = dest.with_suffix("")  # xxx.fits.fz -> xxx.fits
            subprocess.run(
                [FUNPACK, "-O", str(out_fits), str(dest)],
                check=True, capture_output=True, timeout=300,
            )
            if out_fits.exists():
                dest.unlink(missing_ok=True)
            else:
                return False, f"funpack failed {rel}"
        return True, f"ok {rel}"

    t0 = time.perf_counter()
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(one, e) for e in entries]
        for i, fut in enumerate(as_completed(futures), 1):
            good, msg = fut.result()
            ok += good
            fail += 0 if good else 1
            if fail and not good:
                print("  FAIL:", msg)
            if i % 25 == 0 or i == len(futures):
                print(f"[progress] {i}/{len(futures)} ok={ok} fail={fail}")
    print(
        f"[done] ok={ok} fail={fail} elapsed={time.perf_counter()-t0:.1f}s "
        f"out={out_dir}"
    )
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
