#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_build_graph.py —— 构建图门（CON-BUILD-GRAPH）。

整改依据（构建图一致性）
  原实现以「文档里是否出现某四个字符串」+「某个 CMakeLists 里是否有 add_library(phase2」
  为判据，故 docs/engineering/BUILD_GRAPH.md 的「生产构建图」三行目标名与路径**全部落空**
  （astro_image_io.dll / hepix_drizzle / orchestrator.exe，且未列 acsd、astrocs_phase2），
  门仍绿。现判据改为读**真实构建图**（eng/ci/cmake_graph.py，唯一实现）：
  文档只作说明，表由 eng/tools/arch/gen_build_graph_doc.py 从构建图导出。

判据（rc=2 = fail-closed；rc=1 = 对象不合规）
  C1 锚点：文档与三个机器块可读、生产入口登记表可读、构建图目标集非空、块内行数非零。
  C2 生产面双向差集：文档块内的目标集合 == 生产闭包（登记入口沿 target_link_libraries
     的传递闭包）；多写一个不在闭包里的目标、漏写一个生产目标 ⇒ 同一判据判红。
  C3 逐行三字段：kind、定义它的 CMakeLists、源集指纹（sha256 前 12 位、4-4-4 分组）
     必须与构建图一致（改错一个目标名或改一个源集即判红）。
  C4 非生产面（本门自身的豁免分支）：登记为非生产的目标必须真实存在且**不在**生产闭包内
     —— 豁免面不得被借道（把生产目标标成非生产即判红）。
  C5 非根图面：登记为「不由根图构建」的目标必须**不在**根构建图内（登记失真即判红）。

Self-test（--self-test）：判别力负例 + 保护性正例 + **落在自身豁免分支内**的负例（C4），
经 eng/ci/gate_trust.py 的三态口径输出（rc=0 PASS / 1 FAIL / 3 CRASH）。
"""
import argparse
import json
import pathlib
import sys

_CI = pathlib.Path(__file__).resolve().parents[3] / "ci"
sys.path.insert(0, str(_CI))
import cmake_graph as cg  # noqa: E402
import gate_common as gc  # noqa: E402
import gate_trust as gt  # noqa: E402

CHECK_ID = "CON-BUILD-GRAPH"
DOC = "docs/engineering/BUILD_GRAPH.md"
BLOCK_PROD = "BUILD-GRAPH-TABLE"
BLOCK_NONPROD = "BUILD-GRAPH-NONPROD"
BLOCK_NONROOT = "BUILD-GRAPH-NONROOT"


def _block_lines(text, marker):
    begin = "<!-- " + marker + ":BEGIN -->"
    end = "<!-- " + marker + ":END -->"
    if begin not in text or end not in text:
        raise gc.GateError("ANCHOR_MISSING: %s 缺机器块 %s" % (DOC, marker))
    body = text.split(begin, 1)[1].split(end, 1)[0]
    out = []
    for line in body.splitlines():
        line = line.strip()
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if not cells or cells[0] in ("target", ""):
            continue
        if set(cells[0]) <= set("-: "):   # 表头分隔行
            continue
        out.append(cells)
    return out


def read_doc_blocks(repo):
    text = gc.read_text(pathlib.Path(repo) / DOC, DOC)
    return {name: _block_lines(text, name)
            for name in (BLOCK_PROD, BLOCK_NONPROD, BLOCK_NONROOT)}


def evaluate(repo):
    """返回 findings（逐条带 文件:行 或对象名）。锚点不可用 ⇒ 抛 GateError。"""
    repo = pathlib.Path(repo)
    blocks = read_doc_blocks(repo)
    graph = cg.parse_cmake_graph(repo)
    entry = cg.production_entry(repo)
    closure = cg.production_closure(graph, entry)
    targets = graph["targets"]
    if not targets:
        raise gc.GateError("ANCHOR_EMPTY: 根构建图解析出 0 个 target（禁止空转判绿）")

    prod_rows = blocks[BLOCK_PROD]
    if not prod_rows:
        raise gc.GateError("ANCHOR_EMPTY: %s 的生产面机器块为 0 行（禁止空转判绿）" % DOC)

    findings = []
    declared = {}
    for cells in prod_rows:
        if len(cells) < 5:
            findings.append("C3 %s 生产面块行字段数 %d < 5"
                            "（target|kind|cmakelists|sources|src_fingerprint）：%s"
                            % (DOC, len(cells), cells))
            continue
        declared[cells[0]] = cells

    expected = set(closure)
    got = set(declared)
    for name in sorted(expected - got):
        findings.append("C2 %s 生产面块漏写生产目标 %r（在 %r 的生产闭包内，定义于 %s:%s）"
                        % (DOC, name, entry, targets[name]["file"],
                           "?"))
    for name in sorted(got - expected):
        where = ("定义于 %s" % targets[name]["file"]) if name in targets else "根构建图无此 target"
        findings.append("C2 %s 生产面块多写目标 %r（不在 %r 的生产闭包内；%s）"
                        % (DOC, name, entry, where))

    for name, cells in sorted(declared.items()):
        info = targets.get(name)
        if info is None:
            continue  # 已由 C2 判红
        if cells[1] != info["kind"]:
            findings.append("C3 %s 行 %r 的 kind=%r != 构建图 %r（定义于 %s）"
                            % (DOC, name, cells[1], info["kind"], info["file"]))
        if cells[2] != info["file"]:
            findings.append("C3 %s 行 %r 的 CMakeLists=%r != 构建图 %r"
                            % (DOC, name, cells[2], info["file"]))
        try:
            declared_count = int(cells[3])
        except ValueError:
            declared_count = -1
        if declared_count != len(info["sources"]):
            findings.append("C3 %s 行 %r 的源数=%r != 构建图 %d（源集已变，须重跑 "
                            "eng/tools/arch/gen_build_graph_doc.py）"
                            % (DOC, name, cells[3], len(info["sources"])))
        fingerprint = cg.source_fingerprint(info["sources"])
        if cells[4] != fingerprint:
            findings.append("C3 %s 行 %r 的源集指纹=%r != 构建图 %r（源集已变）"
                            % (DOC, name, cells[4], fingerprint))

    for cells in blocks[BLOCK_NONPROD]:
        if len(cells) < 2:
            findings.append("C4 %s 非生产面块行字段数 %d < 2（target|理由）：%s"
                            % (DOC, len(cells), cells))
            continue
        name = cells[0]
        if name not in targets:
            findings.append("C4 %s 非生产面块登记 %r 但根构建图无此 target（锚失效）"
                            % (DOC, name))
        elif name in closure:
            findings.append("C4 %s 非生产面块把 %r 登记为非生产，但它在 %r 的生产闭包内"
                            "（豁免面被借道）" % (DOC, name, entry))

    for cells in blocks[BLOCK_NONROOT]:
        if len(cells) < 3:
            findings.append("C5 %s 非根图块行字段数 %d < 3（target|cmakelists|理由）：%s"
                            % (DOC, len(cells), cells))
            continue
        name, cmakelists = cells[0], cells[1]
        if name in targets:
            findings.append("C5 %s 非根图块登记 %r 为「不由根图构建」，但它已在根构建图内"
                            % (DOC, name))
        if not (repo / cmakelists).is_file():
            findings.append("C5 %s 非根图块行 %r 的 CMakeLists %r 不存在（锚失效）"
                            % (DOC, name, cmakelists))
    return findings, {"entry": entry, "targets": len(targets),
                      "closure": len(closure), "doc_rows": len(prod_rows),
                      "nonprod_rows": len(blocks[BLOCK_NONPROD]),
                      "nonroot_rows": len(blocks[BLOCK_NONROOT])}


# ------------------------------------------------------------------- self-test ----
_FIXTURE_CMAKE = """cmake_minimum_required(VERSION 3.20)
project(fx LANGUAGES CXX)
add_library(fx_algo STATIC lib/algo/impl.cpp)
add_library(fx_tool STATIC lib/algo/tool.cpp)
add_executable(acsd app/main.cpp)
target_link_libraries(acsd PRIVATE fx_algo)
add_executable(fx_cli app/extra.cpp)
"""
# 负例**刻意**指向的不存在路径：它是「被测对象」（锚失效），不是夹具义务，
# 故不计入 _required_fixture_paths()（否则会把「期望缺失」当成「夹具缺失」）。
_ABSENT_CMAKE = "sub/cmake/__no_such_cmake__.txt"
_FIXTURE_ABSENT = frozenset((_ABSENT_CMAKE,))


def _write(root, rel, text):
    """夹具写一个文件：先建父目录，再写。

    为什么不是「先 mkdir 一份目录清单、再逐个 write_text」：那份清单是判据读面的
    **手写副本**。判据改一次指向（DOC 从迁移前的 docs/architecture/ 落到
    docs/engineering/，fef78f90）、清单没跟着改，夹具就写到不存在的目录上，
    2026-09-30 崩 1 = FileNotFoundError。让写文件自带 mkdir，这一类
    「清单与读面脱节」在结构上就发生不了。
    """
    p = pathlib.Path(root) / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")
    return p


def _anchor_dirs():
    """夹具要建的目录：**从判据自己的路径常量派生**，不手写第二份。

    evaluate() 的仓库内读面只有三个锚：DOC、cg.ENTRY_REGISTRY（生产入口登记表）、
    根 CMakeLists.txt。后两个与 DOC 的父目录都在这里派生 ⇒ 判据改指向，夹具自动跟着走。
    其余目录（app / lib/algo / sub/cmake）是夹具自己的源集与「非根图」证据，
    与判据读面无关，可以手写。
    """
    out = {"app", "lib/algo", "sub/cmake"}
    for rel in (DOC, cg.ENTRY_REGISTRY):
        parent = pathlib.PurePosixPath(rel).parent.as_posix()
        if parent not in (".", "", "/"):
            out.add(parent)
    return sorted(out)


def _fixture(root, *, doc_rows=None, nonprod_rows=(), nonroot_rows=()):
    root = pathlib.Path(root)
    for sub in _anchor_dirs():
        (root / sub).mkdir(parents=True, exist_ok=True)
    _write(root, "CMakeLists.txt", _FIXTURE_CMAKE)
    _write(root, "app/main.cpp", "int main(){return 0;}")
    _write(root, "app/extra.cpp", "int main(){return 0;}")
    _write(root, "lib/algo/impl.cpp", "int f(){return 1;}")
    _write(root, "lib/algo/tool.cpp", "int g(){return 1;}")
    # 非根图目标 legacy_cli 的定义处：根 CMakeLists 不 add_subdirectory(sub/cmake)，
    # 故它不在根构建图内 —— C5「非根图面」用例的锚（迁移后这项夹具面不再空挂）。
    _write(root, "sub/cmake/CMakeLists.txt", "add_executable(legacy_cli x.cpp)")
    _write(root, cg.ENTRY_REGISTRY,
           json.dumps({"production_entry": "acsd", "entries": []}))
    graph = cg.parse_cmake_graph(root)
    closure = cg.production_closure(graph, "acsd")
    if doc_rows is None:
        doc_rows = []
        for name in sorted(closure, key=lambda n: (n != "acsd", n)):
            info = graph["targets"][name]
            doc_rows.append([name, info["kind"], info["file"],
                             str(len(info["sources"])),
                             cg.source_fingerprint(info["sources"])])
    lines = ["<!-- BUILD-GRAPH-TABLE:BEGIN -->",
             "| target | kind | cmakelists | sources | digest |",
             "|---|---|---|---|---|"]
    lines += ["| " + " | ".join(r) + " |" for r in doc_rows]
    lines.append("<!-- BUILD-GRAPH-TABLE:END -->")
    lines += ["<!-- BUILD-GRAPH-NONPROD:BEGIN -->", "| target | 理由 |", "|---|---|"]
    lines += ["| " + " | ".join(r) + " |" for r in nonprod_rows]
    lines.append("<!-- BUILD-GRAPH-NONPROD:END -->")
    lines += ["<!-- BUILD-GRAPH-NONROOT:BEGIN -->",
              "| target | cmakelists | 理由 |", "|---|---|---|"]
    lines += ["| " + " | ".join(r) + " |" for r in nonroot_rows]
    lines.append("<!-- BUILD-GRAPH-NONROOT:END -->")
    _write(root, DOC, chr(10).join(lines) + chr(10))
    return root


# 用例表（**唯一一份**）：(name, kind, 夹具子目录, inject, want_red, probe)
_CASES = (
    ("pos_doc_matches_graph", gt.KIND_PROTECTIVE, "ok", {}, False, None),
    ("neg_doc_target_wrong_kind", gt.KIND_DISCRIMINATING, "kind",
     {"doc_rows": [["acsd", "add_library", "CMakeLists.txt", "1", "x"],
                   ["fx_algo", "add_library", "lib/algo/CMakeLists.txt", "1", "y"]]},
     True, lambda g: any(s.startswith("C3") for s in g)),
    ("neg_doc_missing_production_target", gt.KIND_DISCRIMINATING, "missing",
     {"doc_rows": [["acsd", "add_executable", "CMakeLists.txt", "2", "x"]]},
     True, lambda g: any(s.startswith("C2") for s in g)),
    ("neg_doc_extra_target", gt.KIND_DISCRIMINATING, "extra",
     {"doc_rows": [["acsd", "add_executable", "CMakeLists.txt", "2", "x"],
                   ["fx_tool", "add_library", "lib/algo/CMakeLists.txt", "1", "y"]]},
     True, lambda g: any(s.startswith("C2") for s in g)),
    ("neg_source_digest_drift", gt.KIND_DISCRIMINATING, "digest",
     {"doc_rows": [["acsd", "add_executable", "CMakeLists.txt", "2", "0"]]},
     True, lambda g: any(s.startswith("C3") for s in g)),
    ("exemption_neg_production_marked_nonprod", gt.KIND_EXEMPTION, "nonprod_leak",
     {"nonprod_rows": [["fx_algo", "宣称非生产"]]},
     True, lambda g: any(s.startswith("C4") for s in g)),
    ("exemption_pos_real_nonprod_declared", gt.KIND_PROTECTIVE, "nonprod_ok",
     {"nonprod_rows": [["fx_tool", "非生产静态库"]]}, False, None),
    # 非根图面两条：C5 两个分支原本一条没测（负例声明的 CMakeLists 夹具根本没建，
    # 而 sub/cmake 这张夹具面自建立起就空挂，无人引用）。
    ("pos_nonroot_real_nonroot_target", gt.KIND_PROTECTIVE, "nonroot_ok",
     {"nonroot_rows": [["legacy_cli", "sub/cmake/CMakeLists.txt", "不由根图构建"]]},
     False, None),
    ("neg_nonroot_claims_target_already_in_graph", gt.KIND_DISCRIMINATING, "nonroot",
     {"nonroot_rows": [["fx_tool", "CMakeLists.txt", "宣称不由根图构建"]]},
     True, lambda g: any(s.startswith("C5") for s in g)),
    ("neg_nonroot_cmakelists_anchor_missing", gt.KIND_DISCRIMINATING, "nonroot_anchor",
     {"nonroot_rows": [["legacy_cli", _ABSENT_CMAKE, "指向不存在的 CMakeLists"]]},
     True, lambda g: any(s.startswith("C5") for s in g)),
)


def _required_fixture_paths():
    """夹具**必须**提供的文件 = 判据要读的锚 + 用例自己声明的 CMakeLists 锚。

    目录清单随 DOC 走，这份清单也随 DOC 与 cg.ENTRY_REGISTRY 走 —— 两者同源，
    夹具因此不可能再与判据读面脱节（崩 1 的形态）。用例声明的 CMakeLists 也计入：
    负例指向一个夹具根本没建的文件时，本函数会让 fixture_layout_complete 判红，
    逼用例改成自己真正想测的那一条（而不是顺手测到「锚失效」）。
    """
    req = {DOC, cg.ENTRY_REGISTRY, "CMakeLists.txt",
           "app/main.cpp", "app/extra.cpp",
           "lib/algo/impl.cpp", "lib/algo/tool.cpp",
           "sub/cmake/CMakeLists.txt"}
    for _name, _kind, _sub, inject, _red, _probe in _CASES:
        for row in inject.get("nonroot_rows") or ():
            if len(row) >= 2 and row[1] not in _FIXTURE_ABSENT:
                req.add(row[1])
    return sorted(req)


def _layout_case(root):
    """夹具布局完整性：判据要读的锚一个都不许缺。

    夹具与判据脱节时本条**判红**并指名缺哪个（KIND_PROTECTIVE ⇒ 汇总为 rc=3 CRASH，
    「门自身不可信」），而不是让后续用例崩出 traceback。
    """
    missing = [rel for rel in _required_fixture_paths()
               if not (pathlib.Path(root) / rel).is_file()]
    return gt.Case("fixture_layout_complete", not missing, gt.KIND_PROTECTIVE,
                   "missing=%r" % (missing,))


def _selftest():
    import tempfile
    cases = []

    def run(repo):
        try:
            findings, _extra = evaluate(repo)
        except gc.GateError as exc:
            return "GateError: %s" % exc
        return findings

    def case(name, kind, root, *, inject, want_red, probe=None):
        _fixture(root, **inject)
        got = run(root)
        is_red = bool(got)
        ok = (is_red == want_red) and (probe is None or probe(got))
        cases.append(gt.Case(name, ok, kind,
                             "want_red=%s got=%r" % (want_red,
                                                     got if isinstance(got, str) else got[:4])))

    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        # 夹具布局体检先跑：它回答「夹具还对不对得上判据」，脱节时立刻判红并指名缺哪条。
        _fixture(base / "layout")
        cases.append(_layout_case(base / "layout"))
        for name, kind, sub, inject, want_red, probe in _CASES:
            case(name, kind, base / sub, inject=inject, want_red=want_red, probe=probe)
        # 去掉机器块 ⇒ 必须 fail-closed 判红（不是判绿）
        _fixture(base / "noblock")
        _write(base / "noblock", DOC, "# build graph" + chr(10))
        got = run(base / "noblock")
        cases.append(gt.Case("failclosed_doc_block_missing",
                             bool(got) and str(got).startswith("GateError"),
                             gt.KIND_DISCRIMINATING, "got=%r" % (got,)))
    return gt.emit(cases, tool=CHECK_ID)


def main(argv=None):
    ap = argparse.ArgumentParser(description=CHECK_ID + " 构建图门（读真实构建图）")
    ap.add_argument("--repo", default=str(gc.repo_root()))
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return _selftest()

    repo = pathlib.Path(args.repo).resolve()
    try:
        findings, extra = evaluate(repo)
    except gc.GateError as exc:
        print(json.dumps({"tool": "check_build_graph", "status": "FAIL", "passed": False,
                          "findings": [{"id": "ANCHOR", "detail": str(exc)}]},
                         ensure_ascii=False))
        print("%s_FAIL: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2

    status = "PASS" if not findings else "FAIL"
    result = {"tool": "check_build_graph", "status": status, "passed": status == "PASS",
              "findings": findings, "summary": extra}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(
            json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_junit).write_text(
            '<testsuite name="check_build_graph" tests="1" failures="%d">'
            '<testcase classname="build" name="graph"/></testsuite>' % len(findings),
            encoding="utf-8")
    if findings:
        gc.print_findings(CHECK_ID, findings, limit=len(findings))
    print(json.dumps(result, ensure_ascii=False))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
