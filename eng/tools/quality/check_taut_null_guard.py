#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_taut_null_guard.py — 恒真/恒假空指针守卫普查判据（TAUT-NULL-GUARD）。

问题形态（本次 Windows 全量构建暴露的语义缺陷面）:
    if ((cs).fault_reported && (faultname) != nullptr) { ... }
                          ^^^^^^^^^^^ 宏形参，全部调用点都传字符串字面量
字面量地址永不为 null ⇒ 该合取项**编译期定值**为真 ⇒ 它对分支选择零贡献，
「故障名不可用」这条失败路径**从未被执行过**。这不是告警噪音，是恒真门
（恒真门没有证据资格，AGENTS.md §5）。

判据口径（可执行，不是文本黑名单）:
  1. 扫描面 = 工作树全部 C/C++ 源与头（.c/.cc/.cpp/.cxx/.h/.hh/.hpp/.hxx/.inl/.ipp），
     排除构建/过程产物目录（build、run、.git、gaia、testdata 等）——
     **不依赖 git**（GITDECOUPLE-01 同款纪律：非 git 树走同一条代码路径）。
  2. 命中形态 = 「某表达式 (==|!=) nullptr」。对每个命中做**静态定值**分类:
       - 定值为「非空常量」（字符串字面量地址 / 静态存储期对象地址 / 函数地址 /
         宏展开成上述任一）⇒ 判红:
             !=  nullptr ⇒ always_true_guard  （真分支恒真，假分支不可达）
             ==  nullptr ⇒ always_false_guard （该分支恒假，永不执行）
       - 定值为「可空运行期值」（函数调用结果、局部变量/形参/成员、库函数返回）
             ⇒ 合法判空，不判红（**不许一律判红**）。
       - 宏形参间接面：守卫操作数是某个函数式宏的形参 ⇒ 解析该宏**全仓调用点**，
         只有当**每一个**实参都是「可证明非空」的常量时，才判红
         （macro_param_always_nonnull）；出现任何一个可空实参或 nullptr 实参 ⇒ 合法。
       - 不可判定 ⇒ UNRESOLVED，**必须逐条打印原因与调用点数**，不得静默计入绿。
  3. 失败路径证据面（恒真门无证据资格的反向强制）: 对项目自有测试断言宏
     （形参名含 fault/inj 且体引用注入注册表），要求**存在可执行的负例调用点**
     （实参为 nullptr 或空串），否则判红 macro_fault_guard_without_negative_case。
     —— 「守卫改成运行期判定但没有任何负例调用」同样判红。
  4. 面隔离（三面显式，互不混计）:
       * project           项目面（默认门，只对它判红）;
       * third_party       第三方隔离面（third_party 目录组件 / vendored 单头 /
                           healpix_db/archive/legacy）—— 单独计数单独打印;
       * self_test_fixture 判据自带夹具面（含**故意判红的负例样本**）—— 该面
         **必须非空**（空 ⇒ 负例证据丢失 ⇒ 门禁判红），因此是自校验而不是豁免。
  5. 豁免面 = 空。本脚本无 allowlist / skip 列表 / 抑制开关。
  6. 内存纪律: 逐文件流式分析（不把全仓文本同时驻留内存），满足受限节点的内存预算。

判据只加严不下放: 判红集合随代码单调收缩（新增形态只会更红），
唯一的「放宽」是 --fail-on-third-party / --fail-on-fixture 这类**额外加严**开关。

用法:
  python3 eng/tools/quality/check_taut_null_guard.py                  # 真仓普查 + 门
  python3 eng/tools/quality/check_taut_null_guard.py --repo <fixture> # 夹具复跑
  python3 eng/tools/quality/check_taut_null_guard.py --self-test      # 正例/负例/恢复三态
  python3 eng/tools/quality/check_taut_null_guard.py --json-out <path>
  python3 eng/tools/quality/check_taut_null_guard.py --list           # 打印 UNRESOLVED 明细

退出码: 0=PASS; 1=FAIL（项目面有判红命中/夹具面为空）; 2=ANCHOR_STALE（扫描面不可用）。
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import tempfile
import re
import sys

CXX_SUFFIXES = (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp")
EXCLUDE_DIR_PARTS = frozenset({
    "build", "run", ".git", ".github", "gaia", "testdata", "node_modules",
    "__pycache__", "artifacts", ".cache", "out", "cmake-build-debug",
})
THIRD_PARTY_DIR_PARTS = frozenset({"third_party"})
THIRD_PARTY_FILE_NAMES = frozenset({"nanoflann.hpp"})
THIRD_PARTY_PATH_MARKERS = ("/healpix_db/archive/legacy/",)
SELF_TEST_FIXTURE_PREFIX = "eng/tools/quality/fixtures/taut_null_guard/"

NULLPTR_RE = re.compile(r"\bnullptr\b")
IDENT_CHARS = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")
STRING_LITERAL_RE = re.compile(
    r'^(?:u8|u|U|L)?"(?:[^"\\\n]|\\.)*"$|^R"[^()\\]{0,16}\((?:[^)\\]|\\.)*\)"$')
DEFINE_RE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)([ \t]*\(([^)]*)\))?", re.M)
DECL_RE = re.compile(r"([A-Za-z_]\w*)\s*(?:\(|\[|=|;)")
FAULT_PARAM_RE = re.compile(
    r"(?i)^(fault|faultname|inject|injected|injected_fault|.*fault.*|.*inject.*)$")


