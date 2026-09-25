#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ARCH-001／ARCH-002 不变量判据（读真实构建图、安装面与登记面）—— 纯函数 + 夹具。

存在理由（独立审查一页纸 S1 第 26／29 条，同型）
  * 第 26 条：单一用户入口不变量的「测试断言」与命题**方向相反** ——
    eng/tests/arch/test_single_cli.py 以「清单里 production exe 数 == 0」为通过条件，
    五条用例只对文档文本做正则、零查构建面 ⇒ 入口唯一性没有实现判据。
  * 第 29 条：生产入口 acsd 在登记面零登记，而「没登记」被写成了通过条件。
  本模块把两条判据写成**对被检对象**求值、方向与命题一致的纯函数，并给出可注入违规的
  夹具，供 unittest 直接做「改前放过 / 改后判红」的负例（AGENTS.md §9）。

被检对象（四类，全部经 eng/ci/cmake_graph.py —— 真实构建图唯一实现）
  1. 根 CMake 构建图：可执行目标集合、生产闭包、生产入口（取登记表 production_entry）；
  2. 安装规则 eng/cmake/install_layout.cmake：被安装的 target（与可执行目标求交 = 交付的可执行单元）；
  3. 安装清单 eng/packaging/install-tree.contract.json：kind=exe 单元；
  4. 登记面 docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv：exe_target 行。

口径
  - 锚点读不到 / 集合为空 ⇒ 返回 FAIL-CLOSED finding（红），不得当成「没问题」；
  - 判据不读任何文档文字（夹具里没有 ARCHITECTURE.md，判据照样成立）；
  - 判词逐条列全、带对象名或 文件:行，不截断。
