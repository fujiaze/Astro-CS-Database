"""LOG-002 资源监控伴随器核心（同一 run ID 每秒采集 + 原始 CSV 不可伪造）。

职责（tasks/03_RUNTIME_DATA_IO_TASKS.md LOG-002）：
  - 重任务（cpu_heavy 等）run 自动创建 monitor；同一 run ID 每秒采集
    process/system CPU、active/granted workers、RSS/private/commit、
    read/write bytes、queue/lock/io wait、provider/module；
  - 原始 CSV 不可手工合成：header 指纹链（seed 行 seq=0 绑定合同 HEADER +
    run_id；数据行指纹绑定前一指纹、行字符串、行号）+ 写后只读 seal() +
    时间戳单调断言 + seq 严格递增校验（verify_csv 机器检查）；
  - I/O 区间与初始化区间分开记录：run_phase ∈ init/active/io/flush，每段独立
    标签；区间切换（set_phase）强制边界采样，短于 1s 的 init/io 段也有样本行；
  - 采样与 RT-006 trace 观测接线：active/granted workers、provider/module 由
    trace_feed 注入真实观测（禁止 config 冒充）。

CSV 布局（每行一个样本；顺序 = HEADER；header 后第一行是 seq=0 seed 指纹行，
之后每行 seq=1..N 一个样本）：
  t_iso_utc, seq, run_id, run_phase, interval_s,
  cpu_pct, sys_cpu_pct,
  active_workers, granted_workers, provider, module,
  rss_bytes, private_bytes, commit_bytes,
  read_bytes, write_bytes,
  queue_wait, lock_wait_ns_est, io_wait_rate_est, faults_rate,
  row_fingerprint
字段键与单位见 CONTRACT_COLUMNS 注释。

写后只读：`seal()` 关闭句柄并把文件权限改为只读（monitor 完成时调用）。

本文件不依赖任何科学常数；不改科学/算法源码。
"""
from __future__ import annotations

import csv
import datetime
import hashlib
import json
import os
import pathlib
import threading
import time
from typing import Any, Callable, Dict, List, Optional

from . import linux_procfs, windows_pdh_etw

# ── CSV 合同 ──────────────────────────────────────────────────────────────────
SCHEMA_ID = "astrocs.monitor.sample.v1"

# 列定义（顺序即 CSV 头；键 = 列名，值 = 单位说明）
CONTRACT_COLUMNS: Dict[str, str] = {
    "t_iso_utc": "RFC3339 UTC 秒精度（墙钟，展示/跨进程比对）",
    "seq": "run 内样本序号（1 起单调递增）",
    "run_id": "同一 run 标识（与 trace run_id 同源）",
    "run_phase": "init|active|io|flush（I/O 区间与初始化区间分开）",
    "interval_s": "距上一采样单调秒（≈1s）",
    "cpu_pct": "进程 CPU% = 本进程 CPU 秒差值 / 墙钟差值（100% = 1 核满载）",
    "sys_cpu_pct": "系统级 CPU% = 系统非空闲 jiffies / 总 jiffies（整机）",
    "active_workers": "实际 active workers（RT-006 trace 真实观测注入）",
    "granted_workers": "授予租约上限（trace node_end/worker_task granted 观测）",
    "provider": "provider id（trace 真实 provider 观测；禁止 config 冒充）",
    "module": "当前 module id（trace module_call/node 真实归属）",
    "rss_bytes": "RSS（/proc/self/status VmRSS）",
    "private_bytes": "private 匿名内存（RssAnon，kB→B）",
    "commit_bytes": "VmSize 近似 commit",
    "read_bytes": "累计读字节（/proc/self/io read_bytes）",
    "write_bytes": "累计写字节（/proc/self/io write_bytes）",
    "queue_wait": "队列/就绪等待代理：系统运行队列（loadavg 1m）",
    "lock_wait_ns_est": "锁等待估计 ns：nvcsw 差值 ×1000（代理，_est 标注）",
    "io_wait_rate_est": "io 等待率估计：缺页差值/墙钟（/s；代理）",
    "faults_rate": "缺页速率（minflt+majflt 差值/墙钟，/s）",
}

# row_fingerprint 为最后一列（指纹列，不属数值/字符串合同语义列）
HEADER: List[str] = list(CONTRACT_COLUMNS.keys()) + ["row_fingerprint"]

