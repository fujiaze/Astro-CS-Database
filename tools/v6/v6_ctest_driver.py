#!/usr/bin/env python3
"""RUNTIME-CI-001 V6 CTest 门驱动（逐名验收锚 + fail-closed）。

语义（任务卡 + CONTROLLER_LOG C-004.4 / C-007 AR-033/AR-034）：
  * 根构建面（CMakeLists.txt / tests/unit/CMakeLists.txt / tests/integration/*）由控制器在
    W9 之后以独立集成提交注册 V6 目标；本驱动不注册目标，只**按名验收**。
  * 目标尚未注册（build 树无 CTestTestfile，或期望名不存在）时：**清晰 FAIL（rc 2）**，
    绝不静默通过；输出明确写出「等待控制器集成提交」与缺失名单。

用法:
  python3 tools/v6/v6_ctest_driver.py --build-dir <dir> --group unit|integration
        [--expect NAME]... [--expect-glob PAT]... [--output <json>]
exit 0 = 期望目标全部存在且通过；1 = 目标存在但有失败；2 = 目标缺失/构建树缺失（fail-closed）。
"""
from __future__ import annotations

import argparse
import fnmatch
import json
import pathlib
import re
import subprocess
import sys

DEFAULT_BUILD = "run/ci/build-gcc-release"


def list_tests(build_dir: pathlib.Path) -> list:
    p = subprocess.run(["ctest", "--test-dir", str(build_dir), "-N"],
                       capture_output=True, text=True, timeout=300)
    if p.returncode != 0:
        return []
    names = []
    for line in p.stdout.splitlines():
        m = re.search(r"Test\s+#\d+:\s+(\S+)", line)
        if m:
            names.append(m.group(1))
    return names


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=DEFAULT_BUILD)
    ap.add_argument("--group", default="unit")
    ap.add_argument("--expect", action="append", default=[])
    ap.add_argument("--expect-glob", action="append", default=[])
    ap.add_argument("--output", default="")
    args = ap.parse_args(argv)

    build_dir = pathlib.Path(args.build_dir)
    out = {"schema": "astrocs.v6.ctest-driver/v1", "group": args.group,
           "build_dir": str(build_dir), "expect": args.expect,
           "expect_glob": args.expect_glob, "verdict": "FAIL", "reason": "",
           "found": [], "missing": []}

    if not (build_dir / "CTestTestfile.cmake").exists():
        out["reason"] = (
            "V6 CTest 目标尚未注册进构建树: %s 无 CTestTestfile.cmake。"
            "根构建面注册由控制器在 RUNTIME-CI-001 之后的独立集成提交完成"
            "(C-004.4 / C-007 AR-033)；本门 fail-closed，不静默通过。" % build_dir)
        _emit(args.output, out)
        print("V6_CTEST_FAIL(prerequisite): " + out["reason"], file=sys.stderr)
        return 2

    tests = list_tests(build_dir)
    if not tests:
        out["reason"] = "ctest -N 无法枚举测试目标（构建树损坏或 ctest 不可用）"
        _emit(args.output, out)
        print("V6_CTEST_FAIL(prerequisite): " + out["reason"], file=sys.stderr)
        return 2

    selected = set()
    missing = []
    for name in args.expect:
        if name in tests:
            selected.add(name)
        else:
            missing.append(name)
    for pat in args.expect_glob:
        hits = [t for t in tests if fnmatch.fnmatchcase(t, pat)]
        if not hits:
            missing.append(pat + " (glob)")
        selected.update(hits)

    out["found"] = sorted(selected)
    out["missing"] = missing
    if missing:
        out["reason"] = ("V6 目标缺失（等待控制器集成提交注册）: " + ", ".join(missing))
        _emit(args.output, out)
        print("V6_CTEST_FAIL(missing): " + out["reason"], file=sys.stderr)
        return 2
    if not selected:
        out["reason"] = "未指定任何期望目标（不得空跑 PASS）"
        _emit(args.output, out)
        print("V6_CTEST_FAIL: " + out["reason"], file=sys.stderr)
        return 2

    regex = "^(" + "|".join(re.escape(t) for t in sorted(selected)) + ")$"
    p = subprocess.run(["ctest", "--test-dir", str(build_dir), "-R", regex,
                        "--output-on-failure"],
                       capture_output=True, text=True, timeout=7200)
    tail = (p.stdout + p.stderr)[-3000:]
    out["ctest_rc"] = p.returncode
    out["ctest_tail"] = tail
    out["verdict"] = "PASS" if p.returncode == 0 else "FAIL"
    out["reason"] = "all selected V6 targets passed" if p.returncode == 0 else "ctest reported failures"
    _emit(args.output, out)
    if p.returncode == 0:
        print("V6_CTEST_PASS group=%s targets=%d" % (args.group, len(selected)))
        return 0
    print("V6_CTEST_FAIL group=%s targets=%d rc=%d" % (args.group, len(selected), p.returncode),
          file=sys.stderr)
    print(tail)
    return 1


def _emit(path, out):
    if not path:
        return
    p = pathlib.Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(out, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
