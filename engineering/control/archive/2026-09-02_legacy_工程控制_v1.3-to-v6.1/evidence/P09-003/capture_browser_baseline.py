#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P09-003 T5: 浏览器性能基线 - 修改前事实记录

目标:
- 记录修改前浏览器的 trace 点能力 (event-level, 无 timing)
- 通过 offscreen 模式启动浏览器, 加载默认 HISS 文件, 捕获 stderr 事件日志
- 记录 wall-clock 启动时间, 文件打开事件序列
- 量化当前 trace 覆盖率 (有/无 timing)
- 标识 P15-001 需要补齐的 trace 缺口

注意:
- 浏览器无 QElapsedTimer / std::chrono / FPS 计数器 (仅 event-level 日志)
- 本脚本只记录 "修改前事实", 不修改浏览器代码
- 实际性能优化在 P15-001 完成
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
EVIDENCE_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P09-003"
RAW_LOGS_DIR = EVIDENCE_DIR / "raw_logs"

BROWSER_EXE = REPO_ROOT / "lib" / "healpix_db" / "healpix_browser_qt" / "build" / "healpix_browser_qt.exe"
DEFAULT_HISS = REPO_ROOT / "output" / "pipeline_debug" / "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red" / "drizzle" / "T4_2x_nside65536.hiss"
P07_HCSD = REPO_ROOT / "engineering" / "evidence" / "P07-001" / "output" / "stage2_run1.hcsd"

MINGW_BIN = r"C:\msys64\mingw64\bin"
QT_PLUGIN_PATH = r"C:\msys64\mingw64\share\Qt6\plugins"
ASTRO_IMAGE_IO_DIR = str(REPO_ROOT / "lib" / "astro_image_io")


def sha256_file(path: Path) -> str:
    import hashlib
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def run_browser_headless(target_file: Path, timeout_sec: float = 30.0) -> dict:
    """启动浏览器 (offscreen), 加载文件, 捕获 stderr, 超时后终止."""
    env = os.environ.copy()
    # 关键: astro_image_io.dll 在 lib/astro_image_io/, 不在浏览器 build/ 目录, 必须加入 PATH
    env["PATH"] = ASTRO_IMAGE_IO_DIR + os.pathsep + MINGW_BIN + os.pathsep + env.get("PATH", "")
    env["QT_PLUGIN_PATH"] = QT_PLUGIN_PATH
    env["QT_QPA_PLATFORM"] = "offscreen"
    env["BROWSER_LOG_FILE"] = str(RAW_LOGS_DIR / f"browser_buffer_{target_file.suffix[1:]}.log")

    cmd = [str(BROWSER_EXE), str(target_file)]
    t0 = time.time()
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        cwd=str(REPO_ROOT),
    )
    try:
        # 等待文件加载完成 (浏览器事件循环不会自己退出)
        proc.wait(timeout=timeout_sec)
        exit_code = proc.returncode
        timed_out = False
    except subprocess.TimeoutExpired:
        # 预期行为: 浏览器事件循环不退出, 我们手动终止
        proc.terminate()
        try:
            proc.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=2.0)
        exit_code = proc.returncode if proc.returncode is not None else -9999
        timed_out = True
    elapsed = round(time.time() - t0, 3)

    stdout, stderr = proc.communicate()
    stdout_str = stdout.decode("utf-8", errors="replace") if stdout else ""
    stderr_str = stderr.decode("utf-8", errors="replace") if stderr else ""

    return {
        "cmd": cmd,
        "env_qt_qpa_platform": env["QT_QPA_PLATFORM"],
        "env_browser_log_file": env["BROWSER_LOG_FILE"],
        "exit_code": exit_code,
        "timed_out": timed_out,
        "elapsed_sec": elapsed,
        "stdout": stdout_str,
        "stderr": stderr_str,
        "stdout_bytes": len(stdout or b""),
        "stderr_bytes": len(stderr or b""),
    }


