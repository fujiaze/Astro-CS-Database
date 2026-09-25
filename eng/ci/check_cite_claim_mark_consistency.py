#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-CITE-CLAIM-MARK-CONSISTENCY：同一文献条目的「已核实类标记」与
「未打开原文/从未核实」声明**不得并存**（跨文件同一件同理）。

判据来源
  * `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md` §0「记录规则」（逐字）：
    「每条必须给**可核对标识** + **借鉴点** + **不借鉴点** + **场景差异**。
    **禁止编造引用。** 无法核对来源的条目一律不收录」；标记词表
    `[CR]` / `[arXiv]` / `[page]` / `[ADS]` / `[UNVERIFIED]`，
    核对状态 = 已核对 / 待核对（注明核对方式）；
  * `AGENTS.md` §5（判据必须非退化）、§8（查证记录证据来源）。

判据（fail-closed）
  T1 同一文献条目（归一化键 = 首作者姓氏 + 年份，兼容「Surname」/「Da Costa」/
     「Surname et al. YYYY」/「A & B YYYY」）出现在 **已核实侧**（该行带已核实类标记
     或已核对类判词）与 **未核实侧**（该行/该条目声明未打开原文、从未核实、未核到、
     待核对、不引用、`[UNVERIFIED]`）⇒ 判红。
  T2 同一行同时带已核实类标记与未核实声明 ⇒ 判红（同行自相矛盾）。
  判词一律带**两侧** `文件:行`。

豁免分支（显式、逐行；**不得**按整节豁免）
  E1 代码围栏（三反引号/波浪线）内的文本是规则模板与示例，不参与两侧抽取。
  E2 「未核实声明」这一侧只由声明行贡献 —— 声明行不因为行内出现标记词而被当作已核实主张。
  E3 负面清单/未收录节的**标题不豁免其正文**：该节内若出现带已核实类标记的条目，
     T1/T2 照常判红（E2 只豁免「声明侧」，不豁免「配对」）。
  行内代码（单反引号）**不作剥离**：本项目真实标记就写在行内代码里
  （例：`REVERSE_VERIFY_BIBLIOGRAPHY.md:77` 的 ``[ADS]``）。

用法
  python3 eng/ci/check_cite_claim_mark_consistency.py [--json-out <path>]
  python3 eng/ci/check_cite_claim_mark_consistency.py --corpus-dir <dir> [--no-default-corpus]
  python3 eng/ci/check_cite_claim_mark_consistency.py --self-test
exit 0 = 无并存；1 = 判红；2 = 环境/用法错误。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_ROOTS = ("docs", "实验", "artifacts", "eng")

VERIFIED_MARK_TOKENS = ("CR", "DOI", "ADS", "S", "arXiv", "page", "ARXIV")
VERIFIED_RE = re.compile(r"\[(?:%s)(?:\s*,\s*(?:%s))*\]"
                         % ("|".join(VERIFIED_MARK_TOKENS), "|".join(VERIFIED_MARK_TOKENS)))
VERIFIED_PROSE = ("已核对", "已逐字核对", "逐字核对", "已核到", "已核实")

UNVERIFIED_RE = re.compile(r"\[UNVERIFIED\]")
# 词表只收「未打开原文 / 未核实」这一类**核对状态**声明。
# 刻意**不收**「不引用 / 不得引用」——那是「借鉴点/不借鉴点」判断，出现在已核实条目上也会命中，
# 会把判据变成恒真门（AGENTS.md §5：判据必须非退化）。
UNVERIFIED_PROSE = ("未打开原文", "从未核实", "未核到", "未核实", "待核对", "核对失败",
                    "ADS 需 token", "未读全文", "未核对", "未取得逐字摘录")

FENCE_RE = re.compile(r"^\s*(?:`{3,}|~{3,})")
NEGATIVE_SECTION_RE = re.compile(r"(未收录|未核实|负面清单|核对失败|豁免|不引用|不得引用|待核对)")

