# -*- coding: utf-8 -*-
"""
批处理脚本 - 使用 C++ DLL 批量校准天文图像（多线程并行版）
功能: 缓存主帧 + ThreadPoolExecutor 多帧并行处理，重叠 IO
优化: 主帧只加载一次，4线程并行处理（每线程4 OpenMP线程=16总线程）
依赖: numpy, astro_calibration (C++ DLL), astro_image_io
调用: python batch_process.py batch_config.json
"""

from __future__ import annotations

import os
import sys
import json
import re
import time
import threading
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

_lib_base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "python")
if _lib_base not in sys.path:
    sys.path.insert(0, _lib_base)

from astro_calibration import (
    AstroCalibration, METHOD_MEDIAN, METHOD_BILINEAR,
)

# 每线程的 OpenMP 线程数（4 worker × 4 omp = 16 总线程）
OMP_PER_WORKER = 4
N_WORKERS = 4


def parse_light_filename(filename: str) -> tuple:
    pattern = r'-(\d+)S-([A-Za-z\-]+)\.fts$'
    m = re.search(pattern, filename, re.IGNORECASE)
    if m:
        return int(m.group(1)), m.group(2)
    return None


def scan_lights(lights_dirs: list, extensions: list) -> list:
    lights = []
    for lights_dir in lights_dirs:
        panel = os.path.basename(os.path.normpath(lights_dir))
        if not os.path.isdir(lights_dir):
            continue
        for f in sorted(os.listdir(lights_dir)):
            ext = os.path.splitext(f)[1].lower()
            if ext not in [e.lower() for e in extensions]:
                continue
            parsed = parse_light_filename(f)
            if parsed is None:
                continue
            exp, filt = parsed
            lights.append((os.path.join(lights_dir, f), panel, exp, filt))
    return lights


# 线程局部存储
_tls = threading.local()


def get_worker_cal():
    """获取当前线程的 AstroCalibration 实例（每线程独立）"""
    if not hasattr(_tls, "cal"):
        _tls.cal = AstroCalibration(max_workers=OMP_PER_WORKER)
    return _tls.cal


def process_one_frame(task: tuple, master_cache: dict, config: dict) -> dict:
    """处理单帧（工作线程内执行）"""
    light_path, panel, exp, filt, out_dir = task

    cal = get_worker_cal()

    basename = os.path.splitext(os.path.basename(light_path))[0]
    out_path = os.path.join(out_dir, f"{basename}_cal.fits")

    dark_data = master_cache["darks"].get(str(exp))
    flat_data = master_cache["flats"].get(filt)
    bias_data = master_cache["bias"]

    if dark_data is None or flat_data is None:
        return {"success": False, "error": "无匹配主帧", "file": basename,
                "panel": panel, "filter": filt, "exposure": exp, "time": 0.0}

    t0 = time.time()

    light_data, w, h, keywords = cal.read_image(light_path)

    result = cal.calibrate_and_correct_mem(
        light_data, w, h, keywords,
        dark_data=dark_data, flat_data=flat_data, bias_data=bias_data,
        dark_optimization=config.get("dark_optimization", False),
        hot_sigma=config.get("hot_sigma", 5.0),
        cold_sigma=config.get("cold_sigma", 5.0),
        method=METHOD_MEDIAN if config.get("method", "median") == "median" else METHOD_BILINEAR,
        max_structure_size=config.get("max_structure_size", 4),
    )

    if result.get("success"):
        out_kw = [kw for kw in (keywords or []) if kw.name.upper() not in ("BZERO", "BSCALE")]
        cal.write_fits(result["data"], out_path, out_kw)

    dt = time.time() - t0
    result["panel"] = panel
    result["filter"] = filt
    result["exposure"] = exp
    result["file"] = basename
    result["time"] = dt
    return result


