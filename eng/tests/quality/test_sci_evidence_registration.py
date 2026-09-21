#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_sci_evidence_registration.py — SCI/ALG 点名测试的"三合一"证据门。

背景（R-2 / M4-F-01 改判）：SCI PHASE2_UPM §11/§15 以现在时把
control_median_mc_test / kcorr_matrix_test 当作**已通过的证据**；事实是这两个 TU
存在且受 git 跟踪，但属 tracked-but-unbuilt（全部 CMakeLists 非注释行 0 命中、
eng/ci/** 无测试选择器命中、ctest -N 无它们）。"文件存在"不等于可执行证据 ——
门必须打在证据链上。

门判据（三合一）：
  A. 文件存在 ∧ git tracked；
  B. 构建注册：名字（文件名或 target 名）出现在某个**受跟踪** CMakeLists.txt /
     *.cmake 的**非注释行**；
  C. CI 采集：登记的 ctest 名（或匹配它的正则，如 CTEST-PHASE2-GATES 的
     phase2_.*）出现在 eng/ci/checks.json / eng/ci/ctest_baseline.json 的**测试选择器面**
     （ctest_targets / --target / -R / --tests-regex），不匹配任意 glob 串。

双向一致（ENGINEERING_SPEC §8"注册表双向一致"）：
  - 声明 EXECUTABLE 的测试：A∧B∧C 必须全真，否则红；
  - 声明 MISSING 的测试：A∧B∧C 不得全真（一旦被收进构建/CI，本门立刻变红，
    强制文档从 MISSING 改回 EXECUTABLE）；
  - 文档点名：SCI/ALG 文档中每一条提到 watched 测试的行，其**名字邻域**
    （±40 字符窗口）必须与实际注册状态一致 —— 未注册的名字窗口内必须有
    MISSING 标记（未注册/MISSING/构建孤儿），已注册的名字窗口内不得有
    MISSING 标记（禁止过期登记）。

机器可执行负例（ENGINEERING_SPEC §8）：--fault-inject 注入 4 类故障，每类都必须
被判红；正常模式必须回到 rc=0（证明红由注入引起）。

用法：
  python3 eng/tests/quality/test_sci_evidence_registration.py --root .
  python3 eng/tests/quality/test_sci_evidence_registration.py --root . --self-test
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re
import subprocess
import sys
import unittest

# ── watched 证据表（唯一登记面；新增 SCI/ALG 点名的测试必须登记在此）────────
WATCHED = {
    # 阳性对照：已注册进根图（lib/algorithms/coverage/CMakeLists.txt 的
    # phase2_synthetic_gate）且被 eng/ci/checks.json CTEST-PHASE2-GATES
    # （--target phase2_.*）采集。
    "synthetic_gate.cpp": {
        "path": "lib/algorithms/coverage/tests/synthetic_gate.cpp",
        "cmake_tokens": ["synthetic_gate"],
        "ctest_names": ["phase2_synthetic_gate"],
        "must_be_registered": True,
    },
    # tracked-but-unbuilt（M4-F-01）：文件在、构建/CI 都不在。
    "control_median_mc_test.cpp": {
        "path": "lib/algorithms/drizzle/healpix_drizzle/tests/control_median_mc_test.cpp",
        "cmake_tokens": ["control_median_mc_test"],
        "ctest_names": [],
        "must_be_registered": False,
    },
    "kcorr_matrix_test.cpp": {
        "path": "lib/algorithms/drizzle/healpix_drizzle/tests/kcorr_matrix_test.cpp",
        "cmake_tokens": ["kcorr_matrix_test"],
        "ctest_names": [],
        "must_be_registered": False,
    },
}

DOC_GLOBS = ("docs/science/*.md", "docs/algorithms/*.md")
MISSING_MARKERS = ("未注册", "MISSING", "构建孤儿", "tracked-but-unbuilt")
PROXIMITY = 40
CI_REGISTRY_FILES = ("eng/ci/checks.json", "eng/ci/ctest_baseline.json")
CMAKE_GLOBS = ("CMakeLists.txt", "**/CMakeLists.txt", "**/*.cmake")
SKIP_DIRS = ("build", "run", ".git", "third_party", "out")
SELECTOR_FLAGS = ("--target", "-R", "--tests-regex", "-E")


def _run_git(root, args):
    try:
        p = subprocess.run(["git", "--no-optional-locks", "-C", root, *args],
                           capture_output=True, text=True, timeout=120)
        return p.returncode, p.stdout
    except (OSError, subprocess.SubprocessError):
        return 1, ""


def tracked_files(root):
    rc, out = _run_git(root, ["ls-files"])
    if rc != 0:
        return set()
    return {line.strip() for line in out.splitlines() if line.strip()}


def _iter_files(root, pattern):
    for path in glob.glob(os.path.join(root, pattern), recursive=True):
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        if any(rel == d or rel.startswith(d + "/") for d in SKIP_DIRS):
            continue
        if os.path.isfile(path):
            yield rel, path


_CMAKE_LINES_CACHE = {}


def cmake_lines(root, tracked):
    """受跟踪 CMake 文件的非注释行索引（进程内缓存；自检要跑 5 次）。"""
    key = (root, len(tracked))
    cached = _CMAKE_LINES_CACHE.get(key)
    if cached is not None:
        return cached
    entries = []
    seen = set()
    for pattern in CMAKE_GLOBS:
        for rel, path in _iter_files(root, pattern):
            if rel in seen:
                continue
            seen.add(rel)
            if rel not in tracked:
                continue
            try:
                with open(path, encoding="utf-8", errors="replace") as fh:
                    for line in fh:
                        if line.lstrip().startswith("#"):
                            continue
                        entries.append((rel, line))
            except OSError:
                continue
    _CMAKE_LINES_CACHE[key] = entries
    return entries


def cmake_registered(entries, tokens):
    """tokens 任一出现在受跟踪 CMake 文件的非注释行。返回命中文件列表。"""
    hits = []
    for rel, line in entries:
        if any(tok in line for tok in tokens):
            if rel not in hits:
                hits.append(rel)
    return hits


def _json_strings(obj):
    if isinstance(obj, str):
        yield obj
    elif isinstance(obj, dict):
        for v in obj.values():
            yield from _json_strings(v)
    elif isinstance(obj, list):
        for v in obj:
            yield from _json_strings(v)


def _ctest_selectors(obj):
    """只取"测试选择器"型 token：ctest_targets 值 + --target/-R/--tests-regex 实参。

    不能把任意 JSON 字符串（如 changed_paths 的 glob lib/**）当采集证据 ——
    否则任何名字都会被 glob 模式"匹配"到（门失效）。
    """
    if isinstance(obj, dict):
        for key, val in obj.items():
            if key in ("ctest_targets", "ctest_pattern", "test_ids") and isinstance(val, list):
                for v in val:
                    if isinstance(v, str):
                        yield v
            for v in obj.values():
                yield from _ctest_selectors(v)
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            if (isinstance(v, str) and v in SELECTOR_FLAGS
                    and i + 1 < len(obj) and isinstance(obj[i + 1], str)):
                yield obj[i + 1]
        for v in obj:
            yield from _ctest_selectors(v)


def _selector_matches(token, name):
    if name in token:
        return True
    try:
        return re.search(token, name) is not None
    except re.error:
        return False


def ci_collected(root, ctest_names):
    """登记的 ctest 名被 eng/ci/** 的测试选择器采集。返回命中文件列表。"""
    if not ctest_names:
        return []
    hits = []
    for rel in CI_REGISTRY_FILES:
        path = os.path.join(root, rel)
        if not os.path.isfile(path):
            continue
        try:
            with open(path, encoding="utf-8") as fh:
                data = json.load(fh)
        except (OSError, json.JSONDecodeError):
            continue
        selectors = list(_ctest_selectors(data))
        matched = False
        for name in ctest_names:
            if any(_selector_matches(tok, name) for tok in selectors):
                matched = True
                break
            if rel.endswith("ctest_baseline.json"):
                if any(tok == name for tok in _json_strings(data)):
                    matched = True
                    break
        if matched:
            hits.append(rel)
    return hits


def evidence_status(root, entry, tracked, cmake_entries):
    path = entry["path"]
    exists = os.path.isfile(os.path.join(root, path))
    is_tracked = path in tracked
    cmake_hits = cmake_registered(cmake_entries, entry.get("cmake_tokens", []))
    ci_hits = ci_collected(root, entry.get("ctest_names", []))
    return {
        "path": path,
        "exists": exists,
        "tracked": is_tracked,
        "cmake": cmake_hits,
        "ci": ci_hits,
        "registered": bool(exists and is_tracked and cmake_hits and ci_hits),
    }


def _name_occurrences(line, name):
    stem = name[:-4] if name.endswith(".cpp") else name
    positions = []
    start = 0
    while True:
        pos = line.find(name, start)
        if pos < 0:
            break
        positions.append((pos, len(name)))
        start = pos + 1
    if not positions:
        start = 0
        while True:
            pos = line.find(stem, start)
            if pos < 0:
                break
            positions.append((pos, len(stem)))
            start = pos + 1
    return positions


def doc_claim_failures(root, status, extra_lines=None):
    """文档点名的 watched 测试，其名字邻域必须与实际注册状态一致。"""
    problems = []
    extra_lines = extra_lines or {}
    for pattern in DOC_GLOBS:
        for rel, path in _iter_files(root, pattern):
            try:
                with open(path, encoding="utf-8", errors="replace") as fh:
                    lines = fh.read().splitlines()
            except OSError:
                continue
            lines = list(lines) + list(extra_lines.get(rel, []))
            for idx, line in enumerate(lines, start=1):
                for name, st in status.items():
                    for pos, length in _name_occurrences(line, name):
                        lo = max(0, pos - PROXIMITY)
                        hi = min(len(line), pos + length + PROXIMITY)
                        window = line[lo:hi]
                        has_marker = any(m in window for m in MISSING_MARKERS)
                        if st["registered"] and has_marker:
                            problems.append(
                                "%s:%d 已注册却标 MISSING（过期登记）: %s"
                                % (rel, idx, name))
                        elif not st["registered"] and not has_marker:
                            problems.append(
                                "%s:%d 点名未注册测试却无 MISSING 标记: %s"
                                % (rel, idx, name))
    return problems


def check_all(root, claims=None, extra_doc_lines=None, tracked=None):
    claims = dict(WATCHED) if claims is None else claims
    tracked = tracked_files(root) if tracked is None else tracked
    cmake_entries = cmake_lines(root, tracked)
    status = {}
    problems = []
    for name, entry in claims.items():
        st = evidence_status(root, entry, tracked, cmake_entries)
        status[name] = st
        want = bool(entry.get("must_be_registered", True))
        if want and not st["registered"]:
            missing = [k for k in ("exists", "tracked", "cmake", "ci") if not st[k]]
            problems.append(
                "声明 EXECUTABLE 但三合一不成立（缺 %s）: %s" % (",".join(missing), name))
        if not want and st["registered"]:
            problems.append(
                "声明 MISSING 但三合一已成立（文档必须改回 EXECUTABLE）: %s" % name)
    problems += doc_claim_failures(root, status, extra_doc_lines)
    report = {"root": root, "status": status, "problems": problems}
    return (1 if problems else 0), report


def self_test(root):
    """机器可执行负例面：4 类注入必须各自判红。"""
    tracked = tracked_files(root)
    base_rc, _ = check_all(root, tracked=tracked)
    failures = []
    injections = []

    claims = dict(WATCHED)
    claims["no_such_test_xyz.cpp"] = {
        "path": "lib/nope/no_such_test_xyz.cpp",
        "cmake_tokens": ["no_such_test_xyz"],
        "ctest_names": ["no_such_test_xyz"],
        "must_be_registered": True,
    }
    injections.append(("S1 文件不存在", claims, None))

    claims = dict(WATCHED)
    claims["control_median_mc_test.cpp"] = dict(
        WATCHED["control_median_mc_test.cpp"], must_be_registered=True)
    injections.append(("S2 存在但未注册", claims, None))

    claims = dict(WATCHED)
    claims["synthetic_gate.cpp"] = dict(
        WATCHED["synthetic_gate.cpp"], must_be_registered=False)
    injections.append(("S3 已注册却标 MISSING", claims, None))

    injections.append((
        "S4 文档现在时证据行",
        dict(WATCHED),
        {"docs/science/PHASE2_UPM.md": [
            "- §11 Oracle 全过（含 control_median_mc_test MC 一致性）"]},
    ))

    for label, claims, extra in injections:
        rc, rep = check_all(root, claims=claims, extra_doc_lines=extra,
                            tracked=tracked)
        if rc == 0:
            failures.append("%s: 注入未被判红（门失效）" % label)
        else:
            print("[self-test] %s: RED ok (%d 条)" % (label, len(rep["problems"])))
    if base_rc != 0:
        print("[self-test] 注意：当前工作树本身为红（常规运行会给出问题清单）")
    if failures:
        for f in failures:
            print("SELF_TEST_FAIL: " + f, file=sys.stderr)
        return 1
    print("[self-test] 4/4 注入均判红 ⇒ 门能红")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--self-test", "--fault-inject", dest="self_test",
                    action="store_true")
    ap.add_argument("--json", default="")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    if args.self_test:
        return self_test(root)
    rc, report = check_all(root)
    for name, st in sorted(report["status"].items()):
        print("[evidence] %s: exists=%s tracked=%s cmake=%s ci=%s registered=%s"
              % (name, st["exists"], st["tracked"], st["cmake"], st["ci"],
                 st["registered"]))
    for p in report["problems"]:
        print("SCI_EVIDENCE_REGISTRATION_FAIL: " + p)
    if args.json:
        os.makedirs(os.path.dirname(os.path.abspath(args.json)), exist_ok=True)
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(report, fh, ensure_ascii=False, indent=2, sort_keys=True)
    if rc == 0:
        print("SCI_EVIDENCE_REGISTRATION_OK")
    return rc


class TestSciEvidenceRegistration(unittest.TestCase):
    """R11 合规包装（CI-003，2026-09-16）。

    `eng/ci/validate_registry.py` R11：每个「直跑验收脚本」（模块级 `def main` +
    `sys.exit(main())`）必须贡献 >=1 个 `TestCase.test_*` 方法，否则 UT-QUALITY 的
    `unittest discover` 采集 0 用例、本门**在 CI 内永不执行**（账面记 OK）。
    本包装只调用既有判据，不改任何语义。
    """

    def test_sci_evidence_registration_clean(self):
        rc, report = check_all(os.getcwd())
        self.assertEqual(rc, 0,
                         "SCI_EVIDENCE_REGISTRATION_FAIL: " + "; ".join(report["problems"]))


if __name__ == "__main__":
    # 双入口（CI-003 R11 包装 + ctest 直跑）：
    #   - ctest 传 --root/--self-test/--json => 走 argparse 门（返回真实 rc）；
    #   - 裸跑 / unittest discover => 走 TestCase（R11 要求 >=1 test_* 方法）。
    _cli = any(a == "--root" or a.startswith("--root=") or a == "--self-test"
               or a == "--fault-inject" or a == "--json"
               for a in sys.argv[1:])
    if _cli:
        sys.exit(main())
    unittest.main(verbosity=2)