_PERSON_WORD = r"[A-Z][A-Za-z'\-]{2,}"
_PA = r"%s(?:\s+%s)?" % (_PERSON_WORD, _PERSON_WORD)  # 支持复姓 "Da Costa"
WORK_PATTERNS = [
    re.compile(r"\b(%s)\s+(?:&|and)\s+%s\s*,?\s*\(?(\d{4})" % (_PA, _PA)),
    re.compile(r"\b(%s)\s+et\s+al\.?\s*,?\s*\(?(\d{4})" % _PA),
    re.compile(r"\b(%s)\s*\(\s*(\d{4})" % _PA),
    re.compile(r"\b(%s)\s*,\s*(\d{4})" % _PA),
    re.compile(r"\b(%s)\s+(\d{4})\b" % _PA),
]
NULL_AUTHORS = {"The", "This", "See", "Note", "Item", "Table", "Fig", "Row", "And", "But", "For",
                "With", "When", "Where", "Since", "However", "Because", "Both", "Each", "All",
                "One", "Two", "Three", "No", "Yes", "If", "Then", "Also", "Thus", "Hence",
                "Section", "Appendix", "Equation", "Version", "Rule", "Rules", "Level", "Phase",
                "Step", "Type", "Phase1", "Phase2", "Phase3", "Only", "More", "Less", "Same",
                "Data", "File", "Files", "Input", "Output", "Result", "Results", "Case", "Given"}


def normalize_key(surname: str, year: str) -> str:
    return "%s %s" % (" ".join(surname.lower().split()), year)


def extract_works(text: str) -> set:
    """键 = **首作者**姓氏 + 年份（「A & B YYYY」只记 A，避免把 B 当成独立条目）。"""
    out = set()
    for idx, pat in enumerate(WORK_PATTERNS):
        for m in pat.finditer(text):
            surname, year = m.group(1), m.group(2)
            if any(w in NULL_AUTHORS for w in surname.split()):
                continue
            if idx >= 3:  # 松模式（"Surname, YYYY" / "Surname YYYY"）：排除合著者位
                lead = text[max(0, m.start() - 6):m.start()]
                if re.search("(?:&|and)\\s*$", lead):
                    continue
            out.add(normalize_key(surname, year))
    return out


def iter_md_files(roots):
    seen = []
    for r in roots:
        p = pathlib.Path(r)
        if p.is_file() and p.suffix == ".md":
            seen.append(p)
        elif p.is_dir():
            seen.extend(sorted(p.rglob("*.md")))
    return sorted(set(seen))


# 段分隔：markdown 表格单元（|）、中文/英文分号、句号。
# 归因必须在**段**这一级做：一行里可以同时出现「某条目已核实」与「另一条目未核实」
# （例：@@BT@@实验/absolute-snr/docs/surveys/f-instr-survey.md:178@@BT@@），
# 行级归因会把两者混成一条恒真判红。
SEG_SPLIT_RE = re.compile(r"[|；;。]")


def segments(line: str) -> list:
    out = []
    for s in SEG_SPLIT_RE.split(line):
        s = s.strip()
        if s:
            out.append(s)
    return out


# 「子条目级」限定语：@@BT@@[UNVERIFIED]@@BT@@ 紧跟在这些词之后就只否定那个**细节**
# （等式号/页码/章节…），不否定**该文献条目**的核对状态。
# 依据：§0 记录规则问的是「条目是否可核对」；把「等式号未核」读成「条目未核实」是把
# 判据放大成恒真门（AGENTS.md §5）。这类命中**不静默丢弃**，单列 narrow_scope 上报。
NARROW_SCOPE_RE = re.compile(r"(等式号|公式号|页码|表格|附录|章节号|Eq\.|Equation|Sec\.|§|page|Table|Appendix)")

STRONG_UNVERIFIED_PROSE = ("未打开原文", "从未核实", "未核到", "未核实", "待核对",
                           "核对失败", "ADS 需 token", "未核对", "未取得逐字摘录")


def _find_all(text: str, needle: str) -> list:
    out, i = [], text.find(needle)
    while i >= 0:
        out.append(i)
        i = text.find(needle, i + 1)
    return out


def _mark_positions(text: str, kind: str) -> list:
    """该单元内「已核实标记 / 未核实声明」的字符位置。"""
    pos = []
    if kind == "v":
        pos += [m.start() for m in VERIFIED_RE.finditer(text)]
        for p in VERIFIED_PROSE:
            pos += _find_all(text, p)
    else:
        pos += [m.start() for m in UNVERIFIED_RE.finditer(text)]
        for p in UNVERIFIED_PROSE:
            pos += _find_all(text, p)
    return sorted(set(pos))


def _work_positions(text: str) -> list:
    """[(文献键, 字符位置)]；键 = 首作者姓氏 + 年份。"""
    out = []
    for idx, pat in enumerate(WORK_PATTERNS):
        for m in pat.finditer(text):
            surname, year = m.group(1), m.group(2)
            if any(w in NULL_AUTHORS for w in surname.split()):
                continue
            if idx >= 3:  # 松模式：排除 "A & B YYYY" 里的合著者位
                lead = text[max(0, m.start() - 6):m.start()]
                if re.search("(?:&|and)\\s*$", lead):
                    continue
            out.append((normalize_key(surname, year), m.start()))
    return out


