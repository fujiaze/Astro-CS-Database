#!/usr/bin/env python3
"""CHK-EXIT-CONSISTENCY：结论与退出码一致性体检（W4-A3 新增登记门）。

口径：打印 FAIL 必须 rc≠0。检查器"打印红却返回 0"是 fail-open —— 上层门禁按退出码判
生死，于是最该拦住的失败被静默放过。

判据（AST 静态，不执行被测脚本）：
  S1 出现打印失败结论的字面量（FAIL / VIOLATION / _FAIL），但全文件没有任何非零退出
     路径（return <非零> / sys.exit(<非零>) / SystemExit(<非零>)）；
  S2 raise SystemExit(<fn>()) 传导（有无 __main__ 守卫都算），而该被委托函数体内没有
     return <非零>。

判据刻意偏向不误报：只把字面量 0/None 当作恒 0；条件表达式、变量、函数调用一律算作
存在非零路径 —— 否则会把 ci/run_checks.py、ci/run.py 这类条件返回误判成 fail-open。
委托型 raise SystemExit(fn()) 不计入 S1 的非零路径，交由 S2 判（否则 S1 永不触发）。
被委托函数名按真实名字解析，不假定叫 main。

能力边界（capability_gaps，随 JSON 公开）：静态只判是否存在潜在非零退出路径，不证明
该路径在失败时真的被走到。门声明自己的局限比假装完备可信。
"""
import argparse
import ast
import json
import os
import pathlib
import re
import sys

FAIL_TOKEN = re.compile(r"(FAIL|VIOLATION|_FAIL\b)")
KNOWN_UNPROVABLE = {
    "tools/quality/build_v19r4_package.py": (
        "打包/取证脚本，不是门禁检查器：FAIL 字面量出现在写出的取证 JSON"
        "（final_archive_reconciliation.result），而不是门的结论；其退出码语义是"
        "本次打包是否成功，与被打包内容的对账结论本就不同层面。若要求它自身对账"
        "失败即非零，应由 PKG/ORCH 线另开工单。"
    ),
}


def _iter_scripts(root):
    for base in ("tools", "ci"):
        d = os.path.join(root, base)
        for dirpath, dirnames, filenames in os.walk(d):
            dirnames[:] = [x for x in dirnames
                           if x not in ("__pycache__", "node_modules", ".git")]
            for fn in filenames:
                if fn.endswith(".py"):
                    yield os.path.join(dirpath, fn)


def _is_zero(node):
    if node is None:
        return True
    if isinstance(node, ast.Constant):
        return node.value in (0, None, False)
    return False


def _call_name(node):
    return getattr(node.func, "attr", None) or getattr(node.func, "id", None)


def _nonzero_returns(tree):
    n = 0
    for node in ast.walk(tree):
        if isinstance(node, ast.Return) and not _is_zero(node.value):
            n += 1
        if isinstance(node, ast.Raise) and isinstance(node.exc, ast.Call):
            if _call_name(node.exc) != "SystemExit" or not node.exc.args:
                continue
            arg = node.exc.args[0]
            if isinstance(arg, ast.Call):
                continue  # 委托型：交 S2 判，否则 S1 永不触发
            if not _is_zero(arg):
                n += 1
        if isinstance(node, ast.Call):
            if _call_name(node) in ("exit", "_exit") and node.args and not _is_zero(node.args[0]):
                n += 1
    return n


def _failsafe_prints(tree, src):
    lines = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Call) and _call_name(node) in ("print", "write"):
            seg = ast.get_source_segment(src, node) or ""
            if FAIL_TOKEN.search(seg):
                lines.append(node.lineno)
    return lines


def _delegated_fn(tree):
    """raise SystemExit(<fn>()) 的被委托函数名（无则 None）；有无 __main__ 守卫都算。"""
    found = None
    for node in ast.walk(tree):
        if isinstance(node, ast.Raise) and isinstance(node.exc, ast.Call):
            if _call_name(node.exc) != "SystemExit" or not node.exc.args:
                continue
            arg = node.exc.args[0]
            if isinstance(arg, ast.Call):
                nm = _call_name(arg)
                if nm:
                    found = nm
    return found


def _fn_has_nonzero(tree, fn_name):
    if not fn_name:
        return True
    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == fn_name:
            for sub in ast.walk(node):
                if isinstance(sub, ast.Return) and not _is_zero(sub.value):
                    return True
    return False


