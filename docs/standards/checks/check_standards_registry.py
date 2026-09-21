#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""STD-REG-001 机器检查器：国际标准冻结注册表（docs/standards/STANDARDS_REGISTRY.md）。

合同（docs/standards/STANDARDS_REGISTRY.md §1/§2 冻结）：
  C1  注册表文件存在且登记为 ACTIVE_NORMATIVE（docs/DOCUMENT_INDEX.yaml 机器可解析）；
  C2  六个必需域齐备且与 §2 域表逐行一致（domain key 集合相等）；
  C3  每域字段面完整（DOMAIN/STANDARD/VERSION/CLAUSES/COMPLIANCE/EVIDENCE/DEVIATION）
      且 VERSION 与 §2 域表冻结值逐字相等（版本不得按实现反推）；
  C4  每域符合性清单表存在、列头冻结、≥1 行，行内 status 合法、
      EVIDENCE 指针指向仓库内实际存在的文件/目录；
  C5  每域偏差表存在（无偏差域写「（无）」）、偏差 ID 唯一、POINTER 非空（禁止空指针）；
      另：§3.2「跨域治理偏差」表（跨域治理级 ID 的**定义域**）必须在位、ID 合法唯一、
      POINTER 非空，且与域偏差表跨表不重号；
  C6  偏差 ID 全局闭包：正文出现的 STD-F*/DISP-* 引用必须已定义，禁悬空指针。
      **闭包域 = 本注册表域偏差表 ∪ 本注册表 §3.2 跨域治理偏差表 ∪（若存在）外部
      findings 登记册**；外部登记册缺席时显式登记 C6_external_findings_source
      （缺席=可选来源, 不得当作"无违规"静默返回空集）；存在但不可读 ⇒ ANCHOR_STALE；
  C7  §3 偏差索引行与定义域逐 ID 一致（「（无）」行不计入 ID 集合）；每行指向的
      域/条款在对应清单中真实存在、域 DEVIATION 字段含该 ID；跨域治理行（域列 =
      (跨域治理)）改为校验其定义在 §3.2 表内且字段齐全；
  C8  §3 偏差索引与偏差登记面双向一致：定义域为空 ⇒ 索引必须有显式「（无）」行
      （不得留空）；定义域非空 ⇒ 索引不得出现「（无）」行（不得用「无」掩盖真实偏差）。
  C9  **[W4-A3] 域清单「偏差」列 → 域 DEVIATION 字段 反向一致**：清单行偏差列里
      出现的每个 STD-F*/DISP-* 词元必须在本域 DEVIATION 字段中有定义（反向指向）。
      C4 只判该列"非空"、C7 只判 §3 索引 → DEVIATION，此前反向无人判 ⇒
      "清单行写着 STD-F1 而 DEVIATION 字段删掉它"可以整体绿。

锚存活与 fail-closed（ENGINEERING_SPEC §8）：
  REQUIRED_ANCHORS（REGISTRY_REL / INDEX_REL）在启动时校验 os.path.exists +
  （--root 为 git 工作树根时）git ls-files --error-unmatch 跟踪复核；失效 ⇒
  stderr 打印 "ANCHOR_STALE: <常量名> <路径>" ⇒ exit 2（不 traceback, 不静默通过）。
  可选外部来源由 EXTERNAL_FINDINGS_REL 声明（见 C6 说明）。

用法：
  python3 docs/standards/checks/check_standards_registry.py [--root <repo>]
      [--json-out <file>] [--fault-inject <scenario>]

退出码：0 = PASS；1 = FAIL；2 = ANCHOR_STALE（锚失效/输入不可读, fail-closed）。
--fault-inject：注入缺陷后判定看 verdict 字段, 恒退出 0；唯一例外 anchor-stale
  （锚失效按 §8 必须非零退出, 不得静默通过）退出 2。

注入空转守卫（W4-A3）：8 个文本场景一律经 _replace_once 确定性命中一次替换；
命中 0 次（正文漂移导致锚点失配）或 >1 次 ⇒ 抛 InjectedNoOp（exit 3 + stderr
  FAULT_INJECT_NOOP: ...），**拒绝以原文冒充"已注入"**。事由：drop-wcs003f1-pointer
  的原锚串与注册表正文漂移，str.replace 命中 0 次却返回原文 ⇒ 该场景长期空转，
  --fault-inject 沦为"无论如何都绿"的假自证。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

REGISTRY_REL = "docs/standards/STANDARDS_REGISTRY.md"
INDEX_REL = "docs/DOCUMENT_INDEX.yaml"

# 必需锚（fail-closed）：文件不存在 / 未跟踪 ⇒ ANCHOR_STALE + exit 2。
REQUIRED_ANCHORS = [
    ("REGISTRY_REL", REGISTRY_REL),
    ("INDEX_REL", INDEX_REL),
]