# 子句边界：声明的辖域是它所在的**子句**，不是整行/整单元。
CLAUSE_BOUNDARY = "。；;⇒|"


def _clause_span(text: str, pos: int):
    left = 0
    for b in CLAUSE_BOUNDARY:
        j = text.rfind(b, 0, pos)
        left = max(left, j + 1)
    right = len(text)
    for b in CLAUSE_BOUNDARY:
        j = text.find(b, pos)
        if j >= 0:
            right = min(right, j)
    return left, right


def _attribute(text: str, kind: str):
    """把标记/声明归因到**同子句内最近的**文献键。返回 (keys, 是否有子条目级命中)。

    「同子句 + 最近优先」而不是「整单元全归因」的理由（都是实测反例）：
      * `f-instr-canon.md:489` 单元里先声明「未核到」，子句结束，后半句才把
        Bessell & Murphy 2012 当**替代依据**提及 —— 整单元归因会把它误判成未核实；
      * `f-instr-survey.md:35` 同一单元里 `[UNVERIFIED]` 只否定紧跟
        其前的 "ACS ISR 2006-01"，前面的 Anderson & King 2000 是**已核替代引用**。
    """
    marks = _mark_positions(text, kind)
    works = _work_positions(text)
    keys, narrow = set(), False
    for mp in marks:
        if kind == "u" and not any(p in text for p in STRONG_UNVERIFIED_PROSE):
            if NARROW_SCOPE_RE.search(text[max(0, mp - 14):mp]):
                narrow = True
                continue
        lo, hi = _clause_span(text, mp)
        pool = [w for w in works if lo <= w[1] < hi]
        if not pool:
            continue
        before = [w for w in pool if w[1] <= mp]
        best = max(before, key=lambda w: w[1]) if before else min(pool, key=lambda w: w[1])
        keys.add(best[0])
    return keys, narrow


def _mark_state(text: str):
    """整单元级：(是否出现已核实标记, 是否出现未核实声明, 是否仅子条目级未核实)。"""
    v = bool(VERIFIED_RE.search(text)) or any(p in text for p in VERIFIED_PROSE)
    u = bool(UNVERIFIED_RE.search(text)) or any(p in text for p in UNVERIFIED_PROSE)
    narrow = False
    if u and not any(p in text for p in STRONG_UNVERIFIED_PROSE):
        m = UNVERIFIED_RE.search(text)
        if m and NARROW_SCOPE_RE.search(text[max(0, m.start() - 14):m.start()]):
            narrow = True
            u = False
    return v, u, narrow


TABLE_ROW_RE = re.compile(r"^\s*\|.*\|\s*$")


def table_cells(line: str) -> list:
    body = line.strip()
    if body.startswith("|"):
        body = body[1:]
    if body.endswith("|"):
        body = body[:-1]
    return [c.strip() for c in body.split("|") if c.strip()]


def units_of(line: str):
    """把一行切成**归因单元**，并给出可选的整行文本。

    归因单元（标记与文献名必须在同一单元内才算归因）：
      * markdown 表格行 ⇒ **单元格**。同一行的不同单元各说各话
        （例 @@BT@@REVERSE_VERIFY_BIBLIOGRAPHY.md:126@@BT@@：一个单元登记
        PhotometricMosaic 未核实，另一个单元把 Jacob et al. 2010 当**替代引用**提及 ——
        行级归因会把后者误判成未核实）；
      * 其余文本 ⇒ 段（| ； ; 。）。同一行可以并列「A 已核实」与「B 未核实」
        （例 @@BT@@f-instr-survey.md:178@@BT@@）。

    整行文本只用于 T2：整行同时出现两侧标记**且该行只有一个文献键**时，
    该行是一个无歧义的自相矛盾条目。
    """
    if TABLE_ROW_RE.match(line):
        return table_cells(line), line.strip()
    return segments(line), None


