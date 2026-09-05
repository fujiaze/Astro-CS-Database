# -*- coding: utf-8 -*-
"""V8-CI-003 资源探测库（owner=SA-CI-32，纯 stdlib，Linux /proc + /sys）。

职责（tasks/02_CI_TASKS.md V8-CI-003）：
  读取 affinity/cpuset/cgroup quota、逻辑/物理核和可用内存，动态计算并发；
  不得硬编码核数。任何来源缺失都必须有 fallback，并在结果里标注 source
  （"sched_getaffinity" / "cpu_count" / "procfs" / "sysfs" / "cgroup2" /
   "cgroup1" / "fallback:..." / "unavailable"）。

probe() 返回结构化 dict（字段与 source 成对出现）：
  cpu_affinity            os.sched_getaffinity(0) 集合大小；不可得时 fallback os.cpu_count()
  cpu_logical             os.cpu_count()；不可得时 /proc/cpuinfo processor 行计数
  cpu_physical            /proc/cpuinfo (physical id, core id) 去重；推导不出时
                          尝试 sysfs cpu*/topology；仍不可得置 None（不报错）
  cgroup_quota_cores      cgroup v2 <cgroup>/cpu.max（"max" 置 None）；
                          v1 回退 cpu.cfs_quota_us / cpu.cfs_period_us；
                          读不到/无限制置 None（不报错）
  mem_total_bytes         /proc/meminfo MemTotal（kB→字节）
  mem_available_bytes     /proc/meminfo MemAvailable（缺失置 None）
  effective_cpu_cores     affinity ∩ quota 语义：min(affinity, quota>0 ? quota : +inf)
  max_workers             由 effective_cpu_cores 推导（1 worker/effective core，
                          封顶常量 WORKERS_PER_CORE=1.0，下限 1）——无硬编码核数

probe() 可注入 proc_root/sys_root/affinity_fn/cpu_count_fn 以便单测构造假 /proc、
/sys 树（见 ci/tests/test_resource_probe.py）。
"""
from __future__ import annotations

import math
import os
from pathlib import Path
from typing import Callable, Optional

__all__ = [
    "WORKERS_PER_CORE",
    "parse_cpu_max",
    "parse_cfs_quota",
    "parse_cpuinfo_logical",
    "parse_cpuinfo_physical",
    "parse_meminfo",
    "compute_effective",
    "derive_max_workers",
    "read_cgroup_quota",
    "detect_cpu_physical",
    "probe",
]

# 并发封顶常量：每 effective core 建议 1 个 worker（只封顶倍率，不硬编码核数）。
WORKERS_PER_CORE = 1.0

_FALLBACK_PERIOD_US = 100000.0  # cgroup 默认周期（quota/period 语义，非核数）


def _read_text(path) -> Optional[str]:
    """读文本文件；任何 OSError/缺失 → None（探测库永不抛错）。"""
    try:
        return Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None


# ---------------------------------------------------------------- 解析器 ----
def parse_cpu_max(text: Optional[str]) -> Optional[float]:
    """解析 cgroup v2 cpu.max 内容 → 核数（"max"/无效/负值 → None）。

    "200000 100000" → 2.0；"max 100000" → None；"" / 垃圾 → None。
    """
    if not text:
        return None
    parts = text.split()
    if not parts:
        return None
    if parts[0].lower() == "max":
        return None
    try:
        quota = float(parts[0])
    except ValueError:
        return None
    if quota <= 0:
        return None
    period = _FALLBACK_PERIOD_US
    if len(parts) > 1:
        try:
            period = float(parts[1])
        except ValueError:
            return None
    if period <= 0:
        return None
    return quota / period


def parse_cfs_quota(quota_text: Optional[str], period_text: Optional[str]) -> Optional[float]:
    """解析 cgroup v1 cpu.cfs_quota_us / cpu.cfs_period_us → 核数（quota<=0 → None）。"""
    if not quota_text:
        return None
    try:
        quota = float(quota_text.split()[0])
    except (ValueError, IndexError):
        return None
    if quota <= 0:  # -1 = 无限制
        return None
    period = _FALLBACK_PERIOD_US
    if period_text:
        try:
            period = float(period_text.split()[0])
        except (ValueError, IndexError):
            period = _FALLBACK_PERIOD_US
    if period <= 0:
        return None
    return quota / period


def parse_cpuinfo_logical(text: Optional[str]) -> int:
    """/proc/cpuinfo 中 processor: 行计数（无 → 0）。"""
    if not text:
        return 0
    return sum(1 for line in text.splitlines() if line.startswith("processor"))


