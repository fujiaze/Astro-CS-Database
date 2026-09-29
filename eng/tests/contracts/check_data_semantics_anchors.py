#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DATA_SEMANTICS.md 代码锚核证器（审计工具，非 unittest 套件）。

目的
    docs/science/DATA_SEMANTICS.md 以 `file.ext:line`（或区间 `line-line2`）
    锚定实现证据。源文件演化后锚会漂移；本工具对全量锚做三档分类：

    high_confidence   语义 token 证据强（唯一 best 行、分数达标、old 行已失配），
                      输出候选行号；替换前须按文档语义人工复核（中文注释行
                      对位而 ASCII token 失配的场景会被误判失配）；
    old_still_valid   锚指向的行在现行文件中仍含文档行语义 token，无需移动；
    manual_review     证据不足或存在歧义（弱匹配/多候选/无语义 token），
                      需人工按文档语义核证，不给出自动建议。

用法
    python3 eng/tests/contracts/check_data_semantics_anchors.py            # 摘要
    python3 eng/tests/contracts/check_data_semantics_anchors.py --report   # 全量 JSON（stdout）

判据（high_confidence 同时满足）
    1. 锚文件按 basename 在 lib/ 与 eng/ 下唯一解析；
    2. old 行已不含任何文档行语义 token（否则归 old_still_valid）；
    3. 全文件中语义 token 得分的 best 行唯一；
    4. 分数 >= 3（引号串命中按 3 分计权）且 |best - old| <= 300。

修复单：eng/tests/contracts/data_semantics_anchor_fixlist.json（登记已核证重锚的逐条证据）。
"""
import argparse
import collections
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DOC = os.path.join(REPO, "docs", "contracts", "DATA_SEMANTICS.md")
BT = chr(96)

ANCHOR = re.compile(r"[\w.\-]+\.(?:c|h|cpp|hpp|py|json|yaml|yml|sh):\d+(?:-\d+)?")
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]{3,}")
STOP = {
    "this", "that", "with", "from", "into", "they", "their", "there", "which",
    "where", "when", "while", "self", "none", "null", "true", "false", "const",
    "return", "class", "struct", "enum", "void", "long", "double", "float",
    "char", "auto", "case", "break", "continue", "default", "docs", "science",
    "standards", "contracts", "plugins", "module_adapters", "orchestrator",
    "estimator", "integrator", "sampler", "cout", "cerr", "size_t", "int32",
    "int64", "uint8", "uint16", "uint32", "uint64", "float32", "float64",
    "index", "indices", "param", "params", "data", "name", "names", "type",
    "types", "test", "tests",
}


def code_spans(s):
    return re.findall(BT + "([^" + BT + "]+)" + BT, s)


def toks_from(s):
    out = set()
    for t in IDENT.findall(s):
        if t.lower() not in STOP and len(t) <= 40:
            out.add(t)
    return out


def row_semantics(lines, i):
    """锚所在行（表格行取全行格；普通行加上一行）的语义 token 与引号串。"""
    ln = lines[i]
    src = ln
    if i and not ln.strip().startswith("|"):
        src = ln + " " + lines[i - 1]
    body = ANCHOR.sub(" ", src)
    quoted = set(re.findall(r'"([^"]{3,60})"', body))
    sems = toks_from(body)
    for sp in code_spans(src):
        if len(sp) >= 4 and not sp.startswith(":"):
            sems.add(sp)
    return {s for s in sems if len(s) >= 4}, quoted


_file_cache = {}


def resolve(name):
    if name not in _file_cache:
        hits = []
        for root in ("lib", "eng"):
            base = os.path.join(REPO, root)
            for dp, dn, fn in os.walk(base):
                dn[:] = [d for d in dn if d not in (".git", "build", "__pycache__", "node_modules", "run")]
                if name in fn:
                    hits.append(os.path.join(dp, name))
        _file_cache[name] = hits
    return _file_cache[name]


def audit():
    lines = open(DOC, encoding="utf-8").read().split(chr(10))
    entries = []
    stats = collections.Counter()
    for i, ln in enumerate(lines):
        for m in ANCHOR.finditer(ln):
            a = m.group(0)
            fname, rest = a.rsplit(":", 1)
            if "-" in rest:
                l1, l2 = [int(x) for x in rest.split("-")]
            else:
                l1, l2 = int(rest), None
            cand = resolve(fname)
            entry = {"docline": i + 1, "anchor": a, "l1": l1, "l2": l2}
            if len(cand) != 1:
                entry["status"] = "manual_review"
                entry["reason"] = "basename_resolver_%d" % len(cand)
                stats[entry["status"]] += 1
                entries.append(entry)
                continue
            path = cand[0]
            entry["path"] = os.path.relpath(path, REPO)
            tl = open(path, encoding="utf-8", errors="replace").read().split(chr(10))
            sems, quoted = row_semantics(lines, i)
            if not sems:
                entry["status"] = "manual_review"
                entry["reason"] = "no_semantic_token"
                stats[entry["status"]] += 1
                entries.append(entry)
                continue

            def hit(j):
                t = tl[j - 1] if 0 < j <= len(tl) else ""
                return sum(1 for s in sems if s in t), sum(1 for q in quoted if q and q in t)

            old_sc, old_q = hit(l1)
            if old_sc > 0 or old_q > 0:
                entry["status"] = "old_still_valid"
                stats[entry["status"]] += 1
                entries.append(entry)
                continue
            scores = []
            for j in range(1, len(tl) + 1):
                sc, q = hit(j)
                tot = sc + 2 * q
                if tot:
                    scores.append((tot, j, q))
            if not scores:
                entry["status"] = "manual_review"
                entry["reason"] = "no_match"
                stats[entry["status"]] += 1
                entries.append(entry)
                continue
            mx = max(s[0] for s in scores)
            tops = [s[1] for s in scores if s[0] == mx]
            if len(tops) != 1 or mx < 3 or abs(tops[0] - l1) > 300:
                entry["status"] = "manual_review"
                entry["reason"] = "weak_or_ambiguous"
                entry["score"] = mx
                entry["n_candidates"] = len(tops)
                stats[entry["status"]] += 1
                entries.append(entry)
                continue
            entry["status"] = "high_confidence"
            entry["suggested_line"] = tops[0]
            entry["score"] = mx
            stats[entry["status"]] += 1
            entries.append(entry)
    return entries, stats


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--report", action="store_true", help="输出全量 JSON 到 stdout")
    args = ap.parse_args()
    entries, stats = audit()
    print("DATA_SEMANTICS.md 锚核证：total=%d" % len(entries))
    for k in ("old_still_valid", "high_confidence", "manual_review"):
        print("  %-16s %d" % (k, stats.get(k, 0)))
    if args.report:
        json.dump(entries, sys.stdout, ensure_ascii=False, indent=1)
        print()
    else:
        hc = [e for e in entries if e["status"] == "high_confidence"]
        for e in hc[:20]:
            print("  L%-5d %-38s %s -> %s" % (e["docline"], e["anchor"], e["l1"], e.get("suggested_line")))
        if len(hc) > 20:
            print("  ... 其余 %d 条见 --report" % (len(hc) - 20))
    return 0


if __name__ == "__main__":
    sys.exit(main())
