# -*- coding: utf-8 -*-
"""
BASS DR3 single_image 索引一致性校验工具

校验项:
  1. index.csv 行数与 index.json 汇总一致 (紧凑字段 d,f,s,k,p,c, 无 URL)
  2. 每个日期 science/weight/od 三类数量相等, ccd 均为 1-4
  3. 按日期分片 dates/<date>.json 与 index.csv 一致
  4. coords.csv 行数 == science 数, RA/DEC 合法, 全部文件名可 join
  5. 抽样 N 个文件用 HTTP Range GET 验证 URL 可下载且字节数与索引一致

用法 (直连, 不使用代理):
  py -3.12 verify_bass_dr3.py [--sample 20] [--workers 8] [--dir ..]
"""

from __future__ import annotations

import argparse
import collections
import csv
import json
import os
import random
import sys
import collections
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import requests


def main() -> int:
    parser = argparse.ArgumentParser(description="BASS DR3 索引一致性校验")
    parser.add_argument("--dir", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--sample", type=int, default=20)
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--seed", type=int, default=20260812)
    args = parser.parse_args()

    base: Path = args.dir.resolve()
    # 清理代理环境变量
    for key in ("HTTP_PROXY", "HTTPS_PROXY", "http_proxy", "https_proxy",
                "ALL_PROXY", "all_proxy", "NO_PROXY", "no_proxy"):
        os.environ.pop(key, None)

    errors: list[str] = []

    index = json.loads((base / "index.json").read_text(encoding="utf-8"))
    with open(base / "index.csv", encoding="utf-8-sig", newline="") as fh:
        rows = list(csv.DictReader(fh))
    summary = index["summary"]

    # 1. 行数与汇总一致
    if len(rows) != summary["total_files"]:
        errors.append(f"csv rows {len(rows)} != summary.total_files {summary['total_files']}")

    # 2. 日期内三类数量相等 + ccd 范围
    by_date: dict[str, list[dict]] = collections.defaultdict(list)
    for r in rows:
        by_date[r["d"]].append(r)
    kind_counter = collections.Counter()
    ccd_counter = collections.Counter()
    for date, entries in by_date.items():
        kc = collections.Counter(e["k"] for e in entries)
        if not (kc["science"] == kc["weight"] == kc["od"]):
            errors.append(f"{date}: kinds unequal {dict(kc)}")
        for e in entries:
            kind_counter[e["k"]] += 1
            ccd_counter[e["c"]] += 1
    if set(kind_counter) != {"science", "weight", "od"}:
        errors.append(f"unexpected kinds {dict(kind_counter)}")
    if set(ccd_counter) - {"1", "2", "3", "4"}:
        errors.append(f"unexpected ccd {set(ccd_counter) - {'1','2','3','4'}}")
    if len(by_date) != summary["date_dirs_indexed"]:
        errors.append(f"dates in csv {len(by_date)} != summary {summary['date_dirs_indexed']}")

    # 3. 日期分片一致性
    for date, entries in by_date.items():
        split = json.loads((base / "dates" / f"{date}.json").read_text(encoding="utf-8"))
        if len(split["files"]) != len(entries):
            errors.append(f"{date}: split files {len(split['files'])} != csv {len(entries)}")
        csv_keys = {(e["f"], str(e["s"])) for e in entries}
        split_keys = {(e["f"], str(e["s"])) for e in split["files"]}
        if csv_keys != split_keys:
            errors.append(f"{date}: file/size sets differ between csv and split json")

    # 4. coords 一致性
    coords_path = base / "coords.csv"
    if coords_path.exists():
        with open(coords_path, encoding="utf-8-sig", newline="") as fh:
            coords = list(csv.DictReader(fh))
        n_science = kind_counter["science"]
        if len(coords) != n_science:
            errors.append(f"coords rows {len(coords)} != science {n_science}")
        coord_keys = {(c["f"], c["d"]) for c in coords}
        science_keys = {
            (e["f"], e["d"]) for e in rows if e["k"] == "science"
        }
        if coord_keys != science_keys:
            errors.append(
                f"coords keys differ: missing={len(science_keys-coord_keys)} extra={len(coord_keys-science_keys)}"
            )
        bad = [
            c for c in coords
            if not (0 <= float(c["ra"]) < 360) or not (-90 <= float(c["dec"]) <= 90)
        ]
        if bad:
            errors.append(f"coords invalid ra/dec rows: {len(bad)}")
    else:
        print("[warn] coords.csv not present, skip check 4")

    # 5. 抽样下载验证 (Range GET, URL 由 d/f 拼接)
    session = requests.Session()
    session.trust_env = False
    session.proxies = {"http": None, "https": None}
    session.headers["User-Agent"] = "AstroCS-BASS-Index-Verify/1.0"

    rng = random.Random(args.seed)
    sample = rng.sample(rows, min(args.sample, len(rows)))

    def check(entry: dict) -> str | None:
        try:
            url = f"https://casdc.china-vo.org/archive/BASS/DR3/single_image/{entry['d']}/{entry['f']}"
            resp = session.get(
                url, headers={"Range": "bytes=0-63"}, timeout=30
            )
            total = resp.headers.get("Content-Range", "")
            if resp.status_code != 206 or not total.endswith(f"/{entry['s']}"):
                return f"{entry['d']} {entry['f']}: status={resp.status_code} range={total} expect={entry['s']}"
        except Exception as exc:  # noqa: BLE001
            return f"{entry['d']} {entry['f']}: {exc!r}"
        return None

    sample_fail = 0
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(check, e): e for e in sample}
        for fut in as_completed(futures):
            err = fut.result()
            if err:
                sample_fail += 1
                errors.append(err)

    print(f"dates={len(by_date)} files={len(rows)} kinds={dict(kind_counter)}")
    print(f"ccd={dict(sorted(ccd_counter.items()))}")
    print(f"sample_downloads={len(sample)} ok={len(sample) - sample_fail} fail={sample_fail}")
    if errors:
        print("FAILED CHECKS:")
        for e in errors[:30]:
            print("  -", e)
        return 1
    print("ALL CHECKS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