"""
import csv
import importlib.util
import json
import os
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent.parent

INVENTORY = "docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv"
INSTALL_CONTRACT = "eng/packaging/install-tree.contract.json"
INSTALL_RULES = "eng/cmake/install_layout.cmake"
ENTRY_REGISTRY = "eng/ci/spec_named_impls.json"
GRAPH_MODULE = "eng/ci/cmake_graph.py"
INV_HEADER = ("category,symbol,location,classification,production_reachable,"
              "phase,thread_model,evidence,risk_note")


def _load_graph_module():
    """加载真实构建图读取器（唯一实现）。缺失 ⇒ RuntimeError（调用方转 FAIL-CLOSED）。"""
    path = REPO / GRAPH_MODULE
    if not path.is_file():
        raise RuntimeError("ANCHOR_MISSING: %s（真实构建图读取器缺失）" % GRAPH_MODULE)
    spec = importlib.util.spec_from_file_location("acsd_cmake_graph", str(path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def read_inventory_rows(repo=REPO):
    path = pathlib.Path(repo) / INVENTORY
    if not path.is_file():
        raise RuntimeError("ANCHOR_MISSING: %s" % INVENTORY)
    with path.open(encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def read_install_contract_exe_units(repo=REPO):
    path = pathlib.Path(repo) / INSTALL_CONTRACT
    if not path.is_file():
        raise RuntimeError("ANCHOR_MISSING: %s" % INSTALL_CONTRACT)
    doc = json.loads(path.read_text(encoding="utf-8"))
    return [u for u in doc.get("units", []) if u.get("kind") == "exe"]


def installed_executable_targets(mod, repo, graph):
    """(安装规则里被安装的**可执行**目标集合, 未解析的安装目标列表)。"""
    installed, unresolved = mod.install_targets(pathlib.Path(repo))
    exes = set(mod.executable_targets(graph))
    return installed & exes, unresolved


def production_exe_rows(rows):
    return [r for r in rows
            if r.get("category") == "exe_target" and r.get("classification") == "production"]


def _production_row_findings(rows, entry, exes):
    """登记面「production exe」面的判据：恰一条、等于入口、可达=yes、不是幽灵行。"""
    findings = []
    prod = production_exe_rows(rows)
    if len(prod) != 1:
        findings.append(
            "R3 登记面 %s 的 exe_target+production 行 %d 条 != 1（%s）"
            " —— 命题是「恰一」，0 条同样是违反（旧断言把 0 条当通过）"
            % (INVENTORY, len(prod), [r.get("symbol") for r in prod]))
        return findings
    row = prod[0]
    if row.get("symbol") != entry:
        findings.append("R3 登记面的 production exe %r != 生产入口 %r"
                        % (row.get("symbol"), entry))
    if row.get("production_reachable") != "yes":
        findings.append("R3 登记面 production 行 %r 的 production_reachable=%r != yes"
                        % (row.get("symbol"), row.get("production_reachable")))
    if exes and row.get("symbol") not in exes:
        findings.append("R4 登记面 production 行 %r 不是根构建图里的可执行目标（幽灵行）"
                        % row.get("symbol"))
    return findings


def single_entry_findings(repo=REPO):
    """命题（方向 = 恰一）：每平台**恰好一个**用户入口 = 生产入口。

    依据 docs/architecture/ARCHITECTURE.md §1（发布物为每平台恰好一个用户入口）
    与 §7 不变量 1（唯一生产入口=acsd）。
    """
    repo = pathlib.Path(repo)
    try:
        mod = _load_graph_module()
        graph = mod.parse_cmake_graph(repo)
        entry = mod.production_entry(repo)
        exes = mod.executable_targets(graph)
        closure = mod.production_closure(graph, entry)
        installed_exes, unresolved = installed_executable_targets(mod, repo, graph)
        units = read_install_contract_exe_units(repo)
        rows = read_inventory_rows(repo)
    except Exception as exc:  # noqa: BLE001 —— 锚点不可用 = 判红（fail-closed）
        return ["FAIL-CLOSED: %s" % exc]

    findings = []
    if not exes:
        findings.append("E0 根构建图可执行目标集合为空 ⇒ 禁止空转判绿（fail-closed）")
    if not installed_exes and not unresolved:
        findings.append("E0 安装规则 %s 未安装任何可执行目标 ⇒ 禁止空转判绿（fail-closed）"
                        % INSTALL_RULES)
    if entry not in exes:
        findings.append("E1 生产入口 %r 不是根构建图里的可执行目标（%d 个可执行目标）"
                        % (entry, len(exes)))
    in_closure = sorted(n for n in closure if n in exes)
    if in_closure != [entry]:
        findings.append("E2 生产闭包内可执行目标 %s != [%r]（唯一入口不成立）"
                        % (in_closure, entry))
    if unresolved:
        findings.append("E3 安装规则 %s 有未解析的 install(TARGETS) 目标 %s ⇒ fail-closed"
                        % (INSTALL_RULES, unresolved))
    if sorted(installed_exes) != [entry]:
        findings.append("E3 安装规则 %s 安装的可执行目标 %s != [%r]"
                        "（每平台恰好一个用户入口）"
                        % (INSTALL_RULES, sorted(installed_exes), entry))
    if len(units) != 1:
        findings.append("E4 %s 的 kind=exe 单元 %d 个 != 1（%s）"
                        % (INSTALL_CONTRACT, len(units),
                           [u.get("install_path") for u in units]))
    else:
        base = os.path.basename(str(units[0].get("install_path")))
        if base.endswith(".exe"):
            base = base[:-4]
        if base != entry:
            findings.append("E4 安装清单 exe 单元 %r 的 basename %r != 生产入口 %r"
                            % (units[0].get("install_path"), base, entry))
    findings.extend(_production_row_findings(rows, entry, exes))
    return findings


def _anchor_finding(repo, row):
    """登记行 evidence 的锚必须活着：文件存在，且行号在该文件内（无行号者只查文件）。"""
    import re
    ev = (row.get("evidence") or "").strip()
    m = re.match(r"^([^:]+):(\d+)", ev)
    path = m.group(1) if m else ev.split(" ")[0]
    if not path:
        return "R5 登记行 %r 的 evidence 为空（无锚）" % row.get("symbol")
    f = pathlib.Path(repo) / path
    if not f.is_file():
        return ("R5 登记行 %r 的锚文件不存在：%s（锚已死）"
                % (row.get("symbol"), path))
    if m:
        lines = f.read_text(encoding="utf-8", errors="replace").splitlines()
        if int(m.group(2)) > len(lines):
            return ("R5 登记行 %r 的锚 %s 超出文件行数 %d（锚已死）"
                    % (row.get("symbol"), ev, len(lines)))
    return None


def registration_findings(repo=REPO):
    """命题：登记面**包含**构建产出的可执行目标集合（方向 = 包含）。

    依据一页纸 S1 第 29 条：生产入口本身曾在登记面零登记，而该缺失被写成通过条件；
    故判据必须是「登记集合 ⊇ 构建产出的可执行目标集合」，不是「production exe 数 == 0」。
    """
    repo = pathlib.Path(repo)
    try:
        mod = _load_graph_module()
        graph = mod.parse_cmake_graph(repo)
        entry = mod.production_entry(repo)
        exes = set(mod.executable_targets(graph))
        rows = read_inventory_rows(repo)
    except Exception as exc:  # noqa: BLE001
        return ["FAIL-CLOSED: %s" % exc]

    findings = []
    if not exes:
        findings.append("R1 根构建图可执行目标集合为空 ⇒ 禁止空转判绿（fail-closed）")
    registered = [r for r in rows if r.get("category") == "exe_target"]
    if not registered:
        findings.append("R1 登记面 %s 的 exe_target 行为 0 ⇒ 禁止空转判绿（fail-closed）"
                        % INVENTORY)
    registered_names = {r.get("symbol") for r in registered}
    missing = sorted(exes - registered_names)
    if missing:
        findings.append("R2 构建图可执行目标未登记 %d 个（登记面 %d 条 exe_target 行）：%s"
                        % (len(missing), len(registered_names), ", ".join(missing)))
    for row in registered:
        f = _anchor_finding(repo, row)
        if f:
            findings.append(f)
    findings.extend(_production_row_findings(rows, entry, exes))
    return findings


# --------------------------------------------------------------------------- 夹具 ----
def build_fixture(root, *, entry="acsd", graph_entry=None, extra_exe=None,
                  install_extra_exe=False, contract_extra_exe=False, csv_exes=None,
                  csv_drop=(), csv_production=None, csv_bad_anchor=False):
    """最小夹具仓：根 CMakeLists + 安装规则 + 安装清单 + 入口登记表 + 登记面 CSV。

    每个开关对应一条可注入的违规（负例），默认值 = 合规（保护性正例）。
    csv_production=None 表示按 csv_exes 的 classification 自动填 production 列。
    graph_entry 用于把「构建出来的 exe 名」与「登记的生产入口名」分开（E1 负例）。
    """
    root = pathlib.Path(root)
    for sub in ("app", "lib/algo", "eng/cmake", "eng/packaging", "eng/ci",
                "docs/architecture"):
        (root / sub).mkdir(parents=True, exist_ok=True)

    built_exe = graph_entry or entry
    extra_line = ("add_executable(%s app/extra.cpp)" % extra_exe) if extra_exe else ""
    (root / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(fx LANGUAGES CXX)\n"
        "add_library(fx_algo STATIC lib/algo/impl.cpp)\n"
        "add_executable(%s app/main.cpp)\n"
        "target_link_libraries(%s PRIVATE fx_algo)\n"
        "%s\n" % (built_exe, built_exe, extra_line), encoding="utf-8")
    (root / "app/main.cpp").write_text("int main(){return 0;}\n", encoding="utf-8")
    (root / "app/extra.cpp").write_text("int main(){return 0;}\n", encoding="utf-8")
    (root / "lib/algo/impl.cpp").write_text("int fx(){return 1;}\n", encoding="utf-8")

    extra_install = ("install(TARGETS %s\n  RUNTIME DESTINATION . COMPONENT acsd_runtime)\n"
                     % extra_exe) if (extra_exe and install_extra_exe) else ""
    (root / INSTALL_RULES).write_text(
        "install(TARGETS %s\n  RUNTIME DESTINATION . COMPONENT acsd_runtime)\n%s"
        % (built_exe, extra_install), encoding="utf-8")

    units = [{"unit_id": "PLATFORM-CLI", "kind": "exe", "install_path": built_exe,
              "required": True}]
    if extra_exe and contract_extra_exe:
        units.append({"unit_id": "PLATFORM-CLI-2", "kind": "exe",
                      "install_path": extra_exe, "required": True})
    (root / INSTALL_CONTRACT).write_text(
        json.dumps({"contract_id": "fx", "units": units}, ensure_ascii=False, indent=1),
        encoding="utf-8")

    (root / ENTRY_REGISTRY).write_text(
        json.dumps({"production_entry": entry, "entries": []}, ensure_ascii=False),
        encoding="utf-8")

    if csv_exes is None:
        csv_exes = [(entry, "production")]
        if extra_exe:
            csv_exes.append((extra_exe, "tool"))
    prod_names = None if csv_production is None else set(csv_production)
    lines = [INV_HEADER]
    for name, cls in csv_exes:
        if name in csv_drop:
            continue
        reach = "yes" if (cls == "production"
                          and (prod_names is None or name in prod_names)) else "no"
        anchor = "nope/CMakeLists.txt:1" if (csv_bad_anchor and name == entry) \
            else "CMakeLists.txt:1"
        lines.append("exe_target,%s,CMakeLists.txt,%s,%s,-,-,%s,"
                     % (name, cls, reach, anchor))
    (root / INVENTORY).write_text(chr(10).join(lines) + chr(10), encoding="utf-8")
    return root
