#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""断言判别力静态检查（GATE-502）——「空断言 / 恒真断言」防复发门。

职责（fail-closed，无豁免清单）：
  1. Python（eng/tests/**/*.py）：每个 test_* 函数/方法必须有**判别力断言** ——
     assert 语句、unittest/numpy 断言方法、self.fail、assertRaises 上下文，
     以及**一层**同文件辅助函数内的断言（辅助函数解析，防误报）。
     无断言 ⇒ FAIL(no_assertion)。
  2. Python 恒真断言：assertTrue(True) / assertFalse(False) / assertIs(True, True) /
     assertEqual(<同值常量>, <同值常量>) / assertIsNotNone(<常量>) / assert(<常量真值>)
     ⇒ FAIL(constant_assertion)。
  3. C++（eng/tests/**/*.{cpp,cc,cxx,hpp,h}，先剥注释）：
     CHECK(true) / CHECK(1) / ASSERT_TRUE(true) / EXPECT_TRUE(true) / assert(true) /
     static_assert(true) ⇒ FAIL(constant_assertion)。io_adapter 曾发现的
     `ASSERT_TRUE(true, "...")` 软通过即此类。

为什么需要它：ENGINEERING_SPEC §10「每项检查有正例与负例（能红能绿）」+ AGENTS §9
「空断言、SKIP 充数、检查器静默退化都算未完成」。人眼普查会随提交复发，故做成机器门。

用法：
  python3 eng/tools/quality/check_test_discriminative.py              # 真扫仓库
  python3 eng/tools/quality/check_test_discriminative.py --quiet
  python3 eng/tools/quality/check_test_discriminative.py --self-test  # 正负例（红绿双向）
  python3 eng/tools/quality/check_test_discriminative.py --json-out X
退出码：0 PASS；1 FAIL（含输入缺失 fail-closed）；2 用法错误或产物写入失败
（TOOLING_FAILURE：--json-out 父目录缺失时自建，仍失败则具名报错不 traceback
—— 01_CHECKS §1「不得 traceback」/「fail-closed」）。

注：本检查器为 GATE-502 新增；注册表条目（eng/ci/checks.json）属 GATE-501 域 ——
    GATE-502 不越域改注册表，登记请求见任务回执。
