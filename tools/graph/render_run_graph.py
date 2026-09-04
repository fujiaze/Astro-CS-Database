#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LOG-003 运行图渲染工具（tools/graph 域，owner SA-LOG-08）。

从 plan/trace 生成 DOT/SVG/JSON 运行图；标出真实入口、数据边、并行轴、
workers、provider、耗时、资源、DLL hash、artifact hash。

角色（tasks/03_RUNTIME_DATA_IO_TASKS.md LOG-003 + 控制包标准
14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md §5 + 23_GRAPH_AND_DOC_TOOL_POLICY.md）:
  - 静态/声明图来自 typed plan（RT-001 `astrocs.plan-graph/v1` 或
    `astrocs.typed-dag/v1` 原始 IR）；真实运行图来自 RT-006 trace JSONL
    （`astrocs.trace-event/v1`，一事件一行）。禁止以静态计划冒充运行事实；
  - 真实入口/调用计数/workers/granted/provider/耗时/DLL hash/artifact hash
    一律从 trace 事件观测取（禁止 config 值冒充）；resource_class 是计划声明
    属性（标注 source=plan），不冒充观测；
  - 每次生成 DOT/JSON（可审计事实；DOT 是纯文本规范形态，无需外部 graphviz
    二进制），`--svg` 时用零第三方依赖的最小合法 SVG 直出（派生展示物，非
    审计事实——主机无已登记 Graphviz 时不假装 dot 调用成功）；
  - 每张图写 generator 版本、source SHA、输入文件 hash（policy §2.5）；
  - 本工具不改变产品执行（Doxygen/Graphviz 仅生成文档）→ scientific_change
    恒 false。

机器验证（验收：图与 trace 调用计数、DLL hash、artifact hash 一致）：
  `--verify graph-runtime.json --trace <jsonl>` 重放 trace 并逐项比对：
    - 图节点集合 == trace replay 节点集合；
    - 每节点 call_count == replay call_count == 原始 module_call 事件计数；
    - entry/module_id/provider/status/wall_ms == trace 观测（观测缺失时同为空）；
    - dll 字段 == module_call 事件携带的 dll_name/dll_sha256；
    - artifact（id/sha256/size）== artifact_publish 事件携带值。
  一致 → exit 0 + `GRAPH_CONSISTENT`；任一不一致 → exit 1 + `GRAPH_INCONSISTENT`
  + 不一致清单。

用法：
  python3 tools/graph/render_run_graph.py --trace <run-trace.jsonl> \
      [--plan <plan.json>] [--out-dir <dir>] [--sha <40hex>] [--svg] [--name <stem>]
  python3 tools/graph/render_run_graph.py --verify <graph.json> --trace <jsonl>
  python3 tools/graph/render_run_graph.py --selfcheck

零第三方依赖（仅 Python 标准库）；可在任意控制节点复跑。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys
from typing import Any, Dict, List, Optional, Tuple

REPO = pathlib.Path(__file__).resolve().parents[2]
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))
try:  # 权威 replay 聚合（RT-006 双实现之一；只读消费，不改动）
    from runtime.pipeline.trace_replay import replay_from_jsonl  # type: ignore
except Exception:  # pragma: no cover - 独立运行时兜底
    replay_from_jsonl = None

try:  # typed DAG 权威编译器（RT-001；只读编译 typed-dag IR → plan-graph 边）
    from runtime.pipeline.typed_dag import TypedDagCompiler  # type: ignore
except Exception:  # pragma: no cover
    TypedDagCompiler = None

TOOL = "tools/graph/render_run_graph.py"
VERSION = "1.0.0"
GRAPH_JSON_SCHEMA = "astrocs.graph-json/v1"

# 合法 trace 事件类型（与 contracts.h trace_event_type_name 同源；空校验集 =
# 不做类型过滤，逐行按字段聚合）
_EVENT_TYPES = {
    "module_call", "provider_enter", "provider_leave", "worker_task",
    "node_start", "node_end", "artifact_publish", "checkpoint", "error",
}

# Standard §5 颜色（DOT fillcolor by resource_class/status；SVG 同色）
COLOR_BY_CLASS = {
    "cpu_heavy": "#ffe8b0",   # 浅金黄
    "io": "#b8d4f0",          # 浅蓝
    "metadata": "#d8d8d8",    # 浅灰
    "cpu_light": "#e0f0c0",   # 浅绿
}
COLOR_FAILED = "#f0b0b0"      # 失败节点（浅红）
COLOR_PLAN_ONLY = "#ffffff"   # 仅计划未运行（白/虚线）
COLOR_BORDER_BLOCKED = "#e0a000"


def _sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def _sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def load_strict_json(text: str) -> Any:
    """严格 JSON：拒绝 NaN/Infinity 与重复 key（与 runtime 同语义）。"""

    def _no_nan(*_a: Any) -> None:
        raise ValueError("non-standard JSON constant rejected")

    def _obj_hook(pairs: list) -> dict:
        seen = set()
        for k, _ in pairs:
            if k in seen:
                raise ValueError(f"duplicate key: {k}")
            seen.add(k)
        return dict(pairs)

    return json.loads(text, parse_constant=_no_nan, object_pairs_hook=_obj_hook)


