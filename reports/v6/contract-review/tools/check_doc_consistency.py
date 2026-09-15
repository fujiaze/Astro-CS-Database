#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""人读合同文档 ↔ 机器冻结表 一致性检查（含文档级负向控制：删行必须判红）。"""
import json, os, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
DOCS = [
    "docs/science/v6/frozen/00_README.md",
    "docs/science/v6/frozen/01_SEMANTIC_FREEZE.md",
    "docs/science/v6/frozen/02_SIGNOFF_CONTROLLER_SUPERSEDED.md",
    "docs/contracts/v6/frozen/00_README.md",
    "docs/contracts/v6/frozen/01_DATA_CONTRACT_FREEZE.md",
    "docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md",
    "docs/algorithms/v6/frozen/00_README.md",
    "docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md",
    "docs/algorithms/v6/frozen/02_GATE_AND_MUTATION_FREEZE.md",
    "reports/v6/contract-review/01_FREEZE_STATUS_MATRIX.md",
    "reports/v6/contract-review/03_SUPERSEDED_SCI_SECTIONS.md",
    "reports/v6/contract-review/04_OPEN_ITEMS_AND_SIGNOFF.md",
]

def esc(s):
    return str(s).replace("|", "\\|").replace("\n", " ")

def load_machine():
    with open(os.path.join(ROOT, "docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json"), encoding="utf-8") as f:
        return json.load(f)

def read_docs():
    return "\n".join(open(os.path.join(ROOT, d), encoding="utf-8").read() for d in DOCS)

def check(text, M):
    fails = []
    n = 0
    for c in M["clauses"]:
        n += 1
        if esc(c["id"]) not in text:
            fails.append("clause id missing from docs: " + c["id"])
        v = c.get("value_display", c.get("value"))
        if v is not None and esc(v) not in text and str(v) not in text:
            fails.append("clause value missing from docs: " + c["id"] + " = " + str(v)[:60])
        if c.get("anchor") and esc(c["anchor"]) not in text:
            fails.append("clause anchor missing from docs: " + c["id"])
    for s in M["signoff_items"]:
        if s["id"] not in text:
            fails.append("signoff id missing: " + s["id"])
    for o in M["open_items"]:
        if o["id"] not in text:
            fails.append("open item id missing: " + o["id"])
    return fails, n

def main():
    M = load_machine()
    text = read_docs()
    fails, n = check(text, M)
    if fails:
        print("FAILURES (%d):" % len(fails))
        for f in fails[:40]:
            print("  - " + f)
        return 1
    # 文档级负向控制：删除 FZ-UNIT-WINFO 所在行后必须判红
    lines = text.splitlines()
    mutated = "\n".join(ln for ln in lines if "FZ-UNIT-WINFO" not in ln)
    mfails, _ = check(mutated, M)
    if not any("FZ-UNIT-WINFO" in f for f in mfails):
        print("NEGATIVE-CONTROL-FAILED: deleting FZ-UNIT-WINFO row was not detected")
        return 1
    print("DOC-CONSISTENCY PASS: %d clauses, %d docs; negative control caught FZ-UNIT-WINFO removal" % (n, len(DOCS)))
    return 0

if __name__ == "__main__":
    sys.exit(main())