"""
import argparse
import ast
import json
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_DEFAULT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
SCAN_ROOT = "eng/tests"

PY_SUFFIX = (".py",)
CPP_SUFFIX = (".cpp", ".cc", ".cxx", ".hpp", ".h", ".c")   # ACCEPT-501：补 .c（eng/tests 有 28 个 .c 此前完全不扫）

# C++ 恒真断言（剥注释后匹配）：CHECK(true) / ASSERT_TRUE(true) 等软通过形态。
CPP_TRIVIAL = [
    # TEST_CHECK 是 lib/ 侧测试框架的宏名（lib/infrastructure/pipeline/orchestrator、
    # lib/infrastructure/aio 的 tests）；本门默认只扫 eng/tests，但形态必须一并识别，
    # 否则扫描面一放宽就会漏（GATE-502：lib/ 侧现存 8 处恒真断言，已派单 A 线）。
    (r"\b(?:TEST_)?(?:CHECK|REQUIRE|ASSERT|EXPECT|VERIFY)_TRUE\s*\(\s*true\b",
     "CHECK/ASSERT_TRUE(true)"),
    (r"\b(?:TEST_)?(?:CHECK|ASSERT|EXPECT|REQUIRE|VERIFY)\s*\(\s*true\b", "CHECK/ASSERT(true)"),
    # ACCEPT-501：原正则要求单参 CHECK(1)，漏掉带消息形态 CHECK(1, "msg")。
    # 逗号形态同样是恒真（判据与消息无关），必须一并识别。
    (r"\b(?:TEST_)?(?:CHECK|ASSERT|EXPECT|REQUIRE|VERIFY)\s*\(\s*1\s*[,)]", "CHECK/ASSERT(1)"),
    (r"\bstatic_assert\s*\(\s*true\b", "static_assert(true)"),
    (r"\bassert\s*\(\s*true\b", "assert(true)"),
    # 恒真析取尾：上面 5 条全部要求实参**以字面 true/1 开头**，
    # 因此 `CHECK(cond) || true` / `CHECK(x == Ok || true)` 一条都匹配不上。
    # 本仓实测两处最高危恒真（p1_resource_test.cpp:78 的发布门判决、
    # p1cos_tests_badcol.cpp:412）都是析取尾形态，而本门当时是绿的。
    # 该形态无论左边是什么，整个表达式恒真 ⇒ 必须判红。
    # **必须带断言宏前缀**：裸 `|| true` 会命中 shell 命令串里的
    # `ls ... | grep -c X || true`（那是让 grep 退出码不失败的**合法** shell 写法，
    # 实测误报于 p3_export_e2e_test.cpp:377）。
    # **前缀必须用后缀通配**：`(?:CHECK|ASSERT|EXPECT|...)[A-Z_]*\s*\(`。
    #   首版用闭集（`(?:CHECK|ASSERT|...)\s*\(`）造成**覆盖面回退**，实测 5 类真恒真
    #   从 RED 变 ok：return 形态、EXPECT_TRUE、CHECK_EQ、ASSERT_EQ、CHECK_MESSAGE
    #   （`_TRUE(` 的下划线使 `EXPECT` 后无法紧跟 `(`，回溯也救不回来）。
    (r"\b(?:TEST_CHECK|CHECK|ASSERT|EXPECT|REQUIRE|VERIFY|check|test)[A-Z_]*\s*\([^;\n]*\|\|\s*true\b",
     "断言内析取尾 `|| true`（整个断言恒真）"),
    #   `return <cond> || true;` 形态：不在断言宏内（V15-N-14 登记的原形态，
    #   曾被闭集前缀漏掉）。**不锚定**是有意的：全仓该形态只在函数返回处出现，
    #   而 shell 串里的 `|| true` 总在引号内且前面是 `grep`/`ls` 之类命令词。
    (r"^\s*return\s+[^;\n]*\|\|\s*true\s*;", "返回 `return … || true`（函数恒返回真）"),
]
CPP_TRIVIAL_RE = [(re.compile(p), name) for p, name in CPP_TRIVIAL]


def strip_cpp_comments(text):
    """剥 // 行注释与 /* */ 块注释（保留行号：注释内容替换为空格）。"""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


# ── Python AST 面 ────────────────────────────────────────────────────────────
def _is_assert_call(node):
    """被调名是否算「判别力断言」。

    房规（FINAL-07 第4轮 `r4-lib-tautology` 独立复算，lib 侧实测 24 条 no_assertion 全是假阳）：
    lib/ 的测试套件不写 `assert*`，写 `check(cond,msg)` / `_assert(cond,msg)`，
    另有 numpy 风格的 `assert_allclose` / `assert_array_equal`。
    **分类依据是「返回值是否最终影响退出码」，不是「有没有计数器」** ——
    两种形态都闭合到 `sys.exit`：
      A  `test_pipeline_blocks.py:50-58` -> `:533 return 0 if FAIL == 0 else 1`
      B  `acceptance_drizzle.py:71-74`（只 print + return cond）-> `:344` 收进 results
         -> `:349-352` 汇总 -> `:358 return 0 if failed == 0 else 1`
    「只 print 不计数」不等于「失败不红」—— 读函数体不足以定性，**必须读到退出码**。
    """
    if not isinstance(node, ast.Call):
        return False
    f = node.func
    # `pytest.raises(...)` / `numpy.testing.assert_*` 也是判别力断言。
    if isinstance(f, ast.Attribute):
        # f.attr: assertXxx / fail / check / _assert / raises / assert_allclose …
        if f.attr.startswith("assert") or f.attr in ("fail", "check", "_assert", "raises"):
            return True
        return False
    if isinstance(f, ast.Name):
        return f.id.startswith("assert") or f.id in ("fail", "check", "_assert")
    return False