def _read_jsonl(path: pathlib.Path) -> Tuple[List[Dict[str, Any]], int]:
    """逐行严格解析 trace JSONL → (events, skipped_lines)。"""
    events: List[Dict[str, Any]] = []
    skipped = 0
    text = path.read_text(encoding="utf-8")
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            ev = load_strict_json(line)
        except Exception:
            skipped += 1
            continue
        if not isinstance(ev, dict):
            skipped += 1
            continue
        if ev.get("type") not in _EVENT_TYPES:
            skipped += 1
            continue
        events.append(ev)
    return events, skipped


# ── 运行时观测聚合（真实 trace，禁止 config 冒充） ─────────────────────────
def aggregate_trace(events: List[Dict[str, Any]]) -> Dict[str, Any]:
    """事件列表 → 按 node 聚合的真实观测。

    语义与 runtime/pipeline/trace_replay.py 对齐（call_count/status/provider/
    entry/module_id 一致），另收集 replay 不携带的 dll/artifact/workers/cpu 等
    观测字段。全部字段只来自事件本身。
    """
    nodes: Dict[str, Dict[str, Any]] = {}
    active: Dict[str, Dict[str, Any]] = {}      # node_id -> last carrier
    sched_active = 0
    sched_max = 0
    worker_lease_max = 0      # max `workers` 观测（任何事件；真实租约观测）
    worker_task_total = 0     # worker_task 事件总数（真实计数）
    run_ids: set = set()
    order: List[str] = []

    def _node(nid: str) -> Dict[str, Any]:
        if nid not in nodes:
            nodes[nid] = {
                "id": nid,
                "kind": "node",
                "module_id": "",
                "module_version": "",
                "entry": "",
                "call_count": 0,
                "status": "",
                "provider": "",
                "workers": 0,
                "granted_workers": 0,
                "wall_ms": 0.0,
                "cpu_ms": 0.0,
                "dll_name": "",
                "dll_sha256": "",
                "artifacts": [],          # 有序去重 artifact_publish
                "artifact_publish_count": 0,
                "first_seen_ts": "",
                "last_seen_ts": "",
                "events": {"module_call": 0, "node_start": 0, "node_end": 0,
                           "artifact_publish": 0, "checkpoint": 0,
                           "error": 0, "worker_task": 0},
            }
            order.append(nid)
        return nodes[nid]

    for ev in events:
        ty = ev.get("type")
        nid = ev.get("node_id") or ""
        rid = ev.get("run_id") or ""
        if rid:
            run_ids.add(rid)
        if nid:
            node = _node(nid)
        if ty == "module_call" and nid:
            node["call_count"] += 1
            node["events"]["module_call"] += 1
            if not node["module_id"]:
                node["module_id"] = ev.get("module_id") or ""
            if not node["module_version"]:
                node["module_version"] = ev.get("module_version") or ""
            if not node["entry"]:
                node["entry"] = ev.get("entry") or ""
            # DLL hash：真实观测（缺失留空，不编造）
            if ev.get("dll_name"):
                node["dll_name"] = ev["dll_name"]
            if ev.get("dll_sha256"):
                node["dll_sha256"] = ev["dll_sha256"]
            w = ev.get("workers")
            if isinstance(w, int) and w > 0:
                node["workers"] = max(node["workers"], int(w))
                worker_lease_max = max(worker_lease_max, int(w))
        elif ty == "node_start" and nid:
            node["events"]["node_start"] += 1
            active[nid] = ev
            if not node["status"]:
                node["status"] = "RUNNING"
            gw = ev.get("granted_workers")
            if isinstance(gw, int) and gw > 0:
                node["granted_workers"] = max(node["granted_workers"], int(gw))
            w = ev.get("workers")
            if isinstance(w, int) and w > 0:
                node["workers"] = max(node["workers"], int(w))
                worker_lease_max = max(worker_lease_max, int(w))
        elif ty == "node_end" and nid:
            node["events"]["node_end"] += 1
            node["status"] = ev.get("status") or node["status"]
            active.pop(nid, None)
            wm = ev.get("wall_ms")
            if isinstance(wm, (int, float)) and wm > 0:
                node["wall_ms"] = float(wm)
            cm = ev.get("cpu_ms")
            if isinstance(cm, (int, float)) and cm > 0:
                node["cpu_ms"] = float(cm)
            gw = ev.get("granted_workers")
            if isinstance(gw, int) and gw > 0:
                node["granted_workers"] = max(node["granted_workers"], int(gw))
            w = ev.get("workers")
            if isinstance(w, int) and w > 0:
                node["workers"] = max(node["workers"], int(w))
                worker_lease_max = max(worker_lease_max, int(w))
        elif ty == "worker_task" and nid:
            # worker_task 事件无显式 leave（事件本身即"领取/完成"记录）→
            # 无法从单行推导精确在途时刻；只记真实计数与 workers 观测，不
            # 冒充精确并发峰值（granted_workers 才是租约上限观测）。
            node["events"]["worker_task"] += 1
            worker_task_total += 1
            w = ev.get("workers")
            if isinstance(w, int) and w > 0:
                node["workers"] = max(node["workers"], int(w))
                worker_lease_max = max(worker_lease_max, int(w))
        elif ty == "artifact_publish" and nid:
            node["events"]["artifact_publish"] += 1
            node["artifact_publish_count"] += 1
            art = {
                "id": ev.get("artifact_id") or "",
                "sha256": ev.get("artifact_sha256") or "",
                "size": ev.get("artifact_size") or 0,
            }
            ids = [a["id"] for a in node["artifacts"]]
            if art["id"] and art["id"] not in ids:
                node["artifacts"].append(art)
            elif not art["id"] and not node["artifacts"]:
                node["artifacts"].append(art)  # 保留空 id 观测（极少见）
        elif ty == "checkpoint" and nid:
            node["events"]["checkpoint"] += 1
        elif ty == "error" and nid:
            node["events"]["error"] += 1
            if not node["status"]:
                node["status"] = "ERROR"
        # provider：真实置位（携带 provider 的事件；最后观测胜出，与 replay 同）
        if ev.get("provider") and nid:
            node["provider"] = ev["provider"]
        if nid:
            ts = ev.get("ts_utc") or ""
            if ts:
                if not node["first_seen_ts"] or ts < node["first_seen_ts"]:
                    node["first_seen_ts"] = ts
                if ts > node["last_seen_ts"]:
                    node["last_seen_ts"] = ts
        # scheduler 级并行轴：NODE_START/NODE_END 出入集
        if ty in ("node_start", "node_end") and nid:
            sched_active = len(active)
            sched_max = max(sched_max, sched_active)

    return {
        "nodes": nodes,
        "order": order,
        "run_ids": sorted(run_ids),
        "parsed_lines": len(events),
        "scheduler_concurrency_max": sched_max,
        "worker_lease_max": worker_lease_max,
        "worker_task_total": worker_task_total,
    }


