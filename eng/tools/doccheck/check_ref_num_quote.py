#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""G-REF-NUM-QUOTE | 数值型断言「逐字一手引用」门。

要判的是什么
  文档给出的**高精度数值常量**（科学常数、拟合系数、字符数、换算因子）必须能
  指回一手来源，而且必须**标明它是逐字取来的**还是**本项目自产/教科书恒等式**。
  两者都不是的数字不可信 —— 它既没说从哪抄的，也没说自己算的。

判据（fail-closed）
  N1 选取面   取**块**（bullet / numbered / 段落）而非字符窗口：块内同时出现
              ① 高精度数值字面量（>=6 位有效数字，或科学计数法 >=4 位）与
              ② 一手来源锚（DOI / arXiv / URL / ISBN / 官方文档路径）=> 进分母。
  N2 粒度声明  N1 块内必须再出现**逐字标记**（逐字 / 原文含 / 逐字命中 / verbatim /
              verbatim quote / 逐字符）或**身份标记**（Project-defined / 教科书 /
              恒等式 / FITS Standard / 本仓自产 / 实测 / Monte Carlo / 自产 MC）
              之一。两者皆无 => 判红（数值无一手引用、也未标来源身份）。
  N3 清单     逐块输出 file:line + 常量 + 缺什么。

为什么不只抓「逐字」   仓内有大量**诚实自标** Project-defined 的数（Huber δ、tolerance、
              MAD 恒等式）。把它们判红是错的 —— 判据必须区分「抄来的」与「自产的」，
              两者都可接受，**不声明身份**才是不合格。

用法
  python3 eng/tools/doccheck/check_ref_num_quote.py [--root .] [--json-out F]
  python3 eng/tools/doccheck/check_ref_num_quote.py --self-test
