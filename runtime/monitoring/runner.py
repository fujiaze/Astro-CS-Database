"""重任务 run 守卫与自动 monitor 起动器（LOG-002）。

验收：无 monitor 的 `cpu_heavy` run 必须失败（负测）。
本模块提供：
  - `HeavyRunGuard`：cpu_heavy（及可配重类）run 启动前必须已 attach monitor，
    否则 raise MonitorRequired（调用方转 FAIL/非零退出）—— 这就是"无 monitor
    的 cpu_heavy run 失败"的执行点；
  - `run_heavy_with_monitor(...)`：一个自包含的重任务伴随执行助手：自动创建
    ResourceMonitor（同 run_id，1s 采样），驱动"init → 工作 → flush"分段与
    I/O 区间分离（io_phase()/active_phase()），工作函数可从 RT-006 trace
    事件流经 TraceSnapshotObserver 实时获取真实观测（provider/module/workers），
    结束 seal() 写后只读。
  - 采集完成把摘要作为 LOG-001 metric 事件输出（复用 run/task/event/JSONL）。

典型接线（调用方 = 未来 Runtime/executor 集成点，本包不侵入 runtime/pipeline）：
    guard = HeavyRunGuard(resource_class="cpu_heavy", run_id=r)
    monitor = guard.create_monitor(csv_path, interval_s=1.0)   # attach + 建档
    guard.assert_ready()                                        # 失败点
    ...run 真实工作，每步 set_phase/io_phase/active_phase...
    monitor.stop(); monitor.seal()

本模块不启动私有线程池、不读核心数做私有决策、不绕过 ArtifactStore 猜路径；
不触碰 runtime/pipeline、runtime/core、lib/core（只读引用 trace 事件 dict）。
"""
from __future__ import annotations

import pathlib
import time
from typing import Any, Callable, Dict, Optional

from .monitor import PHASES, ResourceMonitor
from .trace_feed import TraceSnapshotObserver, utc_now_s

# 强制 monitor 的资源类（任务规格: heavy/cpu_heavy 自动创建 monitor）
REQUIRE_MONITOR_CLASSES = ("cpu_heavy", "io")


class MonitorRequired(RuntimeError):
    """cpu_heavy（等）run 未附 monitor 时抛出的守卫失败。"""

    def __init__(self, resource_class: str, run_id: str) -> None:
        super().__init__(
            f"MonitorRequired: resource_class={resource_class!r} run_id={run_id!r} "
            "必须有资源 monitor（cpu_heavy 无 monitor 的 run 必须失败）；"
            "请先 HeavyRunGuard.create_monitor()")
        self.resource_class = resource_class
        self.run_id = run_id


class HeavyRunGuard:
    """重任务 run 守卫：cpu_heavy/io run 必须先 attach monitor 才能跑。"""

    def __init__(self, resource_class: str, run_id: str,
                 required_classes=REQUIRE_MONITOR_CLASSES) -> None:
        if not isinstance(run_id, str) or not run_id:
            raise ValueError("run_id 必须为非空安全字符串")
        self.resource_class = resource_class
        self.run_id = run_id
        self.required_classes = tuple(required_classes)
        self._monitor: Optional[ResourceMonitor] = None

    @property
    def requires_monitor(self) -> bool:
        return self.resource_class in self.required_classes

    def create_monitor(self, csv_path: pathlib.Path,
                       interval_s: float = 1.0, **kw) -> ResourceMonitor:
        """自动创建并 attach monitor（建档 header+seed，未开始采样）。

        调用方随后必须 start()/工作/stop()/seal()（或使用 run_heavy_with_monitor）。
        """
        m = ResourceMonitor(self.run_id, csv_path, interval_s=interval_s, **kw)
        m.begin()
        self._monitor = m
        return m

    def attach(self, monitor: ResourceMonitor) -> None:
        if monitor.run_id != self.run_id:
            raise ValueError(
                f"monitor run_id {monitor.run_id!r} != guard run_id {self.run_id!r}")
        self._monitor = monitor

    def assert_ready(self) -> None:
        """无 monitor 的 cpu_heavy/io run 在此失败（验收负测执行点）。"""
        if self.requires_monitor and self._monitor is None:
            raise MonitorRequired(self.resource_class, self.run_id)


