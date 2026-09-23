#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ALG-LINE-ANCHORS | ALG 文档行号/行数锚 vs 源文件实测门（审计 CONFORM-SWEEP-3-018）。

为什么另立本门（根因）
  `DOC-LINE-ANCHORS`（step of CHK-SCI-REF，
  `docs/algorithms/anchors/check_doc_line_anchors.py`）只判
  「锚能解析 + 1 <= start <= end <= 目标行数」，即**界内**；
  它**不判**：
    (a) 文档自述的**行数锚**（`sampler.cpp（1156 行）`）是否等于源文件实测行数；
    (b) 逐符号表里**裸行号范围**（`| cvar | :840-842 |`）所指区间是否真的含该符号。
  审计 CONFORM-SWEEP-3-018 的三份 ALG 文档（PHASE2_SAMPLER / PHASE2_UPM_IMPL /
  PHASE2_REJECTION）在这两条上**系统性过期**，而旧门禁全程判绿 —— 这是「行号锚漂移」
  类缺陷长期无机器门的结构性原因。

判据（任一 L 违规 ⇒ exit 1）
  L0 scan_floor        fail-closed：文档根不存在 / 扫到 0 篇文档 /
                       提取到 0 个行数锚 或 0 个符号锚 ⇒ 判红（禁止空转判绿）；
                       git 不可用 ⇒ 判红（无法判定锚目标是否 tracked）。
  L1 count_mismatch    行数锚 `<file>（N 行` 的 N 必须 == 目标文件实测行数（去尾换行）。
                       作用域 = DOC_GLOB（现 docs/**/*.md，含 docs/contracts、docs/modules）。
  L3 symbol_drift      逐符号表中「列头声明了文件」的锚：该行首列符号必须逐字出现在
                       该锚的行范围内。符号整体不在目标文件 ⇒ L3 symbol_absent。
                       作用域 = SYMBOL_GLOB（**有意保持** docs/algorithms/*.md，见常量注释）。
  L2 target_unresolved 锚目标必须解析到唯一**被 git 跟踪**的仓库文件（exact →
                       doc-relative → unique basename）；解析不到/未跟踪 ⇒ 判红。

不覆盖（如实声明，不假装覆盖）
  - 显式 `file:N-M` 的**界内**判定由兄弟门 `DOC-LINE-ANCHORS` 承担，本门不重复实现；
  - **锚边界落在空行**由 `DOC-LINE-ANCHORS` 的 C6 判（该门有完整 resolver 链与
    unresolved_registry，本门无）；本门不重复实现，避免第二套解析口径；
  - `docs/contracts/DATA_SEMANTICS.md` 的逐符号表漂移（实测 35 条）本门**当前不判**
    （L3 作用域未扩），见模块常量注释与 DOC-DRIFT-FIX-01 回执的「发现但未改」；
  - 散文正文里不带文件列的裸行号（如 `# :555` 代码注释式锚）**无法在无符号绑定的
    前提下自动核对**，本门不计入判据，清单见 reports/RELEASE-02/guard-tools-fix.md。

用法
  python3 eng/tools/doccheck/check_alg_line_anchors.py [--root .] [--json-out F]
  python3 eng/tools/doccheck/check_alg_line_anchors.py --self-test
exit 0 = PASS；1 = 判据违规；2 = 输入不可用（fail-closed）。

只读；仅 stdlib；输出稳定排序（无时间戳），跨 cwd 复跑 bitwise 一致。
"""
from __future__ import annotations

import argparse
import glob as _glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
# L1（行数锚）作用域：DOC-DRIFT-FIX-01 由 docs/algorithms/*.md 扩到全 docs 面 ——
# docs/contracts/**、docs/modules/** 的「<file>（N 行」长期无门，实测 70 条与源文件不符。
DOC_GLOB = "docs/**/*.md"
# L3（逐符号表行号范围）作用域**有意保持** docs/algorithms/*.md：扩到 docs/contracts 会
# 立刻暴露 docs/contracts/DATA_SEMANTICS.md 两张状态表 35 条符号漂移（rejection.cpp /
# p2_session.cpp 整体位移），订正需逐符号重新推导语义，不属本任务声明的判据面 ——
# 该项已在回执中作为「发现但未改」逐条列出，不得当作已覆盖。
SYMBOL_GLOB = "docs/algorithms/*.md"

