#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P07-001 性能与峰值内存基线测量脚本

功能：
  1. 运行 orchestrator.exe stage1/stage2，后台采样内存（峰值）
  2. 捕获 stdout (JSONL) / stderr
  3. 解析 JSONL 获取各 stage 耗时
  4. 计算输出文件 SHA-256
  5. 支持取消测试（运行后发送 Ctrl+C 等价信号）
  6. 输出结构化 JSON 结果

用法：
  python perf_runner.py run-stage1 --frame <fits> --config <json> --output <hiss> --label <label>
  python perf_runner.py run-stage2 --frames <dir> --output <hcsd> --config <json> --label <label>
  python perf_runner.py cancel-test --frame <fits> --config <json> --output <hiss> --label <label>
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import threading
import time
from pathlib import Path


ORCH_EXE = "lib/orchestrator/cpp/orchestrator.exe"
PROJECT_ROOT = Path(__file__).resolve().parents[3]  # perf_runner.py 在 evidence/P07-001/
SAMPLE_INTERVAL_MS = 100  # 内存采样间隔（毫秒）


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def sample_memory(proc: subprocess.Popen, samples: list, stop_event: threading.Event):
    """后台线程：每 SAMPLE_INTERVAL_MS 采样一次进程 WorkingSet64"""
    import ctypes
    psapi = ctypes.WinDLL("psapi.dll")
    kernel32 = ctypes.WinDLL("kernel32.dll")
    PROCESS_QUERY_INFORMATION = 0x0400
    PROCESS_VM_READ = 0x0010

    while not stop_event.is_set() and proc.poll() is None:
        try:
            pid = proc.pid
            handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
            if handle:
                class PROCESS_MEMORY_COUNTERS(ctypes.Structure):
                    _fields_ = [
                        ("cb", ctypes.c_ulong),
                        ("PageFaultCount", ctypes.c_ulong),
                        ("PeakWorkingSetSize", ctypes.c_size_t),
                        ("WorkingSetSize", ctypes.c_size_t),
                        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                        ("PagefileUsage", ctypes.c_size_t),
                        ("PeakPagefileUsage", ctypes.c_size_t),
                    ]
                pmc = PROCESS_MEMORY_COUNTERS()
                pmc.cb = ctypes.sizeof(pmc)
                if psapi.GetProcessMemoryInfo(handle, ctypes.byref(pmc), pmc.cb):
                    ws_mb = pmc.WorkingSetSize / (1024 * 1024)
                    peak_ws_mb = pmc.PeakWorkingSetSize / (1024 * 1024)
                    pf_mb = pmc.PagefileUsage / (1024 * 1024)
                    peak_pf_mb = pmc.PeakPagefileUsage / (1024 * 1024)
                    samples.append({
                        "t": round(time.time(), 3),
                        "ws_mb": round(ws_mb, 2),
                        "peak_ws_mb": round(peak_ws_mb, 2),
                        "pf_mb": round(pf_mb, 2),
                        "peak_pf_mb": round(peak_pf_mb, 2),
                    })
                kernel32.CloseHandle(handle)
        except Exception:
            pass
        time.sleep(SAMPLE_INTERVAL_MS / 1000.0)


def run_with_memory(exe: str, args: list, cwd: str, timeout: int,
                    stdout_file: str, stderr_file: str,
                    env: dict) -> dict:
    """运行子进程并后台采样内存"""
    t0 = time.time()
    samples: list = []
    stop_event = threading.Event()

    # 用文件捕获 stdout/stderr，避免管道死锁
    Path(stdout_file).parent.mkdir(parents=True, exist_ok=True)
    Path(stderr_file).parent.mkdir(parents=True, exist_ok=True)

    with open(stdout_file, "w", encoding="utf-8", errors="replace") as fout, \
         open(stderr_file, "w", encoding="utf-8", errors="replace") as ferr:
        proc = subprocess.Popen(
            [exe] + args, cwd=cwd, stdout=fout, stderr=ferr, env=env,
            creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
        )
        sampler = threading.Thread(target=sample_memory, args=(proc, samples, stop_event), daemon=True)
        sampler.start()

        try:
            rc = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            # 超时强制终止
            proc.kill()
            proc.wait()
            rc = -1
        finally:
            stop_event.set()
            sampler.join(timeout=2.0)

    elapsed = round(time.time() - t0, 3)

    # 计算峰值内存
    peak_ws = max([s["ws_mb"] for s in samples], default=0.0)
    peak_pf = max([s["pf_mb"] for s in samples], default=0.0)
    os_peak_ws = max([s["peak_ws_mb"] for s in samples], default=0.0)
    os_peak_pf = max([s["peak_pf_mb"] for s in samples], default=0.0)

    return {
        "exit_code": rc,
        "elapsed_sec": elapsed,
        "peak_working_set_mb": round(peak_ws, 2),
        "peak_pagefile_mb": round(peak_pf, 2),
        "os_peak_working_set_mb": round(os_peak_ws, 2),
        "os_peak_pagefile_mb": round(os_peak_pf, 2),
        "sample_count": len(samples),
        "samples": samples,
    }


