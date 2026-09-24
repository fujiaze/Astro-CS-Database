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
  D3 写作规则：前向陈述（禁词表 D3a–D3h）
      D3a 绝对日期 20xx-xx-xx（豁免：同行带外部出处指针 DOI/arXiv/RFC/ISO/CCSDS/URL/REC-/WD-）；
      D3b 进程/任务编号：命名空间前缀表（**数据驱动**：由 run/ 与 工程控制/ 的目录名派生）∧ 编号形态
          ∧ 不在机器登记面（eng/contracts、eng/ci、docs、artifacts/evidence 的 json/yaml/csv）；
      D3c run/ 路径（证据路径落 artifacts/evidence/）；D3d git sha 字面量（7–64 位十六进制且含 a–f）；
      D3e 历史词（原先/此前/曾经/后来/…/改为）；D3f 负责人引述（负责人+原话|指示|裁决|授权|要求|口径|令|确认）；
      D3g 删除线留档（~~…~~）；D3h 禁令类表述（不得/禁止/严禁/不允许）。
  D4 禁模式（结构性判据）
      D4a 过程段落（单段内 ≥2 个 D3b 命中）；D4b 事实唯一性（登记事实键取值在多份文档间发散，
          正本 = eng/tools/doccheck/doc_fact_authority.json 的 authority_value）；
      D4c 锚密度（file:line 锚 > 0.2/行）；D4d 单元格墙（表格行 > 200 字符）；D4e 段落长度（> 12 行）。
  豁免机制（三层，全部显式可审计，逐项计数上报 notes）：
      ① 结构豁免：代码围栏与行内 code span（机器字面量）；
      ② 规则内建豁免：机器登记字段位、机器登记面里的已登记 ID、带出处的外部日期；
      ③ 显式豁免台账 eng/tools/doccheck/doc_hygiene_exempt.json（逐条 path+rule+fingerprint+reason+authority，
         只减不增：条目不再命中 ⇒ 判红 D3_exempt_stale）。
  棘轮基线 artifacts/evidence/doc-hygiene/baseline.json：入口文档（README 与根级规范）不参与基线（零容忍），
      其余文档新增违规判红 D3_new_violation / D4_new_violation，基线残留判红 D3_ratchet_stale；
      --update-baseline 只允许收紧（新基线必须是旧基线的子集）。

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
import collections
import hashlib
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
        dirnames[:] = [d for d in dirnames
                       if d != ".git" and not RESERVED_DEVICE_RE.match(d)]
        for fn in filenames:
            if RESERVED_DEVICE_RE.match(fn):
                continue
            rel = rel_path(root, os.path.join(dirpath, fn))
            if rel is None:
                continue
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


# Windows 保留设备名（NUL/CON/PRN/AUX/COM1-9/LPT1-9）。os.walk 在工作树里遇到这类
# 条目时，ntpath.relpath 会把路径解析成设备名并抛
#   ValueError: path is not on mount 'F:' / path is on mount '\\.\nul'
# —— 检查器因此抛 Traceback 而不是给 verdict（门崩 ≠ 判红，两者必须分开）。
RESERVED_DEVICE_RE = re.compile(
    r"^(?:nul|con|prn|aux|com[1-9]|lpt[1-9])(?:\..*)?$", re.IGNORECASE)


def rel_path(root, full):
    """相对仓库根的 POSIX 风格路径；不可解析（设备名/跨卷/符号环）⇒ None。

    判据：路径无法归到仓库根时**不静默丢弃** —— 返回 None 由调用方登记
    ``D2_scan_unresolvable`` 判红（fail-closed），不得让门崩掉。
    """
    try:
        return os.path.relpath(full, root).replace(os.sep, "/")
    except (ValueError, OSError):
        return None


def ref_files(root):
    out = []
    unresolvable = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames
                       if d not in ("build", ".git", ".dsh-code-index", "gaia")
                       and not RESERVED_DEVICE_RE.match(d)]
        for fn in filenames:
            if RESERVED_DEVICE_RE.match(fn):
                unresolvable.append(os.path.join(dirpath, fn))
                continue
            rel = rel_path(root, os.path.join(dirpath, fn))
            if rel is None:
                unresolvable.append(os.path.join(dirpath, fn))
                continue
            if any(rel.startswith(s) for s in REF_SCAN_SKIP):
                continue
            if not rel.endswith(SCAN_EXTS):
                continue
            out.append(rel)
    ref_files.unresolvable = unresolvable
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
    unresolvable = getattr(ref_files, "unresolvable", [])
    if unresolvable:
        v.append({"check": "D2_scan_unresolvable",
                  "detail": ("扫描面有 %d 条路径无法归到仓库根（Windows 保留设备名/跨卷/"
                             "符号环）：%s —— 不静默丢弃，按 fail-closed 判红"
                             % (len(unresolvable), unresolvable[:5]))})
    if scanned == 0:
        v.append({"check": "D2_scan_empty",
                  "detail": "扫描面读到 0 条 KNOWN_LIMITATIONS 引用（fail-closed）"})
    notes["refs_scanned"] = scanned
    notes["refs_unresolvable"] = len(unresolvable)


def run(root):
    v, notes = [], {}
    if not os.path.isdir(root):
        return [{"check": "root_missing", "detail": root}], notes
    docs = formal_docs(root)
    if not docs:
        return [{"check": "D1_scan_empty", "detail": "正式文档扫描面为 0（fail-closed）"}], notes
    check_d1(root, docs, v, notes)
    check_d2(root, v, notes)
    check_writing_rules(root, v, notes)
    return v, notes


