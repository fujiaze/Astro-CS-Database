#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-ANCHOR-001｜文档源码行号锚全量复测器（exit 0 = PASS）。

任务：冻结文档行号锚（作用域 = anchor_contract.json.doc_globs，现为 docs/**/*.md）
全量复测——锚指向的符号/行为与文档一致；漂移锚更新行号并保持语义不变。

规则（详见 docs/algorithms/anchors/ANCHOR_CONTRACT.md）：
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
"""
from __future__ import annotations

import argparse
import glob as _glob
import json
import os
import re
import subprocess
import sys

CONTRACT_REL = "docs/algorithms/anchors/anchor_contract.json"
REGISTRY_REL = "docs/algorithms/anchors/unresolved_registry.json"
CONTRACT_DOC_REL = "docs/algorithms/anchors/ANCHOR_CONTRACT.md"
EXTS = ("cpp", "cc", "cxx", "h", "hpp", "hh", "py", "sh", "ps1", "txt",
        "json", "yaml", "yml", "md", "cmake", "in")
_FILE = (r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*"
         r"[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:" + "|".join(EXTS) + r"))")
_ONE = r"[:#]L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?"
_CONT = r"(?:\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?)*"
ANCHOR_RE = re.compile(_FILE + _ONE + _CONT)
CONT_RE = re.compile(r"\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?")
# C7 规模声明的**规范句式**（ANCHOR_CONTRACT.md §1 必须逐字如此；改锚必须同步本行）
SCALE_RE = re.compile(
    r"现行规模（C7 逐字复测）：\*\*(\d+) 文档 / (\d+) 锚\*\* = "
    r"(\d+) 目标锚 \+ (\d+) 登记豁免 \+ (\d+) 未解析登记。")

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
    """Yield (raw, base, start, end) for every anchor occurrence on one doc line."""
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


def main(argv=None):
    ap = argparse.ArgumentParser(description="ALG/SCI doc line-anchor re-test")
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--print-scale", action="store_true",
                    help="只打印 ANCHOR_CONTRACT.md §1 应逐字写成的现行规模行")
    ap.add_argument("--print-registry", action="store_true",
                    help="只打印 unresolved_registry.json 应登记的未解析锚清单")
    args = ap.parse_args(argv)
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