# 可选外部来源：原 05 号 findings 登记册随旧控制包世代清理（ROOT-007/ARCH-001/
# RETIRE-001）删除；其「跨域治理偏差定义」职能已按 CI-003 迁入本注册表 §3.2
# （STD-F6）。存在 ⇒ 并入 C6 闭包域；不存在 ⇒ 显式登记为"缺席(可选)"；存在但
# 不可读 ⇒ ANCHOR_STALE。声明为必需时（EXTERNAL_FINDINGS_REQUIRED=True）缺席即红。
EXTERNAL_FINDINGS_REL = (
    "工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/"
    "05_FINDINGS_REGISTER_20260911.md")
EXTERNAL_FINDINGS_REQUIRED = False

# 负例注入点（仅用于 --fault-inject anchor-stale 与人工复现）：
#   ASTROCS_STD_REG_ANCHOR_OVERRIDE="REGISTRY_REL=<不存在路径>" <cmd>
ANCHOR_OVERRIDE_ENV = "ASTROCS_STD_REG_ANCHOR_OVERRIDE"
ANCHOR_STALE_PROBE_REL = "docs/standards/__anchor_stale_probe__.md"
GOVERNANCE_DOMAIN_MARKER = "(跨域治理)"
GIT_TIMEOUT_S = 30

# 冻结域表（§2）：key -> 冻结标准版本字面量（版本不得按实现反推）
FROZEN_DOMAINS = [
    ("spherical-projection", "FITS WCS Paper I/II + SIP（Shupe et al. 2005）",
     "Paper I = A&A 395, 1061 (2002)；Paper II = A&A 395, 1077 (2002)；SIP = ASPC 347, 491 (2005)"),
    ("hips", "IVOA HiPS Recommendation 1.0（properties 修订 1.4）",
     "HiPS 1.0 (PR-HiPS-1.0-20161122) + properties hips_version=\"1.4\""),
    ("healpix", "Górski et al. 2005 HEALPix（NESTED）",
     "ApJ 622, 759 (2005)，bibcode 2005ApJ...622..759G"),
    ("drizzle", "Fruchter & Hook 2002 Drizzle",
     "PASP 114, 144 (2002)，bibcode 2002PASP..114..144F"),
    ("catalog", "Gaia DR3 data model（本地 XPSD 星表）",
     "Gaia DR3（Gaia Collaboration et al. 2023, A&A 674, A1）+ XPSD 本地编码合同"),
    ("fits", "FITS Standard 4.0",
     "FITS 4.0（IAU FWG，2016-07-22 批准版）"),
]
DOMAIN_KEYS = [d[0] for d in FROZEN_DOMAINS]
DOMAIN_STANDARD = {d[0]: d[1] for d in FROZEN_DOMAINS}
DOMAIN_VERSION = {d[0]: d[2] for d in FROZEN_DOMAINS}

REQUIRED_FIELDS = ["DOMAIN", "STANDARD", "VERSION", "CLAUSES", "COMPLIANCE",
                   "EVIDENCE", "DEVIATION"]
CHECKLIST_COLUMNS = ["条款", "标准要求", "符合状态", "证据指针", "偏差"]
DEVIATION_COLUMNS = ["偏差 ID", "严重度", "指针", "处置归属"]
INDEX_COLUMNS = ["偏差 ID", "域", "条款", "注册表清单行", "状态", "处置归属"]
LEGAL_STATUS = {"CONFORMANT", "PARTIAL", "NON_CONFORMANT", "PROJECT_DEFINED"}
DEVIATION_ID_RE = re.compile(r"^(?:STD-F\d+|DISP-[A-Z0-9]+-\d+)$")
NONE_MARKERS = {"（无）", "(无)", "NONE", "none", "无"}
# 2026-09-21 根目录整合：tests/tools/ci/contracts → eng/** 后，原式会在 "eng/tests/x" 里
# 从 "tests/" 起截出一段假路径（前置无边界）⇒ 真实存在的证据被判"不存在"。
# 修法：补 eng 前缀 + 前置边界（负向后顾），只准更精确、不准更宽松。
PATH_RE = re.compile(r"(?<![\w/.-])(?:docs|eng|lib|tests|tools|ci|modules|runtime|contracts|"
                     r"include|cli|scripts|工程控制|reports|artifacts|evidence|testdata)/"
                     r"[A-Za-z0-9_./\u4e00-\u9fff-]+")
ID_TOKEN_RE = re.compile(r"\b(?:STD-F\d+|DISP-[A-Z0-9]+-\d+)\b")
FIELD_RE = re.compile(r"^-\s*([A-Z]+)\s*[:：]\s*(.*)$")

