#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ALG-LINE-ANCHORS | ALG 文档行号/行数锚 vs 源文件实测门。

为什么另立本门（根因）
  `DOC-LINE-ANCHORS`（step of CHK-SCI-REF，
  `docs/algorithms/anchors/check_doc_line_anchors.py`）只判
  「锚能解析 + 1 <= start <= end <= 目标行数」，即**界内**；
  它**不判**：
    (a) 文档自述的**行数锚**（`sampler.cpp（1156 行）`）是否等于源文件实测行数；
    (b) 逐符号表里**裸行号范围**（`| cvar | :840-842 |`）所指区间是否真的含该符号。
  三份 ALG 文档（PHASE2_SAMPLER / PHASE2_UPM_IMPL /
  PHASE2_REJECTION）在这两条上**系统性过期**，而旧门禁全程判绿 —— 这是「行号锚漂移」
  类缺陷长期无机器门的结构性原因。

判据（任一 L 违规 ⇒ exit 1）
  L0 scan_floor        fail-closed：文档根不存在 / 扫到 0 篇文档 /
                       提取到 0 个行数锚 或 0 个符号锚 ⇒ 判红（禁止空转判绿）；
                       git 不可用 ⇒ 判红（无法判定锚目标是否 tracked）。
  L1 count_mismatch    行数锚 `<file>（N 行` 的 N 必须 == 目标文件实测行数（去尾换行）。
                       作用域 = DOC_GLOB（现 docs/**/*.md，含 docs/contracts、docs/modules）。
  L3 symbol_drift      逐符号表中「列头声明了文件」的锚：该行首列符号必须作为**代码 token**
                       出现在该锚的行范围内（口径见下方「L3 匹配规则」）。符号整体不在目标
                       文件 ⇒ L3 symbol_absent。
  L3 prose_only        符号只在区间的**注释/字符串**里出现（散文命中）⇒ 判红：该锚没有指向
                       符号的实现站点，属「无区分力锚」。旧口径（区间原文子串）对这一形态
                       静默判绿——实测两例见下方「L3 匹配规则」。
                       作用域 = SYMBOL_GLOB（**有意保持** docs/science/algorithms/*.md，见常量注释）。
  L1b bare_range_oob    **邻接**裸行号锚（紧贴文件名的 `file:N` / `file:N-M`，含 :N-M 裸写形态）必须
                       1 <= start <= end <= 目标文件实测行数。作用域 = DOC_GLOB。
                       绑定规则保守到"只判紧贴"（文件名 token 后仅允许反引号/星号/空白 + 至多一个
                       冒号）；未解析的同名 token 不报（本 pass 只判界内，不扩 L2 解析面）。
  L2 target_unresolved 锚目标必须解析到唯一**被 git 跟踪**的仓库文件（exact →
                       doc-relative → unique basename）；解析不到/未跟踪 ⇒ 判红。

不覆盖（如实声明，不假装覆盖）
  - 显式 `file:N-M` 的**界内**判定由兄弟门 `DOC-LINE-ANCHORS` 承担，本门不重复实现；
  - **锚边界落在空行**由 `DOC-LINE-ANCHORS` 的 C6 判（该门有完整 resolver 链与
    unresolved_registry，本门无）；本门不重复实现，避免第二套解析口径；
  - `docs/science/DATA_SEMANTICS.md` 的逐符号表漂移（实测 35 条）本门**当前不判**
    （L3 作用域未扩），见模块常量注释与 DOC-DRIFT-FIX-01 回执的「发现但未改」；
  - 散文/表格里**非邻接**的裸行号**不判**：跨格引用、文档自定义文件别名（如 `ex :1706`、
    `compat :1896-1907`、`h:89-91`）、以及一行内出现多个文件名时的后续裸行号，都无法在无
    符号绑定的前提下唯一确定目标。实测「同一行最近文件名」绑定口径会**误绑 69 条**，抽样 11
    条全部为跨格/别名误绑（典型：`p1_session.h:19 / :103` 的 `:103` 实指 `p1_session.cpp`，
    绑到 40 行的 `p1_session.h` 即假红）⇒ 本门**不猜**，如实不计入判据；
    同类实测：`docs/science/algorithms/PHASE2_INTEGRATION.md` §9「实现锚」列的裸 `:N`（行内无文件名，
    实现在文档头声明）既非邻接也无行内绑定 ⇒ 不判（FINAL-07 实测该列 7 格中 4 格行号陈旧，已人工订正，
    留档见 run/FINAL-07/审核包/CI/文档与登记类红项收口报告.md §1）。
    该「发现但未判」清单见 RELEASE-02 guard-tools-fix.md（已退役，见仓库 git 历史）。

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
# L1（行数锚）作用域：DOC-DRIFT-FIX-01 由 docs/science/algorithms/*.md 扩到全 docs 面 ——
# docs/contracts/**、docs/modules/** 的「<file>（N 行」长期无门，实测 70 条与源文件不符。
DOC_GLOB = "docs/**/*.md"
# L3（逐符号表行号范围）作用域**有意保持** docs/science/algorithms/*.md：扩到 docs/contracts 会
# 立刻暴露 docs/science/DATA_SEMANTICS.md 两张状态表 35 条符号漂移（rejection.cpp /
# p2_session.cpp 整体位移），订正需逐符号重新推导语义，不属本任务声明的判据面 ——
# 该项已在回执中作为「发现但未改」逐条列出，不得当作已覆盖。
SYMBOL_GLOB = "docs/science/algorithms/*.md"