def parse_cpuinfo_physical(text: Optional[str]) -> Optional[int]:
    """/proc/cpuinfo (physical id, core id) 去重对计数 → 物理核数。

    文件缺失或无 core id 字段 → None（推导不出不报错）。超线程共享 core id，
    因此去重对数即物理核数。
    """
    if not text:
        return None
    pairs: set[tuple[str, str]] = set()
    physical_id: Optional[str] = None
    core_id: Optional[str] = None
    saw_core_field = False
    for line in text.splitlines():
        if line.startswith("processor"):
            physical_id = None
            core_id = None
            continue
        key, sep, value = line.partition(":")
        if not sep:
            continue
        key = key.strip()
        value = value.strip()
        if key == "physical id":
            physical_id = value
        elif key == "core id":
            core_id = value
            saw_core_field = True
            if physical_id is not None:
                pairs.add((physical_id, core_id))
    if not saw_core_field or not pairs:
        return None
    return len(pairs)


def parse_meminfo(text: Optional[str]) -> dict:
    """/proc/meminfo → {"mem_total_bytes", "mem_available_bytes"}（kB→字节；缺失 → None）。"""
    out = {"mem_total_bytes": None, "mem_available_bytes": None}
    if not text:
        return out
    for line in text.splitlines():
        key, sep, value = line.partition(":")
        if not sep:
            continue
        key = key.strip()
        fields = value.split()
        if not fields:
            continue
        try:
            kb = int(fields[0])
        except ValueError:
            continue
        if key == "MemTotal":
            out["mem_total_bytes"] = kb * 1024
        elif key == "MemAvailable":
            out["mem_available_bytes"] = kb * 1024
    return out


# ------------------------------------------------------------- 语义计算 ----
def compute_effective(affinity: int, quota: Optional[float]) -> tuple[float, str]:
    """affinity ∩ quota 语义 → (effective_cpu_cores, source)。

    quota 为 None/<=0（无限制或不可得）→ effective=affinity，source="affinity"；
    否则 effective=min(affinity, quota)，source 取生效约束（quota 更小 → "quota"）。
    """
    aff = max(1, int(affinity))
    if quota is not None and quota > 0:
        eff = min(float(aff), float(quota))
        return eff, ("quota" if float(quota) < float(aff) else "affinity")
    return float(aff), "affinity"


def derive_max_workers(effective_cpu_cores: float) -> int:
    """由 effective core 推导并发建议：floor(effective × WORKERS_PER_CORE)，下限 1。"""
    workers = int(math.floor(float(effective_cpu_cores) * WORKERS_PER_CORE + 1e-9))
    return max(1, workers)


# --------------------------------------------------------------- 探测源 ----
def _cgroup_relative_path(proc_root: Path) -> Optional[tuple[str, str]]:
    """从 /proc/self/cgroup 取本进程 cgroup 相对路径。

    返回 (v2_path 或 "", v1_cpu_path 或 "")；读不到 → None。
    v2 行形如 "0::/system.slice/x.service"；v1 cpu 行形如 "N:cpu,cpuacct:/path"。
    """
    text = _read_text(proc_root / "self" / "cgroup")
    if not text:
        return None
    v2_path: Optional[str] = None
    v1_cpu_path: Optional[str] = None
    for line in text.splitlines():
        parts = line.split(":", 2)
        if len(parts) != 3:
            continue
        _hier, controllers, path = parts
        if controllers == "" and v2_path is None:  # "0::/path" → v2 unified
            v2_path = path
        ctrl_list = controllers.split(",")
        if "cpu" in ctrl_list and v1_cpu_path is None:
            v1_cpu_path = path
    if v2_path is None and v1_cpu_path is None:
        return None
    return (v2_path or "", v1_cpu_path or "")


def read_cgroup_quota(proc_root="/proc", sys_root="/sys") -> tuple[Optional[float], Optional[str]]:
    """读本进程 cgroup CPU 配额 → (核数, source)。

    优先 cgroup v2（<root>/cpu.max，含 /proc/self/cgroup 相对路径），
    回退 cgroup v1（cpu.cfs_quota_us / cpu.cfs_period_us）。
    读不到/无限制 → (None, None)。
    """
    proc_root = Path(proc_root)
    sys_root = Path(sys_root)
    rel = _cgroup_relative_path(proc_root)

    # --- v2 ---
    candidates = [sys_root / "cpu.max"]
    if rel and rel[0]:
        candidates.insert(0, sys_root / rel[0].lstrip("/") / "cpu.max")
    for cand in candidates:
        value = parse_cpu_max(_read_text(cand))
        if value is not None:
            return value, "cgroup2"

    # --- v1 ---
    v1_base = sys_root / "cpu"
    quota_paths = [v1_base]
    if rel and rel[1]:
        quota_paths.insert(0, sys_root / "cpu" / rel[1].lstrip("/"))
    for base in quota_paths:
        value = parse_cfs_quota(_read_text(base / "cpu.cfs_quota_us"),
                                _read_text(base / "cpu.cfs_period_us"))
        if value is not None:
            return value, "cgroup1"

    return None, None


