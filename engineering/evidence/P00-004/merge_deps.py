#!/usr/bin/env python3
"""Merge group1/group2 dependency JSONs into unified dependency_graph.json and .md"""
import json
from pathlib import Path

evidence_dir = Path(__file__).parent
g1 = json.loads((evidence_dir / "deps_group1.json").read_text(encoding="utf-8"))
g2 = json.loads((evidence_dir / "deps_group2.json").read_text(encoding="utf-8"))

# Merge modules (skip healpix_db parent if drizzle/stack already separate)
all_modules = []
seen = set()
for m in g1.get("modules", []) + g2.get("modules", []):
    name = m["module_name"]
    if name == "healpix_db":
        # healpix_db parent is a container, keep it but note children are separate
        if name not in seen:
            all_modules.append(m)
            seen.add(name)
    else:
        if name not in seen:
            all_modules.append(m)
            seen.add(name)

# Merge issues
all_issues = g1.get("potential_issues", []) + g2.get("potential_issues", [])

# Build unified graph
graph = {
    "project": "AstroCS",
    "generated_at": "2026-07-24",
    "module_count": len(all_modules),
    "modules": all_modules,
    "dependency_edges": [],
    "potential_issues": all_issues,
}

# Extract dependency edges
for m in all_modules:
    src = m["module_name"]
    for dep in m.get("includes_other_modules", []):
        if isinstance(dep, dict):
            tgt = dep.get("module", dep.get("target", str(dep)))
        else:
            tgt = dep
        graph["dependency_edges"].append({
            "from": src, "to": tgt, "type": "include"
        })
    # DLL link dependencies from external_libs that match other module DLLs
    for lib in m.get("external_libs", []):
        # -lastro_image_io -> astro_image_io
        if lib.startswith("-l"):
            dll_name = lib[2:]
            for other in all_modules:
                if other["module_name"] != src:
                    other_dll = other.get("dll_output", "") or ""
                    if dll_name in other_dll.lower() or dll_name in other["module_name"].lower():
                        graph["dependency_edges"].append({
                            "from": src, "to": other["module_name"], "type": "link"
                        })

# Deduplicate edges
seen_edges = set()
unique_edges = []
for e in graph["dependency_edges"]:
    key = (e["from"], e["to"], e["type"])
    if key not in seen_edges:
        seen_edges.add(key)
        unique_edges.append(e)
graph["dependency_edges"] = unique_edges

# Write JSON
(evidence_dir / "dependency_graph.json").write_text(
    json.dumps(graph, ensure_ascii=False, indent=2), encoding="utf-8")

# Write Markdown
lines = [
    "# AstroCS 模块依赖图",
    "",
    f"- 生成时间: 2026-07-24",
    f"- 模块数: {len(all_modules)}",
    f"- 依赖边数: {len(unique_edges)}",
    f"- 潜在问题数: {len(all_issues)}",
    "",
    "## 模块清单",
    "",
    "| 模块 | DLL 产出 | 外部库 | Python 绑定 | 数据依赖 |",
    "|---|---|---|---|---|",
]
for m in all_modules:
    dll = m.get("dll_output", "—") or "—"
    libs = ", ".join(m.get("external_libs", [])) or "—"
    py = m.get("python_binding", "—") or "—"
    data = ", ".join(m.get("data_dependencies", [])) or "—"
    lines.append(f"| {m['module_name']} | {dll} | {libs} | {py} | {data} |")

lines += ["", "## 依赖关系（调用方向）", ""]
lines += ["```"]
lines += ["调用方 → 被调用方 (类型)"]
lines += ["---"]
for e in sorted(unique_edges, key=lambda x: (x["from"], x["to"])):
    lines.append(f"{e['from']} → {e['to']} ({e['type']})")
lines += ["```", ""]

lines += ["## 分层架构", ""]
lines += ["```"]
lines += ["基础层（无跨模块依赖）:"]
lines += ["  astro_image_io    — FITS/XISF/.ahpx I/O + Pipeline 引擎"]
lines += ["  calibration       — CCD 校准（OpenMP）"]
lines += ["  dynamic_psf       — PSF 拟合（GSL, OpenMP）"]
lines += ["  gaia_xpsd_client  — Gaia 星表客户端（mmap, OpenMP）"]
lines += ["  star_detector     — 星点检测（GSL, OpenMP）"]
lines += ["  snr_estimator     — SNR 估算（OpenMP）"]
lines += [""]
lines += ["中间层（依赖基础层）:"]
lines += ["  healpix_drizzle   → astro_image_io (link+include)"]
lines += ["  healpix_stack     → astro_image_io (link+include)"]
lines += ["  photometric_calib → gaia_xpsd_client (link+include)"]
lines += ["  healpix_browser_qt → astro_image_io (link+include, Qt6/OpenGL)"]
lines += [""]
lines += ["顶层（运行时动态加载）:"]
lines += ["  orchestrator      → 运行时 LoadLibrary 加载所有 DLL"]
lines += ["  plate_solve       → 运行时 LoadLibrary 加载 astro_image_io/gaia_client/star_detector"]
lines += ["```", ""]

lines += ["## 潜在问题", ""]
for i, issue in enumerate(all_issues, 1):
    if isinstance(issue, dict):
        lines.append(f"{i}. **{issue.get('title', issue.get('module', '未知'))}**: {issue.get('description', str(issue))}")
    else:
        lines.append(f"{i}. {issue}")
lines.append("")

(evidence_dir / "dependency_graph.md").write_text("\n".join(lines), encoding="utf-8")
print(f"OK: {len(all_modules)} modules, {len(unique_edges)} edges, {len(all_issues)} issues")
print(f"  -> dependency_graph.json")
print(f"  -> dependency_graph.md")