EXTS = ("cpp", "cc", "cxx", "h", "hpp", "hh", "py", "sh", "ps1", "json",
        "yaml", "yml", "md", "cmake", "in", "txt")
_FILE = (r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*"
         r"[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:" + "|".join(EXTS) + r"))")
FILE_RE = re.compile(_FILE)
# 行数锚：<file>（N 行 / <file> (N 行 / <file> N 行（右括号可缺，如「（2076 行，astrocs_phase2 静态库」）
COUNT_RE = re.compile(_FILE + r"[ \t]*[（(]?[ \t]*(\d+)[ \t]*行")
# 裸行号范围：:N / :N-M / :N–M
RANGE_RE = re.compile(r":L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?")
# L1b **邻接**裸行号锚：文件名 token 之后仅允许反引号/星号/空白 + 至多一个冒号，紧随 `:N` / `:N-M`。
# 只判这一确凿形态；跨格/别名/一行多文件名的非邻接裸行号不可自动判定（见文件头「不覆盖」）。
# 注：分隔类用 \x60 表示反引号，以免依赖在其后才定义的 BT 常量。
BARE_ADJ_RE = re.compile(r"^[\s\x60*]{0,2}[:：]?[\s\x60*]{0,2}"
                         r"(:L?\d+(?:\s*[-\u2013]\s*L?\d+)?)")
SEP_RE = re.compile(r"^\s*\|[\s:|-]+\|\s*$")
IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)?$")
ARCHIVE_MARKERS = ("/archive/", "/legacy/", "/.git/", "/third_party/",
                   "/build/", "/out/", "/run/", "/worktrees/")
BT = chr(96)

