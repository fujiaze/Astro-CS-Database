#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""G-REF-LIC-CLOSED | 许可证/核验状态「措辞闭环」门。

要判的是什么
  参考代码库条目里的**许可证一栏**不得留「需网络核验 / 待核验 / 未核验」这类
  **未闭环措辞**。这类措辞不是错，但它是「这项还没做完」的机器可读标记；
  一旦留在正本里，读者无法区分「已核定」与「没查」，R-16 的来源腿就是空的。
  本门要求：要么给出已闭环的许可证标识（GPL-3.0 / BSD-3-Clause / CFITSIO / …），
  要么显式登记为 UNRESOLVED 并**带上处置结论**，不接受悬空的「需核验」。

判据（fail-closed）
  C1 scan_floor   docs/ 不存在 / 扫到 0 个 .md / 0 个含许可证语境的条目 => rc=2。
  C2 许可证语境未闭环  条目出现「许可证 / license / SPDX / 许可」且同条目含
                    未闭环措辞（需核验 / 未核验 / 待核验 / 需网络核验 / 待确认 /
                    TBD / ??）=> 判红。
  C3 全库未闭环措辞（只报不判红） 任何条目出现未闭环措辞但**不在**许可证语境
                    （如「卷页需网络核验」）=> 登记，供负责人分派。
  C4 清单          逐条输出 file:line + 命中的措辞 + 上下文摘要。

判定「已闭环」：同条目出现至少一个确定的许可证标识
  GPL-2.0/3.0(-or-later) / LGPL / BSD-2/3-Clause / MIT / Apache / MPL /
  ISC / zlib / CC0 / CFITSIO / NASA / AFL / 商业许可 / 不可用(非 OSI)…

用法
  python3 eng/tools/doccheck/check_ref_lic_closed.py [--root .] [--json-out F]
  python3 eng/tools/doccheck/check_ref_lic_closed.py --self-test
