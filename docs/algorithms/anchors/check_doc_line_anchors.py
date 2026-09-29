#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-ANCHOR-001｜文档源码行号锚全量复测器（exit 0 = PASS）。

任务：冻结文档行号锚（作用域 = anchor_contract.json.doc_globs，现为 docs/**/*.md）
全量复测——锚指向的符号/行为与文档一致；漂移锚更新行号并保持语义不变。

规则（详见 docs/detail/anchors/ANCHOR_CONTRACT.md）：
  C1 docs_tracked      作用域文档存在且被 Git 跟踪
  C2 anchor_resolved   每个锚解析到唯一真实目标文件（exact → doc-relative →
                       contract.resolvers → unique basename）；无法解析即 FAIL
                       （除非在 contract.exemptions 或 unresolved_registry 中登记）
  C3 range_in_bounds   1 <= start <= end <= 目标文件行数
  C4 symbol_binding    contract.bindings 声明的 (doc, target, symbol)：该 doc 内
                       必须存在一个指向 target 的锚，其行范围内逐字包含 symbol。
                       符号整体不存在于 target → STALE_BINDING；
                       存在但已不在任何锚范围内 → BINDING_VIOLATION（即漂移锚）
  C5 exemptions_live   exemption 必须命中至少一个真实锚，否则 STALE_EXEMPTION
  C6 boundary_blank    锚的边界行（start 与 end）不得为空行 —— 行号锚的语义是
                       「所描述的符号/行为位于该行（区间）」，边界落在空行即等于
                       锚没指向任何内容（DOC-DRIFT-FIX-01 新增）。口径 = read_lines
                       的空行定义，即 line == 空串；**不用 strip/rstrip**：
                       纯空白行不是空行，按 rstrip 判空会引入误报。
  C7 contract_scale    ANCHOR_CONTRACT.md §1 的「现行规模」行必须与本门实测逐项相等
                       （文档数 / 锚总数 / 目标锚 / 登记豁免 / 未解析登记）；
                       该行缺失或不可解析 ⇒ 判红（fail-closed，禁止删声明躲门）。
                       口径即本门扫描器；--print-scale 输出可直接粘贴的现行行。
  C8 unresolved_registry  未解析锚必须逐条登记在 unresolved_registry.json
                       （kind/reason/owner/handoff），且**只减不增**：
                       新出现的未解析锚 ⇒ C2+C8 判红；登记项不再命中 ⇒
                       STALE_REGISTRY 判红；条目数 > max_entries ⇒ 棘轮判红。

输出为稳定排序 JSON（无时间戳），跨 cwd/复跑 bitwise 一致。

用法：
  python3 docs/algorithms/anchors/check_doc_line_anchors.py [--root .] [--json-out F]
  python3 docs/algorithms/anchors/check_doc_line_anchors.py --print-scale
  python3 docs/algorithms/anchors/check_doc_line_anchors.py --print-registry
  python3 docs/algorithms/anchors/check_doc_line_anchors.py --self-test

