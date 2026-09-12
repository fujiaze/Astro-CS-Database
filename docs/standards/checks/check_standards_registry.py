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
  C5  每域偏差表存在（无偏差域写「（无）」）、偏差 ID 唯一、
      POINTER 非空（禁止空指针）；
  C6  偏差 ID 全局闭包：正文出现的 STD-F*/DISP-* 引用必须在本注册表或
      05 号 findings 登记册中有定义（禁悬空指针）；
  C7  每域 DEVIATION 字段与 §3 偏差索引逐行一致，且索引行
      REGISTRY-CHECKLIST 指向的域/条款在对应清单中真实存在；
  C8  全域无偏差时必须在 §3 偏差索引显式登记「（无）」行（不得留空）。

用法：
  python3 docs/standards/checks/check_standards_registry.py [--root <repo>]
      [--json-out <file>] [--fault-inject <scenario>]

退出码：0 = PASS，1 = FAIL（--fault-inject 时恒退出 0，供负向注入自证使用）。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys

REGISTRY_REL = "docs/standards/STANDARDS_REGISTRY.md"
INDEX_REL = "docs/DOCUMENT_INDEX.yaml"
FINDINGS_REL = "工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/05_FINDINGS_REGISTER_20260911.md"

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
LEGAL_STATUS = {"CONFORMANT", "PARTIAL", "NON_CONFORMANT", "PROJECT_DEFINED"}
DEVIATION_ID_RE = re.compile(r"^(?:STD-F\d+|DISP-[A-Z0-9]+-\d+)$")
NONE_MARKERS = {"（无）", "(无)", "NONE", "none", "无"}
PATH_RE = re.compile(r"(?:docs|lib|tests|tools|ci|modules|runtime|contracts|include|cli|scripts|工程控制|reports|artifacts|evidence|testdata)/[A-Za-z0-9_./\u4e00-\u9fff-]+")
ID_TOKEN_RE = re.compile(r"\b(?:STD-F\d+|DISP-[A-Z0-9]+-\d+)\b")
FIELD_RE = re.compile(r"^-\s*([A-Z]+)\s*[:：]\s*(.*)$")

FAULT_SCENARIOS = [
    "drop-domain-section",
    "drop-checklist-table",
    "illegal-status",
    "version-drift",
    "drop-wcs003f1-pointer",
    "dangling-deviation-id",
]


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def read_text(path: str) -> str:
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


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


def findings_ids(root: str) -> set:
    full = os.path.join(root, FINDINGS_REL)
    if not os.path.isfile(full):
        return set()
    return set(ID_TOKEN_RE.findall(read_text(full)))


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
                    "domains": [], "deviations": []}
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

    # C6 偏差 ID 闭包
    findings = findings_ids(root)
    dangling = sorted({i for i in ID_TOKEN_RE.findall(text)
                       if i not in deviations_all and i not in findings})
    results.append(check("C6_deviation_id_closure", not dangling,
                         "悬空引用=%s" % dangling if dangling else "定义域=%d 条" % len(deviations_all)))

    # C7 §3 偏差索引
    idx_title = [t for t in sections if re.match(r"^3\b", t)]
    idx_rows = []
    if idx_title:
        lines = [l.strip() for l in sections[idx_title[0]].splitlines() if l.strip().startswith("|")]
        idx_rows = parse_tables(lines, ["偏差 ID", "域", "条款", "注册表清单行", "状态", "处置归属"])
    results.append(check("C7_deviation_index_present", bool(idx_title) and len(idx_rows) >= 1,
                         "%d 行" % len(idx_rows)))
    idx_ids = sorted(r[0] for r in idx_rows)
    results.append(check("C7_deviation_index_matches_domains",
                         idx_ids == sorted(deviations_all.keys()),
                         "索引=%s 域表=%s" % (idx_ids, sorted(deviations_all.keys()))))
    bad_idx = []
    for r in idx_rows:
        did, dom_key, clause = r[0], r[1], r[2]
        if did in NONE_MARKERS:
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

    passed = all(r["pass"] for r in results)
    return {"results": sorted(results, key=lambda r: r["check"]),
            "verdict": "STANDARDS_REGISTRY_PASS" if passed else "STANDARDS_REGISTRY_FAIL",
            "domains": sorted(domains.keys()),
            "deviations": sorted(deviations_all.keys())}


def inject(text: str, scenario: str) -> str:
    if scenario == "drop-domain-section":
        return re.sub(r"^## D\.drizzle\b.*?(?=^## )", "", text, flags=re.S | re.M)
    if scenario == "drop-checklist-table":
        m = re.search(r"^## D\.hips\b.*?(?=^## )", text, flags=re.S | re.M)
        seg = m.group(0)
        return text.replace(seg, "\n".join(l for l in seg.splitlines()
                                           if not l.strip().startswith("|")) + "\n")
    if scenario == "illegal-status":
        return text.replace("| CONFORMANT |", "| PASS |", 1)
    if scenario == "version-drift":
        # §2 域表 healpix 行的「冻结版本」列（实现漂移即版本漂移）
        return text.replace("| ApJ 622, 759 (2005)，bibcode 2005ApJ...622..759G |",
                            "| ApJ 999, 1 (2099) |", 1)
    if scenario == "drop-wcs003f1-pointer":
        # 只动域 DEVIATION 字段：去掉 WCS-003-F1 指针（清单行与 §3 索引保留）
        return text.replace(
            "- DEVIATION: STD-F1；DISP-WCS-001；DISP-WCS-006；DISP-P3PROJ-001",
            "- DEVIATION: DISP-WCS-001；DISP-WCS-006；DISP-P3PROJ-001", 1)
    if scenario == "dangling-deviation-id":
        return text.replace("| STD-F4 |", "| STD-F99 |", 1)
    raise SystemExit("unknown scenario: %s" % scenario)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--fault-inject", choices=FAULT_SCENARIOS + ["none"], default="none")
    args = ap.parse_args()
    root = os.path.abspath(args.root)

    text = None
    if args.fault_inject != "none":
        reg_full = os.path.join(root, REGISTRY_REL)
        if not os.path.isfile(reg_full):
            out = {"verdict": "STANDARDS_REGISTRY_FAIL", "fault_inject": args.fault_inject,
                   "results": [check("C1_registry_exists", False, REGISTRY_REL)]}
            print(json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True))
            return 0
        text = inject(read_text(reg_full), args.fault_inject)

    ev = evaluate(root, text)
    sha = None
    reg_full = os.path.join(root, REGISTRY_REL)
    if os.path.isfile(reg_full):
        sha = hashlib.sha256(open(reg_full, "rb").read()).hexdigest()
    out = {
        "tool": "docs/standards/checks/check_standards_registry.py",
        "version": "1.0.0",
        "task": "STD-REG-001",
        "root": root,
        "registry": REGISTRY_REL,
        "registry_sha256": sha,
        "fault_inject": args.fault_inject,
        "results": ev["results"],
        "domains": ev.get("domains", []),
        "deviations": ev.get("deviations", []),
        "verdict": ev["verdict"],
    }
    payload = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            fh.write(payload + "\n")
    print(payload)
    if args.fault_inject != "none":
        return 0
    return 0 if ev["verdict"] == "STANDARDS_REGISTRY_PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