EXTS = ("cpp", "cc", "cxx", "h", "hpp", "hh", "py", "sh", "ps1", "json",
        "yaml", "yml", "md", "cmake", "in", "txt")
_FILE = (r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*"
         r"[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:" + "|".join(EXTS) + r"))")
FILE_RE = re.compile(_FILE)
# 行数锚：<file>（N 行 / <file> (N 行 / <file> N 行（右括号可缺，如「（2076 行，astrocs_phase2 静态库」）
COUNT_RE = re.compile(_FILE + r"[ \t]*[（(]?[ \t]*(\d+)[ \t]*行")
# 裸行号范围：:N / :N-M / :N–M
RANGE_RE = re.compile(r":L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?")
SEP_RE = re.compile(r"^\s*\|[\s:|-]+\|\s*$")
IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)?$")
ARCHIVE_MARKERS = ("/archive/", "/legacy/", "/.git/", "/third_party/",
                   "/build/", "/out/", "/run/", "/worktrees/")
BT = chr(96)


def read_lines(path):
    with open(path, "rb") as fh:
        text = fh.read().decode("utf-8", errors="replace")
    if text.endswith("\n"):
        text = text[:-1]
    return text.split("\n")


def git_ls(root):
    r = subprocess.run(["git", "ls-files"], cwd=root, capture_output=True, text=True)
    if r.returncode != 0:
        return None
    return [x for x in r.stdout.splitlines() if x.strip()]


def build_basename_index(root):
    idx = {}
    for dirpath, dirnames, filenames in os.walk(root):
        d = dirpath.replace(os.sep, "/") + "/"
        dirnames[:] = [x for x in dirnames if x != ".git"]
        if any(s in d for s in ARCHIVE_MARKERS):
            dirnames[:] = []
            continue
        for fn in filenames:
            rel = os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/")
            idx.setdefault(fn, []).append(rel)
    for k in idx:
        idx[k].sort()
    return idx


def resolve(root, doc, base, basename_index):
    direct = os.path.normpath(base).replace(os.sep, "/")
    if not direct.startswith("../") and os.path.isfile(os.path.join(root, direct)):
        return direct, "exact"
    rel = os.path.normpath(os.path.join(os.path.dirname(doc), base)).replace(os.sep, "/")
    if not rel.startswith("../") and os.path.isfile(os.path.join(root, rel)):
        return rel, "doc-relative"
    cands = basename_index.get(os.path.basename(base), [])
    if len(cands) == 1:
        return cands[0], "basename-unique"
    if not cands:
        return None, "no-such-file"
    return None, "ambiguous:" + ",".join(cands[:6])


def md_cells(line):
    s = line.strip()
    if not s.startswith("|"):
        return None
    return [c.strip() for c in s.strip("|").split("|")]