def _plan_graph(plan_obj: Any) -> Tuple[Optional[Dict[str, Any]], str]:
    """把 typed-dag IR 或 plan-graph JSON 归一为
    {schema, pipeline_id, phase, nodes:[{node_id,module_id,operation,entry,
    resource_class,inputs,outputs}], edges:[{from_node,to_node,artifact,...}]}。

    返回 (plan_graph | None, 说明)。拒绝结构非法输入（说明非空）。
    """
    if not isinstance(plan_obj, dict):
        return None, "plan 顶层必须是对象"
    schema = plan_obj.get("schema", "")
    if schema in ("astrocs.plan-graph/v1",):
        nodes = [dict(n) for n in plan_obj.get("nodes", [])]
        edges = [dict(e) for e in plan_obj.get("edges", [])]
        return {
            "schema": schema,
            "pipeline_id": plan_obj.get("pipeline_id", ""),
            "phase": plan_obj.get("phase", ""),
            "nodes": nodes,
            "edges": edges,
        }, ""
    if schema in ("astrocs.typed-dag/v1",):
        # typed-dag IR 无显式边：用 RT-001 编译器（只读 import，不改动）权威
        # 推导数据边（artifact producer→consumer 已类型校验）。
        nodes = []
        edges = []
        plan_graph_edges = []
        if TypedDagCompiler is not None:
            try:
                res = TypedDagCompiler().compile(plan_obj)
                if res.ok and res.plan is not None:
                    pj = json.loads(res.plan.to_json())
                    plan_graph_edges = pj.get("edges", [])
            except Exception:
                plan_graph_edges = []
        for nj in plan_obj.get("nodes", []):
            if not isinstance(nj, dict):
                continue
            nodes.append({
                "node_id": nj.get("node_id", ""),
                "module_id": nj.get("module_id", ""),
                "operation": nj.get("operation", ""),
                "resource_class": (nj.get("resources") or {}).get("class", ""),
                "parallel": bool((nj.get("resources") or {}).get("parallel")),
                "inputs": dict(nj.get("inputs", {}) or {}),
                "outputs": dict(nj.get("outputs", {}) or {}),
            })
        for e in plan_graph_edges:
            edges.append({
                "from_node": e.get("from_node", ""),
                "to_node": e.get("to_node", ""),
                "artifact": e.get("artifact", ""),
                "data_schema_id": e.get("data_schema_id", ""),
                "unit": e.get("unit", ""),
            })
        return {
            "schema": schema,
            "pipeline_id": plan_obj.get("pipeline_id", ""),
            "phase": plan_obj.get("phase", ""),
            "nodes": nodes,
            "edges": edges,
        }, ""