# ---- D3/D4：写作规则（前向陈述；禁词表 + 禁模式表）------------------------------
# 权威依据：ASTROCS_DESIGN.md §0.3（只写现行设计要怎样）、ENGINEERING_SPEC.md §8 规则 2
# （正式文档写"要怎样"）、AGENTS.md §1.1（先定规范再动手）。
# 判据逐条对应 run/DOC-SYSTEM-PLAN-01/PLAN.md §2.2.2 的 D3a–D3g / D4a–D4e；
# 新增 D3h（禁令类表述：不得/禁止/严禁/不允许）承载"只写正向约束"这条写作规则。
ENTRY_DOCS = ("README.md", "ASTROCS_DESIGN.md", "ENGINEERING_SPEC.md", "ACCEPTANCE_SPEC.md",
              "CONTROL_PACK_SPEC.md", "DEPENDENCIES.md")
EXEMPT_LEDGER = "eng/tools/doccheck/doc_hygiene_exempt.json"
RATCHET_BASELINE = "artifacts/evidence/doc-hygiene/baseline.json"
FACT_REGISTRY = "eng/tools/doccheck/doc_fact_authority.json"
REGISTRY_SURFACES = ("eng/contracts", "eng/ci", "docs", "artifacts/evidence")
REGISTRY_EXTS = (".json", ".yaml", ".yml", ".csv")
TASK_FIELD_RE = re.compile(r'"(?:owner|task|by|authoring_task|authoring_owner|reanchored_by)"\s*:')
FENCE_LINE_RE = re.compile(r"^\s*(?:" + chr(96) * 3 + r"|~~~)")
INLINE_CODE_RE = re.compile(chr(96) + "[^" + chr(96) + "]*" + chr(96))
D3A_DATE_RE = re.compile(r"\b20\d{2}-\d{2}-\d{2}\b")
D3A_CITE_RE = re.compile(r"DOI|arXiv|RFC\s?\d|ISO\s?\d|CCSDS|https?://|\bREC-|\bWD-")
D3B_ID_RE = re.compile(r"\b([A-Za-z][A-Za-z0-9]*)((?:-[A-Za-z0-9]+)+)-(\d{1,3})\b")
D3C_RUNPATH_RE = re.compile(r"(?:^|[\s(（\[\"'" + chr(96) + r"])run/[A-Za-z0-9_./-]+")
D3D_SHA_RE = re.compile(r"(?<![\w./])(?=[0-9a-f]*[a-f])[0-9a-f]{7,64}(?![\w./])")
D3E_HIST_RE = re.compile(r"原先|原来|此前|以前|曾经|后来|早先|先前|一度|现已|本会话|上一轮|改为|改成|已改为|不再(?:是|返回|使用)")
D3F_OWNER_RE = re.compile(r"负责人(?:的)?(?:原话|指示|裁决|授权|要求|口径|令|确认)")
D3G_STRIKE_RE = re.compile(r"~~.+?~~")
D3H_BAN_RE = re.compile(r"不得|禁止|严禁|不允许")
D4C_ANCHOR_RE = re.compile(
    r"[\w./-]+\.(?:c|h|cpp|hpp|py|json|yaml|yml|md|txt|csv|sh|ps1|cmake|js|ts|html):\d+")
D4_TABLE_ROW_RE = re.compile(r"^\s*\|")
D4_MAX_PARAGRAPH_LINES = 12
D4_MAX_TABLE_ROW_CHARS = 200
D4_MAX_ANCHOR_DENSITY = 0.2
FINGERPRINT_LEN = 16
D3_RULE_IDS = ("D3a_date", "D3b_proc_id", "D3c_run_path", "D3d_sha",
               "D3e_history", "D3f_owner_quote", "D3g_strikethrough", "D3h_prohibition")
D4_RULE_IDS = ("D4a_process_paragraph", "D4c_anchor_density", "D4d_cell_wall", "D4e_paragraph_len")


def fingerprint(line):
    return hashlib.sha256(line.strip().encode("utf-8", "replace")).hexdigest()[:FINGERPRINT_LEN]


def mask_code(text):
    """去代码围栏与行内 code span（机器字面量的结构性豁免）；返回 (lines, notes)。"""
    out, in_fence, fence_lines, spans = [], False, 0, 0
    for ln in text.splitlines():
        if FENCE_LINE_RE.match(ln):
            in_fence = not in_fence
            fence_lines += 1
            out.append("")
            continue
        if in_fence:
            fence_lines += 1
            out.append("")
            continue
        spans += len(INLINE_CODE_RE.findall(ln))
        out.append(INLINE_CODE_RE.sub(lambda m: " " * len(m.group(0)), ln))
    return out, {"fence_lines": fence_lines, "inline_code_spans": spans}


def namespace_prefixes(root):
    """进程编号命名空间前缀表：由 run/ 与 工程控制/ 的目录名数据驱动派生（无手写 token 表）。"""
    out = set()
    for base in ("run", "工程控制"):
        d = os.path.join(root, base)
        if not os.path.isdir(d):
            continue
        for name in os.listdir(d):
            if name.startswith(".") or not os.path.isdir(os.path.join(d, name)):
                continue
            head = re.split(r"[-_]", name)[0]
            if re.match(r"^[A-Z][A-Za-z0-9]*$", head) and len(head) >= 2:
                out.add(head)
    return out


def registered_ids(root):
    """机器登记面里的 ID 集合（登记即事实；任务字段位不入面）。"""
    out = set()
    for base in REGISTRY_SURFACES:
        if not os.path.isdir(os.path.join(root, base)):
            continue
        for dirpath, dirnames, filenames in os.walk(os.path.join(root, base)):
            dirnames[:] = [d for d in dirnames if d != ".git" and not RESERVED_DEVICE_RE.match(d)]
            for fn in filenames:
                if not fn.endswith(REGISTRY_EXTS) or RESERVED_DEVICE_RE.match(fn):
                    continue
                try:
                    text = read(os.path.join(dirpath, fn))
                except OSError:
                    continue
                for ln in text.splitlines():
                    if TASK_FIELD_RE.search(ln):
                        continue
                    for m in D3B_ID_RE.finditer(ln):
                        out.add(m.group(0))
    return out