def scan_doc(root, doc, basename_index, tracked_set, symbol_scope=True):
    """Return (findings, counters, records). symbol_scope=False ⇒ 只跑 L1（行数锚）。"""
    findings = []
    lines = read_lines(os.path.join(root, doc))
    counts = {"count_anchors": 0, "symbol_anchors": 0}
    records = []

    def resolve_or_fail(lineno, base, kind):
        path, how = resolve(root, doc, base, basename_index)
        if path is None:
            findings.append({"code": "L2_target_unresolved",
                             "detail": "%s:%d %s (%s)" % (doc, lineno, base, how)})
            return None
        if tracked_set is not None and path not in tracked_set:
            findings.append({"code": "L2_target_unresolved",
                             "detail": "%s:%d %s -> untracked %s" % (doc, lineno, base, path)})
            return None
        return path

    # L1：行数锚
    for i, line in enumerate(lines, 1):
        for m in COUNT_RE.finditer(line):
            base, claimed = m.group(1), int(m.group(2))
            path = resolve_or_fail(i, base, "count")
            if path is None:
                continue
            actual = len(read_lines(os.path.join(root, path)))
            counts["count_anchors"] += 1
            rec = {"doc": doc, "doc_line": i, "raw": m.group(0).strip(),
                   "kind": "line_count", "target": path,
                   "claimed": claimed, "actual": actual}
            if claimed != actual:
                rec["status"] = "COUNT_MISMATCH"
                findings.append({"code": "L1_count_mismatch",
                                 "detail": "%s:%d %s -> 文档 %d 行 / 实测 %d 行 (delta %+d)"
                                           % (doc, i, base, claimed, actual, actual - claimed)})
            else:
                rec["status"] = "OK"
            records.append(rec)

    # L3：逐符号表（列头声明文件）里的裸行号范围
    for i, line in enumerate(lines) if symbol_scope else ():
        if not SEP_RE.match(line) or i == 0:
            continue
        hdr = md_cells(lines[i - 1])
        if not hdr:
            continue
        hdr_files = [FILE_RE.search(c) for c in hdr]
        if not any(hdr_files):
            continue
        col_files = [(f.group(1) if f else None) for f in hdr_files]
        j = i + 1
        while j < len(lines) and md_cells(lines[j]) is not None:
            cells = md_cells(lines[j])
            if len(cells) != len(hdr):
                break
            sym = cells[0].strip(BT + "* ")
            if IDENT_RE.match(sym):
                for k, base in enumerate(col_files):
                    if not base or k >= len(cells):
                        continue
                    path = resolve_or_fail(j + 1, base, "symbol")
                    if path is None:
                        continue
                    body = read_lines(os.path.join(root, path))
                    for m in RANGE_RE.finditer(cells[k]):
                        s0 = int(m.group(1))
                        e0 = int(m.group(2) or m.group(1))
                        counts["symbol_anchors"] += 1
                        rec = {"doc": doc, "doc_line": j + 1, "raw": "%s:%s" % (base, m.group(0)),
                               "kind": "symbol_range", "target": path,
                               "symbol": sym, "start": s0, "end": e0}
                        if s0 < 1 or e0 < s0 or e0 > len(body):
                            rec["status"] = "OUT_OF_BOUNDS"
                            findings.append({"code": "L2_range_out_of_bounds",
                                             "detail": "%s:%d %s -> %s has %d lines"
                                                       % (doc, j + 1, rec["raw"], path, len(body))})
                        elif any(sym in b for b in body[s0 - 1:e0]):
                            rec["status"] = "OK"
                        elif any(sym in b for b in body):
                            hits = [q + 1 for q, b in enumerate(body) if sym in b]
                            rec["status"] = "SYMBOL_DRIFT"
                            rec["actual_lines"] = hits[:6]
                            findings.append({"code": "L3_symbol_drift",
                                             "detail": "%s:%d sym=%s %s:%d-%d 区间内无该符号"
                                                       "（实测行 %s）"
                                                       % (doc, j + 1, sym, path, s0, e0, hits[:6])})
                        else:
                            rec["status"] = "SYMBOL_ABSENT"
                            findings.append({"code": "L3_symbol_absent",
                                             "detail": "%s:%d sym=%s 在 %s 中整体不存在"
                                                       % (doc, j + 1, sym, path)})
                        records.append(rec)
            j += 1
    return findings, counts, records


def check_root(root):
    root = os.path.abspath(root)
    errors = []
    if not os.path.isdir(os.path.join(root, "docs")):
        return {"verdict": "FAIL", "errors": [
            {"code": "L0_scan_floor", "detail": "ANCHOR_STALE: DOC_GLOB 根不存在 docs/"}],
            "anchors": [], "counters": {}}
    tracked = git_ls(root)
    tracked_set = set(tracked) if tracked is not None else None
    if tracked_set is None:
        errors.append({"code": "L0_scan_floor",
                       "detail": "git ls-files 不可用，无法判定锚目标是否 tracked（fail-closed）"})
    basename_index = build_basename_index(root)
    docs = sorted(os.path.relpath(p, root).replace(os.sep, "/")
                  for p in _glob.glob(os.path.join(root, DOC_GLOB), recursive=True))
    symbol_docs = set(os.path.relpath(p, root).replace(os.sep, "/")
                      for p in _glob.glob(os.path.join(root, SYMBOL_GLOB), recursive=True))
    if not docs:
        errors.append({"code": "L0_scan_floor", "detail": "ANCHOR_STALE: 0 篇文档命中 " + DOC_GLOB})
    if not symbol_docs:
        errors.append({"code": "L0_scan_floor", "detail": "ANCHOR_STALE: 0 篇文档命中 " + SYMBOL_GLOB})
    counters = {"count_anchors": 0, "symbol_anchors": 0}
    anchors = []
    for d in docs:
        f, c, r = scan_doc(root, d, basename_index, tracked_set, symbol_scope=(d in symbol_docs))
        errors.extend(f)
        anchors.extend(r)
        for k in counters:
            counters[k] += c[k]
    if counters["count_anchors"] == 0:
        errors.append({"code": "L0_scan_floor",
                       "detail": "行数锚提取数 = 0（扫描面塌缩，禁止空转判绿）"})
    if counters["symbol_anchors"] == 0:
        errors.append({"code": "L0_scan_floor",
                       "detail": "逐符号表行号锚提取数 = 0（扫描面塌缩，禁止空转判绿）"})
    errors.sort(key=lambda e: (e["code"], e["detail"]))
    return {"verdict": "PASS" if not errors else "FAIL",
            "documents": len(docs), "counters": counters,
            "errors": errors, "anchors": anchors}


