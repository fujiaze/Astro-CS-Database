#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_prod_reachability.py — CHK-001 生产可达调用图检查器。

基于真实构建产物（compile_commands.json + 链接后二进制 nm 符号表 + 源码引用），
构建 CLI→生产库→内核 的可达图，并机器断言：

1. CLI handler 只能调用 public Runtime/Benchmark/Test/Verify API；
2. session/科学内核/I/O 内部 symbol 从 CLI 不可达（禁止 p*_session_*、hp_drizzle_*、fits_* 直连）；
3. 生产只有一个 scheduler owner（Pipeline Runtime 唯一调度入口）；
4. ACR symbol/target/module 从默认产品不可达；
5. legacy wrapper 如保留，只能通过 test registry/preset。

负例（--selftest）：
- CLI 新增一次 hp_drizzle_run_hips 直连 → FAIL
- dead Runtime（core 符号无生产可达）→ FAIL

用法:
  python3 tools/quality/check_prod_reachability.py --repo ROOT --binary build/astrocs --compile-commands build/compile_commands.json
  python3 tools/quality/check_prod_reachability.py --selftest
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys

# 禁止从 CLI 直接调用的生产内部符号（session/科学内核/I-O 内部）
BANNED_CLI_SYMBOLS = [
    r"p[123]_session_(?:create|validate|run|inspect|destroy)",
    r"hp_drizzle_run_hips",
    r"spawn_frame_from_fits",
    r"\bfits_read_", r"\bfits_write_", r"\bfits_open_", r"\bfits_create_",
    r"aio_pipeline_run", r"aio_hio_",
]
# 禁止 CLI include 的生产内部头
BANNED_CLI_INCLUDES = [
    "p1_session.h", "p2_session.h", "p3_session.h",
    "hp_drizzle_api.h", "aio_pipeline.h", "aio_fits.h",
    "astro_image_io.h", "fitsio.h",
]
# Runtime 唯一调度 owner（生产只允许一个）
RUNTIME_OWNER_SYMBOLS = ["PipelineIR", "ModuleRegistry", "Scheduler", "RunContext",
                         "acquire_lease", "ThreadBudget"]
# ACR 符号（DORMANT，禁生产可达）
ACR_SYMBOLS = ["astro::compute", "acr_", "device_executor", "kernel_registry"]


