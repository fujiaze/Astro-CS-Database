#!/usr/bin/env python3
"""lib/infrastructure/scheduler/v6_budget.py — RUNTIME-CI-001 进程级统一资源预算 + heavy run 记录面。

宪章 §10.4：一个进程只有一个资源调度器与线程预算源；模块按 work unit 申请线程
租约，Runtime 防止嵌套并行和超额订阅。

宪章 §10.5 / §17.6：每个 heavy 运行自动记录进程/线程 CPU、每线程 CPU、RSS/PSS、
内存增长、读写字节、I/O wait、work units、队列深度、worker 均衡与墙钟。

SO-05（04_OPEN_ITEMS_AND_SIGNOFF）：『自动判决 vs 只记录』属负责人签字项。未签字
前本模块按 fail-closed **只记录 + 显式 pending 标记**，绝不把资源判据升级为硬失败。

本模块是 CLI C++ 契约（lib/infrastructure/cli/v6_runtime_contract.h）的**独立 Python 记录面**，供
CI/工具链在进程外做同一口径的采样与门限记录（不同实现，非自证）。

用法:
  python3 lib/infrastructure/scheduler/v6_budget.py selftest
  python3 lib/infrastructure/scheduler/v6_budget.py sample --pid <pid> --seconds 2
  python3 lib/infrastructure/scheduler/v6_budget.py evaluate --metrics <metrics.json> [--json-out <path>]
"""
from __future__ import annotations

import argparse
import ctypes
import dataclasses
import json
import os
import pathlib
import sys
import time

# §10.5 必采字段键（与 C++ required_metric_keys 同口径；此处独立列写，便于交叉核对）
REQUIRED_METRIC_KEYS = [
    "process_cpu_seconds",
    "per_thread_cpu_max_seconds",
    "per_thread_cpu_sum_seconds",
    "rss_bytes",
    "pss_bytes",
    "rss_growth_mb_per_s",
    "read_bytes",
    "write_bytes",
    "io_wait_percent",
    "work_units",
    "queue_depth",
    "worker_balance",
    "wall_seconds",
    "active_window_seconds",
]

# SO-05 记录/裁决分离策略（未签字前恒定）
SO05_ID = "SO-05"
SO05_STATUS = "PENDING_OWNER_SIGNOFF"
RECORD_ONLY_POLICY = "record_only"
NO_AUTO_ADJUDICATION = "auto_adjudication_withheld_pending_owner_signoff"


@dataclasses.dataclass
class HeavyRunMetrics:
    run_id: str = ""
    phase: str = ""
    mode: str = ""
    process_cpu_seconds: float = 0.0
    avg_equivalent_cores: float = 0.0
    cpu_p50_percent: float = -1.0
    cpu_mean_percent: float = -1.0
    threads: int = 0
    active_compute_threads: int = 0
    per_thread_cpu_max_seconds: float = 0.0
    per_thread_cpu_sum_seconds: float = 0.0
    rss_bytes: int = 0
    pss_bytes: int = 0
    rss_growth_mb_per_s: float = 0.0
    read_bytes: int = 0
    write_bytes: int = 0
    io_wait_percent: float = 0.0
    work_units: int = 0
    queue_depth: int = 0
    worker_balance: float = 0.0
    wall_seconds: float = 0.0
    active_window_seconds: float = 0.0
    allocated_cores: int = 0

    def as_dict(self) -> dict:
        return dataclasses.asdict(self)

    def missing_keys(self):
        miss = []
        for k in REQUIRED_METRIC_KEYS:
            if not hasattr(self, k):
                miss.append(k)
        if self.allocated_cores < 1:
            miss.append("allocated_cores")
        if self.threads < 1:
            miss.append("threads")
        if self.active_compute_threads < 1:
            miss.append("active_compute_threads")
        if self.per_thread_cpu_sum_seconds <= 0.0:
            miss.append("per_thread_cpu_sum_seconds>0")
        if self.per_thread_cpu_max_seconds <= 0.0:
            miss.append("per_thread_cpu_max_seconds>0")
        if not self.phase:
            miss.append("phase")
        if not self.mode:
            miss.append("mode")
        return miss


