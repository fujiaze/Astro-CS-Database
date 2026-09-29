#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_ctest_reg_condition.py — CTest 注册面/引用面**条件上下文**一致性门。

背景（真实事故，提交 8097000c）：eng/tests/unit/p1wcs/CMakeLists.txt 把 negative 测试的
add_test(NAME p1wcs_negative …) 限定在 if(UNIX) 内（该组依赖 POSIX fork 隔离与信号终止
判据，非 UNIX 平台无等价语义），而 set_tests_properties(…) 仍**无条件**列出该名。Windows
上 configure 期直接 FATAL（找不到该测试），整机构建无法配置。已按「属性面按实际注册集
派生」修好，但同类形态可能还有别处；而此前的登记面门
（eng/tools/quality/check_ctest_registration.py，CI-REG-002）只判「名字是否被 CI 登记」，
**不看条件上下文**，对本类缺陷天然失明。

判据（两条事实交叉判定；不设白名单、不设豁免面）：

  C1 事实一（注册面）：静态解析活动 CTest 面全部 CMake 源的 add_test(NAME <名> …)，
     得到每个注册名**及其条件上下文**（平台条件 if(UNIX)/if(WIN32)/…、选项条件
     if(ASTROCS_…) 的布尔组合；含 if/elseif/else 分支语义与 CONFIGURATIONS 限定）。
  C2 事实二（引用面）：静态解析按名引用测试的调用 —— set_tests_properties(…) 与
     set_property(TEST|TESTS …) —— 得到每个引用名**及其条件上下文**；引用名可以来自
     变量（set / list(APPEND) 派生），此时变量各元素**各自携带其绑定时点的条件上下文**
     （这正是 8097000c 修复后的形态：p1wcs_negative 只在 if(UNIX) 时被 append 进
     P1WCS_LABELLED_TESTS，于是引用上下文 = UNIX，与注册上下文逐点一致）。
  C3 判红（主判据 RC1）：某名在某个「平台/选项组合」下**被引用但未注册**。形式化：
     存在对上下文原子的赋值 A，使 引用式(A)=真 ∧ 全部注册式(A)=假 —— 即引用处条件
     **宽于**注册处条件（含「引用无条件、注册有条件」这一事故形态）。
  C4 不判红：引用处条件**窄于或等于**注册处条件（合法：引用只会落在已注册的组合上）。
  C5 悬空引用（RC2）：引用名在注册面**从未出现**（C3 的极端形态，同样配置期致命）。
  C6 fail-closed：注册名/引用名不可静态枚举（未解析变量）、条件不可解析、块结构不可信、
     扫描面为空，一律判红并逐条点名 —— 看不见的东西不得当成合格（docs/ci/01_CHECKS.md §1）。
  C7 非退化守卫：注册面或引用面为空即判红（空面不得静默判绿）；--fault-inject 在**真实
     仓库扫描面**上注入本类缺陷，逐例必须判红，注入锚消失（注入失效）同样判红 ——
     防止判据退化成恒绿门。
  C8 扫描面口径与 check_ctest_registration.py **同一处**（复用其 discover_sources 与排除
     规则），保证「登记面」与「条件面」两个门看的是同一批源；排除 run/、build/、out/、
     artifacts/、third_party/ 与 archive/superseded 归档旧树（历史快照不是活动面）。

自测（--self-test；内存 fixture，零副作用）：T1..T18，含真实事故最小复现（T3）、
foreach/变量派生形态（T9/T10/T17c）、跨目录形态（T17）、不可枚举/不可解析/空扫描面/
非退化守卫的 fail-closed 面与真实仓库回归（T18，现场漂移即红）。

用法:
  python3 eng/tools/quality/check_ctest_reg_condition.py                    # 校验
  python3 eng/tools/quality/check_ctest_reg_condition.py --json-out run/ci/ctest-reg-condition/report.json
  python3 eng/tools/quality/check_ctest_reg_condition.py --self-test        # 正/负例自检
  python3 eng/tools/quality/check_ctest_reg_condition.py --fault-inject all # 真实扫描面注入（必须全红）
  python3 eng/tools/quality/check_ctest_reg_condition.py --inventory        # 扫描面清单（覆盖统计）

只读（除 --json-out 显式请求）；仅 stdlib。
"""
from __future__ import annotations

import argparse
import datetime as _dt
import importlib.util
import itertools
import json
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[3]
SURFACE_TOOL_REL = "eng/tools/quality/check_ctest_registration.py"

# 条件原子上限：某名的注册/引用式里不同原子数超过此值即 fail-closed
# （判定是 2^n 枚举；这里防的是「原子爆炸把门拖慢」，不是判据本身的强度）。
MAX_ATOMS = 16
MAX_VAR_VARIANTS = 64
MAX_VAR_ELEMS = 4096

TRUE = ("true",)
FALSE = ("false",)


class Unavailable(RuntimeError):
    """依赖不可用（git / 扫描面工具）—— fail-closed，不给结论。"""


class CondError(RuntimeError):
    def __init__(self, line, text, why):
        super().__init__("line %d: if(%s) —— %s" % (line, text.strip(), why))
        self.line = line
        self.text = text
        self.why = why


# --------------------------------------------------------------------------- 公式 ----

def mk_and(*fs):
    acc = []
    for f in fs:
        if f == FALSE:
            return FALSE
        if f == TRUE:
            continue
        acc.append(f)
    if not acc:
        return TRUE
    if len(acc) == 1:
        return acc[0]
    return ("and", tuple(acc))


def mk_or(*fs):
    acc = []
    for f in fs:
        if f == TRUE:
            return TRUE
        if f == FALSE:
            continue
        acc.append(f)
    if not acc:
        return FALSE
    if len(acc) == 1:
        return acc[0]
    return ("or", tuple(acc))


def mk_not(f):
    if f == TRUE:
        return FALSE
    if f == FALSE:
        return TRUE
    if f[0] == "not":
        return f[1]
    return ("not", f)


def atoms_of(f, out=None):
    if out is None:
        out = set()
    k = f[0]
    if k in ("true", "false"):
        return out
    if k == "atom":
        out.add(f[1])
        return out
    if k == "not":
        return atoms_of(f[1], out)
    for sub in f[1]:
        atoms_of(sub, out)
    return out


def ev(f, a):
    k = f[0]
    if k == "true":
        return True
    if k == "false":
        return False
    if k == "atom":
        return bool(a[f[1]])
    if k == "not":
        return not ev(f[1], a)
    if k == "and":
        return all(ev(s, a) for s in f[1])
    return any(ev(s, a) for s in f[1])


def show(f):
    k = f[0]
    if k == "true":
        return "TRUE"
    if k == "false":
        return "FALSE"
    if k == "atom":
        return f[1]
    if k == "not":
        return "NOT(%s)" % show(f[1])
    op = " AND " if k == "and" else " OR "
    return "(" + op.join(show(s) for s in f[1]) + ")"


# ------------------------------------------------------------------ CMake 命令切分 ----

_IDENT_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(")


class RawCmd:
    __slots__ = ("name", "body", "line")

    def __init__(self, name, body, line):
        self.name, self.body, self.line = name, body, line


def strip_line_comments(text):
    """剥整行注释（行首 # 行）—— 与 check_ctest_registration.py 同口径。"""
    return "\n".join(l for l in text.splitlines() if not l.lstrip().startswith("#"))


