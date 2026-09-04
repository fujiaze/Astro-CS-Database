"""RT-006 trace 真实观测接线（LOG-002；provider/module/workers 真实来源）。

任务要求"与 RT-006 的真实 trace 字段对接（provider/module/active/granted/
workers 从 trace 真实来源取，禁止 config 值冒充观测）"。

TraceSnapshotObserver —— 从 RT-006 TraceStore 快照（真实事件流）推导当前
观测值：
  - provider  : 最近一条携带非空 provider 的 trace 事件（MODULE_CALL/
                PROVIDER_ENTER/WORKER_TASK/NODE_END）里的 provider —— 真实
                观测置位（executor ctx.set_provider / module adapter），
                不是计划值；
  - module    : 最近一条 MODULE_CALL/NODE_START 的 module_id / node_id 归属；
  - active_workers : 当前在途（未 NODE_END）节点数 × 该节点 granted 或
                WORKER_TASK workers 观测的累计（无完成事件则视为活动）；
  - granted_workers: 最近 NODE_START/MODULE_CALL 携带的 granted_workers（授予
                租约上限观测）；取最近一条事件的最大值作为当前上限。
简化且明确的语义（JSONL 重放同样适用）：
  1. 扫描全部事件（时间序）；维护 run 的活动节点集合：
     NODE_START 入集，NODE_END 出集（同 node_id）——集合大小 = active 节点数
     （scheduler 层一个 worker 一个节点，active_workers≈活动节点）；
  2. active_workers = 活动节点数（真实 trace 观测）；
  3. granted_workers = max(granted_workers over 活动节点的 NODE_START/MODULE_CALL)，
     无则最近 MODULE_CALL/WORKER_TASK 的 workers 观测；
  4. provider/module 取最近一个携带真实观测的事件（无 → ""）。

禁止 config 冒充的硬约束：本模块**不读计划/配置 JSON**，只读 trace 事件
（JSONL 行或 TraceStore 快照 dict 列表）。事件字段名与
`include/astrocs/core/contracts.h` TraceEvent JSONL 完全一致（type/run_id/
node_id/module_id/provider/workers/granted_workers/status 等）。

实现还提供 emit_metric_events(): 把采样摘要作为 LOG-001 结构化事件写出
（phase=monitoring，event=metric），复用 run/task/event/JSONL 语义。
"""
from __future__ import annotations

import datetime
import json
from typing import Any, Dict, Iterable, List, Optional

# RT-006 TraceEvent 事件类型名（contracts.h trace_event_type_name 同源）
_EVENT_TYPES = {
    "module_call", "provider_enter", "provider_leave", "worker_task",
    "node_start", "node_end", "artifact_publish", "checkpoint", "error",
}

_NODE_START_END = {"node_start", "node_end"}
_WITH_PROVIDER = {"module_call", "provider_enter", "provider_leave",
                  "worker_task", "node_start", "node_end"}
_WITH_MODULE = {"module_call", "node_start", "worker_task"}
_GRANTED_CARRIERS = {"node_start", "module_call", "worker_task", "node_end"}


def _safe_str(v: Any) -> str:
    return "" if v is None else str(v)


class TraceSnapshotObserver:
    """从 RT-006 trace 事件序列推导当前真实观测（provider/module/workers）。

    线程安全：构造后只读（快照事件列表已定）；monitor 采样线程调用 observe()。
    同时提供 __call__ = observe()，可直接作为 ResourceMonitor 的 observer。
    """

    def __init__(self, run_id: str, events: Iterable[Dict[str, Any]]) -> None:
        self.run_id = run_id
        self._events = list(events)
        self._active: Dict[str, Dict[str, Any]] = {}  # node_id -> last carrier

    def __call__(self) -> Dict[str, Any]:
        return self.observe()

    def observe(self) -> Dict[str, Any]:
        """对全事件重放，返回 {provider, module, active_workers,
        granted_workers}（真实 trace 观测，无 config 冒充）。"""
        provider = ""
        module = ""
        active: Dict[str, Dict[str, Any]] = {}
        granted = 0
        for ev in self._events:
            ty = _safe_str(ev.get("type"))
            node = _safe_str(ev.get("node_id"))
            if ty not in _EVENT_TYPES:
                continue
            if ty in _NODE_START_END and node:
                if ty == "node_start":
                    active[node] = ev
                elif ty == "node_end":
                    active.pop(node, None)
            # provider：只取真实置位（携带 provider 的事件类型）
            if ty in _WITH_PROVIDER:
                p = _safe_str(ev.get("provider"))
                if p:
                    provider = p
            if ty in _WITH_MODULE:
                m = _safe_str(ev.get("module_id")) or _safe_str(ev.get("node_id"))
                if m:
                    module = m
            if ty in _GRANTED_CARRIERS:
                gw = ev.get("granted_workers")
                if isinstance(gw, int) and gw > 0:
                    granted = max(granted, int(gw))
        active_workers = len(active)
        if granted == 0:
            # 无 granted 观测：用最近 worker_task/module_call 的 workers 观测
            for ev in reversed(self._events):
                w = ev.get("workers")
                if isinstance(w, int) and w > 0:
                    granted = int(w)
                    break
        return {
            "provider": provider,
            "module": module,
            "active_workers": active_workers,
            "granted_workers": granted,
        }


def observer_from_jsonl(run_id: str, jsonl: str) -> TraceSnapshotObserver:
    """从 RT-006 trace JSONL 文本构造 observer（逐行严格 JSON）。

    非法行跳过（与 trace_replay 语义一致，不中断）。
    """
    events: List[Dict[str, Any]] = []
    for line in jsonl.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
        except Exception:  # 非法 JSON 行跳过
            continue
        if isinstance(obj, dict):
            events.append(obj)
    return TraceSnapshotObserver(run_id, events)


def emit_metric_event(*, seq: int, run: str, ts: str, phase: str = "monitoring",
                      diagnostic: str, units: str = "", elapsed: float = 0.0,
                      value: Optional[float] = None,
                      task: str = "", node: str = "", module: str = "",
                      commit: str = "", host: str = "") -> Dict[str, Any]:
    """构造一条 LOG-001 结构化 metric 事件 dict（phase=monitoring）。

    复用 runtime/logging/log_event.py 的 LogEvent 语义；返回已脱敏 dict，
    供调用方 to_jsonl() 或直接 JSON 落盘。字段集合满足
    astrocs.log.event.v1 required（缺省 task/node/module 为空串）。
    """
    from runtime.logging.log_event import LogEvent  # 延迟导入避免环
    ev = LogEvent(seq=seq, ts=ts, run=run, level="info", event="metric",
                  diagnostic=diagnostic, task=task, node=node, module=module,
                  phase=phase, commit=commit, host=host, units=units,
                  elapsed=elapsed, value=value)
    return ev.to_dict()


def utc_now_s() -> str:
    """RFC3339 UTC 秒精度（与 monitor._utc_now 同格式）。"""
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


__all__ = ["TraceSnapshotObserver", "observer_from_jsonl", "emit_metric_event",
           "utc_now_s"]