def load_exempt(root):
    p = os.path.join(root, EXEMPT_LEDGER)
    if not os.path.isfile(p):
        return [], None
    try:
        doc = json.loads(read(p))
    except (ValueError, OSError) as exc:
        return [], "exempt_ledger_unparsable: %s" % exc
    out = []
    for e in doc.get("entries", []):
        out.append({"path": e.get("path", ""), "rule": e.get("rule", ""),
                    "fingerprint": e.get("fingerprint", ""), "reason": e.get("reason", ""),
                    "authority": e.get("authority", "")})
    return out, None


def load_baseline(root):
    p = os.path.join(root, RATCHET_BASELINE)
    if not os.path.isfile(p):
        return None
    try:
        return json.loads(read(p))
    except (ValueError, OSError) as exc:
        return {"__error__": str(exc)}


def writing_docs(root):
    out = list(formal_docs(root))
    for name in ENTRY_DOCS:
        if os.path.isfile(os.path.join(root, name)) and name not in out:
            out.append(name)
    return sorted(set(out))


def scan_hits(root, docs):
    """D3 词面 + D4 结构面一次扫完；返回 (hits, metrics, exempt, used, code_notes, ex_err)。"""
    prefixes = namespace_prefixes(root)
    regs = registered_ids(root)
    exempt, ex_err = load_exempt(root)
    used = set()
    hits, metrics = [], {}
    code_notes = collections.Counter()
    for rel in docs:
        try:
            text = read(os.path.join(root, rel))
        except OSError:
            continue
        lines, cnotes = mask_code(text)
        code_notes["fence_lines"] += cnotes["fence_lines"]
        code_notes["inline_code_spans"] += cnotes["inline_code_spans"]
        para, para_hits, anchors = 0, 0, 0
        for i, ln in enumerate(lines, 1):
            if ln.strip():
                para += 1
            else:
                if para > D4_MAX_PARAGRAPH_LINES:
                    metrics[("D4e_paragraph_len", rel)] = metrics.get(("D4e_paragraph_len", rel), 0) + 1
                if para_hits >= 2:
                    metrics[("D4a_process_paragraph", rel)] = metrics.get(("D4a_process_paragraph", rel), 0) + 1
                para, para_hits = 0, 0
            if D4_TABLE_ROW_RE.match(ln) and len(ln) > D4_MAX_TABLE_ROW_CHARS:
                metrics[("D4d_cell_wall", rel)] = metrics.get(("D4d_cell_wall", rel), 0) + 1
            if not ln.strip() and False:
                pass
            anchors += len(D4C_ANCHOR_RE.findall(ln))
            cand = []
            if D3A_DATE_RE.search(ln) and not D3A_CITE_RE.search(ln):
                cand.append("D3a_date")
            if D3C_RUNPATH_RE.search(ln):
                cand.append("D3c_run_path")
            if D3D_SHA_RE.search(ln) and not REGISTRY_FIELD_RE.search(ln):
                cand.append("D3d_sha")
            if D3E_HIST_RE.search(ln):
                cand.append("D3e_history")
            if D3F_OWNER_RE.search(ln):
                cand.append("D3f_owner_quote")
            if D3G_STRIKE_RE.search(ln):
                cand.append("D3g_strikethrough")
            if D3H_BAN_RE.search(ln):
                cand.append("D3h_prohibition")
            for m in D3B_ID_RE.finditer(ln):
                if m.group(1) in prefixes and m.group(0) not in regs:
                    cand.append("D3b_proc_id")
                    para_hits += 1
            if not cand:
                continue
            fp = fingerprint(ln)
            for rule in cand:
                ex = None
                for e in exempt:
                    if e["path"] == rel and e["rule"] == rule and e["fingerprint"] == fp:
                        ex = e
                        break
                if ex is not None:
                    used.add((ex["path"], ex["rule"], ex["fingerprint"]))
                    continue
                hits.append({"rule": rule, "path": rel, "line": i, "fingerprint": fp,
                             "text": ln.strip()[:160]})
        if para > D4_MAX_PARAGRAPH_LINES:
            metrics[("D4e_paragraph_len", rel)] = metrics.get(("D4e_paragraph_len", rel), 0) + 1
        if para_hits >= 2:
            metrics[("D4a_process_paragraph", rel)] = metrics.get(("D4a_process_paragraph", rel), 0) + 1
        if lines and anchors / max(1.0, len(lines)) > D4_MAX_ANCHOR_DENSITY:
            metrics[("D4c_anchor_density", rel)] = round(anchors / max(1.0, len(lines)), 4)
    return hits, metrics, exempt, used, code_notes, ex_err