def emit(result, json_out):
    by_code = {}
    for e in result["errors"]:
        by_code[e["code"]] = by_code.get(e["code"], 0) + 1
    report = {
        "schema": "astrocs/alg-line-anchors/v1",
        "task": "GUARD-TOOLS-FIX / CONFORM-SWEEP-3-018",
        "doc_glob": DOC_GLOB,
        "symbol_glob": SYMBOL_GLOB,
        "verdict": result["verdict"],
        "documents": result.get("documents", 0),
        "counters": result.get("counters", {}),
        "by_code": dict(sorted(by_code.items())),
        "errors": result["errors"],
        "anchors": sorted(result["anchors"],
                          key=lambda a: (a["doc"], a["doc_line"], a["kind"], a.get("raw", ""))),
    }
    if json_out:
        parent = os.path.dirname(os.path.abspath(json_out))
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(json_out, "w", encoding="utf-8") as fh:
            json.dump(report, fh, ensure_ascii=False, indent=1, sort_keys=True)
            fh.write("\n")
    if result["verdict"] != "PASS":
        print("ALG_LINE_ANCHORS_FAIL: %d findings (by_code=%s)" %
              (len(result["errors"]), ",".join("%s:%d" % kv for kv in sorted(by_code.items()))))
        for e in result["errors"]:
            print("  [%s] %s" % (e["code"], e["detail"]))
        return 1
    print("ALG_LINE_ANCHORS_PASS: %d docs, 行数锚 %d, 逐符号行号锚 %d, 全部与源文件实测一致"
          % (report["documents"], report["counters"].get("count_anchors", 0),
             report["counters"].get("symbol_anchors", 0)))
    return 0


# ---------------------------------------------------------------- self-test
_SYNTH_DOC = """# SYNTH ALG

> 模块: lib/x/synth.cpp（5 行）+ 头 lib/x/synth.h（4 行，实测 2026-01-01）

## 1 逐符号锚

| 符号 | 声明（synth.h） | 实现（synth.cpp） | 语义 |
|---|---|---|---|
| p2_synth_run | :2 | :3-4 | 入口 |
| kSynthMagic | :3 | :2 | 常量 |
"""
_SYNTH_H = "// synth.h\nint p2_synth_run(void);\nstatic const int kSynthMagic = 7;\n// end\n"
_SYNTH_CPP = "// synth.cpp\nstatic const int kSynthMagic = 7;\nint p2_synth_run(void) {\n  return kSynthMagic;\n}\n"


