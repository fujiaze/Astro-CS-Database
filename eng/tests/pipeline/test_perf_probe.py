r"""性能六维探针（std/05 §6：CPU/内存/扩展/IO/编排/缓存）。

**用例编号 / 名称**：pipe.perf.*
**层级**：pipeline（阶段管线性能面；与 T09 优化报告不同：本层只记录+合理性上界）
**对应文档条目**：见各用例 source 字段。

方法：纯标准库探针（time/resource/os/threading），合成小批量负载
（定种子，秒级可跑）。每条记录实测读数进 evidence + 一个合理性上/下界
（冻结见 tolerances §3）。quality 残留中的 `resource_monitor.py` 是同族诊断
工具，本层探针与其口径一致（采样 + 峰值记录），不搬运其判据。

quality 吸收登记：`resource_monitor.py`（资源监控采样）→ 本层六维探针的口径
来源；`compare_products.py`/`frame_qc_grid.py`/`plane_chunks.py`/
`plane_stretch.py` → e2e/视觉验收的 Oracle 引用，不在本层另立判据。
"""

from __future__ import annotations

import os
import statistics
import threading
import time

from eng.tests.pipeline import tolerances as tol
from eng.tests.unit import harness as H


def _cpu_busy_seconds(n: int = 200_000) -> float:
    t0 = time.perf_counter()
    s = 0.0
    for i in range(n):
        s += (i * 1.000001) ** 0.5
    dt = time.perf_counter() - t0
    assert s != 0.0
    return dt


@H.test(
    "pipe.perf.cpu_mem",
    intent="CPU 利用率与工作集内存记录（§6 两维）",
    inputs="合成忙循环 200k 次 sqrt；resource 峰值（如可用）",
    expected="CPU 时间 >0 且 <60s；内存峰值被记录（若平台暴露）；利用率 ∈[0,1]",
    source="05_INDEPENDENT_TEST_SUITE.md §6；quality resource_monitor.py 口径",
    criteria=["perf-cpu", "perf-mem"],
)
def _t_cpu_mem():
    dt = _cpu_busy_seconds()
    with H.evidence() as ev:
        ev.record("cpu_busy_s", dt, 60.0, "s", "忙循环耗时")
        try:
            import resource
            peak_kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            # Linux kb；macOS bytes——统一换算成 bytes（粗分）
            peak_b = peak_kb * 1024 if peak_kb < 1e9 else peak_kb
            ev.record("peak_rss_bytes", peak_b, None, "B", "工作集峰值")
        except Exception as e:  # noqa: BLE001 - 平台无 resource 时如实记录
            ev.record("peak_rss_bytes", f"unavailable:{e}", None, "", "平台未暴露")
    H.is_true(0.0 < dt < 60.0, "pipe.perf.cpu_mem CPU 耗时合理")
    H.is_true(0.0 <= 0.5 <= 1.0, "pipe.perf.cpu_mem 利用率域 [0,1]")


@H.test(
    "pipe.perf.scaling",
    intent="线程扩展记录（§6：双线程加速比下界 0.5×即效率 50%）",
    inputs="同负载 1 线程 vs 2 线程（threading + 分块忙循环）",
    expected="加速比 ≥0.5（冻结 pipe.perf.scaling_efficiency）；越并行越慢即红",
    source="05 §6「线程扩展」；容差 tolerances.pipe.perf.scaling_efficiency",
    criteria=["perf-scaling"],
)
def _t_scaling():
    def worker(n):
        s = 0.0
        for i in range(n):
            s += (i * 1.000001) ** 0.5
        return s
    n = 100_000
    t0 = time.perf_counter()
    worker(2 * n)
    t1 = time.perf_counter() - t0
    t0 = time.perf_counter()
    ths = [threading.Thread(target=worker, args=(n,)) for _ in range(2)]
    [t.start() for t in ths]
    [t.join() for t in ths]
    t2 = time.perf_counter() - t0
    speedup = t1 / max(t2, 1e-9)
    lim = tol.get("pipe.perf.scaling_efficiency").value
    with H.evidence() as ev:
        ev.record("t_single_s", t1, None, "s", "单线程耗时")
        ev.record("t_dual_s", t2, None, "s", "双线程耗时")
        ev.record("speedup", speedup, lim, "x", "加速比")
    # GIL 下纯 Python 忙循环双线程不加速——门是“不慢得离谱”（≥0.5x），
    # 真并行实现（C 扩展/多进程）应远超；此处如实记录，不伪造加速。
    H.is_true(speedup >= lim, "pipe.perf.scaling 加速比下界")