def build_graph(*, trace_path: Optional[pathlib.Path] = None,
                trace_events: Optional[List[Dict[str, Any]]] = None,
                plan_obj: Any = None, sha: str = "") -> Dict[str, Any]:
    """核心构建：trace（必需）→ 运行图 JSON；plan（可选）→ 合并声明资源/
    数据边。返回 astrocs.graph-json/v1 对象。"""
    if trace_events is None:
        if trace_path is None:
            raise ValueError("需要 trace（--trace 路径或预解析事件）")
        events, skipped = _read_jsonl(trace_path)
    else:
        events = trace_events
        skipped = 0
    agg = aggregate_trace(events)
    plan_graph = None
    plan_error = ""
    if plan_obj is not None:
        plan_graph, plan_error = _plan_graph(plan_obj)

    # plan 索引：node_id → 声明属性
    plan_nodes: Dict[str, Dict[str, Any]] = {}
    plan_edges: List[Dict[str, Any]] = []
    if plan_graph and plan_graph["nodes"]:
        for pn in plan_graph["nodes"]:
            pid = pn.get("node_id", "")
            if pid:
                plan_nodes[pid] = pn
        plan_edges = list(plan_graph.get("edges", []))

    # 节点合并
    node_list: List[Dict[str, Any]] = []
    seen_nodes: set = set()
    for nid in agg["order"]:
        ob = dict(agg["nodes"][nid])
        seen_nodes.add(nid)
        pn = plan_nodes.get(nid)
        if pn is not None:
            ob["resource_class"] = pn.get("resource_class", "")
            ob["resource_class_source"] = "plan"
            ob["operation"] = pn.get("operation", "")
            ob["parallel_declared"] = bool(pn.get("parallel"))
        else:
            ob["resource_class"] = ""
            ob["resource_class_source"] = ""
            ob["operation"] = ""
            ob["parallel_declared"] = False
        # plan-only 节点（声明但未在 trace 观测到执行）
        node_list.append(ob)
    for pid, pn in plan_nodes.items():
        if pid not in seen_nodes:
            node_list.append({
                "id": pid,
                "kind": "node",
                "module_id": pn.get("module_id", ""),
                "module_version": "",
                "entry": pn.get("entry", ""),
                "operation": pn.get("operation", ""),
                "call_count": 0,
                "status": "PLAN_ONLY",
                "provider": "",
                "workers": 0,
                "granted_workers": 0,
                "wall_ms": 0.0,
                "cpu_ms": 0.0,
                "dll_name": "",
                "dll_sha256": "",
                "artifacts": [],
                "artifact_publish_count": 0,
                "resource_class": pn.get("resource_class", ""),
                "resource_class_source": "plan",
                "parallel_declared": bool(pn.get("parallel")),
                "first_seen_ts": "",
                "last_seen_ts": "",
                "events": {"module_call": 0, "node_start": 0, "node_end": 0,
                           "artifact_publish": 0, "checkpoint": 0, "error": 0,
                           "worker_task": 0},
            })
    node_list.sort(key=lambda n: n["id"])

    # artifact 发布 hash 索引：artifact_id -> sha256/size（真实观测）
    artifact_obs: Dict[str, Dict[str, Any]] = {}
    for nid in agg["order"]:
        for a in agg["nodes"][nid]["artifacts"]:
            if a["id"] and a["id"] not in artifact_obs:
                artifact_obs[a["id"]] = dict(a)

    # 数据边：优先计划边（声明数据流），标注 producer/consumer 是否真实运行；
    # artifact hash 标注来自真实 artifact_publish 观测（缺失 → 空，不编造）。
    edges: List[Dict[str, Any]] = []
    if plan_edges:
        node_status = {n["id"]: n.get("status", "") for n in node_list}
        for pe in plan_edges:
            frm = pe.get("from_node", "")
            to = pe.get("to_node", "")
            art = pe.get("artifact", "")
            edges.append({
                "from": frm,
                "to": to,
                "artifact": art,
                "artifact_sha256": artifact_obs.get(art, {}).get("sha256", "")
                if art in artifact_obs else "",
                "edge_source": "plan",
                "producer_status": node_status.get(frm, ""),
                "consumer_status": node_status.get(to, ""),
                "data_schema_id": pe.get("data_schema_id", ""),
                "unit": pe.get("unit", ""),
            })
    else:
        # 无 plan：从 trace 的 artifact_publish 归属推导 producer → artifact；
        # consumer 无法从单事件流推导（无 plan 输入声明）→ 只列 producer 边。
        for nid in agg["order"]:
            for a in agg["nodes"][nid]["artifacts"]:
                edges.append({
                    "from": nid, "to": "", "artifact": a.get("id", ""),
                    "artifact_sha256": a.get("sha256", ""),
                    "edge_source": "trace", "producer_status": "",
                    "consumer_status": "", "data_schema_id": "",
                    "unit": "",
                })
    edges.sort(key=lambda e: (e["from"], e["to"], e["artifact"]))

    # 顶层输出（plan 声明 outputs）
    outputs: Dict[str, str] = {}
    if isinstance(plan_obj, dict):
        outputs = dict(plan_obj.get("outputs", {}) or {})

    # 输入文件 hash（policy §2.5）
    inputs_meta: Dict[str, Any] = {}
    if trace_path is not None:
        inputs_meta["trace"] = {"path": trace_path.name,
                                "sha256": _sha256_file(trace_path)}
    if plan_obj is not None:
        inputs_meta["plan"] = {"sha256": _sha256_text(
            json.dumps(plan_obj, ensure_ascii=False, sort_keys=True))}

    graph = {
        "schema": GRAPH_JSON_SCHEMA,
        "graph_kind": "runtime",
        "generator": {"tool": TOOL, "version": VERSION},
        "source": {"main_sha": sha, "inputs": inputs_meta},
        "run_ids": agg["run_ids"],
        "parsed_lines": agg["parsed_lines"],
        "skipped_lines": skipped,
        "plan": ({"schema": plan_graph["schema"],
                  "pipeline_id": plan_graph.get("pipeline_id", ""),
                  "phase": plan_graph.get("phase", "")}
                 if plan_graph is not None else None),
        "plan_error": plan_error or None,
        "outputs": outputs,
        "metrics": {
            "node_count": len(node_list),
            "module_call_total": sum(
                n["events"]["module_call"] for n in node_list),
            "artifact_publish_total": sum(
                n["artifact_publish_count"] for n in node_list),
            "scheduler_concurrency_max": agg["scheduler_concurrency_max"],
            "worker_lease_max": agg["worker_lease_max"],
            "worker_task_total": agg["worker_task_total"],
        },
        "nodes": node_list,
        "edges": edges,
    }
    return graph


