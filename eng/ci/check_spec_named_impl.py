#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-SPEC-NAMED-IMPL-ON-PROD-PATH —— 规范点名的权威实现必须在生产可达路径上。

依据
  - 工程控制/RELEASE-02/GAP_AUDIT.md §9.44「暴露的门禁盲区」（:1295-1298，负责人令
    「须新增门禁类 CHK-SPEC-NAMED-IMPL-ON-PROD-PATH」）；
  - reports/RELEASE-02/design-gap-synthesis.md DG-C-01（:708-713）；
  - docs/ci/01_CHECKS.md §1（唯一注册表 · 能绿能红 · fail-closed · 锚存活）。

防的复发缺口（三条同类缺陷，均为 P0）：
  - S1-002：ALG-STARDET-001（docs/algorithms/STAR_DETECTION_ALGORITHMS.md:8）点名
    lib/algorithms/star_detection/src/sdet_api.cpp 为**唯一权威生产源**，但生产
    star-psf 节点（module_adapters.cpp p1_op_star_psf_impl）跑的是
    wrapper_phase1::StarDetector（345,960 条源 vs sdet 1,473 条，Gaia 纯度 0.212%）；
  - S2-NS-01：ALG-NOISE（docs/algorithms/NOISE_ESTIMATION.md:120）写「生产符号唯一源 =
    lib/algorithms/noise_snr/cpp/src/noise_model.cpp」，但该模块子图因根
    CMakeLists.txt:240 的 add_subdirectory(lib/algorithms/noise_snr) 被注释而不在构建图；
  - S4-P3X-06/P3X-12：p3_v6_export.cpp 被
    docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv 标 production=yes，实际只被
    tests/integration/v6_p3 编译 ⇒ FZ-P3-MODES 等四条 FROZEN 合同在生产运行面无载体。

判据（登记表 = eng/ci/spec_named_impls.json；每条含 spec/named/kind/owner/exit_condition）
  E1 anchor_alive（fail-closed rc=2）：每条登记的 spec 文档存在，且其 quote 逐字出现
     （规范不再点名该实现 ⇒ 登记表本身失效，必须显式改表，不得静默判绿）；
  E2 named_file_alive（fail-closed rc=2）：登记的点名文件存在；
  E3 prod_path：kind=prod_path 的条目，点名文件必须出现在**根 CMake 构建图**
     （自根 CMakeLists.txt 沿未注释的 add_subdirectory 递归）中某个 target 的源列表，
     且该 target 在生产入口（默认 astrocs）的 target_link_libraries 传递闭包内；
  E4 call_site：kind=call_site 的条目，点名的生产节点函数体必须真正引用权威符号，
     且该调用点文件本身在闭包内；
  E5 inventory_claim：登记文件若同时被
     docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv 标 production_reachable=yes，
     则该声明必须与 E3 的实际可达性一致（防清单反标）；
  E6 scanned>0（fail-closed rc=2）：登记表为空 / 构建图闭包为空 ⇒ rc=2，禁止空转判绿。

已登记缺口（eng/ci/ledgers/spec_named_impl_gaps.json，非豁免）
  未能满足 E3/E4/E5 的条目必须由台账显式承载，逐条带 id/kind/reason/owner/exit_condition
  五字段（缺一 ⇒ rc=2）。台账命中 ≠ 通过：该条仍以 GAP(ledgered) 计入 stdout 与 JSON
  （ledgered_gaps），--strict 下与未登记缺口同样判红。台账条目若不再复现 ⇒
  ledger_stale 判红（豁免必须存活，只减不增）。本门禁**不写** eng/ci/exemptions.json。

用法
  python3 eng/ci/check_spec_named_impl.py [--repo ROOT] [--manifest F] [--ledger F]
                                      [--entry TARGET] [--json-out F] [--strict] [--self-test]
