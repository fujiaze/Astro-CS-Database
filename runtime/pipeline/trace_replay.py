#!/usr/bin/env python3
"""RT-006 JSONL trace 可重放（Python 权威执行形态）。

角色（tasks/03_RUNTIME_DATA_IO_TASKS.md RT-006 + 14 §5 运行图 + LOG-003 消费端）:
  - trace JSONL 由 Runtime（lib/core/src/trace.cpp TraceStore::export_jsonl）
    每事件一行输出；本模块把 JSONL 重放为按 node 聚合的可渲染摘要
    （replay_schema=astrocs.trace-replay/v1），供 LOG-003 图渲染工具直接消费；
  - 与 C++ trace_replay_from_jsonl 双实现同构（字段语义一致）；测试互相对照；
  - 隐藏 session / 重复调用检测：同一 entry 出现在 ≥2 个不同 node 的 module_call
    = 隐藏 session 扇出；同一 node module_call 计数 >1 = 重复调用 —— 均违反
    "7 个 P2 节点各一次" 契约（约束 F.1 禁止多节点重复调用完整 Session）。
纯结构/契约语义，无科学常数。
"""
from __future__ import annotations

import json
import pathlib
import sys
from typing import Any, Dict, List

REPLAY_SCHEMA_CONST = "astrocs.trace-replay/v1"

# 合法事件类型（与 contracts.h trace_event_type_name 一一对应）
_EVENT_TYPES = {
    "module_call", "provider_enter", "provider_leave", "worker_task",
    "node_start", "node_end", "artifact_publish", "checkpoint", "error",
}


def load_strict_json(text: str) -> Any:
    """严格 JSON 解析：拒绝 NaN/Infinity 与对象重复 key。"""
    def _no_nan(*_args: Any) -> None:
        raise ValueError("non-standard JSON constant (NaN/Infinity) is rejected")
    def _obj_hook(pairs: list) -> dict:
        seen = set()
        for k, _ in pairs:
            if k in seen:
                raise ValueError(f"duplicate key: {k}")
            seen.add(k)
        return dict(pairs)
    return json.loads(text, parse_constant=_no_nan, object_pairs_hook=_obj_hook)


def replay_from_jsonl(jsonl: str) -> Dict[str, Any]:
    """JSONL → 可渲染摘要（合法空输入 → ok, nodes=[]）。

    非法行跳过并计入 skipped_lines；永不抛异常（除 JSON 超深等系统错误外）。
    """
    out: Dict[str, Any] = {
        "replay_schema": REPLAY_SCHEMA_CONST,
        "parsed_lines": 0,
        "skipped_lines": 0,
        "nodes": [],
    }
    nodes: Dict[str, Dict[str, Any]] = {}
    artifacts: Dict[str, List[str]] = {}
    if not jsonl:
        return out
    for line in jsonl.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            ev = load_strict_json(line)
        except Exception:
            out["skipped_lines"] += 1
            continue
        if not isinstance(ev, dict):
            out["skipped_lines"] += 1
            continue
        etype = ev.get("type")
        if etype not in _EVENT_TYPES:
            out["skipped_lines"] += 1
            continue
        out["parsed_lines"] += 1
        nid = ev.get("node_id") or ""
        node = nodes.setdefault(
            nid, {"node_id": nid, "status": "", "call_count": 0,
                  "module_id": "", "entry": "", "provider": "",
                  "artifact_ids": [], "wall_ms": 0.0})
        if etype == "module_call":
            node["call_count"] += 1
            if not node["module_id"]:
                node["module_id"] = ev.get("module_id") or ""
            if not node["entry"]:
                node["entry"] = ev.get("entry") or ""
        elif etype == "node_end":
            node["status"] = ev.get("status") or node["status"]
            wm = ev.get("wall_ms")
            if isinstance(wm, (int, float)) and wm > 0:
                node["wall_ms"] = float(wm)
        elif etype == "node_start":
            if not node["status"]:
                node["status"] = "RUNNING"
        elif etype == "error":
            if not node["status"]:
                node["status"] = "ERROR"
        if ev.get("provider"):
            node["provider"] = ev["provider"]
        if ev.get("artifact_id"):
            arts = artifacts.setdefault(nid, [])
            if ev["artifact_id"] not in arts:
                arts.append(ev["artifact_id"])
    for nid, node in nodes.items():
        node["artifact_ids"] = artifacts.get(nid, [])
    out["nodes"] = sorted(nodes.values(), key=lambda n: n["node_id"])
    return out


def detect_violations(jsonl: str) -> List[str]:
    """隐藏 session / 重复调用检测（RT-006 验收）。

    - hidden-session-fanout: 同一 entry 在 ≥2 个不同 node 出现 module_call；
    - repeated-call: 同一 node module_call 计数 > 1。
    精确判定基于原始 JSONL 行（每行一个 module_call 事件）。
    """
    violations: List[str] = []
    entry_nodes: Dict[str, set] = {}   # entry -> nodes
    call_counts: Dict[str, int] = {}
    if not jsonl:
        return violations
    for line in jsonl.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            ev = load_strict_json(line)
        except Exception:
            continue
        if not isinstance(ev, dict) or ev.get("type") != "module_call":
            continue
        entry = ev.get("entry") or ""
        nid = ev.get("node_id") or ""
        if entry:
            entry_nodes.setdefault(entry, set()).add(nid)
        call_counts[nid] = call_counts.get(nid, 0) + 1
    for entry, nodeset in sorted(entry_nodes.items()):
        if len(nodeset) >= 2:
            violations.append(
                f"hidden-session-fanout: entry '{entry}' observed at "
                f"{len(nodeset)} nodes [{','.join(sorted(nodeset))}]")
    for nid, cnt in sorted(call_counts.items()):
        if cnt > 1:
            violations.append(
                f"repeated-call: node '{nid}' module_call count={cnt}")
    return violations


if __name__ == "__main__":
    data = sys.stdin.read()
    rep = replay_from_jsonl(data)
    json.dump(rep, sys.stdout, ensure_ascii=False, indent=2)
    sys.stdout.write("\n")
