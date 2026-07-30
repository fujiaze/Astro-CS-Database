"""
H-001 资源监测框架

监控 CPU / RAM (RSS+Commit) / Disk I/O / 温度, 提供滚动统计与快照。
设计要点:
  - 不依赖 OS swap, 显式追踪进程 RSS 与系统 Commit
  - 滚动窗口 (默认 60s) 计算 CPU 负载均值/峰值
  - 线程安全: 所有采样加锁
  - 可注入式: 支持注入 mock 采样器用于测试 (不依赖真实硬件)

规范来源: engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md
  ResourceMonitor: CPU、RSS、Commit、I/O、活跃阶段
"""

from __future__ import annotations

import threading
import time
import os
import collections
import json
from dataclasses import dataclass, field, asdict
from typing import Optional, Callable, List, Deque


# ============================================================================
# 资源快照数据结构
# ============================================================================

@dataclass
class ResourceSnapshot:
    """单次资源采样快照 (某一时刻的系统状态)"""
    timestamp_sec: float              # 采样时间戳 (time.time())
    cpu_percent: float                # CPU 使用率 (0-100, 全系统)
    rss_bytes: int                    # 当前进程 RSS (驻留集大小)
    commit_bytes: int                 # 系统已提交内存 (Commit)
    commit_limit_bytes: int           # 系统提交上限 (Commit Limit)
    disk_read_bytes_per_sec: float    # 磁盘读速率
    disk_write_bytes_per_sec: float   # 磁盘写速率
    temperature_c: Optional[float] = None  # CPU/GPU 温度 (无可获取时 None)

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class ResourceSummary:
    """滚动窗口内的资源统计摘要"""
    cpu_mean: float
    cpu_max: float
    cpu_p95: float
    rss_mean: int
    rss_max: int
    commit_mean: int
    commit_max: int
    commit_limit: int
    disk_read_mean: float
    disk_write_mean: float
    temperature_mean: Optional[float]
    n_samples: int
    window_sec: float

    def to_dict(self) -> dict:
        return asdict(self)


# ============================================================================
# 采样器接口 (可注入, 便于测试)
# ============================================================================

class ResourceSampler:
    """资源采样器基类, 子类实现具体平台采集"""

    def sample(self) -> ResourceSnapshot:
        raise NotImplementedError


class PsutilSampler(ResourceSampler):
    """基于 psutil 的真实采样器 (生产环境使用)"""

    def __init__(self, process=None):
        try:
            import psutil
            self._psutil = psutil
            self._process = process or psutil.Process(os.getpid())
            self._last_disk = psutil.disk_io_counters()
            self._last_time = time.time()
        except ImportError:
            raise ImportError(
                "psutil not installed. Install with: pip install psutil"
            )

    def sample(self) -> ResourceSnapshot:
        psutil = self._psutil
        now = time.time()
        cpu = psutil.cpu_percent(interval=None)
        mem = psutil.virtual_memory()
        rss = self._process.memory_info().rss

        # Commit 信息 (Windows: 从 virtual_memory 获取; Linux: 从 /proc/meminfo)
        commit = mem.total - mem.available  # 近似 Commit
        commit_limit = mem.total

        # 磁盘 I/O 速率
        disk = psutil.disk_io_counters()
        dt = now - self._last_time
        if dt > 0 and disk and self._last_disk:
            read_rate = (disk.read_bytes - self._last_disk.read_bytes) / dt
            write_rate = (disk.write_bytes - self._last_disk.write_bytes) / dt
        else:
            read_rate = 0.0
            write_rate = 0.0
        self._last_disk = disk
        self._last_time = now

        # 温度 (psutil.sensors_temperatures 仅 Linux, Windows 返回 {})
        temp = None
        try:
            temps = psutil.sensors_temperatures()
            if temps:
                for name, entries in temps.items():
                    if entries:
                        temp = entries[0].current
                        break
        except (AttributeError, Exception):
            pass

        return ResourceSnapshot(
            timestamp_sec=now,
            cpu_percent=cpu,
            rss_bytes=rss,
            commit_bytes=commit,
            commit_limit_bytes=commit_limit,
            disk_read_bytes_per_sec=read_rate,
            disk_write_bytes_per_sec=write_rate,
            temperature_c=temp,
        )


class MockSampler(ResourceSampler):
    """注入式 mock 采样器, 用于单元测试"""

    def __init__(self, samples: List[ResourceSnapshot]):
        self._samples = list(samples)
        self._idx = 0

    def sample(self) -> ResourceSnapshot:
        if self._idx >= len(self._samples):
            self._idx = 0  # 循环播放
        snap = self._samples[self._idx]
        self._idx += 1
        return snap


# ============================================================================
# ResourceMonitor 核心类
# ============================================================================