# 数值列（校验/统计用；其余为字符串列）
_NUMERIC_COLUMNS = {
    "seq", "interval_s", "cpu_pct", "sys_cpu_pct", "active_workers",
    "granted_workers", "rss_bytes", "private_bytes", "commit_bytes",
    "read_bytes", "write_bytes", "queue_wait", "lock_wait_ns_est",
    "io_wait_rate_est", "faults_rate",
}
_STR_COLUMNS = {"t_iso_utc", "run_id", "run_phase", "provider", "module"}
assert HEADER[-1] == "row_fingerprint"
assert set(HEADER) == _NUMERIC_COLUMNS | _STR_COLUMNS | {"row_fingerprint"}
assert len(HEADER) == len(set(HEADER)), "HEADER 不得重复列"

# 合法区间（I/O 区间与初始化区间分开 = 独立阶段，各自样本带自身阶段标签）
PHASES = ("init", "active", "io", "flush")

# 指纹轮换参数（fixed salt 保证跨进程稳定可复验）
_FP_SALT = b"astrocs-log002-v1"


def _fingerprint(prev_fp: str, row_str: Dict[str, str], seq: int) -> str:
    """行指纹：f(F_prev, F_row, n) = sha256(salt | prev | json(row_str) | n)。

    row_str 必须是**即将写入 CSV 的字符串形态**（与文件内容逐字节一致），
    否则浮点格式差异会导致可复验性失败。fingerprint 列自身不入指纹；
    行号 n 与前一指纹都参与——手工追加/改行必然破坏整条后续链。
    """
    canonical = json.dumps(
        {k: row_str.get(k, "") for k in HEADER if k != "row_fingerprint"},
        ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    h = hashlib.sha256()
    h.update(_FP_SALT)
    h.update(prev_fp.encode("ascii"))
    h.update(canonical.encode("utf-8"))
    h.update(str(seq).encode("ascii"))
    return h.hexdigest()


def _seed_fingerprint(run_id: str) -> str:
    """seq=0 seed 行指纹（绑定 header 合同与 run_id；首链种子）。"""
    header_canon = json.dumps(HEADER, ensure_ascii=False,
                              sort_keys=True, separators=(",", ":"))
    h = hashlib.sha256()
    h.update(_FP_SALT)
    h.update(b"seed")
    h.update(header_canon.encode("utf-8"))
    h.update(run_id.encode("utf-8"))
    return h.hexdigest()


def _utc_now() -> str:
    """RFC3339 UTC 秒精度（固定 Z）。"""
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


def _num(v: Any) -> str:
    """数值列格式：None/缺/NaN → ''；int 原样；float 定宽避免抖动。"""
    if v is None:
        return ""
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, str):
        return v
    try:
        f = float(v)
    except (TypeError, ValueError):
        return ""
    if f != f or f in (float("inf"), float("-inf")):
        return ""
    return f"{f:.6f}".rstrip("0").rstrip(".")


# ── 平台后端选择 ──────────────────────────────────────────────────────────────
class _Backend:
    """平台后端选择（Linux procfs 真实 / Windows stub 显式不可用）。"""

    def __init__(self) -> None:
        if linux_procfs.is_available():
            self.kind = "linux_procfs"
        elif windows_pdh_etw.is_available():
            # Windows stub 恒 False；若未来实现为 True 则用真实 Windows 路径
            self.kind = "windows_pdh_etw"
        else:
            self.kind = "unavailable"

    def sample_raw(self) -> Dict[str, Any]:
        if self.kind == "linux_procfs":
            return linux_procfs.collect()
        if self.kind == "windows_pdh_etw":
            return windows_pdh_etw.collect()  # 显式 NotImplementedError
        raise RuntimeError(
            "no resource-monitor backend available (Linux procfs required; "
            "Windows PDH/ETW 显式未实现)")

    def describe(self) -> str:
        return self.kind


def _resolve_backend() -> _Backend:
    b = _Backend()
    if b.kind == "unavailable":
        raise RuntimeError(
            "no resource-monitor backend available: Linux procfs is required "
            "on the Linux control node; Windows PDH/ETW is an explicit "
            "not-implemented stub")
    return b