def parse_jsonl_timings(stdout_file: str) -> list:
    """解析 orchestrator stdout JSONL，提取 stage timings"""
    timings = []
    if not os.path.exists(stdout_file):
        return timings
    with open(stdout_file, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                evt = json.loads(line)
            except json.JSONDecodeError:
                continue
            # 提取 timings 字段
            if "timings" in evt:
                timings.extend(evt["timings"])
            # 也记录事件类型
            evt_type = evt.get("event", evt.get("type", ""))
            if evt_type in ("stage_complete", "stage_completed") or "stage" in evt:
                t = evt.get("timings", [])
                if t:
                    timings.extend(t)
    return timings


def parse_stderr_stages(stderr_file: str) -> dict:
    """从 stderr 日志提取各 stage 耗时（DEBUG 级别）"""
    stages = {}
    if not os.path.exists(stderr_file):
        return stages
    with open(stderr_file, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            # 查找类似 "stage X completed in Y.YYYs" 的行
            line = line.strip()
            if "completed in" in line.lower() or "duration" in line.lower():
                # 简单记录，后续手动解析
                pass
    return stages


def make_env() -> dict:
    """构造环境变量（注入 DLL 路径）"""
    env = os.environ.copy()
    arts = str(PROJECT_ROOT / "build" / "artifacts")
    env["PATH"] = arts + os.pathsep + r"C:\msys64\mingw64\bin" + os.pathsep + env.get("PATH", "")
    return env


def cmd_run_stage1(args):
    """运行 stage1 并采样内存"""
    frame = args.frame
    config = args.config
    output = args.output
    label = args.label
    timeout = args.timeout

    stdout_file = f"engineering/evidence/P07-001/logs/{label}_stdout.jsonl"
    stderr_file = f"engineering/evidence/P07-001/logs/{label}_stderr.log"
    mem_file = f"engineering/evidence/P07-001/logs/{label}_memory.json"

    exe = str(PROJECT_ROOT / ORCH_EXE)
    cmd_args = ["stage1", "--frame", frame, "--output", output, "--config", config]
    env = make_env()

    print(f"[perf] Running stage1: {label}")
    print(f"[perf] exe: {exe}")
    print(f"[perf] args: {cmd_args}")
    print(f"[perf] timeout: {timeout}s")

    result = run_with_memory(exe, cmd_args, str(PROJECT_ROOT), timeout,
                             stdout_file, stderr_file, env)
    result["label"] = label
    result["frame"] = frame
    result["config"] = config
    result["output"] = output

    # 解析 timings
    timings = parse_jsonl_timings(stdout_file)
    result["timings"] = timings

    # 计算 HISS SHA-256
    if os.path.exists(output) and result["exit_code"] == 0:
        result["output_sha256"] = sha256_file(output)
        result["output_size"] = os.path.getsize(output)
    else:
        result["output_sha256"] = ""
        result["output_size"] = 0

    # 保存内存采样
    with open(mem_file, "w", encoding="utf-8") as f:
        json.dump({"samples": result["samples"], "peak_ws_mb": result["peak_working_set_mb"],
                   "peak_pf_mb": result["peak_pagefile_mb"], "label": label}, f, indent=2)

    # 移除 samples 后输出（避免结果太大）
    summary = {k: v for k, v in result.items() if k != "samples"}
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return result


def cmd_run_stage2(args):
    """运行 stage2 并采样内存"""
    frames_dir = args.frames
    output = args.output
    config = args.config
    label = args.label
    timeout = args.timeout

    stdout_file = f"engineering/evidence/P07-001/logs/{label}_stdout.jsonl"
    stderr_file = f"engineering/evidence/P07-001/logs/{label}_stderr.log"
    mem_file = f"engineering/evidence/P07-001/logs/{label}_memory.json"

    exe = str(PROJECT_ROOT / ORCH_EXE)
    cmd_args = ["stage2", "--frames", frames_dir, "--output", output]
    if config:
        cmd_args += ["--config", config]
    env = make_env()

    print(f"[perf] Running stage2: {label}")

    result = run_with_memory(exe, cmd_args, str(PROJECT_ROOT), timeout,
                             stdout_file, stderr_file, env)
    result["label"] = label
    result["frames_dir"] = frames_dir
    result["output"] = output

    timings = parse_jsonl_timings(stdout_file)
    result["timings"] = timings

    if os.path.exists(output) and result["exit_code"] == 0:
        result["output_sha256"] = sha256_file(output)
        result["output_size"] = os.path.getsize(output)
    else:
        result["output_sha256"] = ""
        result["output_size"] = 0

    with open(mem_file, "w", encoding="utf-8") as f:
        json.dump({"samples": result["samples"], "peak_ws_mb": result["peak_working_set_mb"],
                   "peak_pf_mb": result["peak_pagefile_mb"], "label": label}, f, indent=2)

    summary = {k: v for k, v in result.items() if k != "samples"}
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return result


def cmd_cancel_test(args):
    """取消测试：启动 stage1，运行几秒后发送 Ctrl+C（SIGINT）"""
    frame = args.frame
    config = args.config
    output = args.output
    label = args.label
    cancel_after = args.cancel_after

    stdout_file = f"engineering/evidence/P07-001/logs/{label}_stdout.jsonl"
    stderr_file = f"engineering/evidence/P07-001/logs/{label}_stderr.log"
    mem_file = f"engineering/evidence/P07-001/logs/{label}_memory.json"

    exe = str(PROJECT_ROOT / ORCH_EXE)
    cmd_args = ["stage1", "--frame", frame, "--output", output, "--config", config]
    env = make_env()

    Path(stdout_file).parent.mkdir(parents=True, exist_ok=True)
    Path(stderr_file).parent.mkdir(parents=True, exist_ok=True)

    print(f"[perf] Cancel test: {label}, cancel after {cancel_after}s")

    import ctypes
    kernel32 = ctypes.WinDLL("kernel32.dll")

    t0 = time.time()
    samples: list = []
    stop_event = threading.Event()

    with open(stdout_file, "w", encoding="utf-8", errors="replace") as fout, \
         open(stderr_file, "w", encoding="utf-8", errors="replace") as ferr:
        proc = subprocess.Popen(
            [exe] + cmd_args, cwd=str(PROJECT_ROOT), stdout=fout, stderr=ferr, env=env,
            creationflags=subprocess.CREATE_NEW_PROCESS_GROUP,
        )
        sampler = threading.Thread(target=sample_memory, args=(proc, samples, stop_event), daemon=True)
        sampler.start()

        # 等待 cancel_after 秒
        time.sleep(cancel_after)

        # 发送 Ctrl+C（CTRL_BREAK_EVENT）到进程组
        print(f"[perf] Sending Ctrl+C to PID {proc.pid}")
        CTRL_BREAK_EVENT = 1
        kernel32.GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, proc.pid)

        try:
            rc = proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            rc = -1
        finally:
            stop_event.set()
            sampler.join(timeout=2.0)

    elapsed = round(time.time() - t0, 3)
    peak_ws = max([s["ws_mb"] for s in samples], default=0.0)
    sample_count = len(samples)

    # 检查进程是否已退出
    process_alive = proc.poll() is not None

    # 检查 partial 输出
    partial_exists = os.path.exists(output)
    partial_size = os.path.getsize(output) if partial_exists else 0

    # 等待内存释放
    time.sleep(2)
    # 检查残留进程
    residual = False
    try:
        # 通过 tasklist 检查
        r = subprocess.run(["tasklist", "/FI", f"PID eq {proc.pid}"],
                           capture_output=True, text=True, timeout=10)
        if str(proc.pid) in r.stdout and "orchestrator" in r.stdout.lower():
            residual = True
    except Exception:
        pass

    result = {
        "label": label,
        "exit_code": rc,
        "elapsed_sec": elapsed,
        "cancel_after_sec": cancel_after,
        "process_exited": process_alive,
        "residual_process": residual,
        "partial_output_exists": partial_exists,
        "partial_output_size": partial_size,
        "peak_working_set_mb": round(peak_ws, 2),
        "sample_count": sample_count,
    }

    with open(mem_file, "w", encoding="utf-8") as f:
        json.dump({"samples": samples, "peak_ws_mb": peak_ws, "label": label}, f, indent=2)

    print(json.dumps(result, ensure_ascii=False, indent=2))
    return result


def main():
    ap = argparse.ArgumentParser(description="P07-001 性能测量脚本")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p1 = sub.add_parser("run-stage1")
    p1.add_argument("--frame", required=True)
    p1.add_argument("--config", required=True)
    p1.add_argument("--output", required=True)
    p1.add_argument("--label", required=True)
    p1.add_argument("--timeout", type=int, default=120)
    p1.set_defaults(func=cmd_run_stage1)

    p2 = sub.add_parser("run-stage2")
    p2.add_argument("--frames", required=True)
    p2.add_argument("--output", required=True)
    p2.add_argument("--config", default="")
    p2.add_argument("--label", required=True)
    p2.add_argument("--timeout", type=int, default=180)
    p2.set_defaults(func=cmd_run_stage2)

    p3 = sub.add_parser("cancel-test")
    p3.add_argument("--frame", required=True)
    p3.add_argument("--config", required=True)
    p3.add_argument("--output", required=True)
    p3.add_argument("--label", required=True)
    p3.add_argument("--cancel-after", type=int, default=10)
    p3.set_defaults(func=cmd_cancel_test)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