# 注入锚（--fault-inject 用的逐字字面量；与注册表 §3/§3.2 现文一致）
IDX_SEP = "|---|---|---|---|---|---|"
NONE_MARKER_ROW = "| （无） | (跨域治理) | — | — | — | — |"
GOVERNANCE_ROW_STD_F6 = (
    "| STD-F6 | 已闭环（治理级/P1） | docs/standards/STANDARDS_REGISTRY.md（本注册表 §1.2/§3.1）；"
    "工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/05_FINDINGS_REGISTER_20260911.md（已删除，仅历史溯源） "
    "| STD-REG-001（本注册表建立即闭环；依据 = 原 05 号 findings 登记册 §STD-F6「国际标准冻结注册表缺失」处置面，"
    "2026-09-16 由 git 历史逐字取证迁移） |")

FAULT_SCENARIOS = [
    "drop-domain-section",
    "drop-checklist-table",
    "illegal-status",
    "version-drift",
    "drop-wcs003f1-pointer",
    "dangling-deviation-id",
    "drop-governance-deviation",
    "add-none-marker-row",
    "anchor-stale",
]


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def read_text(path: str) -> str:
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


# ---- 锚存活（ENGINEERING_SPEC §8） ------------------------------------------
def git_toplevel(root: str):
    """root 为 git 工作树根时返回其真实路径, 否则 None（跳过跟踪复核）。"""
    try:
        p = subprocess.run(["git", "-C", root, "rev-parse", "--show-toplevel"],
                           capture_output=True, text=True, timeout=GIT_TIMEOUT_S)
    except Exception:
        return None
    if p.returncode != 0:
        return None
    top = os.path.realpath(p.stdout.strip())
    return top if top == os.path.realpath(root) else None


def git_tracked(root: str, rel: str):
    """(True|False|None, 说明)：None = git 不可用/无法判定。"""
    try:
        p = subprocess.run(["git", "-C", root, "ls-files", "--error-unmatch",
                            "--", rel], capture_output=True, text=True,
                           timeout=GIT_TIMEOUT_S)
    except Exception as exc:
        return None, "git 调用失败: %s" % exc
    if p.returncode == 0:
        return True, "ls-files --error-unmatch rc=0"
    last = (p.stderr.strip().splitlines() or [""])[-1]
    return False, "ls-files --error-unmatch rc=%d (%s)" % (p.returncode, last[:120])


def anchor_overrides() -> dict:
    raw = os.environ.get(ANCHOR_OVERRIDE_ENV, "")
    out = {}
    for item in raw.split(","):
        if "=" in item:
            k, v = item.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def check_required_anchors(root: str) -> tuple:
    """必需锚存活：os.path.exists 必需 + （git 树根时）跟踪复核。返回 (checks, stale)。"""
    results, stale = [], []
    overrides = anchor_overrides()
    for name, rel in REQUIRED_ANCHORS:
        rel_eff = overrides.get(name, rel)
        note = "" if rel_eff == rel else "（注入覆盖）"
        full = os.path.join(root, rel_eff)
        if not os.path.exists(full):
            ok, detail = False, "路径不存在 (os.path.exists=False)%s" % note
        elif git_toplevel(root) is None:
            ok, detail = True, "存在（root 非 git 工作树根, 未做跟踪复核）%s" % note
        else:
            t_ok, why = git_tracked(root, rel_eff)
            if t_ok is False:
                ok, detail = False, "存在但 git 未跟踪: %s%s" % (why, note)
            elif t_ok is None:
                ok, detail = True, "存在（git 不可用, 未做跟踪复核: %s）%s" % (why, note)
            else:
                ok, detail = True, "存在 + git 跟踪 (%s)%s" % (why, note)
        results.append(check("anchor_alive_%s" % name, ok,
                             "%s: %s" % (rel_eff, detail)))
        if not ok:
            stale.append("ANCHOR_STALE: %s %s" % (name, rel_eff))
    return results, stale


# ---- 注册表解析 --------------------------------------------------------------
def split_sections(text: str) -> dict:
    """按 '## N ...' 顶级节切分（返回 {标题: 正文}）。"""
    out, cur, buf = {}, None, []
    for line in text.splitlines():
        if line.startswith("## "):
            if cur is not None:
                out[cur] = "\n".join(buf)
            cur, buf = line[3:].strip(), []
        elif cur is not None:
            buf.append(line)
    if cur is not None:
        out[cur] = "\n".join(buf)
    return out


def parse_domain_sections(sections: dict) -> dict:
    """§D.<key> 域节 -> {key: {"fields": {...}, "checklist": [rows], "deviations": [rows]}}"""
    domains = {}
    for title, body in sections.items():
        m = re.match(r"^D\.\s*([a-z0-9-]+)\b", title)
        if not m:
            continue
        key = m.group(1)
        fields, tables = {}, []
        for line in body.splitlines():
            fm = FIELD_RE.match(line.strip())
            if fm:
                fields[fm.group(1)] = fm.group(2).strip()
            if line.strip().startswith("|"):
                tables.append(line.strip())
        domains[key] = {"fields": fields, "tables": tables,
                        "checklist": parse_tables(tables, CHECKLIST_COLUMNS),
                        "deviations": parse_tables(tables, DEVIATION_COLUMNS)}
    return domains


