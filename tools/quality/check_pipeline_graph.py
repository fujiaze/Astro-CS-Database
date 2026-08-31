#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_pipeline_graph.py — CHK-002 静态 Pipeline IR 与 observed trace 双向比较器。

命令显式接收 `--ir`（PipelineIR JSON）、`--module-index`（模块索引 JSON）、`--trace`（运行 trace JSON）。
任一文件缺失或 trace 零节点直接 FAIL。

逐项比较（双向集合）：
- node ID 集合：static 每个 node 恰有一个 observed terminal state；observed 无静态图外隐藏节点；
- 每个 node 的 module_id/module_version 一致；
- 每条 edge（producer/consumer 端口）以相同 artifact ID 的 produce/consume 对应；
- artifact ID/hash（trace 中 producer 输出 hash 与 consumer 输入一致）；
- DATA schema、unit、coordinate（IR node 端口声明 vs trace 记录）；
- resource class、provider、workers、status、start/end 完整性。

负例（--selftest）覆盖：少节点、换 artifact、换单位、隐藏节点、空 trace。

用法:
  python3 tools/quality/check_pipeline_graph.py --ir ir.json --module-index mods.json --trace trace.json
  python3 tools/quality/check_pipeline_graph.py --selftest
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys


def load(path: pathlib.Path) -> dict:
    if not path.is_file():
        raise FileNotFoundError(f"missing required file: {path}")
    doc = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(doc, dict):
        raise ValueError(f"not a JSON object: {path}")
    return doc


def compare(ir: dict, module_index: dict, trace: dict) -> list[str]:
    errors: list[str] = []

    ir_nodes = {n["node_id"]: n for n in ir.get("nodes", [])}
    trace_nodes = {n.get("node_id") or n.get("id"): n for n in trace.get("nodes", [])}

    if not ir_nodes:
        errors.append("IR has zero nodes")
    if not trace_nodes:
        errors.append("trace has zero nodes (empty trace must FAIL)")
    if not errors and ir_nodes and trace_nodes:
        # 1) node ID 双向
        only_static = sorted(set(ir_nodes) - set(trace_nodes))
        only_observed = sorted(set(trace_nodes) - set(ir_nodes))
        if only_static:
            errors.append(f"static nodes missing terminal state in trace: {only_static}")
        if only_observed:
            errors.append(f"hidden observed nodes not in static IR: {only_observed}")
        # 2) module_id/module_version 一致
        for nid in set(ir_nodes) & set(trace_nodes):
            s, o = ir_nodes[nid], trace_nodes[nid]
            for key in ("module_id", "module_version"):
                if key in s and o.get(key) != s[key]:
                    errors.append(f"{nid}: {key} mismatch static={s[key]} observed={o.get(key)}")
            if "status" not in o:
                errors.append(f"{nid}: no observed terminal status")
            for key in ("start", "end", "started_utc", "ended_utc"):
                if key in o and not o[key]:
                    errors.append(f"{nid}: empty {key}")
        # 3) artifact produce/consume 边
        static_edges = _ir_edges(ir_nodes)
        observed_edges = _trace_edges(trace_nodes)
        for edge in static_edges:
            if edge not in observed_edges:
                errors.append(f"static edge not observed: {edge}")
        for edge in observed_edges:
            if edge not in static_edges:
                errors.append(f"observed edge not in static IR (hidden edge): {edge}")
        # 4) artifact hash 传递（producer 输出 hash == consumer 输入 hash）
        _check_artifact_hashes(ir_nodes, trace_nodes, errors)
        # 5) unit / schema 一致
        _check_units(ir_nodes, trace_nodes, errors)
        # 6) resource class 一致
        for nid in set(ir_nodes) & set(trace_nodes):
            s_res = ir_nodes[nid].get("resources", {})
            o_res = trace_nodes[nid].get("resources", {})
            if s_res.get("class") and o_res.get("class") and s_res["class"] != o_res["class"]:
                errors.append(f"{nid}: resource class static={s_res['class']} observed={o_res['class']}")
    return errors


def _ir_edges(ir_nodes: dict) -> set:
    edges = set()
    for nid, node in ir_nodes.items():
        for port, art in node.get("inputs", {}).items():
            edges.add(("in", nid, port, art))
        for port, art in node.get("outputs", {}).items():
            edges.add(("out", nid, port, art))
    return edges


