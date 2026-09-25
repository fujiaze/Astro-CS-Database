#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""R1 补充取证：D3b 在干净克隆前缀表 {"RELEASE"} 下对正式文档面的实际命中数。
复刻 check_doc_hygiene.py 的 mask_code + D3B_ID_RE + writing_docs 扫描面（只读，不执行仓库脚本）。"""
import collections
import os
import re
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = r"F:\Astro dev\Astro CS Normalization Database"
FORMAL_ROOT_DOCS = ("ASTROCS_DESIGN.md", "AGENTS.md", "ENGINEERING_SPEC.md", "ACCEPTANCE_SPEC.md")
ENTRY_DOCS = ("README.md", "ASTROCS_DESIGN.md", "ENGINEERING_SPEC.md", "ACCEPTANCE_SPEC.md",
              "CONTROL_PACK_SPEC.md", "DEPENDENCIES.md")
EXEMPT_DIRS = ("docs/research/", "docs/archive/")
SCAN_EXTS = (".md", ".yaml", ".yml", ".csv", ".json", ".txt")
D3B_ID_RE = re.compile(r"\b([A-Za-z][A-Za-z0-9]*)((?:-[A-Za-z0-9]+)+)-(\d{1,3})\b")
FENCE_LINE_RE = re.compile(r"^\s*(?:" + chr(96) * 3 + r"|~~~)")
INLINE_CODE_RE = re.compile(chr(96) + "[^" + chr(96) + "]*" + chr(96))

docs = [n for n in FORMAL_ROOT_DOCS if os.path.isfile(os.path.join(REPO, n))]
for dirpath, dirnames, filenames in os.walk(os.path.join(REPO, "docs")):
    for fn in filenames:
        rel = os.path.relpath(os.path.join(dirpath, fn), REPO).replace(os.sep, "/")
        if not rel.endswith(SCAN_EXTS) or any(rel.startswith(e) for e in EXEMPT_DIRS):
            continue
        docs.append(rel)
for n in ENTRY_DOCS:
    if os.path.isfile(os.path.join(REPO, n)) and n not in docs:
        docs.append(n)
docs = sorted(set(docs))
print("正式文档扫描面文件数 = %d" % len(docs))

tok_total = collections.Counter()
heads = collections.Counter()
caught_clean = []
in_fence_or_span = 0
for rel in docs:
    text = open(os.path.join(REPO, rel), encoding="utf-8", errors="replace").read()
    in_fence = False
    for i, ln in enumerate(text.splitlines(), 1):
        if FENCE_LINE_RE.match(ln):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        m_ln = INLINE_CODE_RE.sub(lambda m: " " * len(m.group(0)), ln)
        for m in D3B_ID_RE.finditer(m_ln):
            tok_total[m.group(0)] += 1
            heads[m.group(1)] += 1
            if m.group(1) == "RELEASE":
                caught_clean.append((rel, i, m.group(0)))

print("D3b 形态可匹配 token 总数（含代码豁免后）= %d，不同前缀 %d 个"
      % (sum(tok_total.values()), len(heads)))
print("前缀分布 top15:", heads.most_common(15))
print("干净克隆前缀表 {'RELEASE'} 实际命中条数 = %d %s" % (len(caught_clean), caught_clean[:5]))
print("=> 覆盖率 = %d / %d" % (len(caught_clean), sum(tok_total.values())))
print("含单 dash 形态（RELEASE-05 / AUDIT-06 类）是否可匹配：",
      bool(D3B_ID_RE.search("RELEASE-05")), bool(D3B_ID_RE.search("AUDIT-06")))
print("三 dash 形态可匹配：", D3B_ID_RE.search("GATE-TRIAGE-01").group(0),
      D3B_ID_RE.search("DOC-HYGIENE-01").group(0))