def _const_value(node):
    """字面常量 → (True, 值)；非常量 → (False, None)。

    含**恒真析取尾**：`x or True` / `True or x` / `cond and True` 恒真，
    之前因只认 ast.Constant 而一律返回 (False, None) ⇒ 漏判。
    本仓实测两处最高危恒真（oracle 的 `all(k in ds or True ...)`、
    `known_failures_baseline.py:1096`）正落在这个缺口里，而门当时是绿的。
    """
    if isinstance(node, ast.Constant):
        return True, node.value
    # 恒真析取尾：or 的右操作数是 truthy 常量 ⇒ 整个表达式恒真；
    # and 的右操作数是 falsy 常量 ⇒ 整个表达式恒假。
    if isinstance(node, ast.BoolOp):
        vals = [v for v in (_const_value(x) for x in node.values) if v[0]]
        if len(vals) != len(node.values):
            return False, None          # 非常量，不能判
        if isinstance(node.op, ast.Or):
            return True, any(bool(v) for _, v in vals)
        return True, all(bool(v) for _, v in vals)
    return False, None


def _is_raises_ctx(item):
    if not isinstance(item, ast.withitem):
        return False
    ctx = item.context_expr
    if isinstance(ctx, ast.Call) and isinstance(ctx.func, ast.Attribute):
        return ctx.func.attr.startswith("assertRaises")
    return False


def _helper_names(tree):
    out = {}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            out.setdefault(node.name, node)
    return out


def _count_assertions(fn, helpers, depth=0):
    """判别力断言计数（含一层辅助函数解析）。"""
    n = 0
    for node in ast.walk(fn):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node is not fn:
            continue
        if isinstance(node, ast.Assert):
            n += 1
        elif isinstance(node, ast.Call):
            if _is_assert_call(node):
                n += 1
            elif depth == 0:
                name = None
                if (isinstance(node.func, ast.Attribute)
                        and isinstance(node.func.value, ast.Name)
                        and node.func.value.id == "self"):
                    name = node.func.attr
                elif isinstance(node.func, ast.Name):
                    name = node.func.id
                if name and name in helpers and helpers[name] is not fn:
                    n += _count_assertions(helpers[name], helpers, depth + 1)
        elif isinstance(node, ast.With):
            if any(_is_raises_ctx(it) for it in node.items):
                n += 1
    return n


def _trivial_python(fn):
    """恒真断言清单（行号 + 形态）。"""
    bad = []
    for node in ast.walk(fn):
        if isinstance(node, ast.Assert):
            ok, v = _const_value(node.test)
            if ok and bool(v):
                bad.append((node.lineno, "assert <truthy constant>"))
            continue
        if not _is_assert_call(node):
            continue
        f = node.func
        meth = f.attr if isinstance(f, ast.Attribute) else f.id
        args = node.args
        if meth in ("assertTrue", "assert_") and len(args) == 1:
            ok, v = _const_value(args[0])
            if ok and bool(v):
                bad.append((node.lineno, "%s(<truthy constant>)" % meth))
        elif meth == "assertFalse" and len(args) == 1:
            ok, v = _const_value(args[0])
            if ok and not bool(v):
                bad.append((node.lineno, "assertFalse(<falsy constant>)"))
        elif meth in ("assertEqual", "assertIs", "assertAlmostEqual") and len(args) >= 2:
            ok1, v1 = _const_value(args[0])
            ok2, v2 = _const_value(args[1])
            if ok1 and ok2 and v1 == v2:
                bad.append((node.lineno, "%s(<same constant>, <same constant>)" % meth))
        elif meth == "assertIsNotNone" and len(args) == 1:
            ok, _ = _const_value(args[0])
            if ok:
                bad.append((node.lineno, "assertIsNotNone(<constant>)"))
        elif meth in ("assertIn", "assertNotIn") and len(args) >= 2:
            ok1, v1 = _const_value(args[0])
            ok2, v2 = _const_value(args[1])
            if ok1 and ok2 and isinstance(v2, (str, list, tuple, set, dict)):
                hit = v1 in v2
                if (meth == "assertIn") == hit:
                    bad.append((node.lineno, "%s(<constant>, <literal>)" % meth))
    return bad


