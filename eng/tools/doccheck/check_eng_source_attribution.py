#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""DOC-ENG-SOURCE-ATTRIBUTION | 工程集来源可辨门（判据 E）。

要判的是什么
  第 ② 层工程集里的每一条**规范性条款**，都必须能分辨它是
    RULING        负责人裁决（且**带可回指凭据**：RULINGS 路径 / 裁决编号 / 条款号）
    ARCH_DECISION 架构决定（且指向架构正本）
    DERIVATION    执行者推导（可由证据复算）
    UNLABELED     以上都不是 —— 规范语气但**来源不可辨**
  另单列 DERIVATION_AS_RULING：**把推导伪装成裁决**（同一块里既喊裁决又写推导，
  或喊裁决却无任何可回指凭据）。这一类在本项目里最难被发现，故独立成面。

工程集根的取法（迁移前后都能用）
  docs/engineering 存在 ⇒ 用它（迁移后拓扑）；
  否则用迁移前工程线集合（映射来源 run/FINAL-07/doc-migration/ 逐目录映射表）。

判据（任一 E 违规 => exit 1；输入不可用/分母为 0 => exit 2，fail-closed）
  E1 扫描面   工程集 md 文件数 = 0 => rc=2。
  E2 分母面   规范性块数 = 0 => rc=2（解析器空转不得判绿）。
  E3 分级     每条给 verdict + 证据片段。
  E4 伪装面   DERIVATION_AS_RULING 逐条点名。
  E5 只报不改。

用法
  python3 eng/tools/doccheck/check_eng_source_attribution.py [--root .]
        [--eng-dir D]... [--json-out F] [--self-test]
exit 0 = 全部可辨且无伪装；1 = 有 UNLABELED 或伪装；2 = 输入不可用/分母为 0。

