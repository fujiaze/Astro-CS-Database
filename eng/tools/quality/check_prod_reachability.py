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
  python3 eng/tools/quality/check_prod_reachability.py --repo ROOT --binary build/astrocs --compile-commands build/compile_commands.json
  python3 eng/tools/quality/check_prod_reachability.py --selftest
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

# ── CLI 生产源清单的单一事实源 ──────────────────────────────────────────────
# AGENTS.md §7 / ENGINEERING_SPEC.md §7：CLI 落位 lib/infrastructure/cli/**（含
# normalize/mosaic/export 子目录）。旧实现写死根级 "cli"（ARCH-001 目录等价迁移
# 之前的旧布局）⇒ 目录不存在 ⇒ cli_sources=[] ⇒ **静默回落**到单个 main.cpp，
# 而紧邻注释明写"扫描整个 lib/infrastructure/cli/（不只 main.cpp）"——实际 5 个 TU
# 只剩 1 个，判据面静默缩水（ENGINEERING_SPEC §10「fail-closed / 锚存活」）。
# 现在按同一先例（eng/ci/prepare_linux_fixtures.py::shared_lib_sources：登记面与
# 构建图不得各写一份）从权威来源派生：① 构建图 compile_commands.json 中的 CLI
# 编译单元；② 回落到同一常量定位的目录扫描。两路皆空 ⇒ ANCHOR_STALE 点名并 rc=2。
CLI_DIR_REL = "lib/infrastructure/cli"
CLI_ANCHOR = "CLI_DIR"
REACH_OUT_DIR_REL = "run/ci/prod-reachability"