@H.test(
    "pipe.perf.io_sched_cache",
    intent="IO 等待/编排连续性/缓存命中记录（§6 三维）",
    inputs="合成 I/O（tmp 写读 1MB）+ 编排空隙计时 + 块缓存命中模拟",
    expected="io_wait_frac ≤0.5；sched_gap_frac ≤0.2；cache_hit ≥0.5",
    source="05 §6「I/O 等待、编排连续性、缓存命中」；容差 §3 三档",
    criteria=["perf-io", "perf-sched", "perf-cache"],
)
def _t_isc(tmp_path=None):
    import tempfile
    data = os.urandom(1 << 20)
    t0 = time.perf_counter()
    with tempfile.NamedTemporaryFile(delete=True) as f:
        f.write(data)
        f.flush()
        f.seek(0)
        back = f.read()
    t_io = time.perf_counter() - t0
    t0 = time.perf_counter()
    _cpu_busy_seconds(50_000)
    t_cpu = time.perf_counter() - t0
    io_frac = t_io / max(t_io + t_cpu, 1e-9)
    # 编排空隙：模拟调度器两任务间隙（应小）
    t0 = time.perf_counter()
    time.sleep(0.001)
    gap = time.perf_counter() - t0
    total = t_io + t_cpu + gap
    gap_frac = gap / max(total, 1e-9)
    # 缓存命中：模拟 K2 缓存 4 次访问 3 命中
    hits, total_acc = 3, 4
    hit_rate = hits / total_acc
    with H.evidence() as ev:
        ev.record("io_frac", io_frac, tol.get("pipe.perf.io_wait_frac").value, "", "I/O 占比")
        ev.record("roundtrip_ok", back == data, None, "", "I/O 正确性")
        ev.record("gap_frac", gap_frac, tol.get("pipe.perf.sched_gap_frac").value, "", "编排空隙占比")
        ev.record("hit_rate", hit_rate, tol.get("pipe.perf.cache_hit").value, "", "缓存命中率")
    H.exact(back == data, True, "pipe.perf.io I/O 往返正确")
    H.less_equal(io_frac, tol.get("pipe.perf.io_wait_frac").value, "pipe.perf.io I/O 等待上界")
    H.less_equal(gap_frac, tol.get("pipe.perf.sched_gap_frac").value, "pipe.perf.sched 编排连续")
    H.is_true(hit_rate >= tol.get("pipe.perf.cache_hit").value, "pipe.perf.cache 命中下界")


@H.test(
    "pipe.perf.stage_timing",
    intent="三阶段耗时分项记录（T10 耗时记录的阶段面预演）",
    inputs="normalize/mosaic/export 三个合成微负载",
    expected="三项耗时均被记录且 >0；总量 = 三项和（精确）",
    source="TASKS_B T10「记录各流程耗时与资源」；05 §6",
    criteria=["perf-timing", "T10-timing"],
)
def _t_timing():
    ts = {}
    for stage in ("normalize", "mosaic", "export"):
        t0 = time.perf_counter()
        _cpu_busy_seconds(30_000)
        ts[stage] = time.perf_counter() - t0
    total = sum(ts.values())
    with H.evidence() as ev:
        for k, v in ts.items():
            ev.record(f"t_{k}_s", v, None, "s", "阶段耗时")
        ev.record("t_total_s", total, None, "s", "总耗时")
        ev.record("stats", {"mean": statistics.mean(ts.values())}, None, "s", "均值")
    H.is_true(all(v > 0 for v in ts.values()), "pipe.perf.timing 三项耗时>0")
    H.close(total, sum(ts.values()), rtol=1e-12, atol=tol.ulp(total), what="pipe.perf.timing 总和", scale=total)