class GateError(Exception):
    """扫描面/锚不可用 —— fail-closed，映射 rc=2。"""


# --------------------------------------------------------------- text utilities ----
def mask_comments(text: str) -> str:
    """去注释但保留换行与字符串字面量内容（字面量是判据输入，不能被抹掉）。"""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == q:
                    j += 1
                    break
                if text[j] == chr(10) and q == "'":
                    break
                j += 1
            out.append(text[i:j])
            i = j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find(chr(10), i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            seg = text[i:j]
            out.append("".join(ch if ch == chr(10) else " " for ch in seg))
            i = j
            continue
        out.append(c)
        i += 1
    return "".join(out)


def line_of(text: str, pos: int) -> int:
    return text.count(chr(10), 0, pos) + 1


def strip_outer_parens(expr: str) -> str:
    e = expr.strip()
    while e.startswith("(") and e.endswith(")"):
        depth, ok = 0, True
        for idx, ch in enumerate(e):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0 and idx != len(e) - 1:
                    ok = False
                    break
        if not ok or depth != 0:
            break
        e = e[1:-1].strip()
    return e


def _trim_backward(text: str, end: int) -> int:
    i = end - 1
    while i >= 0 and text[i] in " \t":
        i -= 1
    return i + 1


def _literal_start_back(text: str, i: int, lo: int) -> int:
    if text[i - 1:i] != '"':
        return -1
    j = i - 2
    while j > lo:
        if text[j] == '"' and text[j - 1] != "\\":
            return j
        j -= 1
    return -1


def expr_backward(text: str, end: int, limit: int = 400) -> str:
    i = _trim_backward(text, end)
    lo = max(0, i - limit)
    while i > lo:
        if text[i - 1] == '"':
            s = _literal_start_back(text, i, lo)
            if s < 0:
                break
            i = s
            continue
        c = text[i - 1]
        if c in ")]":
            depth, j = 0, i - 1
            while j > lo:
                if text[j] in ")]":
                    depth += 1
                elif text[j] in "([":
                    depth -= 1
                    if depth == 0:
                        break
                j -= 1
            if depth != 0:
                break
            i = j
            continue
        if c in IDENT_CHARS or c in ".:&*":
            i -= 1
            if c == "&" and i >= 1 and text[i - 1] == "&":
                break
            if c in "&*":
                continue
            while i > lo and text[i - 1] in IDENT_CHARS:
                i -= 1
            continue
        break
    return text[i:end].strip()


def expr_forward(text: str, start: int, limit: int = 400) -> str:
    n = len(text)
    i = start
    while i < n and text[i] in " \t":
        i += 1
    hi = min(n, i + limit)
    while i < hi:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < hi:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == '"':
                    j += 1
                    break
                j += 1
            i = j
            continue
        if c in "([":
            depth, j = 0, i
            while j < hi:
                if text[j] in "([":
                    depth += 1
                elif text[j] in ")]":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            if depth != 0:
                break
            i = j + 1
            continue
        if c in IDENT_CHARS or c in ".:&*":
            i += 1
            if c == "&" and i < hi and text[i] == "&":
                break
            if c in "&*":
                continue
            while i < hi and text[i] in IDENT_CHARS:
                i += 1
            continue
        break
    return text[start:i].strip()


def split_top_level_args(text: str) -> list:
    args, depth, cur, i, n = [], 0, [], 0, len(text)
    while i < n:
        c = text[i]
        if c in ('"', "'"):
            q = c
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == q:
                    j += 1
                    break
                j += 1
            cur.append(text[i:j])
            i = j
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    args.append("".join(cur).strip())
    return args


# ------------------------------------------------------------------- the gate ----
FINDING_CLASSES = (
    "always_true_guard",             # != nullptr 且操作数可静态定值为非空 → 恒真
    "always_false_guard",            # == nullptr 且操作数可静态定值为非空 → 恒假
    "macro_param_always_nonnull",    # 宏形参守卫: 全仓每个实参都是可证明非空常量
    "macro_param_nullable",          # 宏形参守卫: 存在可空实参 ⇒ 合法判空
    "macro_fault_guard_without_negative_case",  # 故障注入断言宏缺可执行负例
    "legit_null_check",              # 对运行期值的合法判空
)


def scan_repo(root: pathlib.Path):
    """两遍流式扫描: pass1 逐文件抽形态与宏定义; pass2 按需抽宏调用点。

    内存: 同一时刻只驻留一个文件的掩码文本（不把全仓文本同时装入内存）。
    """
    files = iter_sources(root)
    findings = []
    param_keys = {}          # (rel, macro, param) -> [site...]
    fault_macros = []        # (rel, macro, param_index, def_line, defs_skip)
    scanned = []
    for ap, rel in files:
        try:
            raw = ap.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise GateError("ANCHOR_UNREADABLE: %s: %s" % (rel, exc))
        ctx = FileCtx(rel, mask_comments(raw))
        scanned.append((ap, rel, is_third_party(rel)))
        findings.extend(scan_file(ctx))
        for macro, info in ctx.macros.items():
            for i, p in enumerate(info["params"]):
                if FAULT_PARAM_RE.match(p.strip()) and (
                        "injected(" in info["body"] or "FaultRegistry" in info["body"]):
                    fault_macros.append((rel, macro, i, info["lines"][0]))
        del ctx, raw

    # pass 2: 只为 pass1 命中的形参守卫解析调用点
    for (rel, macro, param), sites in sorted(param_keys.items()):
        calls = []
        for ap, r2 in files:
            if is_self_test_fixture(r2) or is_third_party(r2):
                pass
            text = mask_comments(ap.read_text(encoding="utf-8", errors="replace"))
            for line, args in find_call_args_in(text, macro, skip_defs=True):
                calls.append((r2, line, args))
            del text
        idx = None
        for site in sites:
            pass
        findings.extend(judge_param_sites(rel, macro, param, sites, calls, files))

    # 负例证据面
    for rel, macro, pidx, defline in fault_macros:
        negatives = []
        total_calls = 0
        for ap, r2 in files:
            text = mask_comments(ap.read_text(encoding="utf-8", errors="replace"))
            for line, args in find_call_args_in(text, macro, skip_defs=True):
                total_calls += 1
                if pidx >= len(args):
                    continue
                a = strip_outer_parens(args[pidx])
                if a == "nullptr" or a in ('""', 'u8""', 'L""'):
                    negatives.append("%s:%d 实参=%s" % (r2, line, a))
            del text
        if negatives:
            continue
        findings.append({
            "file": rel, "line": defline, "operator": "-", "operand": "-",
            "operand_verdict": "nonnull_const", "operand_kind": "fault_macro_param",
            "code": macro, "verdict": "RED",
            "klass": "macro_fault_guard_without_negative_case",
            "surface": surface_of(rel),
            "reason": "宏 %s 的故障名守卫无可执行负例调用（nullptr/空串实参）⇒ "
                      "守卫失败路径无证据，恒真门没有证据资格" % macro,
            "call_sites": total_calls,
        })
    return scanned, findings


def scan_file(ctx: FileCtx):
    """单个文件里的空指针守卫形态普查。"""
    out = []
    text = ctx.text
    for m in NULLPTR_RE.finditer(text):
        pos = m.start()
        line = line_of(text, pos)
        i = _trim_backward(text, pos)
        left, op = None, None
        if i >= 2 and text[i - 2:i] in ("!=", "=="):
            op = text[i - 2:i]
            left = expr_backward(text, i - 2)
        if left is None:
            j = m.end()
            while j < len(text) and text[j] in " \t":
                j += 1
            if text[j:j + 2] in ("==", "!="):
                op = text[j:j + 2]
                left = expr_forward(text, j + 2)
        if not left:
            continue
        op = op.strip()
        v, kind, detail = classify_operand(left, ctx, line)
        code = text.split(chr(10))[line - 1].strip()
        site = {"file": ctx.rel, "line": line, "operator": op, "operand": left,
                "operand_verdict": v, "operand_kind": kind, "detail": detail,
                "code": code[:160], "surface": ctx.surface}
        if v == "nonnull_const":
            site.update(verdict="RED",
                        klass=("always_true_guard" if op == "!=" else "always_false_guard"),
                        reason=detail)
            out.append(site)
        elif v == "macro_param":
            out.append(site)          # 交由 pass2 判定（需要全仓调用点）
        else:
            site.update(verdict="OK", klass="legit_null_check", reason=detail)
            out.append(site)
    return out


# ------------------------------------------------------------------ repo scanning ----
def iter_sources(root: pathlib.Path):
    """工作树枚举（不依赖 git）。返回 (abs_path, rel_posix, is_third_party)。"""
    if not root.is_dir():
        raise GateError("ANCHOR_MISSING: repo root %s" % root)
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames
                             if d not in EXCLUDE_DIR_PARTS and not d.startswith(".git"))
        for fn in sorted(filenames):
            if not fn.endswith(CXX_SUFFIXES):
                continue
            ap = pathlib.Path(dirpath) / fn
            out.append((ap, ap.relative_to(root).as_posix()))
    if not out:
        raise GateError("ANCHOR_STALE: 源码扫描面为空 (root=%s)" % root)
    return out