# ---- L3 匹配规则（「有区分力」的唯一口径；FINAL-07 收紧，只加严不放松） ----
# 旧口径 = 符号子串出现在**区间原文**里即判绿。两处实测假绿（同一张表、同一棵树）：
#   `INVALID_INPUT :2045-2054`：区间内只有 :2051 的**注释**提到该字样，而实现站点在 :2060；
#   `INVALID_METHOD :2020-2033（status :2037）`：status 赋值在 :2040，区间内只有 :2030 的注释命中。
# ⇒ 旧口径对「锚停在散文上」的形态没有判别力（无区分力 token 的等价形态）。
# 新口径两步：① 先剥掉注释与字符串字面量（code_lines）；② 再要求标识符**段**命中
# （sym_in_code 三种形态，皆为**代码内**逐字可复测的标识符，不是散文 token）：
#   ① 本形：匹配段两侧不得再是 [A-Za-z0-9]，下划线计边界 ⇒ `MIN_SAMPLES` 命中
#      `P2_STATUS_MIN_SAMPLES`；
#   ② 前缀族：以 `_` 结尾者按前缀匹配 ⇒ `P2_SEMANTIC_` 命中 `P2_SEMANTIC_NONE`；
#   ③ 形态族：严格小驼峰字段名（`^[a-z][a-z0-9]*(?:[A-Z][a-z0-9]*)+$`）额外接受其
#      snake_case 写法 ⇒ 字段 `maxStars` 命中其传递参数 `max_stars`（同一实体在代码里的
#      两种拼写；实测例 = STAR_DETECTION_ALGORITHMS.md:279 的截断段 `sdet_emit_records`，
#      其区间内只有参数 `max_stars`，字段名本身只出现在注释里）。
#      形态族**不**放行散文：snake 形态同样必须落在代码行，否则仍判 L3_prose_only。
# 命中仅落在注释/字符串 ⇒ L3_prose_only。
# 不覆盖（如实声明）：本规则不判首列 token 的**语义歧义**（`OK` 与 `P2_STATUS_OK` 视为同段，
# 这是本表头「首列 = 文档侧状态名」的既有约定）；残余风险 = 区间内出现同段名的**其他**标识符，
# 实测本仓 0 例。
C_LIKE_EXT = ("c", "cc", "cpp", "cxx", "h", "hh", "hpp", "in")
HASH_COMMENT_EXT = ("cmake", "ps1", "py", "sh", "yaml", "yml")


def _skip_literal(line, i):
    """跳过 line[i] 起的字符串字面量，返回其后的下标（未闭合则到行尾）。"""
    quote = line[i]
    j = i + 1
    while j < len(line):
        if line[j] == "\\":
            j += 2
            continue
        if line[j] == quote:
            return j + 1
        j += 1
    return len(line)


def code_lines(path, body):
    """剥掉注释与字符串字面量后的行文本；非源码扩展名原样返回（无注释语法）。"""
    ext = path.rsplit(".", 1)[-1].lower() if "." in path else ""
    if ext not in C_LIKE_EXT and ext not in HASH_COMMENT_EXT:
        return list(body)
    out = []
    in_block = False
    for line in body:
        buf = []
        i = 0
        if ext in C_LIKE_EXT:
            while i < len(line):
                if in_block:
                    j = line.find("*/", i)
                    if j < 0:
                        i = len(line)
                        break
                    in_block = False
                    i = j + 2
                    continue
                if line.startswith("//", i):
                    break
                if line.startswith("/*", i):
                    in_block = True
                    i += 2
                    continue
                if line[i] in "\"'":
                    i = _skip_literal(line, i)
                    continue
                buf.append(line[i])
                i += 1
        else:
            while i < len(line):
                if line[i] in "\"'":
                    i = _skip_literal(line, i)
                    continue
                if line[i] == "#":
                    break
                buf.append(line[i])
                i += 1
        out.append("".join(buf))
    return out


CAMEL_RE = re.compile(r"^[a-z][a-z0-9]*(?:[A-Z][a-z0-9]*)+$")


def snake_form(sym):
    """严格小驼峰字段名 -> 其 snake_case 写法；非该形态返回 None（口径见「L3 匹配规则」）。"""
    if not CAMEL_RE.match(sym):
        return None
    return re.sub(r"(?<=[a-z0-9])([A-Z])", r"_\1", sym).lower()