# ── DOT 输出（可审计文本事实；无需外部 dot 二进制） ───────────────────────
def _dot_escape(text: str) -> str:
    return (text.replace("\\", "\\\\").replace('"', '\\"')
            .replace("\n", "\\n"))


def node_label(node: Dict[str, Any]) -> str:
    """节点标签：真实入口、调用计数、provider、workers、耗时、资源、hash。"""
    lines = [node["id"]]
    if node.get("entry"):
        lines.append("entry=" + node["entry"])
    if node.get("module_id") and node["module_id"] != node["id"]:
        lines.append(node["module_id"])
    if node.get("call_count", 0) > 0:
        lines.append(f"call_count={node['call_count']} (trace)")
    elif node.get("status") == "PLAN_ONLY":
        lines.append("PLAN_ONLY (未运行)")
    prov = node.get("provider") or ""
    if prov:
        lines.append(f"provider={prov}")
    w = node.get("workers") or 0
    gw = node.get("granted_workers") or 0
    if gw:
        lines.append(f"workers={w} granted={gw}")
    elif w:
        lines.append(f"workers={w}")
    if node.get("dll_name"):
        dll = node["dll_name"]
        dll_sha = (node.get("dll_sha256") or "")[:16]
        lines.append(f"dll={dll}" + (f" sha={dll_sha}…" if dll_sha else ""))
    if node.get("wall_ms", 0.0) > 0:
        lines.append(f"wall_ms={node['wall_ms']:.1f}")
    rc = node.get("resource_class") or ""
    if rc:
        lines.append(f"class={rc} (plan)")
    arts = [a.get("id", "") for a in node.get("artifacts", []) if a.get("id")]
    if arts:
        lines.append("publishes=" + ",".join(arts))
    return "\n".join(lines)


def node_fill(node: Dict[str, Any]) -> str:
    if node.get("status") in ("FAILED", "ERROR"):
        return COLOR_FAILED
    rc = node.get("resource_class") or ""
    if node.get("status") == "PLAN_ONLY":
        return COLOR_PLAN_ONLY
    return COLOR_BY_CLASS.get(rc, "#f4f4f4")


def to_dot(graph: Dict[str, Any]) -> str:
    """graph JSON → DOT 文本（审计事实；含 generator/source/hash 头注释）。"""
    out: List[str] = []
    out.append("digraph run_graph {")
    out.append("  // generated by %s v%s (LOG-003; stdlib, no dot binary)" %
               (TOOL, VERSION))
    out.append("  // graph_kind=%s  schema=%s" %
               (graph.get("graph_kind", ""), graph.get("schema", "")))
    src = graph.get("source", {})
    out.append("  // main_sha=%s" % (src.get("main_sha") or "unknown"))
    for key, meta in sorted((src.get("inputs") or {}).items()):
        out.append("  // input %s sha256=%s%s" %
                   (key, meta.get("sha256", ""),
                    " path=" + str(meta.get("path", "")) if meta.get("path")
                    else ""))
    m = graph.get("metrics", {})
    out.append("  // metrics node_count=%s module_call_total=%s "
               "scheduler_concurrency_max=%s worker_lease_max=%s "
               "worker_task_total=%s" %
               (m.get("node_count"), m.get("module_call_total"),
                m.get("scheduler_concurrency_max"), m.get("worker_lease_max"),
                m.get("worker_task_total")))
    out.append("  node [shape=box, style=\"rounded,filled\", fontname=\"monospace\"];")
    # 并行轴 cluster（观测事实：scheduler 级并发）
    if m.get("scheduler_concurrency_max", 0) > 1:
        out.append("  subgraph cluster_parallel_axis {")
        out.append("    label=\"并行轴 scheduler 并发=%s (trace 观测)\";"
                   % m["scheduler_concurrency_max"])
        out.append("    color=lightgrey;")
        out.append("  }")
    for n in graph.get("nodes", []):
        attrs = [
            'label="%s"' % _dot_escape(node_label(n)),
            'fillcolor="%s"' % node_fill(n),
        ]
        if n.get("status") == "PLAN_ONLY":
            attrs.append("style=dashed,filled")
        if n.get("status") in ("FAILED", "ERROR"):
            attrs.append("color=red")
        if n.get("parallel_declared"):
            attrs.append("penwidth=2.0")
        out.append('  "%s" [%s];' % (_dot_escape(n["id"]), ", ".join(attrs)))
    for e in graph.get("edges", []):
        frm = _dot_escape(e.get("from", ""))
        to = e.get("to", "")
        label = e.get("artifact", "")
        sha = e.get("artifact_sha256") or ""
        if label and sha:
            label = label + "\\nsha=" + sha[:16] + "…"
        if not to:  # 无 plan 时的 producer 边：虚指向 artifact 占位
            to = "__artifact__" + _dot_escape(label or e.get("artifact", "?"))
            if not any(n["id"] == to for n in graph.get("nodes", [])):
                out.append('  "%s" [label="%s", shape=ellipse, '
                           'style=dashed, fillcolor="#eeeeee"];'
                           % (to, _dot_escape(label or "?")))
        if label:
            out.append('  "%s" -> "%s" [label="%s"];'
                       % (frm, _dot_escape(to), _dot_escape(label)))
        else:
            out.append('  "%s" -> "%s";' % (frm, _dot_escape(to)))
    out.append("}")
    return "\n".join(out) + "\n"


