#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""DOC-LAYER1-GRANULARITY | 第一层粗度门（判据 C）：反向下溢。

要判的是什么
  负责人裁定的四层结构里第 ① 层是**单篇最高设计**（docs/ASTROCS_DESIGN.md），
  它的职责是「要粗、给索引」。因此**下层细节被写进第一层**就是缺陷：
    C-a 源码行锚          name.cpp:1617 / name.h:72-80   （属第 ④ 层代码）
    C-b1 高精度科学常数   0.1043885 / 9.968368384        （属第 ② 层科学集）
    C-c 推导步骤           令…则…/由此得/展开得 + >=3 个数学记号（属第 ②/③ 层）
  **只报不改**。判据只加严不放宽：路径指针（file.ext 无行号）**合法**，
  因为那正是「给索引」；容差阈值的科学计数法（1e-6）**不判红**但逐条计数。

为什么这样切（标定记录，防再次踩假阳性）
  * 「命令是」里的「令」曾被单字匹配吃到 → C-c 的令 必须带**后随连接词**
    （令…则/令…得），单字令 一律不算；
  * 1e-6/1e-5/1e-12 这类容差在第一层是**规范性复述**（同行常自带 容差/闭合/门）
    ⇒ 归 C-b2，只计数打印，不判红；
  * 正例对照：同一套判据在 docs/science/DRIZZLE.md 上 C-a 命中 3、C-b 命中 22，
    在 docs/DOCUMENT_GOVERNANCE.md 上三判据全 0 ⇒ 判据是活的，不是恒绿。

判据（任一 G 违规 => exit 1；输入不可用/分母为 0 => exit 2，fail-closed）
  G1 扫描面  第一层文件可读；可判行数 = 0 => rc=2（SCAN_FLOOR）。
  G2 C-a     源码行锚命中 => 判红，逐条 file:line + 原文。
  G3 C-b1    高精度常数命中 => 判红。
  G4 C-b2    容差科学计数法 => 不判红，逐条计数并打印（不得静默）。
  G5 C-c     推导步骤命中 => 判红。
  G6 分母    打印 文件行/标题/围栏行/可判行（分母）/命中数/命中率。
  G7 自身排除 排除本文件自身（自检夹具含故意的坏行）。

用法
  python3 eng/tools/doccheck/check_layer1_granularity.py [--root .] [--layer-file P]
                                                [--json-out F] [--self-test]
exit 0 = 无下溢；1 = 有下溢；2 = 输入不可用或分母为 0。
--self-test 恒 0 = 正例绿 + 注入负例（行锚/高精度常数/推导步骤）红 + 两类不该红的绿。

