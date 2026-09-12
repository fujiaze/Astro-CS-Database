#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-ANCHOR-001｜ALG/SCI 文档源码行号锚全量复测器（exit 0 = PASS）。

任务：冻结文档行号锚（docs/science/*.md、docs/algorithms/*.md 中的 `path/to/file.ext:N[-M]`
形态锚）全量复测——锚指向的符号/行为与文档一致；漂移锚更新行号并保持语义不变。

规则（详见 docs/algorithms/anchors/ANCHOR_CONTRACT.md）：
  C1 docs_tracked      作用域文档存在且被 Git 跟踪
  C2 anchor_resolved   每个锚解析到唯一真实目标文件（exact → doc-relative →
                       contract.resolvers → unique basename）；无法解析即 FAIL
                       （除非在 contract.exemptions 中登记并给出理由）
  C3 range_in_bounds   1 <= start <= end <= 目标文件行数
  C4 symbol_binding    contract.bindings 声明的 (doc, target, symbol)：该 doc 内
                       必须存在一个指向 target 的锚，其行范围内逐字包含 symbol。
                       符号整体不存在于 target → STALE_BINDING；
                       存在但已不在任何锚范围内 → BINDING_VIOLATION（即漂移锚）
  C5 exemptions_live   exemption 必须命中至少一个真实锚，否则 STALE_EXEMPTION

输出为稳定排序 JSON（无时间戳），跨 cwd/复跑 bitwise 一致。

用法：
  python3 docs/algorithms/anchors/check_doc_line_anchors.py [--root .] [--json-out F]
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
EXTS = ("cpp", "cc", "cxx", "h", "hpp", "hh", "py", "sh", "ps1", "txt",
        "json", "yaml", "yml", "md", "cmake", "in")
_FILE = (r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*"
         r"[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:" + "|".join(EXTS) + r"))")
_ONE = r"[:#]L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?"
_CONT = r"(?:\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?)*"
ANCHOR_RE = re.compile(_FILE + _ONE + _CONT)
CONT_RE = re.compile(r"\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?")

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


def emit(errors, anchors, json_out):
    anchors = anchors or []
    by_status = {}
    for a in anchors:
        by_status[a["status"]] = by_status.get(a["status"], 0) + 1
    doc_count = len({a["doc"] for a in anchors})
    report = {
        "schema": "astrocs/doc-line-anchors/v1",
        "task": "SCI-ANCHOR-001",
        "verdict": "PASS" if not errors else "FAIL",
        "documents": doc_count,
        "anchors_total": len(anchors),
        "by_status": dict(sorted(by_status.items())),
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
        print("DOC_LINE_ANCHORS_FAIL:")
        for e in errors:
            print("  [%s] %s" % (e["code"], e["detail"]))
        return 1
    print("DOC_LINE_ANCHORS_PASS: %d docs, %d anchors, status=%s"
          % (doc_count, len(anchors),
             ",".join("%s:%d" % kv for kv in sorted(by_status.items()))))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="ALG/SCI doc line-anchor re-test")
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    args = ap.parse_args(argv)
    root = os.path.abspath(args.root)

    errors = []
    contract_path = os.path.join(root, CONTRACT_REL)
    if not os.path.isfile(contract_path):
        errors.append(fail("C0_contract_exists", CONTRACT_REL))
        return emit(errors, None, args.json_out)
    contract = load_json(contract_path)
    resolvers = contract.get("resolvers", [])
    exemptions = contract.get("exemptions", [])
    bindings = contract.get("bindings", [])

    tracked = git_ls(root)
    tracked_set = set(tracked) if tracked is not None else None
    basename_index = build_basename_index(root)

    docs = []
    for pattern in contract.get("doc_globs", []):
        for p in sorted(_glob.glob(os.path.join(root, pattern))):
            docs.append(os.path.relpath(p, root).replace(os.sep, "/"))
    docs = sorted(set(docs))

    for d in docs:
        if not os.path.isfile(os.path.join(root, d)):
            errors.append(fail("C1_docs_tracked", "missing doc: " + d))
        elif tracked_set is not None and d not in tracked_set:
            errors.append(fail("C1_docs_tracked", "untracked doc: " + d))

    exempt_keys = {(ex["doc"], ex["raw"]) for ex in exemptions}
    matched_exemptions = set()

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
                    anchors.append(rec)
                    continue
                resolved, how = resolve(root, doc, base, resolvers, basename_index)
                rec["how"] = how
                if resolved is None:
                    rec["status"] = "UNRESOLVED"
                    errors.append(fail("C2_anchor_resolved",
                                       "%s:%d %s (%s)" % (doc, i, raw, how)))
                    anchors.append(rec)
                    continue
                rec["resolved"] = resolved
                if tracked_set is not None and resolved not in tracked_set:
                    rec["status"] = "UNTRACKED"
                    errors.append(fail("C2_anchor_resolved",
                                       "%s:%d %s -> untracked %s" % (doc, i, raw, resolved)))
                    anchors.append(rec)
                    continue
                n = count_lines(root, resolved)
                rec["target_lines"] = n
                if start < 1 or end < start or end > n:
                    rec["status"] = "OUT_OF_BOUNDS"
                    errors.append(fail("C3_range_in_bounds",
                                       "%s:%d %s -> %s has %d lines"
                                       % (doc, i, raw, resolved, n)))
                else:
                    rec["status"] = "OK"
                anchors.append(rec)

    for doc, raw in sorted(exempt_keys):
        if (doc, raw) not in matched_exemptions:
            errors.append(fail("C5_exemptions_live", "stale exemption: %s %s" % (doc, raw)))

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

    return emit(errors, anchors, args.json_out)


if __name__ == "__main__":
    sys.exit(main())
