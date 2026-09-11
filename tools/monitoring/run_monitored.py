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
           "FROZEN_GATE_MIN_EFFECTIVE_CPUS", "FROZEN_GATE_MIN_INTERVAL_SECONDS",
           "FROZEN_GATE_MIN_AVG_UTILIZATION", "FROZEN_GATE_WINDOW_SECONDS",
           "FROZEN_GATE_WINDOW_MIN_UTILIZATION", "FROZEN_GATE_MIN_ACTIVE_THREADS"]

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
# ── RT-001 冻结利用率门禁（宪章 §10.5 + §18.2 负责人裁决 2）──
# 冻结阈值（负责人裁决, 只能按宪章 §1.2 修改, 本文件不得放宽）:
FROZEN_GATE_MIN_EFFECTIVE_CPUS = 2        # 有效 CPU 数 < 2 → 门禁不适用
FROZEN_GATE_MIN_INTERVAL_SECONDS = 10.0   # 计算区间须严格 >10s 才适用
FROZEN_GATE_MIN_AVG_UTILIZATION = 0.85    # 平均利用率 ≥ 已分配容量的 85%
FROZEN_GATE_WINDOW_SECONDS = 10.0         # 任何连续 10s 窗口
FROZEN_GATE_WINDOW_MIN_UTILIZATION = 0.60  # 窗口内利用率下限 60%
FROZEN_GATE_MIN_ACTIVE_THREADS = 2        # 只有一个活跃计算线程即失败