def scan(root):
    findings, clean, provable = [], [], []
    for path in sorted(_iter_scripts(root)):
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        try:
            src = open(path, encoding="utf-8", errors="replace").read()
            tree = ast.parse(src)
        except Exception:  # noqa: BLE001
            continue
        prints = _failsafe_prints(tree, src)
        if not prints:
            clean.append(rel)
            continue
        nz = _nonzero_returns(tree)
        delegated = _delegated_fn(tree)
        if nz == 0 and delegated is None:
            findings.append({"path": rel, "rule": "S1",
                             "detail": "打印失败结论但全文件无非零退出路径",
                             "print_lines": prints[:6]})
        elif delegated is not None and not _fn_has_nonzero(tree, delegated):
            findings.append({"path": rel, "rule": "S2",
                             "detail": ("SystemExit 委托 " + delegated
                                        + "() 传导，但该函数无 return <非零>"),
                             "print_lines": prints[:6]})
        else:
            provable.append(rel)
    return findings, clean, provable


# ── 自测面（§8 可执行负例面）：fixture 内层用单引号，无需转义 ──
SELFTEST_CASES = {
    "positive-clean": ([
        "def main():",
        "    print('OK_PASS')",
        "    return 0",
        "raise SystemExit(main())",
    ], 0),
    "positive-conditional-return": ([
        "def main():",
        "    bad = False",
        "    print('CHECK_FAIL' if bad else 'CHECK_PASS')",
        "    return 1 if bad else 0",
        "raise SystemExit(main())",
    ], 0),
    "positive-delegated-name": ([
        "def check():",
        "    bad = False",
        "    if bad:",
        "        print('X_FAIL')",
        "        return 1",
        "    return 0",
        "raise SystemExit(check())",
    ], 0),
    "negative-literal-zero": ([
        "def main():",
        "    print('GATE_FAIL: bad')",
        "    return 0",
        "raise SystemExit(main())",
    ], 1),
    "negative-no-exit-path": ([
        "def run():",
        "    print('VIOLATION: x')",
        "run()",
    ], 1),
    "negative-sys-exit-zero-after-fail": ([
"def main():",
"    print('FAIL: z')",
"    sys.exit(0)",
], 1),
}


def _self_test() -> int:
    import tempfile
    failures = []
    for name, (src_lines, expect) in sorted(SELFTEST_CASES.items()):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            (root / "tools").mkdir()
            (root / "tools" / "sample.py").write_text(
                chr(10).join(src_lines) + chr(10), encoding="utf-8")
            findings, _clean, _provable = scan(str(root))
        got = len(findings)
        bad = got != expect
        if bad:
            failures.append("%s: 期望 findings=%d，实际 %d（%s）"
                            % (name, expect, got,
                               findings[0]["rule"] if findings else "-"))
        print("  case %-40s %s" % (name, "FAIL" if bad else "OK"))
    if failures:
        print("SELFTEST_FAIL: %d" % len(failures))
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: %d 组（3 正例：干净 / 条件返回 / 被委托函数名解析；"
          "3 负例：字面量 return 0、无非零退出路径、sys.exit(0) 紧随失败结论）"
          % len(SELFTEST_CASES))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args()
    if args.self_test:
        return _self_test()
    root = os.path.abspath(args.root)
    findings, clean, provable = scan(root)
    live = [f for f in findings if f["path"] not in KNOWN_UNPROVABLE]
    registered = [{"path": p, "reason": r} for p, r in sorted(KNOWN_UNPROVABLE.items())
                  if any(f["path"] == p for f in findings)]
    summary = {
        "checker": "check_exit_conclusion_consistency.py",
        "root": root,
        "scanned": len(findings) + len(clean),
        "failed_conclusion_printers": len(findings),
        "findings": live,
        "registered_noncompliant": registered,
        "non_zero_path_present": len(provable),
        "capability_gaps": [
            "静态可达性分析：只判文件里是否存在潜在非零退出路径，不证明该路径在失败时",
            "真的被走到（需要逐脚本动态负例注入）。本轮对少量脚本做了人肉动态复核，",
            "证据见 run/PROJECT-GOVERNANCE-01/W4-A3/logs/after_EXIT-CONSISTENCY.log。",
            "已知不可静态判定：同一函数内先打印失败结论、随后在失败分支 return 0",
            "（但函数另有非零返回）—— 需要分支/数据流语义，本轮不判，登记为能力缺口。",
        ],
        "result": "EXIT_CONSISTENCY_FAIL" if live else "EXIT_CONSISTENCY_PASS",
    }
    text = json.dumps(summary, ensure_ascii=False, indent=1, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)) or ".", exist_ok=True)
        open(args.json_out, "w", encoding="utf-8").write(text + chr(10))
    if live:
        print("EXIT_CONSISTENCY_FAIL findings=%d" % len(live))
        for f in live:
            print("  [%s] %s：%s（打印行 %s）" % (f["rule"], f["path"], f["detail"],
                                                  f["print_lines"]))
    else:
        print("EXIT_CONSISTENCY_PASS scanned=%d findings=0" % summary["scanned"])
    return 1 if live else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        raise SystemExit(2)
