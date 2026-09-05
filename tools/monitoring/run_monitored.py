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

__all__ = ["run_monitored", "evaluate", "parse_progress_line", "main"]

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
def main(argv: Optional[list[str]] = None) -> int:
    """CLI：`run_monitored.py [opts] -- <cmd> [args...]`（`--` 手工切分，防误吞子进程选项）。"""
    raw = list(sys.argv[1:] if argv is None else argv)
    if "--" in raw:
        cut = raw.index("--")
        opts, child = raw[:cut], raw[cut + 1:]
    else:
        opts, child = raw, []
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
    args = parser.parse_args(opts)
    if not child:
        parser.error("缺少被监控命令：run_monitored.py [opts] -- <cmd> [args...]")

    result = run_monitored(child, timeout=args.timeout,
                           poll_interval=args.poll_interval,
                           output=args.output, progress_file=args.progress_file)
    print(json.dumps(result, ensure_ascii=False))
    if result["timed_out"]:
        return 124
    code = result["exit_code"]
    if code is None:
        return 125
    return code if code >= 0 else 128 + (-code)  # 信号退出 → shell 惯例 128+signum


if __name__ == "__main__":
    sys.exit(main())