def scan_python(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        src = fh.read()
    tree = ast.parse(src, filename=path)
    helpers = _helper_names(tree)
    findings = []
    n_tests = 0
    for node in ast.walk(tree):
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            continue
        if not node.name.startswith("test"):
            continue
        n_tests += 1
        if _count_assertions(node, helpers) == 0:
            findings.append({"path": path, "line": node.lineno, "kind": "no_assertion",
                             "detail": "%s 无判别力断言" % node.name})
        for ln, what in _trivial_python(node):
            findings.append({"path": path, "line": ln, "kind": "constant_assertion",
                             "detail": "%s: %s" % (node.name, what)})
    return findings, n_tests


def scan_cpp(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        src = fh.read()
    clean = strip_cpp_comments(src)
    findings = []
    for i, line in enumerate(clean.splitlines(), 1):
        for rx, name in CPP_TRIVIAL_RE:
            if rx.search(line):
                findings.append({"path": path, "line": i, "kind": "constant_assertion",
                                 "detail": name})
    return findings


def scan_repo(repo):
    root = os.path.join(repo, SCAN_ROOT)
    if not os.path.isdir(root):
        raise RuntimeError("扫描根不存在（fail-closed）: %s" % root)
    findings = []
    stats = {"py_files": 0, "cpp_files": 0, "py_tests": 0, "skipped": []}
    for dirpath, dirs, files in os.walk(root):
        dirs[:] = sorted(d for d in dirs if d != "__pycache__")
        for name in sorted(files):
            p = os.path.join(dirpath, name)
            rel = os.path.relpath(p, repo).replace(os.sep, "/")
            if name.endswith(PY_SUFFIX):
                stats["py_files"] += 1
                try:
                    f, n = scan_python(p)
                except SyntaxError as e:
                    with open(p, encoding="utf-8", errors="replace") as fh:
                        body = fh.read()
                    if "def test" in body:
                        findings.append({"path": rel, "line": getattr(e, "lineno", 0) or 0,
                                         "kind": "syntax_error",
                                         "detail": "测试模块语法错误: %s" % e.msg})
                    else:
                        # 非测试模块（无 test_* 定义）的语法错误不属本门职责，但必须可见。
                        stats["skipped"].append("%s: SyntaxError(%s)" % (rel, e.msg))
                    continue
                findings += [dict(x, path=os.path.relpath(x["path"], repo).replace(os.sep, "/"))
                             for x in f]
                stats["py_tests"] += n
            elif name.endswith(CPP_SUFFIX):
                stats["cpp_files"] += 1
                findings += [dict(x, path=os.path.relpath(x["path"], repo).replace(os.sep, "/"))
                             for x in scan_cpp(p)]
    return findings, stats


# ── self-test（红绿双向） ────────────────────────────────────────────────────
GOOD_PY = """import unittest


def helper(a):
    assert a > 0


class T(unittest.TestCase):
    def test_real(self):
        self.assertEqual(2 + 2, 4)

    def test_via_helper(self):
        helper(3)

    def test_raises(self):
        with self.assertRaises(ValueError):
            int("x")
"""

BAD_PY = {
    "test_no_assertion.py": """import unittest


class T(unittest.TestCase):
    def test_nothing(self):
        print("ran")
""",
    "test_true_assert.py": """import unittest


class T(unittest.TestCase):
    def test_true(self):
        self.assertTrue(True)
""",
    "test_same_const.py": """import unittest


class T(unittest.TestCase):
    def test_same(self):
        self.assertEqual("abc", "abc")
""",
    "test_not_none_const.py": """import unittest


class T(unittest.TestCase):
    def test_nn(self):
        self.assertIsNotNone(42)
""",
}

GOOD_CPP = """#include <cstdio>
static int failures = 0;
#define CHECK(c) do { if (!(c)) ++failures; } while (0)
int main() {
  // CHECK(true);  <-- 注释里的形态不得判红
  CHECK(1 + 1 == 2);
  return failures ? 1 : 0;
}
"""

BAD_CPP = {
    "empty_check.cpp": """#define CHECK(c) do { if (!(c)) ++failures; } while (0)
int main() { CHECK(true); return 0; }
""",
    "gtest_soft.cpp": """#define ASSERT_TRUE(c, m) do { if (!(c)) ++failures; } while (0)
int main() { ASSERT_TRUE(true, "soft pass"); return 0; }
""",
}


def self_test(repo):
    """正例必绿、负例逐条必红。返回 (ok, problems)。"""
    problems = []
    real, _ = scan_repo(repo)
    if real:
        problems.append("真实仓库未清零: %d 条（%s）"
                        % (len(real), ", ".join("%s:%d" % (f["path"], f["line"]) for f in real[:5])))
    tmp = tempfile.mkdtemp(prefix="astrocs_disc_")
    try:
        pos = os.path.join(tmp, "eng", "tests", "pos")
        os.makedirs(pos)
        with open(os.path.join(pos, "test_good.py"), "w", encoding="utf-8") as fh:
            fh.write(GOOD_PY)
        with open(os.path.join(pos, "good.cpp"), "w", encoding="utf-8") as fh:
            fh.write(GOOD_CPP)
        f, _ = scan_repo(tmp)
        if f:
            problems.append("正例被判红（假阳性）: %s"
                            % [(x["path"], x["line"], x["detail"]) for x in f])
        for name, text in list(BAD_PY.items()) + list(BAD_CPP.items()):
            neg = os.path.join(tmp, "eng", "tests", "neg")
            os.makedirs(neg, exist_ok=True)
            with open(os.path.join(neg, name), "w", encoding="utf-8") as fh:
                fh.write(text)
            g, _ = scan_repo(tmp)
            hits = [x for x in g if os.path.basename(x["path"]) == name]
            if not hits:
                problems.append("负例未被判红: %s" % name)
            os.remove(os.path.join(neg, name))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return (not problems), problems


def main(argv=None):
    ap = argparse.ArgumentParser(description="断言判别力静态检查（GATE-502）")
    ap.add_argument("--repo", default=REPO_DEFAULT)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--json-out")
    args = ap.parse_args(argv)
    repo = os.path.abspath(args.repo)
    if not os.path.isdir(os.path.join(repo, SCAN_ROOT)):
        print("FAIL: 扫描根不存在（fail-closed）: %s" % os.path.join(repo, SCAN_ROOT))
        return 1
    try:
        findings, stats = scan_repo(repo)
    except Exception as e:                                    # noqa: BLE001
        print("FAIL: 扫描失败（fail-closed）: %s" % e)
        return 1
    selftest = None
    if args.self_test:
        ok, problems = self_test(repo)
        n_neg = len(BAD_PY) + len(BAD_CPP)
        selftest = {"ok": ok, "problems": problems, "injections": n_neg, "positives": 2}
        if ok:
            # 门禁自证必须**可见**：正例必绿 + 每条负例逐条必红 + 真实仓库清零
            print("SELF_TEST PASS positives=2（正例必绿）injections=%d（负例逐条必红）"
                  % n_neg)
        else:
            print("SELF_TEST FAIL problems=%d" % len(problems))
            for p in problems:
                print("  - %s" % p)
    if findings:
        print("FAIL findings=%d" % len(findings))
        for f in findings:
            print("  %s:%d  %s  %s" % (f["path"], f["line"], f["kind"], f["detail"]))
    else:
        print("PASS py_files=%d py_tests=%d cpp_files=%d findings=0%s"
              % (stats["py_files"], stats["py_tests"], stats["cpp_files"],
                 "" if not stats["skipped"] else " skipped=%d" % len(stats["skipped"])))
    for s in stats["skipped"]:
        print("  note(skipped, 非测试模块): %s" % s)
    if args.json_out:
        # 产物落盘是检查器合同的一部分：父目录不存在时**自建**，仍失败则具名
        # TOOLING_FAILURE + rc=2（fail-closed），绝不 traceback（01_CHECKS §1
        # 「不得 traceback」）。原实现在父目录缺失时抛 FileNotFoundError
        # （复现：--json-out run/<task>/nonexistent_dir/out.json ⇒ rc=1 + traceback）。
        try:
            parent = os.path.dirname(os.path.abspath(args.json_out))
            if parent:
                os.makedirs(parent, exist_ok=True)
            with open(args.json_out, "w", encoding="utf-8") as fh:
                json.dump({"findings": findings, "stats": stats, "selftest": selftest}, fh,
                          ensure_ascii=False, indent=1)
        except OSError as exc:
            print("TOOLING_FAILURE: 写 json-out 失败 %s: %s" % (args.json_out, exc),
                  file=sys.stderr)
            return 2
    if selftest is not None and not selftest["ok"]:
        return 1
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