exit 0 = 全部条目在生产可达路径上（或缺口已由台账显式承载）；1 = 有 finding；
exit 2 = 锚点/台账/登记表不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-SPEC-NAMED-IMPL-ON-PROD-PATH"
MANIFEST = "eng/ci/spec_named_impls.json"
LEDGER = "eng/ci/ledgers/spec_named_impl_gaps.json"
INVENTORY = "docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv"
DEFAULT_ENTRY_TARGET = "astrocs"
SOURCE_SUFFIXES = (".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx")
CMAKE_KEYWORDS = ("STATIC", "SHARED", "MODULE", "INTERFACE", "OBJECT", "ALIAS", "IMPORTED",
                  "WIN32", "MACOSX_BUNDLE", "EXCLUDE_FROM_ALL")
LINK_KEYWORDS = ("PUBLIC", "PRIVATE", "INTERFACE", "DEBUG", "OPTIMIZED", "GENERAL",
                 "LINK_PUBLIC", "LINK_PRIVATE", "LINK_INTERFACE_LIBRARIES")
_CMAKE_VAR_RE = re.compile(r"\$\{([A-Za-z0-9_]+)\}")


# --------------------------------------------------------------------- cmake graph ----
def _strip_cmake_comments(text: str) -> str:
    out = []
    for line in text.splitlines():
        cut = line.find("#")
        out.append(line[:cut] if cut >= 0 else line)
    return "\n".join(out)


def _commands(text: str, name: str):
    """产出 name(...) 的实参文本（括号配平；含跨行）。"""
    for m in re.finditer(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"\s*\(", text):
        start = text.find("(", m.start())
        depth = 0
        for i in range(start, len(text)):
            if text[i] == "(":
                depth += 1
            elif text[i] == ")":
                depth -= 1
                if depth == 0:
                    yield text[start + 1:i]
                    break


def _tokens(body: str):
    return [t for t in re.split(r"[\s\n\r\t]+", body.strip()) if t]


def _expand(token: str, variables: dict, depth: int = 0):
    """展开 ${VAR}；未知变量返回 None（不猜）。"""
    if depth > 8:
        return None
    out = token
    for _ in range(8):
        m = _CMAKE_VAR_RE.search(out)
        if not m:
            return out
        key = m.group(1)
        value = variables.get(key)
        if value is None:
            return None
        out = out[:m.start()] + value + out[m.end():]
    return None


def _iter_reachable_cmake(repo: pathlib.Path):
    """自根 CMakeLists.txt 沿未注释 add_subdirectory 递归（只认根构建图）。"""
    root = repo / "CMakeLists.txt"
    if not root.is_file():
        raise gc.GateError("ANCHOR_MISSING: CMakeLists.txt（根构建图唯一事实源）")
    seen, queue = set(), [root]
    while queue:
        path = queue.pop(0)
        rel = path.relative_to(repo).as_posix()
        if rel in seen:
            continue
        seen.add(rel)
        yield path
        text = _strip_cmake_comments(gc.read_text(path, rel))
        for body in _commands(text, "add_subdirectory"):
            toks = _tokens(body)
            if not toks:
                continue
            sub = toks[0].strip('"')
            if "$" in sub:
                continue
            candidate = (path.parent / sub).resolve()
            try:
                candidate.relative_to(repo.resolve())
            except ValueError:
                continue
            cm = candidate / "CMakeLists.txt"
            if cm.is_file():
                queue.append(cm)


