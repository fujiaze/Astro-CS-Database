#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-402 A1: defaults.json 出处自洽核对（只读，不改仓库）。

对每个 field：
  * 比较 source 文本里第一个 `path:line` 与 source_ref.path/line 是否同锚；
  * 打开被引文件，取该行原文，判断是否含该 field 的 value（数字/枚举 token）；
输出 TSV 供判读表引用。
"""
import json
import re
import sys
from pathlib import Path

REPO = Path(r"F:/Astro dev/Astro CS Normalization Database")
OUT = Path(r"独立审计/复算件/q402-A1/anchor_check.tsv")

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

doc = json.loads((REPO / "eng/packaging/config/defaults.json").read_text(encoding="utf-8"))
anchor_re = re.compile(r"(docs/[A-Za-z0-9_\-/.]+\.md|eng/[A-Za-z0-9_\-/.]+\.json|run/[A-Za-z0-9_\-/.]+|lib/[A-Za-z0-9_\-/.]+)[,: ]*(\d+)")


def value_tokens(v):
    if isinstance(v, bool):
        return ["true", "false"]
    if isinstance(v, (int, float)):
        s = repr(float(v)) if isinstance(v, float) else str(v)
        return [s, s.rstrip("0").rstrip("."), str(v)]
    if isinstance(v, list):
        out = []
        for x in v:
            out += value_tokens(x)
        return out
    if isinstance(v, str):
        return [v]
    return []


rows = []
for f in doc["fields"]:
    key = f["key"]
    val = f.get("value")
    src = f.get("source") or ""
    sref = f.get("source_ref") or {}
    m = anchor_re.search(src)
    src_path, src_line = (m.group(1), int(m.group(2))) if m else ("", 0)
    ref_path, ref_line = sref.get("path") or "", sref.get("line")
    same = (src_path == ref_path) and (src_line == ref_line) if ref_path else None
    text = ""
    hit = "n/a"
    for p, ln in ((src_path, src_line), (ref_path or "", ref_line or 0)):
        if not p or not ln:
            continue
        fp = REPO / p
        if not fp.exists():
            text = "<<MISSING FILE>>" + p
            hit = "missing"
            break
        lines = fp.read_text(encoding="utf-8", errors="replace").splitlines()
        line = lines[ln - 1] if 0 < ln <= len(lines) else "<<OUT OF RANGE>>"
        toks = [t for t in value_tokens(val) if t not in ("None",)]
        hit = "HIT" if any(t and t in line for t in toks) else "NO-VALUE"
        text = "%s:%d => %s" % (p, ln, line.strip()[:150])
        break
    rows.append((key, json.dumps(val, ensure_ascii=False),
                 "SAME" if same else ("DIFF" if same is False else "ONLY-SOURCE"),
                 "%s:%s vs %s:%s" % (src_path, src_line, ref_path, ref_line), hit, text))

with OUT.open("w", encoding="utf-8") as fh:
    fh.write("key\tvalue\tanchor_agree\tanchors\tvalue_in_cited_line\tcited_line_text\n")
    for r in rows:
        fh.write("\t".join(str(x) for x in r) + "\n")

for r in rows:
    print(" | ".join(str(x) for x in r))
