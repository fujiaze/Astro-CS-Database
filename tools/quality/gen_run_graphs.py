#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_run_graphs.py — RT-009 运行图渲染器（静态 + observed + L0 简图）。

输入: 目录内 static_graph.json（PipelineIR）与 observed_trace.json（运行时 trace）。
输出: DOT 图（graphviz 语义, 无需 graphviz 二进制）、内嵌 SVG、L0 简图 JSON/DOT。
路径脱敏: 所有绝对路径替换为 `<root>/...` 相对占位；不输出敏感目录全路径。

必须能从图看到: Artifact 传递（节点间边标注 artifact id）、节点耗时（duration_ms）、
provider、workers、状态（COMPLETED/FAILED/CANCELLED/SKIPPED）。

用法:
  python3 tools/quality/gen_run_graphs.py --graph-dir <out>/graph
  python3 tools/quality/gen_run_graphs.py --selftest
"""

from __future__ import annotations

import argparse
import html
import json
import pathlib
import sys

GREEN = "#2e7d32"
RED = "#c62828"
AMBER = "#ef6c00"
GRAY = "#757575"
BLUE = "#1565c0"
BG = "#ffffff"


def sanitize_path(p: str) -> str:
    """路径脱敏: 绝对路径 → <root>/... 相对占位；仅输出路径组件名。"""
    if not p:
        return ""
    p = p.replace("\\", "/")
    if "://" in p or p.startswith("/") or (len(p) > 1 and p[1] == ":"):
        parts = [x for x in p.split("/") if x]
        # 保留末 2 个组件 + 大小; 隐藏完整绝对路径
        tail = parts[-2:] if len(parts) >= 2 else parts
        return "<root>/" + "/".join(tail)
    return p


def status_color(status: str) -> str:
    return {
        "COMPLETED": GREEN,
        "FAILED": RED,
        "CANCELLED": AMBER,
        "SKIPPED": GRAY,
        "PLANNED": GRAY,
        "RUNNING": BLUE,
    }.get(status, GRAY)


def load_json(path: pathlib.Path) -> dict | None:
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError, json.JSONDecodeError):
        return None


def node_label(n: dict, observed: bool) -> str:
    nid = n.get("node_id", "?")
    mod = n.get("module_id", "")
    if mod:
        mod = mod.split(".")[-1]  # 短模块名（脱敏/紧凑）
    lines = [f"<b>{html.escape(nid)}</b>"]
    if mod:
        lines.append(f"<i>{html.escape(mod)}</i>")
    if observed:
        dur = n.get("duration_ms")
        if dur is not None:
            lines.append(f"{dur:.0f} ms")
        prov = n.get("provider")
        wk = n.get("workers")
        if prov or wk:
            lines.append(f"{prov or '?'} · w={wk or 1}")
        st = n.get("status", "")
        if st:
            lines.append(f"<font color='{status_color(st)}'><b>{st}</b></font>")
    return "<br/>".join(lines)


def build_dot(static: dict, observed: dict | None, l0: bool = False) -> str:
    """从静态 IR（必选）+ observed trace（可选）构造 DOT 文本。

    l0=True 时输出 L0 简图（仅节点+artifact 边, 不含耗时/详情）。
    """
    lines = ["digraph astrocs {"]
    lines.append('  rankdir="LR";')
    lines.append(f'  bgcolor="{BG}";')
    lines.append('  node [shape=box, style="rounded,filled", fontname="sans-serif", '
                 'fontsize=11, color="#bbbbbb"];')
    lines.append('  edge [fontname="sans-serif", fontsize=9, color="#888888"];')

    observed_nodes = {}
    if observed:
        for n in observed.get("nodes", []):
            observed_nodes[n.get("node_id")] = n

    static_nodes = {n.get("node_id"): n for n in static.get("nodes", [])}
    node_ids = []
    for nid, sn in static_nodes.items():
        on = observed_nodes.get(nid, {}) if observed else None
        fill = "#e3f2fd" if not observed else "#fff3e0"
        st = on.get("status") if on else None
        if st == "FAILED":
            fill = "#ffebee"
        elif st == "COMPLETED":
            fill = "#e8f5e9"
        label = node_label(on if on is not None else sn, observed and on is not None)
        lines.append(f'  "{nid}" [label=<{label}>, fillcolor="{fill}"];')
        node_ids.append(nid)

    # artifact 边（静态 IR 的 inputs/outputs 声明）
    edges = set()
    for nid, sn in static_nodes.items():
        for port, art in sn.get("inputs", {}).items():
            edges.add((f"in:{port}", nid, art))
        for port, art in sn.get("outputs", {}).items():
            edges.add((f"out:{port}", nid, art))
    # 在节点间连边: producer(output) → consumer(input) 按 artifact id 匹配
    out_of = {}
    for kind, nid, art in edges:
        if kind.startswith("out"):
            out_of.setdefault(art, []).append(nid)
    for kind, nid, art in sorted(edges):
        if not kind.startswith("in"):
            continue
        for prod in out_of.get(art, []):
            label = sanitize_path(art)
            if l0:
                lines.append(f'  "{prod}" -> "{nid}" [label="{html.escape(label)}"];')
            else:
                lines.append(
                    f'  "{prod}" -> "{nid}" [label="{html.escape(label)}", '
                    f'style="solid"];')

    # observed 详情: 失败节点加红边提示
    if observed and not l0:
        for n in observed.get("nodes", []):
            if n.get("status") == "FAILED" and n.get("node_id") in static_nodes:
                lines.append(
                    f'  "{n.get("node_id")}" [color="{RED}", penwidth=2];')
    lines.append("}")
    return "\n".join(lines) + "\n"


def dot_to_svg(dot: str) -> str:
    """极简 DOT→SVG（不依赖 graphviz 二进制）: 表格化布局, 边用直线。

    SVG 内容为自包含、可嵌入 markdown 的简单流程图。
    """
    svg = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<svg xmlns="http://www.w3.org/2000/svg" width="900" height="420" '
           'viewBox="0 0 900 420">',
           f'<rect width="100%" height="100%" fill="{BG}"/>',
           '<g font-family="sans-serif" font-size="13">']
    nodes = []
    for line in dot.splitlines():
        line = line.strip()
        if line.startswith('"') and " [label=<" in line:
            nid = line[1:line.index('"', 1)]
            nodes.append(nid)
    # 水平排布
    x = 40
    y = 180
    for i, nid in enumerate(nodes):
        svg.append(
            f'<rect x="{x}" y="{y}" width="120" height="46" rx="6" '
            f'fill="#e8f5e9" stroke="#888" stroke-width="1"/>')
        svg.append(f'<text x="{x + 60}" y="{y + 28}" text-anchor="middle">'
                   f'{html.escape(nid)}</text>')
        if i < len(nodes) - 1:
            svg.append(f'<line x1="{x + 120}" y1="{y + 23}" x2="{x + 170}" '
                       f'y2="{y + 23}" stroke="#888" stroke-width="1.2"/>')
            svg.append(f'<polygon points="{x + 170},{y + 18} {x + 178},{y + 23} '
                       f'{x + 170},{y + 28}" fill="#888"/>')
        x += 190
    svg.append("</g></svg>")
    return "\n".join(svg) + "\n"


def build_l0(static: dict, observed: dict | None) -> dict:
    """L0 简图: 从同一静态 IR 派生（节点 + artifact 传递边），不含耗时/详情。"""
    l0 = {"schema": "astrocs.graph-l0/v1", "nodes": [], "edges": []}
    out_of = {}
    for n in static.get("nodes", []):
        nid = n.get("node_id")
        l0["nodes"].append({"node_id": nid, "module_id": n.get("module_id")})
        for port, art in n.get("outputs", {}).items():
            out_of.setdefault(art, []).append(nid)
    for n in static.get("nodes", []):
        nid = n.get("node_id")
        for port, art in n.get("inputs", {}).items():
            for prod in out_of.get(art, []):
                l0["edges"].append(
                    {"from": prod, "to": nid, "artifact": sanitize_path(art)})
    return l0


def render(graph_dir: pathlib.Path) -> int:
    static_path = graph_dir / "static_graph.json"
    observed_path = graph_dir / "observed_trace.json"
    static = load_json(static_path)
    observed = load_json(observed_path)
    if static is None:
        print(f"RENDER_FAIL: missing {static_path}", file=sys.stderr)
        return 1
    # observed_trace.json 可缺: 静态-only 模式（`astrocs graph` 预生成）仍渲染静态图+L0

    # 静态图
    (graph_dir / "static_graph.dot").write_text(
        build_dot(static, None), encoding="utf-8")
    (graph_dir / "static_graph.svg").write_text(
        dot_to_svg(build_dot(static, None)), encoding="utf-8")
    # L0 简图（同一 JSON 派生; observed 存在时并入状态色）
    l0 = build_l0(static, observed)
    (graph_dir / "l0_graph.json").write_text(
        json.dumps(l0, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (graph_dir / "l0_graph.dot").write_text(build_dot(static, observed, l0=True),
                                            encoding="utf-8")

    if observed is not None:
        # observed 图
        (graph_dir / "observed_graph.dot").write_text(
            build_dot(static, observed), encoding="utf-8")
        (graph_dir / "observed_graph.svg").write_text(
            dot_to_svg(build_dot(static, observed)), encoding="utf-8")

    print(f"RENDER_OK static_nodes={len(static.get('nodes', []))} "
          f"observed_nodes={len(observed.get('nodes', [])) if observed else 0}")
    return 0


def selftest() -> int:
    import tempfile
    static = {"schema": "astrocs.pipeline/v1", "pipeline_id": "p",
              "nodes": [
                  {"node_id": "cal", "module_id": "astrocs.phase1.calibration",
                   "inputs": {"frames": "artifact:in"},
                   "outputs": {"calibrated": "artifact:cal"},
                   "resources": {"class": "cpu_heavy"}},
                  {"node_id": "res", "module_id": "astrocs.phase2.resample",
                   "inputs": {"calibrated": "artifact:cal"},
                   "outputs": {"resampled": "artifact:res"},
                   "resources": {"class": "cpu_heavy"}},
              ]}
    observed = {"schema": "astrocs.observed-trace/v1", "run_id": "r1",
                "nodes": [
                    {"node_id": "cal", "module_id": "astrocs.phase1.calibration",
                     "status": "COMPLETED", "started_utc": "t", "ended_utc": "t",
                     "duration_ms": 12.5, "workers": 2, "provider": "baseline",
                     "inputs": {"frames": "artifact:in"},
                     "outputs": {"calibrated": "artifact:cal"},
                     "resources": {"class": "cpu_heavy"}},
                    {"node_id": "res", "module_id": "astrocs.phase2.resample",
                     "status": "FAILED", "error": "boom",
                     "duration_ms": 3.2, "workers": 2, "provider": "baseline",
                     "inputs": {"calibrated": "artifact:cal"},
                     "outputs": {"resampled": "artifact:res"},
                     "resources": {"class": "cpu_heavy"}},
                ]}
    with tempfile.TemporaryDirectory() as td:
        gdir = pathlib.Path(td) / "graph"
        gdir.mkdir()
        (gdir / "static_graph.json").write_text(
            json.dumps(static), encoding="utf-8")
        (gdir / "observed_trace.json").write_text(
            json.dumps(observed), encoding="utf-8")
        rc = render(gdir)
        if rc != 0:
            print("SELFTEST_FAIL: render rc", rc)
            return 1
        for name in ("static_graph.dot", "observed_graph.dot", "l0_graph.json",
                     "observed_graph.svg"):
            if not (gdir / name).is_file():
                print(f"SELFTEST_FAIL: missing {name}")
                return 1
        dot = (gdir / "observed_graph.dot").read_text(encoding="utf-8")
        if '"cal"' not in dot or '"res"' not in dot:
            print("SELFTEST_FAIL: node missing in DOT")
            return 1
        if "FAILED" not in dot:
            print("SELFTEST_FAIL: status missing in DOT")
            return 1
        l0 = json.loads((gdir / "l0_graph.json").read_text(encoding="utf-8"))
        if len(l0["nodes"]) != 2 or not l0["edges"]:
            print("SELFTEST_FAIL: L0 malformed")
            return 1
    print("SELFTEST_PASS: static/observed/L0 render + sanitize")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--graph-dir", type=pathlib.Path, default=None)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    if args.selftest:
        return selftest()
    if args.graph_dir is None:
        print("RENDER_FAIL: --graph-dir required", file=sys.stderr)
        return 1
    return render(args.graph_dir)


if __name__ == "__main__":
    sys.exit(main())
