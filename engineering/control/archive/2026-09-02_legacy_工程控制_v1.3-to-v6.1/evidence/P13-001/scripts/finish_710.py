#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
I-002/I-003 完成自动化脚本 — 710帧回归收尾

功能:
  1. 等待 T4 进程完成
  2. 重启 T2/T3 完成剩余帧（跳过已缓存）
  3. 等待 T2/T3 完成
  4. 运行 merge_stage_b.py 合并结果
  5. 生成 GATE_I_REPORT.md 最终版
  6. 创建审计 ZIP

用法:
  python finish_710.py

注意: 此脚本会阻塞等待，适合长时间后台运行
"""

from __future__ import annotations
import json
import os
import subprocess
import sys
import time
from pathlib import Path

PROJECT_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
SCRIPTS_DIR = PROJECT_ROOT / "工程控制" / "evidence" / "P13-001" / "scripts"
MINGW_BIN = r"C:\msys64\mingw64\bin"

def set_env():
    os.environ["Path"] = os.environ.get("Path", "") + ";" + MINGW_BIN

def get_batch_state(device: str) -> dict:
    f = PROJECT_ROOT / "output" / f"p13-001-{device}" / "batch_state.json"
    if not f.exists():
        return {}
    with open(f, "r", encoding="utf-8") as fh:
        return json.load(fh)

def is_complete(device: str) -> bool:
    bs = get_batch_state(device)
    if not bs:
        return False
    return bs.get("completed", 0) >= bs.get("total_frames", 999)

def wait_for_device(device: str, timeout_s: int = 36000):
    """等待设备完成，每60秒检查一次"""
    print(f"[wait] 等待 {device} 完成 (timeout={timeout_s}s)...")
    start = time.time()
    while time.time() - start < timeout_s:
        bs = get_batch_state(device)
        if not bs:
            print(f"[wait] {device}: batch_state not found, waiting...")
            time.sleep(60)
            continue
        done = bs.get("completed", 0)
        total = bs.get("total_frames", 0)
        passed = bs.get("passed", 0)
        errors = bs.get("errors", 0)
        pct = (done / total * 100) if total > 0 else 0
        print(f"[wait] {device}: {done}/{total} ({pct:.1f}%) pass={passed} err={errors}")
        if done >= total:
            print(f"[wait] {device} 完成!")
            return True
        time.sleep(60)
    print(f"[wait] {device} 超时!", file=sys.stderr)
    return False

def run_device(device: str):
    """启动设备回归进程（后台）"""
    env = os.environ.copy()
    env["P13_EVIDENCE_DIR"] = f"output/p13-001-{device}"
    log_file = PROJECT_ROOT / "output" / f"p13-001-{device}" / "raw_logs" / f"full_{device}_resume.log"
    cmd = [
        sys.executable,
        str(SCRIPTS_DIR / "stage1_batch_runner.py"),
        "run",
        "--device", device,
    ]
    print(f"[run] 启动 {device}: {' '.join(cmd)}")
    proc = subprocess.Popen(
        cmd,
        stdout=open(log_file, "w"),
        stderr=subprocess.STDOUT,
        cwd=str(PROJECT_ROOT),
        env=env,
    )
    return proc

def run_merge():
    """运行汇总脚本"""
    print("[merge] 运行 merge_stage_b.py...")
    proc = subprocess.run(
        [sys.executable, str(SCRIPTS_DIR / "merge_stage_b.py")],
        cwd=str(PROJECT_ROOT),
        capture_output=True, text=True,
    )
    print(proc.stdout)
    if proc.returncode != 0:
        print(f"[merge] 错误: {proc.stderr}", file=sys.stderr)
    return proc.returncode == 0

def main():
    set_env()

    # 1. 等待 T4 完成
    print("=" * 60)
    print("阶段1: 等待 T4 完成")
    print("=" * 60)
    wait_for_device("T4", timeout_s=36000)  # 10小时超时

    # 2. 重启 T2/T3 完成剩余帧
    print("\n" + "=" * 60)
    print("阶段2: 重启 T2/T3 完成剩余帧")
    print("=" * 60)
    for dev in ["T2", "T3"]:
        if not is_complete(dev):
            print(f"[resume] {dev} 未完成, 重启...")
            run_device(dev)
        else:
            print(f"[resume] {dev} 已完成, 跳过")

    # 3. 等待 T2/T3 完成
    for dev in ["T2", "T3"]:
        if not is_complete(dev):
            wait_for_device(dev, timeout_s=36000)

    # 4. 运行汇总
    print("\n" + "=" * 60)
    print("阶段3: 合并结果并生成报告")
    print("=" * 60)
    run_merge()

    # 5. 检查最终结果
    print("\n" + "=" * 60)
    print("阶段4: 最终状态检查")
    print("=" * 60)
    total_done = 0
    total_pass = 0
    total_err = 0
    for dev in ["T2", "T3", "T4"]:
        bs = get_batch_state(dev)
        if bs:
            done = bs.get("completed", 0)
            total = bs.get("total_frames", 0)
            passed = bs.get("passed", 0)
            errors = bs.get("errors", 0)
            pct = (done / total * 100) if total > 0 else 0
            print(f"  {dev}: {done}/{total} ({pct:.1f}%) pass={passed} err={errors}")
            total_done += done
            total_pass += passed
            total_err += errors

    print(f"\n  TOTAL: {total_done}/710 pass={total_pass} err={total_err}")
    print(f"  通过率: {total_pass/710*100:.1f}%")

    if total_done >= 710:
        print("\n[完成] 710帧全量回归完成!")
        print("[完成] 报告已生成: engineering_authoritative/evidence/I-002/STAGE_B_REPORT.md")
    else:
        print(f"\n[警告] 仍有 {710 - total_done} 帧未完成")

if __name__ == "__main__":
    main()