def parse_commands(text, errors, rel):
    cmds = []
    i, n = 0, len(text)
    while True:
        m = _IDENT_RE.search(text, i)
        if not m:
            break
        name = m.group(1)
        j = m.end()
        depth, k, q = 1, j, False
        while k < n and depth > 0:
            ch = text[k]
            if q:
                if ch == "\\" and k + 1 < n:
                    k += 2
                    continue
                if ch == '"':
                    q = False
            elif ch == '"':
                q = True
            elif ch == "#":
                nl = text.find("\n", k)
                k = n if nl < 0 else nl
                continue
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
            k += 1
        if depth != 0:
            errors.append("C6 %s: 命令 %s( 的括号未闭合（扫描面不可信 ⇒ fail-closed）"
                          % (rel, name))
            break
        cmds.append(RawCmd(name, text[j:k - 1], text.count("\n", 0, m.start()) + 1))
        i = k
    return cmds


def split_args(body):
    args, cur, q, i, n = [], "", False, 0, len(body)
    while i < n:
        ch = body[i]
        if q:
            if ch == "\\" and i + 1 < n:
                cur += body[i:i + 2]
                i += 2
                continue
            if ch == '"':
                q = False
            cur += ch
        elif ch == '"':
            q = True
            cur += ch
        elif ch in " \t\r\n":
            if cur:
                args.append(cur)
                cur = ""
        else:
            cur += ch
        i += 1
    if cur:
        args.append(cur)
    return args


def unquote(tok):
    if len(tok) >= 2 and tok[0] == '"' and tok[-1] == '"':
        return tok[1:-1].replace('\\"', '"').replace("\\\\", "\\")
    return tok


# ------------------------------------------------------------- if() 条件解析 ----

_TRUE_CONST = {"1", "ON", "YES", "TRUE", "Y"}
_FALSE_CONST = {"0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND", ""}
_UNARY_KW = {"DEFINED", "COMMAND", "POLICY", "TARGET", "TEST", "EXISTS",
             "IS_DIRECTORY", "IS_SYMLINK", "IS_ABSOLUTE"}
# 测试属性名清单（含 DEPENDS 与常用项）：用于把 PROPERTIES 之后的值按属性切分。
_KNOWN_TEST_PROPERTIES = {
    "DEPENDS", "TIMEOUT", "LABELS", "ENVIRONMENT", "WORKING_DIRECTORY", "WILL_FAIL",
    "PASS_REGULAR_EXPRESSION", "FAIL_REGULAR_EXPRESSION", "SKIP_RETURN_CODE",
    "RUN_SERIAL", "PROCESSORS", "REQUIRED_FILES", "FIXTURES_SETUP", "FIXTURES_CLEANUP",
    "FIXTURES_REQUIRED", "RESOURCE_LOCK", "COST", "ATTACHED_FILES",
    "ATTACHED_FILES_ON_FAIL", "GENERATED_RESOURCE_SPEC_FILE", "MEASUREMENT",
    "REQUIRED_FILES", "ENVIRONMENT_MODIFICATION",
}
_CMP_OPS = {"STREQUAL", "STRLESS", "STRGREATER", "STRLESS_EQUAL", "STRGREATER_EQUAL",
            "EQUAL", "LESS", "GREATER", "LESS_EQUAL", "GREATER_EQUAL", "MATCHES",
            "VERSION_EQUAL", "VERSION_LESS", "VERSION_GREATER", "VERSION_LESS_EQUAL",
            "VERSION_GREATER_EQUAL", "IN_LIST", "IS_NEWER_THAN", "PATH_EQUAL"}


def _split_parens(tok):
    if tok in ("(", ")") or tok.startswith('"'):
        return [tok]
    return [p for p in re.split(r"([()])", tok) if p != ""]


class CondParser:
    def __init__(self, toks, line, text, env):
        self.toks, self.i, self.line, self.text, self.env = toks, 0, line, text, env

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else None

    def next(self):
        t = self.peek()
        self.i += 1
        return t

    def parse(self):
        f = self.parse_or()
        if self.i != len(self.toks):
            raise CondError(self.line, self.text,
                            "剩余无法解析的记号 %r" % (self.toks[self.i:],))
        return f

    def parse_or(self):
        left = self.parse_and()
        while (self.peek() or "").upper() == "OR":
            self.next()
            left = mk_or(left, self.parse_and())
        return left

    def parse_and(self):
        left = self.parse_unary()
        while (self.peek() or "").upper() == "AND":
            self.next()
            left = mk_and(left, self.parse_unary())
        return left

    def parse_unary(self):
        t = self.peek()
        if t is None:
            raise CondError(self.line, self.text, "表达式意外结束")
        if t.upper() == "NOT":
            self.next()
            return mk_not(self.parse_unary())
        if t == "(":
            self.next()
            f = self.parse_or()
            if self.peek() != ")":
                raise CondError(self.line, self.text, "缺右括号")
            self.next()
            return f
        if t == ")":
            raise CondError(self.line, self.text, "多余的右括号")
        return self.parse_primary()

    def parse_primary(self):
        t = self.next()
        up = unquote(t).upper()
        if up in _UNARY_KW:
            arg = self.next()
            if arg is None:
                raise CondError(self.line, self.text, "%s 缺参数" % up)
            return ("atom", "%s(%s)" % (up, unquote(arg)))
        left = self.value_f(t)
        nxt = self.peek()
        if nxt is not None and unquote(nxt).upper() in _CMP_OPS:
            op = unquote(self.next()).upper()
            rhs = self.next()
            if rhs is None:
                raise CondError(self.line, self.text, "%s 缺右操作数" % op)
            return ("atom", "cmp:%s:%s:%s" % (op, show(left), unquote(rhs)))
        if nxt is not None and unquote(nxt).upper() not in ("AND", "OR", "NOT", ")"):
            raise CondError(self.line, self.text,
                            "记号 %r 无法解析（既非 AND/OR/比较运算，也非表达式结束）"
                            % (unquote(nxt),))
        return left

    def value_f(self, tok):
        """常量直接定值；变量名先按 env 求值，求不出时退化为「按变量名的原子」。

        同名的两处条件互相抵消；不同名一律不相容（偏向判红，不放过「两侧各自漂移」）。
        """
        s = unquote(tok)
        up = s.upper()
        if up in _TRUE_CONST:
            return TRUE
        if up in _FALSE_CONST or up.endswith("-NOTFOUND"):
            return FALSE
        m = re.fullmatch(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}", s)
        if m is not None:
            entries = self.env.lookup(m.group(1)) if self.env else None
            if entries is not None:
                vals = {v.strip().upper() for v, _c in entries}
                if len(vals) == 1:
                    v = vals.pop()
                    if v in _TRUE_CONST:
                        return TRUE
                    if v in _FALSE_CONST or v.endswith("-NOTFOUND"):
                        return FALSE
            return ("atom", "var:%s" % m.group(1))
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", s):
            return ("atom", "var:%s" % s)
        return ("atom", "lit:%s" % s)


def parse_condition(expr, line, env):
    toks = []
    for tok in split_args(expr):
        toks.extend(_split_parens(tok))
    if not toks:
        raise CondError(line, expr, "空条件")
    return CondParser(toks, line, expr, env).parse()


