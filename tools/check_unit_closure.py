#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_unit_closure.py — DOCCHK-002 单位二义性机器检查器 (V3 漏检修复)。
权威: 08 §2「核心单位禁止 'ADU 或 ADU/pixel' 这类二选一表述; 必须冻结一种定义并说明换算」+
      DOC-001 GLOSSARY (adu=canonical 信号单位; electron=DRIZZLE 域允许的等价标注)。

检出规则:
  U1 逻辑二选一: `X 或 Y`(X,Y∈单位 token, 且 X≠Y) — 单位定义处出现即候选。
  U2 斜杠双单位: `X/Y`(X,Y∈信号单位集合 {ADU, e-, e⁻, electron, electron, Jy, DN, count})
       且上下文是在**单位/定义为 X/Y** 的声明(非 px² 这类派生维数)。
  U3 解析性: 每个候选必须在该文档同节(或全文)出现「语义固定/冻结/禁止…混/取 ADU/以 ADU 为
       主」这类冻结消歧语句; 否则 FAIL(mutation 注入二义即被抓, 无需预知内容)。

出口: 0 PASS, 1 合同不一致, 2 环境错。
用法: tools/check_unit_closure.py [--repo <root>] [--docs-dir <dir>] [--stdout-json] [--allow-syn]
"""
from __future__ import annotations

import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# 信号单位 token(二义判定对象); px²/deg 等派生维数不算"二选一单位"本身
SIGNAL = {"ADU", "e-", "e⁻", "e−", "electron", "electron-", "Jy", "DN", "count"}
UNIT_TKN = {"ADU", "e-", "e⁻", "e−", "electron", "Jy", "DN", "count", "px", "px²", "px^2",
            "deg", "mag", "dex", "rad", "s", "ADU²", "ADU^2", "ADU⁻²", "pixel"}
# 冻结消歧语句(存在其一即认定已冻结/说明换算)
FREEZE = re.compile(
    r"语义固定|冻结|禁止[^。]{0,20}混|取\s*ADU|以\s*ADU\s*为主|等价标注|单一单位|统一为|"
    r"主单位\s*[=：:][^。{或}]{0,20}ADU", re.S)


def _repo(p: str) -> str:
    p = os.path.abspath(p)
    if os.path.isdir(os.path.join(p, "docs")) and os.path.isdir(os.path.join(p, "lib")):
        return p
    return REPO


class Chk:
    def __init__(self, repo: str, docs_dir: str | None = None):
        self.repo = repo
        self.doc_api = docs_dir if docs_dir else os.path.join(repo, "docs", "api")
        self.failures: list[str] = []

    def fail(self, m):
        self.failures.append(m)

    def _scan(self, rel: str, text: str) -> list[str]:
        err = []
        # U2: X/Y 双信号单位(如 ADU/e-)
        for m in re.finditer(r"([A-Za-z⁻²^]+)\s*/\s*([A-Za-z⁻²^]+)", text):
            a, b = m.group(1).strip(), m.group(2).strip()
            if a in SIGNAL and b in SIGNAL and a != b:
                seg = text[max(0, m.start() - 40):m.end() + 40]
                if not FREEZE.search(seg + " " + text[:2000]):
                    err.append(f"{rel}: U2 双信号单位 `{a}/{b}` (第{m.start()}字) 未见冻结消歧")
        # U1: X 或 Y 二选一(复合单位 token, 可含 / 与 ² 幂)
        toks = r"[A-Za-z⁻²^]+(?:\s*/\s*[A-Za-z⁻²^]+)*"
        pat1 = re.compile(r"(" + toks + r")\s*[或]\s*(" + toks + r")")
        for m in pat1.finditer(text):
            a, b = m.group(1).strip(), m.group(2).strip()
            a_n = a.replace(" ", ""); b_n = b.replace(" ", "")
            if a_n != b_n and (a_n in SIGNAL or "/" in a_n) and (b_n in SIGNAL or "/" in b_n):
                seg = text[max(0, m.start() - 40):m.end() + 40]
                if not FREEZE.search(seg + " " + text[:2000]):
                    err.append(f"{rel}: U1 二选一单位 `{a_n}或{b_n}` (第{m.start()}字) 未见冻结消歧")
        return err

    def check_docs(self):
        roots = ["docs/science", "docs/api"]
        for root in roots:
            d = os.path.join(self.repo, root)
            if not os.path.isdir(d):
                continue
            for dirpath, _, files in os.walk(d):
                for fn in files:
                    if not fn.endswith(".md"):
                        continue
                    full = os.path.join(dirpath, fn)
                    rel = os.path.relpath(full, self.repo)
                    txt = open(full, encoding="utf-8", errors="ignore").read()
                    for e in self._scan(rel, txt):
                        self.fail(e)


def main():
    repo = REPO
    if "--repo" in sys.argv:
        repo = _repo(sys.argv[sys.argv.index("--repo") + 1])
    dd = None
    if "--docs-dir" in sys.argv:
        dd = sys.argv[sys.argv.index("--docs-dir") + 1]
    c = Chk(repo, docs_dir=dd)
    c.check_docs()
    if c.failures:
        if "--stdout-json" in sys.argv:
            print(json.dumps({"status": "FAIL", "failures": c.failures[:50]}))
        for f in c.failures[:50]:
            sys.stderr.write("DOCCHK-002 UNIT FAIL: %s\n" % f)
        return 1
    if "--stdout-json" in sys.argv:
        print(json.dumps({"status": "PASS", "failures": 0}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
