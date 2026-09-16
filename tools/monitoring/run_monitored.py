# -*- coding: utf-8 -*-
"""V8-CI-003 heavy wrapper（owner=SA-CI-32，纯 stdlib，Linux /proc）。

职责（tasks/02_CI_TASKS.md V8-CI-003）：
  以 argv 数组启动子进程（subprocess，永不 shell=True），按 poll_interval
  （默认 0.2s，可参数化）采样并输出 JSON 证据文件：真实进程/线程 CPU、
  RSS/PSS、IO、threads、progress；支持 --timeout：超时 kill 进程组
  （start_new_session=True + os.killpg），timed_out=true，exit_code 按实际。

progress 约定（两种途径均实现）：
  1) 被监控进程向 stdout 打印形如 ``PROGRESS: <done>/<total>`` 的行
     （取最后一条）；wrapper 以行缓冲读取，不会被子进程写满管道阻塞。
  2) ``--progress-file PATH``：wrapper 把绝对路径经环境变量
     ``ASTROCS_PROGRESS_FILE`` 传给被监控进程；进程按行向该文件写进度
     （每行 ``PROGRESS: <done>/<total>`` 或裸 ``<done>/<total>``，
     取最后一条有效行，进程退出后重读一次）。
  两种途径都有结果时以 progress-file 为准（source 字段标注出处）。
  无任何进度 → progress 字段为 None。

CPU% 口径：进程树（/proc/<pid>/task/* 子线程聚合 + /proc/<pid>/task/*/children
递归子进程）utime+stime（+父进程 cutime+cstime 覆盖已 reap 子进程）在相邻两次
采样间的差分，除以墙钟差与 CLK_TCK → 单核满载 = 100%。RSS/PSS 为树内进程
（/proc/<pid>/status VmRSS、/proc/<pid>/smaps_rollup Pss，PSS 不可得置 None）
求和的采样值；peak 为采样峰值，rss_start_kb 为首个采样（evaluate 的增长基线）。
IO 为 /proc/<pid>/io rchar/wchar（缺失回退 read_bytes/write_bytes）父子树累计
（对已退出子进程保留其最后一次观测值，保证不丢累计）。
threads_max 为树内线程数总和的采样峰值。

用法：
    python3 tools/monitoring/run_monitored.py [--timeout S] [--poll-interval S]
        [--output FILE] [--progress-file FILE] -- <cmd> [args...]

CLI 退出码：timed_out → 124；子进程被信号杀死 → 128+signum；否则透传子进程
退出码。库函数 run_monitored(argv, ...) 返回结果 dict（evaluate() 做阈值判定）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import signal
import statistics
import subprocess
import sys
import threading
import time
from collections import deque
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

try:  # 库导入（ci/tests 经 namespace package 导入）
    from tools.monitoring import resource_probe as _rp
except ImportError:  # 脚本直跑（sys.path[0] = 本目录）
    import resource_probe as _rp  # type: ignore[no-redef]

__all__ = ["run_monitored", "evaluate", "evaluate_frozen_gate",
           "build_arg_parser", "parse_progress_line", "main",
           "load_resource_gate_contract", "RESOURCE_GATE_CONTRACT",
           "resolve_allocated_capacity",
           "FROZEN_GATE_MIN_EFFECTIVE_CPUS", "FROZEN_GATE_MIN_INTERVAL_SECONDS",
           "FROZEN_GATE_MIN_AVG_UTILIZATION", "FROZEN_GATE_MIN_P50_UTILIZATION",
           "FROZEN_GATE_MIN_SAMPLE_UTILIZATION",
           "FROZEN_GATE_MIN_SAMPLE_PASS_FRACTION", "FROZEN_GATE_WINDOW_SECONDS",
           "FROZEN_GATE_WINDOW_MIN_UTILIZATION", "FROZEN_GATE_MIN_ACTIVE_THREADS",
           "FROZEN_GATE_MIN_QUEUED_THREADS",
           "FROZEN_GATE_WORKLOAD_FLOOR_CORE_SECONDS"]

try:
    CLK_TCK = float(os.sysconf("SC_CLK_TCK"))
except (ValueError, OSError, AttributeError):  # 非 Linux 容错
    CLK_TCK = 100.0

PROGRESS_RE = re.compile(r"^\s*PROGRESS:\s*(\d+)\s*/\s*(\d+)\s*$")
PLAIN_PROGRESS_RE = re.compile(r"^\s*(\d+)\s*/\s*(\d+)\s*$")

STDOUT_TAIL_LINES = 400  # stdout/stderr 只保留尾部行，防长跑撑爆内存


def parse_progress_line(line: str) -> Optional[dict]:
    """解析一行进度（"PROGRESS: d/t" 或裸 "d/t"）→ {"done","total"}；无效 → None。"""
    if not line:
        return None
    m = PROGRESS_RE.match(line) or PLAIN_PROGRESS_RE.match(line)
    if not m:
        return None
    return {"done": int(m.group(1)), "total": int(m.group(2))}


# ------------------------------------------------------------ /proc 读取 ----
def _read_text(path) -> Optional[str]:
    try:
        return Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None


def _stat_cpu_ticks(pid: int, proc_root: Path) -> Optional[int]:
    """/proc/<pid>/stat 的 utime+stime+cutime+cstime（时钟 tick；进程消失 → None）。"""
    text = _read_text(proc_root / str(pid) / "stat")
    if not text:
        return None
    rest = text.rsplit(")", 1)[1]  # comm 可能含空格/括号，从最后一个 ')' 之后解析
    fields = rest.split()
    if len(fields) < 15:  # state..cstime（field17 → rest 索引 14）
        return None
    try:
        return (int(fields[11]) + int(fields[12])
                + int(fields[13]) + int(fields[14]))
    except ValueError:
        return None


def _task_children(pid: int, proc_root: Path) -> list[int]:
    """/proc/<pid>/task/<tid>/children → 直接子进程 pid 列表（读不到 → []）。"""
    task_dir = proc_root / str(pid) / "task"
    try:
        tids = sorted(os.listdir(task_dir))
    except OSError:
        return []
    for tid in tids:
        text = _read_text(task_dir / tid / "children")
        if text is None:
            continue
        out = []
        for tok in text.split():
            try:
                out.append(int(tok))
            except ValueError:
                continue
        return out
    return []


def _tree_pids(root_pid: int, proc_root: Path) -> list[int]:
    """BFS 收集进程树（含子孙）；visited 防环，读不到子进程文件即止。"""
    seen: set[int] = {root_pid}
    order = [root_pid]
    queue = deque([root_pid])
    while queue:
        pid = queue.popleft()
        for child in _task_children(pid, proc_root):
            if child not in seen:
                seen.add(child)
                order.append(child)
                queue.append(child)
    return order


def _tree_threads(pid: int, proc_root: Path) -> int:
    """进程线程数 = /proc/<pid>/task/ 目录数（不可得 → 0）。"""
    try:
        return len(os.listdir(proc_root / str(pid) / "task"))
    except OSError:
        return 0


def _thread_state(tid_dir: Path) -> Optional[str]:
    """/proc/<pid>/task/<tid>/stat 的第 3 字段（state 单字符）；不可得 → None。"""
    text = _read_text(tid_dir / "stat")
    if not text:
        return None
    rest = text.rsplit(")", 1)[-1].split()   # comm 可能含空格/括号
    return rest[0] if rest else None


def _tree_runnable_threads(pid: int, proc_root: Path) -> int:
    """就绪/运行态（state=R）线程数 —— 队列有工作（CPU 饥饿）的证据面。

    G-RES-01 的「连续低利用窗」判据要求「**且队列有工作**」（契约
    compute.queue_low_window_requires_queued_work）：只有存在就绪线程却拿不到
    CPU 才是 CPU 饥饿；纯 I/O 等待（state=S/D）不得判红。Linux 的 R 态不区分
    「正在跑」与「已就绪待调度」，故判据用**窗内 R 态线程数中位数 ≥ 2 且利用率
    < 60%**：若两个就绪线程都在跑，利用率不可能低于 60%。不可得 → 0。
    """
    task_dir = proc_root / str(pid) / "task"
    try:
        tids = os.listdir(task_dir)
    except OSError:
        return 0
    n = 0
    for tid in tids:
        if _thread_state(task_dir / tid) == "R":
            n += 1
    return n


def _status_rss_kb(pid: int, proc_root: Path) -> Optional[int]:
    """/proc/<pid>/status VmRSS → kB（不可得 → None）。"""
    text = _read_text(proc_root / str(pid) / "status")
    if not text:
        return None
    for line in text.splitlines():
        if line.startswith("VmRSS:"):
            fields = line.split()
            if len(fields) >= 2:
                try:
                    return int(fields[1])
                except ValueError:
                    return None
    return None


def _pss_kb(pid: int, proc_root: Path) -> Optional[int]:
    """/proc/<pid>/smaps_rollup Pss → kB（不可得/内核不支持 → None）。"""
    text = _read_text(proc_root / str(pid) / "smaps_rollup")
    if not text:
        return None
    for line in text.splitlines():
        if line.startswith("Pss:"):
            fields = line.split()
            if len(fields) >= 2:
                try:
                    return int(fields[1])
                except ValueError:
                    return None
    return None


def _io_bytes(pid: int, proc_root: Path) -> tuple[Optional[int], Optional[int]]:
    """/proc/<pid>/io → (rchar, wchar)；缺失回退 read_bytes/write_bytes（不可得 → None）。"""
    text = _read_text(proc_root / str(pid) / "io")
    if not text:
        return None, None
    values: dict[str, int] = {}
    for line in text.splitlines():
        key, sep, value = line.partition(":")
        if not sep:
            continue
        try:
            values[key.strip()] = int(value.strip())
        except ValueError:
            continue
    rchar = values.get("rchar", values.get("read_bytes"))
    wchar = values.get("wchar", values.get("write_bytes"))
    return rchar, wchar


def _sample_tree(root_pid: int, proc_root: Path, io_seen: dict[int, list[int]]) -> dict:
    """对当前进程树做一次聚合采样。"""
    total_ticks = 0
    ticks_seen = False
    rss_sum = 0
    rss_seen = False
    pss_sum = 0
    pss_seen = False
    threads = 0
    runnable_threads = 0
    io_read_sum = 0
    io_write_sum = 0
    io_seen_now = False
    pids = _tree_pids(root_pid, proc_root)
    for pid in pids:
        ticks = _stat_cpu_ticks(pid, proc_root)
        if ticks is not None:
            total_ticks += ticks
            ticks_seen = True
        threads += _tree_threads(pid, proc_root)
        runnable_threads += _tree_runnable_threads(pid, proc_root)
        rss = _status_rss_kb(pid, proc_root)
        if rss is not None:
            rss_sum += rss
            rss_seen = True
        pss = _pss_kb(pid, proc_root)
        if pss is not None:
            pss_sum += pss
            pss_seen = True
        r, w = _io_bytes(pid, proc_root)
        if r is not None or w is not None:
            io_seen_now = True
            slot = io_seen.setdefault(pid, [0, 0])
            if r is not None:
                slot[0] = max(slot[0], r)
            if w is not None:
                slot[1] = max(slot[1], w)
            io_read_sum += slot[0]
            io_write_sum += slot[1]
    return {
        "pids": len(pids),
        "cpu_ticks": total_ticks if ticks_seen else None,
        "rss_kb": rss_sum if rss_seen else None,
        "pss_kb": pss_sum if pss_seen else None,
        "threads": threads,
        # G-RES-01 队列证据：R 态（就绪/运行）线程数; 0 是合法观测值（无就绪竞争）。
        "runnable": runnable_threads,
        "io_read_bytes": io_read_sum if io_seen_now else None,
        "io_write_bytes": io_write_sum if io_seen_now else None,
    }


def _read_progress_file(path) -> Optional[dict]:
    """读 progress 文件最后一条有效行 → progress dict（文件/行无效 → None）。"""
    text = _read_text(path)
    if not text:
        return None
    for line in reversed(text.splitlines()):
        parsed = parse_progress_line(line)
        if parsed is not None:
            return parsed
    return None


# ------------------------------------------------------------ 监控主函数 ----
def run_monitored(argv: list[str], *, timeout: Optional[float] = None,
                  poll_interval: float = 0.2, output=None,
                  progress_file=None, cwd=None, env=None,
                  proc_root="/proc") -> dict:
    """以 argv 数组启动子进程并按 poll_interval 采样；返回证据 dict。

    argv 必须是数组（永不 shell=True）；timeout 秒后 SIGKILL 整个进程组；
    output 非空时把 JSON 证据写到该文件。子进程环境注入 PYTHONUNBUFFERED=1
    与（给了 progress_file 时）ASTROCS_PROGRESS_FILE=<绝对路径>。
    """
    if not argv:
        raise ValueError("argv 不能为空")
    proc_root = Path(proc_root)
    io_seen: dict[int, list[int]] = {}

    child_env = dict(env) if env is not None else dict(os.environ)
    child_env.setdefault("PYTHONUNBUFFERED", "1")
    if progress_file is not None:
        child_env["ASTROCS_PROGRESS_FILE"] = str(Path(progress_file).resolve())

    t0 = time.monotonic()
    started_utc = datetime.now(timezone.utc).isoformat()
    proc = subprocess.Popen(
        list(argv),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        start_new_session=True,   # 子进程成为会话/进程组首 → 可 killpg
        text=True,
        bufsize=1,
        errors="replace",
        env=child_env,
        cwd=None if cwd is None else str(cwd),
    )

    stdout_tail: deque[str] = deque(maxlen=STDOUT_TAIL_LINES)
    stderr_tail: deque[str] = deque(maxlen=STDOUT_TAIL_LINES)
    progress_holder: dict[str, Optional[dict]] = {"progress": None}

    def _pump_stdout() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            line = line.rstrip("\r\n")
            if line:
                stdout_tail.append(line)
            parsed = parse_progress_line(line)
            if parsed is not None:
                progress_holder["progress"] = {
                    "done": parsed["done"], "total": parsed["total"],
                    "raw": line.strip(), "source": "stdout",
                }

    def _pump_stderr() -> None:
        assert proc.stderr is not None
        for line in proc.stderr:
            line = line.rstrip("\r\n")
            if line:
                stderr_tail.append(line)

    threads = [
        threading.Thread(target=_pump_stdout, daemon=True),
        threading.Thread(target=_pump_stderr, daemon=True),
    ]
    for th in threads:
        th.start()

    timed_out = False
    killed = False
    deadline = (t0 + float(timeout)) if timeout is not None else None
    prev_sample: Optional[tuple[float, int]] = None  # (monotonic, cpu_ticks)
    samples: list[dict] = []

    def _take_sample() -> None:
        nonlocal prev_sample
        snap = _sample_tree(proc.pid, proc_root, io_seen)
        now = time.monotonic()
        cpu_percent: Optional[float] = None
        if prev_sample is not None and snap["cpu_ticks"] is not None:
            dt = now - prev_sample[0]
            dticks = snap["cpu_ticks"] - prev_sample[1]
            if dt > 0 and dticks >= 0:
                cpu_percent = dticks / (dt * CLK_TCK) * 100.0
        if snap["cpu_ticks"] is not None:
            prev_sample = (now, snap["cpu_ticks"])
        samples.append({
            "t": round(now - t0, 4),
            "cpu_percent": None if cpu_percent is None else round(cpu_percent, 2),
            "rss_kb": snap["rss_kb"],
            "pss_kb": snap["pss_kb"],
            "threads": snap["threads"],
            "runnable": snap["runnable"],
            "io_read_bytes": snap["io_read_bytes"],
            "io_write_bytes": snap["io_write_bytes"],
            "pids": snap["pids"],
        })

    while proc.poll() is None:
        _take_sample()
        if deadline is not None and time.monotonic() >= deadline:
            timed_out = True
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                killed = True
            except (ProcessLookupError, PermissionError, OSError):
                try:
                    proc.kill()
                    killed = True
                except OSError:
                    pass
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:  # 极端情况：强杀单进程兜底
                proc.kill()
                proc.wait(timeout=15)
            break
        if deadline is not None:
            remaining = deadline - time.monotonic()
            time.sleep(max(0.01, min(poll_interval, remaining)))
        else:
            time.sleep(poll_interval)

    exit_code = proc.wait(timeout=15)
    _take_sample()  # 终采样：覆盖退出前的最后一段时间
    for th in threads:
        th.join(timeout=5)
    for stream in (proc.stdout, proc.stderr):  # 显式关管道，避免 ResourceWarning
        if stream is not None:
            try:
                stream.close()
            except OSError:
                pass
    finished_utc = datetime.now(timezone.utc).isoformat()
    duration = time.monotonic() - t0

    # progress：stdout 记录优先级低于 progress-file（文档化约定）
    progress = progress_holder["progress"]
    if progress_file is not None:
        from_file = _read_progress_file(progress_file)
        if from_file is not None:
            progress = {"done": from_file["done"], "total": from_file["total"],
                        "raw": f"{from_file['done']}/{from_file['total']}",
                        "source": "progress_file"}

    cpu_values = [s["cpu_percent"] for s in samples if s["cpu_percent"] is not None]
    rss_values = [s["rss_kb"] for s in samples if s["rss_kb"] is not None]
    pss_values = [s["pss_kb"] for s in samples if s["pss_kb"] is not None]
    io_read_total = sum(v[0] for v in io_seen.values()) if io_seen else None
    io_write_total = sum(v[1] for v in io_seen.values()) if io_seen else None
    threads_max = max((s["threads"] for s in samples), default=0)

    result = {
        "command": list(argv),
        "exit_code": exit_code,
        "timed_out": timed_out,
        "killed": killed,
        "duration_seconds": round(duration, 3),
        "cpu_percent_avg": round(statistics.fmean(cpu_values), 2) if cpu_values else None,
        "cpu_percent_median": round(statistics.median(cpu_values), 2) if cpu_values else None,
        "cpu_samples": samples,
        "samples": len(samples),
        "peak_rss_kb": max(rss_values) if rss_values else None,
        "rss_start_kb": rss_values[0] if rss_values else None,
        "peak_pss_kb": max(pss_values) if pss_values else None,
        "io_read_bytes": io_read_total,
        "io_write_bytes": io_write_total,
        "threads_max": threads_max,
        "progress": progress,
        "stdout_tail": list(stdout_tail)[-STDOUT_TAIL_LINES:],
        "stderr_tail": list(stderr_tail)[-STDOUT_TAIL_LINES:],
        "poll_interval": poll_interval,
        "timeout_seconds": timeout,
        "started_utc": started_utc,
        "finished_utc": finished_utc,
        "host_probe": _rp.probe(proc_root=proc_root),
    }

    if output is not None:
        out_path = Path(output)
        if out_path.parent != Path(""):
            out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(result, ensure_ascii=False, indent=1),
                            encoding="utf-8")
    return result


# ------------------------------------------------------------ 阈值判定 ----
# ── G-RES-01 重计算负载资源门（判据权威: docs/plugins/infrastructure/
#    21_observability.md §8）──
# 阈值不在本文件发明：唯一数值源 = contracts/resource_gate_v1.json。
# 契约缺失/不可解析/schema 不符 → 立即 RuntimeError（fail-closed）：阈值没有
# 第二处来源，「静默回落内置默认值」等于把数值权威倒置回实现。
CONTRACT_PATH = Path(__file__).resolve().parents[2] / "contracts" / "resource_gate_v1.json"
CONTRACT_SCHEMA = "astrocs.resource-gate/v1"


def load_resource_gate_contract(path=None) -> dict:
    """读 G-RES-01 数值契约（唯一数值源）；不可得 → RuntimeError（不静默回落）。"""
    p = Path(path) if path is not None else CONTRACT_PATH
    try:
        with open(p, "r", encoding="utf-8") as fh:
            doc = json.load(fh)
    except OSError as exc:
        raise RuntimeError(f"资源门数值契约不可读: {p} ({exc})") from exc
    except ValueError as exc:
        raise RuntimeError(f"资源门数值契约非法 JSON: {p} ({exc})") from exc
    if doc.get("schema") != CONTRACT_SCHEMA:
        raise RuntimeError(
            f"资源门数值契约 schema 非 {CONTRACT_SCHEMA}: {p} "
            f"(实测 {doc.get('schema')!r})")
    return doc


RESOURCE_GATE_CONTRACT = load_resource_gate_contract()
_APPLICABILITY = RESOURCE_GATE_CONTRACT["applicability"]
_COMPUTE = RESOURCE_GATE_CONTRACT["compute"]
_GATE_DENOMINATOR = RESOURCE_GATE_CONTRACT["denominator"]

FROZEN_GATE_MIN_EFFECTIVE_CPUS = int(_APPLICABILITY["min_effective_cpus"])
# 计算区间须严格 >该值 才适用（NOT_APPLICABLE 是显式分类，不是豁免）。
FROZEN_GATE_MIN_INTERVAL_SECONDS = float(
    _APPLICABILITY["min_active_window_seconds_exclusive"])
FROZEN_GATE_MIN_AVG_UTILIZATION = float(_COMPUTE["mean_utilization_min_percent"]) / 100.0
FROZEN_GATE_MIN_P50_UTILIZATION = float(_COMPUTE["p50_utilization_min_percent"]) / 100.0
FROZEN_GATE_MIN_SAMPLE_UTILIZATION = float(
    _COMPUTE["per_sample_utilization_min_percent"]) / 100.0
FROZEN_GATE_MIN_SAMPLE_PASS_FRACTION = float(_COMPUTE["per_sample_pass_fraction_min"])
FROZEN_GATE_WINDOW_SECONDS = float(_COMPUTE["queue_low_window_seconds_min"])
FROZEN_GATE_WINDOW_MIN_UTILIZATION = float(
    _COMPUTE["queue_low_utilization_percent"]) / 100.0
FROZEN_GATE_MIN_ACTIVE_THREADS = int(_COMPUTE["min_active_compute_threads"])
FROZEN_GATE_MIN_QUEUED_THREADS = int(_COMPUTE["queued_work_min_runnable_threads"])
FROZEN_GATE_WORKLOAD_FLOOR_CORE_SECONDS = float(
    RESOURCE_GATE_CONTRACT["workload_floor_core_seconds"])
GATE_SENTINEL_UNOBSERVED = int(_GATE_DENOMINATOR["sentinel_unobserved"])
# record_and_justify 判据：必须记录 + 超标须登记，不改变退出码（契约 compute.*）。
FROZEN_GATE_RECORD_ONLY_UTILIZATION = True


def _allocated_source(allocated, selected_workers, granted_workers) -> str:
    """分母来源标签（审计用；与契约 denominator 同源）。"""
    def _pos(v):
        return isinstance(v, int) and not isinstance(v, bool) and v > 0

    if allocated <= 0:
        return "undeclared"
    if _pos(granted_workers):
        return "granted_workers"
    if _pos(selected_workers):
        return "selected_workers"
    return "caller_supplied"


def resolve_allocated_capacity(*, granted_workers, selected_workers, available_cpus):
    """已分配容量分母的唯一实现点（契约 denominator）。

    granted_workers  已授予并发租约峰值（观测/显式声明）；哨兵 0 = 未观测，
                     **不得**以配置值回填（include/astrocs/core/context.h:93-103）。
    selected_workers 配置选择的 worker 数；哨兵 0 = 未声明。
    available_cpus   机器有效核（affinity ∩ cgroup）。

    返回 int：granted>0 → min(granted, available)；否则回落
    min(selected, available)；两者皆为哨兵 → 0（利用率类判据不成立，
    显式记入 recorded，绝不拿机器有效核充当已分配容量）。
    """
    def _pos(v):
        return isinstance(v, int) and not isinstance(v, bool) and v > 0

    avail = available_cpus if _pos(available_cpus) else 0
    if _pos(granted_workers):
        return min(int(granted_workers), avail) if avail else int(granted_workers)
    if _pos(selected_workers):
        return min(int(selected_workers), avail) if avail else int(selected_workers)
    return 0


def evaluate_frozen_gate(result: dict, *, effective_cpus, allocated_workers=None,
                         compute_interval_seconds: Optional[float] = None,
                         require_progress: bool = False,
                         selected_workers=None, granted_workers=None) -> dict:
    """对 run_monitored 结果做 G-RES-01 重计算负载资源门判定（实测 fail-closed）。

    判据权威: docs/plugins/infrastructure/21_observability.md §8「重计算负载资源门
    （G-RES-01）」；阈值唯一数值源: contracts/resource_gate_v1.json（本函数不含
    任何字面量阈值）。

    effective_cpus   有效 CPU 数（affinity ∩ cgroup；None/非正 → fail-closed,
                     不得以机器总核或配置值冒充）
    allocated_workers 已分配容量分母（等效核）。调用方应经
                     resolve_allocated_capacity(granted_workers=..., selected_workers=...,
                     available_cpus=...) 解析；哨兵 0/None = 未声明 → 利用率类判据
                     **不成立**并记入 recorded（禁止以机器有效核冒充）；
                     负数/非整数 = 非法输入 → fail-closed。
    selected_workers / granted_workers 仅用于回显三分量（契约 denominator.
                     echo_components: selected_workers/available_cpus/granted_workers）
    compute_interval_seconds 计算区间时长；None → 取 duration_seconds（监控
                     面向计算段时同义；混合 run 由调用方显式给出计算区间）
    require_progress True 时 progress 缺失/零进度计入违规（默认 False——
                     progress 证据面由调用方按运行类型启用）

    返回 dict（确定性纯函数，不抛错）:
      verdict: "pass" | "fail" | "not_applicable"
      violations: **硬失败**清单（fail 时非空 → 退出码 10；契约 hard_fail_criteria）
      recorded:   record_and_justify 记录项（超标须登记，不改退出码；契约
                  compute.*_enforcement = record_and_justify）
      reason: not_applicable 时的显式分类说明（门禁不适用 ≠ 豁免,
              BASE-UTIL-001 分类口径）
      metrics: 实测值回显（三分量分母/avg/p50/逐样本占比/低利用窗/线程统计量）
    """
    metrics: dict = {}
    recorded: list[str] = []
    # ── 输入有效性: 无法判定适用性 → fail-closed（绝不 pass） ──
    cpus_ok = isinstance(effective_cpus, int) and not isinstance(
        effective_cpus, bool) and effective_cpus >= 1
    # allocated_workers 语义（契约 denominator）：已分配容量分母（等效核）。
    # None/0 = 未声明哨兵（利用率类判据不成立 → recorded；**禁止**以机器有效核
    # 冒充 —— 旧实现把 --gate-required 的机器有效核当已分配容量，导致任何
    # worker 数 < 机器核数的并行任务结构性判红）；负数/非整数 = 非法 → fail-closed。
    if allocated_workers is None:
        allocated_workers = GATE_SENTINEL_UNOBSERVED
    workers_ok = isinstance(allocated_workers, int) and not isinstance(
        allocated_workers, bool) and allocated_workers >= 0
    if not cpus_ok or not workers_ok:
        return {
            "verdict": "fail",
            "violations": ["monitoring_missing: effective_cpus 不可得/非法或 "
                           "allocated_workers 非法（门禁适用性无法判定 → fail-closed）"],
            "recorded": recorded,
            "reason": None,
            "metrics": metrics,
        }
    interval = (float(compute_interval_seconds) if compute_interval_seconds
                is not None else result.get("duration_seconds"))
    interval_ok = isinstance(interval, (int, float)) and interval >= 0.0
    if not interval_ok:
        return {
            "verdict": "fail",
            "violations": ["monitoring_missing: 计算区间时长不可得（fail-closed）"],
            "recorded": recorded,
            "reason": None,
            "metrics": metrics,
        }
    interval = float(interval)
    allocated = (min(int(allocated_workers), effective_cpus)
                 if allocated_workers > 0 else 0)
    metrics.update({
        "effective_cpus": effective_cpus,
        # 三分量回显（契约 denominator.echo_components）：配置选择 /
        # 机器有效核 / 真实授予观测。分母 = 已分配容量 allocated。
        "available_cpus": effective_cpus,
        "selected_workers": selected_workers,
        "granted_workers": granted_workers,
        "allocated": allocated,
        "allocated_source": _allocated_source(allocated, selected_workers,
                                              granted_workers),
        "interval_seconds": round(interval, 6),
        "threads_max": result.get("threads_max"),
    })
    if allocated <= 0:
        recorded.append(
            "allocated_capacity_undeclared: 已分配容量未声明（granted_workers / "
            "selected_workers 皆哨兵 0）→ 利用率类判据不成立，记入 recorded；"
            "禁止以机器有效核冒充（契约 denominator.forbid）")
    # ── 适用性: 有效 CPU<2 或区间 ≤10s → NOT_APPLICABLE（显式分类, 非豁免） ──
    if (effective_cpus < FROZEN_GATE_MIN_EFFECTIVE_CPUS
            or interval <= FROZEN_GATE_MIN_INTERVAL_SECONDS):
        reasons = []
        if effective_cpus < FROZEN_GATE_MIN_EFFECTIVE_CPUS:
            reasons.append(f"effective_cpus={effective_cpus} < "
                           f"{FROZEN_GATE_MIN_EFFECTIVE_CPUS}")
        if interval <= FROZEN_GATE_MIN_INTERVAL_SECONDS:
            reasons.append(f"compute_interval={interval:.3f}s <= "
                           f"{FROZEN_GATE_MIN_INTERVAL_SECONDS}s")
        return {
            "verdict": "not_applicable",
            "violations": [],
            "recorded": recorded,
            "reason": "门禁不适用（NOT_APPLICABLE, 非豁免）: " + "; ".join(reasons),
            "metrics": metrics,
        }

    denom = 100.0 * allocated
    violations: list[str] = []

    # ── 采样证据: 缺失即 fail（监控缺失直接 FAIL, 不可豁免） ──
    samples = result.get("cpu_samples") or []
    cpu_values = [s.get("cpu_percent") for s in samples]
    valid = [v for v in cpu_values if isinstance(v, (int, float))]
    if not valid:
        violations.append(
            f"monitoring_missing: 无有效 CPU 采样（{len(samples)} 样本全无效/缺失）"
            f"—— G-RES-01 利用率判据无实测证据")
    else:
        # 区间中部断流（首末样本之间出现 None）= 监控缺口 → fail-closed
        first_valid = next(i for i, v in enumerate(cpu_values)
                           if isinstance(v, (int, float)))
        last_valid = len(cpu_values) - 1 - next(
            i for i, v in enumerate(reversed(cpu_values))
            if isinstance(v, (int, float)))
        if any(not isinstance(v, (int, float))
               for v in cpu_values[first_valid:last_valid + 1]):
            violations.append(
                "monitoring_missing: 计算区间中部 CPU 采样断流（证据缺口, "
                "fail-closed; 启动/收尾边界样本除外）")
        # 工作量事实标记（契约 workload_floor_effect = fact_marker_only）：
        # 线程秒 = 等效核·秒 = (CPU%/100) × 区间长度。
        avg_cpu_all = (result.get("cpu_percent_avg")
                       if isinstance(result.get("cpu_percent_avg"), (int, float))
                       else sum(valid) / len(valid))
        metrics["work_core_seconds"] = round(avg_cpu_all / 100.0 * interval, 6)
        metrics["workload_floor_core_seconds"] = FROZEN_GATE_WORKLOAD_FLOOR_CORE_SECONDS
        metrics["workload_floor_reached"] = (
            metrics["work_core_seconds"] >= FROZEN_GATE_WORKLOAD_FLOOR_CORE_SECONDS)
        if allocated <= 0:
            metrics["utilization_evaluated"] = False
        else:
            metrics["utilization_evaluated"] = True
            avg_cpu = avg_cpu_all
            avg_util = avg_cpu / denom
            metrics["avg_utilization"] = round(avg_util, 6)
            metrics["avg_cpu_percent"] = round(avg_cpu, 6)
            # record_and_justify 判据 1: 计算区间平均利用率 ≥ 已分配容量 85%
            # （契约 compute.mean_utilization_enforcement；16-worker 真负载实测
            # 65.09%，未标定前不得硬失败 —— 必须记录 + 超标须登记）。
            if avg_util < FROZEN_GATE_MIN_AVG_UTILIZATION:
                recorded.append(
                    f"frozen_avg_utilization_low: 平均利用率 {avg_util:.3f} "
                    f"(<{FROZEN_GATE_MIN_AVG_UTILIZATION:.2f}, 实测 CPU {avg_cpu:.1f}% "
                    f"/ 已分配容量 {allocated} 核={denom:.0f}%; "
                    f"enforcement=record_and_justify)")
            # record_and_justify 判据 2/3: p50 ≥90% 与逐样本 ≥85% 占比 ≥0.70
            # （契约 compute.p50_utilization_min_percent /
            #  per_sample_utilization_min_percent + per_sample_pass_fraction_min）。
            sample_utils = [v / denom for v in valid]
            p50_util = statistics.median(sample_utils)
            metrics["p50_utilization"] = round(p50_util, 6)
            if p50_util < FROZEN_GATE_MIN_P50_UTILIZATION:
                recorded.append(
                    f"frozen_p50_utilization_low: 样本利用率中位数 {p50_util:.3f} "
                    f"(<{FROZEN_GATE_MIN_P50_UTILIZATION:.2f}; "
                    f"enforcement=record_and_justify)")
            pass_frac = (sum(1 for u in sample_utils
                             if u >= FROZEN_GATE_MIN_SAMPLE_UTILIZATION)
                         / len(sample_utils))
            metrics["sample_pass_fraction"] = round(pass_frac, 6)
            if pass_frac < FROZEN_GATE_MIN_SAMPLE_PASS_FRACTION:
                recorded.append(
                    f"frozen_sample_utilization_low: 单样本 ≥"
                    f"{FROZEN_GATE_MIN_SAMPLE_UTILIZATION:.0%} 的样本占比 "
                    f"{pass_frac:.3f} (<{FROZEN_GATE_MIN_SAMPLE_PASS_FRACTION:.2f}; "
                    f"enforcement=record_and_justify)")
        # 硬失败: 任何连续 ≥10s 窗口利用率 <60% **且队列有工作**
        # （契约 compute.queue_low_window_requires_queued_work）。
        if allocated > 0:
            poll = result.get("poll_interval")
            poll = poll if isinstance(poll, (int, float)) and poll > 0 else 0.2
            run_seconds = 0.0
            best_low = 0.0
            prev_t = None
            prev_low = False
            runnable_sampled = False
            cur_runnable: list[float] = []
            best_runnable: list[float] = []
            for s in samples:
                v = s.get("cpu_percent")
                t = s.get("t")
                rn = s.get("runnable")
                if isinstance(rn, (int, float)) and not isinstance(rn, bool):
                    runnable_sampled = True
                low = isinstance(v, (int, float)) and (v / denom) < (
                    FROZEN_GATE_WINDOW_MIN_UTILIZATION)
                if low and prev_low and isinstance(t, (int, float)) and prev_t is not None:
                    run_seconds += max(0.0, float(t) - float(prev_t))
                    cur_runnable.append(rn)
                elif low:
                    run_seconds = poll  # 单样本按其覆盖的采样间隔计
                    cur_runnable = [rn]
                else:
                    run_seconds = 0.0
                    cur_runnable = []
                if run_seconds > best_low:
                    best_low = run_seconds
                    best_runnable = [x for x in cur_runnable
                                     if isinstance(x, (int, float))
                                     and not isinstance(x, bool)]
                prev_t = t if isinstance(t, (int, float)) else prev_t
                prev_low = low
            metrics["max_low_window_seconds"] = round(best_low, 3)
            metrics["runnable_sampled"] = runnable_sampled
            if best_low >= FROZEN_GATE_WINDOW_SECONDS:
                if not runnable_sampled:
                    # 证据不可得（合成/旧证据）：沿用无前置判据（向后兼容），
                    # 但显式标注证据面缺失。
                    queued_work = True
                    metrics["queued_work"] = "evidence_unavailable"
                else:
                    stat = (statistics.median(best_runnable)
                            if best_runnable else 0.0)
                    # 「队列有工作」= 就绪（R 态）线程数中位数 **超过已分配槽位**
                    # —— 就绪线程多于可用核 ⇒ 必有线程在排队（CPU 饥饿）。
                    # 仅 R≥2 不足以判定：2 个线程在 16 核配额上全速跑也是 R=2，
                    # 那是并行宽度不足（记录项），不是饥饿。
                    queued_work = (stat > allocated
                                   and stat >= FROZEN_GATE_MIN_QUEUED_THREADS)
                    metrics["queued_work"] = bool(queued_work)
                    metrics["queued_work_runnable_p50"] = round(stat, 3)
                    metrics["queued_work_reference"] = "runnable_p50 > allocated"
                if queued_work:
                    violations.append(
                        f"frozen_low_utilization_window: 连续 {best_low:.1f}s 利用率"
                        f"<{FROZEN_GATE_WINDOW_MIN_UTILIZATION:.0%} 且队列有工作"
                        f"（窗口阈值 {FROZEN_GATE_WINDOW_SECONDS:.0f}s）")
                else:
                    recorded.append(
                        f"frozen_low_utilization_window_no_queue: 连续 {best_low:.1f}s "
                        f"利用率<{FROZEN_GATE_WINDOW_MIN_UTILIZATION:.0%} 但无就绪线程"
                        f"积压（不判 CPU 饥饿；策略同 C++ runnable>0 前置）")

    # ── 硬失败 1: 只有一个活跃计算线程（分母无关判据） ──
    # 统计量 = 计算区间内活跃线程数的 **p50**（契约
    # compute.min_active_compute_threads_statistic）；峰值只在有效样本 <2 时回落
    # （fallback_statistic/fallback_domain）。峰值为判据会放过「绝大多数时间单线程、
    # 偶发并发」的 run（R-4 实测：GIL 绑定 2 线程 run 的 threads_max=3 而真实并行
    # 宽度=1）—— 峰值只作回显。
    threads_max = result.get("threads_max")
    if not isinstance(threads_max, (int, float)) or isinstance(threads_max, bool) \
            or threads_max <= 0:
        violations.append(
            "monitoring_missing: 活跃线程采样不可得（threads_max 无效）")
    else:
        thread_vals = [s.get("threads") for s in samples
                       if isinstance(s.get("threads"), (int, float))
                       and not isinstance(s.get("threads"), bool)]
        if len(thread_vals) >= 2:
            threads_stat = float(statistics.median(thread_vals))
            metrics["active_threads_statistic"] = "p50"
        else:
            threads_stat = float(max([threads_max] + thread_vals))
            metrics["active_threads_statistic"] = "peak(<2 samples)"
        metrics["active_threads_stat"] = round(threads_stat, 3)
        if threads_stat < FROZEN_GATE_MIN_ACTIVE_THREADS:
            violations.append(
                f"frozen_single_active_thread: 活跃计算线程 "
                f"{metrics['active_threads_statistic']}={threads_stat:g} "
                f"< {FROZEN_GATE_MIN_ACTIVE_THREADS}"
                f"（单活跃计算线程即失败；threads_max={threads_max:g} 仅回显）")

    # ── progress 证据面（调用方按运行类型启用） ──
    if require_progress:
        progress = result.get("progress")
        if not progress or not progress.get("total"):
            violations.append("no_progress: 未观测到进度证据（require_progress）")
        elif progress.get("done", 0) <= 0:
            violations.append("no_progress: 进度 done=0（计算无推进证据）")

    return {
        "verdict": "fail" if violations else "pass",
        "violations": violations,
        "recorded": recorded,
        "reason": None,
        "metrics": metrics,
    }


def evaluate(result: dict, min_cpu_percent: Optional[float] = None,
             max_rss_growth_kb: Optional[float] = None,
             min_progress: Optional[int] = None) -> list[str]:
    """对 run_monitored 结果做阈值判定 → 违规清单（空 list = 通过）。

    min_cpu_percent      平均 CPU% 低于阈值 → "low_cpu_utilization"
    max_rss_growth_kb    peak_rss_kb - rss_start_kb 超阈值 → "memory_leak_suspected"；
                         峰值/基线不可得 → fail-closed "rss_unavailable"
    min_progress         progress 缺失 → "no_progress"；done 低于阈值 →
                         "progress_below_threshold"
    """
    violations: list[str] = []
    if min_cpu_percent is not None:
        avg = result.get("cpu_percent_avg")
        if avg is None:
            violations.append(
                f"low_cpu_unavailable: cpu_percent_avg 不可得（要求 >= {min_cpu_percent}%）")
        elif avg < min_cpu_percent:
            violations.append(
                f"low_cpu_utilization: avg {avg:.1f}% < {min_cpu_percent}%")
    if max_rss_growth_kb is not None:
        peak = result.get("peak_rss_kb")
        start = result.get("rss_start_kb")
        if peak is None or start is None:
            violations.append(
                f"rss_unavailable: RSS 采样不可得（增长上限 {max_rss_growth_kb} KB 无法判定）")
        else:
            growth = peak - start
            if growth > max_rss_growth_kb:
                violations.append(
                    f"memory_leak_suspected: RSS 增长 {growth} KB > {max_rss_growth_kb} KB")
    if min_progress is not None:
        progress = result.get("progress")
        if not progress:
            violations.append(f"no_progress: 未观测到进度（要求 done >= {min_progress}）")
        elif progress.get("done", 0) < min_progress:
            violations.append(
                f"progress_below_threshold: done {progress.get('done')} < {min_progress}")
    return violations


# ------------------------------------------------------------------ CLI ----
def build_arg_parser() -> argparse.ArgumentParser:
    """构造 CLI 解析器（-- 与被监控命令手工切分, 防误吞子进程选项）。"""
    parser = argparse.ArgumentParser(
        description="heavy wrapper：采样子进程 CPU/RSS/PSS/IO/threads/progress → JSON 证据")
    parser.add_argument("--timeout", type=float, default=3600.0,
                        help="超时秒数，超时 SIGKILL 进程组（默认 3600）")
    parser.add_argument("--poll-interval", type=float, default=0.2,
                        help="采样间隔秒（默认 0.2）")
    parser.add_argument("--output", "-o", default=None,
                        help="JSON 证据文件路径（同时始终打印到 stdout）")
    parser.add_argument("--progress-file", default=None,
                        help="progress 文件路径（经 ASTROCS_PROGRESS_FILE 传给子进程）")
    # G-RES-01 重计算负载资源门（判据权威 21_observability §8; 数值源
    # contracts/resource_gate_v1.json; 显式 opt-in, 不改变既有用法）:
    parser.add_argument("--gate-workers", type=int, default=None,
                        help="已授予并发租约峰值 granted_workers（**已分配容量分母**）; "
                             "给出后对本次运行做 G-RES-01 判定; 0 = 未观测哨兵 → "
                             "回落 min(selected_workers, available_cpus)")
    parser.add_argument("--gate-selected-workers", type=int, default=None,
                        help="配置选择的 worker 数 selected_workers（仅哨兵回落与"
                             "三分量回显; 缺省 = 机器有效核）")
    # CI-001 收紧（控制包 02_GATES_AND_EXECUTION.md §执行"监控必须调用
    # evaluate"）: CI 注册检查的监控必须判定且 fail-closed。判定结果写入
    # frozen_gate, fail → 10。
    # 订正（GATE-FIX-RES / R-4 D-12）: 旧实现把 host_probe.effective_cpu_cores
    # （机器有效核）当已分配容量 ⇒ 任何 worker 数 < 机器核数的并行任务结构性判红
    # （实测 2 线程满核 14s → U=6.3% → rc=10）。机器有效核不是已分配容量；
    # 未声明已分配容量时利用率判据不成立（recorded），分母无关判据照常判定。
    parser.add_argument("--gate-required", action="store_true",
                        help="强制 G-RES-01 判定（CI 重计算检查必选）; 已分配容量"
                             "分母由 --gate-workers/--gate-selected-workers 声明, "
                             "**不得**取机器有效核")
    parser.add_argument("--gate-effective-cpus", type=int, default=None,
                        help="有效 CPU 数（available_cpus）; 缺省取 "
                             "host_probe.effective_cpu_cores")
    parser.add_argument("--gate-require-progress", action="store_true",
                        help="progress 证据缺失计入门禁违规（按运行类型启用）")
    parser.add_argument("--gate-compute-interval", type=float, default=None,
                        help="计算区间秒; 缺省取 duration_seconds（混合 run 显式给出）")
    return parser


def main(argv: Optional[list[str]] = None) -> int:
    """CLI：`run_monitored.py [opts] -- <cmd> [args...]`（`--` 手工切分，防误吞子进程选项）。"""
    raw = list(sys.argv[1:] if argv is None else argv)
    if "--" in raw:
        cut = raw.index("--")
        opts, child = raw[:cut], raw[cut + 1:]
    else:
        opts, child = raw, []
    parser = build_arg_parser()
    args = parser.parse_args(opts)
    if not child:
        parser.error("缺少被监控命令：run_monitored.py [opts] -- <cmd> [args...]")

    result = run_monitored(child, timeout=args.timeout,
                           poll_interval=args.poll_interval,
                           output=args.output, progress_file=args.progress_file)
    # RT-001 冻结利用率门禁: 显式 opt-in（--gate-workers）/ CI-001 CI 面必选
    # （--gate-required, 与前者互斥）; 判定结果写入输出 JSON 的 frozen_gate
    # 字段; fail → 退出码 10（与项目 RESOURCE 退出码约定一致, lib/infrastructure/cli/exit_codes.h）。
    # NOT_APPLICABLE 是显式分类（非豁免）, 透传子进程退出码; 监控/证据缺失由
    # evaluate_frozen_gate fail-closed 判 fail。
    if args.gate_required or args.gate_workers is not None:
        effective = args.gate_effective_cpus
        if effective is None:
            probe_cpus = (result.get("host_probe") or {}).get(
                "effective_cpu_cores")
            effective = int(probe_cpus) if isinstance(probe_cpus, (int, float)) \
                and probe_cpus >= 1 else None
        # 已分配容量分母 = resolve_allocated_capacity（契约 denominator 的 Python
        # 唯一实现点）：granted_workers（观测/声明）优先；哨兵 0 → 回落
        # min(selected_workers, available_cpus)；两者皆哨兵 → 0（不成立，
        # **绝不**用机器有效核冒充）。effective 不可得 → evaluate_frozen_gate
        # 输入无效 fail-closed（绝不 pass）。
        effective_int = effective if isinstance(effective, int) else None
        # selected_workers 缺省 = 哨兵 0（**未声明**），不得用机器有效核回填：
        # run_monitored 监控的是外部命令，无从得知其线程预算；以机器核充当已分配
        # 容量正是本任务要修的结构性误报（R-4 D-12 / E7b）。
        selected = (args.gate_selected_workers
                    if args.gate_selected_workers is not None else 0)
        allocated = resolve_allocated_capacity(
            granted_workers=args.gate_workers,
            selected_workers=selected,
            available_cpus=effective_int)
        gate = evaluate_frozen_gate(
            result, effective_cpus=effective,
            allocated_workers=allocated,
            selected_workers=selected,
            granted_workers=args.gate_workers,
            compute_interval_seconds=args.gate_compute_interval,
            require_progress=args.gate_require_progress)
        result["frozen_gate"] = gate
        if gate.get("metrics", {}).get("allocated", 0) <= 0:
            print("run_monitored: warning: 已分配容量未声明（--gate-workers/"
                  "--gate-selected-workers 皆缺）→ G-RES-01 利用率判据不成立；"
                  "禁止以机器有效核冒充已分配容量（21_observability §8）",
                  file=sys.stderr)
    if args.output is not None:  # gate 结果并入证据文件
        out_path = Path(args.output)
        if out_path.parent != Path(""):
            out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(result, ensure_ascii=False, indent=1),
                            encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    gate = result.get("frozen_gate")
    if isinstance(gate, dict) and gate.get("verdict") == "fail":
        return 10  # RESOURCE 门禁失败（exit_codes 约定）
    if result["timed_out"]:
        return 124
    code = result["exit_code"]
    if code is None:
        return 125
    return code if code >= 0 else 128 + (-code)  # 信号退出 → shell 惯例 128+signum


if __name__ == "__main__":
    sys.exit(main())