# ----------------------------------------------------------------------- 作用域 ----

class Env:
    """CMake 变量作用域（目录继承 + 循环变量遮蔽）；值带**绑定时点的条件上下文**。"""

    def __init__(self, parent=None):
        self.parent = parent
        self.vars = {}

    def lookup(self, name):
        e = self
        while e is not None:
            if name in e.vars:
                return e.vars[name]
            e = e.parent
        return None

    def assign(self, name, entries):
        self.vars[name] = list(entries)

    def child(self):
        return Env(self)


class Occ:
    """一条事实：某名字在某 文件:行 的某条件下被 注册 / 引用。"""

    __slots__ = ("name", "cond", "path", "line", "kind", "expr")

    def __init__(self, name, cond, path, line, kind, expr=""):
        self.name, self.cond, self.path, self.line = name, cond, path, line
        self.kind, self.expr = kind, expr

    def where(self, root):
        p = self.path
        try:
            p = str(pathlib.Path(self.path).relative_to(root)).replace("\\", "/")
        except ValueError:
            pass
        return "%s:%d" % (p, self.line)


# ------------------------------------------------------------------- 语句处理 ----

class Analyzer:
    def __init__(self):
        self.regs = []
        self.refs = []
        self.deps = []          # 依赖面（DEPENDS 属性值）：实测非配置期致命，单独成面
        self.errors = []
        self.atom_inventory = {}
        self.cond_inventory = {}
        self.stats = {"sources": 0, "add_test_calls": 0, "reference_calls": 0,
                      "unresolved_name_tokens": 0, "unresolved_value_tokens": 0,
                      "foreach_loops": 0, "opaque_list_ops": 0}

    _VAR_RE = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}|\$([A-Za-z_][A-Za-z0-9_]*)")

    def expand_token(self, tok, env, ctx, path, line, strict=True):
        """记号 → [(值, 条件)]；strict=名字位置（不可枚举即 fail-closed）。

        值位置（set/list/foreach 头）传 strict=False：不可解析的变量按**字面量**保留，
        不在此处报错 —— 该值若最终落到名字位置，仍会在那里 fail-closed；
        若从不落到名字位置，则与本判据无关（不得因无关变量判红）。
        """
        t = unquote(tok)
        parts = t.split(";") if ";" in t else [t]
        out = []
        for part in parts:
            variants = [(part, ctx)]
            for _ in range(8):
                nxt, changed, bad = [], False, None
                for s, c in variants:
                    m = self._VAR_RE.search(s)
                    if not m:
                        nxt.append((s, c))
                        continue
                    changed = True
                    var = m.group(1) or m.group(2)
                    entries = env.lookup(var)
                    if entries is None:
                        bad = "${%s}" % var
                        break
                    for ev_, ec in entries:
                        nxt.append((s[:m.start()] + ev_ + s[m.end():], mk_and(c, ec)))
                if bad is not None:
                    self.stats["unresolved_name_tokens" if strict
                               else "unresolved_value_tokens"] += 1
                    if not strict:
                        bad = None            # 值位置：按字面量保留，延后到名字位置判
                        nxt = list(variants)
                        variants = nxt
                        break
                    self.errors.append(
                        "C6 %s:%d: 记号 %r 含未解析变量 %s —— 名字集合无法静态枚举，"
                        "fail-closed 判红（变量须在扫描面内可静态求值）"
                        % (path, line, tok, bad))
                    return []
                variants = nxt
                if not changed:
                    break
                if len(variants) > MAX_VAR_VARIANTS:
                    self.errors.append(
                        "C6 %s:%d: 记号 %r 的变量展开超过 %d 个候选（扫描不可控）"
                        % (path, line, tok, MAX_VAR_VARIANTS))
                    return []
            for v, c in variants:
                if v != "":
                    out.append((v, c))
        return out

    # -- 命令 -------------------------------------------------------------------
    def exec_cmd(self, cmd, env, ctx, path):
        name = cmd.name.lower()
        if name == "add_test":
            self.handle_add_test(cmd, env, ctx, path)
        elif name == "set_tests_properties":
            self.handle_set_tests_properties(cmd, env, ctx, path)
        elif name == "set_property":
            self.handle_set_property(cmd, env, ctx, path)
        elif name == "set":
            self.handle_set(cmd, env, ctx, path)
        elif name == "list":
            self.handle_list(cmd, env, ctx, path)
        elif name == "unset":
            for v in split_args(cmd.body):
                v = unquote(v)
                if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", v):
                    env.vars.pop(v, None)

    def handle_add_test(self, cmd, env, ctx, path):
        args = split_args(cmd.body)
        if not args:
            return
        self.stats["add_test_calls"] += 1
        if args[0].upper() == "NAME" and len(args) >= 2:
            name_tok, tail = args[1], args[2:]
        else:
            name_tok, tail = args[0], args[1:]
        cond = ctx
        heads = [unquote(a).upper() for a in tail]
        if "CONFIGURATIONS" in heads:
            cfgs = []
            for b in tail[heads.index("CONFIGURATIONS") + 1:]:
                if unquote(b).upper() in ("COMMAND", "WORKING_DIRECTORY", "NAME",
                                          "WILL_FAIL"):
                    break
                cfgs.append(("atom", "config:%s" % unquote(b)))
            if cfgs:
                cond = mk_and(ctx, mk_or(*cfgs))
        for nm, c in self.expand_token(name_tok, env, ctx, path, cmd.line):
            self.regs.append(Occ(nm, mk_and(cond, c), path, cmd.line, "add_test",
                                 "add_test(NAME %s …)" % name_tok))

    @staticmethod
    def _property_pairs(args):
        """PROPERTIES 之后的 (属性名, 值记号) 序列；值可跨多个记号，直到下一个属性名。"""
        ups = [unquote(a).upper() for a in args]
        if "PROPERTIES" not in ups:
            return []
        out, cur_key, cur_vals = [], None, []
        for a in args[ups.index("PROPERTIES") + 1:]:
            if unquote(a).upper() in _KNOWN_TEST_PROPERTIES:
                if cur_key is not None:
                    out.append((cur_key, cur_vals))
                cur_key, cur_vals = unquote(a).upper(), []
            elif cur_key is not None:
                cur_vals.append(a)
        if cur_key is not None:
            out.append((cur_key, cur_vals))
        return out

    @staticmethod
    def _name_list(args, stop_words):
        names = []
        for a in args:
            if unquote(a).upper() in stop_words:
                break
            names.append(a)
        return names

    def handle_set_tests_properties(self, cmd, env, ctx, path):
        args = split_args(cmd.body)
        if not args:
            return
        if not any(unquote(a).upper() == "PROPERTIES" for a in args):
            self.errors.append(
                "C6 %s:%d: set_tests_properties 无 PROPERTIES 关键字（调用形态不可信）"
                % (path, cmd.line))
            return
        self.stats["reference_calls"] += 1
        for tok in self._name_list(args, {"PROPERTIES"}):
            for nm, c in self.expand_token(tok, env, ctx, path, cmd.line):
                self.refs.append(Occ(nm, c, path, cmd.line, "set_tests_properties",
                                     "set_tests_properties(%s …)" % tok))
        pairs = self._property_pairs(args)
        for key, toks in pairs:
            if key != "DEPENDS":
                continue
            for tok in toks:
                for nm, c in self.expand_token(tok, env, ctx, path, cmd.line):
                    self.deps.append(Occ(nm, c, path, cmd.line, "DEPENDS",
                                         "set_tests_properties(… DEPENDS %s)" % tok))

    def handle_set_property(self, cmd, env, ctx, path):
        args = split_args(cmd.body)
        if not args:
            return
        scope = unquote(args[0]).upper()
        if scope not in ("TEST", "TESTS"):
            return
        self.stats["reference_calls"] += 1
        for tok in self._name_list(args[1:], {"PROPERTY", "APPEND", "APPEND_STRING",
                                             "REQUIRED"}):
            for nm, c in self.expand_token(tok, env, ctx, path, cmd.line):
                self.refs.append(Occ(nm, c, path, cmd.line, "set_property(%s)" % scope,
                                     "set_property(%s %s …)" % (scope, tok)))
        ups = [unquote(a).upper() for a in args]
        if "PROPERTY" in ups:
            tail = args[ups.index("PROPERTY") + 1:]
            if tail and unquote(tail[0]).upper() == "DEPENDS":
                for tok in tail[1:]:
                    if unquote(tok).upper() in _KNOWN_TEST_PROPERTIES:
                        break
                    for nm, c in self.expand_token(tok, env, ctx, path, cmd.line):
                        self.deps.append(Occ(nm, c, path, cmd.line, "DEPENDS",
                                             "set_property(%s … PROPERTY DEPENDS %s)"
                                             % (scope, tok)))

    def handle_set(self, cmd, env, ctx, path):
        args = split_args(cmd.body)
        if not args:
            return
        var = unquote(args[0])
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", var):
            return
        if len(args) < 2:
            env.assign(var, [])
            return
        toks = list(args[1:])
        ups = [unquote(t).upper() for t in toks]
        if "CACHE" in ups:
            toks = toks[:ups.index("CACHE")]
        toks = [t for t in toks if unquote(t).upper() not in ("PARENT_SCOPE", "FORCE")]
        entries = []
        for t in toks:
            for nm, c in self.expand_token(t, env, ctx, path, cmd.line, strict=False):
                entries.append((nm, c))
        if len(entries) > MAX_VAR_ELEMS:
            self.errors.append("C6 %s:%d: 变量 %s 元素过多（扫描不可控）"
                               % (path, cmd.line, var))
            return
        env.assign(var, entries)

    def handle_list(self, cmd, env, ctx, path):
        args = split_args(cmd.body)
        if len(args) < 2:
            return
        op, var = unquote(args[0]).upper(), unquote(args[1])
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", var):
            return
        if op in ("APPEND", "PREPEND"):
            entries = []
            for t in args[2:]:
                for nm, c in self.expand_token(t, env, ctx, path, cmd.line, strict=False):
                    entries.append((nm, c))
            cur = env.lookup(var) or []
            if len(cur) + len(entries) > MAX_VAR_ELEMS:
                self.errors.append("C6 %s:%d: 变量 %s 元素过多（扫描不可控）"
                                   % (path, cmd.line, var))
                return
            env.assign(var, (cur + entries) if op == "APPEND" else (entries + cur))
        elif op == "REMOVE_ITEM":
            drop = set()
            for t in args[2:]:
                for nm, _c in self.expand_token(t, env, ctx, path, cmd.line):
                    drop.add(nm)
            env.assign(var, [(v, c) for v, c in (env.lookup(var) or []) if v not in drop])
        elif op == "REMOVE_DUPLICATES":
            seen, keep = set(), []
            for v, c in (env.lookup(var) or []):
                if v in seen:
                    continue
                seen.add(v)
                keep.append((v, c))
            env.assign(var, keep)
        elif op in ("SORT", "REVERSE"):
            pass  # 只改顺序，不改值集合
        elif op in ("LENGTH", "GET", "JOIN", "FIND", "SUBLIST"):
            pass  # 只读算子，不改值集合
        else:
            # 未知/会改写值集合的算子：丢弃该变量的静态可求值性**而不在此处报错** ——
            # 若它最终落到名字位置，那里会按「未解析变量」fail-closed 点名；
            # 若从不落到名字位置，则与本判据无关（不因无关变量判红）。
            env.vars.pop(var, None)
            self.stats["opaque_list_ops"] += 1

    # -- 块 ---------------------------------------------------------------------
    def exec_nodes(self, nodes, env, ctx, path):
        for node in nodes:
            kind = node[0]
            if kind == "cmd":
                self.exec_cmd(node[1], env, ctx, path)
            elif kind == "if":
                self.exec_if(node, env, ctx, path)
            elif kind == "foreach":
                self.exec_foreach(node, env, ctx, path)
            elif kind in ("function", "macro"):
                self.check_for_hidden_facts(node[2], node[1], path)
            elif kind == "while":
                self.errors.append(
                    "C6 %s:%d: while() 块内的注册/引用面无法静态定点 ⇒ fail-closed"
                    % (path, node[1].line))

    def check_for_hidden_facts(self, nodes, cmd, path):
        for node in nodes:
            if node[0] == "cmd" and node[1].name.lower() in (
                    "add_test", "set_tests_properties", "set_property"):
                self.errors.append(
                    "C6 %s:%d: %s() 定义体内出现 %s() —— 静态扫描看不见调用点，"
                    "fail-closed 判红" % (path, cmd.line, cmd.name, node[1].name))
            elif node[0] == "if":
                for _bk, _e, _ln, body in node[1]:
                    self.check_for_hidden_facts(body, cmd, path)
            elif node[0] in ("foreach", "while", "function", "macro"):
                self.check_for_hidden_facts(node[2], cmd, path)

    def exec_if(self, node, env, ctx, path):
        raw = []
        for bkind, expr, line, body in node[1]:
            try:
                if bkind == "else":
                    c = mk_not(mk_or(*raw)) if raw else TRUE
                else:
                    e = parse_condition(expr, line, env)
                    self.atom_inventory[show(e)] = self.atom_inventory.get(show(e), 0) + 1
                    key = " ".join(expr.split())
                    self.cond_inventory[key] = self.cond_inventory.get(key, 0) + 1
                    c = e if bkind == "if" else mk_and(mk_not(mk_or(*raw[:-1])), e)
                    raw.append(e)
            except CondError as exc:
                self.errors.append("C6 %s: 条件不可解析 —— %s（fail-closed）"
                                   % (path, exc))
                continue
            self.exec_nodes(body, env, mk_and(ctx, c), path)

    def exec_foreach(self, node, env, ctx, path):
        cmd, body = node[1], node[2]
        args = split_args(cmd.body)
        if not args:
            return
        var = unquote(args[0])
        rest = args[1:]
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", var):
            self.errors.append("C6 %s:%d: foreach 变量名 %r 非法" % (path, cmd.line, var))
            return
        if len(rest) >= 2 and unquote(rest[0]).upper() == "IN":
            mode = unquote(rest[1]).upper()
            if mode not in ("LISTS", "ITEMS"):
                self.errors.append(
                    "C6 %s:%d: foreach(… IN %s …) 形态不覆盖（名字集合不可枚举）⇒ "
                    "fail-closed" % (path, cmd.line, mode))
                return
            toks = rest[2:]
        elif rest and unquote(rest[0]).upper() == "RANGE":
            self.errors.append(
                "C6 %s:%d: foreach(… RANGE …) 形态不覆盖（名字集合不可枚举）⇒ "
                "fail-closed" % (path, cmd.line))
            return
        else:
            toks = rest
        values = []
        for t in toks:
            for nm, _c in self.expand_token(t, env, ctx, path, cmd.line, strict=False):
                values.append(nm)
        self.stats["foreach_loops"] += 1
        for v in values:
            inner = env.child()
            inner.assign(var, [(v, ctx)])
            self.exec_nodes(body, inner, ctx, path)