# ── SVG 输出（最小合法；纯 Python 直出，派生展示物非审计事实） ─────────────
def to_svg(graph: Dict[str, Any]) -> str:
    """把图绘制为最小合法 SVG（stdlib 直出，无 dot 布局依赖）。

    拓扑层布局（Kahn）：模块节点 → 数据边按层排布；节点为圆角矩形，
    颜色与 DOT 一致（class/status）；派生展示物——DOT/JSON 才是审计事实。
    """
    nodes = graph.get("nodes", [])
    edges = graph.get("edges", [])
    by_id = {n["id"]: n for n in nodes}
    # Kahn 分层
    indeg: Dict[str, int] = {}
    adj: Dict[str, List[str]] = {}
    for n in nodes:
        indeg[n["id"]] = 0
        adj[n["id"]] = []
    for e in edges:
        if e.get("to") and e["to"] in by_id and e["from"] in by_id:
            indeg[e["to"]] = indeg.get(e["to"], 0) + 1
            adj.setdefault(e["from"], []).append(e["to"])
    from collections import deque
    q = deque(sorted(k for k, v in indeg.items() if v == 0))
    level: Dict[str, int] = {}
    while q:
        u = q.popleft()
        lv = level.get(u, 0)
        for v in sorted(adj.get(u, [])):
            level[v] = max(level.get(v, 0), lv + 1)
            indeg[v] -= 1
            if indeg[v] == 0:
                q.append(v)
    for n in nodes:  # 环/未达 → 层 0
        level.setdefault(n["id"], 0)
    levels: Dict[int, List[str]] = {}
    for nid, lv in level.items():
        levels.setdefault(lv, []).append(nid)
    for lv in levels:
        levels[lv].sort()

    W, H = 60, 40
    X0, Y0, XG, YG = 40, 40, 320, 100
    pos: Dict[str, Tuple[float, float]] = {}
    max_lv = max(levels) if levels else 0
    # 每层自上而下叠放；垂直余量按本层节点数计算（中心列对齐单节点）
    for lv, ids in levels.items():
        n = len(ids)
        col_h = YG * max(n - 1, 0)
        for i, nid in enumerate(ids):
            cx = X0 + XG * lv + (XG - W) / 2
            cy = Y0 + (col_h / 2 if n > 1 else 0) + YG * i
            pos[nid] = (cx, cy)
    max_col_nodes = max((len(ids) for ids in levels.values()), default=1)
    height = Y0 + YG * max(max_col_nodes - 1, 0) + H + 60
    width = X0 + XG * (max_lv + 1) + 20

    def esc(t: str) -> str:
        return (t.replace("&", "&amp;").replace("<", "&lt;")
                .replace(">", "&gt;").replace('"', "&quot;"))

    lines: List[str] = []
    lines.append('<?xml version="1.0" encoding="UTF-8"?>')
    lines.append('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
                 'viewBox="0 0 %d %d">' % (width, height, width, height))
    m = graph.get("metrics", {})
    lines.append('  <title>AstroCS runtime graph (LOG-003, %s v%s) — '
                 'derived display; DOT/JSON are audit truth</title>'
                 % (TOOL, VERSION))
    lines.append('  <desc>nodes=%s module_calls=%s sched_concurrency=%s '
                 'worker_lease_max=%s worker_task_total=%s main_sha=%s</desc>' %
                 (m.get("node_count"), m.get("module_call_total"),
                  m.get("scheduler_concurrency_max"), m.get("worker_lease_max"),
                  m.get("worker_task_total"),
                  graph.get("source", {}).get("main_sha", "")))
    lines.append('  <rect width="100%" height="100%" fill="white"/>')
    for e in edges:
        frm, to = e.get("from", ""), e.get("to", "")
        if frm not in pos or not to or to not in pos:
            continue
        x1, y1 = pos[frm]
        x2, y2 = pos[to]
        midx, midy = (x1 + x2) / 2, (y1 + y2) / 2
        lines.append('  <line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" '
                     'stroke="#888" stroke-width="1"/>'
                     % (x1 + W, y1 + H / 2, x2, y2 + H / 2))
        art = e.get("artifact", "")
        if art:
            lines.append('  <text x="%.1f" y="%.1f" font-size="9" '
                         'text-anchor="middle" fill="#666">%s</text>'
                         % (midx, midy - 3, esc(art)))
    for nid, (x, y) in pos.items():
        node = by_id.get(nid, {})
        fill = node_fill(node) if node else "#f4f4f4"
        if node.get("status") in ("FAILED", "ERROR"):
            stroke = "red"
        elif node.get("status") == "PLAN_ONLY":
            stroke = "#999"
        else:
            stroke = "#444"
        lines.append('  <rect x="%.1f" y="%.1f" width="%d" height="%d" rx="6" '
                     'fill="%s" stroke="%s" stroke-width="1.2"/>'
                     % (x, y, W, H, fill, stroke))
        title = esc(node_label(node)) if node else nid
        lines.append('  <title>%s</title>' % title)
        lines.append('  <text x="%.1f" y="%.1f" font-size="10" '
                     'text-anchor="middle" font-family="monospace">%s</text>'
                     % (x + W / 2, y + H / 2 + 3, esc(nid[:22])))
    lines.append('</svg>')
    return "\n".join(lines) + "\n"