锚口径（DOC-DRIFT-FIX-01 复核补正）：一行内的**每个**锚 token 单独成锚并单独受判。
path:N,M 与 path:N/L 形态里的 M / L 是**独立锚**，不是首锚的附属——它们同样要过
C2 解析、C3 界内、C4 绑定、C6 边界空行。历史实现把续锚吞进首锚的匹配区间（既不判
也不计数），本门已订正为逐条展开；--self-test 的负例 C 专测这一面。
"""
from __future__ import annotations

import argparse
import glob as _glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

CONTRACT_REL = "docs/algorithms/anchors/anchor_contract.json"
REGISTRY_REL = "docs/algorithms/anchors/unresolved_registry.json"
CONTRACT_DOC_REL = "docs/detail/anchors/ANCHOR_CONTRACT.md"
EXTS = ("cpp", "cc", "cxx", "h", "hpp", "hh", "py", "sh", "ps1", "txt",
        "json", "yaml", "yml", "md", "cmake", "in")
_FILE = (r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*"
         r"[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:" + "|".join(EXTS) + r"))")
_ONE = r"[:#]L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?"
CONT_RE = re.compile(r"\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?")
# ANCHOR_RE 只吃「文件名 + 首个行号段」；同一 token 里 ,N 与 / :N 形态的**续锚**
# 由 expand_anchors 用 CONT_RE 循环逐个展开成独立锚记录（契约 §1 的锚形态逐个受判）。
# 缺陷留痕（DOC-LINE-ANCHORS 复核）：ANCHOR_RE 曾把与 CONT_RE 同形的贪婪重复组拼在
# 末尾，group(0)/m.end() 直接越过全部续锚 ⇒ 下面的 while 循环成死代码，一行里的
# 第 2..n 个锚被**静默吞掉**：不解析（C2）、不判界内（C3）、不判边界空行（C6）、
# 不参与符号绑定（C4）——实测 287 个锚长期无门（见 ANCHOR_CONTRACT.md §7）。
ANCHOR_RE = re.compile(_FILE + _ONE)
# C7 规模声明的**规范句式**（ANCHOR_CONTRACT.md §1 必须逐字如此；改锚必须同步本行）
SCALE_RE = re.compile(
    r"现行规模（C7 逐字复测）：\*\*(\d+) 文档 / (\d+) 锚\*\* = "
    r"(\d+) 目标锚 \+ (\d+) 登记豁免 \+ (\d+) 未解析登记。")
# C7 规范句式的成句模板（--self-test 的 fixture 与正例回读都用它；不得另立句式）
SCALE_LINE = ("现行规模（C7 逐字复测）：**%d 文档 / %d 锚** = "
              "%d 目标锚 + %d 登记豁免 + %d 未解析登记。")

ARCHIVE_MARKERS = ("/archive/", "/legacy/", "/.git/", "/third_party/",
                   "/build/", "/out/", "/run/", "/worktrees/")


def fail(code, detail):
    return {"severity": "ERROR", "code": code, "detail": detail}


def load_json(path):
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def git_ls(root):
    r = subprocess.run(["git", "ls-files"], cwd=root, capture_output=True, text=True)
    if r.returncode != 0:
        return None
    return [x for x in r.stdout.splitlines() if x.strip()]


def read_lines(root, rel):
    """行口径：只去掉**一个**尾部换行，再按换行切分。

    空行 == 空字符串。**不要**用 strip()/rstrip() 判空：纯空白行（缩进行）不是空行，
    按 rstrip 判空会引入误报（DOC-DRIFT-FIX-01 实测 docs/** 为 0 例，但口径必须写死）。
    """
    with open(os.path.join(root, rel), "rb") as fh:
        raw = fh.read()
    text = raw.decode("utf-8", errors="replace")
    if text.endswith("\n"):
        text = text[:-1]
    return text.split("\n")


def count_lines(root, rel):
    try:
        return len(read_lines(root, rel))
    except OSError:
        return -1


def expand_anchors(line):
    """Yield (raw, base, start, end) for every anchor occurrence on one doc line.

    一行里可以有多个锚：首个锚 = 文件名 + 首个行号段；其后每个 ,N / / :N 形态的
    续锚各**单独**产出一条记录（start/end 取该续锚自己的数字，不是首锚的），
    这样续锚与首锚一样受 C2/C3/C4/C6 全判。续锚记录的 raw = 该行从首锚开头到
    本续锚结尾的**原文切片**（与豁免/登记台账的 (doc, raw) 键口径一致）。
    """
    out = []
    for m in ANCHOR_RE.finditer(line):
        base = m.group(1)
        start = int(m.group(2))
        end = int(m.group(3)) if m.group(3) else start
        out.append((m.group(0), base, start, end))
        pos = m.end()
        while True:
            cm = CONT_RE.match(line, pos)
            if not cm:
                break
            s2 = int(cm.group(1))
            e2 = int(cm.group(2)) if cm.group(2) else s2
            out.append((line[m.start():cm.end()], base, s2, e2))
            pos = cm.end()
    return out


def build_basename_index(root):
    idx = {}
    for dirpath, dirnames, filenames in os.walk(root):
        d = dirpath.replace(os.sep, "/") + "/"
        dirnames[:] = [x for x in dirnames if x != ".git"]
        if any(s in d for s in ARCHIVE_MARKERS):
            dirnames[:] = []
            continue
        for fn in filenames:
            rel = os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/")
            idx.setdefault(fn, []).append(rel)
    for k in idx:
        idx[k].sort()
    return idx


def resolve(root, doc, base, resolvers, basename_index):
    """Return (relpath, how) or (None, reason)."""
    direct = os.path.normpath(base).replace(os.sep, "/")
    if not direct.startswith("../") and os.path.isfile(os.path.join(root, direct)):
        return direct, "exact"
    rel = os.path.normpath(os.path.join(os.path.dirname(doc), base)).replace(os.sep, "/")
    if not rel.startswith("../") and os.path.isfile(os.path.join(root, rel)):
        return rel, "doc-relative"
    bname = os.path.basename(base)
    for rule in resolvers:
        if rule.get("doc") and rule["doc"] != doc:
            continue
        if rule.get("doc_prefix") and not doc.startswith(rule["doc_prefix"]):
            continue
        if rule.get("basename") != bname:
            continue
        return rule["path"], "contract-rule"
    cands = basename_index.get(bname, [])
    if len(cands) == 1:
        return cands[0], "basename-unique"
    if not cands:
        return None, "no-such-file"
    return None, "ambiguous:" + ",".join(cands[:6])


def scale_claim(root):
    """读 ANCHOR_CONTRACT.md §1 的规范规模句；返回 (nums, raw_line) 或 (None, reason)。"""
    path = os.path.join(root, CONTRACT_DOC_REL)
    if not os.path.isfile(path):
        return None, "missing " + CONTRACT_DOC_REL
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            if "现行规模" in line:
                m = SCALE_RE.search(line)
                if not m:
                    return None, "unparseable: " + line.strip()[:120]
                return [int(x) for x in m.groups()], line.strip()
    return None, "no 现行规模 line in " + CONTRACT_DOC_REL


def emit(errors, anchors, counters, json_out, scale_line=None, registry_rows=None):
    anchors = anchors or []
    by_status = {}
    for a in anchors:
        by_status[a["status"]] = by_status.get(a["status"], 0) + 1
    by_code = {}
    for e in errors:
        by_code[e["code"]] = by_code.get(e["code"], 0) + 1
    doc_count = len({a["doc"] for a in anchors})
    # W4-A3（C 类收口）：豁免面必须**逐条点名**。原实现只在汇总行打印
    # "status=EXEMPT:8"，读日志的人无法知道被豁免的到底是哪 8 个对象、依据什么
    # 类型/理由豁免 —— 打印结论与判定面不自洽（审计面缺口）。现落进 JSON 的
    # exemptions 数组并在 stdout 逐条打印；豁免计数与逐条列表同源，不可能不匹配。
    exempt_list = sorted(
        ({"doc": a["doc"], "doc_line": a["doc_line"], "raw": a["raw"],
          "kind": a.get("kind", ""), "reason": a.get("reason", ""),
          "owner": a.get("owner", "")}
         for a in anchors if a["status"] == "EXEMPT"),
        key=lambda e: (e["doc"], e["doc_line"], e["raw"]))
    # C8 同口径：未解析登记同样逐条点名（计数与列表同源）。
    unreg_list = sorted(
        ({"doc": a["doc"], "doc_line": a["doc_line"], "raw": a["raw"],
          "kind": a.get("kind", ""), "reason": a.get("reason", ""),
          "owner": a.get("owner", "")}
         for a in anchors if a["status"] == "REGISTERED_UNRESOLVED"),
        key=lambda e: (e["doc"], e["doc_line"], e["raw"]))
    report = {
        "schema": "astrocs/doc-line-anchors/v2",
        "task": "SCI-ANCHOR-001 / DOC-DRIFT-FIX-01",
        "verdict": "PASS" if not errors else "FAIL",
        "documents": doc_count,
        "anchors_total": len(anchors),
        "by_status": dict(sorted(by_status.items())),
        "by_code": dict(sorted(by_code.items())),
        "counters": counters,
        "scale_claim": scale_line,
        "unresolved_registry": registry_rows or [],
        "exemptions": exempt_list,
        "errors": errors,
        "anchors": sorted(anchors, key=lambda a: (a["doc"], a["doc_line"], a["start"], a["raw"])),
    }
    if json_out:
        parent = os.path.dirname(os.path.abspath(json_out))
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(json_out, "w", encoding="utf-8") as fh:
            json.dump(report, fh, ensure_ascii=False, indent=1, sort_keys=True)
            fh.write("\n")
    if errors:
        print("DOC_LINE_ANCHORS_FAIL: %d findings (by_code=%s)"
              % (len(errors), ",".join("%s:%d" % kv for kv in sorted(by_code.items()))))
        for e in errors:
            print("  [%s] %s" % (e["code"], e["detail"]))
        return 1
    print("DOC_LINE_ANCHORS_PASS: %d docs, %d anchors, status=%s"
          % (doc_count, len(anchors),
             ",".join("%s:%d" % kv for kv in sorted(by_status.items()))))
    print("  scale: %s" % (scale_line or "-"))
    # 豁免对象逐条点名（W4-A3）：EXEMPT 计数 == 逐条列表长度；空则不打印。
    if exempt_list:
        print("  EXEMPT_OBJECTS (%d) —— 逐条点名，禁止只报计数:" % len(exempt_list))
        for e in exempt_list:
            print("    %s:%d %s [%s] owner=%s"
                  % (e["doc"], e["doc_line"], e["raw"], e["kind"] or "-",
                     e["owner"] or "-"))
            print("      理由: %s" % (e["reason"] or "-"))
    if unreg_list:
        print("  REGISTERED_UNRESOLVED (%d) —— 未解析但已登记（只减不增）:"
              % len(unreg_list))
        for e in unreg_list:
            print("    %s:%d %s [%s] owner=%s"
                  % (e["doc"], e["doc_line"], e["raw"], e["kind"] or "-",
                     e["owner"] or "-"))
    return 0


def _st_write(path, text):
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def _st_run(root):
    """在 fixture 上跑本检查器本体（子进程），返回 (rc, stdout+stderr)。"""
    r = subprocess.run([sys.executable, os.path.abspath(__file__), "--root", root],
                       capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


ST_TARGET = "lib/sample/sample_impl.cpp"
ST_DOC = "docs/science/algorithms/SAMPLE.md"
ST_TARGET_BODY = [
    "// synthetic source for DOC-LINE-ANCHORS self-test",
    "int sample_kernel(int x) {",
    "    return x + 1;",
    "}",
    "",
    "int sample_helper(int x) {",
    "    return x - 1;",
    "}",
]


def _st_make_fixture(tmp, anchor):
    """最小 git fixture：1 文档 / 1 行锚（2 个锚 token）/ 1 目标文件；C7 行按实测写死。"""
    _st_write(os.path.join(tmp, CONTRACT_REL), json.dumps(
        {"schema": "astrocs/doc-line-anchor-contract/v1",
         "doc_globs": ["docs/**/*.md"], "resolvers": [], "exemptions": [],
         "bindings": []}, ensure_ascii=False))
    _st_write(os.path.join(tmp, REGISTRY_REL), json.dumps(
        {"schema": "astrocs/doc-anchor-unresolved-registry/v1",
         "max_entries": 0, "entries": []}, ensure_ascii=False))
    _st_write(os.path.join(tmp, CONTRACT_DOC_REL),
              "# SYNTH ANCHOR CONTRACT\n\n" + SCALE_LINE % (1, 2, 2, 0, 0) + "\n")
    _st_write(os.path.join(tmp, ST_DOC),
              "# synthetic ALG doc\n\nanchors: " + chr(96) + anchor + chr(96) + "\n")
    _st_write(os.path.join(tmp, ST_TARGET), "\n".join(ST_TARGET_BODY) + "\n")
    env = dict(os.environ)
    env.update({"GIT_AUTHOR_NAME": "selftest", "GIT_AUTHOR_EMAIL": "selftest@local",
                "GIT_COMMITTER_NAME": "selftest", "GIT_COMMITTER_EMAIL": "selftest@local"})
    for cmd in (["git", "init", "-q"], ["git", "add", "-A"],
                ["git", "commit", "-q", "-m", "fixture"]):
        subprocess.run(cmd, cwd=tmp, check=True, capture_output=True, env=env)