def is_self_test_fixture(rel: str) -> bool:
    # 判据自带的正/负例夹具面（含故意判红的负例样本）
    return rel.startswith(SELF_TEST_FIXTURE_PREFIX)


def is_third_party(rel: str) -> bool:
    parts = rel.split("/")
    if any(p in THIRD_PARTY_DIR_PARTS for p in parts):
        return True
    if parts[-1] in THIRD_PARTY_FILE_NAMES:
        return True
    return any(m in ("/" + rel) for m in THIRD_PARTY_PATH_MARKERS)


def surface_of(rel: str) -> str:
    if is_self_test_fixture(rel):
        return "self_test_fixture"
    return "third_party" if is_third_party(rel) else "project"


class FileCtx:
    """单文件分析上下文（只在本文件分析期间存活 ⇒ 内存 O(单文件)）。"""

    def __init__(self, rel: str, text: str):
        self.rel = rel
        self.text = text
        self.surface = surface_of(rel)
        self.macros = {}
        self.obj_macros = {}
        self.declared = set()
        self._parse_defines()
        self._parse_declarations()

    def _parse_defines(self):
        for m in DEFINE_RE.finditer(self.text):
            name, params = m.group(1), m.group(3)
            start = line_of(self.text, m.start())
            n = len(self.text)
            end = m.end()
            while True:
                nl = self.text.find(chr(10), end)
                if nl < 0:
                    end = n
                    break
                line_start = end
                k = nl - 1
                while k >= line_start and self.text[k] in " \t":
                    k -= 1
                end = nl + 1
                if k >= line_start and self.text[k] == "\\":
                    continue
                break
            body = self.text[m.end():end]
            stop = line_of(self.text, end - 1 if end > 0 else 0)
            if params is not None:
                self.macros[name] = {
                    "params": [p.strip() for p in params.split(",") if p.strip()],
                    "body": body, "lines": (start, stop)}
            else:
                self.obj_macros[name] = body

    def macro_params_at(self, line: int) -> dict:
        return {n: set(i["params"]) for n, i in self.macros.items()
                if i["lines"][0] <= line <= i["lines"][1]}

    def _parse_declarations(self):
        # 宽松声明索引（逐行、行内简单正则, O(总字符数)）。宁可少报 UNRESOLVED
        # 也不误报 RED：本集合只影响 OK/UNRESOLVED 标签，不参与任何判红判定。
        for ln in self.text.split(chr(10)):
            for m in DECL_RE.finditer(ln):
                self.declared.add(m.group(1))

    def declared_static(self, name: str) -> bool:
        pats = (r"^%s\s*(?:=|\[|;)" % re.escape(name),
                r"^static\b[^;\n]*\b%s\b" % re.escape(name),
                r"^constexpr\b[^;\n]*\b%s\b" % re.escape(name),
                r"^[A-Za-z_][\w:<>,\s\*&]*\b%s\s*\([^;]*\)\s*(?:const\s*)?\{" % re.escape(name))
        return any(re.search(p, self.text, re.M) for p in pats)