def scan(files):
    verified: dict = {}
    unverified: dict = {}
    same_line: list = []
    narrow_scope: list = []
    for f in files:
        try:
            lines = f.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        fenced = False
        section = ""
        for i, line in enumerate(lines, start=1):
            if FENCE_RE.match(line):
                fenced = not fenced
                continue
            if fenced:
                continue
            m = re.match(r"^\s*#{1,6}\s+(.*)$", line)
            if m:
                section = m.group(1).strip()
            in_neg = bool(NEGATIVE_SECTION_RE.search(section))
            units, row_text = units_of(line)
            for seg in units:
                v, u, _ = _mark_state(seg)
                if not (v or u):
                    continue
                vk, _nv = _attribute(seg, "v")
                uk, narrow = _attribute(seg, "u")
                if narrow and uk:
                    narrow_scope.append({"file": str(f), "line": i, "works": sorted(uk),
                                         "text": seg[:220], "section": section})
                if not (vk or uk):
                    continue
                rec = {"file": str(f), "line": i, "works": sorted(vk | uk),
                       "text": seg[:220], "section": section, "in_negative_section": in_neg}
                if v and u:
                    same_line.append(dict(rec, scope="unit"))
                for w in uk:
                    unverified.setdefault(w, []).append(rec)
                for w in vk - uk:
                    verified.setdefault(w, []).append(rec)
            if row_text:
                v, u, _ = _mark_state(row_text)
                works = extract_works(row_text)
                if v and u and len(works) == 1:
                    same_line.append({"file": str(f), "line": i, "works": sorted(works),
                                      "text": row_text[:220], "section": section,
                                      "in_negative_section": in_neg,
                                      "scope": "single_work_row"})
    dedup = {}
    for r in same_line:
        dedup[(r["file"], r["line"], r["scope"])] = r
    return verified, unverified, list(dedup.values()), narrow_scope


def _rel(p) -> str:
    try:
        return str(pathlib.Path(p).relative_to(REPO))
    except ValueError:
        return str(p)


def loc(rec) -> str:
    return "%s:%d" % (_rel(rec["file"]), rec["line"])


def verdicts(verified, unverified, same_line) -> list:
    out = []
    for k in sorted(set(verified) & set(unverified)):
        vs, us = verified[k], unverified[k]
        out.append({
            "kind": "cross_side_same_work",
            "work": k,
            "verified_side": [loc(r) for r in vs],
            "unverified_side": [loc(r) for r in us],
            "in_negative_section": any(r["in_negative_section"] for r in vs + us),
            "judgment": "同一文献条目 %s 并存：已核实侧 %s ↔ 未核实侧 %s"
                        % (k, "、".join(loc(r) for r in vs[:3]),
                           "、".join(loc(r) for r in us[:3])),
            "samples": {"verified": vs[0]["text"], "unverified": us[0]["text"]},
        })
    for r in same_line:
        out.append({
            "kind": "same_line",
            "work": ",".join(r["works"]),
            "verified_side": [loc(r)],
            "unverified_side": [loc(r)],
            "in_negative_section": r["in_negative_section"],
            "judgment": "同一%s并存已核实类标记与未核实声明（自相矛盾）：%s"
                        % ("个单文献行的整行" if r.get("scope") == "single_work_row" else "单元",
                           loc(r)),
            "scope": r.get("scope", "unit"),
            "samples": {"verified": r["text"], "unverified": r["text"]},
        })
    return out


def run(json_out: str = "", corpus_dirs=None, use_default_corpus: bool = True) -> int:
    roots = []
    if use_default_corpus:
        roots += [REPO / r for r in DEFAULT_ROOTS]
    roots += [pathlib.Path(d) for d in (corpus_dirs or [])]
    files = iter_md_files(roots)
    if not files:
        print("CITE-CLAIM-MARK-CONSISTENCY_FAIL: 无任何语料（fail-closed）", file=sys.stderr)
        return 2
    verified, unverified, same_line, narrow_scope = scan(files)
    findings = verdicts(verified, unverified, same_line)
    report = {
        "schema": "astrocs.cite-claim-mark-consistency/v1",
        "n_files": len(files),
        "n_works_verified": len(verified),
        "n_works_unverified": len(unverified),
        "n_findings": len(findings),
        "findings": findings,
        "narrow_scope_not_counted": narrow_scope,
        "narrow_scope_note": "仅「子条目（等式号/页码…）未核实」，不计入条目级并存判据；单列以便复核",
        "verdict": "FAIL" if findings else "PASS",
    }
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("cite-claim-mark-consistency: files=%d 已核实条目=%d 未核实条目=%d 并存判红=%d"
          % (len(files), len(verified), len(unverified), len(findings)))
    for f in findings[:25]:
        print("  " + f["judgment"])
    print("CITE-CLAIM-MARK-CONSISTENCY_%s" % report["verdict"])
    return 1 if findings else 0


