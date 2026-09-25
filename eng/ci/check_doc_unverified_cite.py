#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-DOC-UNVERIFIED-CITE：被**未核实清单**点名的文献，不得出现在
`docs/science/**` 的**出处列**（一手来源位置）。

实例（审查给出的门）：
  * `Merline & Howell 1995` 在 `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md:167`
    自陈「（从未核实）」，却被 `docs/science/PHOTOMETRY.md:381` 的
    「一手出处（研究包 §7）」列当作一手来源引用。
  * **注意**：该条的 DOI 与卷页经 Crossref **对得上** —— 问题在**未核实**，不在错引。
    本门因此只判「未核实清单 ↔ 出处列」的交叉，不判引用是否写错。

权威依据
  * `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md` §0 记录规则：
    「无法核对来源的条目一律不收录」；§9「本轮明确**未**收录的条目（核对失败）」
    是**未核实清单**的正本形态；
  * `AGENTS.md` §5「判据必须非退化」、§6「不以环境问题掩盖失败」、
    §8「科学查证必须记录来源」。

判据（fail-closed）
  T1 未核实清单（默认 = 仓内 `docs/science/` **之外**所有 md 里被声明未核实的
     文献键）∩ `docs/science/` 出处列里出现的文献键 ≠ ∅ ⇒ 判红；
     判词带**两侧** `文件:行`。
  T2 「出处列」判定：markdown 表格的表头单元命中 出处/来源/文献/参考/引用/一手 ⇒ 该列
     为出处列；无表格时，取「参考文献/引用/出处/文献」节下的列表行。

豁免分支（显式、**逐行**；不得按整节豁免）
  E1 出处列同一单元格（或紧随其前的 60 字符内）自带未核实披露
     （`[UNVERIFIED]` / 未核实 / 未打开原文 / 未核到 / 待核对）⇒ 该行豁免
     —— 文档自己已经披露，这不算「当一手出处引用」。
  E2 引用出现在**非出处列**（例：仓内状态、用途列）⇒ 不判。
  E3 「参考文献 / 引用 / 出处 / 文献」节的标题**不豁免其正文**：把引用挪进这类小节
     但**不做行内披露**，照常判红（豁免是行长层面的，不是节层面的）。

用法
  python3 eng/ci/check_doc_unverified_cite.py [--json-out <path>]
  python3 eng/ci/check_doc_unverified_cite.py --corpus-dir <dir> [--no-default-corpus]
  python3 eng/ci/check_doc_unverified_cite.py --self-test
exit 0 = 无越界引用；1 = 判红；2 = 环境/用法错误。
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import re
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
MARKER = REPO / "eng/ci/check_cite_claim_mark_consistency.py"
DEFAULT_ROOTS = ("docs", "实验", "artifacts")
TARGET_ROOT = "docs/science"

SOURCE_HEADER_RE = re.compile(r"(出处|来源|文献|参考|引用|一手)")
REFLIST_HEADING_RE = re.compile(r"(参考文献|引用|出处|文献|参考资料)")
SEPARATOR_ROW_RE = re.compile(r"^\s*\|[\s:|-]+\|\s*$")
INLINE_DISCLOSURE = ("[UNVERIFIED]", "未核实", "未打开原文", "未核到", "待核对", "未核对")