class ResourceMonitor:
    """
    资源监测器: 后台线程定期采样, 维护滚动窗口统计。

    用法:
        monitor = ResourceMonitor(sampler=PsutilSampler())
        monitor.start()
        # ... 执行管线 ...
        summary = monitor.get_summary()
        monitor.stop()
    """

    DEFAULT_WINDOW_SEC = 60.0       # 滚动窗口默认 60s
    DEFAULT_SAMPLE_INTERVAL = 0.5   # 默认采样间隔 0.5s

    def __init__(
        self,
        sampler: Optional[ResourceSampler] = None,
        window_sec: float = DEFAULT_WINDOW_SEC,
        sample_interval: float = DEFAULT_SAMPLE_INTERVAL,
    ):
        self._sampler = sampler
        self._window_sec = window_sec
        self._sample_interval = sample_interval
        self._samples: Deque[ResourceSnapshot] = collections.deque()
        self._lock = threading.Lock()
        self._thread: Optional[threading.Thread] = None
        self._running = threading.Event()
        self._active_stages: dict[str, float] = {}  # stage_name -> start_time

    # ------------------------------------------------------------------
    # 生命周期
    # ------------------------------------------------------------------

    def set_sampler(self, sampler: ResourceSampler):
        """设置采样器 (启动前调用)"""
        self._sampler = sampler

    def start(self):
        """启动后台采样线程"""
        if self._running.is_set():
            return
        if self._sampler is None:
            raise RuntimeError("Sampler not set. Call set_sampler() first.")
        self._running.set()
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self):
        """停止后台采样线程"""
        self._running.clear()
        if self._thread:
            self._thread.join(timeout=2.0)
            self._thread = None

    def _loop(self):
        while self._running.is_set():
            try:
                snap = self._sampler.sample()
                with self._lock:
                    self._samples.append(snap)
                    # 裁剪窗口外的样本
                    cutoff = snap.timestamp_sec - self._window_sec
                    while self._samples and self._samples[0].timestamp_sec < cutoff:
                        self._samples.popleft()
            except Exception:
                pass  # 采样失败不中断监测
            time.sleep(self._sample_interval)

    # ------------------------------------------------------------------
    # 手动采样 (无后台线程模式)
    # ------------------------------------------------------------------

    def sample_once(self) -> ResourceSnapshot:
        """手动触发一次采样 (适用于无后台线程场景)"""
        if self._sampler is None:
            raise RuntimeError("Sampler not set")
        snap = self._sampler.sample()
        with self._lock:
            self._samples.append(snap)
            cutoff = snap.timestamp_sec - self._window_sec
            while self._samples and self._samples[0].timestamp_sec < cutoff:
                self._samples.popleft()
        return snap

    # ------------------------------------------------------------------
    # 活跃阶段追踪
    # ------------------------------------------------------------------

    def mark_stage_start(self, stage_name: str):
        """标记某 stage 开始执行"""
        with self._lock:
            self._active_stages[stage_name] = time.time()

    def mark_stage_end(self, stage_name: str):
        """标记某 stage 结束"""
        with self._lock:
            self._active_stages.pop(stage_name, None)

    def get_active_stages(self) -> List[str]:
        """获取当前活跃的 stage 列表"""
        with self._lock:
            return list(self._active_stages.keys())

    # ------------------------------------------------------------------
    # 统计查询
    # ------------------------------------------------------------------

    def get_snapshot(self) -> Optional[ResourceSnapshot]:
        """获取最近一次快照"""
        with self._lock:
            if self._samples:
                return self._samples[-1]
        return None

    def get_samples(self) -> List[ResourceSnapshot]:
        """获取窗口内全部快照 (副本)"""
        with self._lock:
            return list(self._samples)

    def get_summary(self) -> Optional[ResourceSummary]:
        """计算滚动窗口内的统计摘要"""
        with self._lock:
            samples = list(self._samples)

        if not samples:
            return None

        n = len(samples)
        cpu_vals = [s.cpu_percent for s in samples]
        rss_vals = [s.rss_bytes for s in samples]
        commit_vals = [s.commit_bytes for s in samples]
        read_vals = [s.disk_read_bytes_per_sec for s in samples]
        write_vals = [s.disk_write_bytes_per_sec for s in samples]
        temps = [s.temperature_c for s in samples if s.temperature_c is not None]

        sorted_cpu = sorted(cpu_vals)
        p95_idx = int(n * 0.95)
        if p95_idx >= n:
            p95_idx = n - 1

        return ResourceSummary(
            cpu_mean=sum(cpu_vals) / n,
            cpu_max=max(cpu_vals),
            cpu_p95=sorted_cpu[p95_idx],
            rss_mean=sum(rss_vals) // n,
            rss_max=max(rss_vals),
            commit_mean=sum(commit_vals) // n,
            commit_max=max(commit_vals),
            commit_limit=samples[-1].commit_limit_bytes,
            disk_read_mean=sum(read_vals) / n,
            disk_write_mean=sum(write_vals) / n,
            temperature_mean=sum(temps) / len(temps) if temps else None,
            n_samples=n,
            window_sec=self._window_sec,
        )

    def get_cpu_load(self) -> float:
        """获取当前 CPU 负载百分比 (0-100)"""
        snap = self.get_snapshot()
        return snap.cpu_percent if snap else 0.0

    def get_rss_bytes(self) -> int:
        """获取当前进程 RSS"""
        snap = self.get_snapshot()
        return snap.rss_bytes if snap else 0

    def get_commit_bytes(self) -> int:
        """获取系统 Commit 字节数"""
        snap = self.get_snapshot()
        return snap.commit_bytes if snap else 0

    def get_available_memory(self) -> int:
        """获取可用内存 (Commit Limit - Commit)"""
        snap = self.get_snapshot()
        if snap:
            return snap.commit_limit_bytes - snap.commit_bytes
        return 0

    def to_json(self) -> str:
        """导出全部快照为 JSON 字符串"""
        with self._lock:
            data = {
                "samples": [s.to_dict() for s in self._samples],
                "active_stages": dict(self._active_stages),
            }
        return json.dumps(data, indent=2)