def classify_operand(expr: str, ctx: FileCtx, line: int, depth: int = 0):
    e = strip_outer_parens(expr)
    if not e:
        return "unresolved", "empty", "表达式为空"
    if e == "nullptr":
        return "null_const", "nullptr_literal", "与 nullptr 比较的常量"
    if re.fullmatch(r"(?:0|0ULL|0L|nullptr_t\(\))", e):
        return "null_const", "integer_zero", "整型 0 空指针常量"
    if STRING_LITERAL_RE.match(e):
        return "nonnull_const", "string_literal", "字符串字面量地址永不为 null"
    if e.startswith("&"):
        rest = strip_outer_parens(e[1:].strip())
        if STRING_LITERAL_RE.match(rest):
            return "nonnull_const", "literal_address", "字面量取地址: 地址常量非空"
        if re.fullmatch(r"[A-Za-z_]\w*(::\w+)*", rest):
            if ctx.declared_static(rest.split("::")[-1]):
                return "nonnull_const", "static_address", \
                    "&%s: 静态存储期对象/函数地址非空" % rest
            return "runtime", "address_of_local", "&%s: 非文件域静态, 判为运行期" % rest
        return "unresolved", "ampersand_expr", "&%s: 取地址对象不可静态定值" % rest
    if re.fullmatch(r"true|false", e):
        return "unresolved", "bool_literal", "bool 字面量与 nullptr 比较"
    if re.match(r"^[A-Za-z_]\w*(?:::\w+)*$", e):
        name = e.split("::")[-1]
        if name in ctx.obj_macros and depth < 8:
            first = ctx.obj_macros[name].strip().split(" ")[0]
            v, k, d = classify_operand(first, ctx, line, depth + 1)
            if v in ("nonnull_const", "null_const"):
                return v, "macro_expansion_" + k, "对象式宏 %s 展开为常量: %s" % (name, d)
            return v, k, d
        for macro, plist in ctx.macro_params_at(line).items():
            if name in plist:
                return "macro_param", "macro_parameter", "宏 %s 的形参 %s" % (macro, name)
        if name in ctx.declared or name == "this":
            return "runtime", "variable", "变量/成员 %s 的值运行期决定" % e
        return "unresolved", "unresolved_identifier", "标识符 %s 在本文件无声明证据" % e
    if re.search(r"\(", e) and e.endswith(")"):
        return "runtime", "call_result", "函数调用结果 %s: 运行期可空" % e.split("(")[0]
    if re.search(r"(\[|\.|->)", e):
        return "runtime", "member_or_index", "成员/下标取值 %s: 运行期可空" % e
    return "unresolved", "unparsed", "无法静态定值: %s" % e