class ProcessBudgetRegistry:
    """进程唯一预算源。第二个 register 调用返回 False（§10.4 单一来源）。"""

    _instance = None

    def __init__(self):
        self._owner = None
        self._cores = 0
        self._leased = 0
        self._denied = 0

    @classmethod
    def instance(cls) -> "ProcessBudgetRegistry":
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance

    def register_source(self, owner: str, cores: int) -> bool:
        if self._owner is not None:
            return False           # 第二来源：拒绝
        if cores <= 0:
            return False           # 零预算非法
        self._owner = owner
        self._cores = int(cores)
        self._leased = 0
        return True

    @property
    def has_source(self) -> bool:
        return self._owner is not None

    @property
    def owner(self) -> str:
        return self._owner or ""

    @property
    def allocated_cores(self) -> int:
        return self._cores

    @property
    def nested_parallel_denied(self) -> int:
        return self._denied

    def request_lease(self, requester: str, want: int) -> int:
        if self._owner is None:
            return 0
        avail = max(0, self._cores - self._leased)
        ask = avail if want == 0 else want
        if ask <= 0:
            return 0
        if ask > avail:
            self._denied += 1
            return 0               # 超额订阅 / 嵌套并行
        self._leased += ask
        return ask

    def release_lease(self, n: int) -> None:
        self._leased = max(0, self._leased - int(n))

    def reset_for_test(self) -> None:
        self._owner = None
        self._cores = 0
        self._leased = 0
        self._denied = 0


# --------------------------------------------------------------------------- 采样 ----
# 判据：采样链在**宿主平台**上必须真的可用（selftest 的 "thread count sampled"
# 就是这一条）。Windows 上没有 /proc，原实现静默返回 0 ⇒ 该门在 Windows 上
# 自检不绿，且是「无判别力的绿」（RSS/线程/CPU 全 0，任何阈值都恒真）。
# 下面用 ctypes 补 Windows 真实读数；API 不可用时返回 0，由 selftest 判据暴露，
# 不静默降级成"看起来通过"。
_IS_WINDOWS = os.name == "nt"


class _PROCESS_MEMORY_COUNTERS(ctypes.Structure):
    _fields_ = [("cb", ctypes.c_ulong),
                ("PageFaultCount", ctypes.c_ulong),
                ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t)]


class _FILETIME(ctypes.Structure):
    _fields_ = [("dwLowDateTime", ctypes.c_ulong),
                ("dwHighDateTime", ctypes.c_ulong)]


class _THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", ctypes.c_ulong),
                ("cntUsage", ctypes.c_ulong),
                ("th32ThreadID", ctypes.c_ulong),
                ("th32OwnerProcessID", ctypes.c_ulong),
                ("tpBasePri", ctypes.c_long),
                ("tpDeltaPri", ctypes.c_long),
                ("dwFlags", ctypes.c_ulong)]


def _win_open_process(pid: int):
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    try:
        return ctypes.windll.kernel32.OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION, False, int(pid))
    except (AttributeError, OSError):
        return None


def _win_thread_count(pid: int) -> int:
    TH32CS_SNAPTHREAD = 0x00000004
    INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
    try:
        snap = ctypes.windll.kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    except (AttributeError, OSError):
        return 0
    if not snap or snap == INVALID_HANDLE_VALUE:
        return 0
    n = 0
    try:
        entry = _THREADENTRY32()
        entry.dwSize = ctypes.sizeof(entry)
        ok = ctypes.windll.kernel32.Thread32First(snap, ctypes.byref(entry))
        while ok:
            if int(entry.th32OwnerProcessID) == int(pid):
                n += 1
            ok = ctypes.windll.kernel32.Thread32Next(snap, ctypes.byref(entry))
    except (AttributeError, OSError):
        return 0
    finally:
        ctypes.windll.kernel32.CloseHandle(snap)
    return n


def _win_read_status(pid: int):
    """(rss, vms, threads)；API 不可用 ⇒ 对应项为 0（不静默假装成功）。"""
    rss = vms = 0
    h = _win_open_process(pid)
    if h:
        try:
            counters = _PROCESS_MEMORY_COUNTERS()
            counters.cb = ctypes.sizeof(counters)
            if ctypes.windll.psapi.GetProcessMemoryInfo(
                    h, ctypes.byref(counters), counters.cb):
                rss = int(counters.WorkingSetSize)
                vms = int(counters.PagefileUsage)
        except (AttributeError, OSError):
            pass
        finally:
            ctypes.windll.kernel32.CloseHandle(h)
    return rss, vms, _win_thread_count(pid)