def _sysfs_cpu_physical(sys_root: Path) -> Optional[int]:
    """sysfs /sys/devices/system/cpu/cpuN/topology → 物理核数（不可得 → None）。"""
    topo_root = sys_root / "devices" / "system" / "cpu"
    if not topo_root.is_dir():
        return None
    pairs: set[tuple[str, str]] = set()
    try:
        cpu_dirs = sorted(p for p in topo_root.glob("cpu[0-9]*") if p.is_dir())
    except OSError:
        return None
    for cpu_dir in cpu_dirs:
        pkg = _read_text(cpu_dir / "topology" / "physical_package_id")
        core = _read_text(cpu_dir / "topology" / "core_id")
        if pkg is None or core is None:
            continue
        pairs.add((pkg.strip(), core.strip()))
    if not pairs:
        return None
    return len(pairs)


def detect_cpu_physical(proc_root="/proc", sys_root="/sys") -> tuple[Optional[int], Optional[str]]:
    """物理核探测：优先 /proc/cpuinfo，fallback sysfs topology；都不可得 → (None, None)。"""
    from_cpuinfo = parse_cpuinfo_physical(_read_text(Path(proc_root) / "cpuinfo"))
    if from_cpuinfo:
        return from_cpuinfo, "procfs"
    from_sysfs = _sysfs_cpu_physical(Path(sys_root))
    if from_sysfs:
        return from_sysfs, "sysfs"
    return None, None


# ------------------------------------------------------------------ probe ----
def probe(proc_root="/proc", sys_root="/sys",
          affinity_fn: Optional[Callable[[], object]] = None,
          cpu_count_fn: Optional[Callable[[], Optional[int]]] = None) -> dict:
    """探测本进程可用计算资源并返回结构化 dict（每个数值字段配 *_source 标注）。

    任何来源缺失都有 fallback 且不抛错；推导不出的字段（cpu_physical/quota/
    mem_available）置 None。affinity_fn / cpu_count_fn 可注入用于单测。
    """
    proc_root = Path(proc_root)
    sys_root = Path(sys_root)

    # --- cpu_affinity：sched_getaffinity → fallback cpu_count → 1 ---
    affinity_source = "sched_getaffinity"
    affinity = None
    if affinity_fn is None:
        affinity_fn = lambda: os.sched_getaffinity(0)  # noqa: E731
    try:
        affinity = len(set(affinity_fn()))
    except (AttributeError, OSError, TypeError, ValueError):
        affinity = None
    if not affinity or affinity < 1:
        affinity_source = "fallback:cpu_count"
        if cpu_count_fn is None:
            cpu_count_fn = os.cpu_count
        affinity = cpu_count_fn() or 1

    # --- cpu_logical：os.cpu_count → /proc/cpuinfo → affinity ---
    logical = None
    logical_source = "unavailable"
    if cpu_count_fn is None:
        cpu_count_fn = os.cpu_count
    try:
        logical = cpu_count_fn()
    except Exception:  # noqa: BLE001 —— 探测库不抛错
        logical = None
    if logical and logical >= 1:
        logical_source = "cpu_count"
    else:
        logical = parse_cpuinfo_logical(_read_text(proc_root / "cpuinfo"))
        if logical >= 1:
            logical_source = "procfs"
        else:
            logical, logical_source = affinity, "fallback:affinity"

    # --- cpu_physical：procfs → sysfs → None ---
    physical, physical_source = detect_cpu_physical(proc_root, sys_root)

    # --- cgroup quota：v2 → v1 → None ---
    quota, quota_source = read_cgroup_quota(proc_root, sys_root)

    # --- memory：/proc/meminfo ---
    meminfo = parse_meminfo(_read_text(proc_root / "meminfo"))
    mem_source = "procfs" if meminfo["mem_total_bytes"] is not None else "unavailable"

    # --- 语义计算（非硬编码）---
    effective, effective_source = compute_effective(affinity, quota)
    max_workers = derive_max_workers(effective)

    return {
        "cpu_affinity": affinity,
        "cpu_affinity_source": affinity_source,
        "cpu_logical": logical,
        "cpu_logical_source": logical_source,
        "cpu_physical": physical,
        "cpu_physical_source": physical_source or "unavailable",
        "cgroup_quota_cores": quota,
        "cgroup_quota_source": quota_source or "unavailable",
        "mem_total_bytes": meminfo["mem_total_bytes"],
        "mem_available_bytes": meminfo["mem_available_bytes"],
        "mem_source": mem_source,
        "effective_cpu_cores": effective,
        "effective_source": effective_source,
        "max_workers": max_workers,
        "max_workers_source": "derived",
    }