def sym_in_code(sym, text):
    """符号在**代码文本**里的标识符段/前缀族/形态族命中（口径见上方「L3 匹配规则」）。"""
    cands = [sym]
    snake = snake_form(sym)
    if snake and snake != sym:
        cands.append(snake)
    for cand in cands:
        prefix_family = cand.endswith("_")
        for m in re.finditer(re.escape(cand), text):
            if m.start() > 0 and text[m.start() - 1].isalnum():
                continue
            if not prefix_family and m.end() < len(text) and text[m.end()].isalnum():
                continue
            return True
    return False


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
    counts = {"count_anchors": 0, "symbol_anchors": 0, "bare_range_anchors": 0}
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

    # L1b：**邻接**裸行号锚界内判定（紧贴文件名的 `file:N` / `file:N-M`）。
    # 绑定规则保守到"只判紧贴"：文件名 token 之后仅允许反引号/星号/空白 + 至多一个冒号，
    # 紧随 `:N`/`:N-M`。跨格、文件别名（ex/compat/h）、一行内多文件名的非邻接裸行号不判
    # （实测按"同行最近文件名"绑定会误绑 69 条，抽样 11/11 为跨格/别名误绑，见文件头「不覆盖」）。
    # 未解析的同名 token 不在此报错：本 pass 只判**界内**，不扩 L2 解析面（避免把散文里的
    # 文件名提及误判为锚）。
    for i, line in enumerate(lines, 1):
        for fmo in FILE_RE.finditer(line):
            base = fmo.group(1)
            msep = BARE_ADJ_RE.match(line[fmo.end():])
            if not msep:
                continue
            path, _how = resolve(root, doc, base, basename_index)
            if path is None:
                continue
            m = RANGE_RE.match(msep.group(1))
            if not m:
                continue
            s0 = int(m.group(1))
            e0 = int(m.group(2) or m.group(1))
            body = read_lines(os.path.join(root, path))
            counts["bare_range_anchors"] += 1
            rec = {"doc": doc, "doc_line": i, "raw": "%s%s" % (base, m.group(0)),
                   "kind": "bare_range", "target": path, "start": s0, "end": e0}
            if s0 < 1 or e0 < s0 or e0 > len(body):
                rec["status"] = "OUT_OF_BOUNDS"
                findings.append({"code": "L2_range_out_of_bounds_bare",
                                 "detail": "%s:%d %s%s -> %s has %d lines"
                                           % (doc, i, base, m.group(0), path, len(body))})
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
                    code = code_lines(path, body)
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
                        elif any(sym_in_code(sym, b) for b in code[s0 - 1:e0]):
                            rec["status"] = "OK"
                        elif any(sym in b for b in body[s0 - 1:e0]):
                            hits = [q + 1 for q, b in enumerate(code) if sym_in_code(sym, b)]
                            rec["status"] = "PROSE_ONLY"
                            rec["actual_lines"] = hits[:6]
                            findings.append({"code": "L3_prose_only",
                                             "detail": "%s:%d sym=%s %s:%d-%d 区间内仅注释/字符串"
                                                       "命中（散文命中无区分力；实现站点不在该区间，"
                                                       "实测代码行 %s）"
                                                       % (doc, j + 1, sym, path, s0, e0, hits[:6])})
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
    counters = {"count_anchors": 0, "symbol_anchors": 0, "bare_range_anchors": 0}
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
    if counters["bare_range_anchors"] == 0:
        errors.append({"code": "L0_scan_floor",
                       "detail": "邻接裸行号锚提取数 = 0（扫描面塌缩，禁止空转判绿）"})
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
    print("ALG_LINE_ANCHORS_PASS: %d docs, 行数锚 %d, 逐符号行号锚 %d, 邻接裸行号锚 %d, "
          "全部与源文件实测一致"
          % (report["documents"], report["counters"].get("count_anchors", 0),
             report["counters"].get("symbol_anchors", 0),
             report["counters"].get("bare_range_anchors", 0)))
    return 0


# ---------------------------------------------------------------- self-test
_SYNTH_DOC = """# SYNTH ALG

> 模块: lib/x/synth.cpp（5 行）+ 头 lib/x/synth.h（4 行，实测 2026-01-01）

## 1 逐符号锚

| 符号 | 声明（synth.h） | 实现（synth.cpp） | 语义 |
|---|---|---|---|
| p2_synth_run | :2 | :3-4 | 入口 |
| kSynthMagic | :3 | :2 | 常量 |

> 邻接裸行号锚（L1b）: lib/x/synth.cpp:3-4（入口实现区）。
"""
_SYNTH_H = "// synth.h\nint p2_synth_run(void);\nstatic const int kSynthMagic = 7;\n// end\n"
_SYNTH_CPP = "// synth.cpp\nstatic const int kSynthMagic = 7;\nint p2_synth_run(void) {\n  return kSynthMagic;\n}\n"

