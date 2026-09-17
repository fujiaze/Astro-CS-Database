#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_cmake_usebeforedef.py — CMake 门「use-before-define / 退化命令」检查器。

事由（W4-A9 确定性翻转实验，2026-09-17）：lib/algorithms/noise_snr/tests/p1noise/
CMakeLists.txt 的 add_test(p1noise_abi_layout COMMAND ${P1NOISE_PYTHON3} ...)
出现在 find_program(P1NOISE_PYTHON3 ...) 之前 ⇒ 全新 build 目录首次 configure 时
该变量为空，CTestTestfile 里命令退化成裸 .py 路径（无可执行位）⇒ 门以 BAD_COMMAND
假红；**再 configure 一次**（缓存已写入该变量）后 100% 通过。绿色与否只取决于
「该目录是否被 configure 过至少一次」—— 典型假绿/假红双面门（该处已修）。

判据（ENGINEERING_SPEC §8 fail-closed）：
  C1 use_before_define  同一文件内 COMMAND 行使用的 ${VAR}，其
                        set/option/find_program/find_package 定义行出现在之后
                        ⇒ 判红（首次 configure 取值空）。
  C2 bare_script_path   COMMAND 中直接给 .py/.sh/.pl 脚本路径而无解释器前缀，
                        且该文件无执行位 ⇒ 判红（退化为 BAD_COMMAND）。
  C3 conditional_gate   仅作清单输出（不计入退出码）：if(VAR) 包裹 add_test 而该
                        块内无 message(FATAL_ERROR|SEND_ERROR) ⇒ 依赖缺失时门
                        静默消失（fail-open）候选面。

用法：
  python3 tools/quality/check_cmake_usebeforedef.py [--root .] [--json-out F]
  python3 tools/quality/check_cmake_usebeforedef.py --selftest
exit 0 = PASS（C1/C2 零命中），1 = FAIL，2 = 用法错误。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import stat
import sys
import tempfile

SKIP_SEGMENTS = ("/build", "/build_", "/run/", "/.git", "/third_party", "/archive",
                 "/问题扫描", "/reports", "/_deps", "/CMakeFiles")

DEF_RE = re.compile(r"\b(set|option|find_program|find_package|find_library|find_path|"
                    r"pkg_check_modules)\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)")
USE_DIRECTIVES = ("add_test", "add_custom_command", "add_custom_target",
                  "execute_process", "try_run")