# --------------------------------------------------------------- 块结构构建 ----

def build_blocks(cmds, i, stop, path, errors):
    out = []
    while i < len(cmds):
        c = cmds[i]
        low = c.name.lower()
        if low in stop:
            return out, i, c
        if low == "if":
            body, i, endc = build_blocks(cmds, i + 1, ("elseif", "else", "endif"),
                                         path, errors)
            branches = [("if", c.body, c.line, body)]
            while endc is not None and endc.name.lower() in ("elseif", "else"):
                head = endc
                if head.name.lower() == "elseif":
                    b2, i, endc = build_blocks(cmds, i + 1,
                                               ("elseif", "else", "endif"),
                                               path, errors)
                    branches.append(("elseif", head.body, head.line, b2))
                else:
                    b2, i, endc = build_blocks(cmds, i + 1, ("endif",), path, errors)
                    branches.append(("else", "", head.line, b2))
            if endc is None:
                errors.append("C6 %s:%d: if() 无对应 endif()（结构不可信）"
                              % (path, c.line))
            out.append(("if", branches, c.line))
            i += 1
        elif low in ("foreach", "while", "function", "macro"):
            endname = {"foreach": "endforeach", "while": "endwhile",
                       "function": "endfunction", "macro": "endmacro"}[low]
            body, i, endc = build_blocks(cmds, i + 1, (endname,), path, errors)
            if endc is None:
                errors.append("C6 %s:%d: %s() 无对应 %s()（结构不可信）"
                              % (path, c.line, low, endname))
            out.append((low, c, body))
            i += 1
        elif low in ("endif", "endforeach", "endwhile", "endfunction", "endmacro",
                     "else", "elseif"):
            errors.append("C6 %s:%d: 多余的 %s()（结构不可信）" % (path, c.line, c.name))
            i += 1
        else:
            out.append(("cmd", c))
            i += 1
    return out, i, None