# N9 夹具（FINAL-07 收紧后的 L3 匹配规则）：同名符号同时有「注释里的散文命中」（:2）
# 与「代码站点」（:3 定义 / :3-4 使用），用来证明「散文命中不构成绑定」既有判别力、
# 又不误伤前缀族写法（`P2_SEMANTIC_` 形态）。
_SYNTH_CPP_PROSE = ("// synth.cpp\n"
                    "// 说明: kSynthMagic 取 7（本行是注释，不是实现站点）\n"
                    "static const int kSynthMagic = 7;\n"
                    "int p2_synth_run(void) {\n"
                    "  return kSynthMagic;\n"
                    "}\n")
_SYNTH_DOC_PROSE = """# SYNTH ALG

> 模块: lib/x/synth.cpp（6 行）+ 头 lib/x/synth.h（4 行，实测 2026-01-01）

| 符号 | 声明（synth.h） | 实现（synth.cpp） | 语义 |
|---|---|---|---|
| kSynthMagic | :3 | :2 | 常量（负例：锚停在注释行） |

> 邻接裸行号锚（L1b）: lib/x/synth.cpp:3-6（入口实现区）。
"""
_SYNTH_DOC_CODE = _SYNTH_DOC_PROSE.replace(
    "| :2 |", "| :3 |").replace("常量（负例：锚停在注释行）", "常量（正例：锚在代码站点）")
_SYNTH_DOC_FAMILY = _SYNTH_DOC_CODE.replace(
    "| kSynthMagic | :3 | :3 | 常量（正例：锚在代码站点） |",
    "| kSynthMagic | :3 | :3 | 常量（正例：锚在代码站点） |\n"
    "| p2_synth_ | :2 | :4-5 | 前缀族（形如 P2_SEMANTIC_ 命中 P2_SEMANTIC_NONE） |")


# N10 夹具（形态族）：字段名 maxStars 只作为**注释**出现于实现文件，代码里只有其
# 传递参数 max_stars（同一实体的 snake_case 拼写）。证明形态族①接受代码内的另一拼写、
# ②不放行散文命中、③不放行符号缺失。
_SYNTH_H_CAMEL = "// synth.h\nlong long maxStars;\n// end\n"
_SYNTH_CPP_CAMEL = ("// synth_camel.cpp\n"
                    "static long long sanitize(long long max_stars) {\n"
                    "    if (max_stars < 0) return 0;\n"
                    "    return max_stars;\n"
                    "}\n")
_SYNTH_CPP_CAMEL_PROSE = ("// synth_camel.cpp\n"
                          "// 说明: maxStars 在输出段截断（本行是注释）\n"
                          "static long long sanitize(long long v) {\n"
                          "    return v;\n"
                          "}\n")
_SYNTH_CPP_CAMEL_NONE = ("// synth_camel.cpp\n"
                         "// 说明: 与统计无关\n"
                         "static long long sanitize(long long v) {\n"
                         "    return v;\n"
                         "}\n")