def evaluate_frozen_gate(result: dict, *, effective_cpus, allocated_workers,
                         compute_interval_seconds: Optional[float] = None,
                         require_progress: bool = False) -> dict:
    """对 run_monitored 结果做宪章 §10.5/§18.2 冻结门禁判定（实测 fail-closed）。

    effective_cpus   有效 CPU 数（affinity ∩ cgroup；None/非正 → fail-closed,
                     不得以机器总核或配置值冒充）
    allocated_workers 已分配 worker 数（已分配容量）；分母 = min(allocated,
                     effective_cpus)，与 cli utilization_value 同一口径
    compute_interval_seconds 计算区间时长；None → 取 duration_seconds（监控
                     面向计算段时同义；混合 run 由调用方显式给出计算区间）
    require_progress True 时 progress 缺失/零进度计入违规（默认 False——
                     progress 证据面由调用方按运行类型启用）

    返回 dict（确定性纯函数，不抛错）:
      verdict: "pass" | "fail" | "not_applicable"
      violations: 违规清单（fail 时非空；哨兵/证据缺失一律 monitoring_missing
                  前缀 —— 采样缺失不是低利用率豁免，是 fail 证据）
      reason: not_applicable 时的显式分类说明（门禁不适用 ≠ 豁免,
              BASE-UTIL-001 分类口径）
      metrics: 实测值回显（avg_utilization/max_low_window_seconds/threads_max/
               interval/effective_cpus/allocated）
    """
    metrics: dict = {}
    # ── 输入有效性: 无法判定适用性 → fail-closed（绝不 pass） ──
    cpus_ok = isinstance(effective_cpus, int) and not isinstance(
        effective_cpus, bool) and effective_cpus >= 1
    workers_ok = isinstance(allocated_workers, int) and not isinstance(
        allocated_workers, bool) and allocated_workers >= 1
    if not cpus_ok or not workers_ok:
        return {
            "verdict": "fail",
            "violations": ["monitoring_missing: effective_cpus/allocated_workers "
                           "不可得或非法（门禁适用性无法判定 → fail-closed）"],
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
            "reason": None,
            "metrics": metrics,
        }
    interval = float(interval)
    metrics.update({
        "effective_cpus": effective_cpus,
        "allocated": min(allocated_workers, effective_cpus),
        "interval_seconds": round(interval, 6),
        "threads_max": result.get("threads_max"),
    })
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
            "reason": "门禁不适用（NOT_APPLICABLE, 非豁免）: " + "; ".join(reasons),
            "metrics": metrics,
        }

    allocated = metrics["allocated"]
    denom = 100.0 * allocated
    violations: list[str] = []

    # ── 采样证据: 缺失即 fail（监控缺失直接 FAIL, 不可豁免） ──
    samples = result.get("cpu_samples") or []
    cpu_values = [s.get("cpu_percent") for s in samples]
    valid = [v for v in cpu_values if isinstance(v, (int, float))]
    if not valid:
        violations.append(
            f"monitoring_missing: 无有效 CPU 采样（{len(samples)} 样本全无效/缺失）"
            f"—— 冻结门禁 {FROZEN_GATE_MIN_AVG_UTILIZATION:.0%} 无实测证据")
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
        avg_cpu = (result.get("cpu_percent_avg")
                   if isinstance(result.get("cpu_percent_avg"), (int, float))
                   else sum(valid) / len(valid))
        avg_util = avg_cpu / denom
        metrics["avg_utilization"] = round(avg_util, 6)
        metrics["avg_cpu_percent"] = round(avg_cpu, 6)
        # 冻结门禁 1: 计算区间平均利用率 ≥ 已分配容量 85%
        if avg_util < FROZEN_GATE_MIN_AVG_UTILIZATION:
            violations.append(
                f"frozen_avg_utilization_low: 平均利用率 {avg_util:.3f} "
                f"(<{FROZEN_GATE_MIN_AVG_UTILIZATION:.2f}, 实测 CPU {avg_cpu:.1f}% "
                f"/ 已分配容量 {allocated} 核={denom:.0f}%)")
        # 冻结门禁 2: 任何连续 10s 窗口利用率 < 60%
        poll = result.get("poll_interval")
        poll = poll if isinstance(poll, (int, float)) and poll > 0 else 0.2
        run_seconds = 0.0
        best_low = 0.0
        prev_t = None
        prev_low = False
        for s in samples:
            v = s.get("cpu_percent")
            t = s.get("t")
            low = isinstance(v, (int, float)) and (v / denom) < (
                FROZEN_GATE_WINDOW_MIN_UTILIZATION)
            if low and prev_low and isinstance(t, (int, float)) and prev_t is not None:
                run_seconds += max(0.0, float(t) - float(prev_t))
            elif low:
                run_seconds = poll  # 单样本按其覆盖的采样间隔计
            else:
                run_seconds = 0.0
            best_low = max(best_low, run_seconds)
            prev_t = t if isinstance(t, (int, float)) else prev_t
            prev_low = low
        metrics["max_low_window_seconds"] = round(best_low, 3)
        if best_low >= FROZEN_GATE_WINDOW_SECONDS:
            violations.append(
                f"frozen_low_utilization_window: 连续 {best_low:.1f}s 利用率"
                f"<{FROZEN_GATE_WINDOW_MIN_UTILIZATION:.0%}（窗口阈值 "
                f"{FROZEN_GATE_WINDOW_SECONDS:.0f}s）")

    # ── 冻结门禁 3: 只有一个活跃计算线程即失败 ──
    threads_max = result.get("threads_max")
    if not isinstance(threads_max, (int, float)) or threads_max <= 0:
        violations.append(
            "monitoring_missing: 活跃线程采样不可得（threads_max 无效）")
    elif threads_max < FROZEN_GATE_MIN_ACTIVE_THREADS:
        violations.append(
            f"frozen_single_active_thread: 活跃线程峰值 {int(threads_max)} "
            f"< {FROZEN_GATE_MIN_ACTIVE_THREADS}（单活跃计算线程即失败）")

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
    # RT-001 冻结利用率门禁（宪章 §10.5/§18.2; 显式 opt-in, 不改变既有用法）:
    parser.add_argument("--gate-workers", type=int, default=None,
                        help="已分配 worker 数; 给出后对本次运行做冻结门禁判定")
    parser.add_argument("--gate-effective-cpus", type=int, default=None,
                        help="有效 CPU 数; 缺省取 host_probe.effective_cpu_cores")
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
    # RT-001 冻结利用率门禁: 显式 opt-in（--gate-workers）; 判定结果写入输出
    # JSON 的 frozen_gate 字段; fail → 退出码 10（与项目 RESOURCE 退出码约定
    # 一致, cli/exit_codes.h）。NOT_APPLICABLE 是显式分类（非豁免）, 透传子进程
    # 退出码; 监控/证据缺失由 evaluate_frozen_gate fail-closed 判 fail。
    if args.gate_workers is not None:
        effective = args.gate_effective_cpus
        if effective is None:
            probe_cpus = (result.get("host_probe") or {}).get(
                "effective_cpu_cores")
            effective = int(probe_cpus) if isinstance(probe_cpus, (int, float)) \
                and probe_cpus >= 1 else None
        gate = evaluate_frozen_gate(
            result, effective_cpus=effective,
            allocated_workers=args.gate_workers,
            compute_interval_seconds=args.gate_compute_interval,
            require_progress=args.gate_require_progress)
        result["frozen_gate"] = gate
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