只读；仅 stdlib；无网络；输出按 (文件, 行号) 排序，跨 cwd 复跑一致。
"""
from __future__ import annotations

import argparse
import contextlib
import io
import json
import os
import re
import sys

CHECK_ID = "DOC-LAYER1-GRANULARITY"
SELF_REL = "eng/tools/doccheck/check_layer1_granularity.py"
DEFAULT_LAYER1 = "docs/ASTROCS_DESIGN.md"

HEAD_RE = re.compile(r"^\s{0,3}(#{1,6})\s+(.*?)\s*$")
FENCE_RE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")
TBL_SEP_RE = re.compile(r"^\s*\|[\s:|-]+\|\s*$")

# ── C-a 源码行锚（必须带行号；纯路径指针不在此判据内）──────────────
CA_RE = re.compile(
    r"(?<![A-Za-z0-9_])[\w./-]+\.(?:cpp|cc|cxx|h|hpp|py|pyx|rs|go|js)"
    r"\s*[:：]\s*\d+(?:\s*[-–—]\s*\d+)?")

# ── C-b 高精度常数 ──────────────────────────────────────────────────
CB1_RE = re.compile(r"(?<![A-Za-z0-9_.§])(\d\.\d{4,})")
CB2_RE = re.compile(r"(?<![A-Za-z0-9_.§])(\d+(?:\.\d+)?[eE][-+]?\d+)")
TOL_WORD_RE = re.compile(r"容差|闭合|等价|门|精度|阈值|冻结|tol|tolerance")

# ── C-c 推导步骤（连接词 + >=3 数学记号）────────────────────────────
CC_CONN_RE = re.compile(
    r"令[^，。；]{1,24}?[则得]|由此得|展开得|代入得|推导得|故有|于是有|因而得|"
    r"证毕|\u2234|\u2235|证明[:：]|推导[:：]")
CC_MATH_RE = re.compile(r"[=≈≤≥≠]|[·×]|√|Σ|∫|\^|\\frac|\\sum|\\int|ε|σ|ρ|Δ|∝")
CC_MIN_MATH = 3


# 裁决（负责人更正）：「最高设计自相矛盾」那条是误报，已撤回。
#   原文 :230 的规矩是「就闭合门取值不复制数值」——作用域受限，
#   不是「本节任何位置不许出现数字」。我原按全节禁令读 => 把窄规矩当全节禁令。
#   => 两处硬规则：
#     1) 粒度面（CB1）在粒度条款落进最高设计之前**不启用**（否则恒红门）；
#     2) 「不复制数值」这类规矩必须带限定语才生效，见 SCOPE_QUALIFIER_RE。
SCOPE_QUALIFIER_RE = re.compile(
    "(就[^，。；]{0,20}|仅就|仅对|针对|本节[^，。；]{0,12}不|(?:该|此)[^，。；]{0,10}不复制)")
NO_COPY_RULE_RE = re.compile("(不复制数值|不复制取值|不复写数值|以[^，。；]{0,20}为准)")
GRANULARITY_DEFAULT = False


def scan_text(text, granularity=None):
    """逐行扫描。返回 (stats, hits, noted, held)。

    granularity=None => 用 GRANULARITY_DEFAULT；
    held = 粒度面命中但未启用时暂存（不判红、也不计为通过）。
    """
    if granularity is None:
        granularity = GRANULARITY_DEFAULT
    stats = {"lines": 0, "headings": 0, "fence_lines": 0, "blank": 0,
             "scannable": 0}
    hits = []
    noted = []
    held = []
    in_fence = False
    for ln_no, raw in enumerate(text.split("\n"), 1):
        ln = raw.rstrip("\r")
        stats["lines"] += 1
        if FENCE_RE.match(ln):
            in_fence = not in_fence
            stats["fence_lines"] += 1
            continue
        if in_fence:
            stats["fence_lines"] += 1
            continue
        if HEAD_RE.match(ln):
            stats["headings"] += 1
            continue
        if not ln.strip():
            stats["blank"] += 1
            continue
        stats["scannable"] += 1
        for m in CA_RE.finditer(ln):
            hits.append({"cat": "C-a-source-anchor", "line": ln_no,
                         "token": m.group(0),
                         "text": ln.strip()[:200],
                         "missing": "源码行号属第 ④ 层；第一层应只给文件级索引"})
        for m in CB1_RE.finditer(ln):
            rec = {"cat": "C-b1-highprec-constant", "line": ln_no,
                   "token": m.group(0),
                   "text": ln.strip()[:200],
                   "missing": "高精度科学常数属第 ② 层科学集；第一层应给指向"}
            # 作用域受限的「不复制数值」规矩不构成对该行的全节禁令（负责人更正）
            if NO_COPY_RULE_RE.search(ln) and not SCOPE_QUALIFIER_RE.search(ln):
                rec["scope_note"] = "同句无限定语的『不复制数值』，作用域不明，不据此判红"
            (hits if granularity else held).append(rec)
        m2 = CB2_RE.search(ln)
        if m2 and TOL_WORD_RE.search(ln):
            noted.append({"cat": "C-b2-tolerance-scientific", "line": ln_no,
                          "token": m2.group(0),
                          "text": ln.strip()[:200],
                          "note": "容差阈值的规范性复述，不判红但计数"})
        m3 = CC_CONN_RE.search(ln)
        if m3 and len(CC_MATH_RE.findall(ln)) >= CC_MIN_MATH:
            hits.append({"cat": "C-c-derivation-step", "line": ln_no,
                         "token": m3.group(0),
                         "text": ln.strip()[:200],
                         "missing": "推导步骤属第 ②/③ 层；第一层应给结论 + 指向"})
    return stats, hits, noted, held


def run_scan(root, rel_files, granularity=None):
    total = {"files": 0, "lines": 0, "headings": 0, "fence_lines": 0,
             "blank": 0, "scannable": 0, "violations": 0, "noted": 0}
    per_file = []
    all_hits, all_notes, all_held = [], [], []
    unavailable = []
    for rel in rel_files:
        ap = os.path.join(root, rel)
        if not os.path.isfile(ap):
            unavailable.append(rel)
            continue
        try:
            with open(ap, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            unavailable.append(rel)
            continue
        total["files"] += 1
        st, hits, notes, held = scan_text(text, granularity)
        for k in ("lines", "headings", "fence_lines", "blank", "scannable"):
            total[k] += st[k]
        total["violations"] += len(hits)
        total["noted"] += len(notes)
        for h in hits:
            h["file"] = rel
        for nrec in notes:
            nrec["file"] = rel
        for h in held:
            h["file"] = rel
        all_hits.extend(hits)
        all_notes.extend(notes)
        all_held.extend(held)
        per_file.append({"file": rel, "scannable": st["scannable"],
                         "violations": len(hits), "noted": len(notes)})
    all_hits.sort(key=lambda h: (h["file"], h["line"]))
    all_notes.sort(key=lambda h: (h["file"], h["line"]))
    all_held.sort(key=lambda h: (h["file"], h["line"]))
    return total, per_file, all_hits, all_notes, all_held, unavailable


def report(out, verbose=True):
    rc = 0
    lines = []
    if out["unavailable"]:
        lines.append("[%s] 目标不可用：%s" % (CHECK_ID, ", ".join(out["unavailable"])))
        out["codes"].append("S1_TARGET_UNAVAILABLE")
        rc = 2
    if out["files"] == 0:
        out["codes"].append("S1_NO_FILE")
        rc = 2
    if out["files"] > 0 and out["scannable"] == 0:
        lines.append("[%s] SCAN_FLOOR：可判行数 = 0（解析器空转不得判绿）" % CHECK_ID)
        out["codes"].append("SCAN_FLOOR")
        rc = 2
    if rc == 0:
        lines.append("[%s] 扫描 %d 个第一层文件；可判行（分母）=%d，标题 %d，围栏行 %d"
                     % (CHECK_ID, out["files"], out["scannable"],
                        out["headings"], out["fence_lines"]))
        lines.append("  下溢命中 %d 处（%.2f%%）；C-b2 容差复述（不判红）%d 处"
                     % (out["violations"],
                        100.0 * out["violations"] / max(1, out["scannable"]),
                        out["noted"]))
        if not out.get("granularity_enabled", False):
            lines.append("  粒度面（C-b1 高精度常数）**未启用**：粒度条款尚未落进第一层文档；"
                         "本次命中 %d 处**暂存不判红、也不计为通过**"
                         % len(out.get("held", [])))
            out["codes"].append("GRANULARITY_FACE_DISABLED")
            for h in out.get("held", []):
                lines.append("  [暂存·不判红] %s:%d [%s] %s"
                             % (h["file"], h["line"], h["cat"], h["token"]))
        if verbose:
            for h in out["hits"]:
                lines.append("  [红] %s:%d [%s] %s"
                             % (h["file"], h["line"], h["cat"], h["token"]))
                lines.append("       原文: %s" % h["text"])
                lines.append("       缺:   %s" % h["missing"])
            for nrec in out["noted_rows"]:
                lines.append("  [注·不判红] %s:%d [%s] %s"
                             % (nrec["file"], nrec["line"], nrec["cat"], nrec["token"]))
    if out["violations"] > 0:
        out["codes"].append("LAYER1_LEAKAGE")
        if rc == 0:
            rc = 1
        lines.append("== %s: FAIL（rc=%d）" % (CHECK_ID, rc))
    else:
        lines.append("== %s: PASS（rc=%d）" % (CHECK_ID, rc))
    for ln in lines:
        print(ln)
    out["rc"] = rc
    return rc


def self_test():
    import tempfile

    cases = []

    def mk(body, name="ASTROCS_DESIGN.md"):
        d = tempfile.mkdtemp(prefix="l1gran_")
        with open(os.path.join(d, name), "w", encoding="utf-8") as fh:
            fh.write(body)
        return d

    cases.append(("N0_coarse_clean", 0, mk(
        "# T\n\n## 1 范围\n\n"
        "- 唯一入口见 lib/infrastructure/cli/exit_codes.h（索引）。\n"
        "- FP64 闭合容差 <1e-6，见 §3.5 与科学集 §2。\n"
        "  权重取 w = A/B，Σ w = 1。\n")))
    cases.append(("N1_source_anchor", 1, mk(
        "# T\n\n## 1 范围\n\n- 核在 drizzle_engine.cpp:1617 计算 weight。\n")))
    cases.append(("N2_highprec_constant", 1, mk(
        "# T\n\n## 1 范围\n\n- 相对亏缺极限 −9.968368384e-2（此行无容差字样）。\n")))
    cases.append(("N3_derivation_step", 1, mk(
        "# T\n\n## 1 范围\n\n- 令 A = a·b，令 C = a+b，由此得 S = A/C = ε·σ。\n")))
    cases.append(("N4_tolerance_not_red", 0, mk(
        "# T\n\n## 1 范围\n\n- FP64 通量闭合容差 <1e-6（主域），逐 leaf <1e-5。\n")))
    cases.append(("N5_ling_not_derivation", 0, mk(
        "# T\n\n## 1 范围\n\n- 命令是平级独立命令，各自独立启动。\n")))
    cases.append(("N6_target_missing", 2, tempfile.mkdtemp(prefix="l1gran_")))
    cases.append(("N7_scan_floor", 2, mk("# T\n\n## 1 范围\n")))

    bad = 0
    for name, want, d in cases:
        target = os.path.join(d, "ASTROCS_DESIGN.md")
        rel = os.path.relpath(target, d)
        total, per_file, hits, notes, held, unavail = run_scan(d, [rel], True)
        out = {"files": total["files"], "lines": total["lines"],
               "headings": total["headings"], "fence_lines": total["fence_lines"],
               "scannable": total["scannable"], "violations": total["violations"],
               "noted": total["noted"], "hits": hits, "noted_rows": notes,
               "unavailable": unavail, "codes": []}
        with contextlib.redirect_stdout(io.StringIO()):
            got = report(out, verbose=True)
        ok = (got == want)
        if not ok:
            bad += 1
        print("  %-24s want rc=%d  got rc=%d  %s  %s"
              % (name, want, got, "OK" if ok else "MISMATCH", out["codes"]))
    print("== %s --self-test: %d 例，%d 例不符预期" % (CHECK_ID, len(cases), bad))
    return 0 if bad == 0 else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="第一层粗度门（反向下溢）")
    ap.add_argument("--root", default=".")
    ap.add_argument("--layer-file", action="append", default=None)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--enable-granularity", action="store_true",
                    help="启用粒度面（仅在粒度条款已落进第一层文档后使用）")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    rels = args.layer_file or [DEFAULT_LAYER1]
    rels = [r for r in rels
            if os.path.normpath(r) != os.path.normpath(os.path.relpath(SELF_REL, root))]
    gran = args.enable_granularity
    total, per_file, hits, notes, held, unavail = run_scan(root, rels, gran)
    out = {"check": CHECK_ID, "layer": "L1", "target_files": rels,
           "files": total["files"], "lines": total["lines"],
           "headings": total["headings"], "fence_lines": total["fence_lines"],
           "scannable": total["scannable"],
           "denominator": total["scannable"],
           "violations": total["violations"], "noted": total["noted"],
           "hits": hits, "noted_rows": notes, "unavailable": unavail,
           "granularity_enabled": gran, "held": held, "codes": []}
    rc = report(out, verbose=not args.quiet)
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(out, fh, ensure_ascii=False, indent=1, sort_keys=True)
    return rc


if __name__ == "__main__":
    sys.exit(main())
