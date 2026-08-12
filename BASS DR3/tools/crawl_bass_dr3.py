# -*- coding: utf-8 -*-
"""
BASS DR3 single_image 归档索引构建工具

数据源: https://casdc.china-vo.org/archive/BASS/DR3/single_image/
结构:   single_image/<YYYYMMDD>/<filename>.fits.fz
        - <pointing>_<ccd>.fits.fz         科学图像 (fpack 压缩 FITS)
        - <pointing>_<ccd>.wht.fits.fz     权重图
        - <pointing>_<ccd>_od.fits.fz      目标检测/掩码图 (od)

特性:
  - 强制直连, 不使用任何代理 (trust_env=False + 清理代理环境变量)
  - 并发抓取日期页 + 自动重试
  - 产物:
      index.json          索引总表 (元数据 + 汇总 + 按日期聚合, 紧凑)
      index.csv           扁平 CSV (一行一个文件, 短字段, 无 URL) + .gz
      dates/<date>.json   按日期分片索引 (无 URL)

用法:
  py -3.12 crawl_bass_dr3.py [--workers 10] [--full-json]
"""

from __future__ import annotations

import argparse
import csv
import datetime as _dt
import gzip
import json
import logging
import os
import re
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Optional

import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

ROOT_URL = "https://casdc.china-vo.org/archive/BASS/DR3/single_image/"
DATE_DIR_RE = re.compile(r'href="(\d{8})/"')
FILE_ROW_RE = re.compile(
    r'<a href="([^"]+\.fits\.fz)">[^<]*</a></td><td class="size">(\d+)</td>'
)
ALL_LINK_RE = re.compile(r'<a href="([^"]+)">')

# 分类规则 (按文件名后缀, 优先级从高到低)
def classify(filename: str) -> str:
    if filename.endswith("_od.fits.fz"):
        return "od"
    if filename.endswith(".wht.fits.fz"):
        return "weight"
    if filename.endswith(".fits.fz"):
        return "science"
    return "other"


def parse_identity(filename: str) -> tuple[str, Optional[str]]:
    """从文件名解析 pointing 与 ccd。例:
    p7030g0031_1.fits.fz      -> ("p7030g0031", "1")
    p7030g0031_1.wht.fits.fz  -> ("p7030g0031", "1")
    p7030g0031_1_od.fits.fz   -> ("p7030g0031", "1")
    """
    stem = filename[: -len(".fits.fz")] if filename.endswith(".fits.fz") else filename
    if stem.endswith("_od"):
        stem = stem[:-3]
    if stem.endswith(".wht"):
        stem = stem[:-4]
    m = re.fullmatch(r"(.*)_(\d+)", stem)
    if m:
        return m.group(1), m.group(2)
    return stem, None


def build_session() -> requests.Session:
    # 强制直连: 忽略任何代理配置
    session = requests.Session()
    session.trust_env = False
    session.proxies = {"http": None, "https": None}
    session.headers.update(
        {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            "AstroCS-BASS-Index/1.0",
            "Accept-Encoding": "gzip, deflate",
        }
    )
    retry = Retry(
        total=5,
        connect=5,
        read=5,
        backoff_factor=0.6,
        status_forcelist=[429, 500, 502, 503, 504],
        allowed_methods=frozenset(["GET"]),
        raise_on_status=False,
    )
    adapter = HTTPAdapter(max_retries=retry, pool_connections=16, pool_maxsize=16)
    session.mount("https://", adapter)
    session.mount("http://", adapter)
    return session


def fetch_text(session: requests.Session, url: str) -> str:
    resp = session.get(url, timeout=(20, 60))
    resp.raise_for_status()
    return resp.text


def fetch_date_page(session: requests.Session, date: str, base: str) -> tuple:
    """返回 (date, files, other_links) 或抛出异常。"""
    url = base + date + "/"
    html = fetch_text(session, url)
    rows = []
    other = []
    for href, size in FILE_ROW_RE.findall(html):
        try:
            size_i = int(size)
        except ValueError:
            size_i = -1
        rows.append((href, size_i))
    # 记录非 .fits.fz 链接 (index.html 等), 供校验
    fits_set = {name for name, _ in rows}
    for href in ALL_LINK_RE.findall(html):
        if href.startswith("../") or href.endswith("/") or href in fits_set:
            continue
        other.append(href)
    return date, rows, other