"""
import argparse
import json
import os
import re
import sys
import tempfile

CHECK_ID = "G-REF-NUM-QUOTE"
EXCLUDE_DIRS = ("build", "run", "testdata", "gaia", ".git")

# 高精度数值：>=6 位有效数字的十进制，或 >=4 位有效数字的科学计数法
HIGH_PREC = re.compile(
    r"(?<![\w.])"
    r"(?:0|[1-9]\d*)\.\d{5,}(?![\w])"                       # 1.482602218505602
    r"|(?<![\w.])[1-9]\d*\.\d{4,}(?![\w])"                    # 162.0560
    r"|(?<![\w.])[1-9]\d{0,3}\.\d{1,3}e[+-]?\d{1,3}(?![\w])"  # 8.0832e-6
    r"|(?<![\w.])[1-9]\d{2,}\.\d+e[+-]?\d{1,3}(?![\w])"       # 4.987e+6
)
SOURCE = re.compile(
    r"10\.\d{4,9}/"
    r"|arXiv[:.]?\s*\d{4}\.\d{4,5}"
    r"|https?://"
    r"|ISBN\s*[\d-]{10,}"
    r"|bibcode"
    r"|\.pidoc"
    r"|CrossRef|Crossref"
)

# **先剥掉来源锚再抽常量**：否则 DOI 前缀 10.1086、arXiv 编号 1512.06872 会被
# 当成「高精度数值常量」，把整仓 85% 的块判成假红（实测首批扫描的教训）。
ANCHOR_STRIP = re.compile(
    r"10\.\d{4,9}/[^\s，）,。；;）】\]<>]*"
    r"|arXiv[:.]?\s*\d{4}\.\d{4,5}"
    r"|https?://[^\s，）,。；;）】\]<>]+"
    r"|ISBN\s*[\d-]{10,}"
    r"|\b\d{4}\.\d{4,5}\b(?=\s*(?:I|II)\b)"   # QA 矩阵里的 arXiv 简写
)
PLACEHOLDER = "\u0000"
VERBATIM = re.compile(
    r"逐字|原文含|逐字命中|逐字核验|逐字引|verbatim|Verbatim|VERBATIM"
)
IDENTITY = re.compile(
    r"Project-defined|PROJECT-DEFINED"
    r"|教科书"
    r"|恒等式"
    r"|FITS\s*Standard"
    r"|IEEE\s*754"
    r"|自产"
    r"|实测"
    r"|Monte\s*Carlo|\bMC\b"
    r"|标准正态"
    r"|本文定义"
    r"|本仓定义"
    r"|冻结"
    # 仓内既有的诚实溯源措辞：「1.4826，4 位截断展示；权威全精度 1.482602218505602」
    # 就是「这个字面量取自哪一源、显示到几位」的声明，属身份自标，不是无源数字。
    r"|截断展示"
    r"|全精度"
)
ENTRY_SPLIT = re.compile(r"^\s*(?:[-*+]\s+|\d+[.)]\s+|\|\s*)")
FENCE = "\u0060\u0060\u0060"


# 默认分母面 = 科学正本（docs/science/）＋ 文献正本两篇：
#   · docs/engineering/SCIENTIFIC_REFERENCES.md —— 文献总表，随文档迁移进入一级·工程正本；
#   · 实验/photometric-magnitude/docs/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md
#     —— 测光文献存档，随测光科学实验单元出库。
# **不覆盖**已知限制篇与工程/验证面（行为合同、阶段详细设计、门禁阈值、验收细则…）的
# 数值：那里出现的是实测误差、内存阈值、确定性容差，属工程观测量，判据是
# GATES_AND_TOLERANCES / ACR_EQUIVALENCE 的面，用「是否逐字引用文献」去判是范畴错误。
# 需要扩面时用 --roots 显式指定，并把扩面登记进报告。
DEFAULT_ROOTS = ("docs/science",
                 "docs/engineering/SCIENTIFIC_REFERENCES.md",
                 "实验/photometric-magnitude/docs/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md")


def iter_doc_files(root, roots):
    """roots 逐项既可给目录（递归 *.md）也可给单个 .md 文件。

    文献正本迁移后是**单篇**文件（不再成目录），因此本门不能只认目录，
    否则文献正本会被悄悄移出分母面（分母缩水 = 判据变弱）。
    """
    out = []
    for rel in roots:
        base = os.path.join(root, rel)
        if os.path.isfile(base):
            if base.endswith(".md"):
                out.append(base)
            continue
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
            for fn in sorted(filenames):
                if fn.endswith(".md"):
                    out.append(os.path.join(dirpath, fn))
    return out


def iter_blocks(text):
    """块级切分（bullet / numbered / 段落），跳过围栏。"""
    blocks, cur, start, fence = [], [], None, False
    for i, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith(FENCE):
            fence = not fence
            continue
        if fence:
            continue
        if line.lstrip().startswith("#"):
            if cur:
                blocks.append(("\n".join(cur), start))
                cur, start = [], None
            continue
        if ENTRY_SPLIT.match(line) and line.strip():
            if cur:
                blocks.append(("\n".join(cur), start))
            cur, start = [line], i
            continue
        if line.strip():
            if start is None:
                start = i
            cur.append(line)
        else:
            if cur:
                blocks.append(("\n".join(cur), start))
                cur, start = [], None
    if cur:
        blocks.append(("\n".join(cur), start))
    return blocks


def evaluate(root, roots=DEFAULT_ROOTS):
    files = iter_doc_files(root, roots)
    rows = []
    for path in files:
        with open(path, "r", encoding="utf-8") as fh:
            rel = os.path.relpath(path, root)
            for text, lineno in iter_blocks(fh.read()):
                stripped = ANCHOR_STRIP.sub(PLACEHOLDER, text)
                nums = [m.group(0) for m in HIGH_PREC.finditer(stripped)]
                if not nums:
                    continue
                anchor = bool(SOURCE.search(text))
                verb = bool(VERBATIM.search(text))
                ident = bool(IDENTITY.search(text))
                # **分母只取「有来源锚 或 有身份自标」的块**：
                #   判红 = 有来源锚、却既未逐字、也未自标身份。
                # 为什么不把「块里有个高精度数」全部收进来：docs/science/** 里大量高精度
                # 数是本仓**自己实测**的 FP 误差（5.7e-14 deg / 3.1e-9 px 这类），
                # 它们不引文献也不该引，用「是否逐字引用一手来源」判它们是范畴错误
                # （实测 213 块全收会得 55% 判红，全是这类假红）。它们的判据是
                # ACR_EQUIVALENCE / GATES_AND_TOLERANCES 的面。
                if not (anchor or ident):
                    continue
                rows.append({
                    "file": rel, "line": lineno,
                    "constants": sorted(set(nums))[:6],
                    "anchor": anchor, "verbatim": verb, "identity": ident,
                    "verdict": "PASS" if ((anchor and verb) or ident) else "FAIL",
                    "excerpt": text.strip()[:140],
                })
    return {"files": len(files), "blocks": rows}


def self_test():
    root = tempfile.mkdtemp(prefix="grefnum_")
    os.makedirs(os.path.join(root, "docs", "science"), exist_ok=True)
    cases = [
        ("POS 逐字一手引用",
         "- c1=8.0832e-6 与 c2=9.0e+6：PixInsight 02-PSF_Flux_Weighting_Algorithms.pidoc "
         "（https://pixinsight.com/x）逐字命中。\n", "PASS"),
        ("POS 身份自标 Project-defined",
         "- MAD→σ 1.482602218505602：Project-defined 渐近常数，标准正态分位恒等式。\n",
         "PASS"),
        ("NEG 数值有 DOI 但无逐字/身份标记",
         "- 拟合系数 c=1.234567：Beaton & Tukey 1974, Technometrics 16, 147"
         "（DOI 10.1080/00401706.1974.10489171）支持该取值。\n", "FAIL"),
        ("NEG 数值无任何一手引用标记",
         "- 系数 2.13794 用于 h_max；参考 Górski et al. 2005 "
         "（https://doi.org/10.1086/427976）。\n", "FAIL"),
    ]
    ok = True
    for label, body, want in cases:
        with open(os.path.join(root, "docs", "science", "T.md"), "w",
                  encoding="utf-8") as fh:
            fh.write("# 断言\n\n" + body)
        rep = evaluate(root)
        got = rep["blocks"][0]["verdict"] if rep["blocks"] else "NO-ENTRY"
        good = got == want
        ok = ok and good
        print("  %-6s %-32s want=%-6s got=%-6s verbatim=%s identity=%s"
              % ("PASS" if good else "FAIL", label, want, got,
                 rep["blocks"][0]["verbatim"] if rep["blocks"] else "-",
                 rep["blocks"][0]["identity"] if rep["blocks"] else "-"))
    print("SELF-TEST %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=CHECK_ID)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--roots", nargs="*", default=list(DEFAULT_ROOTS),
                    help="分母面（默认：科学正本 docs/science ＋ 文献正本两篇）")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    root = os.path.abspath(a.root)
    if not os.path.isdir(os.path.join(root, "docs")):
        print("N1_INPUT_UNAVAILABLE: docs/ 不存在", file=sys.stderr)
        return 2
    rep = evaluate(root, tuple(a.roots))
    if rep["files"] == 0:
        print("N1_SCAN_FLOOR: 扫到 0 个 .md", file=sys.stderr)
        return 2
    if not rep["blocks"]:
        print("N1_SCAN_FLOOR: 0 个含高精度常量且带来源锚的块（不得判绿）", file=sys.stderr)
        return 2
    n = len(rep["blocks"])
    fails = [b for b in rep["blocks"] if b["verdict"] == "FAIL"]
    verb = sum(1 for b in rep["blocks"] if b["verbatim"])
    ident = sum(1 for b in rep["blocks"] if b["identity"])
    anch = sum(1 for b in rep["blocks"] if b["anchor"])
    print("[%s] 含高精度数值常量的块=%d  判红=%d (%.1f%%)"
          % (CHECK_ID, n, len(fails), 100.0 * len(fails) / n))
    print("  带一手来源锚=%d  逐字标记=%d  身份自标=%d（判绿条件：(锚且逐字) 或 身份自标）"
          % (anch, verb, ident))
    for b in fails:
        print("  RED  %s:%d  常量=%s" % (b["file"], b["line"],
                                         ",".join(b["constants"])))
        print("        %s" % b["excerpt"][:110])
    if a.json_out:
        rep["failures"] = fails
        with open(a.json_out, "w", encoding="utf-8") as fh:
            json.dump(rep, fh, ensure_ascii=False, indent=1, sort_keys=True)
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