def parse_browser_events(stderr: str) -> dict:
    """解析浏览器 stderr 事件日志, 统计 trace 点类型与计数 + 提取时间戳计算耗时."""
    events = []
    hio_events = []  # [hio] 前缀的低级 C 日志 (无时间戳)
    for line in stderr.splitlines():
        # [hio] 前缀的低级日志 (无时间戳)
        if line.startswith("[hio]"):
            hio_events.append({"raw": line, "message": line})
            continue
        # 格式: [YYYY-MM-DD HH:MM:SS][LEVEL] message
        m = re.match(r"\[([^\]]+)\]\[([A-Z]+)\]\s*(.*)", line)
        if m:
            events.append({
                "timestamp": m.group(1),
                "level": m.group(2),
                "message": m.group(3),
            })

    # 按关键字分类统计 trace 点
    trace_categories = {
        "file_open": 0,
        "header_decompress": 0,
        "index_read": 0,
        "visible_leaf_query": 0,
        "leaf_io": 0,
        "ud_grade": 0,
        "cpu_lookup": 0,
        "vbo_build": 0,
        "vbo_upload": 0,
        "draw": 0,
        "stf": 0,
        "view_change": 0,
        "other": 0,
    }
    for e in events:
        msg = e["message"].lower()
        if "open" in msg and ("file" in msg or ".hiss" in msg or ".hcsd" in msg or "magic" in msg):
            trace_categories["file_open"] += 1
        elif "解压" in e["message"] or "decompress" in msg or "子叶索引" in e["message"]:
            trace_categories["header_decompress"] += 1
        elif "索引" in e["message"] and "子叶" not in e["message"]:
            trace_categories["index_read"] += 1
        elif "可见叶" in e["message"] or "required_leaves" in msg or "candidate" in msg:
            trace_categories["visible_leaf_query"] += 1
        elif "leaf" in msg and "load" in msg or "子叶" in e["message"]:
            trace_categories["leaf_io"] += 1
        elif "ud_grade" in msg or "降采样" in e["message"]:
            trace_categories["ud_grade"] += 1
        elif "lookup" in msg or "nside_target" in msg or "nside_ideal" in msg:
            trace_categories["cpu_lookup"] += 1
        elif "vbo" in msg or "mesh" in msg or "顶点" in e["message"]:
            trace_categories["vbo_build"] += 1
        elif "upload" in msg or "纹理" in e["message"]:
            trace_categories["vbo_upload"] += 1
        elif "render" in msg or "draw" in msg or "渲染" in e["message"]:
            trace_categories["draw"] += 1
        elif "stf" in msg or "stretch" in msg or "拉伸" in e["message"]:
            trace_categories["stf"] += 1
        elif "fov" in msg or "view" in msg or "视角" in e["message"] or "reset" in msg:
            trace_categories["view_change"] += 1
        else:
            trace_categories["other"] += 1

    # 从时间戳提取耗时 (秒级精度, 来自日志时间戳)
    timings = {}
    if len(events) >= 2:
        try:
            ts_first = datetime.strptime(events[0]["timestamp"], "%Y-%m-%d %H:%M:%S")
            ts_last = datetime.strptime(events[-1]["timestamp"], "%Y-%m-%d %H:%M:%S")
            timings["first_event_ts"] = events[0]["timestamp"]
            timings["last_event_ts"] = events[-1]["timestamp"]
            timings["event_span_sec"] = (ts_last - ts_first).total_seconds()
        except (ValueError, KeyError):
            pass

    # 寻找 leaf index 开始/完成时间戳, 计算 leaf_index_duration
    leaf_start_ts = None
    leaf_end_ts = None
    for e in events:
        if "build_hiss_leaf_index" in e["message"] and "开始" in e["message"]:
            leaf_start_ts = e["timestamp"]
        elif "build_hiss_leaf_index" in e["message"] and "完成" in e["message"]:
            leaf_end_ts = e["timestamp"]
    if leaf_start_ts and leaf_end_ts:
        try:
            t_start = datetime.strptime(leaf_start_ts, "%Y-%m-%d %H:%M:%S")
            t_end = datetime.strptime(leaf_end_ts, "%Y-%m-%d %H:%M:%S")
            timings["leaf_index_start_ts"] = leaf_start_ts
            timings["leaf_index_end_ts"] = leaf_end_ts
            timings["leaf_index_duration_sec"] = (t_end - t_start).total_seconds()
        except ValueError:
            pass

    return {
        "total_events": len(events),
        "total_hio_events": len(hio_events),
        "trace_categories": trace_categories,
        "timings": timings,
        "events": events,
        "hio_events": hio_events,
    }