def _load_marker():
    """共用 G1 的标记词表 / 文献键抽取 / 子句归因（判据口径必须与 G1 完全一致）。"""
    spec = importlib.util.spec_from_file_location("ccmc_shared", MARKER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    for sym in ("_mark_state", "_attribute", "_work_positions", "units_of", "iter_md_files"):
        if not hasattr(mod, sym):
            raise RuntimeError("共用件 %s 缺少符号 %s（fail-closed）" % (MARKER.name, sym))
    return mod


def _rel(p) -> str:
    try:
        return str(pathlib.Path(p).relative_to(REPO))
    except ValueError:
        return str(p)


def build_ledger(md, files, target_root: pathlib.Path):
    """未核实清单：{文献键: [(file, line, text)]}，**排除** docs/science 自身。"""
    ledger: dict = {}
    for f in files:
        try:
            if target_root in pathlib.Path(f).resolve().parents:
                continue
        except OSError:
            pass
        try:
            lines = pathlib.Path(f).read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        fenced = False
        for i, line in enumerate(lines, start=1):
            if md.FENCE_RE.match(line):
                fenced = not fenced
                continue
            if fenced:
                continue
            units, _row = md.units_of(line)
            for seg in units:
                keys, _narrow = md._attribute(seg, "u")
                for k in keys:
                    ledger.setdefault(k, []).append(
                        {"file": str(f), "line": i, "text": seg[:220]})
    return ledger


def source_columns(header_cells):
    return [i for i, c in enumerate(header_cells) if SOURCE_HEADER_RE.search(c)]


def scan_target(md, files, ledger, target_root: pathlib.Path):
    """在 docs/science/** 中找出处列里被未核实清单点名的文献。"""
    findings = []
    for f in files:
        try:
            if target_root not in pathlib.Path(f).resolve().parents:
                continue
        except OSError:
            continue
        try:
            lines = pathlib.Path(f).read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        fenced = False
        section = ""
        cols = []
        for i, line in enumerate(lines, start=1):
            if md.FENCE_RE.match(line):
                fenced = not fenced
                continue
            if fenced:
                continue
            m = re.match(r"^\s*#{1,6}\s+(.*)$", line)
            if m:
                section = m.group(1).strip()
                cols = []
                continue
            if md.TABLE_ROW_RE.match(line):
                nxt = lines[i] if i < len(lines) else ""
                if SEPARATOR_ROW_RE.match(nxt):
                    cols = source_columns(md.table_cells(line))
                    continue
                if not cols:
                    continue
                cells = md.table_cells(line)
                for ci in cols:
                    if ci >= len(cells):
                        continue
                    cell = cells[ci]
                    ctx = (line[max(0, line.find(cell) - 60):line.find(cell)] + cell)
                    if any(t in ctx for t in INLINE_DISCLOSURE):
                        continue  # E1 行内已披露
                    for key in sorted(md._work_positions(cell) and
                                      {k for k, _ in md._work_positions(cell)}):
                        if key in ledger:
                            findings.append(_finding(key, f, i, cell, ledger, "source_column",
                                                     section, ci))
                continue
            # 列表形态：参考文献/引用/出处 节下的条目行
            if REFLIST_HEADING_RE.search(section) and line.lstrip().startswith(("-", "*", "1.", "2.")):
                if any(t in line for t in INLINE_DISCLOSURE):
                    continue  # E1
                for key in sorted({k for k, _ in md._work_positions(line)}):
                    if key in ledger:
                        findings.append(_finding(key, f, i, line.strip(), ledger, "reflist_line",
                                                 section, None))
    return findings


def _finding(key, f, line_no, text, ledger, surface, section, col):
    srcs = ledger[key]
    sides = ["%s:%d" % (_rel(s["file"]), s["line"]) for s in srcs]
    return {
        "work": key,
        "target_side": "%s:%d" % (_rel(f), line_no),
        "unverified_list_side": sides[0],
        "unverified_list_sides": sides,
        "surface": surface,
        "column_index": col,
        "section": section,
        "judgment": "未核实清单点名的 %s 出现在 %s 的出处列：目标侧 %s ↔ 清单侧 %s"
                    % (key, _rel(f), "%s:%d" % (_rel(f), line_no), "、".join(sides[:3])),
        "samples": {"target": text[:220], "ledger": srcs[0]["text"]},
    }


def run(json_out: str = "", corpus_dirs=None, use_default_corpus: bool = True,
        target_root: pathlib.Path = None) -> int:
    md = _load_marker()
    target_root = target_root or (REPO / TARGET_ROOT)
    roots = []
    if use_default_corpus:
        roots += [REPO / r for r in DEFAULT_ROOTS]
    roots += [pathlib.Path(d) for d in (corpus_dirs or [])]
    files = md.iter_md_files(roots)
    if not files:
        print("DOC-UNVERIFIED-CITE_FAIL: 无任何语料（fail-closed）", file=sys.stderr)
        return 2
    target_files = [f for f in files if target_root in pathlib.Path(f).resolve().parents]
    if not target_files:
        print("DOC-UNVERIFIED-CITE_FAIL: 目标面 %s 下无 markdown（fail-closed）"
              % _rel(target_root), file=sys.stderr)
        return 2
    ledger = build_ledger(md, files, target_root)
    findings = scan_target(md, files, ledger, target_root)
    report = {
        "schema": "astrocs.doc-unverified-cite/v1",
        "target_root": _rel(target_root),
        "n_files": len(files),
        "n_target_files": len(target_files),
        "n_ledger_works": len(ledger),
        "n_findings": len(findings),
        "findings": findings,
        "verdict": "FAIL" if findings else "PASS",
    }
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("doc-unverified-cite: files=%d（目标面 %d）未核实清单条目=%d 越界引用=%d"
          % (len(files), len(target_files), len(ledger), len(findings)))
    for f in findings[:25]:
        print("  " + f["judgment"])
    print("DOC-UNVERIFIED-CITE_%s" % report["verdict"])
    return 1 if findings else 0


# ── 自检 ─────────────────────────────────────────────────────────────────────
_LEDGER_SAMPLE = ("## 9 本轮明确**未**收录的条目（核对失败）\n\n"
                  "- Merline & Howell 1995（从未核实）；\n"
                  "- SWarp 的 ASCL ID（[UNVERIFIED]）；\n")

_TARGET_TABLE = ("## 16.4 误差预算的构成与出处\n\n"
                 "| 预算项 | 一手出处（研究包 §7） | 仓内状态 |\n"
                 "|---|---|---|\n"
                 "| 光子噪声（源+天光）与读出/量化 | Mortara & Fowler 1981；"
                 "Merline & Howell 1995 | 由 variance/ivar 传播 |\n")


def _cases():
    out = []

    def add(name, expect_fail, files, note=""):
        with tempfile.TemporaryDirectory() as td:
            md = _load_marker()
            td = pathlib.Path(td)
            paths = []
            for rel, text in sorted(files.items()):
                p = td / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(text, encoding="utf-8")
                paths.append(p)
            target = td / "docs" / "science"
            target.mkdir(parents=True, exist_ok=True)
            ledger = build_ledger(md, paths, target)
            findings = scan_target(md, paths, ledger, target)
        out.append({"case": name, "expect": "FAIL" if expect_fail else "PASS",
                    "ok": bool(findings) == expect_fail,
                    "verdict": "FAIL" if findings else "PASS",
                    "problems": [x["judgment"] for x in findings[:3]], "note": note})

    add("clean_no_ledger_work_in_source_column", False, {
        "实验/shared/refs/bib.md": _LEDGER_SAMPLE,
        "docs/science/PHOTOMETRY.md": _TARGET_TABLE.replace(
            "Merline & Howell 1995", "Bessell & Murphy 2012"),
    }, "出处列引用的件不在未核实清单内 ⇒ 绿")
    add("neg_real_instance_merline_howell", True, {
        "实验/shared/refs/bib.md": _LEDGER_SAMPLE,
        "docs/science/PHOTOMETRY.md": _TARGET_TABLE,
    }, "真实形态：bib:167「从未核实」 ↔ PHOTOMETRY.md:381 出处列")
    add("neg_inside_own_exemption_branch", True, {
        "实验/shared/refs/bib.md": _LEDGER_SAMPLE,
        "docs/science/PHOTOMETRY.md": (
            "## 20 参考文献\n\n"
            "| 预算项 | 一手出处 | 用途 |\n|---|---|---|\n"
            "| 光子噪声 | Merline & Howell 1995 | 由 variance/ivar 传播 |\n"),
    }, "引用被挪进「参考文献」节（朴素实现会整节豁免）—— 无行内披露仍判红")
    add("exempt_inline_disclosure_line_level", False, {
        "实验/shared/refs/bib.md": _LEDGER_SAMPLE,
        "docs/science/PHOTOMETRY.md": (
            "| 预算项 | 一手出处（研究包 §7） | 仓内状态 |\n|---|---|---|\n"
            "| 光子噪声 | Merline & Howell 1995（[UNVERIFIED]，本轮未打开原文） | 待核 |\n"),
    }, "E1：出处列同一单元已做行内披露 ⇒ 豁免（对照面：去掉披露即判红）")
    add("not_in_source_column_not_flagged", False, {
        "实验/shared/refs/bib.md": _LEDGER_SAMPLE,
        "docs/science/PHOTOMETRY.md": (
            "| 预算项 | 一手出处（研究包 §7） | 仓内状态 |\n|---|---|---|\n"
            "| 光子噪声 | Bessell & Murphy 2012 | 与 Merline & Howell 1995 同族 |\n"),
    }, "被点名件出现在**非出处列** ⇒ 不判（列定位有效）")
    add("ledger_surface_excludes_science_itself", False, {
        "docs/science/PHOTOMETRY.md": (
            "## 20 参考文献（本轮未打开原文）\n\n"
            "| 预算项 | 一手出处 | 用途 |\n|---|---|---|\n"
            "| 光子噪声 | Merline & Howell 1995（未打开原文） | 待核 |\n"),
    }, "清单面排除 docs/science 自身：自陈未核实不构成自指判红（且行内已披露）")
    return out


def self_test(json_out: str = "") -> int:
    cases = _cases()
    ok = all(c["ok"] for c in cases)
    report = {"schema": "astrocs.doc-unverified-cite-selftest/v1", "n_cases": len(cases),
              "cases": cases, "verdict": "PASS" if ok else "FAIL"}
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for c in cases:
        print("[%s] %-44s expect=%s got=%s %s"
              % ("PASS" if c["ok"] else "FAIL", c["case"], c["expect"], c["verdict"],
                 str(c["problems"])[:170]))
    print("DOC-UNVERIFIED-CITE-SELFTEST_%s cases=%d" % (report["verdict"], len(cases)))
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json-out", default="")
    ap.add_argument("--corpus-dir", action="append", default=[])
    ap.add_argument("--no-default-corpus", action="store_true")
    ap.add_argument("--target-root", default="")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test(args.json_out)
    tr = pathlib.Path(args.target_root) if args.target_root else None
    return run(args.json_out, args.corpus_dir, not args.no_default_corpus, tr)


if __name__ == "__main__":
    raise SystemExit(main())