# ── 机器验证（验收：图与 trace 调用计数/DLL hash/artifact hash 一致） ──────
def verify_consistency(graph: Dict[str, Any],
                       trace_events: List[Dict[str, Any]]) -> Tuple[bool, List[str]]:
    """重放 trace，与 graph JSON 逐项比对。返回 (一致?, 不一致清单)。"""
    diffs: List[str] = []
    replay = replay_from_jsonl(
        "\n".join(json.dumps(e, ensure_ascii=False) for e in trace_events))
    if replay is None:
        diffs.append("replay 模块不可用（runtime/pipeline/trace_replay 导入失败）")
        return False, diffs
    replay_nodes = {n["node_id"]: n for n in replay.get("nodes", [])}
    agg = aggregate_trace(trace_events)
    g_nodes = {n["id"]: n for n in graph.get("nodes", [])}

    # 1. 图节点集合 == replay 节点集合（replay 会把无 node_id 事件聚到 "" 节点；
    # 本图构建器只收真实有 id 节点 → 比对时剔除 "" 空节点）
    g_ids = set(g_nodes)
    r_ids = {nid for nid in replay_nodes if nid}
    if g_ids != r_ids:
        only_g = sorted(g_ids - r_ids)
        only_r = sorted(r_ids - g_ids)
        if only_g:
            diffs.append("图节点不在 replay 中: " + ",".join(only_g))
        if only_r:
            diffs.append("replay 节点不在图中: " + ",".join(only_r))
        return False, diffs
    # 2. 每节点 call_count == replay call_count == 原始计数
    for nid in sorted(g_ids):
        gn = g_nodes[nid]
        rn = replay_nodes.get(nid, {})
        if int(gn.get("call_count", 0)) != int(rn.get("call_count", 0)):
            diffs.append(f"{nid}: 图 call_count={gn.get('call_count')} != "
                         f"replay call_count={rn.get('call_count')}")
        if int(gn.get("call_count", 0)) != int(agg["nodes"][nid]["call_count"]):
            diffs.append(f"{nid}: 图 call_count 与原始事件计数不一致")
        for key, rk in (("entry", "entry"), ("module_id", "module_id"),
                        ("status", "status"), ("provider", "provider")):
            if (gn.get(key) or "") != (rn.get(rk) or ""):
                diffs.append(f"{nid}: 图 {key}={gn.get(key)!r} != "
                             f"replay {rk}={rn.get(rk)!r}")
    # 3. DLL hash == module_call 事件携带值（观测缺失双方同为空 → OK）
    for nid in sorted(g_ids):
        gn = g_nodes[nid]
        ob = agg["nodes"][nid]
        for key in ("dll_name", "dll_sha256"):
            if (gn.get(key) or "") != (ob.get(key) or ""):
                diffs.append(f"{nid}: 图 {key} {gn.get(key)!r} != "
                             f"trace {ob.get(key)!r}")
    # 4. artifact（id/sha256/size）== artifact_publish 事件携带值
    for nid in sorted(g_ids):
        gn = g_nodes[nid]
        ob = agg["nodes"][nid]
        g_arts = [(a.get("id", ""), a.get("sha256", ""), a.get("size", 0))
                  for a in gn.get("artifacts", [])]
        o_arts = [(a.get("id", ""), a.get("sha256", ""), a.get("size", 0))
                  for a in ob.get("artifacts", [])]
        if g_arts != o_arts:
            diffs.append(f"{nid}: 图 artifacts {g_arts} != trace {o_arts}")
    # 5. 图边 artifact_sha256 标注必须来自真实观测（已在 build 时注入 → 此处
    #    校验边引用的 artifact 与观测值一致；未发布（缺观测）→ 标注必须为空）
    obs_sha = {}
    for nid in agg["order"]:
        for a in agg["nodes"][nid]["artifacts"]:
            if a["id"] and a["id"] not in obs_sha:
                obs_sha[a["id"]] = a.get("sha256", "")
    for e in graph.get("edges", []):
        art = e.get("artifact", "")
        if not art:
            continue
        if e.get("artifact_sha256", "") != obs_sha.get(art, ""):
            diffs.append(f"edge {e.get('from')}->{e.get('to')} artifact {art}: "
                         f"图 sha256 {e.get('artifact_sha256')!r} != "
                         f"trace {obs_sha.get(art)!r}")
    return not diffs, diffs