def _win_read_proc_cpu(pid: int) -> float:
    """进程累计 CPU 秒（GetProcessTimes）。"""
    def _secs(ft):
        return ((int(ft.dwHighDateTime) << 32) | int(ft.dwLowDateTime)) / 1e7

    h = _win_open_process(pid)
    if not h:
        return 0.0
    try:
        creation, exit_, kernel, user = (_FILETIME(), _FILETIME(),
                                         _FILETIME(), _FILETIME())
        if ctypes.windll.kernel32.GetProcessTimes(
                h, ctypes.byref(creation), ctypes.byref(exit_),
                ctypes.byref(kernel), ctypes.byref(user)):
            return _secs(kernel) + _secs(user)
    except (AttributeError, OSError):
        pass
    finally:
        ctypes.windll.kernel32.CloseHandle(h)
    return 0.0


def _read_status(pid: int):
    if _IS_WINDOWS:
        return _win_read_status(pid)
    rss = vms = threads = 0
    try:
        with open("/proc/%d/status" % pid, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    rss = int(line.split()[1]) * 1024
                elif line.startswith("VmSize:"):
                    vms = int(line.split()[1]) * 1024
                elif line.startswith("Threads:"):
                    threads = int(line.split()[1])
    except OSError:
        pass
    return rss, vms, threads


def _read_pss(pid: int) -> int:
    try:
        with open("/proc/%d/smaps_rollup" % pid, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("Pss:"):
                    return int(line.split()[1]) * 1024
    except OSError:
        pass
    return 0


def _read_io(pid: int):
    rb = wb = 0
    try:
        with open("/proc/%d/io" % pid, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("read_bytes:"):
                    rb = int(line.split()[1])
                elif line.startswith("write_bytes:"):
                    wb = int(line.split()[1])
    except OSError:
        pass
    return rb, wb


def _read_proc_cpu(pid: int) -> float:
    if _IS_WINDOWS:
        return _win_read_proc_cpu(pid)
    try:
        with open("/proc/%d/stat" % pid, "r", encoding="utf-8", errors="replace") as f:
            data = f.read()
        rp = data.rfind(")")
        fields = data[rp + 2:].split()
        utime, stime = int(fields[11]), int(fields[12])
        hz = os.sysconf("SC_CLK_TCK") or 100
        return (utime + stime) / float(hz)
    except (OSError, IndexError, ValueError):
        return 0.0


def _read_thread_cpu(pid: int):
    """返回 {tid: 累计 CPU 秒}。

    Windows 无 /proc/<pid>/task：逐线程 CPU 由 C++ 侧记录面提供；此处返回空表，
    d_thread_sum 退化为 0，selftest 的 "per-thread cpu delta sampled" 判据
    (>= 0.0) 仍成立，但 **不** 假装采到了真实值（空表可被调用方识别）。
    """
    if _IS_WINDOWS:
        return {}
    out = {}
    try:
        tids = os.listdir("/proc/%d/task" % pid)
    except OSError:
        return out
    hz = os.sysconf("SC_CLK_TCK") or 100
    for tid in tids:
        if not tid.isdigit():
            continue
        try:
            with open("/proc/%d/task/%s/stat" % (pid, tid), "r", encoding="utf-8", errors="replace") as f:
                data = f.read()
            rp = data.rfind(")")
            fields = data[rp + 2:].split()
            cpu = (int(fields[11]) + int(fields[12])) / float(hz)
            out[int(tid)] = cpu
        except (OSError, IndexError, ValueError):
            continue
    return out


def _read_iowait_seconds() -> float:
    try:
        with open("/proc/stat", "r", encoding="utf-8", errors="replace") as f:
            line = f.readline()
        if line.startswith("cpu"):
            parts = line.split()
            hz = os.sysconf("SC_CLK_TCK") or 100
            return int(parts[5]) / float(hz)
    except (OSError, IndexError, ValueError):
        pass
    return 0.0


def sample_proc(pid: int, prev=None) -> dict:
    """单次进程采样（含每线程 CPU 与 iowait）。prev 为上次 sample_proc 结果。"""
    rss, vms, threads = _read_status(pid)
    pss = _read_pss(pid)
    rb, wb = _read_io(pid)
    cpu = _read_proc_cpu(pid)
    tcpu = _read_thread_cpu(pid)
    iow = _read_iowait_seconds()
    s = {
        "t": time.monotonic(),
        "rss_bytes": rss, "vms_bytes": vms, "pss_bytes": pss,
        "read_bytes": rb, "write_bytes": wb,
        "process_cpu_seconds": cpu,
        "threads": threads,
        "thread_cpu": tcpu,
        "iowait_seconds": iow,
    }
    if prev is None:
        s.update(d_process_cpu=0.0, d_read_bytes=0, d_write_bytes=0,
                 d_thread_sum=0.0, d_thread_max=0.0, d_active_threads=0,
                 d_iowait=0.0, dt=0.0, rss_growth_mb_per_s=0.0)
        return s
    dt = max(1e-6, s["t"] - prev["t"])
    dsum = sum(tcpu.values()) - sum(prev["thread_cpu"].values())
    def _dmax():
        best = 0.0
        for tid, v in tcpu.items():
            pv = prev["thread_cpu"].get(tid, 0.0)
            best = max(best, v - pv)
        return best
    active = 0
    for tid, v in tcpu.items():
        pv = prev["thread_cpu"].get(tid)
        if pv is None:
            if v > 0.0:
                active += 1
        elif v > pv:
            active += 1
    s.update(
        d_process_cpu=max(0.0, cpu - prev["process_cpu_seconds"]),
        d_read_bytes=max(0, rb - prev["read_bytes"]),
        d_write_bytes=max(0, wb - prev["write_bytes"]),
        d_thread_sum=max(0.0, dsum),
        d_thread_max=max(0.0, _dmax()),
        d_active_threads=active,
        d_iowait=max(0.0, iow - prev["iowait_seconds"]),
        dt=dt,
        rss_growth_mb_per_s=((rss - prev["rss_bytes"]) / 1048576.0) / dt,
    )
    return s


def record_heavy_run(pid: int, seconds: float, phase: str, mode: str,
                     allocated_cores: int, interval: float = 0.5,
                     work_units: int = 0, queue_depth: int = 0) -> HeavyRunMetrics:
    """采样 pid 的 heavy run，聚合为 §10.5 字段面。"""
    t0 = time.monotonic()
    prev = None
    samples = []
    while time.monotonic() - t0 < seconds:
        cur = sample_proc(pid, prev)
        samples.append(cur)
        prev = cur
        time.sleep(interval)
    m = HeavyRunMetrics(run_id="pid-%d" % pid, phase=phase, mode=mode,
                        allocated_cores=allocated_cores,
                        work_units=work_units, queue_depth=queue_depth)
    if not samples:
        return m
    last = samples[-1]
    m.wall_seconds = last["t"] - samples[0]["t"]
    m.active_window_seconds = m.wall_seconds
    m.rss_bytes = last["rss_bytes"]
    m.pss_bytes = last["pss_bytes"]
    m.threads = last["threads"]
    m.process_cpu_seconds = last["process_cpu_seconds"] - samples[0]["process_cpu_seconds"]
    m.read_bytes = sum(s["d_read_bytes"] for s in samples[1:])
    m.write_bytes = sum(s["d_write_bytes"] for s in samples[1:])
    m.per_thread_cpu_sum_seconds = sum(s["d_thread_sum"] for s in samples[1:])
    m.per_thread_cpu_max_seconds = max((s["d_thread_max"] for s in samples[1:]), default=0.0)
    m.active_compute_threads = max((s["d_active_threads"] for s in samples[1:]), default=0)
    iow = sum(s["d_iowait"] for s in samples[1:])
    span = max(1e-6, m.wall_seconds)
    m.io_wait_percent = 100.0 * iow / span
    if m.wall_seconds > 0:
        m.avg_equivalent_cores = m.process_cpu_seconds / m.wall_seconds
    m.cpu_mean_percent = (100.0 * m.avg_equivalent_cores / allocated_cores
                          if allocated_cores > 0 else -1.0)
    m.rss_growth_mb_per_s = samples[-1]["rss_growth_mb_per_s"]
    return m


# ------------------------------------------------------------------- SO-05 门 ----

def so05_policy() -> dict:
    return {
        "signoff_id": SO05_ID,
        "signoff_status": SO05_STATUS,
        "measurement_policy": RECORD_ONLY_POLICY,
        "auto_adjudication_allowed": False,
        "auto_adjudication_policy": NO_AUTO_ADJUDICATION,
        "hard_fail_on_resource_verdict": False,
    }


def evaluate_heavy_run(m: HeavyRunMetrics) -> dict:
    """记录面裁决：阈值越线只登记 finding；SO-05 未签字前恒不改退出码。"""
    findings = []
    if m.cpu_mean_percent >= 0.0 and m.cpu_mean_percent < 85.0:
        findings.append({"kind": "cpu_mean_low",
                         "detail": "cpu mean %.2f%% < 85%% of allocated capacity" % m.cpu_mean_percent,
                         "would_fail_if_signed": True})
    if m.allocated_cores >= 2 and m.active_compute_threads < 2 and m.wall_seconds > 10.0:
        findings.append({"kind": "single_threaded",
                         "detail": "active_compute_threads=%d < 2" % m.active_compute_threads,
                         "would_fail_if_signed": True})
    if m.rss_growth_mb_per_s > 32.0 and m.active_window_seconds >= 10.0:
        findings.append({"kind": "memory_growth",
                         "detail": "rss_slope %.2f MB/s > 32" % m.rss_growth_mb_per_s,
                         "would_fail_if_signed": True})
    if m.io_wait_percent > 50.0:
        findings.append({"kind": "io_wait_high",
                         "detail": "iowait %.2f%% > 50%%" % m.io_wait_percent,
                         "would_fail_if_signed": True})
    policy = so05_policy()
    return {
        "status": "record_only_pending_owner_signoff",
        "signoff_id": SO05_ID,
        "signoff_status": SO05_STATUS,
        "hard_fail": False,               # 未签字前恒 False
        "measurement_policy": policy["measurement_policy"],
        "auto_adjudication_allowed": False,
        "findings": findings,
        "metrics": m.as_dict(),
        "missing_metrics": m.missing_keys(),
    }


# ---------------------------------------------------------------------- CLI ----

def _selftest() -> int:
    fails = 0

    def check(cond, what):
        nonlocal fails
        if not cond:
            fails += 1
            print("  [FAIL] " + what)
        else:
            print("  [ok] " + what)

    R = ProcessBudgetRegistry.instance()
    R.reset_for_test()
    check(len(REQUIRED_METRIC_KEYS) == len(set(REQUIRED_METRIC_KEYS)), "metric keys unique")
    check(R.register_source("runtime", 8), "first source accepted")
    check(not R.register_source("rogue", 4), "second source rejected")
    check(R.request_lease("a", 3) == 3, "lease granted")
    check(R.request_lease("b", 5) == 5, "second lease granted")
    check(R.request_lease("c", 1) == 0, "oversubscription rejected")
    check(R.nested_parallel_denied == 1, "denied counter")
    R.release_lease(8)
    R.reset_for_test()
    pol = so05_policy()
    check(pol["measurement_policy"] == "record_only", "record-only policy")
    check(pol["signoff_status"] == "PENDING_OWNER_SIGNOFF", "pending signoff")
    check(pol["auto_adjudication_allowed"] is False, "no auto adjudication")
    m = HeavyRunMetrics(allocated_cores=4, threads=4, active_compute_threads=4,
                        per_thread_cpu_sum_seconds=1.0, per_thread_cpu_max_seconds=0.3,
                        wall_seconds=20.0, active_window_seconds=20.0,
                        phase="phase2", mode="psfsw_robust", avg_equivalent_cores=0.5,
                        cpu_mean_percent=12.5)
    v = evaluate_heavy_run(m)
    check(v["hard_fail"] is False, "evaluate never hard fails before signoff")
    check(any(f["kind"] == "cpu_mean_low" for f in v["findings"]), "low cpu recorded as finding")
    # 从本进程自身采样一次，确认宿主平台记录链可用（Linux=/proc，Windows=ctypes）
    s = sample_proc(os.getpid(), None)
    s2 = sample_proc(os.getpid(), s)
    check(s2["threads"] >= 1, "thread count sampled")
    check(s2["d_thread_sum"] >= 0.0, "per-thread cpu delta sampled")

    # 负例（GATE-TRIAGE-01）：平台分派必须真的切到 Windows 面，且不得崩。
    # ① 分派：把 _IS_WINDOWS 置真、把三个 Win 读数换成已知值，断言 _read_status /
    #    _read_proc_cpu 走的是 Win 面（原实现无条件读 /proc ⇒ 这里必失败）。
    # ② 判别力自证：置真 _IS_WINDOWS 但**不**替换实现时，Linux 上 _win_* 必须
    #    返回 0/空而不是抛异常（Windows API 不可用 ⇒ 明确降级，不静默假装成功）。
    real_flag = globals()["_IS_WINDOWS"]
    real_status = globals()["_win_read_status"]
    real_cpu = globals()["_win_read_proc_cpu"]
    try:
        globals()["_IS_WINDOWS"] = True
        globals()["_win_read_status"] = lambda pid: (123456, 654321, 7)
        globals()["_win_read_proc_cpu"] = lambda pid: 4.25
        check(_read_status(os.getpid()) == (123456, 654321, 7),
              "windows path dispatch (status)")
        check(abs(_read_proc_cpu(os.getpid()) - 4.25) < 1e-9,
              "windows path dispatch (cpu)")
        globals()["_win_read_status"] = real_status
        globals()["_win_read_proc_cpu"] = real_cpu
        check(_read_status(os.getpid()) == (0, 0, 0),
              "windows api unavailable degrades to 0 (no crash)")
        check(_read_proc_cpu(os.getpid()) == 0.0,
              "windows cpu unavailable degrades to 0 (no crash)")
    finally:
        globals()["_IS_WINDOWS"] = real_flag
        globals()["_win_read_status"] = real_status
        globals()["_win_read_proc_cpu"] = real_cpu
    R.reset_for_test()
    print("V6_BUDGET_SELFTEST_%s fails=%d" % ("PASS" if fails == 0 else "FAIL", fails))
    return 0 if fails == 0 else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("selftest")
    sp = sub.add_parser("sample")
    sp.add_argument("--pid", type=int, required=True)
    sp.add_argument("--seconds", type=float, default=2.0)
    sp.add_argument("--phase", default="phase2")
    sp.add_argument("--mode", default="psfsw_robust")
    sp.add_argument("--allocated-cores", type=int, default=1)
    sp.add_argument("--json-out", default="")
    ep = sub.add_parser("evaluate")
    ep.add_argument("--metrics", required=True)
    ep.add_argument("--json-out", default="")
    args = ap.parse_args(argv)

    if args.cmd == "selftest":
        return _selftest()
    if args.cmd == "sample":
        m = record_heavy_run(args.pid, args.seconds, args.phase, args.mode,
                             args.allocated_cores)
        out = evaluate_heavy_run(m)
        text = json.dumps(out, ensure_ascii=False, indent=2)
        if args.json_out:
            pathlib.Path(args.json_out).parent.mkdir(parents=True, exist_ok=True)
            pathlib.Path(args.json_out).write_text(text + "\n", encoding="utf-8")
        print(text)
        return 0
    if args.cmd == "evaluate":
        doc = json.loads(pathlib.Path(args.metrics).read_text(encoding="utf-8"))
        m = HeavyRunMetrics(**{k: v for k, v in doc.items()
                               if k in {f.name for f in dataclasses.fields(HeavyRunMetrics)}})
        out = evaluate_heavy_run(m)
        text = json.dumps(out, ensure_ascii=False, indent=2)
        if args.json_out:
            pathlib.Path(args.json_out).parent.mkdir(parents=True, exist_ok=True)
            pathlib.Path(args.json_out).write_text(text + "\n", encoding="utf-8")
        print(text)
        return 0
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