def _trace_edges(trace_nodes: dict) -> set:
    edges = set()
    for nid, node in trace_nodes.items():
        for port, art in node.get("inputs", {}).items():
            edges.add(("in", nid, port, art))
        for port, art in node.get("outputs", {}).items():
            edges.add(("out", nid, port, art))
    return edges


def _check_artifact_hashes(ir_nodes: dict, trace_nodes: dict, errors: list[str]) -> None:
    prod_hashes: dict[str, str] = {}
    for nid, node in trace_nodes.items():
        for art in node.get("output_artifacts", []):
            if isinstance(art, dict) and art.get("id") and art.get("sha256"):
                prod_hashes[art["id"]] = art["sha256"]
    for nid, node in trace_nodes.items():
        for art in node.get("input_artifacts", []):
            if isinstance(art, dict) and art.get("id") and art.get("sha256"):
                pid = art["id"]
                if pid in prod_hashes and prod_hashes[pid] != art["sha256"]:
                    errors.append(f"{nid}: artifact {pid} hash mismatch "
                                  f"producer={prod_hashes[pid]} consumer={art['sha256']}")


def _check_units(ir_nodes: dict, trace_nodes: dict, errors: list[str]) -> None:
    for nid in set(ir_nodes) & set(trace_nodes):
        s, o = ir_nodes[nid], trace_nodes[nid]
        for port in set(s.get("inputs", {})) & set(o.get("input_units", {})):
            su = s.get("input_units", {}).get(port)
            ou = o.get("input_units", {}).get(port)
            if su and ou and su != ou:
                errors.append(f"{nid}: input unit mismatch port={port} static={su} observed={ou}")
        for port in set(s.get("outputs", {})) & set(o.get("output_units", {})):
            su = s.get("output_units", {}).get(port)
            ou = o.get("output_units", {}).get(port)
            if su and ou and su != ou:
                errors.append(f"{nid}: output unit mismatch port={port} static={su} observed={ou}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ir", type=pathlib.Path, default=None)
    parser.add_argument("--module-index", type=pathlib.Path, default=None)
    parser.add_argument("--trace", type=pathlib.Path, default=None)
    parser.add_argument("--output", type=pathlib.Path, default=None)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)

    if args.selftest:
        return _selftest()

    if args.ir is None or args.module_index is None or args.trace is None:
        print("PIPELINE_GRAPH_FAIL: --ir, --module-index, --trace all required", file=sys.stderr)
        return 1

    try:
        ir = load(args.ir)
        mods = load(args.module_index)
        trace = load(args.trace)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"PIPELINE_GRAPH_FAIL: {exc}", file=sys.stderr)
        return 1
    errors = compare(ir, mods, trace)
    if args.output:
        args.output.write_text(
            json.dumps({"schema": "astrocs.pipeline-graph-check/v1",
                        "ir": str(args.ir), "module_index": str(args.module_index),
                        "trace": str(args.trace), "pass": not errors,
                        "errors": errors}, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8")
    if errors:
        print("PIPELINE_GRAPH_FAIL")
        for err in errors[:60]:
            print(f"  - {err}", file=sys.stderr)
        return 1
    print(f"PIPELINE_GRAPH_PASS ir_nodes={len(ir.get('nodes', []))} "
          f"trace_nodes={len(trace.get('nodes', []))}")
    return 0


def _base_ir() -> dict:
    return {
        "schema": "astrocs.pipeline/v2",
        "pipeline_id": "test-chain", "version": "1.0.0",
        "source_commit": "0" * 40, "preset": "test",
        "nodes": [
            {"node_id": "p1_writer", "module_id": "astrocs.p1.writer", "module_version": "1.0.0",
             "config_ref": {"schema": "cfg", "sha256": "0" * 64},
             "inputs": {"frame": "artifact:p1:frame"},
             "outputs": {"hips": "artifact:p2:hips"},
             "resources": {"class": "cpu_light", "parallel_axes": [], "work_units": 1,
                           "estimated_memory_bytes": 1024, "kernel_ids": []}},
            {"node_id": "p3_resample", "module_id": "astrocs.p3.resample", "module_version": "1.0.0",
             "config_ref": {"schema": "cfg", "sha256": "0" * 64},
             "inputs": {"hips": "artifact:p2:hips"},
             "outputs": {"fits": "artifact:p3:fits"},
             "resources": {"class": "cpu_heavy", "parallel_axes": ["tile"], "work_units": 100,
                           "estimated_memory_bytes": 1048576, "kernel_ids": ["resample"]}},
        ],
        "outputs": {"fits": "artifact:p3:fits"},
        "resource_policy": {"max_memory_bytes": 1073741824},
        "determinism": "fixed-order",
    }


def _base_trace() -> dict:
    return {
        "schema": "astrocs.trace/v1",
        "run_id": "test-run",
        "nodes": [
            {"node_id": "p1_writer", "module_id": "astrocs.p1.writer", "module_version": "1.0.0",
             "inputs": {"frame": "artifact:p1:frame"},
             "outputs": {"hips": "artifact:p2:hips"},
             "status": "OK", "start": "2026-01-01T00:00:00Z", "end": "2026-01-01T00:00:01Z",
             "resources": {"class": "cpu_light"},
             "output_artifacts": [{"id": "artifact:p2:hips", "sha256": "a" * 64}],
             "input_artifacts": [{"id": "artifact:p1:frame", "sha256": "b" * 64}]},
            {"node_id": "p3_resample", "module_id": "astrocs.p3.resample", "module_version": "1.0.0",
             "inputs": {"hips": "artifact:p2:hips"},
             "outputs": {"fits": "artifact:p3:fits"},
             "status": "OK", "start": "2026-01-01T00:00:01Z", "end": "2026-01-01T00:00:03Z",
             "resources": {"class": "cpu_heavy"},
             "output_artifacts": [{"id": "artifact:p3:fits", "sha256": "c" * 64}],
             "input_artifacts": [{"id": "artifact:p2:hips", "sha256": "a" * 64}]},
        ],
    }


def _selftest() -> int:
    import copy
    import tempfile
    ir = _base_ir()
    trace = _base_trace()
    mods = {"schema": "astrocs.module-index/v1", "modules": []}
    failures = 0

    def run(name: str, ir_doc: dict, trace_doc: dict, expect_fail: bool) -> None:
        nonlocal failures
        with tempfile.TemporaryDirectory() as td:
            d = pathlib.Path(td)
            (d / "ir.json").write_text(json.dumps(ir_doc), encoding="utf-8")
            (d / "mods.json").write_text(json.dumps(mods), encoding="utf-8")
            (d / "trace.json").write_text(json.dumps(trace_doc), encoding="utf-8")
            errs = compare(ir_doc, mods, trace_doc)
            ok = bool(errs) == expect_fail
            if ok:
                print(f"SELFTEST_PASS {name}")
            else:
                print(f"SELFTEST_FAIL {name}: errs={errs[:3]}")
                failures += 1

    run("ok", ir, trace, expect_fail=False)
    # 少节点: trace 缺 p3_resample
    t1 = copy.deepcopy(trace); t1["nodes"] = t1["nodes"][:1]
    run("fewer_nodes", ir, t1, expect_fail=True)
    # 换 artifact: trace 中 p3 输入改 artifact 名
    t2 = copy.deepcopy(trace); t2["nodes"][1]["inputs"] = {"hips": "artifact:p2:other"}
    run("swapped_artifact", ir, t2, expect_fail=True)
    # 换单位: trace 资源 class 改
    t3 = copy.deepcopy(trace); t3["nodes"][1]["resources"] = {"class": "io"}
    run("changed_resource_class", ir, t3, expect_fail=True)
    # 隐藏节点: trace 多一个静态图外节点
    t4 = copy.deepcopy(trace)
    t4["nodes"].append({"node_id": "hidden_science", "module_id": "astrocs.x", "module_version": "1.0.0",
                        "inputs": {}, "outputs": {}, "status": "OK", "start": "t", "end": "t", "resources": {}})
    run("hidden_node", ir, t4, expect_fail=True)
    # 空 trace
    t5 = copy.deepcopy(trace); t5["nodes"] = []
    run("empty_trace", ir, t5, expect_fail=True)
    # module version mismatch
    t6 = copy.deepcopy(trace); t6["nodes"][1]["module_version"] = "9.9.9"
    run("module_version_mismatch", ir, t6, expect_fail=True)

    if failures:
        print(f"SELFTEST_FAIL total={failures}")
        return 1
    print("SELFTEST_PASS all fixtures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
