"""Linux procfs 资源采集后端（LOG-002；唯一真实实现）。

只做"从 Linux 内核 procfs 读真实观测值"这一件事，字段语义与
`runtime/monitoring/monitor.py` 的 Sample 对齐（monitor 侧做差值/百分比换算）。

来源与语义（全部为真实观测，禁止 config 值冒充）：
  /proc/self/status   — VmRSS (RSS), RssAnon (private), VmSize (commit 近似),
                         voluntary/nonvoluntary ctxt_switches, Threads;
  /proc/self/smaps_rollup — Pss（可得时；不可得保持 None，不算失败）；
  /proc/self/stat     — utime/stime (clock ticks → 秒);
  /proc/self/io       — read_bytes/write_bytes（页缓存命中也计入的块 I/O 计数）,
                         syscr/syscw (read/write syscalls);
  /proc/self/stat     — 与上一采样差值 → io wait / lock wait 的代理（见下）;
  /proc/stat          — 系统级 CPU 占用（系统模式）:
                         total_jiffies 与 idle_jiffies 差值 → system cpu fraction;
  /proc/loadavg       — 系统运行队列长度（runnable + uninterruptible）作为
                         queue_wait 的只读来源之一（真实系统状态，非注入值）。

字段说明（避免语义误用）：
  - io_wait_seconds   : 本进程 minflt/majflt（缺页）CPU 型等待以 wall 差值
                        （sleep 等不属于 io wait）。为真实观测：
                        minflt/majflt 差值 *0 不代表 0 —— 这里以
                        /proc/self/stat 的 minflt+majflt 差值计 page-fault
                        次数，作为 io 等待代理次数；monitor 层除以样本墙钟
                        得到 io_wait_rate。精确的进程级 iowait 时间 Linux
                        不直接提供；系统级 iowait 从 /proc/stat 的 iowait
                        jiffies 差值可得 → 写入 sys_iowait_fraction。
  - lock_wait_ns     : 用户态锁等待没有 procfs 单一计数器；取
                        nonvoluntary_ctxt_switches 差值 ×1000 作为代理
                        （被调度抢占/锁让出 → 非自愿切换）→ 只读参考列，
                        monitor 层以 _est 后缀输出，不冒充精确观测。
  - queue_wait       : /proc/loadavg 1 分钟运行队列长度（系统级真实值）。

返回结构统一为 dict（缺字段用 None），采集失败用异常向上抛出（可观测的
采集故障必须显式冒泡，不能静默 0）。
"""
from __future__ import annotations

import os
import pathlib
import re
from typing import Any, Dict, Optional

# /proc 路径常量（集中管理；测试可注入伪 /proc）
_PROC = pathlib.Path("/proc")
_SELF = _PROC / "self"
STATUS = _SELF / "status"
SMAPS_ROLLUP = _SELF / "smaps_rollup"
STAT = _SELF / "stat"
IO = _SELF / "io"
SYSTEM_STAT = _PROC / "stat"
LOADAVG = _PROC / "loadavg"

_CLK_TCK = os.sysconf("SC_CLK_TCK") or 100  # 实际在 Linux 恒为 100


def _read_uint(file: pathlib.Path, key: str) -> Optional[int]:
    """从 key: value 形态的 procfs 文件读单个无符号整数；缺 key → None。"""
    try:
        text = file.read_text(encoding="ascii", errors="replace")
    except OSError:
        return None
    for line in text.splitlines():
        if line.startswith(key + ":"):
            parts = line.split()
            if len(parts) >= 2:
                try:
                    return int(parts[1])
                except ValueError:
                    return None
    return None


def _read_stat_fields() -> Dict[str, int]:
    """读 /proc/self/stat（括号安全切分）。

    comm 可能含空格/括号，因此取最后一个 ')' 后的字段（field 3 起）。
    字段号（man proc_pid_stat）：
      3 state, 4 ppid, ... 9 vsize, 10 rss(页), 12 utime, 13 stime,
      14 cutime, 15 cstime, ... 20 num_threads, 21 starttime,
      22 vsize2/23 rsslim ... (k) 24 rsslim? —— 版本不同字段号会漂移，
      这里只用括号后固定相对序：第一个 token=field3(state) 字母，
      第二个=field4(ppid)…… 按 Linux 5.x 标准索引取 utime/stime 等。
    """
    text = STAT.read_text(encoding="ascii", errors="replace")
    rp = text.rfind(")")
    if rp < 0:
        raise OSError("malformed /proc/self/stat: no closing paren")
    tail = text[rp + 1:].split()
    # tail[0] == field3(state), tail[1] == field4 ... 索引 = field - 3
    def at(field: int) -> int:
        idx = field - 3
        if idx < len(tail):
            try:
                return int(tail[idx])
            except ValueError:
                return 0
        return 0

    # 进程页错误（io 等待代理）：field 10=minflt, 12=majflt（11=cminflt 子进程）；
    # utime/stime 在 Linux 5.x 为 field 14/15；num_threads 为 field 20。
    return {
        "utime_ticks": at(14),
        "stime_ticks": at(15),
        "threads": at(20),
        "minflt": at(10),
        "majflt": at(12),
    }