def parse_tables(lines: list, header: list) -> list:
    """在原始表格行中找出以 header 起始的表格，返回数据行（单元格列表）。"""
    rows, i = [], 0
    while i < len(lines):
        cells = [c.strip() for c in lines[i].strip("|").split("|")]
        if cells == header:
            j = i + 2  # 跳过表头与分隔行
            while j < len(lines):
                rc = [c.strip() for c in lines[j].strip("|").split("|")]
                if len(rc) != len(header):
                    break
                rows.append(rc)
                j += 1
            i = j
            continue
        i += 1
    return rows


def section_table_rows(sections: dict, section_rx: str, header: list) -> tuple:
    """取匹配 section_rx 的顶级节内、以 header 起始的表 -> (节标题, 数据行)。"""
    titles = [t for t in sections if re.match(section_rx, t)]
    if not titles:
        return None, []
    lines = [l.strip() for l in sections[titles[0]].splitlines()
             if l.strip().startswith("|")]
    return titles[0], parse_tables(lines, header)


def parse_index_entry(root: str, rel: str) -> tuple:
    """返回 (登记为 ACTIVE_NORMATIVE?, 说明)。"""
    full = os.path.join(root, INDEX_REL)
    if not os.path.isfile(full):
        return False, "docs/DOCUMENT_INDEX.yaml 缺失"
    try:
        import yaml  # type: ignore
    except Exception:
        return False, "pyyaml 不可用"
    try:
        data = yaml.safe_load(read_text(full)) or {}
    except Exception as exc:  # pragma: no cover
        return False, "YAML 解析失败: %s" % exc
    active = ((data.get("doc_index") or {}).get("active") or [])
    for entry in active:
        if isinstance(entry, dict) and entry.get("path") == rel:
            return entry.get("status") == "ACTIVE_NORMATIVE", "status=%s" % entry.get("status")
    return False, "未登记于 doc_index.active"


def findings_ids(root: str) -> tuple:
    """外部 findings 登记册定义域 -> (ids, results, stale)。

    绝不把"文件不存在"当"无违规"静默返回空集（ENGINEERING_SPEC §8 fail-closed）：
      - 缺席且未声明必需 ⇒ 显式登记"缺席(可选来源)", 定义域由注册表自身承载；
      - 缺席且声明必需, 或存在但不可读 ⇒ ANCHOR_STALE（exit 2）。
    """
    full = os.path.join(root, EXTERNAL_FINDINGS_REL)
    if not os.path.isfile(full):
        if EXTERNAL_FINDINGS_REQUIRED:
            return set(), [check("C6_external_findings_source", False,
                                 "ANCHOR_STALE: %s 缺失（声明为必需来源）"
                                 % EXTERNAL_FINDINGS_REL)], \
                ["ANCHOR_STALE: EXTERNAL_FINDINGS_REL %s" % EXTERNAL_FINDINGS_REL]
        return set(), [check(
            "C6_external_findings_source", True,
            "缺席（可选来源：原 05 号 findings 登记册已随旧控制包世代清理删除；"
            "定义域 = 本注册表域偏差表 + §3.2 跨域治理偏差表, 非空集静默回退）")], []
    try:
        ids = set(ID_TOKEN_RE.findall(read_text(full)))
    except OSError as exc:
        return set(), [check("C6_external_findings_source", False,
                             "ANCHOR_STALE: 存在但不可读 (%s)" % exc)], \
            ["ANCHOR_STALE: EXTERNAL_FINDINGS_REL %s（不可读: %s）"
             % (EXTERNAL_FINDINGS_REL, exc)]
    return ids, [check("C6_external_findings_source", True,
                       "在位：%d 个 ID 并入闭包域" % len(ids))], []


def path_exists(root: str, ref: str) -> bool:
    ref = ref.rstrip(".,;:）)")
    full = os.path.join(root, ref)
    return os.path.exists(full)


def extract_paths(cell: str) -> list:
    return [p for p in PATH_RE.findall(cell)]