def _read_call_args(text: str, open_pos: int):
    depth, i, n = 0, open_pos, len(text)
    while i < n:
        c = text[i]
        if c in ('"', "'"):
            q = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == q:
                    i += 1
                    break
                i += 1
            continue
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
            if depth == 0:
                return split_top_level_args(text[open_pos + 1:i]), i + 1
        i += 1
    return None, n


# ------------------------------------------------------------------- the gate ----
FINDING_CLASSES = (
    "always_true_guard",             # != nullptr 且操作数可静态定值为非空 → 恒真
    "always_false_guard",            # == nullptr 且操作数可静态定值为非空 → 恒假
    "macro_param_always_nonnull",    # 宏形参守卫: 全仓每个实参都是可证明非空常量
    "macro_param_nullable",          # 宏形参守卫: 存在可空实参 ⇒ 合法判空
    "macro_fault_guard_without_negative_case",  # 故障注入断言宏缺可执行负例
    "legit_null_check",              # 对运行期值的合法判空
)


def scan_file(ctx: FileCtx):
    """单个文件里的空指针守卫形态普查。"""
    out = []
    text = ctx.text
    for m in NULLPTR_RE.finditer(text):
        pos = m.start()
        line = line_of(text, pos)
        i = _trim_backward(text, pos)
        left, op = None, None
        if i >= 2 and text[i - 2:i] in ("!=", "=="):
            op = text[i - 2:i]
            left = expr_backward(text, i - 2)
        if left is None:
            j = m.end()
            while j < len(text) and text[j] in " \t":
                j += 1
            if text[j:j + 2] in ("==", "!="):
                op = text[j:j + 2]
                left = expr_forward(text, j + 2)
        if not left:
            continue
        op = op.strip()
        v, kind, detail = classify_operand(left, ctx, line)
        code = text.split(chr(10))[line - 1].strip()
        site = {"file": ctx.rel, "line": line, "operator": op, "operand": left,
                "operand_verdict": v, "operand_kind": kind, "detail": detail,
                "code": code[:160], "surface": ctx.surface}
        if v == "nonnull_const":
            site.update(verdict="RED",
                        klass=("always_true_guard" if op == "!=" else "always_false_guard"),
                        reason=detail)
        elif v == "macro_param":
            site["param"] = strip_outer_parens(left).split("::")[-1]
            for macro, plist in ctx.macro_params_at(line).items():
                if site["param"] in plist:
                    site["macro"] = macro
        else:
            site.update(verdict="OK", klass="legit_null_check", reason=detail)
        out.append(site)
    return out


def collect_calls(files, macros):
    """一遍文件流, 抽出所有相关函数式宏的调用点: {macro: [(rel, line, args)]}。"""
    out = {m: [] for m in macros}
    pats = {m: re.compile(r"\b%s\s*\(" % re.escape(m)) for m in macros}
    for ap, rel in files:
        try:
            text = mask_comments(ap.read_text(encoding="utf-8", errors="replace"))
        except OSError as exc:
            raise GateError("ANCHOR_UNREADABLE: %s: %s" % (rel, exc))
        for m, pat in pats.items():
            for mo in pat.finditer(text):
                if re.search(r"#[ \t]*define[ \t]*$", text[max(0, mo.start() - 40):mo.start()]):
                    continue
                args, _end = _read_call_args(text, mo.end() - 1)
                if args is not None:
                    out[m].append((rel, line_of(text, mo.start()), args))
        del text
    return out