# ------------------------------------------------------------------------ 判定 ----

def analyze(sources, root=REPO):
    """sources: {仓库相对路径: 文本} —— CMakeLists.txt 与 *.cmake。"""
    an = Analyzer()
    paths = sorted(sources, key=lambda p: (p.count("/"), p))
    dir_env = {"": Env(None)}
    for rel in paths:
        pdir = str(pathlib.PurePosixPath(rel).parent)
        if pdir == ".":
            pdir = ""
        base, cur = None, pdir
        while True:
            if cur in dir_env:
                base = dir_env[cur]
                break
            if cur == "":
                base = dir_env[""]
                break
            cur = str(pathlib.PurePosixPath(cur).parent)
            if cur == ".":
                cur = ""
        env = base.child() if rel.endswith("CMakeLists.txt") else base
        text = strip_line_comments(sources[rel])
        cmds = parse_commands(text, an.errors, rel)
        nodes, _i, leftover = build_blocks(cmds, 0, (), rel, an.errors)
        if leftover is not None:
            an.errors.append("C6 %s:%d: 未闭合的块结构" % (rel, leftover.line))
        an.stats["sources"] += 1
        an.exec_nodes(nodes, env, TRUE, rel)
        if rel.endswith("CMakeLists.txt"):
            dir_env[pdir] = env
    return an


def judge(an, root=REPO):
    reg, ref = {}, {}
    for o in an.regs:
        reg.setdefault(o.name, []).append(o)
    for o in an.refs:
        ref.setdefault(o.name, []).append(o)

    errors = list(an.errors)
    if not an.regs and not an.refs:
        errors.append("C6 扫描面为空：未解析出任何 add_test 或按名引用测试的调用 —— "
                      "fail-closed 拒绝空扫描判绿")
    elif not an.regs:
        errors.append("C7 非退化守卫：注册面为空（解析出的 add_test(NAME …) 为 0）—— "
                      "判据会退化成恒绿门，判红")
    elif not an.refs:
        errors.append("C7 非退化守卫：引用面为空（set_tests_properties / "
                      "set_property(TEST|TESTS …) 为 0）—— 判据会退化成恒绿门，判红")

    deps = {}
    for o in an.deps:
        deps.setdefault(o.name, []).append(o)

    def cross(rule, refmap):
        out = []
        for name in sorted(refmap):
            refF = mk_or(*[o.cond for o in refmap[name]])
            if name not in reg:
                out.append({
                    "rule": rule, "name": name,
                    "ref": [{"at": o.where(root), "cond": show(o.cond), "call": o.expr}
                            for o in refmap[name]],
                    "reg": [],
                    "witness": "任何组合（该名从未注册）",
                    "why": ("引用名在注册面从未出现（悬空引用）"
                            + ("—— 配置期必然 FATAL" if rule != "RC3"
                               else "—— 依赖面悬空（实测非配置期致命）")),
                })
                continue
            regF = mk_or(*[o.cond for o in reg[name]])
            ats = sorted(atoms_of(refF) | atoms_of(regF))
            if len(ats) > MAX_ATOMS:
                errors.append("C6 名字 %s 的条件原子数 %d > %d（枚举不可控）⇒ fail-closed"
                              % (name, len(ats), MAX_ATOMS))
                continue
            witness = None
            for bits in itertools.product((False, True), repeat=len(ats)):
                a = dict(zip(ats, bits))
                if ev(refF, a) and not ev(regF, a):
                    witness = a
                    break
            if witness is not None:
                out.append({
                    "rule": rule, "name": name,
                    "ref": [{"at": o.where(root), "cond": show(o.cond), "call": o.expr}
                            for o in refmap[name]],
                    "reg": [{"at": o.where(root), "cond": show(o.cond), "call": o.expr}
                            for o in reg[name]],
                    "witness": "{" + ", ".join("%s=%s" % (k, "真" if v else "假")
                                               for k, v in witness.items()) + "}",
                    "why": ("引用条件宽于注册条件：见证组合下该名被引用但未注册"
                            + ("（configure 期找不到该测试 ⇒ FATAL）" if rule != "RC3"
                               else "（依赖面，运行期语义，实测非配置期致命）")),
                })
        return out

    violations = cross("RC1", ref)
    for v in violations:
        if not v["reg"]:
            v["rule"] = "RC2"
    runtime = cross("RC3", deps)

    summary = {
        "sources": an.stats["sources"],
        "add_test_calls": an.stats["add_test_calls"],
        "reference_calls": an.stats["reference_calls"],
        "foreach_loops": an.stats["foreach_loops"],
        "registered_names": len(reg),
        "referenced_names": len(ref),
        "registration_occurrences": len(an.regs),
        "reference_occurrences": len(an.refs),
        "dependency_occurrences": len(an.deps),
        "dependency_names": len(deps),
        "unresolved_name_tokens": an.stats["unresolved_name_tokens"],
        "unresolved_value_tokens": an.stats["unresolved_value_tokens"],
        "opaque_list_ops": an.stats["opaque_list_ops"],
        "atom_inventory": dict(sorted(an.atom_inventory.items())),
        "condition_inventory": dict(sorted(an.cond_inventory.items(),
                                           key=lambda kv: (-kv[1], kv[0]))),
    }
    return reg, ref, violations, errors, summary, runtime