def self_test():
    """判据自检：正例判绿 + 负例判红 + 还原回绿（证明失败由注入引起）。

    覆盖本门 2026-09-29 订正的枚举缺陷：同一 token 里的续锚（,N 与 / :N-M 形态）
    此前被 ANCHOR_RE 的贪婪重复组吞掉、完全不判；本自检的负例 C 专测该面——
    注入一个越界的**续锚**必须判红（旧实现下会漏判成绿）。
    """
    steps = []

    def check(name, cond, extra=""):
        steps.append((name, bool(cond), extra))
        print("  [%s] %s%s" % ("OK" if cond else "FAIL", name, (" | " + extra) if extra else ""))
        return bool(cond)

    # 1) 枚举口径（不依赖子进程）：一行三锚，续锚必须各成一条记录
    unit = expand_anchors("x " + chr(96) + "lib/sample/sample_impl.cpp:2-3 / :6 / :7-8" + chr(96))
    check("expand_anchors 逐条展开续锚（不含首锚共 3 条）: %r" % (unit,),
          [t[2:] for t in unit] == [(2, 3), (6, 6), (7, 8)] and len(unit) == 3)

    m_scale = SCALE_RE.search(SCALE_LINE % (1, 2, 2, 0, 0))
    check("C7 规范句式模板可被 SCALE_RE 逐字回读",
          m_scale is not None and [int(x) for x in m_scale.groups()] == [1, 2, 2, 0, 0])

    tmp = tempfile.mkdtemp(prefix="doc_line_anchors_selftest_")
    try:
        _st_make_fixture(tmp, "lib/sample/sample_impl.cpp:1-2 / :6-7")
        rc, out = _st_run(tmp)
        check("正例（首锚 + 续锚均在界内且边界非空）rc=0", rc == 0 and "DOC_LINE_ANCHORS_PASS" in out,
              "rc=%d" % rc)
        for name, bad, code in (
                ("负例A 续锚越界（:6-7 -> :6-9）", "lib/sample/sample_impl.cpp:1-2 / :6-9", "C3_range_in_bounds"),
                ("负例B 续锚边界落空行（:6-7 -> :5-6）", "lib/sample/sample_impl.cpp:1-2 / :5-6", "C6_boundary_blank"),
                ("负例C 续锚被吞的回归守卫（:6-7 -> :99）", "lib/sample/sample_impl.cpp:1-2 / :99", "C3_range_in_bounds")):
            _st_make_fixture(tmp, bad)
            rc, out = _st_run(tmp)
            check("%s -> 判红" % name, rc != 0 and code in out, "rc=%d/%s" % (rc, code))
        _st_make_fixture(tmp, "lib/sample/sample_impl.cpp:1-2 / :6-7")
        rc, out = _st_run(tmp)
        check("还原后回绿（失败由注入引起）", rc == 0 and "DOC_LINE_ANCHORS_PASS" in out, "rc=%d" % rc)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    bad_steps = [s for s in steps if not s[1]]
    print("SELF_TEST_%s: %d/%d" % ("PASS" if not bad_steps else "FAIL",
                                   len(steps) - len(bad_steps), len(steps)))
    return 0 if not bad_steps else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="ALG/SCI doc line-anchor re-test")
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--print-scale", action="store_true",
                    help="只打印 ANCHOR_CONTRACT.md §1 应逐字写成的现行规模行")
    ap.add_argument("--print-registry", action="store_true",
                    help="只打印 unresolved_registry.json 应登记的未解析锚清单")
    ap.add_argument("--self-test", action="store_true",
                    help="判据自检（正例判绿 + 负例判红 + 还原回绿），不扫描本仓")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)

    errors = []
    contract_path = os.path.join(root, CONTRACT_REL)
    if not os.path.isfile(contract_path):
        errors.append(fail("C0_contract_exists", CONTRACT_REL))
        return emit(errors, None, {}, args.json_out)
    contract = load_json(contract_path)
    resolvers = contract.get("resolvers", [])
    exemptions = contract.get("exemptions", [])
    bindings = contract.get("bindings", [])

    registry_path = os.path.join(root, REGISTRY_REL)
    if not os.path.isfile(registry_path):
        errors.append(fail("C0_contract_exists", REGISTRY_REL))
        return emit(errors, None, {}, args.json_out)
    registry_doc = load_json(registry_path)
    registry = registry_doc.get("entries", [])
    registry_keys = {(e["doc"], e["raw"]) for e in registry}
    registry_meta = {(e["doc"], e["raw"]): e for e in registry}

    tracked = git_ls(root)
    tracked_set = set(tracked) if tracked is not None else None
    if tracked_set is None:
        errors.append(fail("C1_docs_tracked",
                           "git ls-files 不可用，无法判定锚目标是否 tracked（fail-closed）"))
    basename_index = build_basename_index(root)

    docs = []
    for pattern in contract.get("doc_globs", []):
        for p in sorted(_glob.glob(os.path.join(root, pattern), recursive=True)):
            docs.append(os.path.relpath(p, root).replace(os.sep, "/"))
    docs = sorted(set(docs))
    if not docs:
        errors.append(fail("C1_docs_tracked",
                           "0 篇文档命中 doc_globs=%s（扫描面塌缩，fail-closed）"
                           % contract.get("doc_globs")))

    for d in docs:
        if not os.path.isfile(os.path.join(root, d)):
            errors.append(fail("C1_docs_tracked", "missing doc: " + d))
        elif tracked_set is not None and d not in tracked_set:
            errors.append(fail("C1_docs_tracked", "untracked doc: " + d))

    exempt_keys = {(ex["doc"], ex["raw"]) for ex in exemptions}
    # W4-A3：豁免元数据（kind/reason/owner）随锚记录落盘 —— 逐条点名要能说清
    # "凭什么豁免"，只报一个 EXEMPT 计数等于没有审计面。
    exempt_meta = {(ex["doc"], ex["raw"]): ex for ex in exemptions}
    matched_exemptions = set()
    matched_registry = set()

    anchors = []
    for doc in docs:
        lines = read_lines(root, doc)
        for i, line in enumerate(lines, 1):
            for raw, base, start, end in expand_anchors(line):
                rec = {"doc": doc, "doc_line": i, "raw": raw, "base": base,
                       "start": start, "end": end}
                if (doc, raw) in exempt_keys:
                    matched_exemptions.add((doc, raw))
                    rec["resolved"] = None
                    rec["status"] = "EXEMPT"
                    _ex = exempt_meta.get((doc, raw), {})
                    rec["kind"] = _ex.get("kind", "")
                    rec["reason"] = _ex.get("reason", "")
                    rec["owner"] = _ex.get("owner", "")
                    rec["exemption_evidence"] = _ex.get("evidence", "")
                    anchors.append(rec)
                    continue
                if (doc, raw) in registry_keys:
                    matched_registry.add((doc, raw))
                    rec["resolved"] = None
                    rec["status"] = "REGISTERED_UNRESOLVED"
                    _rg = registry_meta.get((doc, raw), {})
                    rec["kind"] = _rg.get("kind", "")
                    rec["reason"] = _rg.get("reason", "")
                    rec["owner"] = _rg.get("owner", "")
                    rec["handoff"] = _rg.get("handoff", "")
                    anchors.append(rec)
                    continue
                resolved, how = resolve(root, doc, base, resolvers, basename_index)
                rec["how"] = how
                if resolved is None:
                    rec["status"] = "UNRESOLVED"
                    errors.append(fail("C2_anchor_resolved",
                                       "%s:%d %s (%s) —— 未解析且未登记在 %s"
                                       % (doc, i, raw, how, REGISTRY_REL)))
                    anchors.append(rec)
                    continue
                rec["resolved"] = resolved
                if tracked_set is not None and resolved not in tracked_set:
                    rec["status"] = "UNTRACKED"
                    errors.append(fail("C2_anchor_resolved",
                                       "%s:%d %s -> untracked %s" % (doc, i, raw, resolved)))
                    anchors.append(rec)
                    continue
                body = read_lines(root, resolved)
                n = len(body)
                rec["target_lines"] = n
                if start < 1 or end < start or end > n:
                    rec["status"] = "OUT_OF_BOUNDS"
                    errors.append(fail("C3_range_in_bounds",
                                       "%s:%d %s -> %s has %d lines"
                                       % (doc, i, raw, resolved, n)))
                else:
                    # C6：边界行不得为空行（口径 = read_lines 的空行，即 line == 空串）
                    blank = []
                    if body[start - 1] == "":
                        blank.append("start=%d" % start)
                    if end != start and body[end - 1] == "":
                        blank.append("end=%d" % end)
                    if blank:
                        rec["status"] = "BOUNDARY_BLANK"
                        rec["blank"] = blank
                        errors.append(fail("C6_boundary_blank",
                                           "%s:%d %s -> %s:%d-%d 边界为空行（%s）"
                                           % (doc, i, raw, resolved, start, end,
                                              "，".join(blank))))
                    else:
                        rec["status"] = "OK"
                anchors.append(rec)

    for doc, raw in sorted(exempt_keys):
        if (doc, raw) not in matched_exemptions:
            errors.append(fail("C5_exemptions_live", "stale exemption: %s %s" % (doc, raw)))

    # C8：未解析登记只减不增
    for doc, raw in sorted(registry_keys):
        if (doc, raw) not in matched_registry:
            errors.append(fail("C8_registry_stale",
                               "登记项已不再命中（锚已消失或已可解析），必须删除: %s %s"
                               % (doc, raw)))
    max_entries = registry_doc.get("max_entries")
    if isinstance(max_entries, int) and len(registry) > max_entries:
        errors.append(fail("C8_registry_ratchet",
                           "unresolved_registry 条目 %d > max_entries %d（只减不增）"
                           % (len(registry), max_entries)))

    for b in bindings:
        doc = b["doc"]
        target = b["target"]
        sym = b["symbol"]
        tid = b.get("id", "%s|%s|%s" % (doc, target, sym))
        tpath = os.path.join(root, target)
        if not os.path.isfile(tpath):
            errors.append(fail("C4_symbol_binding", "%s: target missing %s" % (tid, target)))
            continue
        tlines = read_lines(root, target)
        hits = [i for i, l in enumerate(tlines, 1) if sym in l]
        if not hits:
            errors.append(fail("C4_symbol_binding", "%s: STALE_BINDING symbol absent" % tid))
            continue
        ok = False
        for a in anchors:
            if a["doc"] != doc or a.get("resolved") != target or a["status"] != "OK":
                continue
            body = "\n".join(tlines[a["start"] - 1:a["end"]])
            if sym in body:
                ok = True
                break
        if not ok:
            errors.append(fail("C4_symbol_binding",
                               "%s: BINDING_VIOLATION symbol now at lines %s "
                               "(outside every anchor range)" % (tid, hits[:6])))

    # ---- 实测口径（C7 的右值来源；与上面逐锚扫描同源，不另立扫描器）
    by_status = {}
    for a in anchors:
        by_status[a["status"]] = by_status.get(a["status"], 0) + 1
    counters = {
        "documents": len({a["doc"] for a in anchors}),
        "anchors_total": len(anchors),
        "targets": sum(v for k, v in by_status.items()
                       if k in ("OK", "OUT_OF_BOUNDS", "BOUNDARY_BLANK")),
        "exempt": by_status.get("EXEMPT", 0),
        "registered_unresolved": by_status.get("REGISTERED_UNRESOLVED", 0),
    }

    if args.print_scale:
        print("现行规模（C7 逐字复测）：**%d 文档 / %d 锚** = %d 目标锚 + %d 登记豁免 + %d 未解析登记。"
              % (counters["documents"], counters["anchors_total"], counters["targets"],
                 counters["exempt"], counters["registered_unresolved"]))
        return 0
    if args.print_registry:
        rows = sorted(
            ({"doc": a["doc"], "doc_line": a["doc_line"], "raw": a["raw"]}
             for a in anchors if a["status"] == "UNRESOLVED"),
            key=lambda e: (e["doc"], e["doc_line"], e["raw"]))
        print(json.dumps(rows, ensure_ascii=False, indent=1))
        return 0

    # C7：合同 §1 的规模声明必须与实测逐项相等（不可解析 = 判红）
    nums, raw = scale_claim(root)
    if nums is None:
        errors.append(fail("C7_contract_scale", "ANCHOR_CONTRACT.md §1 规模声明不可用: " + raw))
        scale_line = None
    else:
        want = [counters["documents"], counters["anchors_total"], counters["targets"],
                counters["exempt"], counters["registered_unresolved"]]
        scale_line = raw
        if nums != want:
            errors.append(fail("C7_contract_scale",
                               "§1 声明 %s ≠ 实测 %s（文档/锚/目标锚/登记豁免/未解析登记）；"
                               "用 --print-scale 取现行行" % (nums, want)))

    registry_rows = sorted(
        ({"doc": a["doc"], "doc_line": a["doc_line"], "raw": a["raw"],
          "kind": a.get("kind", ""), "owner": a.get("owner", "")}
         for a in anchors if a["status"] == "REGISTERED_UNRESOLVED"),
        key=lambda e: (e["doc"], e["doc_line"], e["raw"]))
    errors.sort(key=lambda e: (e["code"], e["detail"]))
    return emit(errors, anchors, counters, args.json_out, scale_line, registry_rows)


if __name__ == "__main__":
    sys.exit(main())
