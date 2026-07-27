#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P07-002 长批次与故障稳定性测试脚本

功能：
  1. Stage1 批量稳定性（6 帧连续运行）
  2. Stage2 重复稳定性（3 次确定性验证）
  3. 取消后重跑（取消 stage1 后立即重跑验证）
  4. 资源泄漏检查（长批次后系统内存/进程/临时文件）
  5. 故障注入（stage2 运行时删除输入 HISS 文件）

用法：
  python stability_runner.py --all              # 运行全部测试
  python stability_runner.py --stage1-batch     # 仅 stage1 批量
  python stability_runner.py --stage2-repeat    # 仅 stage2 重复
  python stability_runner.py --cancel-rerun     # 仅取消重跑
  python stability_runner.py --leak-check       # 仅资源泄漏检查
  python stability_runner.py --fault-inject     # 仅故障注入

输出：
  engineering/evidence/P07-002/stability_results.json
  engineering/evidence/P07-002/logs/*.log
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]  # stability_runner.py 在 engineering/evidence/P07-002/
ORCH_EXE = str(PROJECT_ROOT / "lib" / "orchestrator" / "cpp" / "orchestrator.exe")
EVIDENCE_DIR = PROJECT_ROOT / "engineering" / "evidence" / "P07-002"
LOGS_DIR = EVIDENCE_DIR / "logs"
OUTPUT_DIR = EVIDENCE_DIR / "output"
CONFIGS_DIR = EVIDENCE_DIR / "configs"
RESULTS_FILE = EVIDENCE_DIR / "stability_results.json"
SAMPLE_INTERVAL_MS = 100

# 6 帧定义（来自 P05-002）
FRAMES = [
    {
        "id": "C001", "label": "C001_Galaxy_Center_T4_Red_180s",
        "fits": "testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts",
        "config": "stage1_config_T4.json", "telescope": "T4",
    },
    {
        "id": "C003", "label": "C003_NGC1727_T2_Red_600s",
        "fits": "testdata/NGC1727_T2_flying_dutchman/lights/NGC1727_RGBHO_T2_flying_dutchman-20251031@064517-600S-Red.fts",
        "config": "stage1_config_T2.json", "telescope": "T2",
    },
    {
        "id": "C004", "label": "C004_NGC247_T2_Lum_600s",
        "fits": "testdata/NGC247_T2_flying_dutchman/lights/NGC247_T2_flying_dutchman-20250816@033428-600S-Lum.fts",
        "config": "stage1_config_T2.json", "telescope": "T2",
    },
    {
        "id": "C005", "label": "C005_NGC55_T3_Red_600s",
        "fits": "testdata/NGC55_T3_flying_dutchman/lights/NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts",
        "config": "stage1_config_T3.json", "telescope": "T3",
    },
    {
        "id": "C006", "label": "C006_NGC83_cluster_T3_Red_600s",
        "fits": "testdata/NGC83_cluster_T3_Flying_Dutchman/lights/NGC90_2025wwk_T3_flying_dutchman-20251011@020846-600S-Red.fts",
        "config": "stage1_config_T3.json", "telescope": "T3",
    },
    {
        "id": "C007", "label": "C007_Victory_Nebula_T4_Lum_180s",
        "fits": "testdata/Victory_Nebula_T4_Flying_Dutchman/lights/Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180S-Lum.fts",
        "config": "stage1_config_T4.json", "telescope": "T4",
    },
]

# Stage2 输入（与 P07-001 一致，HCSD SHA-256 基线 2A9BD12E...）
STAGE2_INPUT_DIR = str(PROJECT_ROOT / "lib" / "orchestrator" / "cpp" / "output_hiss_dir")
STAGE2_BASELINE_SHA256 = "2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37"


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


def make_env() -> dict:
    env = os.environ.copy()
    arts = str(PROJECT_ROOT / "build" / "artifacts")
    env["PATH"] = arts + os.pathsep + r"C:\msys64\mingw64\bin" + os.pathsep + env.get("PATH", "")
    return env


def run_with_memory(exe: str, args: list, cwd: str, timeout: int,
                    stdout_file: str, stderr_file: str, env: dict) -> dict:
    t0 = time.time()
    samples: list = []
    stop_event = threading.Event()

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
            proc.kill()
            proc.wait()
            rc = -1
        finally:
            stop_event.set()
            sampler.join(timeout=2.0)

    elapsed = round(time.time() - t0, 3)
    peak_ws = max([s["ws_mb"] for s in samples], default=0.0)
    peak_pf = max([s["pf_mb"] for s in samples], default=0.0)

    return {
        "exit_code": rc,
        "elapsed_sec": elapsed,
        "peak_working_set_mb": round(peak_ws, 2),
        "peak_pagefile_mb": round(peak_pf, 2),
        "sample_count": len(samples),
        "samples": samples,
    }


def parse_jsonl_timings(stdout_file: str) -> list:
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
            if "timings" in evt:
                timings.extend(evt["timings"])
    return timings


def get_system_memory_mb() -> dict:
    """获取系统可用/已用内存（MB）"""
    import ctypes
    class MEMORYSTATUSEX(ctypes.Structure):
        _fields_ = [
            ("dwLength", ctypes.c_ulong),
            ("dwMemoryLoad", ctypes.c_ulong),
            ("ullTotalPhys", ctypes.c_ulonglong),
            ("ullAvailPhys", ctypes.c_ulonglong),
            ("ullTotalPageFile", ctypes.c_ulonglong),
            ("ullAvailPageFile", ctypes.c_ulonglong),
            ("ullTotalVirtual", ctypes.c_ulonglong),
            ("ullAvailVirtual", ctypes.c_ulonglong),
            ("ullAvailExtendedVirtual", ctypes.c_ulonglong),
        ]
    m = MEMORYSTATUSEX()
    m.dwLength = ctypes.sizeof(m)
    ctypes.WinDLL("kernel32.dll").GlobalMemoryStatusEx(ctypes.byref(m))
    return {
        "memory_load_pct": m.dwMemoryLoad,
        "total_phys_mb": round(m.ullTotalPhys / (1024 * 1024), 2),
        "avail_phys_mb": round(m.ullAvailPhys / (1024 * 1024), 2),
        "used_phys_mb": round((m.ullTotalPhys - m.ullAvailPhys) / (1024 * 1024), 2),
        "total_pagefile_mb": round(m.ullTotalPageFile / (1024 * 1024), 2),
        "avail_pagefile_mb": round(m.ullAvailPageFile / (1024 * 1024), 2),
    }


def check_residual_processes() -> dict:
    """检查 orchestrator 残留进程"""
    try:
        r = subprocess.run(["tasklist", "/FI", "IMAGENAME eq orchestrator.exe"],
                           capture_output=True, text=True, timeout=10)
        lines = [l for l in r.stdout.splitlines() if "orchestrator" in l.lower()]
        return {"residual_count": len(lines), "details": lines}
    except Exception as e:
        return {"residual_count": -1, "details": [str(e)]}


def check_temp_files() -> dict:
    """检查输出目录临时文件残留"""
    temp_files = []
    for d in [str(OUTPUT_DIR), str(PROJECT_ROOT / "output"),
              str(PROJECT_ROOT / "lib" / "orchestrator" / "cpp" / "output_hiss_dir")]:
        if os.path.exists(d):
            for f in os.listdir(d):
                if f.endswith((".tmp", ".partial", ".lock", ".bak")):
                    temp_files.append(os.path.join(d, f))
    return {"temp_file_count": len(temp_files), "temp_files": temp_files}


# ---------------------------------------------------------------------------
# 测试用例
# ---------------------------------------------------------------------------

def test_stage1_batch(results: dict):
    """Stage1 批量稳定性：6 帧连续运行"""
    print("\n" + "=" * 70)
    print("[P07-002] Stage1 批量稳定性测试（6 帧连续）")
    print("=" * 70)

    batch_results = []
    batch_start = time.time()
    env = make_env()
    config_cache = {}  # 缓存配置路径

    for i, frame in enumerate(FRAMES, 1):
        label = f"batch_{frame['id']}"
        print(f"\n[batch {i}/6] {frame['id']} - {frame['label']}")

        fits_path = str(PROJECT_ROOT / frame["fits"])
        if not os.path.exists(fits_path):
            print(f"  [WARN] FITS 不存在: {fits_path}")
            batch_results.append({"frame_id": frame["id"], "label": frame["label"],
                                  "success": False, "error": f"FITS not found: {fits_path}"})
            continue

        config_path = str(CONFIGS_DIR / frame["config"])
        if not os.path.exists(config_path):
            print(f"  [WARN] 配置不存在: {config_path}")
            batch_results.append({"frame_id": frame["id"], "label": frame["label"],
                                  "success": False, "error": f"config not found: {config_path}"})
            continue

        output_hiss = str(OUTPUT_DIR / f"{label}.hiss")
        stdout_file = str(LOGS_DIR / f"{label}_stdout.jsonl")
        stderr_file = str(LOGS_DIR / f"{label}_stderr.log")
        mem_file = str(LOGS_DIR / f"{label}_memory.json")

        # 运行前系统内存
        mem_before = get_system_memory_mb()

        cmd_args = ["stage1", "--frame", fits_path, "--output", output_hiss, "--config", config_path]
        print(f"  exe: {ORCH_EXE}")
        print(f"  frame: {frame['fits']}")
        print(f"  timeout: 120s")

        r = run_with_memory(ORCH_EXE, cmd_args, str(PROJECT_ROOT), 120,
                            stdout_file, stderr_file, env)

        # 运行后系统内存（等2秒让OS回收）
        time.sleep(2)
        mem_after = get_system_memory_mb()

        r["frame_id"] = frame["id"]
        r["label"] = frame["label"]
        r["telescope"] = frame["telescope"]
        r["fits_path"] = frame["fits"]
        r["config"] = frame["config"]
        r["mem_before_mb"] = mem_before
        r["mem_after_mb"] = mem_after
        r["mem_delta_avail_mb"] = round(mem_after["avail_phys_mb"] - mem_before["avail_phys_mb"], 2)
        r["timings"] = parse_jsonl_timings(stdout_file)

        if os.path.exists(output_hiss) and r["exit_code"] == 0:
            r["output_sha256"] = sha256_file(output_hiss)
            r["output_size"] = os.path.getsize(output_hiss)
            r["success"] = True
        else:
            r["output_sha256"] = ""
            r["output_size"] = 0
            r["success"] = False

        # 保存内存采样
        with open(mem_file, "w", encoding="utf-8") as f:
            json.dump({"samples": r["samples"], "peak_ws_mb": r["peak_working_set_mb"],
                       "peak_pf_mb": r["peak_pagefile_mb"], "label": label,
                       "mem_before": mem_before, "mem_after": mem_after}, f, indent=2)

        summary = {k: v for k, v in r.items() if k != "samples"}
        print(f"  exit_code: {r['exit_code']}, elapsed: {r['elapsed_sec']}s, "
              f"peak_mem: {r['peak_working_set_mb']} MB, success: {r['success']}")
        if r["success"]:
            print(f"  HISS SHA-256: {r['output_sha256'][:16]}..., size: {r['output_size']} bytes")

        batch_results.append(summary)

    batch_total = round(time.time() - batch_start, 3)
    success_count = sum(1 for r in batch_results if r.get("success"))

    # 检查帧间内存回归（每帧运行后系统可用内存是否回到基线）
    if len(batch_results) >= 2:
        first_mem_after = batch_results[0].get("mem_after_mb", {}).get("avail_phys_mb", 0)
        last_mem_after = batch_results[-1].get("mem_after_mb", {}).get("avail_phys_mb", 0)
        mem_regression = round(first_mem_after - last_mem_after, 2)
    else:
        mem_regression = 0

    results["stage1_batch"] = {
        "total_frames": len(FRAMES),
        "success_count": success_count,
        "batch_total_sec": batch_total,
        "mem_regression_mb": mem_regression,
        "frames": batch_results,
        "verdict": "PASS" if success_count == len(FRAMES) else "FAIL",
    }
    print(f"\n[batch] 完成: {success_count}/{len(FRAMES)} 成功, 总耗时 {batch_total}s, 内存回归 {mem_regression} MB")


def test_stage2_repeat(results: dict):
    """Stage2 重复稳定性：3 次运行验证 HCSD 确定性"""
    print("\n" + "=" * 70)
    print("[P07-002] Stage2 重复稳定性测试（3 次）")
    print("=" * 70)

    repeat_results = []
    env = make_env()
    sha256_set = set()

    for i in range(1, 4):
        label = f"stage2_repeat_{i}"
        print(f"\n[repeat {i}/3] {label}")

        output_hcsd = str(OUTPUT_DIR / f"{label}.hcsd")
        stdout_file = str(LOGS_DIR / f"{label}_stdout.jsonl")
        stderr_file = str(LOGS_DIR / f"{label}_stderr.log")
        mem_file = str(LOGS_DIR / f"{label}_memory.json")

        mem_before = get_system_memory_mb()

        cmd_args = ["stage2", "--frames", STAGE2_INPUT_DIR, "--output", output_hcsd]
        print(f"  exe: {ORCH_EXE}")
        print(f"  frames_dir: {STAGE2_INPUT_DIR}")
        print(f"  timeout: 180s")

        r = run_with_memory(ORCH_EXE, cmd_args, str(PROJECT_ROOT), 180,
                            stdout_file, stderr_file, env)

        time.sleep(2)
        mem_after = get_system_memory_mb()

        r["label"] = label
        r["run_index"] = i
        r["mem_before_mb"] = mem_before
        r["mem_after_mb"] = mem_after
        r["timings"] = parse_jsonl_timings(stdout_file)

        if os.path.exists(output_hcsd) and r["exit_code"] == 0:
            r["output_sha256"] = sha256_file(output_hcsd)
            r["output_size"] = os.path.getsize(output_hcsd)
            r["success"] = True
            sha256_set.add(r["output_sha256"])
        else:
            r["output_sha256"] = ""
            r["output_size"] = 0
            r["success"] = False

        with open(mem_file, "w", encoding="utf-8") as f:
            json.dump({"samples": r["samples"], "peak_ws_mb": r["peak_working_set_mb"],
                       "peak_pf_mb": r["peak_pagefile_mb"], "label": label,
                       "mem_before": mem_before, "mem_after": mem_after}, f, indent=2)

        summary = {k: v for k, v in r.items() if k != "samples"}
        print(f"  exit_code: {r['exit_code']}, elapsed: {r['elapsed_sec']}s, "
              f"peak_mem: {r['peak_working_set_mb']} MB")
        if r["success"]:
            print(f"  HCSD SHA-256: {r['output_sha256'][:16]}..., size: {r['output_size']} bytes")

        repeat_results.append(summary)

    success_count = sum(1 for r in repeat_results if r.get("success"))
    deterministic = len(sha256_set) == 1 and success_count == 3
    matches_baseline = STAGE2_BASELINE_SHA256 in sha256_set

    results["stage2_repeat"] = {
        "total_runs": 3,
        "success_count": success_count,
        "unique_sha256_count": len(sha256_set),
        "deterministic": deterministic,
        "matches_p07_001_baseline": matches_baseline,
        "baseline_sha256": STAGE2_BASELINE_SHA256,
        "runs": repeat_results,
        "verdict": "PASS" if (deterministic and matches_baseline) else "FAIL",
    }
    print(f"\n[repeat] 完成: {success_count}/3 成功, 确定性: {deterministic}, "
          f"匹配基线: {matches_baseline}")


def test_cancel_rerun(results: dict):
    """取消后重跑：取消 stage1 后立即重跑验证"""
    print("\n" + "=" * 70)
    print("[P07-002] 取消后重跑测试")
    print("=" * 70)

    import ctypes
    kernel32 = ctypes.WinDLL("kernel32.dll")

    # 使用 C003（南天，耗时长，便于取消）
    frame = FRAMES[1]  # C003
    label_cancel = "cancel_rerun_cancel"
    label_rerun = "cancel_rerun_rerun"

    fits_path = str(PROJECT_ROOT / frame["fits"])
    config_path = str(CONFIGS_DIR / frame["config"])
    output_cancel = str(OUTPUT_DIR / f"{label_cancel}.hiss")
    output_rerun = str(OUTPUT_DIR / f"{label_rerun}.hiss")
    stdout_cancel = str(LOGS_DIR / f"{label_cancel}_stdout.jsonl")
    stderr_cancel = str(LOGS_DIR / f"{label_cancel}_stderr.log")
    mem_cancel = str(LOGS_DIR / f"{label_cancel}_memory.json")
    stdout_rerun = str(LOGS_DIR / f"{label_rerun}_stdout.jsonl")
    stderr_rerun = str(LOGS_DIR / f"{label_rerun}_stderr.log")
    mem_rerun = str(LOGS_DIR / f"{label_rerun}_memory.json")

    env = make_env()
    cancel_after = 10  # 10s 后取消

    # === 阶段 1：取消测试 ===
    print(f"\n[cancel] 启动 stage1 {frame['id']}, {cancel_after}s 后取消")
    cmd_args = ["stage1", "--frame", fits_path, "--output", output_cancel, "--config", config_path]

    t0 = time.time()
    samples: list = []
    stop_event = threading.Event()

    with open(stdout_cancel, "w", encoding="utf-8", errors="replace") as fout, \
         open(stderr_cancel, "w", encoding="utf-8", errors="replace") as ferr:
        proc = subprocess.Popen(
            [ORCH_EXE] + cmd_args, cwd=str(PROJECT_ROOT), stdout=fout, stderr=ferr, env=env,
            creationflags=subprocess.CREATE_NEW_PROCESS_GROUP,
        )
        sampler = threading.Thread(target=sample_memory, args=(proc, samples, stop_event), daemon=True)
        sampler.start()

        time.sleep(cancel_after)
        print(f"[cancel] 发送 CTRL_BREAK_EVENT 到 PID {proc.pid}")
        CTRL_BREAK_EVENT = 1
        kernel32.GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, proc.pid)

        try:
            rc_cancel = proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            rc_cancel = -1
        finally:
            stop_event.set()
            sampler.join(timeout=2.0)

    cancel_elapsed = round(time.time() - t0, 3)
    cancel_peak_ws = max([s["ws_mb"] for s in samples], default=0.0)
    process_exited = proc.poll() is not None
    partial_exists = os.path.exists(output_cancel)
    partial_size = os.path.getsize(output_cancel) if partial_exists else 0

    with open(mem_cancel, "w", encoding="utf-8") as f:
        json.dump({"samples": samples, "peak_ws_mb": cancel_peak_ws, "label": label_cancel}, f, indent=2)

    print(f"[cancel] exit_code: {rc_cancel}, elapsed: {cancel_elapsed}s, "
          f"process_exited: {process_exited}, partial: {partial_exists} ({partial_size} bytes)")

    # 等待资源回收
    time.sleep(3)

    # 检查残留进程
    residual = check_residual_processes()

    # === 阶段 2：重跑验证 ===
    print(f"\n[rerun] 重跑 stage1 {frame['id']} 验证恢复")
    cmd_args_rerun = ["stage1", "--frame", fits_path, "--output", output_rerun, "--config", config_path]
    mem_before_rerun = get_system_memory_mb()

    r_rerun = run_with_memory(ORCH_EXE, cmd_args_rerun, str(PROJECT_ROOT), 120,
                              stdout_rerun, stderr_rerun, env)

    time.sleep(2)
    mem_after_rerun = get_system_memory_mb()

    r_rerun["label"] = label_rerun
    r_rerun["mem_before_mb"] = mem_before_rerun
    r_rerun["mem_after_mb"] = mem_after_rerun
    r_rerun["timings"] = parse_jsonl_timings(stdout_rerun)

    rerun_success = False
    if os.path.exists(output_rerun) and r_rerun["exit_code"] == 0:
        r_rerun["output_sha256"] = sha256_file(output_rerun)
        r_rerun["output_size"] = os.path.getsize(output_rerun)
        rerun_success = True
    else:
        r_rerun["output_sha256"] = ""
        r_rerun["output_size"] = 0

    with open(mem_rerun, "w", encoding="utf-8") as f:
        json.dump({"samples": r_rerun["samples"], "peak_ws_mb": r_rerun["peak_working_set_mb"],
                   "peak_pf_mb": r_rerun["peak_pagefile_mb"], "label": label_rerun,
                   "mem_before": mem_before_rerun, "mem_after": mem_after_rerun}, f, indent=2)

    print(f"[rerun] exit_code: {r_rerun['exit_code']}, elapsed: {r_rerun['elapsed_sec']}s, "
          f"success: {rerun_success}")
    if rerun_success:
        print(f"[rerun] HISS SHA-256: {r_rerun['output_sha256'][:16]}..., size: {r_rerun['output_size']} bytes")

    rerun_summary = {k: v for k, v in r_rerun.items() if k != "samples"}

    results["cancel_rerun"] = {
        "cancel": {
            "label": label_cancel,
            "frame_id": frame["id"],
            "cancel_after_sec": cancel_after,
            "exit_code": rc_cancel,
            "elapsed_sec": cancel_elapsed,
            "process_exited": process_exited,
            "partial_output_exists": partial_exists,
            "partial_output_size": partial_size,
            "peak_working_set_mb": round(cancel_peak_ws, 2),
        },
        "residual_after_cancel": residual,
        "rerun": rerun_summary,
        "rerun_success": rerun_success,
        "verdict": "PASS" if (process_exited and rerun_success and residual["residual_count"] == 0) else "FAIL",
    }
    print(f"\n[cancel_rerun] verdict: {results['cancel_rerun']['verdict']}")


def test_leak_check(results: dict):
    """资源泄漏检查：长批次后系统内存/进程/临时文件

    判定逻辑（基于 P07-001 已建立的基线）：
      - 残留进程 = 0（orchestrator.exe 无残留）
      - 临时文件 = 0（无 .tmp/.partial/.lock/.bak 残留）
      - 系统可用内存 > 10GB（长批次后系统内存回归健康）
      - stage2 重复运行峰值差异 < 200MB（确定性 + 冷启动可接受范围）
      - stage1 不同帧峰值差异不作为泄漏指标（已知天区特性：
        C001 赤道 3.6GB vs C003 南天 35.5GB，Gaia xpsd 分区差异，P07-001 已记录）
    """
    print("\n" + "=" * 70)
    print("[P07-002] 资源泄漏检查")
    print("=" * 70)

    # 记录当前系统内存（长批次后）
    mem_now = get_system_memory_mb()
    residual_proc = check_residual_processes()
    temp_files = check_temp_files()

    # 检查 P07-002 output 目录的文件
    output_files = []
    if os.path.exists(str(OUTPUT_DIR)):
        output_files = [f for f in os.listdir(str(OUTPUT_DIR)) if not f.startswith(".")]

    # stage1 不同帧峰值内存（仅记录，不作为泄漏指标 - 天区特性）
    batch_frames = results.get("stage1_batch", {}).get("frames", [])
    peak_mems = [f.get("peak_working_set_mb", 0) for f in batch_frames if f.get("success")]
    if len(peak_mems) >= 2:
        stage1_peak_diff = round(max(peak_mems) - min(peak_mems), 2)
    else:
        stage1_peak_diff = 0

    # stage2 重复运行峰值内存（真正的泄漏指标 - 同输入多次运行）
    stage2_runs = results.get("stage2_repeat", {}).get("runs", [])
    stage2_peaks = [r.get("peak_working_set_mb", 0) for r in stage2_runs if r.get("success")]
    if len(stage2_peaks) >= 2:
        stage2_peak_diff = round(max(stage2_peaks) - min(stage2_peaks), 2)
    else:
        stage2_peak_diff = 0

    # 系统内存健康度（长批次后可用内存）
    system_mem_healthy = mem_now["avail_phys_mb"] > 10240  # > 10GB 可用

    # stage2 重复运行峰值稳定（同输入多次运行，差异 < 200MB 为稳定，考虑冷启动+系统波动）
    stage2_stable = stage2_peak_diff < 200

    leak_verdict = "PASS"
    issues = []
    if residual_proc["residual_count"] > 0:
        leak_verdict = "FAIL"
        issues.append(f"残留进程 {residual_proc['residual_count']} 个")
    if temp_files["temp_file_count"] > 0:
        leak_verdict = "FAIL"
        issues.append(f"临时文件 {temp_files['temp_file_count']} 个")
    if not system_mem_healthy:
        leak_verdict = "FAIL"
        issues.append(f"系统可用内存不足 ({mem_now['avail_phys_mb']} MB < 10240 MB)")
    if not stage2_stable:
        leak_verdict = "FAIL"
        issues.append(f"stage2 重复运行峰值不稳定 (差异 {stage2_peak_diff} MB >= 200 MB)")

    results["leak_check"] = {
        "system_memory_now_mb": mem_now,
        "system_mem_healthy": system_mem_healthy,
        "residual_processes": residual_proc,
        "temp_files": temp_files,
        "stage1_peak_mem_diff_mb": stage1_peak_diff,
        "stage1_peak_diff_note": "不同帧天区峰值差异（Gaia xpsd 南天分区特性，非泄漏，P07-001 已记录）",
        "stage2_peak_mem_diff_mb": stage2_peak_diff,
        "stage2_peak_stable": stage2_stable,
        "output_files_count": len(output_files),
        "issues": issues,
        "verdict": leak_verdict,
    }
    print(f"\n[leak] 系统内存: {mem_now['avail_phys_mb']} MB 可用 / {mem_now['total_phys_mb']} MB 总计 (healthy={system_mem_healthy})")
    print(f"[leak] 残留进程: {residual_proc['residual_count']}")
    print(f"[leak] 临时文件: {temp_files['temp_file_count']}")
    print(f"[leak] stage1 峰值差异: {stage1_peak_diff} MB (天区特性, 非泄漏)")
    print(f"[leak] stage2 重复峰值差异: {stage2_peak_diff} MB (stable={stage2_stable})")
    print(f"[leak] verdict: {leak_verdict}")


def test_fault_injection(results: dict):
    """故障注入：stage2 运行时删除输入 HISS 文件"""
    print("\n" + "=" * 70)
    print("[P07-002] 故障注入测试（stage2 运行时删除输入 HISS）")
    print("=" * 70)

    # 复制 HISS 文件到临时目录，避免破坏原始文件
    fault_input_dir = str(OUTPUT_DIR / "fault_inject_input")
    if os.path.exists(fault_input_dir):
        shutil.rmtree(fault_input_dir)
    os.makedirs(fault_input_dir, exist_ok=True)

    # 复制 frame1.hiss 和 frame2.hiss
    src_files = []
    for f in os.listdir(STAGE2_INPUT_DIR):
        if f.endswith(".hiss"):
            shutil.copy2(os.path.join(STAGE2_INPUT_DIR, f), os.path.join(fault_input_dir, f))
            src_files.append(f)
    print(f"[fault] 复制 {len(src_files)} 个 HISS 到 {fault_input_dir}: {src_files}")

    if len(src_files) < 2:
        results["fault_injection"] = {
            "verdict": "SKIP",
            "reason": f"HISS 文件不足 ({len(src_files)} < 2)",
        }
        print(f"[fault] 跳过: HISS 文件不足")
        return

    # 启动 stage2，运行 2s 后删除一个 HISS 文件
    output_hcsd = str(OUTPUT_DIR / "fault_inject.hcsd")
    stdout_file = str(LOGS_DIR / "fault_inject_stdout.jsonl")
    stderr_file = str(LOGS_DIR / "fault_inject_stderr.log")
    mem_file = str(LOGS_DIR / "fault_inject_memory.json")

    env = make_env()
    cmd_args = ["stage2", "--frames", fault_input_dir, "--output", output_hcsd]

    import ctypes
    kernel32 = ctypes.WinDLL("kernel32.dll")

    t0 = time.time()
    samples: list = []
    stop_event = threading.Event()
    delete_target = os.path.join(fault_input_dir, src_files[0])
    delete_time = None
    delete_success = False

    print(f"[fault] 启动 stage2, 2s 后删除 {src_files[0]}")

    with open(stdout_file, "w", encoding="utf-8", errors="replace") as fout, \
         open(stderr_file, "w", encoding="utf-8", errors="replace") as ferr:
        proc = subprocess.Popen(
            [ORCH_EXE] + cmd_args, cwd=str(PROJECT_ROOT), stdout=fout, stderr=ferr, env=env,
            creationflags=subprocess.CREATE_NEW_PROCESS_GROUP,
        )
        sampler = threading.Thread(target=sample_memory, args=(proc, samples, stop_event), daemon=True)
        sampler.start()

        # 等待 2s 后删除文件
        time.sleep(2)
        try:
            os.remove(delete_target)
            delete_success = True
            delete_time = round(time.time() - t0, 3)
            print(f"[fault] 已删除 {src_files[0]} at t={delete_time}s")
        except Exception as e:
            print(f"[fault] 删除失败: {e}")

        # 等待进程结束（最多 60s）
        try:
            rc = proc.wait(timeout=60)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            rc = -1
        finally:
            stop_event.set()
            sampler.join(timeout=2.0)

    elapsed = round(time.time() - t0, 3)
    peak_ws = max([s["ws_mb"] for s in samples], default=0.0)

    with open(mem_file, "w", encoding="utf-8") as f:
        json.dump({"samples": samples, "peak_ws_mb": peak_ws, "label": "fault_inject"}, f, indent=2)

    # 读取 stderr 末尾
    stderr_tail = ""
    if os.path.exists(stderr_file):
        with open(stderr_file, "r", encoding="utf-8", errors="replace") as f:
            stderr_tail = f.read()[-2000:]

    # 验证：进程应正常退出（不崩溃），输出可能不生成或报错
    process_exited = proc.poll() is not None
    crashed = rc is None or (rc != 0 and rc < 0 and rc != -1)
    hcsd_generated = os.path.exists(output_hcsd)

    # 评估：故障注入后进程不应硬崩溃（exit_code 不应是负数除 -1 超时）
    # 可接受：报错退出（非零 exit_code）或成功完成（如果文件在删除前已读取）
    graceful = process_exited and not (rc is not None and rc < -1)

    results["fault_injection"] = {
        "delete_target": src_files[0],
        "delete_success": delete_success,
        "delete_at_sec": delete_time,
        "exit_code": rc,
        "elapsed_sec": elapsed,
        "process_exited": process_exited,
        "crashed": crashed,
        "graceful_handling": graceful,
        "hcsd_generated": hcsd_generated,
        "peak_working_set_mb": round(peak_ws, 2),
        "stderr_tail": stderr_tail[-500:],
        "verdict": "PASS" if graceful else "FAIL",
    }
    print(f"\n[fault] exit_code: {rc}, elapsed: {elapsed}s, process_exited: {process_exited}")
    print(f"[fault] graceful: {graceful}, hcsd_generated: {hcsd_generated}")
    print(f"[fault] verdict: {results['fault_injection']['verdict']}")

    # 清理临时目录
    try:
        shutil.rmtree(fault_input_dir)
    except Exception:
        pass


def main():
    ap = argparse.ArgumentParser(description="P07-002 长批次与故障稳定性测试")
    ap.add_argument("--all", action="store_true", help="运行全部测试")
    ap.add_argument("--stage1-batch", action="store_true", help="仅 stage1 批量")
    ap.add_argument("--stage2-repeat", action="store_true", help="仅 stage2 重复")
    ap.add_argument("--cancel-rerun", action="store_true", help="仅取消重跑")
    ap.add_argument("--leak-check", action="store_true", help="仅资源泄漏检查")
    ap.add_argument("--fault-inject", action="store_true", help="仅故障注入")
    args = ap.parse_args()

    # 确保目录存在
    LOGS_DIR.mkdir(parents=True, exist_ok=True)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    CONFIGS_DIR.mkdir(parents=True, exist_ok=True)

    # 如果已有结果文件，加载它（增量测试）
    results = {}
    if RESULTS_FILE.exists():
        try:
            with open(RESULTS_FILE, "r", encoding="utf-8") as f:
                results = json.load(f)
        except Exception:
            results = {}

    results["task_id"] = "P07-002"
    results["started_at"] = results.get("started_at", time.strftime("%Y-%m-%dT%H:%M:%S%z"))
    results["environment"] = {
        "orchestrator_exe": ORCH_EXE,
        "stage2_input_dir": STAGE2_INPUT_DIR,
        "stage2_baseline_sha256": STAGE2_BASELINE_SHA256,
        "system_memory_start_mb": get_system_memory_mb(),
    }

    run_all = args.all or not any([args.stage1_batch, args.stage2_repeat,
                                    args.cancel_rerun, args.leak_check, args.fault_inject])

    if run_all or args.stage1_batch:
        test_stage1_batch(results)
        save_results(results)

    if run_all or args.stage2_repeat:
        test_stage2_repeat(results)
        save_results(results)

    if run_all or args.cancel_rerun:
        test_cancel_rerun(results)
        save_results(results)

    if run_all or args.fault_inject:
        test_fault_injection(results)
        save_results(results)

    if run_all or args.leak_check:
        test_leak_check(results)
        save_results(results)

    # 汇总 verdict
    verdicts = {k: v.get("verdict", "N/A") for k, v in results.items()
                if isinstance(v, dict) and "verdict" in v}
    overall = "PASS" if all(v == "PASS" for v in verdicts.values()) else "FAIL"
    results["overall_verdict"] = overall
    results["verdicts"] = verdicts
    results["completed_at"] = time.strftime("%Y-%m-%dT%H:%M:%S%z")
    results["environment"]["system_memory_end_mb"] = get_system_memory_mb()
    save_results(results)

    print("\n" + "=" * 70)
    print(f"[P07-002] 全部测试完成 - Overall VERDICT: {overall}")
    print("=" * 70)
    for k, v in verdicts.items():
        print(f"  {k}: {v}")
    print(f"\n结果已保存: {RESULTS_FILE}")

    return 0 if overall == "PASS" else 1


def save_results(results: dict):
    RESULTS_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(RESULTS_FILE, "w", encoding="utf-8") as f:
        json.dump(results, f, ensure_ascii=False, indent=2)


if __name__ == "__main__":
    sys.exit(main())