def evaluate(root: str, text: str = None) -> dict:
    results = []
    reg_full = os.path.join(root, REGISTRY_REL)
    if text is None:
        exists = os.path.isfile(reg_full)
        results.append(check("C1_registry_exists", exists, REGISTRY_REL))
        if not exists:
            return {"results": results, "verdict": "STANDARDS_REGISTRY_FAIL",
                    "domains": [], "deviations": [], "stale": []}
        text = read_text(reg_full)

    ok_index, detail_index = parse_index_entry(root, REGISTRY_REL)
    results.append(check("C1_registry_index_active_normative", ok_index, detail_index))

    sections = split_sections(text)
    domains = parse_domain_sections(sections)

    # C2 域齐备 + 域表逐行一致（key 集合 + 冻结 STANDARD/VERSION 逐字）
    domain_table = parse_tables([l.strip() for l in text.splitlines() if l.strip().startswith("|")],
                                ["域 key", "标准", "冻结版本", "标准条款面"])
    table_keys = [r[0] for r in domain_table]
    results.append(check("C2_domain_set_frozen",
                         sorted(table_keys) == sorted(DOMAIN_KEYS),
                         "域表=%s" % sorted(table_keys)))
    bad_rows = []
    for row in domain_table:
        if len(row) != 4:
            bad_rows.append("%s:列数=%d" % (row[0], len(row)))
            continue
        key, std, ver, _clauses = row
        if key not in DOMAIN_STANDARD:
            bad_rows.append("%s:非冻结域 key" % key)
            continue
        if std != DOMAIN_STANDARD[key]:
            bad_rows.append("%s:标准漂移 %s" % (key, std))
        if ver != DOMAIN_VERSION[key]:
            bad_rows.append("%s:版本漂移 %s" % (key, ver))
    results.append(check("C2_domain_table_values_frozen", not bad_rows,
                         "漂移行=%s" % bad_rows if bad_rows else "ok"))
    results.append(check("C2_domain_sections_present",
                         sorted(domains.keys()) == sorted(DOMAIN_KEYS),
                         "域节=%s" % sorted(domains.keys())))

    deviations_all = {}
    for key in DOMAIN_KEYS:
        dom = domains.get(key)
        if dom is None:
            continue
        f = dom["fields"]
        # C3 字段面
        missing = [n for n in REQUIRED_FIELDS if not f.get(n)]
        results.append(check("C3_fields_%s" % key, not missing,
                             "缺字段=%s" % missing if missing else "ok"))
        if f.get("DOMAIN") and f["DOMAIN"] != key:
            results.append(check("C3_domain_key_%s" % key, False,
                                 "DOMAIN=%s != 节名 %s" % (f["DOMAIN"], key)))
        if f.get("STANDARD") and f["STANDARD"] != DOMAIN_STANDARD[key]:
            results.append(check("C3_standard_frozen_%s" % key, False,
                                 "STANDARD 与冻结值不符: %s" % f["STANDARD"]))
        if f.get("VERSION") and f["VERSION"] != DOMAIN_VERSION[key]:
            results.append(check("C3_version_frozen_%s" % key, False,
                                 "VERSION 漂移: 注册表=%s 冻结=%s" % (f["VERSION"], DOMAIN_VERSION[key])))
        if f.get("CLAUSES") and "§" not in f["CLAUSES"]:
            results.append(check("C3_clauses_form_%s" % key, False, "CLAUSES 无 § 条款锚"))
        if f.get("COMPLIANCE") and f["COMPLIANCE"] not in LEGAL_STATUS:
            results.append(check("C3_compliance_legal_%s" % key, False,
                                 "COMPLIANCE=%s" % f["COMPLIANCE"]))
        for ref in extract_paths(f.get("EVIDENCE", "")):
            if not path_exists(root, ref):
                results.append(check("C3_evidence_path_%s" % key, False, "证据路径不存在: %s" % ref))

        # C4 符合性清单
        rows = dom["checklist"]
        results.append(check("C4_checklist_present_%s" % key, len(rows) >= 1,
                             "%d 行" % len(rows)))
        clause_ids = [r[0] for r in rows]
        results.append(check("C4_checklist_clause_unique_%s" % key,
                             len(clause_ids) == len(set(clause_ids)),
                             "重复=%s" % [c for c in clause_ids if clause_ids.count(c) > 1]))
        bad_status = [r[0] for r in rows if r[2] not in LEGAL_STATUS]
        results.append(check("C4_checklist_status_legal_%s" % key, not bad_status,
                             "非法状态行=%s" % bad_status if bad_status else "ok"))
        bad_ev = []
        for r in rows:
            refs = extract_paths(r[3])
            if not refs:
                bad_ev.append(r[0] + "(无路径指针)")
            for ref in refs:
                if not path_exists(root, ref):
                    bad_ev.append("%s:%s" % (r[0], ref))
        results.append(check("C4_checklist_evidence_paths_%s" % key, not bad_ev,
                             "坏指针=%s" % bad_ev if bad_ev else "ok"))
        bad_dev = [r[0] for r in rows if not r[4]]
        results.append(check("C4_checklist_deviation_column_%s" % key, not bad_dev,
                             "空偏差列=%s" % bad_dev if bad_dev else "ok"))

        # C5 偏差表
        drows = dom["deviations"]
        results.append(check("C5_deviation_table_present_%s" % key, len(drows) >= 1,
                             "%d 行" % len(drows)))
        ids = [r[0] for r in drows]
        results.append(check("C5_deviation_id_unique_%s" % key,
                             len(ids) == len(set(ids)),
                             "重复=%s" % [i for i in ids if ids.count(i) > 1]))
        bad_ids = [i for i in ids if i not in NONE_MARKERS and not DEVIATION_ID_RE.match(i)]
        results.append(check("C5_deviation_id_form_%s" % key, not bad_ids,
                             "非法 ID=%s" % bad_ids if bad_ids else "ok"))
        bad_ptr = [i for i, r in zip(ids, drows) if not r[2]]
        results.append(check("C5_deviation_pointer_nonempty_%s" % key, not bad_ptr,
                             "空指针=%s" % bad_ptr if bad_ptr else "ok"))
        for i, r in zip(ids, drows):
            if i in NONE_MARKERS:
                continue
            if i in deviations_all:
                results.append(check("C5_deviation_id_global_unique", False,
                                     "%s 同时出现于 %s 与 %s" % (i, deviations_all[i], key)))
            deviations_all[i] = key

    # C5' §3.2 跨域治理偏差表（跨域治理级 ID 的定义域）
    gov_title, gov_rows = section_table_rows(sections, r"^3\b", DEVIATION_COLUMNS)
    gov_ids = [r[0] for r in gov_rows if r[0] not in NONE_MARKERS]
    results.append(check("C5_governance_deviation_table_present", len(gov_rows) >= 1,
                         "节=%s 行数=%d" % (gov_title, len(gov_rows))))
    results.append(check("C5_governance_deviation_id_unique",
                         len(gov_ids) == len(set(gov_ids)),
                         "重复=%s" % [i for i in gov_ids if gov_ids.count(i) > 1]))
    bad_gov_ids = [i for i in gov_ids if not DEVIATION_ID_RE.match(i)]
    results.append(check("C5_governance_deviation_id_form", not bad_gov_ids,
                         "非法 ID=%s" % bad_gov_ids if bad_gov_ids else "ok"))
    bad_gov_ptr = [r[0] for r in gov_rows
                   if r[0] not in NONE_MARKERS and not r[2]]
    results.append(check("C5_governance_deviation_pointer_nonempty", not bad_gov_ptr,
                         "空指针=%s" % bad_gov_ptr if bad_gov_ptr else "ok"))
    cross = sorted(set(gov_ids) & set(deviations_all))
    results.append(check("C5_deviation_id_cross_table_unique", not cross,
                         "域/治理重号=%s" % cross if cross else "ok"))

    # C6 偏差 ID 闭包（闭包域 = 域偏差表 ∪ 跨域治理偏差表 ∪ 外部 findings 登记册（若存在））
    findings, ext_results, ext_stale = findings_ids(root)
    results.extend(ext_results)
    closure = set(deviations_all) | set(gov_ids) | findings
    dangling = sorted({i for i in ID_TOKEN_RE.findall(text) if i not in closure})
    results.append(check("C6_deviation_id_closure", not dangling,
                         "悬空引用=%s" % dangling if dangling else
                         "定义域=%d 域偏差 + %d 跨域治理偏差 + %d 外部 findings"
                         % (len(deviations_all), len(gov_ids), len(findings))))

    # C7 §3 偏差索引
    idx_title, idx_rows = section_table_rows(sections, r"^3\b", INDEX_COLUMNS)
    results.append(check("C7_deviation_index_present", bool(idx_title) and len(idx_rows) >= 1,
                         "%d 行" % len(idx_rows)))
    idx_ids = sorted(r[0] for r in idx_rows if r[0] not in NONE_MARKERS)
    defined = sorted(set(deviations_all) | set(gov_ids))
    results.append(check("C7_deviation_index_matches_domains",
                         idx_ids == defined,
                         "索引=%s 定义域(域偏差∪跨域治理)=%s" % (idx_ids, defined)))
    bad_idx = []
    for r in idx_rows:
        did, dom_key, clause = r[0], r[1], r[2]
        if did in NONE_MARKERS:
            continue
        if did in gov_ids:
            if dom_key != GOVERNANCE_DOMAIN_MARKER:
                bad_idx.append("%s:跨域治理 ID 的域列必须为 %s（实际 %s）"
                               % (did, GOVERNANCE_DOMAIN_MARKER, dom_key))
            elif not (clause and r[3] and r[4] and r[5]):
                bad_idx.append("%s:跨域治理索引行字段不全" % did)
            continue
        dom = domains.get(dom_key)
        if dom is None:
            bad_idx.append("%s:未知域 %s" % (did, dom_key))
            continue
        clause_ids = [x[0] for x in dom["checklist"]]
        if clause not in clause_ids:
            bad_idx.append("%s:域 %s 清单无条款 %s" % (did, dom_key, clause))
        field_dev = dom["fields"].get("DEVIATION", "")
        if did not in ID_TOKEN_RE.findall(field_dev):
            bad_idx.append("%s:域 %s 的 DEVIATION 字段未含该 ID" % (did, dom_key))
    results.append(check("C7_deviation_index_rows_resolve", not bad_idx,
                         "坏行=%s" % bad_idx if bad_idx else "ok"))

    # C9 域清单「偏差」列 ↔ 域 DEVIATION 字段反向一致（W4-A3；§8 注册表双向一致）
    #
    # 事由：C4 只判清单偏差列"非空"、C7 只判 §3 索引行 → 域 DEVIATION 字段。
    # 反向（清单行引用的偏差 ID → 必须出现在本域 DEVIATION 字段）此前**无人判**，
    # 于是"清单行写着 STD-F1 而 DEVIATION 字段把它删掉"可以整体绿 —— 正是
    # --fault-inject drop-wcs003f1-pointer 想验却验不到的形态。
    # 口径收窄：只取清单偏差列里的 ID 词元（ID_TOKEN_RE），不判自由文本；
    # "无（…）"-类文字（不含 ID 词元）自然放行。
    backref_bad = []
    for key in DOMAIN_KEYS:
        dom = domains.get(key)
        if dom is None:
            continue
        field_dev = dom["fields"].get("DEVIATION", "")
        field_ids = set(ID_TOKEN_RE.findall(field_dev))
        for row in dom["checklist"]:
            if len(row) < 5:
                continue
            for tok in ID_TOKEN_RE.findall(row[4]):
                if tok not in field_ids:
                    backref_bad.append("%s/%s: 清单偏差列引用 %s 但 DEVIATION 字段未含"
                                       % (key, row[0], tok))
    results.append(check("C9_checklist_deviation_backref", not backref_bad,
                         "反向失配=%s" % backref_bad if backref_bad else
                         "ok（%d 域清单偏差列 ⊆ 各域 DEVIATION 字段）" % len(DOMAIN_KEYS)))

    # C8 §3 索引与偏差登记面双向一致（（无）行只允许在定义域为空时出现）
    none_rows = [r[0] for r in idx_rows if r[0] in NONE_MARKERS]
    if not defined:
        results.append(check("C8_index_explicit_none_when_no_deviations",
                             bool(none_rows),
                             "全域无偏差 ⇒ §3 索引必须显式登记（无）行; 实际（无）行=%d"
                             % len(none_rows)))
    else:
        results.append(check("C8_index_explicit_none_when_no_deviations",
                             not none_rows,
                             "定义域 %d 条非空 ⇒ §3 索引不得出现（无）行; 实际=%s"
                             % (len(defined), none_rows)))

    passed = all(r["pass"] for r in results)
    return {"results": sorted(results, key=lambda r: r["check"]),
            "verdict": "STANDARDS_REGISTRY_PASS" if passed else "STANDARDS_REGISTRY_FAIL",
            "domains": sorted(domains.keys()),
            "deviations": sorted(deviations_all.keys()),
            "governance_deviations": sorted(gov_ids),
            "stale": ext_stale}