def check_d4b_facts(root, v, notes):
    """D4b 事实唯一性：登记事实键的取值必须等于其正本取值。"""
    p = os.path.join(root, FACT_REGISTRY)
    if not os.path.isfile(p):
        v.append({"check": "D4b_fact_registry_missing", "detail": "缺 " + FACT_REGISTRY + "（fail-closed）"})
        return
    try:
        doc = json.loads(read(p))
    except (ValueError, OSError) as exc:
        v.append({"check": "D4b_fact_registry_unparsable", "detail": str(exc)})
        return
    facts = doc.get("facts", [])
    if not facts:
        v.append({"check": "D4b_fact_registry_empty", "detail": "登记事实为 0（fail-closed）"})
        return
    notes["d4b_facts"] = len(facts)
    pending, divergent = [], []
    # 事实正本的取值以 authority_value 指向的文件为准；注册表里的字面量只是无指针时的来源。
    # 这样「正本改了而注册表没跟」不会判成发散，也不会用过期字面量把真发散盖掉。
    def _fact_authority_read(root_dir, authority_value, fallback):
        src, _, ptr = (authority_value or "").partition("#")
        # 只有 JSON 登记表能按指针取正本值；Markdown 锚点形态（file#anchor）用登记的字面量。
        if not ptr or not src.lower().endswith(".json"):
            return fallback, None
        try:
            with open(os.path.join(root_dir, src), "r", encoding="utf-8") as fh:
                node = json.load(fh)
        except Exception as exc:  # noqa: BLE001 - 读不出即 fail-closed
            return None, "正本读取失败 %s（%s）" % (src, exc)
        for part in [p for p in ptr.split("/") if p]:
            if isinstance(node, dict) and part in node:
                node = node[part]
            else:
                node = None
                break
        if node is None:
            # 登记表形态的正本（[{key, value, ...}, ...]）：按 key 命中取 value。
            def _by_key(n):
                if isinstance(n, dict):
                    if n.get("key") == ptr and "value" in n:
                        return n["value"]
                    for sub in n.values():
                        got = _by_key(sub)
                        if got is not None:
                            return got
                elif isinstance(n, list):
                    for sub in n:
                        got = _by_key(sub)
                        if got is not None:
                            return got
                return None
            with open(os.path.join(root_dir, src), "r", encoding="utf-8") as fh:
                node = _by_key(json.load(fh))
        if node is None:
            return None, "指针 %s 在正本 %s 中不存在" % (ptr, src)
        return str(node), None
    for fact in facts:
        key = fact.get("key", "")
        auth_value = fact.get("authority_value", "")
        auth_src = auth_value.split("#", 1)[0]
        if auth_src and not os.path.exists(os.path.join(root, auth_src)):
            v.append({"check": "D4b_fact_authority_missing", "fact": key,
                      "detail": "事实正本文件不存在：" + auth_value})
            continue
        value_res = [re.compile(r) for r in fact.get("value_res", [])]
        if not value_res:
            v.append({"check": "D4b_fact_value_res_empty", "fact": key,
                      "detail": "事实未登记取值正则（fail-closed）"})
            continue
        sites = collections.defaultdict(list)
        for rel in writing_docs(root):
            # 事实键的取值声明常写在 code span 内（如 drizzle.pixfrac=1.0），故 D4b 扫原始行
            lines = read(os.path.join(root, rel)).splitlines()
            for i, ln in enumerate(lines, 1):
                for rx in value_res:
                    m = rx.search(ln)
                    if m:
                        sites[m.group(1)].append("%s:%d" % (rel, i))
        if not sites:
            v.append({"check": "D4b_fact_unmatched", "fact": key,
                      "detail": "登记事实在文档面零命中（fail-closed）：" + fact.get("value_hint", "")})
            continue
        want, why = _fact_authority_read(root, auth_value, fact.get("authority_literal", ""))
        if want is None:
            v.append({"check": "D4b_fact_authority_unreadable", "fact": key,
                      "detail": why + "（fail-closed：注册表字面量只作无指针时的来源）"})
            continue
        bad = {val: ws for val, ws in sites.items() if val != want}
        if bad:
            rec = {"fact": key, "authority_value": auth_value, "authority_literal": want,
                   "divergent": {val: sorted(ws)[:6] for val, ws in bad.items()},
                   "matched": sorted(sites.get(want, []))[:6]}
            if fact.get("adjudication") == "pending":
                pending.append(rec)
            else:
                divergent.append(rec)
    notes["d4b_pending"] = pending
    for rec in divergent:
        v.append({"check": "D4b_fact_divergence", "fact": rec["fact"],
                  "detail": "取值发散：正本 %s = %s；异值 %s" % (rec["authority_value"],
                          rec["authority_literal"], rec["divergent"])})


def check_writing_rules(root, v, notes):
    docs = writing_docs(root)
    if not docs:
        v.append({"check": "D3_scan_empty", "detail": "写作规则扫描面为 0（fail-closed）"})
        return
    hits, metrics, exempt, used, code_notes, ex_err = scan_hits(root, docs)
    notes["d3_scanned"] = len(docs)
    prefixes_now = namespace_prefixes(root)
    notes["d3_prefixes"] = len(prefixes_now)
    if not prefixes_now:
        v.append({"check": "D3b_namespace_table_empty",
                  "detail": ("进程编号命名空间前缀表为空（run/ 与 工程控制/ 下均无目录）"
                             "⇒ D3b 判据不可评估，按 fail-closed 判红")})
    notes["d3_registered_ids"] = len(registered_ids(root))
    notes["exempt_ledger_entries"] = len(exempt)
    notes["d3_exempt_structural"] = dict(code_notes)
    if ex_err:
        v.append({"check": "D3_exempt_ledger_unparsable", "detail": ex_err})
    for e in exempt:
        if not e["reason"] or not e["authority"]:
            v.append({"check": "D3_exempt_unauditable", "path": e["path"], "rule": e["rule"],
                      "detail": "豁免台账条目缺 reason/authority"})
        if (e["path"], e["rule"], e["fingerprint"]) not in used:
            v.append({"check": "D3_exempt_stale", "path": e["path"], "rule": e["rule"],
                      "detail": "豁免台账条目当前不再命中 ⇒ 应删除或收紧"})
    notes["d3_exempt_used"] = len(used)
    entry_hits = [h for h in hits if h["path"] in ENTRY_DOCS]
    rest = [h for h in hits if h["path"] not in ENTRY_DOCS]
    notes["d3_entry_doc_violations"] = len(entry_hits)
    notes["d3_other_violations"] = len(rest)
    notes["d3_current"] = dict(collections.Counter(h["rule"] for h in hits))
    for h in entry_hits:
        v.append({"check": "D3_entry_doc_violation", "rule": h["rule"], "file": h["path"],
                  "line": h["line"], "detail": "入口文档零容忍：" + h["text"]})
    base = load_baseline(root)
    if isinstance(base, dict) and "__error__" in base:
        v.append({"check": "D3_ratchet_unparsable", "detail": base["__error__"]})
        base = None
    notes["ratchet_baseline"] = RATCHET_BASELINE if base is not None else "absent"
    base_d3 = (base or {}).get("d3", {}) if isinstance(base, dict) else {}
    base_d4 = (base or {}).get("d4", {}) if isinstance(base, dict) else {}
    cur_d3 = collections.defaultdict(lambda: collections.defaultdict(set))
    for h in rest:
        cur_d3[h["rule"]][h["path"]].add(h["fingerprint"])
    text_of = {(h["rule"], h["path"], h["fingerprint"]): (h["line"], h["text"]) for h in rest}
    new_cnt = stale_cnt = 0
    for rule in D3_RULE_IDS:
        for path in sorted(set(cur_d3.get(rule, {})) | set(base_d3.get(rule, {}))):
            cur = set(cur_d3.get(rule, {}).get(path, set()))
            old = set(base_d3.get(rule, {}).get(path, []))
            for fp in sorted(cur - old):
                new_cnt += 1
                ln, txt = text_of.get((rule, path, fp), (0, ""))
                v.append({"check": "D3_new_violation", "rule": rule, "file": path, "line": ln,
                          "detail": "棘轮基线未覆盖：" + txt})
            for fp in sorted(old - cur):
                if path in ENTRY_DOCS:
                    continue
                stale_cnt += 1
                v.append({"check": "D3_ratchet_stale", "rule": rule, "file": path,
                          "fingerprint": fp, "detail": "基线条目不再命中 ⇒ 收紧基线"})
    notes["d3_new_violations"] = new_cnt
    notes["d3_ratchet_stale"] = stale_cnt
    cur_d4 = collections.defaultdict(dict)
    for (rule, path), val in metrics.items():
        cur_d4[rule][path] = val
    d4_new = d4_stale = 0
    for rule in D4_RULE_IDS:
        for path in sorted(set(cur_d4.get(rule, {})) | set(base_d4.get(rule, {}))):
            c = cur_d4.get(rule, {}).get(path, 0)
            b = base_d4.get(rule, {}).get(path, 0)
            if c > b:
                d4_new += 1
                v.append({"check": "D4_new_violation", "rule": rule, "file": path,
                          "detail": "结构面计数 %s > 基线 %s" % (c, b)})
            elif c < b:
                d4_stale += 1
                v.append({"check": "D4_ratchet_stale", "rule": rule, "file": path,
                          "detail": "结构面计数 %s < 基线 %s ⇒ 收紧基线" % (c, b)})
    notes["d4_new_violations"] = d4_new
    notes["d4_ratchet_stale"] = d4_stale
    notes["d4_current"] = {rule: sum(cur_d4.get(rule, {}).values()) for rule in D4_RULE_IDS}
    check_d4b_facts(root, v, notes)