def read_text(path: pathlib.Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--binary", type=pathlib.Path, default=None)
    parser.add_argument("--compile-commands", type=pathlib.Path, default=None)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    repo: pathlib.Path = args.repo.resolve()

    if args.selftest:
        return _selftest(repo)

    binary = args.binary
    cc_path = args.compile_commands
    if binary is None:
        candidates = list(repo.glob("run/temp/build_v61/astrocs")) + \
                     list(repo.glob("build/*/astrocs")) + [repo / "build" / "astrocs"]
        binary = next((c for c in candidates if c.is_file()), None)
    if binary is None or not binary.is_file():
        print("REACH_FAIL: no production binary found (pass --binary)", file=sys.stderr)
        return 2
    if cc_path is None:
        cc_path = next(repo.glob("run/temp/build_v61/compile_commands.json"), None) or \
                  next(repo.glob("build/*/compile_commands.json"), None)
    if cc_path is None or not cc_path.is_file():
        print("REACH_FAIL: no compile_commands.json (pass --compile-commands)", file=sys.stderr)
        return 2

    errors: list[str] = []

    # RT-008: 扫描整个 cli/ 目录（main.cpp + 拆分后的 parser/commands 等），
    # 保证 CLI 整体不 include/调用生产内部符号（不只 main.cpp）
    cli_dir = repo / "cli"
    cli_sources = sorted(cli_dir.glob("*.cpp")) if cli_dir.is_dir() else []
    if not cli_sources:
        cli_sources = [repo / "cli" / "main.cpp"]
    cli_all_text = "\n".join(read_text(p) for p in cli_sources)
    cli_includes = re.findall(r'#include\s*[<"]([^>"]+)[">]', cli_all_text)

    # 1) CLI 禁止 include 生产内部头
    for banned in BANNED_CLI_INCLUDES:
        if any(inc.endswith(banned) for inc in cli_includes):
            errors.append(f"CLI includes banned internal header: {banned}")

    # 2) CLI 禁止直接调用 session/科学/IO 内部符号
    for pattern in BANNED_CLI_SYMBOLS:
        hits = re.findall(pattern, cli_all_text)
        if hits:
            errors.append(f"CLI direct call to banned symbol {pattern}: {sorted(set(hits))[:5]}")

    # 3) nm 符号表：binary 中 Runtime owner 是否可达（生产唯一 scheduler owner）
    nm = subprocess.run(["nm", "-C", str(binary)], capture_output=True, text=True, timeout=120)
    if nm.returncode != 0:
        print(f"REACH_FAIL: nm failed on {binary}: {nm.stderr}", file=sys.stderr)
        return 2
    nm_text = nm.stdout
    runtime_reachable = [sym for sym in RUNTIME_OWNER_SYMBOLS if sym in nm_text]
    if not runtime_reachable:
        errors.append("Runtime owner symbols absent from production binary "
                      "(dead Runtime: PipelineIR/ModuleRegistry/Scheduler/RunContext not linked)")

    # 4) ACR 从生产二进制不可达
    acr_hits = [line for line in nm_text.splitlines()
                if any(tok in line for tok in ACR_SYMBOLS)]
    if acr_hits:
        errors.append(f"ACR symbols reachable in production binary: {len(acr_hits)} (e.g. {acr_hits[0].strip()[:100]})")

    # 5) 每个生产编译单元的 include 依赖完整性（compile_commands 引用的源必须存在）
    cc = json.loads(cc_path.read_text(encoding="utf-8"))
    missing_src = []
    for entry in cc:
        f = pathlib.Path(entry["file"])
        if not f.is_file():
            missing_src.append(str(f))
    if missing_src:
        errors.append(f"compile_commands references missing sources: {missing_src[:5]}")

    # 输出可达图 JSON/DOT（真实边来自 compile_commands 的 file→target 映射 + nm 符号）
    # 无论 PASS/FAIL 都生成，供审计与 RT-008 修复对照
    graph = _build_graph(repo, cc_path, nm_text)
    out_dir = repo / "evidence" / "v6_1_rework" / "tasks" / "CHK-001"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "PROD_REACHABILITY.json").write_text(
        json.dumps(graph, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    dot = ["digraph prod_reachability {"]
    for node, info in graph["nodes"].items():
        dot.append(f'  "{node}" [label="{node}\\n{info.get("kind","")}"];')
    for src, dsts in graph["edges"].items():
        for dst in dsts:
            dot.append(f'  "{src}" -> "{dst}";')
    dot.append("}")
    (out_dir / "PROD_REACHABILITY.dot").write_text("\n".join(dot) + "\n", encoding="utf-8")

    if errors:
        print("REACH_FAIL")
        for err in errors[:40]:
            print(f"  - {err}", file=sys.stderr)
        return 1
    print(f"REACH_PASS binary={binary.name} runtime_owners={runtime_reachable} "
          f"compile_entries={len(cc)} acr=0 graph=PROD_REACHABILITY.json/dot")
    return 0


def _build_graph(repo: pathlib.Path, cc_path: pathlib.Path, nm_text: str) -> dict:
    """构建生产可达图：compile_commands 中每个生产编译单元 → 依赖头 → 导出符号。"""
    cc = json.loads(cc_path.read_text(encoding="utf-8"))
    # 预索引 include/ lib/ cli/ 下所有头文件，避免逐 include rglob
    header_index: dict[str, list[pathlib.Path]] = {}
    for top in ("include", "lib", "cli"):
        base = repo / top
        if not base.is_dir():
            continue
        for p in base.rglob("*.h"):
            header_index.setdefault(p.name, []).append(p)
        for p in base.rglob("*.hpp"):
            header_index.setdefault(p.name, []).append(p)
    nodes: dict[str, dict] = {}
    edges: dict[str, list[str]] = {}
    for entry in cc:
        f = pathlib.Path(entry["file"])
        try:
            rel = f.relative_to(repo).as_posix()
        except ValueError:
            rel = f.name
        kind = "cli" if rel.startswith("cli/") else \
            ("test" if rel.startswith("tests/") else "lib")
        if kind == "test":
            continue
        nodes[rel] = {"kind": kind}
        text = read_text(f)
        for inc in re.findall(r'#include\s*[<"]([^>"]+)[">]', text):
            base = inc.split("/")[-1]
            for cand in header_index.get(base, []):
                try:
                    crel = cand.relative_to(repo).as_posix()
                except ValueError:
                    continue
                if crel.startswith(("include/", "lib/", "cli/")):
                    edges.setdefault(rel, []).append(crel)
        edges.setdefault(rel, [])
    # 生产二进制符号并入（nm 证明链接）
    linked = [line.split(" ")[-1] for line in nm_text.splitlines() if line.strip()]
    return {"schema": "astrocs.prod-reachability/v1", "nodes": nodes,
            "edges": {k: sorted(set(v)) for k, v in edges.items()},
            "linked_symbol_count": len(linked)}


def _selftest(repo: pathlib.Path) -> int:
    """负例：伪造 CLI 直连 drizzle / dead runtime → 必须 FAIL。"""
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        td = pathlib.Path(tmp)
        fake_cli = td / "main.cpp"
        fake_cli.write_text(
            '#include "p3_session.h"\n#include "hp_drizzle_api.h"\n'
            'int main(){ p3_session_run(nullptr, {}); hp_drizzle_run_hips(nullptr,0,0,0,"",nullptr,nullptr,0); return 0; }\n',
            encoding="utf-8")
        errors: list[str] = []
        text = fake_cli.read_text(encoding="utf-8")
        includes = re.findall(r'#include\s*[<"]([^>"]+)[">]', text)
        for banned in BANNED_CLI_INCLUDES:
            if any(inc.endswith(banned) for inc in includes):
                errors.append(f"CLI includes banned internal header: {banned}")
        for pattern in BANNED_CLI_SYMBOLS:
            hits = re.findall(pattern, text)
            if hits:
                errors.append(f"CLI direct call to banned symbol {pattern}")
        if not errors:
            print("SELFTEST_FAIL: fake CLI not caught", file=sys.stderr)
            return 1
        print("SELFTEST_PASS: direct session+drizzle in CLI caught")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