class ResourceMonitor:
    """同一 run 的资源监控伴随器。

    用法（自动建档在 runner.py 内完成；本类直接驱动采集循环）：
      m = ResourceMonitor(run_id, csv_path, interval_s=1.0)
      m.begin()                       # 建档: header + seed 指纹行
      m.start()                       # 立即同步 baseline + 启动后台采样线程
      m.set_trace_observer(...)       # 可选: 真实观测注入
      m.io_phase() / m.active_phase() # 分段（切换强制边界采样）
      ...工作...
      m.flush_phase()
      m.stop()                        # 停采样线程（含末段边界样本）
      m.seal()                        # 写后只读（关闭并移除写权限）

    线程模型：采样线程与调用线程全部经 `_lock` 串行化；set_phase 在锁内
    先为**离开段**补一个边界样本再切换标签——即使 init/io 段短于 1s 也会
    各自留下样本行（I/O 区间与初始化区间分开可验证）。
    """

    def __init__(self, run_id: str, csv_path: pathlib.Path,
                 interval_s: float = 1.0, phase: str = "init",
                 provider: str = "", module: str = "",
                 active_workers: int = 0, granted_workers: int = 0,
                 observer=None, collect_raw: Optional[Callable[[], Dict[str, Any]]] = None
                 ) -> None:
        if not isinstance(run_id, str) or not run_id:
            raise ValueError("run_id 必须为非空安全字符串")
        self.run_id = run_id
        self.csv_path = pathlib.Path(csv_path)
        self.interval_s = float(interval_s)
        if self.interval_s <= 0:
            raise ValueError("interval_s 必须 > 0")
        if phase not in PHASES:
            raise ValueError(f"初始 phase 非法: {phase!r}")
        self.phase = phase
        # trace 真实观测（active/granted/provider/module）；None → 留空，不注入假值
        self._observer = observer
        self._provider = provider
        self._module = module
        self._active_workers = int(active_workers)
        self._granted_workers = int(granted_workers)
        # 采集后端（测试可注入伪 raw collector）
        self._collect_raw: Callable[[], Dict[str, Any]] = (
            collect_raw if collect_raw else _resolve_backend().sample_raw)
        self._backend_kind = getattr(self._collect_raw, "__name__", "custom")
        self._started = False
        self._stopped = False
        self._thread: Optional[threading.Thread] = None
        self._lock = threading.Lock()
        self._seq = 0
        self._baseline_taken = False   # 首样本仅作 baseline（无差值可比）
        self._last_mono: Optional[float] = None
        self._prev_raw: Optional[Dict[str, Any]] = None
        self._prev_fp = ""
        self._rows: List[Dict[str, Any]] = []
        self._csv_writer = None
        self._fh = None
        self._sealed = False
        self._errors: List[str] = []

    # ── 观测注入 ───────────────────────────────────────────────────────────
    def set_trace_observer(self, observer) -> None:
        """注入 RT-006 trace 观测回调（取真实 provider/module/workers）。

        observer 每次采样被调用，返回 dict（键见 _observe()）。禁止 config
        值冒充：本方法只接受带 run 身份与真实来源的 observer（如
        TraceSnapshotObserver.observe）。
        """
        with self._lock:
            self._observer = observer

    def set_static_observation(self, *, provider: str = "", module: str = "",
                               active_workers: Optional[int] = None,
                               granted_workers: Optional[int] = None) -> None:
        """静态真实观测（由 runner/调用方在真实运行点置位）。

        注意：这不是 config 冒充——调用方（executor/runner）只在真实
        运行现场调用，等价于观测回调的常量形式。若无真实来源，应留空。
        """
        with self._lock:
            if provider:
                self._provider = provider
            if module:
                self._module = module
            if active_workers is not None:
                self._active_workers = int(active_workers)
            if granted_workers is not None:
                self._granted_workers = int(granted_workers)

    # ── 阶段（I/O 区间与初始化区间分开） ───────────────────────────────────
    def set_phase(self, phase: str) -> None:
        """切换 run_phase；先为离开段补边界样本，再改标签。

        锁内执行（与采样线程互斥）；离开段即使短于采样间隔也产生样本行，
        保证 init/io/active/flush 各段独立可分离。
        """
        if phase not in PHASES:
            raise ValueError(f"phase 非法: {phase!r}")
        with self._lock:
            if phase == self.phase:
                return
            try:
                self._sample_locked(time.monotonic(), tag_phase=self.phase)
            except Exception as exc:  # 边界采样失败不阻塞切换；显式记录
                self._errors.append(f"set_phase 边界采样失败: {type(exc).__name__}: {exc}")
            self.phase = phase

    def active_phase(self) -> None:
        self.set_phase("active")

    def io_phase(self) -> None:
        self.set_phase("io")

    def init_phase(self) -> None:
        self.set_phase("init")

    def flush_phase(self) -> None:
        self.set_phase("flush")

    @property
    def current_phase(self) -> str:
        with self._lock:
            return self.phase

    # ── 建档/生命周期 ──────────────────────────────────────────────────────
    def begin(self) -> None:
        """建档：确保目录、写 header + seq=0 指纹 seed 行。

        seed 行指纹 = sha256(salt|"seed"|sorted(HEADER)|run_id)：既是首行
        指纹链种子，也绑定 run_id（防把别的 run 的链复制过来）。建档完成后
        CSV 仅由本进程追加；外部篡改任意字节都会被 verify_csv 抓出。
        每个 run 是全新链：**拒绝在已有非空 CSV 上续写**（防伪造延续）。
        """
        self.csv_path.parent.mkdir(parents=True, exist_ok=True)
        if self.csv_path.exists() and self.csv_path.stat().st_size > 0:
            raise RuntimeError(
                f"CSV 已存在非空文件，拒绝续写（每个 run 独立链）: {self.csv_path}")
        with self._lock:
            if self._fh is not None:
                raise RuntimeError("monitor 已 begin")
            self._fh = open(self.csv_path, "w", encoding="utf-8", newline="")
            self._csv_writer = csv.writer(self._fh)
            self._csv_writer.writerow(HEADER)
            seed_fp = _seed_fingerprint(self.run_id)
            seed_row = {k: "" for k in HEADER}
            seed_row["seq"] = "0"
            seed_row["run_id"] = self.run_id
            seed_row["row_fingerprint"] = seed_fp
            self._csv_writer.writerow([seed_row.get(k, "") for k in HEADER])
            self._fh.flush()
            os.fsync(self._fh.fileno())
            self._prev_fp = seed_fp
            self._seq = 0

    # ── 采样 ───────────────────────────────────────────────────────────────
    def _observe(self) -> Dict[str, Any]:
        """取真实观测：trace observer 优先，否则静态真实置位值。

        返回 {provider, module, active_workers, granted_workers}。
        调用方必须在 _lock 内（_sample_locked 保证）。
        """
        if self._observer is not None:
            obs = self._observer()
            if not isinstance(obs, dict):
                raise TypeError("trace observer 必须返回 dict")
            return {
                "provider": str(obs.get("provider") or ""),
                "module": str(obs.get("module") or ""),
                "active_workers": int(obs.get("active_workers") or 0),
                "granted_workers": int(obs.get("granted_workers") or 0),
            }
        return {
            "provider": self._provider,
            "module": self._module,
            "active_workers": self._active_workers,
            "granted_workers": self._granted_workers,
        }

    def _row_from_delta(self, raw: Dict[str, Any], prev: Dict[str, Any],
                        mono_now: float, phase: str,
                        interval: Optional[float]) -> Dict[str, Any]:
        """用当前/上一原始样本差值与真实观测构造一行（锁内调用）。"""
        obs = self._observe()
        row: Dict[str, Any] = {
            "t_iso_utc": _utc_now(),
            "run_id": self.run_id,
            "run_phase": phase,
            "interval_s": interval if interval is not None else "",
            "cpu_pct": "",
            "sys_cpu_pct": "",
            "active_workers": obs["active_workers"],
            "granted_workers": obs["granted_workers"],
            "provider": obs["provider"],
            "module": obs["module"],
            "rss_bytes": raw.get("rss_bytes"),
            "private_bytes": raw.get("private_bytes"),
            "commit_bytes": raw.get("commit_bytes"),
            "read_bytes": raw.get("read_bytes"),
            "write_bytes": raw.get("write_bytes"),
            "queue_wait": raw.get("sys_loadavg_1m"),
            "lock_wait_ns_est": "",
            "io_wait_rate_est": "",
            "faults_rate": "",
        }
        dt = interval if interval and interval > 0 else None

        def _d(a: Optional[float], b: Optional[float]) -> Optional[float]:
            if a is None or b is None:
                return None
            return float(a) - float(b)

        # 进程 CPU%：CPU 秒差值 / 墙钟差值 ×100（100% = 1 核满载）
        u0, u1 = prev.get("cpu_user_seconds"), raw.get("cpu_user_seconds")
        s0, s1 = prev.get("cpu_sys_seconds"), raw.get("cpu_sys_seconds")
        du = _d(u1, u0)
        ds = _d(s1, s0)
        if du is not None and ds is not None and dt:
            row["cpu_pct"] = max(0.0, (du + ds) / dt * 100.0)
        # 系统级 CPU%：非空闲 jiffies / 总 jiffies
        t0 = prev.get("sys_cpu_jiffies_total")
        t1 = raw.get("sys_cpu_jiffies_total")
        i0 = prev.get("sys_cpu_jiffies_idle")
        i1 = raw.get("sys_cpu_jiffies_idle")
        if None not in (t0, t1, i0, i1):
            d_total = t1 - t0
            d_idle = i1 - i0
            if d_total and d_total > 0:
                row["sys_cpu_pct"] = max(0.0, (d_total - d_idle) / d_total * 100.0)
        # lock_wait 代理：非自愿 ctxt 切换差值 ×1000 ns（_est 标注）
        c0 = prev.get("ctxt_switches_nvcsw")
        c1 = raw.get("ctxt_switches_nvcsw")
        if c0 is not None and c1 is not None:
            dc = c1 - c0
            row["lock_wait_ns_est"] = max(0, dc) * 1000.0
        # io wait 代理：缺页差值/墙钟（faults_rate 与 io_wait_rate_est 同源）
        m0 = (prev.get("faults_minor") or 0) + (prev.get("faults_major") or 0)
        m1 = (raw.get("faults_minor") or 0) + (raw.get("faults_major") or 0)
        fm = max(0, m1 - m0)
        if dt:
            row["faults_rate"] = fm / dt
            row["io_wait_rate_est"] = fm / dt
        # queue_wait：系统 loadavg 1m 运行队列（当前/上一值兜底）
        q = raw.get("sys_loadavg_1m")
        if q is None:
            q = prev.get("sys_loadavg_1m")
        row["queue_wait"] = q
        return row

    def _append(self, row: Dict[str, Any]) -> None:
        """锁内：写一行（先转字符串形态，再算指纹）。"""
        self._seq += 1
        seq = self._seq
        row["seq"] = seq
        line_vals: Dict[str, str] = {}
        for k in HEADER:
            if k == "row_fingerprint":
                continue
            v = row.get(k, "")
            if k in _NUMERIC_COLUMNS:
                line_vals[k] = _num(v)
            else:
                line_vals[k] = "" if v is None else str(v)
        fp = _fingerprint(self._prev_fp, line_vals, seq)
        self._prev_fp = fp
        row["row_fingerprint"] = fp
        line = [line_vals.get(k, "") for k in HEADER[:-1]] + [fp]
        self._rows.append(dict(row))
        if self._csv_writer is not None and self._fh is not None:
            self._csv_writer.writerow(line)
            self._fh.flush()

    def _sample_locked(self, mono_now: float,
                       tag_phase: Optional[str] = None) -> None:
        """锁内单次采样（调用方必须已持 _lock）。

        tag_phase=None 用当前 self.phase；否则用给定 phase（set_phase 离开段）。
        首样本（无 prev_raw）只记 baseline 不落行；其后每样本一行。
        与上一采样间隔 < floor 的样本丢弃（不落行，仅 re-anchor 计时），
        避免边界采样与周期采样在锁竞争下产生近零间隔重复行。
        """
        if self._fh is None or self._sealed:
            return
        raw = self._collect_raw()
        if not isinstance(raw, dict):
            raise TypeError("collect_raw 必须返回 dict")
        interval = None
        if self._last_mono is not None:
            interval = mono_now - self._last_mono
        self._last_mono = mono_now
        prev = self._prev_raw
        self._prev_raw = raw
        if prev is None:
            self._baseline_taken = True
            return
        # 近零间隔（两采样竞争锁）：re-anchor 但不落行
        floor = max(0.01, self.interval_s * 0.2)
        if interval is not None and interval < floor:
            return
        phase = tag_phase if tag_phase is not None else self.phase
        row = self._row_from_delta(raw, prev, mono_now, phase, interval)
        self._append(row)

    def sample_once(self) -> int:
        """主动采一次（同步；测试/短 run 用）。返回当前 seq。"""
        with self._lock:
            self._sample_locked(time.monotonic())
            return self._seq

    def _sample_loop(self) -> None:
        try:
            while True:
                with self._lock:
                    if self._stopped:
                        return
                time.sleep(self.interval_s)
                with self._lock:
                    if self._stopped:
                        return
                    try:
                        self._sample_locked(time.monotonic())
                    except Exception as exc:
                        self._errors.append(
                            f"采样失败: {type(exc).__name__}: {exc}")
        except Exception as exc:  # 循环级异常（不应发生）显式记录
            with self._lock:
                self._errors.append(f"采样循环终止: {type(exc).__name__}: {exc}")

    def start(self) -> None:
        """启动：同步 baseline + 后台采样线程（interval_s 间隔）。"""
        with self._lock:
            if self._started:
                raise RuntimeError("monitor 已启动")
            if self._fh is None:
                raise RuntimeError("必须先 begin()")
            self._started = True
            self._stopped = False
            # 同步 baseline：确保其后首个区间（哪怕 <1s）有差值基准
            try:
                self._sample_locked(time.monotonic())
            except Exception as exc:
                self._errors.append(f"baseline 采样失败: {type(exc).__name__}: {exc}")
        self._thread = threading.Thread(target=self._sample_loop,
                                        name=f"monitor-{self.run_id[:12]}",
                                        daemon=True)
        self._thread.start()

    def stop(self) -> None:
        """停采样线程；停止前补当前段末边界样本（标签=当前 phase）。"""
        with self._lock:
            if not self._started:
                return
            self._stopped = True
            try:
                self._sample_locked(time.monotonic())
            except Exception as exc:
                self._errors.append(f"stop 末样本失败: {type(exc).__name__}: {exc}")
        th = self._thread
        self._thread = None
        if th is not None:
            th.join(timeout=max(2.0, self.interval_s * 3 + 1.0))

    # ── seal（写后只读） ────────────────────────────────────────────────────
    def seal(self) -> None:
        """关闭 CSV 句柄并移除写权限（写后只读：原始 CSV 不可再被篡改写入）。"""
        with self._lock:
            if self._sealed:
                return
            self._sealed = True
            self._stopped = True
            fh = self._fh
            self._fh = None
            self._csv_writer = None
        if fh is not None:
            try:
                fh.flush()
                os.fsync(fh.fileno())
            finally:
                fh.close()
        # 全只读权限（owner/组/其他均不可写——防同一账号误写）
        try:
            st = self.csv_path.stat()
            os.chmod(self.csv_path, st.st_mode & 0o444)
        except OSError:
            pass

    # ── 结果/统计 ──────────────────────────────────────────────────────────
    @property
    def seq(self) -> int:
        with self._lock:
            return self._seq

    @property
    def error_messages(self) -> List[str]:
        with self._lock:
            return list(self._errors)

    def row_count(self) -> int:
        with self._lock:
            return len(self._rows)

    def rows(self) -> List[Dict[str, Any]]:
        with self._lock:
            return [dict(r) for r in self._rows]

    def intervals(self) -> List[Optional[float]]:
        with self._lock:
            return [r.get("interval_s") for r in self._rows]

    def phases_seen(self) -> List[str]:
        with self._lock:
            out: List[str] = []
            for r in self._rows:
                p = r.get("run_phase")
                if p not in out:
                    out.append(p)
            return out