def parse_cmake_graph(repo: pathlib.Path):
    """返回 {targets: {name: {sources, file, kind}}, edges: {name: set(dep)}}。"""
    targets: dict = {}
    edges: dict = {}
    for cmake in _iter_reachable_cmake(repo):
        rel = cmake.relative_to(repo).as_posix()
        text = _strip_cmake_comments(gc.read_text(cmake, rel))
        variables = {
            "CMAKE_CURRENT_SOURCE_DIR": cmake.parent.as_posix(),
            "CMAKE_CURRENT_LIST_DIR": cmake.parent.as_posix(),
            "CMAKE_SOURCE_DIR": repo.as_posix(),
            "PROJECT_SOURCE_DIR": repo.as_posix(),
        }
        for body in _commands(text, "set"):
            toks = _tokens(body)
            if len(toks) >= 2 and not _CMAKE_VAR_RE.search(toks[0]):
                value = _expand(toks[1], variables)
                if value is not None:
                    variables[toks[0]] = value
        for kind in ("add_library", "add_executable"):
            for body in _commands(text, kind):
                toks = _tokens(body)
                if not toks:
                    continue
                name = toks[0].strip('"')
                if "$" in name or not re.match(r"^[A-Za-z_][A-Za-z0-9_.\-]*$", name):
                    continue
                info = targets.setdefault(name, {"sources": set(), "file": rel, "kind": kind})
                for tok in toks[1:]:
                    if tok.upper() in CMAKE_KEYWORDS:
                        continue
                    resolved = _expand(tok.strip('"'), variables)
                    if resolved is None:
                        continue
                    candidate = pathlib.Path(resolved)
                    if not candidate.is_absolute():
                        candidate = cmake.parent / candidate
                    try:
                        relsrc = candidate.resolve().relative_to(repo.resolve()).as_posix()
                    except ValueError:
                        continue
                    if relsrc.endswith(SOURCE_SUFFIXES):
                        info["sources"].add(relsrc)
        for body in _commands(text, "target_link_libraries"):
            toks = _tokens(body)
            if not toks:
                continue
            name = toks[0].strip('"')
            if "$" in name:
                continue
            deps = edges.setdefault(name, set())
            for tok in toks[1:]:
                tok = tok.strip('"')
                if tok.upper() in LINK_KEYWORDS:
                    continue
                if "$" in tok or "::" in tok:
                    continue
                deps.add(tok)
    return {"targets": targets, "edges": edges}


def production_closure(graph: dict, entry: str):
    targets = graph["targets"]
    if entry not in targets:
        raise gc.GateError("ANCHOR_STALE: 生产入口 target %r 不在根构建图" % entry)
    seen, stack = set(), [entry]
    while stack:
        node = stack.pop()
        if node in seen:
            continue
        seen.add(node)
        for dep in graph["edges"].get(node, ()):  # 未声明的名字 = 外部库，忽略
            if dep in targets and dep not in seen:
                stack.append(dep)
    if not seen:
        raise gc.GateError("ANCHOR_STALE: 生产闭包为空（禁止空转判绿）")
    return seen


def _inventory_rows(repo: pathlib.Path):
    path = repo / INVENTORY
    if not path.is_file():
        raise gc.GateError("ANCHOR_MISSING: %s" % INVENTORY)
    rows = {}
    lines = gc.read_text(path, INVENTORY).splitlines()
    for idx, line in enumerate(lines[1:], start=2):
        cells = line.split(",")
        if len(cells) < 5:
            continue
        loc = cells[2].strip()
        if loc:
            rows.setdefault(loc, []).append((idx, cells[4].strip()))
    return rows


def _function_body(text: str, name: str):
    m = re.search(r"(?<![A-Za-z0-9_:])" + re.escape(name) + r"\s*\(", text)
    if not m:
        return None
    start = text.find("(", m.start())
    depth, end = 0, None
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                end = i
                break
    if end is None:
        return None
    brace = text.find("{", end)
    if brace < 0:
        return None
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1:i]
    return None


def _references(body: str, token: str) -> bool:
    if "::" in token:
        return token in body
    return re.search(r"(?<![A-Za-z0-9_])" + re.escape(token) + r"(?![A-Za-z0-9_])", body) is not None


