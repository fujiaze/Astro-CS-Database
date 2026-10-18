#!/usr/bin/env python3
"""Issue #87 / E2：性能与共享 Tile Cache 量化对照（修复前 vs 修复后）。

两组同一真实 M42 writer 产品（只读单位归一化视图）：
  A. cache_resident：384²、scale=0.001°（工作集约 16 tile ≤ 默认 max_tiles=17）
     sb=384/192/96 → 1/4/16 worker；测 checksum + 错误表新增开销。
  B. real_stress：1536²、scale=0.002°（工作集 ~500 tile，远大于默认 cap=25）
     sb=768/384 → 4/16 worker；测真实大工作集/缓存抖动下的端到端回归。
每配置多跑几次，记录 wall / RSS 峰值 / worker_bands / tile_cache / FITS 逐位 sha。

用法:
  python3 实验/issue87-stage3-tile-integrity/perf_matrix.py \
      --baseline /tmp/acsd-e87-base-build/acsd --fixed build/linux-dev/acsd
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import statistics
import subprocess
import sys
import time

import numpy as np
from astropy.io import fits

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
EVID = os.path.join(HERE, "evidence", "perf")
HIPS = os.path.join(WORK, "real", "hips")
CENTER = (83.759765625, -5.421895955151432)

SUITES = [
    # suite, W, H, scale, [(sb, workers_label)], reps
    ("cache_resident", 384, 384, 0.001, [(384, 1), (192, 4), (96, 16)], 4),
    ("real_stress", 1536, 1536, 0.002, [(768, 4), (384, 16)], 4),
]


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def fits_sha(p: str) -> str:
    with fits.open(p) as hdul:
        d = np.asarray(hdul[0].data)
        return hashlib.sha256(np.ascontiguousarray(d).tobytes()).hexdigest()


def run(acsd: str, tag: str, suite: str, w: int, h: int, scale: float,
        sb: int, rep: int) -> dict:
    out = os.path.join(WORK, "perf", tag, f"{suite}_sb{sb}_r{rep}")
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    cfg = {
        "schema_version": "1",
        "source": {"hips_dir": HIPS},
        "output_dir": out,
        "center": {"ra_deg": CENTER[0], "dec_deg": CENTER[1]},
        "width_px": w, "height_px": h, "scale_deg_per_px": scale,
        "sampler": "nearest", "longitude_parity": "east_left",
        "output_mode": "surface_brightness", "sub_block_px": sb,
    }
    cfg_dir = os.path.join(WORK, "perf", "configs")
    os.makedirs(cfg_dir, exist_ok=True)
    cfg_path = os.path.join(cfg_dir, f"{suite}_sb{sb}.json")
    json.dump(cfg, open(cfg_path, "w", encoding="utf-8"), indent=2)
    log = os.path.join(EVID, f"{tag}_{suite}_sb{sb}_r{rep}.log")
    os.makedirs(EVID, exist_ok=True)
    env = dict(os.environ)
    env.pop("ACSD_RT001_FAULT", None)
    t0 = time.monotonic()
    with open(log, "w", encoding="utf-8") as lf:
        proc = subprocess.run([acsd, "export", "--json", cfg_path, "-y"],
                              stdout=lf, stderr=subprocess.STDOUT, env=env)
    wall = time.monotonic() - t0
    rec = {"tag": tag, "suite": suite, "sb": sb, "rep": rep,
           "exit": proc.returncode, "wall_s": wall, "log": log}
    res_p = os.path.join(out, "p3_resampled.json")
    if os.path.isfile(res_p):
        res = json.load(open(res_p, encoding="utf-8"))
        rec["worker_bands"] = res.get("worker_bands")
        rec["tile_cache"] = res.get("tile_cache")
        rec["tile_integrity"] = res.get("tile_integrity")
        rec["bin_sha256"] = res.get("bin_sha256")
    rs_p = os.path.join(out, "resource_summary.json")
    if os.path.isfile(rs_p):
        rs = json.load(open(rs_p, encoding="utf-8"))
        rec["resource_wall_s"] = rs.get("wall_seconds")
        rec["rss_peak_bytes"] = max((s.get("rss_peak_bytes", 0) or 0)
                                    for s in rs.get("stages", []))
        rec["workers_peak"] = max((s.get("workers_peak", 0) or 0)
                                  for s in rs.get("stages", []))
    fp = os.path.join(out, "output_phase3.fits")
    if os.path.isfile(fp):
        rec["fits_sha256"] = fits_sha(fp)
        rec["fits_size"] = os.path.getsize(fp)
        with fits.open(fp) as hdul:
            d = np.asarray(hdul[0].data)
            rec["nan_px"] = int(np.isnan(d).sum())
            rec["finite_px"] = int(np.isfinite(d).sum())
    return rec


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--fixed", required=True)
    args = ap.parse_args()
    os.makedirs(EVID, exist_ok=True)
    if not os.path.isdir(HIPS):
        print(f"missing real view: {HIPS}", file=sys.stderr)
        return 2
    run(args.fixed, "warmup", "cache_resident", 384, 384, 0.001, 96, 0)
    results = []
    # 交错执行 prefix/fixed（同一 sb 背靠背），减小页缓存/系统漂移偏差。
    for suite, w, h, scale, sbs, reps in SUITES:
        for r in range(reps):
            for tag, acsd in (("prefix", args.baseline), ("fixed", args.fixed)):
                for sb, wl in sbs:
                    rec = run(acsd, tag, suite, w, h, scale, sb, r)
                    rec["workers_label"] = wl
                    results.append(rec)
                    print(f"[perf] {suite:14s} {tag:6s} sb={sb:4d} "
                          f"(label {wl:2d}) r{r} exit={rec['exit']} "
                          f"wall={rec['wall_s']:.3f}s "
                          f"rss={(rec.get('rss_peak_bytes') or 0)/1e6:.1f}MB "
                          f"sha={(rec.get('fits_sha256') or '')[:16]}")
    out = {"schema": "acsd.issue87.perf/v1",
           "hips": HIPS, "center": CENTER,
           "baseline_acsd_sha256": sha256_file(args.baseline),
           "fixed_acsd_sha256": sha256_file(args.fixed),
           "results": results}
    path = os.path.join(EVID, "results.json")
    json.dump(out, open(path, "w", encoding="utf-8"), indent=2)
    print(f"wrote {path}")

    summary = {}
    for suite, _w, _h, _s, sbs, _reps in SUITES:
        for sb, wl in sbs:
            for tag in ("prefix", "fixed"):
                rs = [r for r in results
                      if r["tag"] == tag and r["suite"] == suite and r["sb"] == sb]
                shas = sorted({r.get("fits_sha256") for r in rs if r.get("fits_sha256")})
                summary[f"{suite}_{tag}_sb{sb}"] = {
                    "workers_label": wl,
                    "wall_min_s": round(min(r["wall_s"] for r in rs), 3),
                    "wall_median_s": round(statistics.median([r["wall_s"] for r in rs]), 3),
                    "wall_max_s": round(max(r["wall_s"] for r in rs), 3),
                    "rss_peak_mb": round(max((r.get("rss_peak_bytes") or 0)
                                             for r in rs) / 1e6, 1),
                    "workers_peak": max(r.get("workers_peak", 0) for r in rs),
                    "fits_sha_unique": len(shas),
                    "fits_sha256": shas,
                    "tile_cache": rs[0].get("tile_cache"),
                }
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
