#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-DOC-HYGIENE：正式文档过程痕迹门 + 已知限制台账 ID 可解析门。

权威依据：
  ASTROCS_DESIGN.md §0.3（文档写法：只写现行设计要怎样；已撤销的设计与已退役的实现不出现在正式文档里）；
  ENGINEERING_SPEC.md §8 规则 2（正式文档不写历史叙事、不堆任务编号与日期）、规则 4（悬空即缺陷）；
  AGENTS.md §8（提交信息不写流水账与任务编号长串）。

判据（exit 0 = PASS）：
  D1 正式文档过程痕迹
     正式文档 = ASTROCS_DESIGN.md / AGENTS.md / ENGINEERING_SPEC.md / ACCEPTANCE_SPEC.md / docs/**；
     豁免面 = docs/research/**（研究包证据指针）、docs/archive/**（归档件）。
     D1a 负责人裁决**逐字引述**（正则 负责人…裁决…：「…」）；
     D1b 「订正/修订/更正/修正 + 日期」流水（含日期在前式）。豁免：
         行内含变更编号 CHG-<日期>（变更编号是 docs/contracts/INDEX.yaml §12 登记的修订标识，不是过程流水）；
         已登记的历史遗留（本轮范围外）逐条列在 PREEXISTING。
     D1c 工作项编号 token 表（FORBIDDEN_TOKENS + SCI-5xx；只减不增的棘轮）。
         豁免：token 作为**已登记检查项 ID**（CHK-/CON-/LOG- 前缀）的一部分出现；PREEXISTING 登记项。
  D2 已知限制台账 ID 可解析（docs/KNOWN_LIMITATIONS.md 的条目号是稳定 ID）
     D2a 正式文档存在且条目号集合非空（scanned == 0 ⇒ 判红，fail-closed）；
     D2b 活动引用「KNOWN_LIMITATIONS.md … 条目 N / §<节>」的 N / 节必须存在；
     D2c 活动引用「… §E <发现编号>」的编号必须出现在台账对照表内；
     D2d 台账 artifacts/evidence/known-limitations-ledger/LEDGER.md 存在，且其 §1 编号表的
         条目号集合与正式文档**双向一致**（迁移后 ID 仍可解析）。

用法：
  python3 eng/tools/doccheck/check_doc_hygiene.py [--root <repo>] [--json-out <file>] [--quiet]
  python3 eng/tools/doccheck/check_doc_hygiene.py --dump <d1|d2|entries>
  python3 eng/tools/doccheck/check_doc_hygiene.py --self-test
exit 0 = PASS；exit 1 = FAIL；exit 2 = 输入不可用（fail-closed）；--self-test 恒 0 = 内置正/负例全符合预期。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

FORMAL_ROOT_DOCS = ("ASTROCS_DESIGN.md", "AGENTS.md", "ENGINEERING_SPEC.md", "ACCEPTANCE_SPEC.md")
FORMAL_DOCS_DIR = "docs"
EXEMPT_DIRS = ("docs/research/", "docs/archive/")
LIMITS_DOC = "docs/KNOWN_LIMITATIONS.md"
LEDGER = "artifacts/evidence/known-limitations-ledger/LEDGER.md"
SCAN_EXTS = (".md", ".yaml", ".yml", ".csv", ".json", ".txt")
# 引用扫描面（活动树）；过程/证据目录不入面，与 docs/DOCUMENT_INDEX.yaml 的 downstream 扫描口径一致
REF_SCAN_SKIP = ("docs/archive/", "artifacts/", "run/", "build/", "lib/third_party/",
                 "实验/", "工程控制/", "gaia/", "testdata/", ".git/", ".dsh-code-index/")

# ---- D1 ----------------------------------------------------------------------
VERBATIM_RE = re.compile(r"负责人[^\n]{0,24}?裁决[^\n]{0,12}?[：:]\s*[「“]")
CORRECTION_RE = re.compile(r"(?:订正|修订|更正|修正)[^\n]{0,60}?20\d{2}-\d{2}-\d{2}"
                           r"|20\d{2}-\d{2}-\d{2}[^\n]{0,60}?(?:订正|修订|更正|修正)")
CHG_EXEMPT_RE = re.compile(r"CHG-\d{4}-\d{2}-\d{2}")
# 工作项编号 token 表（本轮回归面 + P5-SNR 家族；只减不增）
FORBIDDEN_TOKENS = (
    "DRIZZLE-FIX-01", "ARCH-DEBT-01", "MEM-WIRE-01", "GAIA-FAILCLOSED-01",
    "ARCH-AUDIT-01", "ARCH-AUDIT-02", "ARCH-AUDIT-03",
    "PROJ-AUDIT-01", "DRIZZLE-AUDIT-01", "DOC-CONTRACT-MERGE-02", "P5-SNR",
)
FORBIDDEN_SCI_TASK_RE = re.compile(r"\bSCI-5\d{2}\b")
# 已登记检查项 ID 前缀：token 作为注册表键的一部分出现不算过程痕迹
REGISTERED_ID_PREFIXES = ("CHK-", "CON-", "LOG-")
# 机器登记表的字段位（owner/task/...）：其取值是登记键，不是文档叙事，故不判过程痕迹
REGISTRY_FIELD_RE = re.compile(
    r'"(?:owner|task|by|reanchored_by|authoring_task|authoring_owner)"\s*:\s*"[^"]*"\s*,?\s*$')
# 已登记的历史遗留（本轮范围外；只减不增）。
# 每项 = (相对路径, 规则, 行内指纹)；指纹不再命中即表示该遗留已被清理，登记项应同步删除。
PREEXISTING = (
    ("docs/DOCUMENT_INDEX.yaml", "D1b", "P3-PROJ-DOC (2026-09-11)"),
    ("docs/DOCUMENT_INDEX.yaml", "D1b", "P2-COV-DOC (2026-09-07)"),
    ("docs/DOCUMENT_INDEX.yaml", "D1b", "P1-PHOT-DOC (2026-09-07)"),
    ("docs/DOCUMENT_INDEX.yaml", "D1b", "P1-WCS-DOC (2026-09-07)"),
    ("docs/DOCUMENT_INDEX.yaml", "D1b", "P1-WCS-DOC 手写合同页修订 (2026-09-07)"),
    ("docs/DOCUMENT_INDEX.yaml", "D1b", "P2-INT-DOC (2026-09-09)"),
    ("docs/DOCUMENT_INDEX.yaml", "D1b", "P2-REJ-DOC (2026-09-09)"),
    ("docs/DOCUMENT_INDEX.yaml", "D1b", "P1-STAR-DOC 事实修订（2026-09-07）"),
    ("docs/contracts/ARCH-001.md", "D1b", "DOC-202 R04 订正（2026-09-20）"),
    ("docs/contracts/CONFIG_CONTRACT.md", "D1b", "DOC-203 / S02 订正"),
    ("docs/contracts/DATA_SEMANTICS.md", "D1b", "多通道 channels≠1 拒绝"),
    ("docs/contracts/DATA_SEMANTICS.md", "D1b", "read_wcs_params_from_frame"),
    ("docs/contracts/DATA_SEMANTICS.md", "D1b", "api.cpp:552-557"),
    ("docs/contracts/DATA_SEMANTICS.md", "D1b", "api.cpp:398-402"),
    ("docs/contracts/DATA_SEMANTICS.md", "D1b", "上位依据订正 2026-09-16"),
    ("docs/contracts/DATA_SEMANTICS.md", "D1b", "订正说明（SCI-FIX-PROJ 2026-09-16"),
    ("docs/contracts/DATA_SEMANTICS.md", "D1b", "上位科学权威（**订正 2026-09-16**"),
    ("docs/contracts/INDEX.yaml", "D1b", "本索引的修订标识一律用"),
    ("docs/contracts/PUBLIC_API.md", "D1b", "api.cpp:168-352"),
    ("docs/contracts/PUBLIC_API.md", "D1b", "DOC-203 订正"),
    ("docs/owner/PIPELINE_OVERVIEW.md", "D1b", "GATE-FIX-RES 订正"),
    ("docs/owner/SCIENCE_OVERVIEW.md", "D1b", "DOC-202 R23-adjacent 订正"),
    ("docs/owner/SCIENCE_OVERVIEW.md", "D1b", "DOC-202 R26 订正 2026-09-20"),
    ("docs/contracts/DUAL_LINE_CONTRACT.md", "D1c", "归 SCI-503/504/505"),
)


def read(path):
    # 非 UTF-8 字节按替换字符读入（不因单个文件编码问题 traceback / 静默跳过）
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


def is_preexisting(rel, rule, line):
    for p, r, fp in PREEXISTING:
        if p == rel and r == rule and fp in line:
            return True
    return False


def formal_docs(root):
    out = []
    for name in FORMAL_ROOT_DOCS:
        if os.path.isfile(os.path.join(root, name)):
            out.append(name)
    for dirpath, dirnames, filenames in os.walk(os.path.join(root, FORMAL_DOCS_DIR)):
        dirnames[:] = [d for d in dirnames if d != ".git"]
        for fn in filenames:
            rel = os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/")
            if not rel.endswith(SCAN_EXTS):
                continue
            if any(rel.startswith(e) for e in EXEMPT_DIRS):
                continue
            out.append(rel)
    return sorted(out)


def token_hits(line, tok):
    """token 作为已登记检查项 ID 的一部分出现时豁免（如 CHK-MEM-WIRE-01）。"""
    start = 0
    while True:
        i = line.find(tok, start)
        if i < 0:
            return False
        if not any(line[:i].endswith(p) for p in REGISTERED_ID_PREFIXES):
            return True
        start = i + len(tok)


def check_d1(root, docs, v, notes):
    notes["d1_scanned"] = len(docs)
    for rel in docs:
        for i, line in enumerate(read(os.path.join(root, rel)).splitlines(), 1):
            if REGISTRY_FIELD_RE.search(line):
                continue  # 机器登记表字段位（owner/task/...）：登记键不是过程叙事
            if VERBATIM_RE.search(line) and not is_preexisting(rel, "D1a", line):
                v.append({"check": "D1a_verbatim_adjudication", "file": rel, "line": i,
                          "detail": "正式文档出现负责人裁决逐字引述"})
            if (CORRECTION_RE.search(line) and not CHG_EXEMPT_RE.search(line)
                    and not is_preexisting(rel, "D1b", line)):
                v.append({"check": "D1b_correction_log", "file": rel, "line": i,
                          "detail": "正式文档出现「订正/修订 + 日期」流水"})
            for tok in FORBIDDEN_TOKENS:
                if token_hits(line, tok) and not is_preexisting(rel, "D1c", line):
                    v.append({"check": "D1c_work_item_token", "file": rel, "line": i,
                              "detail": "正式文档出现工作项编号 " + tok})
            m = FORBIDDEN_SCI_TASK_RE.search(line)
            if m and not is_preexisting(rel, "D1c", line):
                v.append({"check": "D1c_work_item_token", "file": rel, "line": i,
                          "detail": "正式文档出现工作项编号 " + m.group(0)})


# ---- D2 ----------------------------------------------------------------------
ITEM_HEAD_RE = re.compile(r"^(\d{1,3})\.\s", re.M)
REF_ITEM_RE = re.compile(r"KNOWN_LIMITATIONS\.md[^\n]{0,60}?条目\s*(\d{1,3})")
REF_SECTION_RE = re.compile(r"KNOWN_LIMITATIONS\.md[^\n]{0,60}?§\s*([A-E])\b")
REF_DISCOVERY_RE = re.compile(r"KNOWN_LIMITATIONS\.md[^\n]{0,80}?§E\s+([A-Z]{1,4}-\d{1,3}[a-z]?)")
REF_DISCOVERY_ALT_RE = re.compile(r"KNOWN_LIMITATIONS\.md[^\n]{0,80}?原发现编号\s*([A-Z]{1,4}-\d{1,3}[a-z]?)")
LEDGER_ROW_RE = re.compile(r"^\|\s*(\d{1,3})\s*\|", re.M)
LEDGER_SECTION_RE = re.compile(r"^##\s*([A-E])\.", re.M)
SECTION_HEAD_RE = re.compile(r"^##\s*([A-E])\.", re.M)


def section_items(text):
    """按 '## <字母>.' 切节，返回 {节字母: {条目号}}。"""
    out, marks = {}, list(SECTION_HEAD_RE.finditer(text))
    for k, m in enumerate(marks):
        end = marks[k + 1].start() if k + 1 < len(marks) else len(text)
        body = text[m.end():end]
        out[m.group(1)] = {int(x.group(1)) for x in ITEM_HEAD_RE.finditer(body)}
    return out


def ref_files(root):
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames
                       if d not in ("build", ".git", ".dsh-code-index", "gaia")]
        for fn in filenames:
            rel = os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/")
            if any(rel.startswith(s) for s in REF_SCAN_SKIP):
                continue
            if not rel.endswith(SCAN_EXTS):
                continue
            out.append(rel)
    return sorted(out)


def check_d2(root, v, notes):
    dp = os.path.join(root, LIMITS_DOC)
    lp = os.path.join(root, LEDGER)
    if not os.path.isfile(dp):
        v.append({"check": "D2a_limits_doc_missing", "detail": "缺 " + LIMITS_DOC})
        return
    doc_text = read(dp)
    items = {int(m.group(1)) for m in ITEM_HEAD_RE.finditer(doc_text)}
    if not items:
        v.append({"check": "D2a_limits_doc_empty",
                  "detail": LIMITS_DOC + " 提取到 0 个条目号（fail-closed）"})
        return
    notes["items"] = sorted(items)
    sec_items = section_items(doc_text)
    notes["section_e_items"] = sorted(sec_items.get("E", set()))
    if not os.path.isfile(lp):
        v.append({"check": "D2d_ledger_missing", "detail": "缺 " + LEDGER})
        return
    ledger_text = read(lp)
    ledger_items = {int(m.group(1)) for m in LEDGER_ROW_RE.finditer(ledger_text)}
    notes["ledger_items"] = sorted(ledger_items)
    if ledger_items != sec_items.get("E", set()):
        v.append({"check": "D2d_ledger_items_mismatch",
                  "detail": "台账编号表与正式文档 §E 条目不一致：only_doc=%s only_ledger=%s"
                            % (sorted(sec_items.get("E", set()) - ledger_items),
                               sorted(ledger_items - sec_items.get("E", set())))})
    sections = {m.group(1) for m in LEDGER_SECTION_RE.finditer(doc_text)}
    scanned = 0
    for rel in ref_files(root):
        if rel == LIMITS_DOC:
            continue
        for i, line in enumerate(read(os.path.join(root, rel)).splitlines(), 1):
            if "KNOWN_LIMITATIONS.md" not in line:
                continue
            scanned += 1
            for m in REF_ITEM_RE.finditer(line):
                if int(m.group(1)) not in items:
                    v.append({"check": "D2b_item_ref_dangling", "file": rel, "line": i,
                              "detail": "引用条目 %s 不存在于 %s" % (m.group(1), LIMITS_DOC)})
            for m in REF_SECTION_RE.finditer(line):
                if m.group(1) not in sections:
                    v.append({"check": "D2b_section_ref_dangling", "file": rel, "line": i,
                              "detail": "引用 §%s 不存在于 %s" % (m.group(1), LIMITS_DOC)})
            for rx in (REF_DISCOVERY_RE, REF_DISCOVERY_ALT_RE):
                for m in rx.finditer(line):
                    if m.group(1) not in ledger_text:
                        v.append({"check": "D2c_discovery_id_unresolved", "file": rel, "line": i,
                                  "detail": "原发现编号 %s 不在台账对照表内" % m.group(1)})
    if scanned == 0:
        v.append({"check": "D2_scan_empty",
                  "detail": "扫描面读到 0 条 KNOWN_LIMITATIONS 引用（fail-closed）"})
    notes["refs_scanned"] = scanned


def run(root):
    v, notes = [], {}
    if not os.path.isdir(root):
        return [{"check": "root_missing", "detail": root}], notes
    docs = formal_docs(root)
    if not docs:
        return [{"check": "D1_scan_empty", "detail": "正式文档扫描面为 0（fail-closed）"}], notes
    check_d1(root, docs, v, notes)
    check_d2(root, v, notes)
    return v, notes


# ---- self-test ---------------------------------------------------------------
CLEAN_LIMITS = ("# Known Limitations\n\n## A. 面\n\n1. **条目一**：陈述。\n\n"
                "## E. 面\n\n30. **条目三十**：陈述。\n    - **归属/去向**：x。\n\n"
                "32. **条目三十二**：陈述。\n\n35. **条目三十五**：陈述。\n")
CLEAN_LEDGER = ("# Ledger\n\n## 1. 编号注册表\n\n| 条目 | 发现任务 |\n|---|---|\n"
                "| 30 | ARCH-AUDIT-01 B-3 |\n| 32 | ARCH-AUDIT-01 M-4 |\n"
                "| 35 | ARCH-AUDIT-02 F-07 |\n\n## 2. 原 §E 全文\n\n（留痕）\n")
CLEAN_REF = ("# 引用方\n\n见 docs/KNOWN_LIMITATIONS.md 条目 30 与 条目 35。\n"
             "见 docs/KNOWN_LIMITATIONS.md §E M-4（原发现编号）。\n")


def build(root, limits=CLEAN_LIMITS, ledger=CLEAN_LEDGER, ref=CLEAN_REF, extra=()):
    os.makedirs(os.path.join(root, "docs"), exist_ok=True)
    os.makedirs(os.path.join(root, "artifacts/evidence/known-limitations-ledger"), exist_ok=True)
    with open(os.path.join(root, "AGENTS.md"), "w", encoding="utf-8") as fh:
        fh.write("# AGENTS\n\n## 11. 任务未完成时的默认动作是继续执行\n")
    with open(os.path.join(root, LIMITS_DOC), "w", encoding="utf-8") as fh:
        fh.write(limits)
    if ledger:
        with open(os.path.join(root, LEDGER), "w", encoding="utf-8") as fh:
            fh.write(ledger)
    with open(os.path.join(root, "docs/refs.md"), "w", encoding="utf-8") as fh:
        fh.write(ref)
    for name, text in extra:
        p = os.path.join(root, name)
        d = os.path.dirname(p)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(p, "w", encoding="utf-8") as fh:
            fh.write(text)


def self_test():
    cases = [
        ("P1 干净仓", dict(), None),
        ("N1 逐字裁决引述", dict(extra=[("docs/x.md", "> 负责人裁决（2026-09-22）：「TRIM 那就纳入。」\n")]), "D1a_verbatim_adjudication"),
        ("N2 订正+日期流水", dict(extra=[("docs/x.md", "> **2026-09-22 订正（DRIZZLE-FIX-01）**：旧写法作废。\n")]), "D1b_correction_log"),
        ("N3 工作项编号", dict(extra=[("docs/x.md", "> 本门由 MEM-WIRE-01 落地。\n")]), "D1c_work_item_token"),
        ("N4 SCI-5xx 工作项编号", dict(extra=[("docs/x.md", "> 复核登记（SCI-505）。\n")]), "D1c_work_item_token"),
        ("N5 条目引用悬空", dict(ref="见 docs/KNOWN_LIMITATIONS.md 条目 99。\n"), "D2b_item_ref_dangling"),
        ("N6 台账编号不一致", dict(ledger=CLEAN_LEDGER.replace("| 35 | ARCH-AUDIT-02 F-07 |\n", "")), "D2d_ledger_items_mismatch"),
        ("N7 台账缺失", dict(ledger=""), "D2d_ledger_missing"),
        ("N8 条目号提取为空", dict(limits="# Known Limitations\n\n无编号。\n"), "D2a_limits_doc_empty"),
        ("N9 发现编号不在台账", dict(ref="见 docs/KNOWN_LIMITATIONS.md §E N-7。\n"), "D2c_discovery_id_unresolved"),
        ("N10 引用扫描面为空", dict(ref="# 无引用\n"), "D2_scan_empty"),
    ]
    tmp = tempfile.mkdtemp(prefix="doc-hygiene-")
    ok = True
    try:
        for idx, (name, kw, expect) in enumerate(cases):
            root = os.path.join(tmp, "c%02d" % idx)
            build(root, **kw)
            v, _ = run(root)
            got = sorted({x["check"] for x in v})
            good = (not v) if expect is None else (expect in got)
            print("%-24s expect=%-30s got=%s  %s"
                  % (name, expect or "(clean)", got or "[]", "OK" if good else "MISMATCH"))
            ok = ok and good
        return 0 if ok else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--json-out")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--dump", choices=("d1", "d2", "entries"))
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if not os.path.isdir(a.root):
        print(json.dumps({"verdict": "FAIL", "reason": "root_missing", "root": a.root}))
        return 2
    v, notes = run(a.root)
    summary = {"check": "CHK-DOC-HYGIENE", "root": a.root, "violations": len(v),
               "verdict": "PASS" if not v else "FAIL", "notes": notes}
    if a.dump:
        print(json.dumps(v if a.dump != "entries" else notes, ensure_ascii=False, indent=1))
        return 0
    if a.json_out:
        d = os.path.dirname(os.path.abspath(a.json_out))
        if d:
            os.makedirs(d, exist_ok=True)
        with open(a.json_out, "w", encoding="utf-8") as fh:
            json.dump({"summary": summary, "violations": v}, fh, ensure_ascii=False, indent=1)
    if not a.quiet or v:
        print(json.dumps(summary, ensure_ascii=False))
        for x in v[:40]:
            print("  " + json.dumps(x, ensure_ascii=False))
    return 0 if not v else 1


if __name__ == "__main__":
    sys.exit(main())