# ------------------------------------------------------------------------ evaluation ----
def evaluate(repo: pathlib.Path, manifest_path=None, ledger_path=None, entry_target=None,
             strict=False):
    manifest_file = pathlib.Path(manifest_path) if manifest_path else repo / MANIFEST
    ledger_file = pathlib.Path(ledger_path) if ledger_path else repo / LEDGER
    manifest = gc.read_json(manifest_file, str(manifest_file))
    if not isinstance(manifest, dict) or not isinstance(manifest.get("entries"), list):
        raise gc.GateError("MANIFEST_INVALID: %s（需 {entries:[...]}）" % manifest_file)
    entries = manifest["entries"]
    if not entries:
        raise gc.GateError("MANIFEST_EMPTY: %s（禁止空转判绿）" % manifest_file)
    ledger = gc.load_ledger(ledger_file, str(ledger_file))

    entry_target = entry_target or manifest.get("production_entry") or DEFAULT_ENTRY_TARGET
    graph = parse_cmake_graph(repo)
    closure = production_closure(graph, entry_target)
    inventory = _inventory_rows(repo)

    findings, results, used_ledger = [], [], set()
    for item in entries:
        eid = item.get("id")
        if not isinstance(eid, str) or not eid.strip():
            raise gc.GateError("MANIFEST_ENTRY_MISSING_ID: %r" % (item,))
        for field in ("kind", "spec", "named", "owner", "exit_condition"):
            if not item.get(field):
                raise gc.GateError("MANIFEST_ENTRY_MISSING_FIELD: %s.%s" % (eid, field))
        kind = item["kind"]
        if kind not in ("prod_path", "call_site", "inventory_claim"):
            raise gc.GateError("MANIFEST_ENTRY_BAD_KIND: %s kind=%r" % (eid, kind))
        spec = item["spec"]
        doc_rel, quote = spec.get("doc"), spec.get("quote")
        if not doc_rel or (kind != "inventory_claim" and not quote):
            raise gc.GateError("MANIFEST_ENTRY_MISSING_FIELD: %s.spec.doc/quote" % eid)
        doc_text = gc.read_text(repo / doc_rel, doc_rel)
        if quote and quote not in doc_text:
            raise gc.GateError(
                "ANCHOR_STALE: %s 的规范 quote 不再出现于 %s（规范不再点名该实现 ⇒ 必须显式改登记表）"
                % (eid, doc_rel))
        named = item["named"]
        named_rel = named.get("file")
        if not named_rel:
            raise gc.GateError("MANIFEST_ENTRY_MISSING_FIELD: %s.named.file" % eid)
        if not (repo / named_rel).is_file():
            raise gc.GateError("ANCHOR_STALE: %s 点名文件不存在 %s" % (eid, named_rel))

        status, detail = "pass", {}
        targets_with = sorted(t for t in graph["targets"]
                              if named_rel in graph["targets"][t]["sources"])
        on_prod = sorted(t for t in targets_with if t in closure)
        detail["targets_with_source"] = targets_with
        detail["targets_in_production_closure"] = on_prod
        if kind == "prod_path":
            if not on_prod:
                status = "gap"
                detail["why"] = ("点名文件不在生产入口 %r 的传递闭包内（源列表命中 target=%s）"
                                 % (entry_target, targets_with or "无"))
        elif kind == "call_site":
            site = item.get("call_site") or {}
            site_rel, fn_name = site.get("file"), site.get("function")
            token = site.get("must_reference")
            if not (site_rel and fn_name and token):
                raise gc.GateError(
                    "MANIFEST_ENTRY_MISSING_FIELD: %s.call_site.{file,function,must_reference}" % eid)
            site_text = gc.read_text(repo / site_rel, site_rel)
            body = _function_body(site_text, fn_name)
            if body is None:
                raise gc.GateError("ANCHOR_STALE: %s 调用点函数 %s 不在 %s" % (eid, fn_name, site_rel))
            site_targets = sorted(t for t in graph["targets"]
                                  if site_rel in graph["targets"][t]["sources"])
            site_on_prod = sorted(t for t in site_targets if t in closure)
            detail["call_site_file"] = site_rel
            detail["call_site_targets"] = site_targets
            detail["call_site_in_production_closure"] = site_on_prod
            if not site_on_prod:
                status = "gap"
                detail["why"] = "调用点文件 %s 不在生产闭包内" % site_rel
            elif not _references(body, token):
                status = "gap"
                detail["why"] = ("生产节点 %s() 未引用权威符号 %s（权威实现被替代实现置换）"
                                 % (fn_name, token))
            else:
                detail["why"] = "生产节点 %s() 已引用 %s" % (fn_name, token)
        else:  # inventory_claim：清单声明面 vs 实际可达性
            inv = inventory.get(named_rel, [])
            yes_rows = sorted(ln for ln, flag in inv if flag == "yes")
            detail["inventory_production_yes"] = yes_rows
            if not yes_rows:
                status = "withdrawn"
                detail["why"] = ("清单未声明 production=yes（声明面已更正/移除）⇒ 本登记项失效，"
                                 "应显式更新登记表")
            elif not on_prod:
                status = "gap"
                detail["why"] = ("清单 %s 声明 production=yes 但点名文件不在生产入口 %r 的传递闭包内"
                                 "（源列表命中 target=%s）"
                                 % (INVENTORY, entry_target, targets_with or "无"))
            else:
                detail["why"] = "清单声明与生产闭包一致"

        inv = inventory.get(named_rel, [])
        detail.setdefault("inventory_production_yes", sorted(ln for ln, flag in inv if flag == "yes"))
        if kind != "inventory_claim" and inv and status == "pass" and not on_prod:
            status = "gap"
            detail["why"] = "清单声明 production=yes 但实际不在生产闭包内"

        ledger_id = item.get("ledger_id") or eid
        if status == "gap":
            if strict or ledger_id not in ledger:
                findings.append("%s: %s | owner=%s | exit_condition=%s%s"
                                % (eid, detail.get("why", ""), item["owner"], item["exit_condition"],
                                   "" if ledger_id in ledger else " | 未登记台账"))
            else:
                used_ledger.add(ledger_id)
                status = "gap_ledgered"
        results.append({"id": eid, "kind": kind, "defect_ref": item.get("defect_ref"),
                        "named_file": named_rel, "status": status, "detail": detail,
                        "spec": "%s%s" % (doc_rel,
                                          (":" + str(spec.get("loc"))) if spec.get("loc") else "")})

    for lid in sorted(set(ledger) - used_ledger):
        entry = ledger[lid]
        if entry.get("kind") in ("dormant", "known_gap"):
            findings.append("ledger_stale:%s（台账条目已不复现 ⇒ 必须删除，豁免只减不增）" % lid)

    extra = {
        "production_entry": entry_target,
        "targets_in_graph": len(graph["targets"]),
        "targets_in_production_closure": len(closure),
        "checked_entries": len(entries),
        "passed": sum(1 for r in results if r["status"] == "pass"),
        "ledgered_gaps": [r["id"] for r in results if r["status"] == "gap_ledgered"],
        "open_gaps": [r["id"] for r in results if r["status"] == "gap"],
        "withdrawn": [r["id"] for r in results if r["status"] == "withdrawn"],
        "entries": results,
    }
    return findings, extra


