#!/usr/bin/env python3
"""mem_guard.py —— 重计算的内存看门狗（进程树 RSS 上限即杀）

背景：本机 swap 永久禁用，物理内存约 23 GiB。AstroCS 的重计算（ctest / 探针 /
基准）曾把单个进程涨到 >11 GB 匿名内存，触发**全局 OOM killer**，连带杀掉同处
`dsh-web.service` 单元内的 DSH 本体，中断会话。

本工具的职责：把重计算包进一个**进程树 RSS 上限**，超限只杀计算进程组，
不触及 DSH。

用法:
    python3 eng/tools/monitoring/mem_guard.py --max-rss-gb 8 -- <command> [args...]

判据:
    * 每 --poll-interval 秒采样一次**进程树**（根 + 全部后代）的 VmRSS 之和；
    * 采样值 > --max-rss-gb ⇒ 对进程组发 SIGTERM，--grace 秒后仍存活则 SIGKILL；
    * 退出码：正常退出透传子进程退出码；被看门狗杀死 ⇒ 137 (128+SIGKILL)
      或 143 (128+SIGTERM)，并在 stderr 打印 `[mem_guard] KILLED`。
    * 结束（无论何种方式）都在 stderr 打印 `[mem_guard] peak_rss_gb=...`，
      供日志留证。

设计约束:
    * 不用 shell=True（argv 数组直传），避免注入与进程组归属歧义；
    * 用 `start_new_session=True` 建独立进程组，killpg 只影响本命令的进程树；
    * 采样走 /proc（无第三方依赖），RSS 不可得时保守跳过该进程并计数，
      连续不可得超过阈值则判 `rss_unavailable` 并杀（fail-closed）。
"""
from __future__ import annotations

import argparse
import os
import signal
import sys
import time


def _read_vm_rss_kb(pid: int):
    """读 /proc/<pid>/status 的 VmRSS（kB）；不可得返回 None。"""
    try:
        with open(f"/proc/{pid}/status", "r") as fh:
            for line in fh:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1])
    except (OSError, ValueError, IndexError):
        return None
    return None


def _tree_pids(root_pid: int):
    """返回 root_pid 及其全部后代的 pid 集合（读 /proc/<pid>/stat 的 ppid）。"""
    ppid_of = {}
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        pid = int(entry)
        try:
            with open(f"/proc/{pid}/stat", "rb") as fh:
                data = fh.read()
        except OSError:
            continue
        # comm 可能含空格与括号 ⇒ 取最后一个 ')' 之后再切
        rp = data.rfind(b")")
        if rp < 0:
            continue
        fields = data[rp + 2:].split()
        if len(fields) < 2:
            continue
        try:
            ppid_of[pid] = int(fields[1])
        except ValueError:
            continue

    children = {}
    for pid, ppid in ppid_of.items():
        children.setdefault(ppid, []).append(pid)

    out, stack = set(), [root_pid]
    while stack:
        cur = stack.pop()
        if cur in out:
            continue
        out.add(cur)
        stack.extend(children.get(cur, ()))
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="重计算内存看门狗：进程树 RSS 超限即杀，保护 DSH 本体")
    ap.add_argument("--max-rss-gb", type=float, required=True,
                    help="进程树 RSS 上限（GiB）；超限即杀")
    ap.add_argument("--poll-interval", type=float, default=0.5,
                    help="采样间隔秒（默认 0.5）")
    ap.add_argument("--grace", type=float, default=10.0,
                    help="SIGTERM 后等待秒数，仍存活则 SIGKILL（默认 10）")
    ap.add_argument("--label", default="",
                    help="日志标签（默认取命令 basename）")
    ap.add_argument("command", nargs=argparse.REMAINDER,
                    help="-- 之后为要执行的命令")
    args = ap.parse_args(argv)

    cmd = args.command
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]
    if not cmd:
        ap.error("缺少命令（用法: mem_guard.py --max-rss-gb N -- <command>）")

    limit_kb = int(args.max_rss_gb * 1024 * 1024)
    label = args.label or os.path.basename(cmd[0])

    proc = __import__("subprocess").Popen(cmd, start_new_session=True)
    root_pid = proc.pid
    pgid = os.getpgid(root_pid)

    peak_kb = 0
    killed = False
    miss_streak = 0

    try:
        while True:
            rc = proc.poll()
            if rc is not None:
                break
            total_kb = 0
            got_any = False
            for pid in _tree_pids(root_pid):
                v = _read_vm_rss_kb(pid)
                if v is None:
                    continue
                got_any = True
                total_kb += v
            if got_any:
                miss_streak = 0
                peak_kb = max(peak_kb, total_kb)
            else:
                miss_streak += 1

            over = got_any and total_kb > limit_kb
            blind = miss_streak >= 20  # 连续 10 s 读不到任何 RSS ⇒ fail-closed
            if over or blind:
                why = (f"RSS {total_kb/1048576:.2f} GiB > 上限 {args.max_rss_gb} GiB"
                       if over else "RSS 连续不可得（fail-closed）")
                print(f"[mem_guard] KILLED label={label} pid={root_pid} {why}",
                      file=sys.stderr, flush=True)
                killed = True
                try:
                    os.killpg(pgid, signal.SIGTERM)
                except OSError:
                    pass
                deadline = time.time() + args.grace
                while time.time() < deadline and proc.poll() is None:
                    time.sleep(0.2)
                if proc.poll() is None:
                    try:
                        os.killpg(pgid, signal.SIGKILL)
                    except OSError:
                        pass
                break
            time.sleep(args.poll_interval)
    except KeyboardInterrupt:
        try:
            os.killpg(pgid, signal.SIGKILL)
        except OSError:
            pass
        raise

    rc = proc.wait()
    print(f"[mem_guard] peak_rss_gb={peak_kb/1048576:.3f} limit_gb={args.max_rss_gb} "
          f"label={label} exit={rc}", file=sys.stderr, flush=True)
    # 被看门狗杀死统一返回 137（与 cgroup 级 OOM 的约定一致），
    # 不透传子进程的负信号码——调用方只需判 137 即知"内存超限被杀"。
    if killed:
        return 137
    return rc if rc is not None else 1


if __name__ == "__main__":
    sys.exit(main())