只读；仅 stdlib；逐文件流式。
"""
from __future__ import annotations

import argparse
import contextlib
import io
import json
import os
import re
import sys

CHECK_ID = "DOC-ENG-SOURCE-ATTRIBUTION"
SELF_REL = "eng/tools/doccheck/check_eng_source_attribution.py"
ENG_PREMIGRATION = ["docs/contracts", "docs/architecture", "docs/interfaces",
                    "docs/api", "docs/standards", "docs/ci", "docs/development",
                    "docs/validation", "docs/acceptance", "docs/traceability",
                    "docs/operations", "docs/performance", "docs/quality"]
ENG_POSTMIGRATION = ["docs/engineering"]

HEAD_RE = re.compile(r"^\s{0,3}(#{1,6})\s+(.*?)\s*$")
FENCE_RE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")
TBL_SEP_RE = re.compile(r"^\s*\|[\s:|-]+\|\s*$")
LIST_RE = re.compile(r"^\s*(?:[-*+]|\d+\.)\s+")

# 规范性语气：只有带这些词的块才进分母（否则门会退化成「所有段落都要写来源」）
NORMATIVE_RE = re.compile(
    r"(必须|不得|不可|禁止|应当|应该|只允许|一律|统一为|判为|视为|要求|"
    r"\bmust\b|\bshall\b|\brequired\b|\bforbidden\b)")
AUTHORITY_RE = re.compile(r"(裁决|裁定|负责人|前台裁决|owner\s*裁决|负责人裁定)")
# 「裁决器」「需负责人批准」「负责人明令」是**流程词**，不是来源声明；
# 权威词必须处在**决策位**（裁定/裁决为/已裁决/定为/决定/明令/批准）才算来源声明。
DECISION_VERB_RE = re.compile(r"(裁定|裁决为|已裁决|裁决|定为|决定|明令|批准|拍板|定案)")
PROCESS_PHRASE_RE = re.compile(r"(裁决器|需负责人|须负责人|负责人批准|负责人明令|负责人裁定前|"
                              r"负责人裁决前|待负责人|报负责人)")
BACKREF_RE = re.compile(
    r"(RULINGS\.md|裁决\s*[A-Z]?\d|\bA\d{1,3}\b|\bR-\d+|\bCHG-\d+|"
    r"§\s*\d+(?:\.\d+)*|RULINGS)")
ARCH_RE = re.compile(r"(架构|ADR|顶层结构|分层|依赖方向|边界划定|统一口径为|定为)")
DERIV_RE = re.compile(
    r"(推导|实测|复算|评估后|本层选择|实现取|按\s*\S+\s*取值|经对比|"
    r"经核|试验表明|测量得|样本|基准显示|reasoned|derived)")


def blocks_of(lines):
    """块级切分（表格每数据行一条，列表每项一条，正文按空行分段）。
    每轮断言游标严格前进（死循环守卫）。"""
    out, i, n = [], 0, len(lines)
    while i < n:
        start = i
        ln = lines[i]
        if HEAD_RE.match(ln) or not ln.strip() or TBL_SEP_RE.match(ln):
            i += 1
            continue
        if ln.strip().startswith("|"):
            out.append((i + 1, ln.strip()))
            i += 1
            continue
        if LIST_RE.match(ln):
            out.append((i + 1, ln.strip()))
            i += 1
            continue
        j = i + 1
        while (j < n and lines[j].strip() and not HEAD_RE.match(lines[j])
               and not lines[j].strip().startswith("|")
               and not LIST_RE.match(lines[j]) and not TBL_SEP_RE.match(lines[j])):
            j += 1
        out.append((i + 1, "\n".join(lines[i:j])))
        i = j
        if i <= start:
            raise RuntimeError("blocks_of 未前进 @ line %d" % (i + 1))
    return out


def verdict_of(text):
    """返回 (verdict, evidence)。"""
    auth = AUTHORITY_RE.search(text)
    if auth and PROCESS_PHRASE_RE.search(text):
        auth = None          # 流程词，不是来源声明
    if auth and not DECISION_VERB_RE.search(text):
        auth = None          # 权威词不在决策位
    norm = NORMATIVE_RE.search(text)
    arch = ARCH_RE.search(text)
    deriv = DERIV_RE.search(text)
    back = BACKREF_RE.search(text)

    # ⚠ 裁决二：断言面（RULING_UNBACKED / DERIVATION_AS_RULING）**已撤出**——
    #   不是降为只打印，是**判据条目不存在**（不留「跑过且通过」的假绿）。
    #   人工 30 条复核显示旧词表 15 条里 13 条是流程词误报（「裁决器」/「不参与裁决」/
    #   「只有负责人可批准」），分类器修好前该面不具备判红资格。
    if auth and back:
        return ("RULING", "裁决词 %r + 凭据 %r" % (auth.group(0), back.group(0)))
    if arch and back:
        return ("ARCH_DECISION", "架构词 %r + 凭据 %r" % (arch.group(0), back.group(0)))
    if arch:
        return ("ARCH_DECISION", "架构词 %r（无凭据，归架构面但不可回读）" % arch.group(0))
    if deriv:
        return ("DERIVATION", "推导词 %r" % deriv.group(0))
    if norm:
        return ("UNLABELED", "规范语气 %r 但无任何来源标记" % norm.group(0))
    return (None, None)


def iter_eng(root, dirs):
    out, missing = [], []
    for d in dirs:
        ap = os.path.join(root, d)
        if not os.path.isdir(ap):
            missing.append(d)
            continue
        for dp, dns, fns in os.walk(ap):
            dns[:] = sorted(dns)
            for fn in sorted(fns):
                if fn.endswith(".md"):
                    out.append(os.path.join(dp, fn))
    out.sort()
    return out, missing


def main(argv=None):
    ap = argparse.ArgumentParser(description="工程集来源可辨门")
    ap.add_argument("--root", default=".")
    ap.add_argument("--eng-dir", action="append", default=None)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    dirs = args.eng_dir
    topo = "explicit"
    if not dirs:
        if os.path.isdir(os.path.join(root, "docs", "engineering")):
            dirs, topo = ENG_POSTMIGRATION, "post-migration(docs/engineering)"
        else:
            dirs, topo = ENG_PREMIGRATION, "pre-migration(现工程线集合)"
    files, missing = iter_eng(root, dirs)
    counts = {"RULING": 0, "ARCH_DECISION": 0, "DERIVATION": 0,
              "UNLABELED": 0}
    rows = []
    for ap in files:
        rel = os.path.relpath(ap, root)
        try:
            with open(ap, encoding="utf-8", errors="replace") as fh:
                lines = fh.read().split("\n")
        except OSError:
            continue
        in_fence = False
        clean = []
        for ln in lines:
            if FENCE_RE.match(ln):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            clean.append(ln)
        for ln_no, blk in blocks_of(clean):
            v, ev = verdict_of(blk)
            if v is None:
                continue
            counts[v] = counts.get(v, 0) + 1
            rows.append({"file": rel, "line": ln_no, "verdict": v,
                         "evidence": ev, "text": blk[:220]})
    total = sum(counts.values())
    unlab = counts["UNLABELED"]   # 度量面唯一判红依据（只随内容变化）
    fake = 0                     # 断言面已撤出（裁决二）
    rc = 0
    if not files:
        rc = 2
    if files and total == 0:
        rc = 2
    if rc == 0 and (fake or unlab):
        rc = 1
    out = {"check": CHECK_ID, "layer": "L2-engineering", "topology": topo,
           "eng_dirs": dirs, "missing_dirs": missing, "files": len(files),
           "denominator": total, "counts": counts,
           "traceable_ratio": ((total - unlab) / total) if total else None,
           "verdict_as_ruling": fake, "rows": rows, "rc": rc,
           "codes": ([] if rc == 0 else
                     (["E_SCAN"] if rc == 2 else
                      (["E_DENOM_FLOOR"] if total == 0 else
                       ["E_UNLABELED"] + (["E_DERIVATION_AS_RULING"] if fake else []))))}
    print("[%s] 拓扑=%s 工程集文件=%d（目录 %s）"
          % (CHECK_ID, topo, len(files), ",".join(dirs)))
    print("  分母（规范性块）=%d：RULING %d / ARCH_DECISION %d / DERIVATION %d / "
          "UNLABELED %d（度量面唯一判红依据）"
          % (total, counts["RULING"], counts["ARCH_DECISION"], counts["DERIVATION"],
             unlab))
    print("  断言面已撤出（裁决二）：RULING_UNBACKED / DERIVATION_AS_RULING 不再是判据条目")
    if total:
        print("  可辨来源 = %d/%d = %.1f%%"
              % (total - unlab, total, 100.0 * (total - unlab) / total))
    if not files:
        print("  E1 扫描面为空（缺失目录：%s）" % ",".join(missing))
    if files and total == 0:
        print("  E2_FLOOR：规范性块数 = 0（该面静默空转不得判绿）")
    if not args.quiet:
        for r in rows:
            if r["verdict"] == "UNLABELED":
                print("  [红] %s:%d [%s] %s" % (r["file"], r["line"],
                                                r["verdict"], r["evidence"]))
                print("       原文: %s" % r["text"][:160])
    print("== %s: %s（rc=%d）" % (CHECK_ID, "FAIL" if rc else "PASS", rc))
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(out, fh, ensure_ascii=False, indent=1, sort_keys=True)
    return rc


def self_test():
    import tempfile

    bad = 0

    def mk(body):
        d = tempfile.mkdtemp(prefix="engsrc_")
        os.makedirs(os.path.join(d, "docs", "contracts"))
        with open(os.path.join(d, "docs", "contracts", "T.md"), "w",
                  encoding="utf-8") as fh:
            fh.write(body)
        return d

    cases = [
        ("N0_ruling_with_backref", 0, mk(
            "# T\n\n- 必须写盘（负责人裁决 R-42，见 run/FINAL-07/plan/RULINGS.md）。\n")),
        # 裁决二：断言面已撤出 ⇒ 这两条**不再**因「裁决词」判红；
        # 它们退回度量面（有推导词 ⇒ DERIVATION=绿；纯规范语气无来源 ⇒ UNLABELED=红）。
        ("N1_ruling_no_backref", 1, mk(
            "# T\n\n- 必须写盘（前台裁决）。\n")),
        ("N2_derivation_wins", 0, mk(
            "# T\n\n- 必须写盘：负责人裁决，实测表明取 1e-6 更稳。\n")),
        ("N3_derivation_ok", 0, mk(
            "# T\n\n- 必须写盘：由实测复算得该阈值。\n")),
        ("N4_unlabeled", 1, mk(
            "# T\n\n- 错误路径必须返回非零退出码。\n")),
        ("N4b_no_normative_marker", 2, mk(
            "# T\n\n- 退出码 10 表示配置错误。\n")),
        ("N5_scan_floor", 2, mk("# T\n\n## 1 概述\n\n本文件说明背景。\n")),
    ]
    for name, want, d in cases:
        with contextlib.redirect_stdout(io.StringIO()):
            got = main(["--root", d, "--quiet"])
        ok = (got == want)
        if not ok:
            bad += 1
        print("  %-26s want rc=%d  got rc=%d  %s"
              % (name, want, got, "OK" if ok else "MISMATCH"))
    print("== %s --self-test: %d 例，%d 例不符预期" % (CHECK_ID, len(cases), bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