# ---------------------------------------------------------------------------- selftest ----
_FIXTURE_CMAKE = """cmake_minimum_required(VERSION 3.20)
project(fixture)
add_library(fx_algo STATIC lib/algo/impl.cpp)
add_library(fx_dead STATIC lib/algo/dead.cpp)
add_executable(astrocs app/main.cpp app/node.cpp)
target_link_libraries(astrocs PRIVATE fx_algo)
"""
_FIXTURE_APP = "#include <cstdio>\nint main(){return 0;}\n"
_FIXTURE_ALGO = "int fx_authority(int x) { return x + 1; }\n"
_FIXTURE_DEAD = "int fx_dead(int x) { return x; }\n"
_FIXTURE_SITE = "int fx_node(int x) {\n  return fx_authority(x);\n}\n"
_FIXTURE_SITE_BAD = "int fx_node(int x) {\n  return fx_other(x);\n}\n"
_FIXTURE_DOC = "# ALG-FX-001\n> 唯一权威生产源: lib/algo/impl.cpp\n"
_FIXTURE_DOC_DRIFT = "# ALG-FX-001\n> 生产源: 见源码\n"
_FIXTURE_INVENTORY = ("category,symbol,location,classification,production_reachable,"
                      "phase,thread_model,evidence,risk_note\n")