# --------------------------------------------------------------------- 真实扫描 ----

def load_surface_tool():
    p = REPO / SURFACE_TOOL_REL
    if not p.is_file():
        raise Unavailable("扫描面工具 %s 不存在（扫描面不可枚举 ⇒ fail-closed）"
                          % SURFACE_TOOL_REL)
    spec = importlib.util.spec_from_file_location("ctest_reg_surface", p)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def collect_real_sources(repo):
    mod = load_surface_tool()
    try:
        paths, untracked = mod.discover_sources(repo, tracked_only=True)
    except mod.GitUnavailable as exc:
        raise Unavailable("CTest 面 = 版本库面，git 面不可用：%s" % exc) from None
    srcs = {}
    for p in paths:
        rel = str(p.relative_to(repo)).replace("\\", "/")
        srcs[rel] = p.read_text(encoding="utf-8", errors="replace")
    return srcs, untracked


# -------------------------------------------------------------------- 自检面 ----

REG_A = "add_test(NAME alpha COMMAND t alpha)\n"
REF_A = "set_tests_properties(alpha PROPERTIES LABELS \"x\")\n"


def run_self_test():
    cases = []

    def case(name, sources, expect_pass, expect_rule=None, expect_name=None,
             expect_runtime_min=0):
        root = pathlib.Path("/fixture")
        an = analyze(sources, root=root)
        _r, _f, vio, errors, _s, rt = judge(an, root=root)
        red = bool(vio) or bool(errors)       # RC3（依赖面）不并入主判红，见 docstring C3
        ok = (not red) if expect_pass else red
        if ok and not expect_pass and expect_rule:
            hit = [v for v in vio if v["rule"] == expect_rule
                   and (expect_name is None or v["name"] == expect_name)]
            ok = bool(hit) or any(expect_rule in e for e in errors)
        if ok and len(rt) < expect_runtime_min:
            ok = False                        # 依赖面命中不得被静默丢弃
        cases.append({"case": name, "expect": "PASS" if expect_pass else "FAIL",
                      "actual": "PASS" if not red else "FAIL", "ok": ok,
                      "violations": [v["rule"] + " " + v["name"] for v in vio],
                      "runtime": [v["rule"] + " " + v["name"] for v in rt],
                      "errors": errors})

    case("T1_uncond_both", {"CMakeLists.txt": REG_A + REF_A}, True)
    case("T2_same_guard",
         {"CMakeLists.txt": "if(UNIX)\n" + REG_A + REF_A + "endif()\n"}, True)
    case("T3_incident_minimal",
         {"CMakeLists.txt": "if(UNIX)\n" + REG_A + "endif()\n" + REF_A},
         False, "RC1", "alpha")
    case("T4_ref_narrower",
         {"CMakeLists.txt": REG_A + "if(UNIX)\n" + REF_A + "endif()\n"}, True)
    case("T5_ref_wider_conj",
         {"CMakeLists.txt": "if(UNIX AND OPT)\n" + REG_A + "endif()\nif(UNIX)\n" + REF_A
          + "endif()\n"}, False, "RC1", "alpha")
    case("T6_reg_wider_conj",
         {"CMakeLists.txt": "if(UNIX)\n" + REG_A + "endif()\nif(UNIX AND OPT)\n" + REF_A
          + "endif()\n"}, True)
    case("T7_platform_mismatch",
         {"CMakeLists.txt": "if(WIN32)\n" + REG_A + "endif()\nif(NOT UNIX)\n" + REF_A
          + "endif()\n"}, False, "RC1", "alpha")
    case("T8_else_branch_covers",
         {"CMakeLists.txt": "if(UNIX)\n" + REG_A + "else()\nadd_test(NAME alpha COMMAND "
          "t alpha)\nendif()\n" + REF_A}, True)
    case("T8b_elseif_gap",
         {"CMakeLists.txt": "if(UNIX)\n" + REG_A + "elseif(APPLE)\nadd_test(NAME alpha "
          "COMMAND t alpha)\nendif()\n" + REF_A}, False, "RC1", "alpha")
    case("T9_variable_derived_guard",
         {"CMakeLists.txt":
          "set(L alpha)\n"
          "add_test(NAME alpha COMMAND t alpha)\n"
          "if(UNIX)\n  list(APPEND L beta)\nendif()\n"
          "if(UNIX)\n  add_test(NAME beta COMMAND t beta)\nendif()\n"
          "set_tests_properties(${L} PROPERTIES LABELS \"x\")\n"}, True)
    case("T10_variable_unconditional_append",
         {"CMakeLists.txt":
          "set(L alpha)\n"
          "add_test(NAME alpha COMMAND t alpha)\n"
          "list(APPEND L beta)\n"
          "if(UNIX)\n  add_test(NAME beta COMMAND t beta)\nendif()\n"
          "set_tests_properties(${L} PROPERTIES LABELS \"x\")\n"}, False, "RC1", "beta")
    case("T11_unenumerable_registration",
         {"CMakeLists.txt": "add_test(NAME ${UNKNOWN} COMMAND t x)\n" + REF_A},
         False, "C6")
    case("T12_unenumerable_reference",
         {"CMakeLists.txt": REG_A + "set_tests_properties(${UNKNOWN} PROPERTIES LABELS "
          "\"x\")\n"}, False, "C6")
    case("T13_empty_surface", {"CMakeLists.txt": "message(STATUS \"hi\")\n"}, False, "C6")
    case("T14_unparseable_condition",
         {"CMakeLists.txt": "if(FOO BAR BAZ)\n" + REG_A + "endif()\n" + REF_A},
         False, "C6")
    case("T15a_set_property_test_ok",
         {"CMakeLists.txt": REG_A + "set_property(TEST alpha PROPERTY LABELS x)\n"}, True)
    case("T15b_set_property_test_red",
         {"CMakeLists.txt": "if(UNIX)\n" + REG_A + "endif()\nset_property(TEST alpha "
          "PROPERTY LABELS x)\n"}, False, "RC1", "alpha")
    case("T15c_set_property_tests_plural_red",
         {"CMakeLists.txt": "if(UNIX)\n" + REG_A + "endif()\nset_property(TESTS alpha "
          "PROPERTY LABELS x)\n"}, False, "RC1", "alpha")
    case("T16_commented_add_test_ignored",
         {"CMakeLists.txt": "# add_test(NAME ghost COMMAND t g)\n" + REF_A + REG_A}, True)
    case("T17_cross_file",
         {"CMakeLists.txt": "add_subdirectory(sub)\n",
          "sub/CMakeLists.txt": "if(UNIX)\n" + REG_A + "endif()\n",
          "sub/refs.cmake": REF_A}, False, "RC1", "alpha")
    case("T17b_dangling_reference_never_registered",
         {"CMakeLists.txt": REG_A + REF_A + "set_tests_properties(ghost PROPERTIES "
          "LABELS x)\n"}, False, "RC2", "ghost")
    case("T17c_foreach_derived_names",
         {"CMakeLists.txt":
          "set(G a b c)\n"
          "foreach(g ${G})\n"
          "  add_test(NAME t_${g} COMMAND t ${g})\n"
          "  set_tests_properties(t_${g} PROPERTIES LABELS x)\n"
          "endforeach()\n"}, True)
    case("T17d_foreach_guard_narrower_inside",
         {"CMakeLists.txt":
          "set(G a b)\n"
          "foreach(g ${G})\n"
          "  if(UNIX)\n    add_test(NAME t_${g} COMMAND t ${g})\n  endif()\n"
          "  set_tests_properties(t_${g} PROPERTIES LABELS x)\n"
          "endforeach()\n"}, False, "RC1", "t_a")

    case("T19_depends_unconditional_is_runtime_not_gating",
         {"CMakeLists.txt": "if(UNIX)\n" + REG_A + "endif()\n"
          + "add_test(NAME other COMMAND t other)\n"
          + "set_tests_properties(other PROPERTIES DEPENDS alpha)\n"},
         True, None, None, 1)
    case("T19b_depends_on_never_registered",
         {"CMakeLists.txt": REG_A
          + "set_tests_properties(alpha PROPERTIES DEPENDS ghost)\n"},
         True, None, None, 1)
    case("T19c_depends_cond_matched_is_clean",
         {"CMakeLists.txt": "if(UNIX)\n" + REG_A + "endif()\n"
          + "add_test(NAME other COMMAND t other)\n"
          + "if(UNIX)\nset_tests_properties(other PROPERTIES DEPENDS alpha)\nendif()\n"},
         True, None, None, 0)

    try:
        srcs, _u = collect_real_sources(REPO)
        an = analyze(srcs, root=REPO)
        _r, _f, vio, errors, summ, rt = judge(an, root=REPO)
        red = bool(vio) or bool(errors)
        cases.append({"case": "T18_real_repo", "expect": "PASS",
                      "actual": "PASS" if not red else "FAIL", "ok": not red,
                      "violations": [v["rule"] + " " + v["name"] for v in vio[:20]],
                      "errors": errors[:20],
                      "scan": {k: summ[k] for k in ("sources", "add_test_calls",
                                                    "reference_calls",
                                                    "registered_names",
                                                    "referenced_names")}})
    except Unavailable as exc:
        cases.append({"case": "T18_real_repo", "expect": "PASS", "actual": "UNAVAILABLE",
                      "ok": False, "violations": [], "errors": [str(exc)]})

    failed = [c for c in cases if not c["ok"]]
    print(json.dumps({"tool": "check_ctest_reg_condition.py", "mode": "self-test",
                      "case_count": len(cases), "cases": cases,
                      "failed": len(failed),
                      "verdict": "PASS" if not failed else "FAIL"},
                     ensure_ascii=False, indent=2))
    return 0 if not failed else 1


