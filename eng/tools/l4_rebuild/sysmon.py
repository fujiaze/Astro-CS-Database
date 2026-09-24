#!/usr/bin/env python3
"""RELEASE-02 系统性能监视器 (sysmon) —— 与目标程序同步启动、按固定间隔采样、写 CSV。

设计意图（负责人原话）:
    「用一个和程序同步启动的 Python 做系统性能利用率统计。」

数据源优先 Linux /proc，**不依赖任何第三方包**（无 psutil 也能跑）。
只观测，不改变被测程序的任何行为；本工具不写被测程序的文件，只写 --out CSV。

采样项:
    - 整机 CPU%（以及每核 CPU%）        /proc/stat
    - 进程树 CPU%（--pid 及其后代）      /proc/<pid>/stat
    - 进程树 RSS / PSS / swap / 线程数   /proc/<pid>/status, smaps_rollup
    - 系统 swap 用量与换入换出           /proc/meminfo, /proc/vmstat
    - 磁盘读/写字节、IO 次数、IO 占用    /proc/diskstats
    - 负载与运行队列                     /proc/loadavg, /proc/stat
    - 系统/进程上下文切换                /proc/stat, /proc/<pid>/status
    - 系统 IO 等待占比                   /proc/stat (iowait)

用法:
    python3 eng/tools/l4_rebuild/sysmon.py --pid 1234 --interval 1 --out run/x/sysmon.csv
    python3 eng/tools/l4_rebuild/sysmon.py --out sysmon.csv --duration 60      # 只看整机
    # 与 run_timed.sh 集成: 后台启动 (跟踪 runner 自身 pid 及其全部后代), 结束后 kill -TERM。

信号: SIGINT/SIGTERM 触发收尾 (写完当前行后正常退出, 退出码 0)。
线程数/间隔不做任何硬编码假设 (AGENTS §5); --interval 由调用方给定。
"""
from __future__ import annotations

import argparse
import datetime
import os
import re
import signal
import sys
import time
from typing import Dict, Iterable, List, Optional, Set, Tuple

# ── /proc 布局 ───────────────────────────────────────────────────────────────
PROC = "/proc"
CLK_TCK = 100  # Linux 恒为 100; 仅用于 ticks→秒换算

# CSV 列顺序 (header 即事实源)
COLUMNS = [
    "ts_utc", "seq", "interval_s", "tag",
    "cpu_pct", "cpu_pct_per_core",
    "load1", "load5", "load15", "runnable", "n_tasks", "n_cpus",
    "mem_total_kb", "mem_available_kb", "mem_used_kb",
    "swap_total_kb", "swap_free_kb", "swap_used_kb",
    "swap_in_kb", "swap_out_kb",
    "disk_read_kb", "disk_write_kb", "disk_read_iops", "disk_write_iops",
    "disk_io_busy_pct",
    "sys_iowait_pct", "sys_ctxt_switches",
    "proc_n", "proc_cpu_pct", "proc_rss_kb", "proc_pss_kb", "proc_swap_kb",
    "proc_threads", "proc_ctxt_switches", "proc_read_kb", "proc_write_kb",
]

_WHOLE_DISK_RE = re.compile(r"^(sd[a-z]+|vd[a-z]+|nvme\d+n\d+|hd[a-z]+|mmcblk\d+)$")


# ── 基础读取 ─────────────────────────────────────────────────────────────────
def _read(path: str) -> Optional[str]:
    try:
        with open(path, "rb") as f:
            return f.read().decode("ascii", errors="replace")
    except OSError:
        return None


def read_cpu_stat() -> Optional[Dict[str, object]]:
    """返回 {total_fields, per_core: {name: fields}}; 失败 None。"""
    text = _read(os.path.join(PROC, "stat"))
    if text is None:
        return None
    out: Dict[str, object] = {"total": None, "per_core": {}, "ctxt": None,
                              "procs_running": None, "procs_blocked": None}
    per_core: Dict[str, List[int]] = {}
    for line in text.splitlines():
        if line.startswith("cpu"):
            parts = line.split()
            name = parts[0]
            try:
                vals = [int(x) for x in parts[1:9]]
            except ValueError:
                continue
            if name == "cpu":
                out["total"] = vals
            else:
                per_core[name] = vals
        elif line.startswith("ctxt "):
            out["ctxt"] = int(line.split()[1])
        elif line.startswith("procs_running "):
            out["procs_running"] = int(line.split()[1])
        elif line.startswith("procs_blocked "):
            out["procs_blocked"] = int(line.split()[1])
    out["per_core"] = per_core
    return out