VAR_RE = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}")
COND_OPEN_RE = re.compile(r"^\s*if\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
COND_ANY_RE = re.compile(r"^\s*(if|elseif)\s*\(")
FATAL_RE = re.compile(r"message\s*\(\s*(FATAL_ERROR|SEND_ERROR)")
SCRIPT_RE = re.compile(r"([^\s\"']+\.(?:py|sh|pl|bash))")


def iter_cmake_files(root):
    for dirpath, dirnames, filenames in os.walk(root):
        posix = dirpath.replace(os.sep, "/")
        if any(seg in posix + "/" for seg in SKIP_SEGMENTS):
            dirnames[:] = []
            continue
        dirnames[:] = [d for d in dirnames
                       if not any(s.strip("/") == d for s in SKIP_SEGMENTS)]
        for fn in filenames:
            if fn == "CMakeLists.txt" or fn.endswith(".cmake"):
                yield os.path.join(dirpath, fn)


def _strip_comment(text):
    """去掉 CMake 行内注释（# 起）；注释里的变量引用不构成使用点。"""
    idx = text.find("#")
    return text if idx < 0 else text[:idx]


def _logical_lines(lines):
    """把续行（行尾反斜杠）与括号内多行拼成逻辑行，返回 (起始行号, 文本)。"""
    out = []
    i = 0
    while i < len(lines):
        start = i + 1
        buf = lines[i]
        depth = buf.count("(") - buf.count(")")
        while i + 1 < len(lines) and (buf.rstrip().endswith("\\") or depth > 0):
            i += 1
            buf += " " + lines[i]
            depth = buf.count("(") - buf.count(")")
        out.append((start, buf))
        i += 1
    return out


def scan_file(path, root):
    with open(path, encoding="utf-8", errors="replace") as fh:
        raw = fh.read().splitlines()
    logical = [(n, _strip_comment(t)) for n, t in _logical_lines(raw)]
    defs = {}
    for ln_no, text in logical:
        for m in DEF_RE.finditer(text):
            defs.setdefault(m.group(2), ln_no)
    c1, c2, c3 = [], [], []
    for ln_no, text in logical:
        if not any(d in text for d in USE_DIRECTIVES):
            continue
        for m in VAR_RE.finditer(text):
            var = m.group(1)
            if var in defs and defs[var] > ln_no:
                c1.append({"file": os.path.relpath(path, root), "line": ln_no,
                           "var": var, "defined_at": defs[var],
                           "text": text.strip()[:160]})
        if "COMMAND" in text:
            head = text.split("COMMAND", 1)[1]
            for m in SCRIPT_RE.finditer(head):
                tok = m.group(1)
                if tok.startswith("$") or tok.startswith("<"):
                    continue  # 变量展开/生成器表达式，静态不可判
                before = head[:m.start()].strip().split()
                if before and ("python" in before[-1] or before[-1] in ("sh", "bash", "perl")):
                    continue  # 有解释器前缀
                cand = os.path.normpath(os.path.join(os.path.dirname(path), tok))
                if not os.path.exists(cand):
                    cand = os.path.normpath(os.path.join(root, tok.lstrip("/")))
                if os.path.exists(cand) and not (os.stat(cand).st_mode & stat.S_IXUSR):
                    c2.append({"file": os.path.relpath(path, root), "line": ln_no,
                               "script": tok, "text": text.strip()[:160]})
    stack = []
    for ln_no, text in logical:
        m = COND_OPEN_RE.match(text)
        if m:
            stack.append({"var": m.group(1), "line": ln_no, "has_fatal": False, "tests": 0})
            continue
        if text.strip().startswith("endif"):
            if stack:
                blk = stack.pop()
                if blk["tests"] and not blk["has_fatal"]:
                    c3.append({"file": os.path.relpath(path, root), "line": blk["line"],
                               "var": blk["var"], "tests": blk["tests"],
                               "text": "if(%s) 内 %d 个 add_test，块内无 FATAL_ERROR"
                                       % (blk["var"], blk["tests"])})
            continue
        if COND_ANY_RE.match(text):
            continue
        if FATAL_RE.search(text):
            for blk in stack:
                blk["has_fatal"] = True
        if "add_test" in text:
            for blk in stack:
                blk["tests"] += 1
    return c1, c2, c3


def run_scan(root):
    c1, c2, c3 = [], [], []
    files = list(iter_cmake_files(root))
    for p in files:
        a, b, c = scan_file(p, root)
        c1 += a
        c2 += b
        c3 += c
    return {"files": len(files), "use_before_define": c1, "bare_script_path": c2,
            "conditional_gate_inventory": c3}


def selftest():
    cases = []
    with tempfile.TemporaryDirectory(prefix="ubd-") as td:
        d = os.path.join(td, "ok")
        os.makedirs(d)
        open(os.path.join(d, "CMakeLists.txt"), "w", encoding="utf-8").write(
            "find_program(PY3 NAMES python3)\n"
            "add_test(NAME t COMMAND ${PY3} ${CMAKE_CURRENT_SOURCE_DIR}/x.py)\n")
        cases.append(("pos_defined_first", len(run_scan(d)["use_before_define"]), 0))
        d2 = os.path.join(td, "bad")
        os.makedirs(d2)
        open(os.path.join(d2, "CMakeLists.txt"), "w", encoding="utf-8").write(
            "add_test(NAME t COMMAND ${PY3} ${CMAKE_CURRENT_SOURCE_DIR}/x.py)\n"
            "find_program(PY3 NAMES python3)\n")
        cases.append(("neg_use_before_define", len(run_scan(d2)["use_before_define"]), 1))
        d3 = os.path.join(td, "bare")
        os.makedirs(d3)
        open(os.path.join(d3, "x.py"), "w", encoding="utf-8").write("print(1)\n")
        open(os.path.join(d3, "CMakeLists.txt"), "w", encoding="utf-8").write(
            "add_test(NAME t COMMAND ./x.py)\n")
        cases.append(("neg_bare_script_noexec", len(run_scan(d3)["bare_script_path"]), 1))
        os.chmod(os.path.join(d3, "x.py"), 0o755)
        cases.append(("pos_bare_script_exec", len(run_scan(d3)["bare_script_path"]), 0))
    ok = True
    for name, got, want in cases:
        good = got == want
        ok = ok and good
        print("[selftest] %-28s findings=%d want=%d %s"
              % (name, got, want, "OK" if good else "MISMATCH"))
    print("[selftest] %d cases, %s" % (len(cases), "ALL OK" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="CMake use-before-define / 退化命令检查")
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print("USAGE_ERROR: root not found: %s" % root, file=sys.stderr)
        return 2
    res = run_scan(root)
    hard = res["use_before_define"] + res["bare_script_path"]
    if args.json_out:
        parent = os.path.dirname(os.path.abspath(args.json_out))
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump({"schema": "astrocs/cmake-usebeforedef/v1", "root": root,
                       "verdict": "PASS" if not hard else "FAIL", **res}, fh,
                      ensure_ascii=False, indent=1, sort_keys=True)
    if hard:
        print("CMAKE_USEBEFOREDEF_FAIL:")
        for h in res["use_before_define"][:40]:
            print("  [C1_use_before_define] %s:%d var=%s defined_at=:%d"
                  % (h["file"], h["line"], h["var"], h["defined_at"]))
        for h in res["bare_script_path"][:40]:
            print("  [C2_bare_script_path] %s:%d %s"
                  % (h["file"], h["line"], h["script"]))
        return 1
    print("CMAKE_USEBEFOREDEF_PASS: files=%d C1=0 C2=0 (C3 条件门清单 %d 条)"
          % (res["files"], len(res["conditional_gate_inventory"])))
    for c in res["conditional_gate_inventory"][:20]:
        print("  [C3_conditional_gate] %s:%d if(%s) add_test=%d"
              % (c["file"], c["line"], c["var"], c["tests"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