def main():
    config_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "batch_config.json")

    print(f"[CONFIG] {config_path}")
    with open(config_path, "r", encoding="utf-8") as f:
        config = json.load(f)

    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    cal_dir = os.path.join(base_dir, config["calibration_dir"])
    out_dir = os.path.join(base_dir, config["output_dir"])
    os.makedirs(out_dir, exist_ok=True)

    # ==== 缓存主帧到内存（只加载一次）====
    print("\n[CACHE] 加载主帧到内存...")
    t_cache_start = time.time()
    tmp_cal = AstroCalibration(max_workers=16)

    bias_path = os.path.join(cal_dir, config["master_bias"])
    bias_data, bw, bh, _ = tmp_cal.read_image(bias_path)
    print(f"  Bias: {os.path.basename(bias_path)} ({bw}x{bh})")

    dark_cache = {}
    for exp, fname in config["master_darks"].items():
        dpath = os.path.join(cal_dir, fname)
        ddata, _, _, _ = tmp_cal.read_image(dpath)
        dark_cache[exp] = ddata
        print(f"  Dark {exp}s: {os.path.basename(dpath)}")

    flat_cache = {}
    for filt, fname in config["master_flats"].items():
        fpath = os.path.join(cal_dir, fname)
        fdata, _, _, _ = tmp_cal.read_image(fpath)
        flat_cache[filt] = fdata
        print(f"  Flat {filt}: {os.path.basename(fpath)}")

    master_cache = {"bias": bias_data, "darks": dark_cache, "flats": flat_cache}
    t_cache = time.time() - t_cache_start
    print(f"[CACHE] 主帧缓存完成 ({t_cache:.1f}s)\n")

    # ==== 扫描 Light 帧 ====
    lights = scan_lights(
        [os.path.join(base_dir, d) for d in config["lights_dirs"]],
        config.get("file_extensions", [".fts"]),
    )
    print(f"[SCAN] {len(lights)} Light 帧")
    print(f"[PARALLEL] {N_WORKERS} workers × {OMP_PER_WORKER} omp = {N_WORKERS * OMP_PER_WORKER} threads\n")

    tasks = [(lp, panel, exp, filt, out_dir) for lp, panel, exp, filt in lights]

    # ==== 多线程并行处理 ====
    results = []
    t_start = time.time()

    with ThreadPoolExecutor(max_workers=N_WORKERS) as executor:
        future_map = {
            executor.submit(process_one_frame, task, master_cache, config): i
            for i, task in enumerate(tasks)
        }

        for future in as_completed(future_map):
            i = future_map[future]
            try:
                r = future.result()
                idx = i + 1
                if r.get("success"):
                    print(f"[{idx}/{len(tasks)}] {r['panel']} {r['filter']} {r['exposure']}s | "
                          f"mean={r['original_mean']:.0f}->{r['calibrated_mean']:.0f}"
                          f"->{r['corrected_mean']:.0f} hot={r['hot_pixels']} "
                          f"cold={r['cold_pixels']} {r['time']:.2f}s")
                else:
                    print(f"[{idx}/{len(tasks)}] FAIL {r['file']} {r.get('error')} {r['time']:.2f}s")
                results.append(r)
            except Exception as e:
                print(f"[{i+1}/{len(tasks)}] EXCEPTION: {e}")
                results.append({"success": False, "error": str(e), "file": "", "time": 0.0})

    total_time = time.time() - t_start
    n_success = sum(1 for r in results if r.get("success"))
    n_fail = len(results) - n_success

    print()
    print("=" * 70)
    print(f"批处理完成: {n_success}/{len(results)} 成功, {n_fail} 失败, 总耗时 {total_time:.1f}s")
    print(f"平均每帧: {total_time/max(n_success,1):.2f}s (并行 {N_WORKERS} workers)")

    by_filter = {}
    for r in results:
        if r.get("success"):
            key = f"{r['filter']}_{r['exposure']}s"
            if key not in by_filter:
                by_filter[key] = {"count": 0, "times": [], "cal_means": [], "hot": [], "cold": []}
            by_filter[key]["count"] += 1
            by_filter[key]["times"].append(r["time"])
            by_filter[key]["cal_means"].append(r["calibrated_mean"])
            by_filter[key]["hot"].append(r["hot_pixels"])
            by_filter[key]["cold"].append(r["cold_pixels"])

    print()
    print(f"{'Filter':<15} {'Frames':>6} {'AvgTime':>8} {'CalMean':>10} {'Hot':>8} {'Cold':>8}")
    print("-" * 60)
    for key, stats in sorted(by_filter.items()):
        avg_time = sum(stats["times"]) / len(stats["times"])
        avg_cal = sum(stats["cal_means"]) / len(stats["cal_means"])
        avg_hot = sum(stats["hot"]) / len(stats["hot"])
        avg_cold = sum(stats["cold"]) / len(stats["cold"])
        print(f"{key:<15} {stats['count']:>6} {avg_time:>7.2f}s {avg_cal:>10.1f} "
              f"{int(avg_hot):>8} {int(avg_cold):>8}")

    report_path = os.path.join(out_dir, "batch_report.json")
    clean_results = []
    for r in results:
        cr = {k: v for k, v in r.items() if k != "data"}
        clean_results.append(cr)
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump({
            "timestamp": datetime.now().isoformat(),
            "total": len(results), "success": n_success, "fail": n_fail,
            "total_time": total_time, "cache_time": t_cache,
            "workers": N_WORKERS, "omp_per_worker": OMP_PER_WORKER,
            "results": sorted(clean_results, key=lambda r: r.get("file", "")),
        }, f, ensure_ascii=False, indent=2)
    print(f"\n报告: {report_path}")


if __name__ == "__main__":
    main()