def _cpu_busy_pct(prev: List[int], cur: List[int]) -> Tuple[float, float]:
    """返回 (busy_pct, iowait_pct); busy = total - idle - iowait。"""
    if prev is None or cur is None or len(cur) < 5:
        return 0.0, 0.0
    d = [max(0, c - p) for c, p in zip(cur, prev)]
    total = sum(d)
    if total <= 0:
        return 0.0, 0.0
    idle = d[3]
    iowait = d[4]
    busy = total - idle - iowait
    return 100.0 * busy / total, 100.0 * iowait / total


def read_meminfo() -> Dict[str, int]:
    out: Dict[str, int] = {}
    text = _read(os.path.join(PROC, "meminfo"))
    if not text:
        return out
    for line in text.splitlines():
        parts = line.split(":")
        if len(parts) != 2:
            continue
        key = parts[0].strip()
        val = parts[1].strip().split()
        if not val:
            continue
        try:
            out[key] = int(val[0])
        except ValueError:
            continue
    return out


def read_vmstat() -> Dict[str, int]:
    out: Dict[str, int] = {}
    text = _read(os.path.join(PROC, "vmstat"))
    if not text:
        return out
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 2:
            try:
                out[parts[0]] = int(parts[1])
            except ValueError:
                continue
    return out


def read_loadavg() -> Tuple[float, float, float, int, int]:
    text = _read(os.path.join(PROC, "loadavg"))
    if not text:
        return 0.0, 0.0, 0.0, 0, 0
    parts = text.split()
    try:
        l1, l5, l15 = float(parts[0]), float(parts[1]), float(parts[2])
        runnable, total = parts[3].split("/")
        return l1, l5, l15, int(runnable), int(total)
    except (IndexError, ValueError):
        return 0.0, 0.0, 0.0, 0, 0


def read_diskstats() -> Dict[str, Tuple[int, int, int, int, int]]:
    """{dev: (sectors_read, sectors_written, reads, writes, ms_io)}，仅整盘。"""
    out: Dict[str, Tuple[int, int, int, int, int]] = {}
    text = _read(os.path.join(PROC, "diskstats"))
    if not text:
        return out
    for line in text.splitlines():
        parts = line.split()
        if len(parts) < 14:
            continue
        dev = parts[2]
        if not _WHOLE_DISK_RE.match(dev):
            continue
        try:
            reads = int(parts[3])
            sectors_read = int(parts[5])
            ms_reading = int(parts[6])
            writes = int(parts[7])
            sectors_written = int(parts[9])
            ms_writing = int(parts[10])
        except ValueError:
            continue
        out[dev] = (sectors_read, sectors_written, reads, writes,
                    ms_reading + ms_writing)
    return out


# ── 进程树 ───────────────────────────────────────────────────────────────────
def snapshot_all_procs() -> Dict[int, Dict[str, int]]:
    """扫描 /proc/<pid>/stat: {pid: {ppid,utime,stime,threads,rss_pages}}。"""
    out: Dict[int, Dict[str, int]] = {}
    try:
        entries = os.listdir(PROC)
    except OSError:
        return out
    for entry in entries:
        if not entry.isdigit():
            continue
        pid = int(entry)
        text = _read(os.path.join(PROC, entry, "stat"))
        if text is None:
            continue
        rp = text.rfind(")")
        if rp < 0:
            continue
        tail = text[rp + 1:].split()
        try:
            out[pid] = {
                "ppid": int(tail[1]),
                "utime": int(tail[11]),
                "stime": int(tail[12]),
                "threads": int(tail[17]),
                "rss_pages": int(tail[21]),
            }
        except (IndexError, ValueError):
            continue
    return out


def descendants(all_procs: Dict[int, Dict[str, int]], roots: Iterable[int]) -> Set[int]:
    selected: Set[int] = {p for p in roots if p in all_procs}
    changed = True
    while changed:
        changed = False
        for pid, info in all_procs.items():
            if pid in selected:
                continue
            if info["ppid"] in selected:
                selected.add(pid)
                changed = True
    return selected


def read_proc_status(pid: int) -> Dict[str, int]:
    out: Dict[str, int] = {}
    text = _read(os.path.join(PROC, str(pid), "status"))
    if not text:
        return out
    for line in text.splitlines():
        if ":" not in line:
            continue
        key, rest = line.split(":", 1)
        parts = rest.strip().split()
        if not parts:
            continue
        try:
            out[key.strip()] = int(parts[0])
        except ValueError:
            continue
    return out


def read_proc_pss_kb(pid: int) -> Optional[int]:
    text = _read(os.path.join(PROC, str(pid), "smaps_rollup"))
    if text is None:
        return None
    for line in text.splitlines():
        if line.startswith("Pss:"):
            parts = line.split()
            try:
                return int(parts[1])
            except (IndexError, ValueError):
                return None
    return None