def scan_repo(root: pathlib.Path):
    """两遍流式扫描: pass1 逐文件抽形态与宏定义; pass2 一次性抽相关宏的全部调用点。

    内存: 同一时刻只驻留一个文件的掩码文本（不把全仓文本同时装入内存）。
    """
    files = iter_sources(root)
    findings, scanned = [], []
    param_sites, fault_macros = {}, []
    for ap, rel in files:
        try:
            raw = ap.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise GateError("ANCHOR_UNREADABLE: %s: %s" % (rel, exc))
        ctx = FileCtx(rel, mask_comments(raw))
        scanned.append((ap, rel, is_third_party(rel)))
        for site in scan_file(ctx):
            if site.get("macro"):
                param_sites.setdefault((site["file"], site["macro"], site["param"]),
                                       []).append(site)
            elif site.get("verdict"):
                findings.append(site)
        for macro, info in ctx.macros.items():
            for i, p in enumerate(info["params"]):
                if FAULT_PARAM_RE.match(p.strip()) and (
                        "injected(" in info["body"] or "FaultRegistry" in info["body"]):
                    fault_macros.append((rel, macro, i, info["lines"][0]))
        del ctx, raw

    macros = {k[1] for k in param_sites} | {m for _r, m, _i, _l in fault_macros}
    calls = collect_calls(files, macros) if macros else {}

    for (rel, macro, param), sites in sorted(param_sites.items()):
        callsites = calls.get(macro, [])
        idx = _param_index(files, rel, macro, param)
        kinds, nulls, runtimes, unresolved, examples = [], [], [], [], []
        for crel, cline, args in callsites:
            if idx is None or idx >= len(args):
                unresolved.append((crel, cline, "<实参不足>"))
                continue
            actual = args[idx]
            v, k, _d = classify_operand(actual, FileCtx(crel, ""), cline)
            kinds.append(v)
            if v == "null_const":
                nulls.append(actual)
            elif v == "runtime":
                runtimes.append(actual)
            elif v == "unresolved":
                unresolved.append((crel, cline, actual))
            if len(examples) < 3:
                examples.append("%s:%d 实参=%s (%s)" % (crel, cline, actual, k))
        for site in sites:
            f = dict(site)
            f["call_sites"] = len(callsites)
            f["examples"] = examples
            f.pop("macro", None)
            f.pop("param", None)
            if idx is None:
                f.update(verdict="UNRESOLVED", klass="macro_param_undefined",
                         reason="宏 %s 在本文件没有形参定义, 无法解析守卫" % macro)
            elif not callsites:
                f.update(verdict="UNRESOLVED", klass="macro_param_no_call_site",
                         reason="宏 %s 无全仓调用点证据，守卫可空性不可判定" % macro)
            elif all(k == "nonnull_const" for k in kinds):
                f.update(verdict="RED", klass="macro_param_always_nonnull",
                         reason="宏 %s 的 %d 个调用点实参全为可证明非空常量 ⇒ %s != nullptr 恒真"
                                % (macro, len(callsites), param))
            elif nulls or runtimes:
                f.update(verdict="OK", klass="macro_param_nullable",
                         reason="存在可空实参（守卫可被触发 ⇒ 合法判空）: %s"
                                % ("; ".join((nulls + runtimes)[:2]) or "见 examples"))
            else:
                f.update(verdict="UNRESOLVED", klass="macro_param_unknown",
                         reason="实参分类不完整: %s"
                                % "; ".join("%s:%d %s" % u for u in unresolved[:2]))
            findings.append(f)

    for rel, macro, pidx, defline in sorted(set(fault_macros)):
        negatives = []
        callsites = calls.get(macro, [])
        for crel, cline, args in callsites:
            if pidx >= len(args):
                continue
            a = strip_outer_parens(args[pidx])
            if a == "nullptr" or a in ('""', 'u8""', 'L""'):
                negatives.append("%s:%d 实参=%s" % (crel, cline, a))
        if negatives:
            continue
        findings.append({
            "file": rel, "line": defline, "operator": "-", "operand": "-",
            "operand_verdict": "nonnull_const", "operand_kind": "fault_macro_param",
            "code": macro, "verdict": "RED",
            "klass": "macro_fault_guard_without_negative_case",
            "surface": surface_of(rel),
            "reason": "宏 %s 的故障名守卫无可执行负例调用（nullptr/空串实参）⇒ "
                      "守卫失败路径无证据，恒真门没有证据资格" % macro,
            "call_sites": len(callsites),
        })
    return scanned, findings


_DEF_CACHE = {}


def _param_index(files, rel, macro, param):
    """在定义文件里取 param 的形参下标（带缓存）。"""
    key = (rel, macro, param)
    if key in _DEF_CACHE:
        return _DEF_CACHE[key]
    idx = None
    for ap, r2 in files:
        if r2 != rel:
            continue
        text = mask_comments(ap.read_text(encoding="utf-8", errors="replace"))
        m = re.search(r"^[ \t]*#[ \t]*define[ \t]+%s\(([^)]*)\)"
                      % re.escape(macro), text, re.M)
        if m:
            plist = [p.strip() for p in m.group(1).split(",") if p.strip()]
            if param in plist:
                idx = plist.index(param)
        del text
        break
    _DEF_CACHE[key] = idx
    return idx


def summarize(findings, scanned):
    project_red = [f for f in findings if f["surface"] == "project" and f["verdict"] == "RED"]
    tp_red = [f for f in findings
              if f["surface"] == "third_party" and f["verdict"] == "RED"]
    fixture_red = [f for f in findings
                   if f["surface"] == "self_test_fixture" and f["verdict"] == "RED"]
    unresolved = [f for f in findings if f["verdict"] == "UNRESOLVED"]
    by_class = {}
    for f in findings:
        by_class.setdefault(f["klass"], {"RED": 0, "OK": 0, "UNRESOLVED": 0})[f["verdict"]] += 1
    return {
        "scanned_files": len(scanned),
        "scanned_project_files": sum(1 for s in scanned if not is_third_party(s[1])),
        "scanned_third_party_files": sum(1 for s in scanned if is_third_party(s[1])),
        "total_sites": len(findings),
        "project_red": len(project_red),
        "third_party_red_isolated": len(tp_red),
        "third_party_isolated": True,
        "self_test_fixture_red": len(fixture_red),
        "self_test_fixture_present": len(fixture_red) > 0,
        "unresolved": len(unresolved),
        "by_class": by_class,
    }