def _write(root, rel, text):
    p = os.path.join(root, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as fh:
        fh.write(text)


def _git_init(root):
    env = dict(os.environ)
    env.update({"GIT_AUTHOR_NAME": "selftest", "GIT_AUTHOR_EMAIL": "selftest@local",
                "GIT_COMMITTER_NAME": "selftest", "GIT_COMMITTER_EMAIL": "selftest@local"})
    for cmd in (["git", "init", "-q"], ["git", "add", "-A"],
                ["git", "commit", "-q", "-m", "fixture"]):
        subprocess.run(cmd, cwd=root, check=True, capture_output=True, env=env)


def _mk_fixture():
    root = tempfile.mkdtemp(prefix="alg_anchor_selftest_")
    _write(root, "docs/algorithms/SYNTH.md", _SYNTH_DOC)
    _write(root, "lib/x/synth.h", _SYNTH_H)
    _write(root, "lib/x/synth.cpp", _SYNTH_CPP)
    _git_init(root)
    return root


def self_test():
    fails = []
    checks = []

    def expect(name, root, want_verdict, want_code=None):
        res = check_root(root)
        codes = {e["code"] for e in res["errors"]}
        ok = res["verdict"] == want_verdict and (want_code is None or want_code in codes)
        checks.append((name, res["verdict"], sorted(codes), ok))
        if not ok:
            fails.append("%s: 期望 %s/%s 实得 %s/%s"
                         % (name, want_verdict, want_code, res["verdict"], sorted(codes)))

    # N0 正例 ⇒ PASS
    root = _mk_fixture()
    try:
        expect("N0 正例（锚与实测一致）", root, "PASS")
        # N1 行数锚不符（注入 5 行 -> 6 行）⇒ L1
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP + "// tail\n")
        expect("N1 行数锚漂移", root, "FAIL", "L1_count_mismatch")
        # N1' 恢复 ⇒ PASS（红→绿）
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP)
        expect("N1' 恢复后回绿", root, "PASS")
        # N2 符号漂移：kSynthMagic 仍在文件内但已不在 :2 区间 ⇒ L3
        _write(root, "lib/x/synth.cpp",
               _SYNTH_CPP.replace("static const int kSynthMagic = 7;",
                                  "static const int kSynthOther = 7;"))
        expect("N2 逐符号行号锚漂移", root, "FAIL", "L3_symbol_drift")
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP)
        expect("N2' 恢复后回绿", root, "PASS")
        # N3 符号整体消失 ⇒ L3_symbol_absent
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP.replace("kSynthMagic", "kSynthOther"))
        expect("N3 符号整体缺失", root, "FAIL", "L3_symbol_absent")
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP)
        # N4 锚目标不存在 ⇒ L2_target_unresolved
        _write(root, "docs/algorithms/SYNTH.md", _SYNTH_DOC.replace("synth.cpp", "nosuch.cpp", 1))
        expect("N4 锚目标不存在", root, "FAIL", "L2_target_unresolved")
        _write(root, "docs/algorithms/SYNTH.md", _SYNTH_DOC)
        # N5 扫描面塌缩（0 行数锚 且 0 符号锚）⇒ L0 fail-closed
        _write(root, "docs/algorithms/SYNTH.md", "# 空文档\n无锚\n")
        expect("N5 扫描面塌缩", root, "FAIL", "L0_scan_floor")
        _write(root, "docs/algorithms/SYNTH.md", _SYNTH_DOC)
        # N6 文档根缺失 ⇒ L0 fail-closed
        empty = tempfile.mkdtemp(prefix="alg_anchor_empty_")
        try:
            expect("N6 文档根缺失", empty, "FAIL", "L0_scan_floor")
        finally:
            shutil.rmtree(empty, ignore_errors=True)
        # N7 未跟踪文件（git 仓库里新增未 add）⇒ L2_target_unresolved
        _write(root, "lib/x/extra.cpp", "// extra\n")
        _write(root, "docs/algorithms/SYNTH.md",
               _SYNTH_DOC + "\n> 旁证 lib/x/extra.cpp（1 行）\n")
        expect("N7 锚指向未跟踪文件", root, "FAIL", "L2_target_unresolved")
        _write(root, "docs/algorithms/SYNTH.md", _SYNTH_DOC)
        os.remove(os.path.join(root, "lib/x/extra.cpp"))
        expect("N7' 恢复后回绿", root, "PASS")
    finally:
        shutil.rmtree(root, ignore_errors=True)

    for name, verdict, codes, ok in checks:
        print("  %-28s verdict=%-4s codes=%-42s %s" % (name, verdict, ",".join(codes) or "-",
                                                       "OK" if ok else "**FAIL**"))
    if fails:
        print("ALG_LINE_ANCHORS_SELFTEST_FAIL:")
        for f in fails:
            print("  " + f)
        return 1
    print("ALG_LINE_ANCHORS_SELFTEST_PASS: %d 组（正例 1 + 负例/恢复 6 类）全部符合预期" % len(checks))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="ALG doc line-anchor re-test")
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    if not os.path.isdir(args.root):
        print("ALG_LINE_ANCHORS_FAIL: ANCHOR_STALE --root 不存在 %s" % args.root)
        return 2
    return emit(check_root(args.root), args.json_out)


if __name__ == "__main__":
    sys.exit(main())