def _write_fixture(root: pathlib.Path, *, linked=True, call_ok=True, doc_ok=True,
                   inventory_yes=False, inventory_extra=(), entries=None, ledger=None):
    (root / "lib/algo").mkdir(parents=True, exist_ok=True)
    (root / "app").mkdir(parents=True, exist_ok=True)
    (root / "eng/ci/ledgers").mkdir(parents=True, exist_ok=True)
    (root / "docs/architecture").mkdir(parents=True, exist_ok=True)
    cmake = _FIXTURE_CMAKE
    if not linked:
        cmake = cmake.replace("target_link_libraries(astrocs PRIVATE fx_algo)\n", "")
    (root / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
    (root / "lib/algo/impl.cpp").write_text(_FIXTURE_ALGO, encoding="utf-8")
    (root / "lib/algo/dead.cpp").write_text(_FIXTURE_DEAD, encoding="utf-8")
    (root / "app/main.cpp").write_text(_FIXTURE_APP, encoding="utf-8")
    (root / "app/node.cpp").write_text(_FIXTURE_SITE if call_ok else _FIXTURE_SITE_BAD,
                                       encoding="utf-8")
    (root / "docs/ALG.md").write_text(_FIXTURE_DOC if doc_ok else _FIXTURE_DOC_DRIFT,
                                      encoding="utf-8")
    inv = _FIXTURE_INVENTORY
    if inventory_yes:
        inv += "io_writer,impl.cpp,lib/algo/impl.cpp,production,yes,Phase1,-,x,\n"
    for row in inventory_extra:
        inv += row + "\n"
    (root / INVENTORY).write_text(inv, encoding="utf-8")
    if entries is None:
        entries = [
            {"id": "FX-PROD-PATH", "kind": "prod_path",
             "spec": {"doc": "docs/ALG.md", "loc": "2", "quote": "唯一权威生产源: lib/algo/impl.cpp"},
             "named": {"file": "lib/algo/impl.cpp"}, "owner": "fx", "exit_condition": "fx"},
            {"id": "FX-CALL-SITE", "kind": "call_site",
             "spec": {"doc": "docs/ALG.md", "loc": "2", "quote": "唯一权威生产源: lib/algo/impl.cpp"},
             "named": {"file": "lib/algo/impl.cpp", "symbol": "fx_authority"},
             "call_site": {"file": "app/node.cpp", "function": "fx_node",
                           "must_reference": "fx_authority"},
             "owner": "fx", "exit_condition": "fx"},
        ]
    (root / MANIFEST).write_text(json.dumps({"production_entry": "astrocs", "entries": entries},
                                            ensure_ascii=False, indent=2), encoding="utf-8")
    (root / LEDGER).write_text(json.dumps(
        ledger if ledger is not None else {"ledger_schema": gc.LEDGER_SCHEMA,
                                           "ledger_id": "fixture", "entries": []},
        ensure_ascii=False, indent=2), encoding="utf-8")


def _selftest() -> int:
    import tempfile
    failures = []

    def run(repo, **kw):
        try:
            findings, _extra = evaluate(pathlib.Path(repo), **kw)
        except gc.GateError as exc:
            return "GateError: %s" % exc
        return findings

    def check(name, got, want_fail):
        is_fail = True if isinstance(got, str) else bool(got)
        if is_fail != want_fail:
            failures.append("%s: want_fail=%s got=%r" % (name, want_fail, got))
        else:
            shown = got if isinstance(got, str) else "%d finding(s)" % len(got)
            print("SELFTEST_PASS %s (want_fail=%s, %s)" % (name, want_fail, shown))

    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        d_ok = base / "ok"
        _write_fixture(d_ok)
        check("green_all_on_prod_path", run(d_ok), False)

        d_unlinked = base / "unlinked"
        _write_fixture(d_unlinked, linked=False)
        check("red_target_not_linked_into_production", run(d_unlinked), True)

        d_nocall = base / "nocall"
        _write_fixture(d_nocall, call_ok=False)
        check("red_node_calls_other_impl", run(d_nocall), True)

        d_inv = base / "inventory"
        _write_fixture(d_inv, inventory_yes=True)
        check("green_inventory_claim_matches", run(d_inv), False)

        ledger_ok = {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fx", "entries": [
            {"id": "FX-CALL-SITE", "kind": "known_gap", "reason": "fixture",
             "owner": "fx", "exit_condition": "fx"}]}
        d_ledger = base / "ledgered"
        _write_fixture(d_ledger, call_ok=False, ledger=ledger_ok)
        check("green_gap_ledgered", run(d_ledger), False)
        check("red_strict_exposes_ledgered_gap", run(d_ledger, strict=True), True)

        d_stale = base / "stale"
        _write_fixture(d_stale, ledger=ledger_ok)
        check("red_ledger_stale", run(d_stale), True)

        d_badledger = base / "badledger"
        _write_fixture(d_badledger, ledger={
            "ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fx",
            "entries": [{"id": "FX-CALL-SITE", "kind": "known_gap"}]})
        check("failclosed_ledger_missing_fields", run(d_badledger), True)

        d_doc = base / "docdrift"
        _write_fixture(d_doc, doc_ok=False)
        check("failclosed_spec_quote_gone", run(d_doc), True)

        d_empty = base / "empty"
        _write_fixture(d_empty, entries=[])
        check("failclosed_empty_manifest", run(d_empty), True)

        claim_entry = [{"id": "FX-INV-CLAIM", "kind": "inventory_claim",
                        "spec": {"doc": "docs/ALG.md", "loc": "2"},
                        "named": {"file": "lib/algo/dead.cpp"}, "owner": "fx", "exit_condition": "fx"}]
        d_invred = base / "invred"
        _write_fixture(d_invred, entries=claim_entry, inventory_extra=(
            "io_writer,dead.cpp,lib/algo/dead.cpp,production,yes,Phase1,-,x,",))
        check("red_inventory_claims_unreachable_file", run(d_invred), True)

        d_invgone = base / "invgone"
        _write_fixture(d_invgone, entries=claim_entry)
        check("green_inventory_claim_withdrawn", run(d_invgone), False)

        d_missing = base / "missingfile"
        _write_fixture(d_missing, entries=[
            {"id": "FX-MISSING", "kind": "prod_path",
             "spec": {"doc": "docs/ALG.md", "loc": "2", "quote": "唯一权威生产源: lib/algo/impl.cpp"},
             "named": {"file": "lib/algo/does_not_exist.cpp"}, "owner": "fx", "exit_condition": "fx"}])
        check("failclosed_named_file_missing", run(d_missing), True)

    if failures:
        print("SELFTEST_FAIL:")
        for item in failures:
            print("  " + item)
        return 1
    print("SELFTEST_PASS: all cases match expectation")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="CHK-SPEC-NAMED-IMPL-ON-PROD-PATH 规范点名权威实现生产可达性门")
    ap.add_argument("--repo", default=str(gc.repo_root()))
    ap.add_argument("--manifest", default=None)
    ap.add_argument("--ledger", default=None)
    ap.add_argument("--entry", default=None, help="生产入口 target（默认取登记表 production_entry）")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--strict", action="store_true", help="审计用：忽略缺口台账，全部判红")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return _selftest()
    repo = pathlib.Path(args.repo).resolve()
    try:
        findings, extra = evaluate(repo, args.manifest, args.ledger, args.entry, args.strict)
    except gc.GateError as exc:
        print("%s_FAIL: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2
    if extra["ledgered_gaps"]:
        print("%s: %d 条已登记缺口（台账承载，非豁免；--strict 下判红）：%s"
              % (CHECK_ID, len(extra["ledgered_gaps"]), ", ".join(extra["ledgered_gaps"])))
    if findings:
        gc.print_findings(CHECK_ID, findings)
        gc.report("FAIL", CHECK_ID, findings, extra, args.json_out)
        return 1
    print("%s_PASS entries=%d passed=%d ledgered_gaps=%d closure_targets=%d"
          % (CHECK_ID, extra["checked_entries"], extra["passed"],
             len(extra["ledgered_gaps"]), extra["targets_in_production_closure"]))
    gc.report("PASS", CHECK_ID, [], extra, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