def print_report(doc, show_all=False):
    s = doc["summary"]
    print("TAUT-NULL-GUARD %s" % doc["verdict"])
    print("扫描面: %d 个 C/C++ 文件（项目面 %d + 第三方隔离面 %d）"
          % (s["scanned_files"], s["scanned_project_files"], s["scanned_third_party_files"]))
    print("命中形态 %d 处：项目面判红 %d；第三方隔离面判红 %d（不与项目面混计）"
          % (s["total_sites"], s["project_red"], s["third_party_red_isolated"]))
    print("判据自带负例夹具面: 判红 %d 处（**必须非空**: 空 ⇒ 负例证据丢失 ⇒ 门禁判红; "
          "不计入项目面）" % s["self_test_fixture_red"])
    print("UNRESOLVED（不可静态定值，须逐条读）: %d" % s["unresolved"])
    print("分类计数（先归纳类别再定量）:")
    for klass in sorted(s["by_class"]):
        c = s["by_class"][klass]
        print("  - %-42s RED=%d OK=%d UNRESOLVED=%d" % (klass, c["RED"], c["OK"], c["UNRESOLVED"]))
    red = [f for f in doc["findings"] if f["verdict"] == "RED"]
    unres = [f for f in doc["findings"] if f["verdict"] == "UNRESOLVED"]
    shown = red + unres if show_all else red
    for f in shown[:200]:
        print("  [%s] %s:%d  %s  %s"
              % (f["klass"], f["file"], f["line"], f.get("code", "")[:80], f.get("reason", "")))
        for ex in f.get("examples", [])[:3]:
            print("        证据: %s" % ex)
    if len(shown) > 200:
        print("  ... (%d more)" % (len(shown) - 200))


# -------------------------------------------------------------------- self-test ----
FIXTURE_DIR = pathlib.Path(__file__).resolve().parent / "fixtures" / "taut_null_guard"


def _expected_from_fixture(case_dir: pathlib.Path) -> dict:
    """夹具里以独占注释行 // TAUT:<class> 标注紧随其后的守卫行。"""
    want = {}
    for p in sorted(case_dir.rglob("*")):
        if p.suffix not in CXX_SUFFIXES or not p.is_file():
            continue
        for i, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
            m = re.match(r"^\s*(?://|/\*)\s*TAUT:([a-z_]+)\s*(?:\*/)?\s*\\?\s*$", line)
            if m:
                want.setdefault(p.relative_to(case_dir).as_posix(), {})[i + 1] = m.group(1)
    return want


def _run_case(case_dir: pathlib.Path):
    _DEF_CACHE.clear()
    scanned, findings = scan_repo(case_dir)
    return findings, summarize(findings, scanned)


def self_test(args) -> int:
    import shutil
    import tempfile
    if not FIXTURE_DIR.is_dir():
        print("SELFTEST_FAIL: 夹具目录缺失 %s" % FIXTURE_DIR)
        return 1
    failures = []

    def copy_case(name):
        dst = pathlib.Path(tempfile.mkdtemp(prefix="tautguard_%s_" % name))
        shutil.copytree(FIXTURE_DIR / name, dst / "repo")
        return dst / "repo"

    # 态 1（正例）
    tmp = copy_case("positive")
    try:
        findings, summ = _run_case(tmp)
        if summ["project_red"] != 0:
            failures.append("positive: 合法判空面被判红 %d 处 %s"
                            % (summ["project_red"],
                               [(f["file"], f["line"], f["klass"]) for f in findings
                                if f["verdict"] == "RED"][:5]))
        else:
            print("SELFTEST_PASS positive (合法判空 %d 处全部未判红)" % summ["total_sites"])
    finally:
        shutil.rmtree(tmp.parent, ignore_errors=True)

    # 态 2（负例）
    tmp = copy_case("negative")
    try:
        findings, summ = _run_case(tmp)
        got = {}
        for f in findings:
            if f["verdict"] == "RED":
                got.setdefault(f["file"], {})[f["line"]] = f["klass"]
        want = _expected_from_fixture(tmp)
        missing, extra = [], []
        for rel, lines in want.items():
            for ln, klass in lines.items():
                if got.get(rel, {}).get(ln) != klass:
                    missing.append("%s:%d 期望 %s 实得 %s"
                                   % (rel, ln, klass, got.get(rel, {}).get(ln)))
        for rel, lines in got.items():
            for ln, klass in lines.items():
                if want.get(rel, {}).get(ln) != klass:
                    extra.append("%s:%d 实得 %s（夹具未标注）" % (rel, ln, klass))
        if missing or extra:
            failures.append("negative: 判定与标注不一致 missing=%s extra=%s" % (missing, extra))
        else:
            print("SELFTEST_PASS negative (注入 %d 处恒真/恒假形态全部判红，行号对齐)"
                  % sum(len(v) for v in want.values()))
    finally:
        shutil.rmtree(tmp.parent, ignore_errors=True)

    # 态 3（恢复）
    tmp = copy_case("restore")
    try:
        findings, summ = _run_case(tmp)
        if summ["project_red"] != 0:
            failures.append("restore: 处置后仍有判红 %s"
                            % [(f["file"], f["line"], f["klass"]) for f in findings
                               if f["verdict"] == "RED"][:5])
        else:
            print("SELFTEST_PASS restore (处置后判红归零，%d 处形态转为运行期判定)"
                  % summ["total_sites"])
    finally:
        shutil.rmtree(tmp.parent, ignore_errors=True)

    # 态 4（可执行证据）
    if args.no_cc_evidence:
        print("SELFTEST_SKIP cc_evidence (--no-cc-evidence：显式降级，非默认)")
    else:
        rc, detail = _cc_evidence_step()
        if rc != 0:
            failures.append("cc_evidence: %s" % detail)
        else:
            print("SELFTEST_PASS cc_evidence (%s)" % detail)

    if failures:
        print("SELFTEST_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: 正例/负例/恢复/可执行证据 四态全部符合预期")
    return 0