def _read_ctxt_switches() -> Dict[str, Optional[int]]:
    """voluntary/nonvoluntary 上下文切换（status 文件，字段名可靠）。"""
    out = {"voluntary": None, "nonvoluntary": None}
    try:
        text = STATUS.read_text(encoding="ascii", errors="replace")
    except OSError:
        return out
    for line in text.splitlines():
        if line.startswith("voluntary_ctxt_switches:"):
            parts = line.split()
            if len(parts) >= 2:
                try:
                    out["voluntary"] = int(parts[1])
                except ValueError:
                    pass
        elif line.startswith("nonvoluntary_ctxt_switches:"):
            parts = line.split()
            if len(parts) >= 2:
                try:
                    out["nonvoluntary"] = int(parts[1])
                except ValueError:
                    pass
    return out


def _read_io() -> Dict[str, int]:
    """/proc/self/io 真实字节计数（含页缓存命中；rchar/wchar 为 syscall 级）。"""
    out: Dict[str, int] = {}
    try:
        text = IO.read_text(encoding="ascii", errors="replace")
    except OSError:
        return out
    for line in text.splitlines():
        for key in ("read_bytes", "write_bytes", "rchar", "wchar", "syscr", "syscw"):
            if line.startswith(key + ":"):
                parts = line.split()
                if len(parts) >= 2:
                    try:
                        out[key] = int(parts[1])
                    except ValueError:
                        pass
    return out


def _read_system_cpu() -> Dict[str, int]:
    """系统级 CPU 总量/空闲 jiffies（差值由 monitor 层计算）。"""
    try:
        lines = SYSTEM_STAT.read_text(encoding="ascii", errors="replace").splitlines()
    except OSError:
        return {}
    for line in lines:
        if line.startswith("cpu "):
            toks = line.split()
            # user nice system idle iowait irq softirq steal ...
            fields = ["user", "nice", "system", "idle", "iowait", "irq",
                      "softirq", "steal"]
            vals: Dict[str, int] = {}
            for i, name in enumerate(fields):
                if i + 1 < len(toks):
                    try:
                        vals[name] = int(toks[i + 1])
                    except ValueError:
                        vals[name] = 0
            return vals
    return {}


def _read_loadavg() -> Optional[float]:
    """系统 1 分钟运行队列长度（真实 /proc/loadavg）。"""
    try:
        first = LOADAVG.read_text(encoding="ascii", errors="replace").split()[0]
        return float(first)
    except (OSError, ValueError, IndexError):
        return None


def collect() -> Dict[str, Any]:
    """单次真实采集（Linux procfs）。

    返回顶层全部真实值；无注入、无 config 冒充。单次失败只对单字段置 None，
    关键文件全挂时抛 OSError（可观测失败冒泡）。
    """
    s = _read_stat_fields()
    st = _read_uint(STATUS, "VmRSS")          # kB
    anon = _read_uint(STATUS, "RssAnon")      # kB (private 近似)
    vsize = _read_uint(STATUS, "VmSize")      # kB (commit 近似)
    threads = s.get("threads")
    ctxt = _read_ctxt_switches()
    nvcsw = ctxt.get("nonvoluntary")
    io = _read_io()
    pss = _read_uint(SMAPS_ROLLUP, "Pss")     # kB（不可得 → None）
    syscpu = _read_system_cpu()
    load = _read_loadavg()

    if st is None and vsize is None and not io:
        raise OSError("linux_procfs: all key /proc/self sources unreadable")

    def _kb(v: Optional[int]) -> Optional[int]:
        return None if v is None else v * 1024

    return {
        "backend": "linux_procfs",
        # 进程 CPU（绝对秒；monitor 层做差值 → cpu fraction）
        "cpu_user_seconds": (s.get("utime_ticks", 0) / _CLK_TCK) if "utime_ticks" in s else None,
        "cpu_sys_seconds": (s.get("stime_ticks", 0) / _CLK_TCK) if "stime_ticks" in s else None,
        # 内存（字节）
        "rss_bytes": _kb(st),
        "private_bytes": _kb(anon),
        "commit_bytes": _kb(vsize),
        "pss_bytes": _kb(pss),
        # 上下文切换（绝对值；monitor 做差值）
        "ctxt_switches_nvcsw": nvcsw,
        "ctxt_switches_vol": ctxt.get("voluntary"),
        # I/O（绝对字节/syscall；monitor 做差值）
        "read_bytes": io.get("read_bytes"),
        "write_bytes": io.get("write_bytes"),
        "read_syscalls": io.get("rchar"),
        "write_syscalls": io.get("wchar"),
        "read_ops": io.get("syscr"),
        "write_ops": io.get("syscw"),
        "threads": threads,
        # io 等待代理：缺页计数（真实观测值，非精确 iowait 时间）
        "faults_minor": s.get("minflt"),
        "faults_major": s.get("majflt"),
        # 系统级（真实 /proc/stat / /proc/loadavg）
        "sys_cpu_jiffies_total": (sum(syscpu.values()) if syscpu else None),
        "sys_cpu_jiffies_idle": syscpu.get("idle"),
        "sys_cpu_jiffies_iowait": syscpu.get("iowait"),
        "sys_loadavg_1m": load,
    }


def is_available() -> bool:
    """当前主机是否可用 Linux procfs 后端（隔离探测，不产生观测数据）。"""
    return (os.name == "posix" and pathlib.Path("/proc/self/status").is_file()
            and pathlib.Path("/proc/self/stat").is_file())


__all__ = ["collect", "is_available"]