def _replace_once(text: str, pattern: str, repl: str, scenario: str) -> str:
    """确定性命中一次并替换；命中 0 次或 >1 次 ⇒ 立即报错（注入空转守卫）。

    W4-A3 事由：`drop-wcs003f1-pointer` 的原锚串与注册表正文漂移，`str.replace`
    命中 0 次却返回原文 ⇒ 注入静默空转，`--fault-inject` 变成"无论如何都绿"的
    假自证。本守卫把"注入未生效"从静默变成硬错误（exit 3 + stderr 点名场景）。
    """
    new, n = re.subn(pattern, repl, text, count=1)
    if n != 1:
        raise InjectedNoOp(
            "FAULT_INJECT_NOOP: 场景 %s 的注入锚点命中 %d 次（要求恰好 1 次）"
            "；注入未生效 ⇒ 拒绝以原文冒充注入结果。锚: %s"
            % (scenario, n, pattern))
    return new


class InjectedNoOp(SystemExit):
    """注入空转：以非零退出（3）暴露，不得被当作"注入已生效"。"""


def inject(text: str, scenario: str) -> str:
    if scenario == "drop-domain-section":
        return _replace_once(text, r"(?ms)^## D\.drizzle\b.*?(?=^## )", "", scenario)
    if scenario == "drop-checklist-table":
        m = re.search(r"^## D\.hips\b.*?(?=^## )", text, flags=re.S | re.M)
        if not m:
            raise InjectedNoOp(
                "FAULT_INJECT_NOOP: 场景 %s 找不到 §D.hips 节（注入未生效）" % scenario)
        seg = m.group(0)
        stripped = "\n".join(l for l in seg.splitlines()
                                    if not l.strip().startswith("|")) + "\n"
        if stripped == seg:
            raise InjectedNoOp(
                "FAULT_INJECT_NOOP: 场景 %s 未删除任何表格行（注入未生效）" % scenario)
        return _replace_once(text, re.escape(seg), stripped.replace("\\", "\\\\"), scenario)
    if scenario == "illegal-status":
        return _replace_once(text, r"\| CONFORMANT \|", "| PASS |", scenario)
    if scenario == "version-drift":
        # §2 域表 healpix 行的「冻结版本」列（实现漂移即版本漂移）
        return _replace_once(text, re.escape("| ApJ 622, 759 (2005)，bibcode 2005ApJ...622..759G |"),
                             "| ApJ 999, 1 (2099) |", scenario)
    if scenario == "drop-wcs003f1-pointer":
        # 只动域 DEVIATION 字段：去掉 STD-F1 指针（清单行与 §3 索引保留）。
        # W4-A3 订正：原锚串漏了 DISP-WCS-001（"- DEVIATION: STD-F1；DISP-WCS-008；…"），
        # 与现行注册表 :62 全文不符 ⇒ str.replace 命中 0 次 = **注入空转**（实测
        # inject() 返回文本与输入逐字节相同）。现改为"行首锚 + 只删 STD-F1 词元"
        # 的形态，并经 _replace_once 空转守卫（命中 0 次即报错退出，不得静默返回
        # 原文冒充"已注入"）。
        return _replace_once(text, r"(?m)^(- DEVIATION: )STD-F1；", r"\1", scenario)
    if scenario == "dangling-deviation-id":
        return _replace_once(text, r"\| STD-F4 \|", "| STD-F99 |", scenario)
    if scenario == "drop-governance-deviation":
        # 删掉 §3.2 的 STD-F6 定义行 ⇒ C6 悬空 + C7 索引失配 + C5' 表空
        return _replace_once(text, re.escape(GOVERNANCE_ROW_STD_F6 + "\n"), "", scenario)
    if scenario == "add-none-marker-row":
        # §3 索引插入（无）行（定义域非空）⇒ C8 判 FAIL（C7 已按 NONE 过滤, 不受影响）
        return _replace_once(text, re.escape(IDX_SEP), IDX_SEP + "\n" + NONE_MARKER_ROW, scenario)
    if scenario == "anchor-stale":
        # 锚失效由 ANCHOR_OVERRIDE_ENV 注入（见 main）；文本不变。
        return text
    raise SystemExit("unknown scenario: %s" % scenario)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--fault-inject", choices=FAULT_SCENARIOS + ["none"], default="none")
    args = ap.parse_args()
    root = os.path.abspath(args.root)

    anchor_results, anchor_stale = check_required_anchors(root)

    text = None
    if args.fault_inject != "none":
        if args.fault_inject == "anchor-stale":
            os.environ[ANCHOR_OVERRIDE_ENV] = "REGISTRY_REL=" + ANCHOR_STALE_PROBE_REL
            anchor_results, anchor_stale = check_required_anchors(root)
        else:
            reg_full = os.path.join(root, REGISTRY_REL)
            if not os.path.isfile(reg_full):
                out = {"verdict": "STANDARDS_REGISTRY_FAIL",
                       "fault_inject": args.fault_inject,
                       "results": [check("C1_registry_exists", False, REGISTRY_REL)]
                       + anchor_results,
                       "anchor_stale": anchor_stale}
                print(json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True))
                return 0
            text = inject(read_text(reg_full), args.fault_inject)

    ev = evaluate(root, text)
    stale = anchor_stale + ev.get("stale", [])
    for line in stale:
        sys.stderr.write(line + "\n")
    results = ev["results"] + anchor_results
    passed = all(r["pass"] for r in results)
    sha = None
    reg_full = os.path.join(root, REGISTRY_REL)
    if os.path.isfile(reg_full):
        sha = hashlib.sha256(open(reg_full, "rb").read()).hexdigest()
    out = {
        "tool": "docs/standards/checks/check_standards_registry.py",
        "version": "1.1.0",
        "task": "STD-REG-001",
        "root": root,
        "registry": REGISTRY_REL,
        "registry_sha256": sha,
        "fault_inject": args.fault_inject,
        "anchor_stale": stale,
        "results": results,
        "domains": ev.get("domains", []),
        "deviations": ev.get("deviations", []),
        "governance_deviations": ev.get("governance_deviations", []),
        "verdict": ("ANCHOR_STALE" if stale else
                    ("STANDARDS_REGISTRY_PASS" if passed else "STANDARDS_REGISTRY_FAIL")),
    }
    payload = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            fh.write(payload + "\n")
    print(payload)
    if stale:
        # 锚失效 = harness 级失败：按 ENGINEERING_SPEC §8 fail-closed 必须非零退出,
        # 不得因 --fault-inject 的"恒退出 0"协议被静默掩盖。
        return 2
    if args.fault_inject != "none":
        return 0
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