def _cc_evidence_step():
    """编译并运行夹具证据 TU：证明「失败路径」不再不可达（真触发，不是恒真门）。"""
    import shutil
    import subprocess
    cxx = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
    if not cxx:
        return 2, "找不到 C++ 编译器（恒真门无证据资格：证据步不得静默跳过）"
    work = pathlib.Path(tempfile.mkdtemp(prefix="tautguard_cc_"))
    try:
        src = FIXTURE_DIR / "evidence" / "guard_trigger_evidence.cpp"
        if not src.is_file():
            return 2, "证据 TU 缺失: %s" % src
        exe = work / "guard_trigger_evidence"
        cmd = [cxx, "-std=c++17", "-Wall", "-Wextra",
               "-I", str(FIXTURE_DIR / "evidence"),
               "-I", str(FIXTURE_DIR / "restore" / "repo" / "src"),
               str(src), "-o", str(exe)]
        cp = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
        if cp.returncode != 0:
            return 2, "证据 TU 编译失败 rc=%d: %s" % (cp.returncode, cp.stderr[-800:])
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
        if run.returncode != 0:
            return 2, "证据 TU 运行失败 rc=%d: %s%s" % (run.returncode, run.stdout[-500:],
                                                       run.stderr[-500:])
        need = ("EVIDENCE positive", "EVIDENCE null_faultname", "EVIDENCE empty_faultname",
                "EVIDENCE restored")
        missing = [n for n in need if n not in run.stdout]
        if missing:
            return 2, "证据 TU 缺三态输出: %s / stdout=%s" % (missing, run.stdout[-500:])
        return 0, run.stdout.strip().replace(chr(10), " | ")
    except Exception as exc:  # noqa: BLE001
        return 2, "证据步异常: %s" % exc
    finally:
        shutil.rmtree(work, ignore_errors=True)


# ------------------------------------------------------------------------ main ----
def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="恒真/恒假空指针守卫普查判据")
    ap.add_argument("--repo", default=str(pathlib.Path(__file__).resolve().parents[3]))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--list", action="store_true", help="打印 UNRESOLVED 明细")
    ap.add_argument("--fail-on-third-party", action="store_true",
                    help="把第三方隔离面也纳入门（加严开关）")
    ap.add_argument("--fail-on-fixture", action="store_true",
                    help="把判据自带夹具面也纳入门（加严开关）")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--no-cc-evidence", action="store_true",
                    help="跳过证据步编译运行（显式降级，默认不跳过）")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test(args)

    root = pathlib.Path(args.repo).resolve()
    try:
        scanned, findings = scan_repo(root)
    except GateError as exc:
        print("TAUT-NULL-GUARD ANCHOR_STALE: %s" % exc)
        return 2
    summ = summarize(findings, scanned)
    red = summ["project_red"] + (summ["third_party_red_isolated"] if args.fail_on_third_party else 0)
    if not summ["self_test_fixture_present"]:
        print("TAUT-NULL-GUARD: 自带负例夹具面为空（判据自身的负例证据丢失 ⇒ 判红）")
        red += 1
    if args.fail_on_fixture:
        red += summ["self_test_fixture_red"]
    doc = {"check_id": "TAUT-NULL-GUARD", "verdict": "FAIL" if red else "PASS",
           "summary": summ, "fail_on_third_party": bool(args.fail_on_third_party),
           "findings": sorted(findings, key=lambda f: (f["file"], f["line"]))}
    print_report(doc, show_all=args.list)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + chr(10), encoding="utf-8")
        print("JSON_OUT %s" % out)
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main())