_SYNTH_DOC_CAMEL = """# SYNTH ALG

> 模块: lib/x/synth.cpp（5 行）+ 头 lib/x/synth.h（3 行，实测 2026-01-01）

| 符号 | 声明（synth.h） | 实现（synth.cpp） | 语义 |
|---|---|---|---|
| maxStars | :2 | :2-4 | 形态族（字段 maxStars vs 传递参数 max_stars） |

> 邻接裸行号锚（L1b）: lib/x/synth.cpp:2-4（形态族代码站点）。
"""


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
    _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC)
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
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC.replace("synth.cpp", "nosuch.cpp", 1))
        expect("N4 锚目标不存在", root, "FAIL", "L2_target_unresolved")
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC)
        # N5 扫描面塌缩（0 行数锚 且 0 符号锚）⇒ L0 fail-closed
        _write(root, "docs/science/algorithms/SYNTH.md", "# 空文档\n无锚\n")
        expect("N5 扫描面塌缩", root, "FAIL", "L0_scan_floor")
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC)
        # N6 文档根缺失 ⇒ L0 fail-closed
        empty = tempfile.mkdtemp(prefix="alg_anchor_empty_")
        try:
            expect("N6 文档根缺失", empty, "FAIL", "L0_scan_floor")
        finally:
            shutil.rmtree(empty, ignore_errors=True)
        # N7 未跟踪文件（git 仓库里新增未 add）⇒ L2_target_unresolved
        _write(root, "lib/x/extra.cpp", "// extra\n")
        _write(root, "docs/science/algorithms/SYNTH.md",
               _SYNTH_DOC + "\n> 旁证 lib/x/extra.cpp（1 行）\n")
        expect("N7 锚指向未跟踪文件", root, "FAIL", "L2_target_unresolved")
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC)
        os.remove(os.path.join(root, "lib/x/extra.cpp"))
        expect("N7' 恢复后回绿", root, "PASS")
        # N8 邻接裸行号锚（L1b）：该类锚在本次修判据前**完全无判据**（判据缺口）。
        #   正例 = 界内必须判绿（防止新判据误伤既有文档）；
        #   负例 = 越界必须判红（证明新判据有判别力，不是恒绿装饰）。
        _write(root, "docs/science/algorithms/SYNTH.md",
               _SYNTH_DOC + "\n> 旁证 lib/x/synth.cpp:1-5 与 lib/x/synth.h:2（均为实测界内）。\n")
        expect("N8 邻接裸行号锚界内", root, "PASS")
        _write(root, "docs/science/algorithms/SYNTH.md",
               _SYNTH_DOC + "\n> 旁证 lib/x/synth.cpp:1-99（超出实测 5 行）。\n")
        expect("N8' 邻接裸行号锚越界", root, "FAIL", "L2_range_out_of_bounds_bare")
        _write(root, "docs/science/algorithms/SYNTH.md",
               _SYNTH_DOC + "\n> 旁证 lib/x/synth.cpp:1-5 与 lib/x/synth.h:2（均为实测界内）。\n")
        expect("N8'' 恢复后回绿", root, "PASS")
        # N9 L3「有区分力」口径：符号只在**注释**里出现 ⇒ 必红（L3_prose_only，旧口径假绿）；
        #   锚改到代码站点 ⇒ 回绿；前缀族符号（以 _ 结尾）在代码站点 ⇒ 绿（防新口径误伤）。
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP_PROSE)
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC_PROSE)
        expect("N9 散文命中（锚停在注释行）", root, "FAIL", "L3_prose_only")
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC_CODE)
        expect("N9' 锚改到代码站点后回绿", root, "PASS")
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC_FAMILY)
        expect("N9'' 前缀族符号（P2_SEMANTIC_ 形态）不误伤", root, "PASS")
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP)
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC)
        # N10 形态族（camelCase 字段 vs snake_case 传递参数）
        _write(root, "lib/x/synth.h", _SYNTH_H_CAMEL)
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP_CAMEL)
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC_CAMEL)
        expect("N10 形态族（代码内 snake 拼写）", root, "PASS")
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP_CAMEL_PROSE)
        expect("N10' 形态族只命中注释", root, "FAIL", "L3_prose_only")
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP_CAMEL_NONE)
        expect("N10'' 形态族符号缺失", root, "FAIL", "L3_symbol_absent")
        _write(root, "lib/x/synth.h", _SYNTH_H)
        _write(root, "lib/x/synth.cpp", _SYNTH_CPP)
        _write(root, "docs/science/algorithms/SYNTH.md", _SYNTH_DOC)
        expect("N10''' 恢复后回绿", root, "PASS")
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
    print("ALG_LINE_ANCHORS_SELFTEST_PASS: %d 组（正例/负例/恢复三态；含 L3 散文命中、"
          "前缀族、形态族三个判别力用例）全部符合预期" % len(checks))
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