def baseline_doc(root):
    """由当前状态构造基线文档（入口文档不入基线）。"""
    hits, metrics, _exempt, _used, _cn, _err = scan_hits(root, writing_docs(root))
    d3 = collections.defaultdict(lambda: collections.defaultdict(list))
    for h in hits:
        if h["path"] in ENTRY_DOCS:
            continue
        d3[h["rule"]][h["path"]].append(h["fingerprint"])
    d4 = collections.defaultdict(dict)
    for (rule, path), val in metrics.items():
        d4[rule][path] = val
    return {"schema": "doc-hygiene-baseline/v1",
            "note": ("只减不增：新增违规判红；基线残留判红（要求收紧）；入口文档不参与基线。"
                     "由 check_doc_hygiene.py --update-baseline 生成，收紧是唯一合法方向。"),
            "d3_rules": list(D3_RULE_IDS), "d4_rules": list(D4_RULE_IDS),
            "d3": {r: {p: sorted(v) for p, v in d3.get(r, {}).items()} for r in D3_RULE_IDS if d3.get(r)},
            "d4": {r: dict(d4[r]) for r in D4_RULE_IDS if d4.get(r)}}


def flatten_baseline(doc):
    out = set()
    for rule, per in (doc or {}).get("d3", {}).items():
        for path, fps in per.items():
            for fp in fps:
                out.add((rule, path, fp))
    return out


# ---- self-test ---------------------------------------------------------------
CLEAN_LIMITS = ("# Known Limitations\n\n## A. 面\n\n1. **条目一**：陈述。\n\n"
                "## E. 面\n\n30. **条目三十**：陈述。\n    - **归属/去向**：x。\n\n"
                "32. **条目三十二**：陈述。\n\n35. **条目三十五**：陈述。\n")
CLEAN_LEDGER = ("# Ledger\n\n## 1. 编号注册表\n\n| 条目 | 发现任务 |\n|---|---|\n"
                "| 30 | ARCH-AUDIT-01 B-3 |\n| 32 | ARCH-AUDIT-01 M-4 |\n"
                "| 35 | ARCH-AUDIT-02 F-07 |\n\n## 2. 原 §E 全文\n\n（留痕）\n")
CLEAN_REF = ("# 引用方\n\n见 docs/KNOWN_LIMITATIONS.md 条目 30 与 条目 35。\n"
             "见 docs/KNOWN_LIMITATIONS.md §E M-4（原发现编号）。\n")


CLEAN_FACTS = json.dumps({"schema": "doc-fact-authority/v1", "facts": [
    {"key": "sandbox.entry_30", "authority_value": "docs/KNOWN_LIMITATIONS.md#E",
     "authority_literal": "30", "value_res": [r"(\d{1,3})\. \*\*条目三十\*\*"],
     "adjudication": "enforced"}]}, ensure_ascii=False)


