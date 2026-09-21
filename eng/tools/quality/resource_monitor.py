#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/tools/quality/resource_monitor.py — 外挂资源监控（P26 T1；纯 stdlib，Linux /proc）。

设计要点（负责人裁决 1: 资源利用率记录不应由程序本身做, 而是外挂脚本监控）:
  * 本脚本是独立外挂进程, 只读 /proc/**; 被监控程序**无需任何配合/埋点/环境变量**。
  * 采样默认 1 Hz(--interval 可配), 覆盖进程树: 总 CPU%(相对已分配容量)、每线程 CPU、
    RSS/PSS、内存增长率(滑动窗口)、读/写字节、I/O wait、墙钟。
  * 产物: samples.csv(原始曲线数据) + threads.csv(每线程 CPU) +
    curve_cpu.png / curve_mem.png / curve_io.png(曲线图) + summary.json(事实记录)。
  * --judge: 按**可配置阈值**给出 PASS/FAIL/WARN/INSUFFICIENT 的**建议**, 落 judge.json。
    它是给前台/负责人判断用的**外挂裁决**, 不是程序内置发布门; 低于工作量下限
    (--min-core-seconds, 线程秒 = 等效核·秒)的运行不做裁决, 只记录。

用法:
  python3 eng/tools/quality/resource_monitor.py --out run/resource/mon -- <cmd> [args...]
  python3 eng/tools/quality/resource_monitor.py --out DIR --pid <pid> [--duration 30]
  python3 eng/tools/quality/resource_monitor.py --judge --out DIR -- <cmd> [args...]
  python3 eng/tools/quality/resource_monitor.py --judge --from DIR

退出码: 默认透传被测命令退出码(超时 124); 加 --judge-exit-code 时按建议 0(PASS/WARN/
INSUFFICIENT)/3(FAIL)。纯 stdlib, 无第三方依赖; 曲线图为自绘 PNG。
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import signal
import statistics
import struct
import subprocess
import sys
import time
import zlib
from collections import deque
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

SCHEMA_VERSION = 1
CLK_TCK = float(os.sysconf("SC_CLK_TCK"))
PAGE_SIZE = int(os.sysconf("SC_PAGE_SIZE"))

# 默认阈值(可被 CLI 覆盖)。注意: 这些是**外挂裁决建议**阈值, 不是程序内置门。
# 数值不在本文件发明：唯一数值源 = contracts/resource_gate_v1.json（G-RES-01）；
# 判据语义权威 = docs/plugins/infrastructure/21_observability.md §8。
_RESOURCE_GATE_CONTRACT_PATH = (
    Path(__file__).resolve().parents[3] / "contracts" / "resource_gate_v1.json")


def load_resource_gate_contract(path=None) -> dict:
    """读 G-RES-01 数值契约（唯一数值源）；不可得 → RuntimeError（不静默回落）。"""
    p = Path(path) if path is not None else _RESOURCE_GATE_CONTRACT_PATH
    try:
        with open(p, "r", encoding="utf-8") as fh:
            doc = json.load(fh)
    except (OSError, ValueError) as exc:
        raise RuntimeError(f"资源门数值契约不可读/非法: {p} ({exc})") from exc
    if doc.get("schema") != "astrocs.resource-gate/v1":
        raise RuntimeError(f"资源门数值契约 schema 不符: {p}")
    return doc


RESOURCE_GATE_CONTRACT = load_resource_gate_contract()
_GATE_COMPUTE = RESOURCE_GATE_CONTRACT["compute"]
_GATE_MEMORY = RESOURCE_GATE_CONTRACT["memory"]

DEFAULT_MIN_MEAN_CAPACITY_PERCENT = float(_GATE_COMPUTE["mean_utilization_min_percent"])
DEFAULT_LOW_UTIL_PERCENT = float(_GATE_COMPUTE["queue_low_utilization_percent"])
DEFAULT_MAX_LOW_UTIL_RUN_SECONDS = float(_GATE_COMPUTE["queue_low_window_seconds_min"])
DEFAULT_MIN_CORE_SECONDS = float(RESOURCE_GATE_CONTRACT["workload_floor_core_seconds"])
# GATE-FIX-RES 对齐（R-4 D-13 item 5）：单位统一为 **MiB/s**（1048576 B/s），
# 判据方向统一为 **>=**（与 C++ kAllocGrowthUnboundedMbPerS / evaluate_mon002
# 同口径）。旧注释写 MB/s 而实际算的是 MiB/s，且方向为严格大于 —— 两处口径差
# 4.858%。
DEFAULT_MAX_MEM_GROWTH_MIB_PER_S = float(_GATE_MEMORY["growth_limit_mib_per_s"])
DEFAULT_MEM_GROWTH_PREDICATE = str(_GATE_MEMORY["growth_predicate"])
DEFAULT_MIN_BUSY_THREADS = int(_GATE_COMPUTE["min_active_compute_threads"])
DEFAULT_MIN_EFFECTIVE_CORES = 2.0          # 有效核数(等效核)下限(仅事实/参考)
DEFAULT_SINGLE_THREAD_CORES_MAX = 1.2      # 有效核数 <= 该值视为"只有一个活跃计算线程"
DEFAULT_MEM_WINDOW_SECONDS = 10.0          # 内存增长率滑动窗口
# 兼容别名（旧名保留，值同 MiB/s；新代码用 MIB 名）。
DEFAULT_MAX_MEM_GROWTH_MB_PER_S = DEFAULT_MAX_MEM_GROWTH_MIB_PER_S


# ------------------------------------------------------------------ /proc 读取 ----
def _read_text(path):
    try:
        return Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None


def _proc_stat(pid):
    """解析 /proc/<pid>/stat。comm 可能含空格/括号 → 以最后一个 ) 为界。"""
    text = _read_text("/proc/%d/stat" % pid)
    if not text:
        return None
    lp = text.rfind(")")
    if lp < 0:
        return None
    body = text[lp + 2:].split()
    if len(body) < 40:
        return None
    try:
        return {
            "state": body[0],
            "ppid": int(body[1]),
            "utime": int(body[11]),
            "stime": int(body[12]),
            "cutime": int(body[13]),
            "cstime": int(body[14]),
            "num_threads": int(body[17]),
            "blkio_ticks": int(body[39]),
        }
    except (ValueError, IndexError):
        return None


def _all_procs():
    """返回 {pid: ppid} (只含可读 stat 的进程)。"""
    out = {}
    try:
        names = os.listdir("/proc")
    except OSError:
        return out
    for name in names:
        if not name.isdigit():
            continue
        st = _proc_stat(int(name))
        # 僵尸/已死进程(Z/X)不再算"活进程树"成员: 否则 --pid 挂到被子进程未被
        # 父进程 reap 的目标上会永远等不到"树空"。
        if st is not None and st["state"] not in ("Z", "X"):
            out[int(name)] = st["ppid"]
    return out


def _tree_pids(root_pid):
    """root_pid 的活进程子树(含自身); root 已不存在 → []。父进程表快照一次。"""
    parents = _all_procs()
    if root_pid not in parents:
        return []
    children = {}
    for pid, ppid in parents.items():
        children.setdefault(ppid, []).append(pid)
    seen = set()
    stack = [root_pid]
    while stack:
        pid = stack.pop()
        if pid in seen:
            continue
        seen.add(pid)
        stack.extend(children.get(pid, []))
    return sorted(seen)


def _task_ticks(pid):
    """{tid: utime+stime ticks}。"""
    out = {}
    try:
        tids = os.listdir("/proc/%d/task" % pid)
    except OSError:
        return out
    for tid in tids:
        if not tid.isdigit():
            continue
        text = _read_text("/proc/%d/task/%s/stat" % (pid, tid))
        if not text:
            continue
        lp = text.rfind(")")
        if lp < 0:
            continue
        body = text[lp + 2:].split()
        if len(body) < 15:
            continue
        try:
            out[int(tid)] = int(body[11]) + int(body[12])
        except (ValueError, IndexError):
            continue
    return out


def _status_kb(pid, key):
    text = _read_text("/proc/%d/status" % pid)
    if not text:
        return None
    for line in text.splitlines():
        k, sep, v = line.partition(":")
        if sep and k.strip() == key:
            try:
                return int(v.split()[0])
            except (ValueError, IndexError):
                return None
    return None


def _smaps_rollup(pid, key):
    text = _read_text("/proc/%d/smaps_rollup" % pid)
    if not text:
        return None
    for line in text.splitlines():
        k, sep, v = line.partition(":")
        if sep and k.strip() == key:
            try:
                return int(v.split()[0])
            except (ValueError, IndexError):
                return None
    return None


def _proc_io(pid):
    text = _read_text("/proc/%d/io" % pid)
    if not text:
        return None
    out = {}
    for line in text.splitlines():
        k, sep, v = line.partition(":")
        if sep:
            try:
                out[k.strip()] = int(v.strip())
            except ValueError:
                pass
    return out


def snapshot(root_pid):
    """进程树单次快照(纯 /proc 观测)。"""
    pids = _tree_pids(root_pid)
    cpu_ticks = 0
    rss = 0
    pss = 0
    pss_seen = False
    read_bytes = 0
    write_bytes = 0
    rchar = 0
    wchar = 0
    blkio = 0
    threads = {}
    n_threads = 0
    for pid in pids:
        st = _proc_stat(pid)
        if st is None:
            continue
        cpu_ticks += st["utime"] + st["stime"]
        blkio += st["blkio_ticks"]
        if pid == root_pid:
            cpu_ticks += st["cutime"] + st["cstime"]   # 已 reap 的子进程 CPU 归属
        n_threads += st["num_threads"]
        v = _status_kb(pid, "VmRSS")
        if v is not None:
            rss += v * 1024
        p = _smaps_rollup(pid, "Pss")
        if p is not None:
            pss += p * 1024
            pss_seen = True
        io = _proc_io(pid)
        if io:
            read_bytes += io.get("read_bytes", 0)
            write_bytes += io.get("write_bytes", 0)
            rchar += io.get("rchar", 0)
            wchar += io.get("wchar", 0)
        for tid, ticks in _task_ticks(pid).items():
            threads[(pid, tid)] = ticks
    return {
        "pids": pids,
        "n_procs": len(pids),
        "cpu_ticks": cpu_ticks,
        "rss_bytes": rss,
        "pss_bytes": pss if pss_seen else None,
        "read_bytes": read_bytes,
        "write_bytes": write_bytes,
        "rchar": rchar,
        "wchar": wchar,
        "blkio_ticks": blkio,
        "threads": threads,
        "n_threads": n_threads,
    }


# ---------------------------------------------------------------- 已分配容量 ----
def allocated_capacity(explicit):
    """已分配容量(核)。优先显式 --capacity; 否则 resource_probe(effective cores)。"""
    if explicit and explicit > 0:
        return float(explicit), "explicit:--capacity"
    try:
        from tools.monitoring import resource_probe as rp
        info = rp.probe()
        return float(info["effective_cpu_cores"]), str(info.get("effective_source", "probe"))
    except Exception:
        try:
            return float(len(os.sched_getaffinity(0))), "sched_getaffinity"
        except AttributeError:
            return float(os.cpu_count() or 1), "cpu_count"


# ------------------------------------------------------------------ PNG 绘制 ----
class _Canvas:
    """极简 RGB 画布 + 自绘 PNG(纯 stdlib)。PIL 可用时另走高质量路径。"""

    def __init__(self, w, h, bg=(255, 255, 255)):
        self.w, self.h = w, h
        self.px = bytearray(bg * (w * h))

    def set(self, x, y, rgb):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 3
            self.px[i:i + 3] = bytes(rgb)

    def line(self, x0, y0, x1, y1, rgb):
        dx = abs(x1 - x0)
        dy = -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            self.set(x0, y0, rgb)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def png(self, path):
        raw = bytearray()
        stride = self.w * 3
        for y in range(self.h):
            raw.append(0)
            raw.extend(self.px[y * stride:(y + 1) * stride])

        def chunk(tag, data):
            body = tag + data
            return (struct.pack(">I", len(data)) + body +
                    struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

        ihdr = struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0)
        blob = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
                chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b""))
        Path(path).write_bytes(blob)


def _nice_max(v):
    if v <= 0:
        return 1.0
    exp = math.floor(math.log10(v))
    base = 10.0 ** exp
    for m in (1, 2, 2.5, 5, 10):
        if v <= m * base:
            return m * base
    return 10 * base


def plot_png(path, series, title, ylabel, xlabel="elapsed (s)"):
    """series: [(label, [(x, y), ...], (r,g,b)), ...]。PIL 可用则带文字标注。"""
    pts = [p for _l, xs, _c in series for p in xs]
    if not pts:
        return False
    xmax = max(1e-9, max(p[0] for p in pts))
    ymax = _nice_max(max(1e-9, max(p[1] for p in pts)))
    try:
        from PIL import Image, ImageDraw  # type: ignore
        W, H, M = 900, 420, 60
        img = Image.new("RGB", (W, H), (255, 255, 255))
        d = ImageDraw.Draw(img)
        d.rectangle([M, 20, W - 20, H - M], outline=(120, 120, 120))
        for i in range(6):
            xx = M + (W - 20 - M) * i / 5.0
            d.line([xx, 20, xx, H - M], fill=(230, 230, 230))
            d.text((xx - 12, H - M + 4), "%.3g" % (xmax * i / 5.0), fill=(60, 60, 60))
        for i in range(6):
            yy = H - M - (H - M - 20) * i / 5.0
            d.line([M, yy, W - 20, yy], fill=(230, 230, 230))
            d.text((4, yy - 6), "%.4g" % (ymax * i / 5.0), fill=(60, 60, 60))
        for label, xs, color in series:
            adj = [(M + (W - 20 - M) * (x / xmax), H - M - (H - M - 20) * (y / ymax))
                   for x, y in xs]
            if len(adj) > 1:
                d.line(adj, fill=color, width=2)
            for p in adj:
                d.ellipse([p[0] - 1, p[1] - 1, p[0] + 1, p[1] + 1], fill=color)
        d.text((M, 5), title, fill=(0, 0, 0))
        d.text((4, H - 18), ylabel, fill=(0, 0, 0))
        d.text((W - 150, H - 18), xlabel, fill=(0, 0, 0))
        lx = W - 230
        for j, (label, _xs, color) in enumerate(series):
            d.line([lx, 30 + j * 14, lx + 18, 30 + j * 14], fill=color, width=2)
            d.text((lx + 22, 24 + j * 14), label, fill=(40, 40, 40))
        img.save(path)
        return True
    except Exception:
        W, H, M = 900, 420, 60
        c = _Canvas(W, H)
        grid = (225, 225, 225)
        for i in range(6):
            xx = M + int((W - 20 - M) * i / 5.0)
            c.line(xx, 20, xx, H - M, grid)
            yy = H - M - int((H - M - 20) * i / 5.0)
            c.line(M, yy, W - 20, yy, grid)
        c.line(M, 20, M, H - M, (90, 90, 90))
        c.line(M, H - M, W - 20, H - M, (90, 90, 90))
        for _label, xs, color in series:
            adj = [(M + int((W - 20 - M) * (x / xmax)),
                    H - M - int((H - M - 20) * (y / ymax))) for x, y in xs]
            for a, b in zip(adj, adj[1:]):
                c.line(a[0], a[1], b[0], b[1], color)
        c.png(path)
        return True


# ------------------------------------------------------------------ 采样主体 ----
def _slope(points):
    """最小二乘斜率 (y 单位/秒)。points: [(t(s), y)]。"""
    n = len(points)
    if n < 2:
        return 0.0
    mt = sum(p[0] for p in points) / n
    my = sum(p[1] for p in points) / n
    num = sum((p[0] - mt) * (p[1] - my) for p in points)
    den = sum((p[0] - mt) ** 2 for p in points)
    return (num / den) if den > 0 else 0.0


def run_monitor(argv, out_dir, interval, capacity, capacity_src, timeout,
                duration, mem_window, pid_mode=False):
    """采样循环; 返回 (records, threads_rows, wall, exit_code, timed_out, cmd)。"""
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    records = []
    thread_rows = []
    started = time.monotonic()
    prev = None
    prev_t = None
    mem_hist = deque()
    proc = None
    root = None
    timed_out = False
    stop = {"flag": False}

    def _on_sigint(_sig, _frm):
        stop["flag"] = True
    old_int = signal.signal(signal.SIGINT, _on_sigint)
    try:
        if not pid_mode:
            proc = subprocess.Popen(argv, start_new_session=True)
            root = proc.pid
        else:
            root = int(argv[0])
        while True:
            now = time.monotonic()
            t = now - started
            snap = snapshot(root)
            rec = {"t": t}
            if prev is not None and prev_t is not None:
                dt = max(1e-9, t - prev_t)
                dticks = max(0, snap["cpu_ticks"] - prev["cpu_ticks"])
                # core = 单核百分比(1 核满载 = 100); cap = 已分配容量百分比
                # (capacity 核全忙 = 100) = core / capacity。
                core = (dticks / CLK_TCK) / dt * 100.0
                cap = (core / capacity) if capacity > 0 else 0.0
                busy = 0
                for key, ticks in snap["threads"].items():
                    old = prev["threads"].get(key, 0)
                    t_core = (ticks - old) / CLK_TCK / dt * 100.0
                    if t_core >= 5.0:
                        busy += 1
                        thread_rows.append({
                            "t": round(t, 4), "pid": key[0], "tid": key[1],
                            "cpu_pct_core": round(t_core, 3),
                            "cpu_pct_capacity": round(
                                t_core / capacity if capacity > 0 else 0.0, 3),
                        })
                mem_hist.append((t, snap["rss_bytes"]))
                while mem_hist and t - mem_hist[0][0] > mem_window:
                    mem_hist.popleft()
                growth = _slope(list(mem_hist)) / (1024.0 * 1024.0)
                rec.update({
                    "dt": dt,
                    "cpu_ticks_total": snap["cpu_ticks"],
                    "cpu_pct_core": core,
                    "cpu_pct_capacity": cap,
                    "effective_cores": core / 100.0,
                    "busy_threads": busy,
                    "n_threads": snap["n_threads"],
                    "n_procs": snap["n_procs"],
                    "rss_bytes": snap["rss_bytes"],
                    "pss_bytes": snap["pss_bytes"],
                    "rss_growth_mb_per_s": growth,
                    "read_bytes": snap["read_bytes"],
                    "write_bytes": snap["write_bytes"],
                    "read_bytes_per_s": max(0, snap["read_bytes"] - prev["read_bytes"]) / dt,
                    "write_bytes_per_s": max(0, snap["write_bytes"] - prev["write_bytes"]) / dt,
                    "io_wait_ms": max(0, snap["blkio_ticks"] - prev["blkio_ticks"])
                                  / CLK_TCK * 1000.0,
                })
            else:
                rec.update({
                    "dt": 0.0, "cpu_ticks_total": snap["cpu_ticks"], "cpu_pct_core": 0.0,
                    "cpu_pct_capacity": 0.0, "effective_cores": 0.0, "busy_threads": 0,
                    "n_threads": snap["n_threads"],
                    "n_procs": snap["n_procs"], "rss_bytes": snap["rss_bytes"],
                    "pss_bytes": snap["pss_bytes"], "rss_growth_mb_per_s": 0.0,
                    "read_bytes": snap["read_bytes"], "write_bytes": snap["write_bytes"],
                    "read_bytes_per_s": 0.0, "write_bytes_per_s": 0.0, "io_wait_ms": 0.0,
                })
            rec["iso_utc"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%fZ")
            records.append(rec)
            prev = snap
            prev_t = t
            # 终止条件
            if duration and t >= duration:
                break
            if timeout and t >= timeout:
                timed_out = True
                break
            if stop["flag"]:
                break
            # 进程树已空 → 结束(含被 PID 空间复用前的最后取样); 被测进程已退出且无
            # 后代时 snapshot 的 n_procs 为 0。
            if snap["n_procs"] == 0:
                break
            if not pid_mode and proc is not None and proc.poll() is not None:
                break   # 被测进程已退出, 末尾样本已捕获; 后代由其自身进程标识, 不再追。
            nxt = started + (len(records)) * interval
            sleep = nxt - time.monotonic()
            if sleep > 0:
                time.sleep(min(sleep, interval))
    finally:
        signal.signal(signal.SIGINT, old_int)
        exit_code = None
        if timed_out and proc is not None:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            except OSError:
                pass
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                except OSError:
                    pass
                proc.wait(timeout=10)
            exit_code = 124
        if proc is not None:
            if proc.poll() is None:
                try:
                    proc.wait(timeout=timeout or 3600)
                except subprocess.TimeoutExpired:
                    pass
            if exit_code is None:
                rc = proc.poll()
                if rc is not None:
                    exit_code = 128 + (-rc) if rc < 0 else rc
        if exit_code is None:
            exit_code = 0 if not pid_mode else 0
        wall = time.monotonic() - started
    return records, thread_rows, wall, exit_code, timed_out, argv


# ------------------------------------------------------------------ 事实汇总 ----
def _frac(values, q):
    if not values:
        return None
    sv = sorted(values)
    if len(sv) == 1:
        return sv[0]
    pos = q * (len(sv) - 1)
    lo = int(math.floor(pos))
    hi = min(lo + 1, len(sv) - 1)
    return sv[lo] + (sv[hi] - sv[lo]) * (pos - lo)


def facts(records, capacity, low_util_percent, max_low_util_run,
          single_thread_cores_max=DEFAULT_SINGLE_THREAD_CORES_MAX,
          min_busy_threads=DEFAULT_MIN_BUSY_THREADS):
    """从曲线记录提取**事实**(不做裁决)。"""
    cpu = [r["cpu_pct_capacity"] for r in records if r["dt"] > 0]
    core = [r["cpu_pct_core"] for r in records if r["dt"] > 0]
    busy = [r["busy_threads"] for r in records if r["dt"] > 0]
    growth = [r["rss_growth_mb_per_s"] for r in records if r["dt"] > 0]
    runs = []
    cur = None
    for r in records:
        if r["dt"] <= 0:
            continue
        if r["cpu_pct_capacity"] < low_util_percent:
            if cur is None:
                cur = {"start_s": r["t"], "end_s": r["t"], "n": 1, "sum": r["cpu_pct_capacity"]}
            else:
                cur["end_s"] = r["t"]
                cur["n"] += 1
                cur["sum"] += r["cpu_pct_capacity"]
        else:
            if cur is not None:
                runs.append(cur)
                cur = None
    if cur is not None:
        runs.append(cur)
    low_runs = [{"start_s": round(x["start_s"], 3), "end_s": round(x["end_s"], 3),
                 "duration_s": round(x["end_s"] - x["start_s"], 3),
                 "mean_cpu_pct_capacity": round(x["sum"] / x["n"], 3)}
                for x in runs
                if (x["end_s"] - x["start_s"]) >= max_low_util_run]
    # 单忙计算线程连续段(事实): 以**有效核数(等效核) <= 阈值**判定, 不数"有活动的
    # 线程数" —— GIL/短抢占会让多线程都动过, 但真实并行宽度仍是 1 核。
    single_runs = []
    cur = None
    for r in records:
        if r["dt"] <= 0:
            continue
        if r["effective_cores"] <= single_thread_cores_max:
            if cur is None:
                cur = {"start_s": r["t"], "end_s": r["t"]}
            else:
                cur["end_s"] = r["t"]
        else:
            if cur is not None:
                single_runs.append(cur)
                cur = None
    if cur is not None:
        single_runs.append(cur)
    single_ge = [x for x in single_runs if (x["end_s"] - x["start_s"]) >= max_low_util_run]
    # I/O 累计: 用逐区间**非负增量**积分, 而不是"末样本-首样本" —— 进程树成员
    # (子进程退出/被 reap) 变化会让树内 /proc/<pid>/io 合计下降, 直接相减会得到
    # 负的"累计读字节"。逐区间取正增量再按 dt 积分可稳健反映实际观测 I/O。
    total_read = sum(max(0.0, r["read_bytes_per_s"]) * r["dt"] for r in records)
    total_write = sum(max(0.0, r["write_bytes_per_s"]) * r["dt"] for r in records)
    io_wait = sum(r["io_wait_ms"] for r in records)
    mean_cap = statistics.fmean(cpu) if cpu else 0.0
    wall = records[-1]["t"] - records[0]["t"] if len(records) > 1 else 0.0
    return {
        "n_samples": len(records),
        "wall_seconds": round(wall, 4),
        "cpu_pct_capacity": {
            "mean": round(mean_cap, 3),
            "peak": round(max(cpu), 3) if cpu else 0.0,
            "p50": round(_frac(cpu, 0.50), 3) if cpu else 0.0,
            "p05": round(_frac(cpu, 0.05), 3) if cpu else 0.0,
            # G-RES-01 对齐事实（契约 compute.per_sample_*）：单样本 >=85% 的
            # 占比（阈 0.70）与判据方向，供与冻结门逐字段对拍。
            "sample_pass_fraction": (
                round(sum(1 for x in cpu
                          if x >= DEFAULT_MIN_MEAN_CAPACITY_PERCENT)
                      / len(cpu), 4) if cpu else 0.0),
            "per_sample_min_percent": DEFAULT_MIN_MEAN_CAPACITY_PERCENT,
            "pass_fraction_min": float(
                _GATE_COMPUTE["per_sample_pass_fraction_min"]),
        },
        "cpu_pct_core": {
            "mean": round(statistics.fmean(core), 3) if core else 0.0,
            "peak": round(max(core), 3) if core else 0.0,
        },
        "effective_cores": {
            "mean": round(statistics.fmean(core) / 100.0, 4) if core else 0.0,
            "peak": round(max(core) / 100.0, 4) if core else 0.0,
            "p50": round((_frac(core, 0.50) or 0.0) / 100.0, 4) if core else 0.0,
        },
        "work_core_seconds": round(mean_cap / 100.0 * capacity * wall, 4),
        "busy_threads": {
            "p50": _frac(busy, 0.50), "max": max(busy) if busy else 0,
            "min": min(busy) if busy else 0,
        },
        "memory_growth_mb_per_s": {
            "peak": round(max(growth), 4) if growth else 0.0,
            "mean": round(statistics.fmean(growth), 4) if growth else 0.0,
        },
        "io": {
            "read_bytes": total_read, "write_bytes": total_write,
            "io_wait_ms": round(io_wait, 3),
        },
        "facts": {
            "low_util_continuous_runs_ge_threshold": low_runs,
            "has_low_util_continuous_run_ge_threshold": bool(low_runs),
            "single_busy_thread": bool(single_ge),
            "single_busy_thread_runs_ge_threshold": [
                {"start_s": round(x["start_s"], 3), "end_s": round(x["end_s"], 3)}
                for x in single_ge],
        },
    }


def judge(summary, thresholds):
    """按可配置阈值给出建议。返回 dict(verdict/checks/notes)。"""
    cpu = summary["cpu_pct_capacity"]
    f = summary["facts"]
    wall = summary["wall_seconds"]
    core_seconds = summary["work_core_seconds"]
    checks = []
    if wall < 1.0 or summary["n_samples"] < 2:
        return {"verdict": "INSUFFICIENT", "reason": "too_few_samples",
                "work_core_seconds": core_seconds,
                "workload_floor_core_seconds": thresholds["min_core_seconds"],
                "checks": checks}
    if core_seconds < thresholds["min_core_seconds"]:
        return {"verdict": "INSUFFICIENT", "reason": "below_workload_floor",
                "work_core_seconds": core_seconds,
                "workload_floor_core_seconds": thresholds["min_core_seconds"],
                "checks": checks,
                "note": "低于工作量下限 → 不做利用率裁决(只记录); 见负责人 2.A"}
    fails = []
    warns = []
    if cpu["mean"] < thresholds["min_mean_capacity_percent"]:
        fails.append("mean_utilization_below_min")
    elif cpu["mean"] < thresholds["min_mean_capacity_percent"] + 5.0:
        warns.append("mean_utilization_near_min")
    if f["has_low_util_continuous_run_ge_threshold"]:
        fails.append("continuous_low_utilization_ge_window")
    # "只有一个活跃计算线程": 以运行期平均有效核数判定(仅在已分配容量>=2 时有意义)。
    if float(summary.get("capacity_cores", 0) or 0) >= 2 and \
            summary["effective_cores"]["mean"] < thresholds["min_effective_cores"]:
        fails.append("single_busy_compute_thread")
    # G-RES-01 对齐：单位 MiB/s（1048576 B/s），方向 >=（契约 memory.growth_predicate）。
    # 旧实现用严格大于 + 注释写 MB/s，与 C++ 侧 >= / MiB/s 分歧（R-4 E6：口径差 4.858%）。
    _mem_limit = thresholds.get("max_mem_growth_mib_per_s",
                                thresholds.get("max_mem_growth_mb_per_s"))
    _mem_peak = summary["memory_growth_mb_per_s"]["peak"]
    if _mem_peak >= _mem_limit:
        fails.append("unbounded_memory_growth")
    if summary["io"]["io_wait_ms"] / max(1.0, wall * 1000.0) * 100.0 > \
            thresholds["max_io_wait_percent"]:
        warns.append("io_wait_high")
    verdict = "FAIL" if fails else ("WARN" if warns else "PASS")
    return {"verdict": verdict, "reason": "thresholds",
            "work_core_seconds": core_seconds,
            "workload_floor_core_seconds": thresholds["min_core_seconds"],
            "failed_checks": fails, "warnings": warns, "checks": checks,
            "thresholds": thresholds}


# ------------------------------------------------------------------ 产物落盘 ----
CSV_COLUMNS = ["t", "iso_utc", "dt", "cpu_ticks_total", "cpu_pct_core",
               "cpu_pct_capacity", "effective_cores", "busy_threads", "n_threads", "n_procs",
               "rss_bytes", "pss_bytes", "rss_growth_mb_per_s", "read_bytes",
               "write_bytes", "read_bytes_per_s", "write_bytes_per_s", "io_wait_ms"]


def write_csv(path, rows, columns):
    with open(path, "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=columns)
        w.writeheader()
        for r in rows:
            w.writerow({c: r.get(c) for c in columns})


def write_curves(out, records):
    ts = [r["t"] for r in records]
    cpu = [(r["t"], r["cpu_pct_capacity"]) for r in records]
    busy = [(r["t"], r["busy_threads"] * 100.0 / max(1, int(records[-1]["n_threads"]) or 1))
            for r in records]
    plot_png(out / "curve_cpu.png",
             [("cpu % of capacity", cpu, (200, 30, 30)),
              ("busy threads (scaled)", busy, (30, 90, 220))],
             "AstroCS resource curve: CPU (capacity-relative)", "%")
    mem = [(r["t"], r["rss_bytes"] / (1024.0 * 1024.0)) for r in records]
    pss = [(r["t"], (r["pss_bytes"] or 0) / (1024.0 * 1024.0)) for r in records]
    plot_png(out / "curve_mem.png",
             [("RSS (MB)", mem, (30, 130, 60)), ("PSS (MB)", pss, (150, 90, 200))],
             "AstroCS resource curve: memory", "MB")
    rb = [(r["t"], r["read_bytes"] / 1e6) for r in records]
    wb = [(r["t"], r["write_bytes"] / 1e6) for r in records]
    plot_png(out / "curve_io.png",
             [("read (MB cumulative)", rb, (200, 120, 20)),
              ("write (MB cumulative)", wb, (20, 120, 160))],
             "AstroCS resource curve: IO", "MB")


# ---------------------------------------------------------------------- CLI ----
def _thresholds(args):
    return {
        "min_mean_capacity_percent": args.min_mean_capacity_percent,
        "low_util_percent": args.low_util_percent,
        "max_low_util_run_seconds": args.max_low_util_run_seconds,
        "min_core_seconds": args.min_core_seconds,
        # 单位 MiB/s（1048576 B/s）；旧键名 max_mem_growth_mb_per_s 保留兼容。
        "max_mem_growth_mib_per_s": args.max_mem_growth_mib_per_s,
        "max_mem_growth_mb_per_s": args.max_mem_growth_mib_per_s,
        "min_busy_threads": args.min_busy_threads,
        "min_effective_cores": args.min_effective_cores,
        "single_thread_cores_max": args.single_thread_cores_max,
        "max_io_wait_percent": args.max_io_wait_percent,
    }


def _load_records_from(out_dir):
    out = Path(out_dir)
    recs = []
    with open(out / "samples.csv", newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            r = {}
            for k, v in row.items():
                if v in ("", "None", None):
                    r[k] = None
                elif k == "iso_utc":
                    r[k] = v
                else:
                    try:
                        r[k] = float(v) if ("." in v or "e" in v or "E" in v) else int(v)
                    except ValueError:
                        r[k] = v
            recs.append(r)
    return recs


def build_parser():
    ap = argparse.ArgumentParser(description="AstroCS 外挂资源监控 + 裁决建议(P26 T1)")
    ap.add_argument("--out", default="run/resource/mon", help="产物目录")
    ap.add_argument("--pid", type=int, default=None, help="挂到已在运行的进程(进程树根)")
    ap.add_argument("--interval", type=float, default=1.0, help="采样周期秒(默认 1Hz)")
    ap.add_argument("--capacity", type=float, default=None, help="已分配容量(核); 默认探测")
    ap.add_argument("--timeout", type=float, default=None, help="墙钟超时秒(超时 124)")
    ap.add_argument("--duration", type=float, default=None, help="采样时长上限秒")
    ap.add_argument("--mem-window", type=float, default=DEFAULT_MEM_WINDOW_SECONDS,
                    help="内存增长率滑动窗口秒")
    ap.add_argument("--judge", action="store_true", help="给出 PASS/FAIL/WARN 建议")
    ap.add_argument("--from", dest="from_dir", default=None,
                    help="--judge 复用既有产物目录(不重新采样)")
    ap.add_argument("--judge-exit-code", action="store_true",
                    help="退出码按建议(0/3); 默认透传被测命令退出码")
    ap.add_argument("--min-mean-capacity-percent", type=float,
                    default=DEFAULT_MIN_MEAN_CAPACITY_PERCENT)
    ap.add_argument("--low-util-percent", type=float, default=DEFAULT_LOW_UTIL_PERCENT)
    ap.add_argument("--max-low-util-run-seconds", type=float,
                    default=DEFAULT_MAX_LOW_UTIL_RUN_SECONDS)
    ap.add_argument("--min-core-seconds", type=float, default=DEFAULT_MIN_CORE_SECONDS)
    # 单位 MiB/s（1048576 B/s）；--max-mem-growth-mb-per-s 保留为兼容别名。
    ap.add_argument("--max-mem-growth-mib-per-s", "--max-mem-growth-mb-per-s",
                    dest="max_mem_growth_mib_per_s", type=float,
                    default=DEFAULT_MAX_MEM_GROWTH_MIB_PER_S)
    ap.add_argument("--min-busy-threads", type=int, default=DEFAULT_MIN_BUSY_THREADS)
    ap.add_argument("--min-effective-cores", type=float, default=DEFAULT_MIN_EFFECTIVE_CORES)
    ap.add_argument("--single-thread-cores-max", type=float,
                    default=DEFAULT_SINGLE_THREAD_CORES_MAX)
    ap.add_argument("--max-io-wait-percent", type=float, default=50.0)
    ap.add_argument("cmd", nargs=argparse.REMAINDER, help="-- <命令...>")
    return ap


def main(argv=None):
    args = build_parser().parse_args(argv)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    thresholds = _thresholds(args)

    if args.judge and args.from_dir:
        recs = _load_records_from(args.from_dir)
        cap = args.capacity or 1.0
        summary_path = Path(args.from_dir) / "summary.json"
        if summary_path.is_file():
            cap = json.loads(summary_path.read_text(encoding="utf-8")).get("capacity_cores", cap)
        summ = facts(recs, cap, args.low_util_percent, args.max_low_util_run_seconds,
                     args.single_thread_cores_max, args.min_busy_threads)
        summ["capacity_cores"] = cap
        j = judge(summ, thresholds)
        summ["judge"] = j
        (out / "judge.json").write_text(json.dumps(j, ensure_ascii=False, indent=2) + "\n",
                                        encoding="utf-8")
        print("JUDGE: %s (%s) work_core_seconds=%.3f floor=%.3f"
              % (j["verdict"], j.get("reason"), summ["work_core_seconds"],
                 thresholds["min_core_seconds"]))
        return 0 if (j["verdict"] != "FAIL" or not args.judge_exit_code) else 3

    cmd = list(args.cmd)
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]
    if args.pid is None and not cmd:
        print("error: need a command after -- or --pid <pid>", file=sys.stderr)
        return 2
    cap, cap_src = allocated_capacity(args.capacity)
    records, thread_rows, wall, exit_code, timed_out, argv_used = run_monitor(
        [str(args.pid)] if args.pid is not None else cmd,
        out, args.interval, cap, cap_src, args.timeout, args.duration,
        args.mem_window, pid_mode=args.pid is not None)
    write_csv(out / "samples.csv", records, CSV_COLUMNS)
    write_csv(out / "threads.csv", thread_rows,
              ["t", "pid", "tid", "cpu_pct_core", "cpu_pct_capacity"])
    write_curves(out, records)
    summ = facts(records, cap, args.low_util_percent, args.max_low_util_run_seconds,
                 args.single_thread_cores_max, args.min_busy_threads)
    summ.update({
        "schema_version": SCHEMA_VERSION,
        "tool": "eng/tools/quality/resource_monitor.py",
        "mode": "attached" if args.pid is not None else "spawned",
        "command": cmd if args.pid is None else ["--pid", str(args.pid)],
        "interval_seconds": args.interval,
        "capacity_cores": cap,
        "capacity_source": cap_src,
        "wall_seconds_total": round(wall, 4),
        "exit_code": exit_code,
        "timed_out": timed_out,
        "products": ["samples.csv", "threads.csv", "summary.json",
                     "curve_cpu.png", "curve_mem.png", "curve_io.png"],
    })
    if args.judge:
        summ["judge"] = judge(summ, thresholds)
        (out / "judge.json").write_text(
            json.dumps(summ["judge"], ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (out / "summary.json").write_text(json.dumps(summ, ensure_ascii=False, indent=2) + "\n",
                                      encoding="utf-8")
    print("[resource_monitor] out=%s samples=%d wall=%.2fs capacity=%.2f exit=%s%s"
          % (out, len(records), wall, cap, exit_code, " timed_out" if timed_out else ""))
    if args.judge:
        j = summ["judge"]
        print("JUDGE: %s (%s) work_core_seconds=%.3f floor=%.3f failed=%s"
              % (j["verdict"], j.get("reason"), summ["work_core_seconds"],
                 thresholds["min_core_seconds"], j.get("failed_checks")))
    if args.judge_exit_code:
        return 0 if summ.get("judge", {}).get("verdict") != "FAIL" else 3
    return exit_code if exit_code is not None else 0


if __name__ == "__main__":
    sys.exit(main())