def run_heavy_with_monitor(
        resource_class: str,
        run_id: str,
        csv_path: pathlib.Path,
        work: Callable[[ResourceMonitor, Dict[str, Any]], None],
        *,
        interval_s: float = 1.0,
        init_work: Optional[Callable[[ResourceMonitor], None]] = None,
        io_work: Optional[Callable[[ResourceMonitor], None]] = None,
        flush_work: Optional[Callable[[ResourceMonitor], None]] = None,
        observer: Optional[TraceSnapshotObserver] = None,
        cpu_burn_seconds: float = 0.0,
        host: str = "", commit: str = "") -> Dict[str, Any]:
    """自包含重任务伴随执行：cpu_heavy run 自动创建 monitor 并强制附随。

    流程（I/O 区间与初始化区间分开记录，各段独立样本标签）：
      begin（建档）→ init_phase + init_work（init 区间）→ io_phase +
      io_work（io 区间）→ active_phase + [cpu_burn 合成负载] + work
      （active 区间）→ flush_phase + flush_work（flush 区间）→ stop + seal。
    区间切换强制边界采样：即使 init/io 段短于采样间隔也会落样本行。

    返回 machine 摘要 {run_id, resource_class, csv, row_count, phases,
    monitor_errors[], interval_stats{min,mean,max}}。

    失败语义：任何阶段异常向外抛（调用方 FAIL）；monitor 采样线程内部错误
    记录在 monitor_errors 中（不静默吞掉）。
    """
    guard = HeavyRunGuard(resource_class, run_id)
    monitor = guard.create_monitor(csv_path, interval_s=interval_s,
                                   observer=observer)
    guard.assert_ready()
    obs: Dict[str, Any] = {}
    monitor.start()  # 后台采样线程（每秒）立即开始；init 区间也被记录
    try:
        monitor.init_phase()
        if init_work is not None:
            init_work(monitor)
        monitor.io_phase()
        if io_work is not None:
            io_work(monitor)
        monitor.active_phase()
        if cpu_burn_seconds and cpu_burn_seconds > 0:
            _burn_cpu(cpu_burn_seconds)
        if observer is not None:
            obs = observer.observe()
        work(monitor, obs)
        monitor.flush_phase()
        if flush_work is not None:
            flush_work(monitor)
    finally:
        monitor.stop()
        monitor.seal()

    intervals = [i for i in monitor.intervals() if i]
    interval_stats: Dict[str, float] = {}
    if intervals:
        interval_stats = {
            "min_s": min(intervals),
            "mean_s": sum(intervals) / len(intervals),
            "max_s": max(intervals),
        }
    return {
        "run_id": run_id,
        "resource_class": resource_class,
        "csv": str(csv_path),
        "row_count": monitor.row_count(),
        "phases": monitor.phases_seen(),
        "monitor_errors": monitor.error_messages,
        "interval_stats": interval_stats,
        "provider": obs.get("provider", ""),
        "module": obs.get("module", ""),
    }


def _burn_cpu(seconds: float) -> None:
    """合成 cpu_heavy 负载（测试/演示用；-j1 起，避免 2c2g 争抢）。

    纯 Python 浮点累加（不产生科学产物，不调科学公式——本模块禁止
    scientific_change）。GIL 限制下单线程约 1 核；多线程由调用方控制。
    """
    end = time.monotonic() + seconds
    acc = 0.0
    i = 0
    while time.monotonic() < end:
        acc += (i * 0.000001) % 1.0
        i += 1
    # 防止优化器把循环去掉（acc 未使用会被 CPython 保留但保留引用防误判）
    if acc == -1.0:  # pragma: no cover - 永不发生
        raise RuntimeError("unreachable")


__all__ = ["HeavyRunGuard", "MonitorRequired", "run_heavy_with_monitor",
           "REQUIRE_MONITOR_CLASSES"]