# --------------------------------------------------------------- 注入面（真实树） ----

P1WCS_REL = "eng/tests/unit/p1wcs/CMakeLists.txt"
PSFW_REL = "eng/tests/unit/p1_psfw/CMakeLists.txt"
UNIT_REL = "eng/tests/unit/CMakeLists.txt"
LABEL_BLOCK = ("if(UNIX)\n  list(APPEND P1WCS_LABELLED_TESTS p1wcs_negative)\nendif()")


def _inj_replace(srcs, rel, old, new, label, rule, name):
    if rel not in srcs:
        return {"injection": label, "ok": False, "why": "目标文件 %s 不在扫描面" % rel}
    if old not in srcs[rel]:
        return {"injection": label, "ok": False,
                "why": ("注入锚已失效（%s 中找不到注入模式）—— 注入本身必须能生效，"
                        "失效即判红" % rel)}
    out = dict(srcs)
    out[rel] = srcs[rel].replace(old, new, 1)
    return {"injection": label, "ok": None, "rule": rule, "name": name, "sources": out}


def _inj_git(srcs, rel, rev, label, rule, name):
    try:
        r = subprocess.run(["git", "-C", str(REPO), "show", "%s:%s" % (rev, rel)],
                           capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as exc:
        return {"injection": label, "ok": False, "why": "git 不可用：%s" % exc}
    if r.returncode != 0 or not r.stdout.strip():
        return {"injection": label, "ok": False,
                "why": "git show %s:%s rc=%d（历史不可用 ⇒ 注入无效，判红）"
                       % (rev, rel, r.returncode)}
    if rel not in srcs:
        return {"injection": label, "ok": False, "why": "目标文件 %s 不在扫描面" % rel}
    out = dict(srcs)
    out[rel] = r.stdout
    return {"injection": label, "ok": None, "rule": rule, "name": name, "sources": out}


def run_fault_inject(which):
    try:
        srcs, _u = collect_real_sources(REPO)
    except Unavailable as exc:
        print("CTEST-REG-COND-FAIL: %s" % exc)
        return 2
    cases = {
        "git-history-prefix": lambda: _inj_git(
            srcs, P1WCS_REL, "8097000c^",
            "INJ-1 事故前原始文件（git 历史逐字换入）", "RC1", "p1wcs_negative"),
        "drop-append-guard": lambda: _inj_replace(
            srcs, P1WCS_REL, LABEL_BLOCK,
            "list(APPEND P1WCS_LABELLED_TESTS p1wcs_negative)",
            "INJ-2 属性面派生列表改回无条件 append", "RC1", "p1wcs_negative"),
        "widen-append-guard": lambda: _inj_replace(
            srcs, P1WCS_REL, LABEL_BLOCK,
            "if(UNIX OR WIN32)\n  list(APPEND P1WCS_LABELLED_TESTS p1wcs_negative)\nendif()",
            "INJ-3 引用面条件放宽为 UNIX OR WIN32（注册面仍 UNIX）", "RC1", "p1wcs_negative"),
        "cross-directory-uncond-ref": lambda: _inj_replace(
            srcs, UNIT_REL, "\n",
            "\nset_tests_properties(p1wcs_negative PROPERTIES TIMEOUT 300)\n",
            "INJ-4 跨目录无条件引用条件注册名", "RC1", "p1wcs_negative"),
        "foreach-name-registration-guard": lambda: _inj_replace(
            srcs, PSFW_REL,
            "  add_test(NAME p1_psfw_${g} COMMAND p1_psfw_tests ${g})\n",
            "  if(UNIX)\n    add_test(NAME p1_psfw_${g} COMMAND p1_psfw_tests "
            "${g})\n  endif()\n",
            "INJ-5 foreach 派生名下注册面被平台条件收窄", "RC1", "p1_psfw_anea"),
        "unenumerable-name": lambda: _inj_replace(
            srcs, P1WCS_REL,
            "set_tests_properties(${P1WCS_LABELLED_TESTS} PROPERTIES LABELS \"p1wcs\")",
            "set_tests_properties(${P1WCS_LABELLED_TESTS} ${P1WCS_UNKNOWN_LIST} "
            "PROPERTIES LABELS \"p1wcs\")",
            "INJ-6 引用名集合不可静态枚举（fail-closed 面）", "C6", ""),
        "unparseable-condition": lambda: _inj_replace(
            srcs, P1WCS_REL, LABEL_BLOCK,
            "if(UNIX WHATEVER)\n  list(APPEND P1WCS_LABELLED_TESTS p1wcs_negative)\nendif()",
            "INJ-7 条件不可解析（fail-closed 面）", "C6", ""),
    }
    if which == "all":
        inj = [cases[k]() for k in sorted(cases)]
    elif which in cases:
        inj = [cases[which]()]
    else:
        print("CTEST-REG-COND-FAIL: 未知注入名 %r；可用：%s"
              % (which, ", ".join(sorted(cases))))
        return 2

    results, allok = [], True
    for item in inj:
        if item.get("ok") is False:
            results.append(item)
            allok = False
            continue
        rule, name = item["rule"], item["name"]
        an = analyze(item["sources"], root=REPO)
        _r, _f, vio, errors, _s, _rt = judge(an, root=REPO)
        hit = [v for v in vio if v["rule"] == rule and v["name"] == name]
        err_hit = [e for e in errors if rule in e]
        ok = bool(err_hit) if rule == "C6" else bool(hit)
        allok = allok and ok
        results.append({"injection": item["injection"], "ok": ok,
                        "red": bool(vio) or bool(errors),
                        "expect": "%s %s" % (rule, name),
                        "violations": [v["rule"] + " " + v["name"] for v in vio[:10]],
                        "witness": [v["witness"] for v in vio[:3]],
                        "errors": errors[:5]})
    print(json.dumps({"tool": "check_ctest_reg_condition.py", "mode": "fault-inject",
                      "selection": which, "injections": results,
                      "failed": len([r for r in results if not r.get("ok")]),
                      "verdict": "PASS" if allok else "FAIL"},
                     ensure_ascii=False, indent=2))
    return 0 if allok else 1


# --------------------------------------------------------------------------- CLI ----

def _utc_now():
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _git_head(repo):
    try:
        r = subprocess.run(["git", "-C", str(repo), "rev-parse", "HEAD"],
                           capture_output=True, text=True, timeout=30)
        return r.stdout.strip() if r.returncode == 0 else ""
    except (OSError, subprocess.SubprocessError):
        return ""


def main(argv=None):
    ap = argparse.ArgumentParser(description="CTest 注册面/引用面条件上下文一致性门")
    ap.add_argument("--repo", default=str(REPO))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", "--selftest", dest="self_test", action="store_true")
    ap.add_argument("--fault-inject", default=None, metavar="CASE",
                    help="真实扫描面注入（all 或单例名），每例必须判红")
    ap.add_argument("--inventory", action="store_true", help="打印扫描面清单与覆盖统计")
    args = ap.parse_args(argv)

    if args.self_test:
        return run_self_test()
    if args.fault_inject:
        return run_fault_inject(args.fault_inject)

    repo = pathlib.Path(args.repo).resolve()
    try:
        srcs, untracked = collect_real_sources(repo)
    except Unavailable as exc:
        print("CTEST-REG-COND-FAIL: UNAVAILABLE %s" % exc)
        return 2
    surface = load_surface_tool()
    an = analyze(srcs, root=repo)
    _reg, _ref, vio, errors, summ, runtime = judge(an, root=repo)
    lines = []
    for v in vio:
        lines.append("%s %s: %s" % (v["rule"], v["name"], v["why"]))
        for r in v["ref"]:
            lines.append("%s  引用 %s  [条件 %s]  %s"
                         % (v["rule"], r["at"], r["cond"], r["call"]))
        for r in v["reg"]:
            lines.append("%s  注册 %s  [条件 %s]  %s"
                         % (v["rule"], r["at"], r["cond"], r["call"]))
        if not v["reg"]:
            lines.append("%s  注册 无（从未注册）" % v["rule"])
        lines.append("%s  见证组合 %s" % (v["rule"], v["witness"]))
    rt_lines = []
    for v in runtime:
        rt_lines.append("%s %s（依赖面·运行期类别）: %s" % (v["rule"], v["name"], v["why"]))
        for r in v["ref"]:
            rt_lines.append("%s  依赖 %s  [条件 %s]  %s"
                            % (v["rule"], r["at"], r["cond"], r["call"]))
        for r in v["reg"]:
            rt_lines.append("%s  注册 %s  [条件 %s]  %s"
                            % (v["rule"], r["at"], r["cond"], r["call"]))
        if not v["reg"]:
            rt_lines.append("%s  注册 无（从未注册）" % v["rule"])
        rt_lines.append("%s  见证组合 %s" % (v["rule"], v["witness"]))
    failclosed = len(errors)
    errors_all = lines + errors

    report = {
        "tool": "check_ctest_reg_condition.py",
        "rule": "CHK-CTEST-REG-CONDITION（配置期注册一致性：注册面/引用面条件上下文交叉判定）",
        "generated_utc": _utc_now(),
        "baseline_commit": _git_head(repo),
        "scan": summ,
        "scan_surface": {
            "tracked_only": True,
            "surface_tool": SURFACE_TOOL_REL,
            "skip_dir_names": sorted(surface.SKIP_DIR_NAMES),
            "skip_path_substrings": list(surface.SKIP_PATH_SUBSTR),
            "note": ("与 eng/tools/quality/check_ctest_registration.py 同一扫描面口径；"
                     "run/ 下的隔离副本与证据副本属历史快照，不在扫描面"
                     "（AGENTS.md §7 目录规范）"),
        },
        "untracked_cmake_sources": untracked,
        "violations": vio,
        "runtime_findings": runtime,
        "runtime_findings_count": len(runtime),
        "runtime_semantics": ("依赖面（DEPENDS）实测非配置期致命：cmake configure exit 0、"
                              "ctest -N exit 0、执行该测试 exit 0（探针工程 "
                              "run/FINAL-07/审核包/CI/ctest-cond-fixtures/depends_probe/）；"
                              "故 RC3 命中逐条打印并计数，但不并入主判红——"
                              "主判红只针对 configure 期必然 FATAL 的 RC1/RC2"),
        "failclosed_count": failclosed,
        "error_count": len(errors_all),
        "errors": errors_all,
        "verdict": "PASS" if not errors_all else "FAIL",
    }
    text = json.dumps(report, ensure_ascii=False, indent=2)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        if not out.is_absolute():
            out = repo / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text + "\n", encoding="utf-8")
    if args.inventory:
        print(json.dumps({"scan": summ, "untracked_cmake_sources": untracked,
                          "verdict": report["verdict"]},
                         ensure_ascii=False, indent=2))
        return 0 if not errors_all else 1
    print("[check_ctest_reg_condition] sources=%d add_test=%d refs=%d deps=%d "
          "names=%d/%d atoms=%d violations=%d runtime_dep_findings=%d failclosed=%d "
          "verdict=%s"
          % (summ["sources"], summ["add_test_calls"], summ["reference_calls"],
             summ["dependency_occurrences"], summ["registered_names"],
             summ["referenced_names"], len(summ["atom_inventory"]), len(vio),
             len(runtime), failclosed, report["verdict"]))
    for line in errors_all:
        print(line)
    for line in rt_lines:
        print(line)
    print(text)
    return 0 if not errors_all else 1


if __name__ == "__main__":
    sys.exit(main())