def main() -> int:
    parser = argparse.ArgumentParser(description="BASS DR3 single_image 索引构建")
    parser.add_argument("--workers", type=int, default=10)
    parser.add_argument("--out-dir", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--log-dir", type=Path, default=None)
    parser.add_argument("--limit-dates", type=int, default=0, help="调试用: 只抓前 N 个日期")
    parser.add_argument(
        "--full-json",
        action="store_true",
        help="index.json 内嵌全部文件条目 (约 115MB, 默认只含元数据+汇总+按日期聚合)",
    )
    args = parser.parse_args()

    out_dir: Path = args.out_dir.resolve()
    log_dir: Path = (args.log_dir or (out_dir / "logs")).resolve()
    log_dir.mkdir(parents=True, exist_ok=True)
    dates_dir = out_dir / "dates"
    dates_dir.mkdir(parents=True, exist_ok=True)

    # 清理代理环境变量 (双保险)
    for key in ("HTTP_PROXY", "HTTPS_PROXY", "http_proxy", "https_proxy",
                "ALL_PROXY", "all_proxy", "NO_PROXY", "no_proxy"):
        os.environ.pop(key, None)

    ts = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    log_path = log_dir / f"crawl_{ts}.log"
    logging.basicConfig(
        filename=str(log_path),
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        encoding="utf-8",
    )
    logging.info("start crawl, workers=%d out=%s", args.workers, out_dir)

    session = build_session()

    # 1. 根索引页 -> 日期目录列表
    root_html = fetch_text(session, ROOT_URL)
    dates = sorted(set(DATE_DIR_RE.findall(root_html)))
    if args.limit_dates:
        dates = dates[: args.limit_dates]
    logging.info("root page ok, date_dirs=%d", len(dates))

    # 2. 并发抓取日期页
    # 紧凑字段: d=date f=file s=size k=kind p=pointing c=ccd (不再存全长 URL)
    all_files: dict[str, list[dict]] = {}
    other_links_total: dict[str, int] = {}
    failed: list[str] = []
    ok = 0
    start = time.perf_counter()
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(fetch_date_page, session, d, ROOT_URL): d for d in dates}
        for i, fut in enumerate(as_completed(futures), 1):
            date = futures[fut]
            try:
                _, rows, other = fut.result()
                entries = []
                for name, size in rows:
                    kind = classify(name)
                    pointing, ccd = parse_identity(name)
                    entries.append(
                        {
                            "f": name,
                            "s": size,
                            "k": kind,
                            "p": pointing,
                            "c": ccd,
                        }
                    )
                all_files[date] = entries
                for o in other:
                    other_links_total[o] = other_links_total.get(o, 0) + 1
                ok += 1
            except Exception as exc:  # noqa: BLE001
                failed.append(date)
                logging.error("date=%s failed: %r", date, exc)
            if i % 25 == 0 or i == len(dates):
                logging.info("progress %d/%d ok=%d failed=%d", i, len(dates), ok, len(failed))
                print(f"[crawl] {i}/{len(dates)} ok={ok} failed={len(failed)}")

    elapsed = time.perf_counter() - start

    # 3. 失败重试一次
    if failed:
        retry_failed = list(failed)
        failed.clear()
        for date in retry_failed:
            try:
                _, rows, other = fetch_date_page(session, date, ROOT_URL)
                entries = [
                    {
                        "f": name,
                        "s": size,
                        "k": classify(name),
                        "p": parse_identity(name)[0],
                        "c": parse_identity(name)[1],
                    }
                    for name, size in rows
                ]
                all_files[date] = entries
                ok += 1
                logging.info("retry ok date=%s", date)
            except Exception as exc:  # noqa: BLE001
                failed.append(date)
                logging.error("retry failed date=%s: %r", date, exc)

    # 4. 汇总统计
    total_files = sum(len(v) for v in all_files.values())
    total_size = sum(e["s"] for v in all_files.values() for e in v if e["s"] > 0)
    kinds: dict[str, int] = {}
    for v in all_files.values():
        for e in v:
            kinds[e["k"]] = kinds.get(e["k"], 0) + 1
    missing_size = sum(1 for v in all_files.values() for e in v if e["s"] < 0)

    index = {
        "dataset": "BASS DR3 single_image",
        "source_url": ROOT_URL,
        "built_at": _dt.datetime.now().isoformat(timespec="seconds"),
        "builder": "tools/crawl_bass_dr3.py",
        "connection": "direct (no proxy)",
        "summary": {
            "date_dirs": len(dates),
            "date_dirs_indexed": len(all_files),
            "date_dirs_failed": len(failed),
            "total_files": total_files,
            "total_size_bytes": total_size,
            "kinds": kinds,
            "size_unknown_count": missing_size,
            "other_link_types": other_links_total,
            "crawl_seconds": round(elapsed, 2),
        },
        "dates": [
            {
                "date": d,
                "files": len(all_files[d]),
                "size_bytes": sum(e["s"] for e in all_files[d] if e["s"] > 0),
            }
            for d in sorted(all_files)
        ],
    }
    if args.full_json:
        index["files"] = [
            {**e, "d": d} for d in sorted(all_files) for e in all_files[d]
        ]

    # 5. 写产物
    index_json = out_dir / "index.json"
    index_json.write_text(
        json.dumps(index, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )

    csv_path = out_dir / "index.csv"
    all_entries = [e for d in sorted(all_files) for e in all_files[d]]
    with open(csv_path, "w", newline="", encoding="utf-8-sig") as fh:
        writer = csv.DictWriter(
            fh,
            fieldnames=["d", "f", "s", "k", "p", "c"],
        )
        writer.writeheader()
        for date, entries in sorted(all_files.items()):
            for e in entries:
                writer.writerow({"d": date, **e})

    # gzip 压缩版
    with open(csv_path, "rb") as fsrc, gzip.open(
        str(csv_path) + ".gz", "wb", compresslevel=9
    ) as fdst:
        fdst.write(fsrc.read())

    for d in sorted(all_files):
        (dates_dir / f"{d}.json").write_text(
            json.dumps(
                {
                    "date": d,
                    "files": all_files[d],
                },
                ensure_ascii=False,
                separators=(",", ":"),
            ),
            encoding="utf-8",
        )

    summary = index["summary"]
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    print(f"[done] index.json={index_json} ({index_json.stat().st_size/1024:.1f} KB)")
    print(f"[done] index.csv={csv_path} ({csv_path.stat().st_size/1024:.1f} KB)")
    print(f"[done] index.csv.gz={csv_path}.gz ({(csv_path.stat().st_size/1024/1024):.2f} MB -> {(csv_path.with_suffix(csv_path.suffix+'.gz').stat().st_size/1024/1024):.2f} MB)")
    print(f"[done] dates json -> {dates_dir}")
    print(f"[done] log -> {log_path}")
    logging.info(
        "done total_files=%d total_size=%d failed=%d elapsed=%.2fs",
        total_files,
        total_size,
        len(failed),
        elapsed,
    )
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