def main() -> int:
    RAW_LOGS_DIR.mkdir(parents=True, exist_ok=True)

    print(f"P09-003 T5 浏览器性能基线 - 修改前事实记录")
    print(f"开始时间: {datetime.now(timezone.utc).isoformat()}")
    print()

    # --- 固定硬件与软件环境 (引用 P07-001 基线) ---
    env_info = {
        "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
        "hardware_source": "engineering/evidence/P07-001/performance_baseline.json",
        "qt_platform": "offscreen",
        "notes": (
            "硬件环境引用 P07-001 性能基线 (AMD Ryzen 7 5800X, 63.91GB RAM, Win11). "
            "本基线使用 QT_QPA_PLATFORM=offscreen 运行, 不依赖 GPU 显示输出. "
            "实际 GUI 性能测量将在 P15-001 完成 (需真实 GPU + 显示器)."
        ),
    }

    # --- 浏览器代码状态 (无 timing instrumentation) ---
    browser_code_state = {
        "exe_path": str(BROWSER_EXE.relative_to(REPO_ROOT)),
        "exe_sha256": sha256_file(BROWSER_EXE) if BROWSER_EXE.exists() else None,
        "timing_instrumentation": {
            "QElapsedTimer_used": False,
            "std_chrono_used": False,
            "high_resolution_clock_used": False,
            "steady_clock_used": False,
            "fps_counter": False,
            "frame_time_display": False,
            "draw_time_measurement": False,
            "vbo_upload_time_measurement": False,
            "leaf_io_time_measurement": False,
            "notes": (
                "整个 healpix_browser_qt 模块完全没有计时器代码. "
                "日志系统 (logger.h) 仅记录事件级别 (file_open, leaf_load, vbo_build), "
                "无任何耗时测量. 这是 P15-001 需要补齐的核心 trace 缺口."
            ),
        },
        "existing_trace_points": {
            "file_open": "browser_backend.cpp:219-220 (LOG_INFO, nside/nested/n_pix/filter)",
            "header_decompress": "browser_backend.cpp:61-62, 139-141 (LOG_INFO, nside/shift/n_pix)",
            "visible_leaf_query": "browser_backend.cpp:369-370 (LOG_DEBUG, candidate/return count)",
            "cpu_lookup_lod": "browser_backend.cpp:409-411 (LOG_DEBUG, fov/nside_ideal/nside_target)",
            "leaf_io_hiss": "browser_backend.cpp:445-447 (LOG_DEBUG, leaf/n_pix/nside)",
            "leaf_io_hcsd": "browser_backend.cpp:475-477 (LOG_DEBUG, leaf/n_pix/nside)",
            "ud_grade": "browser_backend.cpp:568-571 (LOG_DEBUG, input/output nside/n_pix)",
            "vbo_build_sphere_static": "gl_renderer.cpp:701, 797 (LOG_INFO, segments/vertices/indices)",
            "vbo_build_sphere_dynamic": "gl_renderer.cpp:836-839, 944-945 (LOG_INFO, fov/segments/vertices)",
            "vbo_build_hiss_polygon": "gl_renderer.cpp:1326, 1505-1508 (LOG_INFO, nside/n_pix/vertices)",
            "vbo_upload_texture": "gl_renderer.cpp:1048-1050 (LOG_DEBUG, leaf/n_pix/tex_id)",
            "draw_render_sphere": "gl_renderer.cpp:1314-1315 (LOG_DEBUG, cached_leaves/vertices)",
            "draw_render_hiss_polygon": "gl_renderer.cpp:1602-1604 (LOG_DEBUG, vertices/fov/ra/dec)",
            "draw_render_single_frame": "gl_renderer.cpp:1786 (LOG_DEBUG, 'render done')",
            "draw_render_grid": "gl_renderer.cpp:2060 (LOG_DEBUG, vertices)",
            "view_reset": "sphere_view.cpp:67-70 (LOG_INFO, fov/mode/forward/up)",
            "view_set_initial": "sphere_view.cpp:107-110 (LOG_INFO, center/size/fov)",
            "view_drag": "sphere_view.cpp:425-426 (LOG_DEBUG, dx/dy/ra/dec)",
        },
        "missing_trace_points_for_p15_001": [
            "wall_clock_file_open (打开文件总耗时)",
            "wall_clock_first_frame (从打开到首帧渲染完成)",
            "wall_clock_first_hires_view (从打开到首个高分辨率视图)",
            "frame_time_p50_p95_p99 (拖动/缩放 frame time 分位数)",
            "gui_max_blocking (GUI 线程最长阻塞)",
            "disk_throughput (磁盘吞吐 MB/s)",
            "cpu_usage (CPU 利用率)",
            "gpu_usage (GPU 利用率)",
            "peak_memory (峰值内存)",
            "leaf_request_count (叶请求数)",
            "cache_hit_rate (缓存命中率)",
            "vbo_upload_time (VBO 上传耗时)",
            "draw_time (draw 调用耗时)",
            "ud_grade_time (降采样耗时)",
            "leaf_io_time (leaf I/O 耗时)",
        ],
    }

    # --- 启动浏览器 (offscreen) 加载默认 HISS, 捕获事件日志 ---
    print(f"=== 测试 1: 加载默认 HISS 文件 ===")
    print(f"  文件: {DEFAULT_HISS.relative_to(REPO_ROOT)}")
    hiss_result = run_browser_headless(DEFAULT_HISS, timeout_sec=30.0)
    hiss_events = parse_browser_events(hiss_result["stderr"])
    print(f"  退出码: {hiss_result['exit_code']} (timed_out={hiss_result['timed_out']})")
    print(f"  wall_clock: {hiss_result['elapsed_sec']}s")
    print(f"  stderr 字节数: {hiss_result['stderr_bytes']}")
    print(f"  解析事件数: {hiss_events['total_events']} (hio: {hiss_events['total_hio_events']})")
    print(f"  trace 分类: {json.dumps(hiss_events['trace_categories'], ensure_ascii=False)}")
    print(f"  timings: {json.dumps(hiss_events['timings'], ensure_ascii=False, indent=2)}")

    # 写出原始日志
    hiss_stderr_path = RAW_LOGS_DIR / "browser_hiss_stderr.log"
    hiss_stderr_path.write_text(hiss_result["stderr"], encoding="utf-8")
    hiss_stdout_path = RAW_LOGS_DIR / "browser_hiss_stdout.log"
    hiss_stdout_path.write_text(hiss_result["stdout"], encoding="utf-8")

    # --- 启动浏览器 (offscreen) 加载 P07-001 HCSD, 捕获事件日志 ---
    print()
    print(f"=== 测试 2: 加载 P07-001 HCSD 文件 ===")
    print(f"  文件: {P07_HCSD.relative_to(REPO_ROOT)}")
    hcsd_result = run_browser_headless(P07_HCSD, timeout_sec=20.0)
    hcsd_events = parse_browser_events(hcsd_result["stderr"])
    print(f"  退出码: {hcsd_result['exit_code']} (timed_out={hcsd_result['timed_out']})")
    print(f"  wall_clock: {hcsd_result['elapsed_sec']}s")
    print(f"  stderr 字节数: {hcsd_result['stderr_bytes']}")
    print(f"  解析事件数: {hcsd_events['total_events']} (hio: {hcsd_events['total_hio_events']})")
    print(f"  trace 分类: {json.dumps(hcsd_events['trace_categories'], ensure_ascii=False)}")
    print(f"  timings: {json.dumps(hcsd_events['timings'], ensure_ascii=False, indent=2)}")

    hcsd_stderr_path = RAW_LOGS_DIR / "browser_hcsd_stderr.log"
    hcsd_stderr_path.write_text(hcsd_result["stderr"], encoding="utf-8")
    hcsd_stdout_path = RAW_LOGS_DIR / "browser_hcsd_stdout.log"
    hcsd_stdout_path.write_text(hcsd_result["stdout"], encoding="utf-8")

    # --- 装配 baseline_performance.json ---
    print()
    print("=== 装配 browser_performance_baseline.json ===")
    baseline = {
        "schema_version": "1.2",
        "task_id": "P09-003",
        "gate": "G9",
        "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
        "spec_source": "engineering_v1.2/docs/12_BROWSER_PERFORMANCE_BASELINE_SPEC.md",
        "environment": env_info,
        "browser_code_state": browser_code_state,
        "fixed_viewpoint": {
            "window_size": {"width": 1280, "height": 800},
            "default_view": {"ra_deg": 0.0, "dec_deg": 0.0, "fov_deg": 50.0},
            "auto_stretch": {
                "shadows_percentile": 0.5,
                "highlights_percentile": 99.5,
                "midtones": "normalized_median_clamped_0.01_0.99",
                "compression": 0.8,
            },
            "file_routing": {
                ".hiss": "set_initial_view_from_data(bbox)",
                ".hcsd": "reset_view()",
            },
        },
        "test_inputs": {
            "default_hiss": {
                "rel_path": str(DEFAULT_HISS.relative_to(REPO_ROOT)),
                "sha256": sha256_file(DEFAULT_HISS),
                "size_mb": round(DEFAULT_HISS.stat().st_size / (1024 * 1024), 3),
            },
            "p07_001_hcsd": {
                "rel_path": str(P07_HCSD.relative_to(REPO_ROOT)),
                "sha256": sha256_file(P07_HCSD),
                "size_mb": round(P07_HCSD.stat().st_size / (1024 * 1024), 3),
            },
        },
        "test_runs": {
            "hiss_offscreen": {
                "cmd": hiss_result["cmd"],
                "exit_code": hiss_result["exit_code"],
                "timed_out": hiss_result["timed_out"],
                "wall_clock_sec": hiss_result["elapsed_sec"],
                "stderr_bytes": hiss_result["stderr_bytes"],
                "stderr_log": str(hiss_stderr_path.relative_to(REPO_ROOT)),
                "total_events": hiss_events["total_events"],
                "total_hio_events": hiss_events["total_hio_events"],
                "trace_categories": hiss_events["trace_categories"],
                "timings": hiss_events["timings"],
                "all_events": hiss_events["events"],
                "all_hio_events": hiss_events["hio_events"],
                "interpretation": (
                    "wall_clock_sec 包含从子进程启动到 (超时终止 | 自然退出) 的总耗时. "
                    "timings 来自日志时间戳 (秒级精度), 可量化 leaf_index_duration 等关键阶段. "
                    "但 frame_time / draw_time / VBO_upload_time 仍无法测量 (需 P15-001 添加 ScopedTimer). "
                    "事件计数仅证明 trace 点被触发, 不代表耗时. "
                    "P15-001 必须补齐 timing 才能获得 frame time p50/p95/p99."
                ),
            },
            "hcsd_offscreen": {
                "cmd": hcsd_result["cmd"],
                "exit_code": hcsd_result["exit_code"],
                "timed_out": hcsd_result["timed_out"],
                "wall_clock_sec": hcsd_result["elapsed_sec"],
                "stderr_bytes": hcsd_result["stderr_bytes"],
                "stderr_log": str(hcsd_stderr_path.relative_to(REPO_ROOT)),
                "total_events": hcsd_events["total_events"],
                "total_hio_events": hcsd_events["total_hio_events"],
                "trace_categories": hcsd_events["trace_categories"],
                "timings": hcsd_events["timings"],
                "all_events": hcsd_events["events"],
                "all_hio_events": hcsd_events["hio_events"],
                "interpretation": (
                    "wall_clock_sec 包含从子进程启动到 (超时终止 | 自然退出) 的总耗时. "
                    "offscreen 模式不渲染实际像素, 渲染阶段耗时与真实 GPU 不同. "
                    "P15-001 必须使用真实 GPU + 显示器才能获得准确的 frame time. "
                    "本测试发现: HCSD 默认视角 (RA=0, Dec=0, FOV=50°) 不覆盖数据 (Galaxy Center 在 RA=272.8°), "
                    "导致所有 leaf 读取返回空 - 这是 HCSD 路由使用 reset_view() 而非 set_initial_view_from_data() 的设计缺陷."
                ),
            },
        },
        "baseline_summary": {
            "pre_modification_fact": (
                "浏览器在 v1.2 P09-003 时的状态: "
                "(1) 有完善的 event-level 日志 (file_open/leaf_load/vbo_build/draw); "
                "(2) 完全没有 timing instrumentation (无 QElapsedTimer/std::chrono); "
                "(3) 无 FPS / frame time / draw time / VBO upload time / leaf I/O time 测量; "
                "(4) main_window 无性能监控 UI (无 FPS 显示, 无 frame time); "
                "(5) BROWSER_LOG_FILE 仅在程序正常退出时写盘, 崩溃会丢失日志."
            ),
            "what_can_be_measured_now": [
                "wall_clock 启动到超时总耗时 (本基线已测)",
                "事件计数 (file_open/leaf_load/vbo_build/draw 各被触发多少次)",
                "stderr 实时输出 (本基线已捕获)",
            ],
            "what_cannot_be_measured_now": [
                "frame time p50/p95/p99",
                "GUI 最长阻塞",
                "磁盘吞吐",
                "CPU/GPU 利用率",
                "峰值内存 (浏览器进程级)",
                "缓存命中率 (无统计)",
                "各阶段耗时 (file_open/header/leaf_load/render 拆分)",
            ],
            "p15_001_prerequisites": [
                "在 logger.h 添加 RAII ScopedTimer (std::chrono::steady_clock)",
                "在 browser_backend.cpp 关键路径包裹计时器 (open_file/build_hiss_leaf_index/load_leaf/ud_grade)",
                "在 gl_renderer.cpp 关键路径包裹计时器 (build_sphere_mesh/render_sphere/render_hiss_polygon/VBO upload)",
                "在 main_window.cpp 添加 FPS / frame time 状态栏显示",
                "添加 cache hit/miss 计数器",
                "添加 leaf_request_count 统计",
                "BROWSER_LOG_FILE 改为定期 flush (避免崩溃丢失)",
            ],
        },
    }

    out_path = EVIDENCE_DIR / "browser_performance_baseline.json"
    out_path.write_text(json.dumps(baseline, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"  写出: {out_path.relative_to(REPO_ROOT)}")
    print()
    print(f"P09-003 T5 浏览器性能基线 - 完成")
    return 0


if __name__ == "__main__":
    sys.exit(main())