def read_proc_io(pid: int) -> Tuple[Optional[int], Optional[int]]:
    text = _read(os.path.join(PROC, str(pid), "io"))
    if text is None:
        return None, None
    rb = wb = None
    for line in text.splitlines():
        parts = line.split(":")
        if len(parts) != 2:
            continue
        try:
            if parts[0].strip() == "read_bytes":
                rb = int(parts[1].strip())
            elif parts[0].strip() == "write_bytes":
                wb = int(parts[1].strip())
        except ValueError:
            continue
    return rb, wb


# ── 采样 ─────────────────────────────────────────────────────────────────────
class Sampler:
    def __init__(self, pids: List[int], tag: str):
        self.pids = pids
        self.tag = tag
        self.seq = 0
        self.prev_cpu: Optional[List[int]] = None
        self.prev_cores: Dict[str, List[int]] = {}
        self.prev_disk: Dict[str, Tuple[int, int, int, int, int]] = {}
        self.prev_vm: Dict[str, int] = {}
        self.prev_proc: Dict[int, Dict[str, int]] = {}
        self.prev_ctxt: Optional[int] = None
        self.prev_proc_io: Dict[int, Tuple[int, int]] = {}
        self.prev_proc_ctxt: Dict[int, int] = {}
        self.prev_t = time.monotonic()

    def sample(self) -> Optional[List[object]]:
        now = time.monotonic()
        interval = now - self.prev_t
        if interval <= 0:
            interval = 1e-9
        self.prev_t = now
        self.seq += 1

        cpu = read_cpu_stat()
        if cpu is None or cpu["total"] is None:
            return None
        busy, iowait = _cpu_busy_pct(self.prev_cpu, cpu["total"])  # type: ignore[arg-type]
        self.prev_cpu = cpu["total"]  # type: ignore[assignment]

        per_core_pcts: List[str] = []
        for name, vals in sorted((cpu["per_core"] or {}).items()):  # type: ignore[union-attr]
            b, _ = _cpu_busy_pct(self.prev_cores.get(name), vals)
            per_core_pcts.append("%.1f" % b)
        self.prev_cores = cpu["per_core"]  # type: ignore[assignment]

        mem = read_meminfo()
        mem_total = mem.get("MemTotal", 0)
        mem_avail = mem.get("MemAvailable", 0)
        swap_total = mem.get("SwapTotal", 0)
        swap_free = mem.get("SwapFree", 0)

        vm = read_vmstat()
        if self.prev_vm:
            swap_in_kb = max(0, vm.get("pswpin", 0) - self.prev_vm.get("pswpin", 0)) * 4
            swap_out_kb = max(0, vm.get("pswpout", 0) - self.prev_vm.get("pswpout", 0)) * 4
        else:
            swap_in_kb = swap_out_kb = 0  # 首个样本无前值, 不把累计计数当增量
        self.prev_vm = vm

        disk = read_diskstats()
        d_read_sec = d_write_sec = d_reads = d_writes = d_ms = 0
        for dev, cur in disk.items():
            prev = self.prev_disk.get(dev)
            if prev is not None:
                d_read_sec += max(0, cur[0] - prev[0])
                d_write_sec += max(0, cur[1] - prev[1])
                d_reads += max(0, cur[2] - prev[2])
                d_writes += max(0, cur[3] - prev[3])
                d_ms += max(0, cur[4] - prev[4])
        self.prev_disk = disk

        load1, load5, load15, runnable, n_tasks = read_loadavg()
        n_cpus = os.cpu_count() or 0

        ctxt = cpu.get("ctxt")
        ctxt_delta = 0
        if isinstance(ctxt, int):
            if self.prev_ctxt is not None:
                ctxt_delta = max(0, ctxt - self.prev_ctxt)
            self.prev_ctxt = ctxt

        # 进程树
        all_procs = snapshot_all_procs()
        sel = descendants(all_procs, self.pids) if self.pids else set()
        proc_cpu_ticks = proc_threads = proc_rss_kb = proc_pss_kb = 0
        proc_swap_kb = proc_ctxt = 0
        proc_read_bytes = proc_write_bytes = 0
        cur_proc_io: Dict[int, Tuple[int, int]] = {}
        cur_proc_ctxt: Dict[int, int] = {}
        for pid in sel:
            info = all_procs.get(pid)
            if info is None:
                continue
            prev = self.prev_proc.get(pid)
            if prev is not None:
                proc_cpu_ticks += max(0, info["utime"] - prev["utime"]) + \
                                  max(0, info["stime"] - prev["stime"])
            proc_threads += info["threads"]
            status = read_proc_status(pid)
            proc_rss_kb += status.get("VmRSS", info["rss_pages"] * 4)
            proc_swap_kb += status.get("VmSwap", 0)
            pss = read_proc_pss_kb(pid)
            if pss is not None:
                proc_pss_kb += pss
            pctx = status.get("voluntary_ctxt_switches", 0) + \
                   status.get("nonvoluntary_ctxt_switches", 0)
            cur_proc_ctxt[pid] = pctx
            pv = self.prev_proc_ctxt.get(pid)
            if pv is not None:
                proc_ctxt += max(0, pctx - pv)
            rb, wb = read_proc_io(pid)
            if rb is not None and wb is not None:
                cur_proc_io[pid] = (rb, wb)
                pv_io = self.prev_proc_io.get(pid)
                if pv_io is not None:
                    proc_read_bytes += max(0, rb - pv_io[0])
                    proc_write_bytes += max(0, wb - pv_io[1])
        self.prev_proc = all_procs
        self.prev_proc_io = cur_proc_io
        self.prev_proc_ctxt = cur_proc_ctxt

        proc_cpu_pct = 0.0
        if sel:
            proc_cpu_pct = 100.0 * (proc_cpu_ticks / CLK_TCK) / interval

        ts = datetime.datetime.now(datetime.timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"
        row: List[object] = [
            ts, self.seq, "%.3f" % interval, self.tag,
            "%.2f" % busy, ";".join(per_core_pcts),
            "%.2f" % load1, "%.2f" % load5, "%.2f" % load15, runnable, n_tasks, n_cpus,
            mem_total, mem_avail, max(0, mem_total - mem_avail),
            swap_total, swap_free, max(0, swap_total - swap_free),
            swap_in_kb, swap_out_kb,
            d_read_sec // 2, d_write_sec // 2, d_reads, d_writes,
            "%.2f" % (100.0 * d_ms / (interval * 1000.0)),
            "%.2f" % iowait, ctxt_delta,
            len(sel), "%.2f" % proc_cpu_pct, proc_rss_kb, proc_pss_kb,
            proc_swap_kb, proc_threads, proc_ctxt,
            proc_read_bytes // 1024, proc_write_bytes // 1024,
        ]
        return row


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="ACSD RELEASE-02 procfs 系统监视器")
    ap.add_argument("--out", required=True, help="输出 CSV 路径")
    ap.add_argument("--pid", action="append", default=[],
                    help="跟踪目标进程 pid (可重复; 含全部后代)")
    ap.add_argument("--interval", type=float, default=1.0, help="采样间隔秒 (默认 1.0)")
    ap.add_argument("--duration", type=float, default=0.0,
                    help="总时长秒 (0 = 直到收到信号)")
    ap.add_argument("--tag", default="", help="写入每行的自由标签")
    ap.add_argument("--quiet", action="store_true", help="不打印启动/收尾信息")
    args = ap.parse_args(argv)

    if args.interval <= 0:
        sys.stderr.write("sysmon: --interval must be > 0\n")
        return 2
    pids: List[int] = []
    for raw in args.pid:
        for piece in str(raw).split(","):
            piece = piece.strip()
            if piece:
                try:
                    pids.append(int(piece))
                except ValueError:
                    sys.stderr.write("sysmon: bad --pid %r\n" % piece)
                    return 2

    out_dir = os.path.dirname(os.path.abspath(args.out))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    stop = {"flag": False}

    def _on_signal(_signum, _frame):
        stop["flag"] = True

    signal.signal(signal.SIGINT, _on_signal)
    signal.signal(signal.SIGTERM, _on_signal)

    sampler = Sampler(pids, args.tag)
    started = time.monotonic()
    rows = 0
    with open(args.out, "w", encoding="utf-8", newline="") as fh:
        fh.write(",".join(COLUMNS) + "\n")
        fh.flush()
        if not args.quiet:
            sys.stderr.write("sysmon: start out=%s interval=%.3f pids=%s\n"
                             % (args.out, args.interval, pids or "-"))
        next_t = time.monotonic()
        while not stop["flag"]:
            row = sampler.sample()
            if row is not None:
                fh.write(",".join(str(x) for x in row) + "\n")
                fh.flush()
                rows += 1
            if args.duration > 0 and (time.monotonic() - started) >= args.duration:
                break
            next_t += args.interval
            sleep_for = next_t - time.monotonic()
            if sleep_for > 0:
                time.sleep(sleep_for)
            else:
                next_t = time.monotonic()  # 落后则重锚, 不堆积
    if not args.quiet:
        sys.stderr.write("sysmon: stop rows=%d out=%s\n" % (rows, args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