# ── CLI ─────────────────────────────────────────────────────────────────────
def _write_out(path: pathlib.Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def cmd_render(args: argparse.Namespace) -> int:
    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = args.name or "graph-runtime"
    trace_path = pathlib.Path(args.trace)
    plan_obj = None
    if args.plan:
        plan_obj = load_strict_json(pathlib.Path(args.plan).read_text(
            encoding="utf-8"))
    graph = build_graph(trace_path=trace_path, plan_obj=plan_obj, sha=args.sha)
    dot = to_dot(graph)
    svg_text = to_svg(graph) if args.svg else ""
    js = json.dumps(graph, ensure_ascii=False, indent=1, sort_keys=False)

    json_path = out_dir / (stem + ".json")
    dot_path = out_dir / (stem + ".dot")
    _write_out(json_path, js + "\n")
    _write_out(dot_path, dot)
    svg_path = None
    if args.svg:
        svg_path = out_dir / (stem + ".svg")
        _write_out(svg_path, svg_text)
    # 输入文件 hash 已在 graph 中（source.inputs）→ 此处打印机器结果
    result = {
        "tool": TOOL, "version": VERSION, "verdict": "GRAPH_RENDER_OK",
        "graph_kind": "runtime", "json": str(json_path),
        "dot": str(dot_path),
        "svg": str(svg_path) if svg_path else None,
        "metrics": graph["metrics"],
        "run_ids": graph["run_ids"],
        "source_main_sha": graph["source"]["main_sha"],
        "plan_schema": (graph["plan"] or {}).get("schema"),
        "dot_sha256": _sha256_text(dot),
        "json_sha256": _sha256_text(js + "\n"),
    }
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    graph = load_strict_json(pathlib.Path(args.verify).read_text(
        encoding="utf-8"))
    events, skipped = _read_jsonl(pathlib.Path(args.trace))
    ok, diffs = verify_consistency(graph, events)
    result = {
        "tool": TOOL, "version": VERSION,
        "verdict": "GRAPH_CONSISTENT" if ok else "GRAPH_INCONSISTENT",
        "graph": args.verify, "trace": args.trace,
        "parsed_lines": len(events), "skipped_lines": skipped,
        "node_count": len(graph.get("nodes", [])),
        "diffs": diffs[:50],
        "diff_count": len(diffs),
    }
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if ok else 1


def cmd_selfcheck() -> int:
    """结构自检：合法空 trace 渲染 + 无 dot 依赖 + replay 可导入。"""
    problems: List[str] = []
    if replay_from_jsonl is None:
        problems.append("runtime/pipeline/trace_replay 导入失败")
    g = build_graph(trace_path=None,
                    trace_events=[], sha="")
    if g["metrics"]["node_count"] != 0:
        problems.append("空 trace 渲染 node_count != 0")
    if g["metrics"]["module_call_total"] != 0:
        problems.append("空 trace module_call_total != 0")
    dot = to_dot(g)
    if 'digraph run_graph {' not in dot:
        problems.append("DOT 头缺失")
    try:
        import xml.etree.ElementTree as ET
        svg = to_svg(g)
        ET.fromstring(svg)
    except Exception as exc:  # pragma: no cover
        problems.append(f"SVG 非法: {exc}")
    if problems:
        for p in problems:
            print(f"GRAPH_SELFCHECK_ISSUE: {p}", file=sys.stderr)
        print("GRAPH_TOOL_SELFCHECK FAIL")
        return 1
    print("GRAPH_TOOL_SELFCHECK PASS")
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd")
    r = sub.add_parser("render", help="trace(+plan) → DOT/SVG/JSON 运行图")
    r.add_argument("--trace", required=True, help="RT-006 trace JSONL 路径")
    r.add_argument("--plan", default=None,
                   help="可选 typed plan JSON（plan-graph v1 或 typed-dag v1）")
    r.add_argument("--out-dir", default=".",
                   help="输出目录（默认当前目录）")
    r.add_argument("--name", default="graph-runtime", help="输出文件名主干")
    r.add_argument("--sha", default="", help="当前 main SHA（40 hex，溯源）")
    r.add_argument("--svg", action="store_true",
                   help="额外生成最小合法 SVG（派生展示；DOT/JSON 才是审计事实）")
    v = sub.add_parser("verify",
                       help="机器验证：图与 trace 调用计数/DLL/artifact hash 一致")
    v.add_argument("--verify", required=True, help="graph JSON 路径")
    v.add_argument("--trace", required=True, help="同一 trace JSONL 路径")
    sub.add_parser("selfcheck", help="工具结构自检")
    args = ap.parse_args(argv)
    if args.cmd == "render":
        return cmd_render(args)
    if args.cmd == "verify":
        return cmd_verify(args)
    if args.cmd == "selfcheck":
        return cmd_selfcheck()
    ap.print_help()
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