def build(root, limits=CLEAN_LIMITS, ledger=CLEAN_LEDGER, ref=CLEAN_REF, extra=(), facts=CLEAN_FACTS,
          ns=True):
    os.makedirs(os.path.join(root, "docs"), exist_ok=True)
    if ns:
        # D3b 的前缀表来源（工程控制/ 随仓库提交，run/ 为本地工作树）
        os.makedirs(os.path.join(root, "工程控制/DOC-SELFTEST-01"), exist_ok=True)
    os.makedirs(os.path.join(root, "artifacts/evidence/known-limitations-ledger"), exist_ok=True)
    if facts:
        os.makedirs(os.path.join(root, "eng/tools/doccheck"), exist_ok=True)
        with open(os.path.join(root, FACT_REGISTRY), "w", encoding="utf-8") as fh:
            fh.write(facts)
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
        ("N10 引用扫描面为空", dict(ref="# 无引用\n", facts=None), "D2_scan_empty"),
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

        # N11（GATE-TRIAGE-01 负例）：Windows 保留设备名/跨卷路径不得让门崩。
        # 复现原缺陷：os.walk 在工作树里遇到设备名条目时 ntpath.relpath 抛
        #   ValueError: path is on mount '\\\\.\\nul', start on mount 'F:'
        # 原实现直接 Traceback（rc=1 但**没有 verdict**，与"判红"不可区分）。
        # 本负例在任意平台可跑：用 monkeypatch 让 relpath 对探针路径抛同样的异常，
        # 并断言 ①run() 不抛 ②判词里出现 D2_scan_unresolvable（fail-closed 留痕）。
        root = os.path.join(tmp, "c%02d" % len(cases))
        build(root)
        probe = os.path.join(root, "docs", "probe.md")
        with open(probe, "w", encoding="utf-8") as fh:
            fh.write("见 docs/KNOWN_LIMITATIONS.md 条目 30。\n")
        real_relpath = os.path.relpath

        def boom(path, start=None):
            if os.path.abspath(str(path)) == os.path.abspath(probe):
                raise ValueError("path is on mount '\\\\\\\\.\\\\nul', start on mount 'F:'")
            return real_relpath(path, start) if start is not None else real_relpath(path)

        os.path.relpath = boom
        try:
            v, notes = run(root)
        except Exception as exc:  # noqa: BLE001 - 崩溃即负例失败
            v, notes = [{"check": "CRASHED:" + exc.__class__.__name__}], {}
        finally:
            os.path.relpath = real_relpath
        got = sorted({x["check"] for x in v})
        good = "D2_scan_unresolvable" in got and notes.get("refs_unresolvable", 0) >= 1
        print("%-24s expect=%-30s got=%s  %s"
              % ("N11 设备名路径不崩", "D2_scan_unresolvable", got or "[]",
                 "OK" if good else "MISMATCH"))
        ok = ok and good

        # N11b（判别力自证）：把 rel_path 换回**修复前**的裸 os.path.relpath，
        # 同一输入必须让 run() 抛 ValueError —— 证明 N11 不是恒真门。
        real_rel_path = globals()["rel_path"]

        def raw_rel_path(root_, full):
            return os.path.relpath(full, root_).replace(os.sep, "/")

        globals()["rel_path"] = raw_rel_path
        os.path.relpath = boom
        try:
            run(root)
            pre_fix_raised = False
        except ValueError:
            pre_fix_raised = True
        except Exception:  # noqa: BLE001
            pre_fix_raised = False
        finally:
            os.path.relpath = real_relpath
            globals()["rel_path"] = real_rel_path
        print("%-24s expect=%-30s got=%s  %s"
              % ("N11b 修复前必崩", "ValueError", pre_fix_raised,
                 "OK" if pre_fix_raised else "MISMATCH"))
        ok = ok and pre_fix_raised

        # N12：RESERVED_DEVICE_RE 的正/负例（判据本身不得恒真/恒假）
        dev_pos = all(RESERVED_DEVICE_RE.match(x) for x in
                      ("nul", "NUL", "con", "aux", "prn", "com1", "lpt9", "nul.txt"))
        dev_neg = not any(RESERVED_DEVICE_RE.match(x) for x in
                          ("null", "console", "com0", "com10", "lpt", "nul_.md"))
        good = dev_pos and dev_neg
        print("%-24s expect=%-30s got=%s  %s"
              % ("N12 设备名正则", "8 pos + 6 neg", "pos=%s neg=%s" % (dev_pos, dev_neg),
                 "OK" if good else "MISMATCH"))
        ok = ok and good

        # ---- D3/D4 写作规则：每条规则一个负例 + 正例（合规文本必绿）----
        def case(name, expect, setup=None, **kw):
            root = os.path.join(tmp, "w%02d" % case.n)
            case.n += 1
            build(root, **kw)
            if setup:
                setup(root)
            v, _ = run(root)
            got = sorted({x["check"] for x in v} | {x["rule"] for x in v if x.get("rule")})
            good = (not v) if expect is None else (expect in got)
            print("%-24s expect=%-30s got=%s  %s"
                  % (name, expect or "(clean)", got or "[]", "OK" if good else "MISMATCH"))
            return good

        case.n = 0

        def mk_run(root):
            os.makedirs(os.path.join(root, "run/DOC-SELFTEST-01"), exist_ok=True)

        def mk_exempt(line, rule):
            def _setup(root):
                ent = {"entries": [{"path": "docs/w14.md", "rule": rule,
                                    "fingerprint": fingerprint(line),
                                    "reason": "引述冻结口径", "authority": "ASTROCS_DESIGN.md §0.3"}]}
                with open(os.path.join(root, EXEMPT_LEDGER), "w", encoding="utf-8") as fh:
                    json.dump(ent, fh, ensure_ascii=False)
            return _setup

        def mk_baseline(doc):
            def _setup(root):
                p = os.path.join(root, RATCHET_BASELINE)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, "w", encoding="utf-8") as fh:
                    fh.write(doc)
            return _setup

        results = []
        results.append(case("W0 写作面干净", None))
        results.append(case("W1 D3a 绝对日期", "D3a_date",
                         extra=[("docs/w1.md", "> 冻结于 2026-09-20。\n")]))
        results.append(case("W2 D3b 进程编号", "D3b_proc_id",
                         extra=[("docs/w2.md", "> 本门由 DOC-ALIGN-01 落地。\n")], setup=mk_run))
        results.append(case("W3 D3c run 路径", "D3c_run_path",
                         extra=[("docs/w3.md", "> 证据见 run/DOC-SELFTEST-01/logs/gate.log。\n")], setup=mk_run))
        results.append(case("W4 D3d sha 字面量", "D3d_sha",
                         extra=[("docs/w4.md", "> 基线 HEAD = da051eb232f660342150effa7c2c97da58718e7d。\n")]))
        results.append(case("W5 D3e 历史词", "D3e_history",
                         extra=[("docs/w5.md", "> 原先的写法后来被改成现值。\n")]))
        results.append(case("W6 D3f 负责人引述", "D3f_owner_quote",
                         extra=[("docs/w6.md", "> 负责人裁决：「按现值执行。」\n")]))
        results.append(case("W7 D3g 删除线留档", "D3g_strikethrough",
                         extra=[("docs/w7.md", "> ~~旧结论作废~~ 现结论：x。\n")]))
        results.append(case("W8 D3h 禁令类表述", "D3h_prohibition",
                         extra=[("docs/w8.md", "> 阈值不得硬编码。\n")]))
        results.append(case("W9 D4d 单元格墙", "D4d_cell_wall",
                         extra=[("docs/w9.md", "| a | " + "x" * 240 + " |\n")]))
        results.append(case("W10 D4e 段落长度", "D4e_paragraph_len",
                         extra=[("docs/w10.md", "".join("- 行 %d\n" % i for i in range(14)))]))
        results.append(case("W11 D4c 锚密度", "D4c_anchor_density",
                         extra=[("docs/w11.md", "".join(
                             "第 %d 行 a.cpp:%d 与 b.h:%d\n" % (i, i, i + 1) for i in range(10)))]))
        results.append(case("W12 D4b 事实发散", "D4b_fact_divergence",
                         extra=[("docs/w12.md", "90. **条目九十**：陈述。\n")],
                         facts=CLEAN_FACTS.replace("\u6761\u76ee\u4e09\u5341", "条目三十") if False else
                         json.dumps({"schema": "doc-fact-authority/v1", "facts": [
                             {"key": "sandbox.k", "authority_value": "docs/KNOWN_LIMITATIONS.md#E",
                              "authority_literal": "30",
                              "value_res": [r"(\d{1,3})\. \*\*条目三十\*\*",
                                            r"(\d{1,3})\. \*\*条目九十\*\*"],
                              "adjudication": "enforced"}]}, ensure_ascii=False)))
        results.append(case("W13 豁免台账残留", "D3_exempt_stale",
                         extra=[(EXEMPT_LEDGER, json.dumps({"entries": [
                             {"path": "docs/gone.md", "rule": "D3e_history",
                              "fingerprint": "0" * FINGERPRINT_LEN, "reason": "r",
                              "authority": "a"}]}, ensure_ascii=False))]))
        w14_line = "> 原先的写法后来被改成现值。\n"
        results.append(case("W14 豁免命中即不判红", None,
                         extra=[("docs/w14.md", w14_line)], setup=mk_exempt(w14_line, "D3e_history")))
        w15_line = "> 阈值不得硬编码。\n"
        results.append(case("W15 入口文档零容忍", "D3_entry_doc_violation",
                         extra=[("README.md", w15_line)],
                         setup=mk_baseline(json.dumps({"schema": "doc-hygiene-baseline/v1", "d3": {
                             "D3h_prohibition": {"README.md": [fingerprint(w15_line)]}}, "d4": {}}))))
        results.append(case("W16 基线残留判红", "D3_ratchet_stale",
                         setup=mk_baseline(json.dumps({"schema": "doc-hygiene-baseline/v1", "d3": {
                             "D3h_prohibition": {"docs/old.md": ["f" * FINGERPRINT_LEN]}}, "d4": {}}))))
        results.append(case("W17 基线未覆盖判红", "D3_new_violation",
                         extra=[("docs/w17.md", "> 阈值不得硬编码。\n")],
                         setup=mk_baseline(json.dumps({"schema": "doc-hygiene-baseline/v1",
                                                       "d3": {}, "d4": {}}))))
        real_ban = globals()["D3H_BAN_RE"]
        globals()["D3H_BAN_RE"] = re.compile(r"(?!x)x")
        blind = case("W18 判别力自证（判据关掉）", None,
                     extra=[("docs/w18.md", "> 阈值不得硬编码。\n")])
        globals()["D3H_BAN_RE"] = real_ban
        armed = case("W18 判别力自证（判据打开）", "D3h_prohibition",
                     extra=[("docs/w18.md", "> 阈值不得硬编码。\n")])
        print("%-24s expect=%-30s got=%s  %s"
              % ("W18 判据非恒真", "关绿/开红", "blind=%s armed=%s" % (blind, armed),
                 "OK" if (blind and armed) else "MISMATCH"))
        results.extend([blind, armed])
        # W19：门自身崩溃必须给 CRASH verdict + rc=2（与"判红"可区分；P0-1 E 档教训）
        import contextlib
        import io
        real_run = globals()["run"]

        def boom(_root):
            raise RuntimeError("selftest-crash")

        globals()["run"] = boom
        buf = io.StringIO()
        try:
            with contextlib.redirect_stdout(buf):
                rc = main(["--root", tmp])
        finally:
            globals()["run"] = real_run
        crash_ok = (rc == 2 and '"CRASH"' in buf.getvalue())
        print("%-24s expect=%-30s got=%s  %s"
              % ("W19 门崩给 CRASH", "rc=2 + CRASH",
                 "rc=%s crash=%s" % (rc, '"CRASH"' in buf.getvalue()),
                 "OK" if crash_ok else "MISMATCH"))
        results.append(crash_ok)
        # W20/W21：--update-baseline 只允许收紧（新基线必须是旧基线的子集）
        def upd(root):
            buf2 = io.StringIO()
            with contextlib.redirect_stdout(buf2):
                rc2 = main(["--root", root, "--update-baseline"])
            return rc2, buf2.getvalue()

        root = os.path.join(tmp, "w20")
        build(root)
        mk_baseline(json.dumps({"schema": "doc-hygiene-baseline/v1", "d3": {}, "d4": {}}))(root)
        with open(os.path.join(root, "docs/w20.md"), "w", encoding="utf-8") as fh:
            fh.write("> 阈值不得硬编码。\n")
        rc2, out2 = upd(root)
        grow_ok = (rc2 == 2 and "baseline_would_grow" in out2)
        print("%-24s expect=%-30s got=%s  %s"
              % ("W20 基线拒绝放宽", "rc=2 + would_grow",
                 "rc=%s %s" % (rc2, "would_grow" in out2), "OK" if grow_ok else "MISMATCH"))
        results.append(grow_ok)
        root = os.path.join(tmp, "w21")
        build(root)
        mk_baseline(json.dumps({"schema": "doc-hygiene-baseline/v1", "d3": {}, "d4": {}}))(root)
        rc3, out3 = upd(root)
        shrink_ok = (rc3 == 0 and '"PASS"' in out3)
        print("%-24s expect=%-30s got=%s  %s"
              % ("W21 干净面可建基线", "rc=0 + PASS",
                 "rc=%s %s" % (rc3, '"PASS"' in out3), "OK" if shrink_ok else "MISMATCH"))
        results.append(shrink_ok)
        root = os.path.join(tmp, "w22")
        build(root, ns=False)
        v22, _n22 = run(root)
        got22 = sorted({x["check"] for x in v22} | {x["rule"] for x in v22 if x.get("rule")})
        w22_ok = "D3b_namespace_table_empty" in got22
        print("%-24s expect=%-30s got=%s  %s"
              % ("W22 前缀表空则判红", "D3b_namespace_table_empty", got22 or "[]",
                 "OK" if w22_ok else "MISMATCH"))
        results.append(w22_ok)
        case.n = 30  # 手写 root（w20/w21/w22）不占用 case.n，避免与自动命名碰撞
        results.append(case("W23 D4a 过程段落", "D4a_process_paragraph",
                            extra=[("docs/w23.md", "> 本门由 DOC-ALIGN-01 落地。\n> 复核由 DOC-FIX-02 完成。\n")]))
        ok = ok and all(results)
        return 0 if ok else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--json-out")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--dump", choices=("d1", "d2", "entries", "d3", "d4", "exempt", "facts"))
    ap.add_argument("--audit", action="store_true",
                    help="打印全部当前违规与规则计数（不改 verdict；棘轮基线不隐藏现状）")
    ap.add_argument("--update-baseline", action="store_true",
                    help="重建棘轮基线 artifacts/evidence/doc-hygiene/baseline.json（只允许收紧）")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if not os.path.isdir(a.root):
        print(json.dumps({"verdict": "FAIL", "reason": "root_missing", "root": a.root}))
        return 2
    if a.update_baseline:
        new = baseline_doc(a.root)
        old = load_baseline(a.root)
        if isinstance(old, dict) and "__error__" in old:
            print(json.dumps({"verdict": "FAIL", "reason": "baseline_unparsable",
                              "detail": old["__error__"]}, ensure_ascii=False))
            return 2
        if isinstance(old, dict):
            new_fps = flatten_baseline(new)
            old_fps = flatten_baseline(old)
            grow = sorted(new_fps - old_fps)
            stale = old_fps - new_fps
            # 与 D4 的「计数下降即 stale」合起来只需一条规则：
            # 新增条目多于失效条目 ⇒ 净增长，拒绝。
            # 指纹含整行文本，改动一行会让同一处违规换指纹（旧条目 stale、新条目 new）；
            # 这种一对一的换名不算增长，不然正常编辑永远无法收紧基线。
            if len(grow) > len(stale):
                print(json.dumps({"verdict": "FAIL", "reason": "baseline_would_grow",
                                  "n": len(grow), "stale": len(stale),
                                  "sample": grow[:5]}, ensure_ascii=False))
                return 2
        p = os.path.join(a.root, RATCHET_BASELINE)
        d = os.path.dirname(p)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(new, fh, ensure_ascii=False, indent=1, sort_keys=True)
        print(json.dumps({"verdict": "PASS", "baseline": RATCHET_BASELINE,
                          "d3_entries": sum(len(x) for per in new["d3"].values() for x in per.values()),
                          "d4_entries": sum(len(x) for x in new["d4"].values())}, ensure_ascii=False))
        return 0
    if a.dump in ("d3", "d4", "exempt", "facts"):
        hits, metrics, exempt, used, code_notes, _err = scan_hits(a.root, writing_docs(a.root))
        if a.dump == "d3":
            print(json.dumps(hits, ensure_ascii=False, indent=1))
        elif a.dump == "d4":
            print(json.dumps({"%s|%s" % k: v for k, v in sorted(metrics.items())},
                             ensure_ascii=False, indent=1))
        elif a.dump == "exempt":
            print(json.dumps({"entries": exempt, "used": sorted("%s|%s|%s" % t for t in used),
                              "structural": dict(code_notes)}, ensure_ascii=False, indent=1))
        else:
            fv, fnotes = [], {}
            check_d4b_facts(a.root, fv, fnotes)
            print(json.dumps({"violations": fv, "notes": fnotes}, ensure_ascii=False, indent=1))
        return 0
    try:
        v, notes = run(a.root)
    except Exception as exc:  # noqa: BLE001 - 门崩（CRASH）与判红（FAIL）必须可区分
        print(json.dumps({"check": "CHK-DOC-HYGIENE", "verdict": "CRASH",
                          "reason": exc.__class__.__name__ + ": " + str(exc)}, ensure_ascii=False))
        return 2
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
        for x in (v if a.audit else v[:40]):
            print("  " + json.dumps(x, ensure_ascii=False))
    return 0 if not v else 1


if __name__ == "__main__":
    sys.exit(main())