# ── 读取与校验 ────────────────────────────────────────────────────────────────
def load_rows(csv_path: pathlib.Path) -> List[Dict[str, Any]]:
    """读 CSV 为 dict 行（含 seed 行 seq=0 与样本行；供校验/统计）。"""
    out: List[Dict[str, Any]] = []
    with open(csv_path, encoding="utf-8", newline="") as f:
        rd = csv.DictReader(f)
        if rd.fieldnames != HEADER:
            raise ValueError(
                f"CSV header 与合同不符: {rd.fieldnames!r} != {HEADER!r}")
        for row in rd:
            d: Dict[str, Any] = {}
            for k in HEADER:
                v = row.get(k, "")
                if k in _NUMERIC_COLUMNS and v != "":
                    try:
                        d[k] = float(v) if "." in v or "e" in v.lower() else int(v)
                    except ValueError:
                        d[k] = v
                else:
                    d[k] = v
            out.append(d)
    return out


def verify_csv(csv_path: pathlib.Path, run_id: Optional[str] = None,
               require_timestamp_monotonic: bool = True) -> Dict[str, Any]:
    """原始 CSV 完整性校验（不可手工合成/篡改检测）。

    检查：
      1. header 精确等于合同 HEADER；
      2. seed 行（seq=0）指纹 == _seed_fingerprint(seed.run_id)；
      3. 链式指纹：每样本行 row_fingerprint == _fingerprint(prev_fp, 行, seq)；
      4. seq 严格 1..N 递增无空洞（seed 后）；
      5. run_id 全行一致（若给 run_id）；
      6. t_iso_utc 单调（若 require_timestamp_monotonic；秒精度允许相等）；
      7. run_phase ∈ PHASES。
    返回 machine dict {ok, errors[]}。
    """
    errs: List[str] = []
    with open(csv_path, encoding="utf-8", newline="") as f:
        rd = csv.reader(f)
        try:
            header = next(rd)
        except StopIteration:
            return {"ok": False, "errors": ["空 CSV（无 header）"]}
        if header != HEADER:
            return {"ok": False, "errors": [f"header 不符: {header!r}"]}
        rows: List[List[str]] = list(rd)

    if not rows:
        return {"ok": False, "errors": errs + ["无数据行"]}
    seed = rows[0]
    if len(seed) != len(HEADER):
        return {"ok": False,
                "errors": errs + [f"seed 行列数不符（{len(seed)}）"]}
    seed_dict = dict(zip(HEADER, seed))
    seed_rid = str(seed_dict.get("run_id", ""))
    if seed_dict.get("seq") != "0":
        errs.append(f"seed 行 seq != 0: {seed_dict.get('seq')!r}")
    expect_seed_fp = _seed_fingerprint(seed_rid)
    if seed_dict.get("row_fingerprint") != expect_seed_fp:
        errs.append("seed 行指纹失配（header/run_id 被篡改或整链手工合成）")
    prev_fp = expect_seed_fp
    prev_ts: Optional[str] = None
    expect_seq = 1
    data_rows = rows[1:]
    for line in data_rows:
        if len(line) != len(HEADER):
            errs.append(f"列数不符（{len(line)} != {len(HEADER)}）")
            continue
        row = dict(zip(HEADER, line))
        seq_s = str(row.get("seq", "")).strip()
        try:
            seq = int(seq_s)
        except ValueError:
            errs.append(f"seq 非整数: {seq_s!r}")
            continue
        if seq != expect_seq:
            errs.append(f"seq 不连续: 期望 {expect_seq}, 实际 {seq}")
        expect_seq += 1
        ts = str(row.get("t_iso_utc", ""))
        if require_timestamp_monotonic and ts:
            if prev_ts is not None and ts < prev_ts:
                errs.append(f"时间戳倒退: {prev_ts!r} -> {ts!r}")
            prev_ts = ts
        ph = row.get("run_phase", "")
        if ph not in PHASES:
            errs.append(f"run_phase 非法: {ph!r}")
        rid = row.get("run_id", "")
        if run_id is not None and rid != run_id:
            errs.append(f"run_id 不一致: {rid!r} != {run_id!r}")
        fp = str(row.get("row_fingerprint", ""))
        expect_fp = _fingerprint(prev_fp, row, seq)
        if fp != expect_fp:
            errs.append(f"seq={seq} 行指纹失配（内容被篡改/追加）")
        else:
            prev_fp = fp
    return {"ok": len(errs) == 0, "errors": errs}


__all__ = [
    "CONTRACT_COLUMNS", "HEADER", "PHASES", "SCHEMA_ID", "ResourceMonitor",
    "load_rows", "verify_csv", "_fingerprint", "_seed_fingerprint",
]