# ── 自检（红/绿双向；含「落在自身豁免分支内」的负例） ─────────────────────────
def _write(td, files: dict):
    td = pathlib.Path(td)
    paths = []
    for name, text in sorted(files.items()):
        p = td / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
        paths.append(p)
    return paths


def _cases():
    out = []

    def add(name, expect_ok, files, note=""):
        with tempfile.TemporaryDirectory() as td:
            v, u, s, _n = scan(_write(td, files))
            f = verdicts(v, u, s)
        out.append({"case": name, "expect_ok": expect_ok, "ok": (not f) == expect_ok,
                    "verdict": "FAIL" if f else "PASS",
                    "problems": [x["judgment"] for x in f[:3]], "note": note})

    add("clean_distinct_works", True, {
        "a.md": "| 1.1 | Padmanabhan et al. 2008, ApJ 674, 1217. [arXiv] | 联合解算 |\n",
        "b.md": "## 9 未收录\n- SWarp 的 ASCL ID（[UNVERIFIED]）；\n"
                "- Trujillo & Fliri 2016（未打开原文）；\n",
    }, "已核实条目与未核实条目互不相交 ⇒ 绿")
    add("neg_same_work_both_sides", False, {
        "bib.md": "## 1 文献\n| 1.1 | Da Costa 1992, ASP Conf. Ser. 23, 90. [ADS] | MMM |\n",
        "exp.md": "**未找到一手来源的项（负面清单，不引用）**：Da Costa 1992"
                  "（ASP Conf. Ser. 23, 90，ADS 需 token，未打开原文）；\n",
    }, "真实形态：bibliography:77 的 [ADS] ↔ EXP-02:389 的未打开原文")
    add("neg_inside_own_exemption_branch", False, {
        "neg.md": "## 9 本轮明确**未**收录的条目（核对失败）\n"
                  "- Da Costa 1992, ASP Conf. Ser. 23, 90. [ADS]（本行仍带已核实标记）\n"
                  "- Da Costa 1992（ADS 需 token，未打开原文）；\n",
    }, "落在自身豁免分支（未收录/负面清单节）内 —— 豁免只豁免声明侧，节内配对仍判红")
    add("neg_same_line_contradiction", False, {
        "neg2.md": "| 2.1 | Merline & Howell 1995 [CR] | 未打开原文 |\n",
    }, "同一行并存 ⇒ 判红")
    add("exempt_fenced_block_not_flagged", True, {
        "ok.md": "示例：\n\n" + "```" + "\n"
                 "Da Costa 1992 [ADS]\n"
                 "Da Costa 1992（未打开原文）\n" + "```" + "\n",
    }, "E1 代码围栏豁免有效（对照面：同一配对在围栏外必红）")
    add("no_mark_means_no_claim", True, {
        "x.md": "Da Costa 1992, ASP Conf. Ser. 23, 90.\n",
        "y.md": "Da Costa 1992（ADS 需 token，未打开原文）\n",
    }, "无已核实标记 ⇒ 该条目只进未核实侧（判红来自标记，不来自关键词自命中）")
    add("inline_code_mark_still_detected", False, {
        "bib.md": "| 1.1 | Da Costa 1992, ASP Conf. Ser. 23, 90. `[ADS]` | MMM |\n",
        "exp.md": "Da Costa 1992（ADS 需 token，未打开原文）；\n",
    }, "标记写在行内代码里也必须被检出（与仓内真实写法一致）")
    return out


def self_test(json_out: str = "") -> int:
    cases = _cases()
    ok = all(c["ok"] for c in cases)
    report = {"schema": "astrocs.cite-claim-mark-consistency-selftest/v1",
              "n_cases": len(cases), "cases": cases,
              "verdict": "PASS" if ok else "FAIL"}
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for c in cases:
        print("[%s] %-38s expect=%s got=%s %s"
              % ("PASS" if c["ok"] else "FAIL", c["case"],
                 "PASS" if c["expect_ok"] else "FAIL", c["verdict"],
                 str(c["problems"])[:170]))
    print("CITE-CLAIM-MARK-CONSISTENCY-SELFTEST_%s cases=%d" % (report["verdict"], len(cases)))
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json-out", default="")
    ap.add_argument("--corpus-dir", action="append", default=[])
    ap.add_argument("--no-default-corpus", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test(args.json_out)
    return run(args.json_out, args.corpus_dir, not args.no_default_corpus)


if __name__ == "__main__":
    raise SystemExit(main())