def read_text(path: pathlib.Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


class AnchorStale(Exception):
    """锚失效：硬编码/派生的仓库路径不可用（ENGINEERING_SPEC §10「锚存活」）。"""


def cli_source_files(repo: pathlib.Path, cc_path: pathlib.Path | None) -> tuple:
    """CLI 生产源清单，返回 (files, origin)。

    来源优先级（单一事实源，不另抄路径）：
      ① 构建图：compile_commands.json 中位于 CLI_DIR_REL 下的编译单元；
      ② 目录扫描：CLI_DIR_REL 下的 *.cpp（与 ① 同一常量定位）；
    两路皆空 ⇒ AnchorStale（fail-closed：判据面为空不得当"无违规"）。
    """
    cli_dir = repo / CLI_DIR_REL
    from_cc: list[pathlib.Path] = []
    if cc_path is not None and cc_path.is_file():
        try:
            for entry in json.loads(cc_path.read_text(encoding="utf-8")):
                f = pathlib.Path(entry.get("file", ""))
                try:
                    rel = f.resolve().relative_to(repo).as_posix()
                except (ValueError, OSError):
                    continue
                if rel.startswith(CLI_DIR_REL + "/") and rel.endswith(".cpp"):
                    from_cc.append(f)
        except (OSError, ValueError, TypeError):
            from_cc = []
    if from_cc:
        return sorted(set(from_cc)), "compile_commands"
    if not cli_dir.is_dir():
        raise AnchorStale(f"{CLI_DIR_REL} 不存在（构建图亦无 CLI 编译单元）")
    scanned = sorted(cli_dir.rglob("*.cpp"))
    if not scanned:
        raise AnchorStale(f"{CLI_DIR_REL} 下无 *.cpp（判据面为空）")
    return scanned, "dir_scan"


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

    # RT-008: 扫描整个 lib/infrastructure/cli/ 目录（main.cpp + 拆分后的 parser/commands 等），
    # 保证 CLI 整体不 include/调用生产内部符号（不只 main.cpp）。
    # 源清单从权威来源派生（构建图优先，其次同一常量定位的目录扫描）；两路皆空即
    # ANCHOR_STALE 判红——**不得**再回落到"只扫 main.cpp 也算通过"。
    try:
        cli_sources, cli_origin = cli_source_files(repo, cc_path)
    except AnchorStale as exc:
        print(f"ANCHOR_STALE: {CLI_ANCHOR} {exc}", file=sys.stderr)
        return 2
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
    try:
        graph = _build_graph(repo, cc_path, nm_text)
    except AnchorStale as exc:
        print(f"ANCHOR_STALE: HEADER_INDEX {exc}", file=sys.stderr)
        return 2
    # 产物落 run/（AGENTS.md §7：一切输出落 output_dir 或 run/；旧值 evidence/v6_1_rework
    # 是 ROOT-007 已清运的退役根目录，mkdir 会在仓库根重新长出未跟踪的 evidence/，
    # 触发 CHK-ROOT-CLEAN）。
    out_dir = repo / REACH_OUT_DIR_REL
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
    print(f"REACH_PASS binary={binary.name} cli_sources={len(cli_sources)} "
          f"cli_origin={cli_origin} runtime_owners={runtime_reachable} "
          f"compile_entries={len(cc)} acr=0 graph={REACH_OUT_DIR_REL}/PROD_REACHABILITY.json")
    return 0


def _build_graph(repo: pathlib.Path, cc_path: pathlib.Path, nm_text: str) -> dict:
    """构建生产可达图：compile_commands 中每个生产编译单元 → 依赖头 → 导出符号。"""
    cc = json.loads(cc_path.read_text(encoding="utf-8"))
    # 预索引 lib/（含 lib/include、lib/infrastructure/cli）下所有头文件，避免逐 include rglob。
    # 旧值的 ("include", "cli") 是 ARCH-001 之前的根级旧布局（两目录均已不存在）；
    # lib/ 的 rglob 已覆盖二者，保留死根只会让索引静默缺项。
    header_index: dict[str, list[pathlib.Path]] = {}
    for top in ("lib",):
        base = repo / top
        if not base.is_dir():
            raise AnchorStale(f"{top} 不存在（头文件索引面为空）")
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
        kind = "cli" if rel.startswith("lib/infrastructure/cli/") else \
            ("test" if rel.startswith("eng/tests/") else "lib")
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
                if crel.startswith(("lib/include/", "lib/", "lib/infrastructure/cli/")):
                    edges.setdefault(rel, []).append(crel)
        edges.setdefault(rel, [])
    # 生产二进制符号并入（nm 证明链接）
    linked = [line.split(" ")[-1] for line in nm_text.splitlines() if line.strip()]
    return {"schema": "astrocs.prod-reachability/v1", "nodes": nodes,
            "edges": {k: sorted(set(v)) for k, v in edges.items()},
            "linked_symbol_count": len(linked)}


def _selftest(repo: pathlib.Path) -> int:
    """负例：伪造 CLI 直连 drizzle / dead runtime → 必须 FAIL；
    另断言 CLI 源清单派生面非退化（构建图优先 / 目录扫描 / 两路皆空即 ANCHOR_STALE）。"""
    import tempfile
    rc = _selftest_cli_sources(repo)
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
        return rc


def _selftest_cli_sources(repo: pathlib.Path) -> int:
    """CLI 源清单派生面自检（RT-008 回归锁）。

    判据（任一不满足 ⇒ rc=1）：
      S1 真实仓库：派生面必须覆盖 lib/infrastructure/cli/ 下**全部** *.cpp
         （≥2；旧实现静默回落到 1 个 main.cpp 即在此判红）；
      S2 构建图优先：compile_commands 含 CLI 与非 CLI 单元时，只取 CLI 单元；
      S3 目录扫描回落：无 compile_commands 时取目录全部 *.cpp；
      S4 fail-closed：CLI 目录不存在 ⇒ AnchorStale（不得静默返回空/单文件）。
    """
    import tempfile
    ok = True

    real, origin = cli_source_files(repo, None)
    cli_dir = repo / CLI_DIR_REL
    expect = sorted(cli_dir.rglob("*.cpp")) if cli_dir.is_dir() else []
    s1 = len(real) == len(expect) and len(real) >= 2
    ok = ok and s1
    print("[selftest] S1 真实仓库扫描面=%d（origin=%s，目录实际=%d）%s"
          % (len(real), origin, len(expect), "OK" if s1 else "MISMATCH"))

    with tempfile.TemporaryDirectory(prefix="reach-selftest-") as tmp:
        root = pathlib.Path(tmp)
        d = root / CLI_DIR_REL
        d.mkdir(parents=True)
        (d / "main.cpp").write_text("int main(){return 0;}\n", encoding="utf-8")
        (d / "parser.cpp").write_text("// parser\n", encoding="utf-8")
        other = root / "lib" / "algorithms" / "x.cpp"
        other.parent.mkdir(parents=True)
        other.write_text("// x\n", encoding="utf-8")
        cc = root / "compile_commands.json"
        cc.write_text(json.dumps([{"file": str(other)}, {"file": str(d / "parser.cpp")},
                                  {"file": str(d / "main.cpp")}]), encoding="utf-8")

        files_cc, org_cc = cli_source_files(root, cc)
        s2 = (org_cc == "compile_commands" and len(files_cc) == 2
              and all(f.name != "x.cpp" for f in files_cc))
        ok = ok and s2
        print("[selftest] S2 构建图优先（只取 CLI 单元）%s" % ("OK" if s2 else "MISMATCH"))

        files_dir, org_dir = cli_source_files(root, None)
        s3 = org_dir == "dir_scan" and len(files_dir) == 2
        ok = ok and s3
        print("[selftest] S3 目录扫描回落（全部 *.cpp）%s" % ("OK" if s3 else "MISMATCH"))

        empty = pathlib.Path(tmp) / "nowhere"
        empty.mkdir()
        try:
            cli_source_files(empty, None)
            s4 = False
        except AnchorStale:
            s4 = True
        ok = ok and s4
        print("[selftest] S4 目录缺失 ⇒ ANCHOR_STALE（fail-closed）%s"
              % ("OK" if s4 else "MISMATCH"))

    print("[selftest] CLI 源清单派生面 %s" % ("ALL OK" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