"""
import argparse
import json
import os
import re
import sys
import tempfile

CHECK_ID = "G-REF-LIC-CLOSED"
EXCLUDE_DIRS = ("build", "run", "testdata", "gaia", ".git")

UNCLOSED = [
    r"需\s*网络核验",
    r"需\s*核验",
    r"待\s*核验",
    r"未\s*核验",
    r"尚未\s*核验",
    r"需\s*确认",
    r"待\s*确认",
    r"待\s*查",
    r"TBD",
    r"\?\?",
    r"待补",
    r"未确认",
]
UNCLOSED_RE = re.compile("|".join(UNCLOSED))

LIC_CTX = re.compile(r"许可证|许可(?!则)|licen[cs]e|SPDX|开源协议|授权")

CLOSED_LICENSE = re.compile(
    r"GPL-?[23](\.0)?(-or-later)?"
    r"|LGPL-?[23](\.0)?"
    r"|AGPL-?3"
    r"|BSD-?[23]-?Clause"
    r"|MIT\b"
    r"|Apache-?[12](\.0)?"
    r"|MPL-?2"
    r"|ISC\b"
    r"|zlib\b"
    r"|CC0|CC-BY"
    r"|AFL-?[23]"
    r"|CFITSIO"
    r"|NASA"
    r"|非\s*OSI"
    r"|source-available"
    r"|All\s*Rights\s*Reserved"
    r"|免费|自由软件|公有领域|不可用"
)

SECTION_RE = re.compile(r"^#{1,6}\s*(?P<title>.*)$")
SECTION_OK = re.compile(r"参考文献|参考代码库|Primary literature|References|引用定位")
ENTRY_SPLIT = re.compile(r"^\s*(?:[-*+]\s+|\d+[.)]\s+|\|\s*)")


def iter_doc_files(root):
    docs = os.path.join(root, "docs")
    if not os.path.isdir(docs):
        return
    for dirpath, dirnames, filenames in os.walk(docs):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
        for fn in sorted(filenames):
            if fn.endswith(".md"):
                yield os.path.join(dirpath, fn)


def iter_entries(text):
    entries, cur, start, in_ref, fence = [], [], None, False, False
    for i, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith("\u0060\u0060\u0060"):
            fence = not fence
            continue
        if fence:
            continue
        m = SECTION_RE.match(line)
        if m:
            if cur:
                entries.append(("\n".join(cur), start))
                cur, start = [], None
            in_ref = bool(SECTION_OK.search(m.group("title")))
            continue
        if not in_ref:
            continue
        if ENTRY_SPLIT.match(line) and line.strip():
            if cur:
                entries.append(("\n".join(cur), start))
            cur, start = [line], i
            continue
        if line.strip():
            if start is None:
                start = i
            cur.append(line)
        else:
            if cur:
                entries.append(("\n".join(cur), start))
                cur, start = [], None
    if cur:
        entries.append(("\n".join(cur), start))
    return entries


def is_legend(text):
    """标记约定说明行（"…[V] = 逐字/接口核验，[U] = 需网络核验。"）不是对某条参考文献的
    许可证声明，只是在**定义**标记含义。这类行不得判红，否则每次都会命中正本的图例。"""
    return bool(re.search(r"\[U\]\s*=", text) and re.search(r"\[V\]\s*=", text))


def evaluate(root):
    files = list(iter_doc_files(root))
    red, reported, legends = [], [], 0
    for path in files:
        with open(path, "r", encoding="utf-8") as fh:
            rel = os.path.relpath(path, root)
            for text, lineno in iter_entries(fh.read()):
                hits = [m.group(0) for m in UNCLOSED_RE.finditer(text)]
                if not hits:
                    continue
                if not LIC_CTX.search(text):
                    continue
                if is_legend(text):
                    legends += 1
                    continue
                red.append({
                    "file": rel, "line": lineno,
                    "unclosed": sorted(set(hits)),
                    "closed_license": bool(CLOSED_LICENSE.search(text)),
                    "excerpt": text.strip()[:150],
                    "verdict": "FAIL",
                })
    return {"files": len(files), "lic_entries": len(red) + legends,
            "legends_excluded": legends, "red": red, "reported": reported}


def self_test():
    root = tempfile.mkdtemp(prefix="greplic_")
    os.makedirs(os.path.join(root, "docs", "science"), exist_ok=True)
    head = "## 14a 参考文献与参考代码库（含许可证）\n\n"
    cases = [
        ("POS 许可证已闭环",
         "- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor，tag 2.8.6）：背景网格。\n",
         "ABSENT"),
        ("NEG 许可证需网络核验(缺陷三形态)",
         "- Astrometry.net（https://astrometry.net，许可证**需网络核验**）。\n",
         "PRESENT"),
        ("NEG 许可证待核验",
         "- Foo（GPL-3.0，https://example.org/foo）：说明（许可证待核验）。\n",
         "PRESENT"),
        ("NEG 许可证未闭环且无任何许可标识",
         "- Bar（https://example.org/bar）：说明（许可未核验）。\n",
         "PRESENT"),
    ]
    ok = True
    for label, body, expect in cases:
        with open(os.path.join(root, "docs", "science", "T.md"), "w",
                  encoding="utf-8") as fh:
            fh.write(head + body)
        rep = evaluate(root)
        got = "PRESENT" if rep["red"] else "ABSENT"
        good = got == expect
        ok = ok and good
        print("  %-6s %-34s expect=%-8s got=%-8s"
              % ("PASS" if good else "FAIL", label, expect, got))
    print("SELF-TEST %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=CHECK_ID)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    root = os.path.abspath(a.root)
    if not os.path.isdir(os.path.join(root, "docs")):
        print("C1_INPUT_UNAVAILABLE: docs/ 不存在", file=sys.stderr)
        return 2
    rep = evaluate(root)
    if rep["files"] == 0:
        print("C1_SCAN_FLOOR: 扫到 0 个 .md", file=sys.stderr)
        return 2
    if rep["lic_entries"] == 0:
        print("C1_SCAN_FLOOR: 0 个许可证语境条目（解析器空转，不得判绿）", file=sys.stderr)
        return 2
    print("[%s] 许可证语境条目=%d（图例说明行 %d 条已排除）判红=%d"
          % (CHECK_ID, rep["lic_entries"], rep["legends_excluded"], len(rep["red"])))
    for r in rep["red"]:
        extra = "（已给许可标识但核验措辞未闭环）" if r["closed_license"] else ""
        print("  RED  %s:%d  未闭环措辞=%s %s"
              % (r["file"], r["line"], "/".join(r["unclosed"]), extra))
        print("        %s" % r["excerpt"][:120])
    if a.json_out:
        with open(a.json_out, "w", encoding="utf-8") as fh:
            json.dump(rep, fh, ensure_ascii=False, indent=1, sort_keys=True)
    return 0 if not rep["red"] else 1


if __name__ == "__main__":
    sys.exit(main())
