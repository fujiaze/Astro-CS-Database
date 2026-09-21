#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-ANCHOR-001：ALG/SCI 文档源码行号锚复测器（docs/algorithms/anchors/
check_doc_line_anchors.py）的机器验收测试。

结构（先红后绿 + 负向注入必败）：
  1) 基线：真实仓库全量锚复测必须 rc=0 / verdict=PASS（784 OK + 9 EXEMPT）；
  2) 合成最小仓库 fixture：逐条注入漂移/破坏，检查器必须 rc!=0 且给出对应规则码；
     注入移除后必须回到 rc=0（证明失败由注入引起，非恒真装饰）；
  3) 确定性：同 cwd 双跑 + 跨 cwd 跑的 JSON 输出逐字节相同（无时间戳/无并发，
     1/N worker parity 不适用——检查器为单进程串行）。
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

BT = chr(96)

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CHECKER = os.path.join(REPO, "docs", "algorithms", "anchors", "check_doc_line_anchors.py")
CONTRACT_REL = os.path.join("docs", "algorithms", "anchors", "anchor_contract.json")

DOC = "docs/algorithms/SAMPLE.md"
SRC = "lib/sample/sample_impl.cpp"
OTHER = "lib/sample/other/sample_impl.cpp"

SRC_BODY = [
    "// synthetic source for SCI-ANCHOR-001 tests",
    "int sample_kernel(int x) {",
    "    return x + 1;",
    "}",
    "",
    "int sample_helper(int x) {",
    "    return x - 1;",
    "}",
]

DOC_BODY = [
    "# synthetic ALG doc",
    "",
    "kernel: " + BT + SRC + ":2" + BT,
    "helper: " + BT + SRC + ":6" + BT,
    "",
]

CONTRACT = {
    "schema": "astrocs/doc-line-anchor-contract/v1",
    "doc_globs": ["docs/algorithms/*.md"],
    "resolvers": [],
    "exemptions": [],
    "bindings": [
        {"id": "SAMPLE-KERNEL", "doc": DOC, "target": SRC, "symbol": "sample_kernel"},
    ],
}


def run_checker(root, json_out=None, cwd=None):
    cmd = [sys.executable, CHECKER, "--root", root]
    if json_out:
        cmd += ["--json-out", json_out]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=300, cwd=cwd or REPO)
    return proc.returncode, proc.stdout, proc.stderr


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="sci_anchor_001_")
        write(os.path.join(self.tmp, CONTRACT_REL), json.dumps(CONTRACT, ensure_ascii=False))
        write(os.path.join(self.tmp, DOC), "\n".join(DOC_BODY) + "\n")
        write(os.path.join(self.tmp, SRC), "\n".join(SRC_BODY) + "\n")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def assert_clean(self):
        rc, out, err = run_checker(self.tmp)
        self.assertEqual(rc, 0, "baseline fixture must pass:\n%s\n%s" % (out, err))
        self.assertIn("DOC_LINE_ANCHORS_PASS", out)

    def assert_red(self, code, label):
        rc, out, err = run_checker(self.tmp)
        self.assertNotEqual(rc, 0, "%s: checker must fail (negative injection)" % label)
        self.assertIn(code, out + err, "%s: expected %s in output" % (label, code))


class TestBaseline(Base):
    def test_t01_real_repo_all_anchors_green(self):
        rc, out, err = run_checker(REPO)
        self.assertEqual(rc, 0, out + err)
        self.assertIn("DOC_LINE_ANCHORS_PASS", out)
        self.assertIn("anchors", out)

    def test_t02_fixture_green(self):
        self.assert_clean()

    def test_t03_deterministic_bitwise_same_and_cross_cwd(self):
        o1 = os.path.join(self.tmp, "r1.json")
        o2 = os.path.join(self.tmp, "r2.json")
        o3 = os.path.join(self.tmp, "r3.json")
        self.assertEqual(run_checker(self.tmp, o1)[0], 0)
        self.assertEqual(run_checker(self.tmp, o2)[0], 0)
        self.assertEqual(run_checker(self.tmp, o3, cwd=self.tmp)[0], 0)
        with open(o1, "rb") as fh:
            b1 = fh.read()
        with open(o2, "rb") as fh:
            b2 = fh.read()
        with open(o3, "rb") as fh:
            b3 = fh.read()
        self.assertEqual(b1, b2, "same-cwd reruns must be bitwise identical")
        self.assertEqual(b1, b3, "cross-cwd run must be bitwise identical")


class TestFaultInjection(Base):
    def test_f01_target_file_deleted(self):
        os.remove(os.path.join(self.tmp, SRC))
        self.assert_red("C2_anchor_resolved", "delete target file")

    def test_f02_target_truncated_out_of_range(self):
        write(os.path.join(self.tmp, SRC), "int sample_kernel(int x) {\n")
        self.assert_red("C3_range_in_bounds", "truncate target")

    def test_f03_symbol_moved_out_of_range(self):
        body = ["// padding", "// padding"] + SRC_BODY
        write(os.path.join(self.tmp, SRC), "\n".join(body) + "\n")
        self.assert_red("C4_symbol_binding", "drift symbol out of anchored range")

    def test_f04_symbol_removed_entirely(self):
        write(os.path.join(self.tmp, SRC),
              "\n".join(l.replace("sample_kernel", "renamed_kernel") for l in SRC_BODY) + "\n")
        self.assert_red("C4_symbol_binding", "stale binding symbol absent")

    def test_f05_anchor_to_missing_file(self):
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "ghost: " + BT + "lib/sample/ghost.cpp:3" + BT + "\n")
        self.assert_red("C2_anchor_resolved", "anchor to nonexistent file")

    def test_f06_anchor_out_of_bounds(self):
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "far: " + BT + SRC + ":900" + BT + "\n")
        self.assert_red("C3_range_in_bounds", "anchor beyond EOF")

    def test_f07_ambiguous_basename_without_resolver(self):
        write(os.path.join(self.tmp, OTHER), "\n".join(SRC_BODY) + "\n")
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "bare: " + BT + "sample_impl.cpp:2" + BT + "\n")
        self.assert_red("C2_anchor_resolved", "ambiguous basename")

    def test_f08_ambiguous_basename_resolved_by_rule(self):
        write(os.path.join(self.tmp, OTHER), "\n".join(SRC_BODY) + "\n")
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "bare: " + BT + "sample_impl.cpp:2" + BT + "\n")
        contract = json.loads(json.dumps(CONTRACT))
        contract["resolvers"] = [{"doc": DOC, "basename": "sample_impl.cpp", "path": SRC}]
        write(os.path.join(self.tmp, CONTRACT_REL), json.dumps(contract, ensure_ascii=False))
        self.assert_clean()

    def test_f09_stale_exemption(self):
        contract = json.loads(json.dumps(CONTRACT))
        contract["exemptions"] = [{"doc": DOC, "raw": "nosuch.cpp:1",
                                   "reason": "test", "kind": "TEST"}]
        write(os.path.join(self.tmp, CONTRACT_REL), json.dumps(contract, ensure_ascii=False))
        self.assert_red("C5_exemptions_live", "stale exemption")

    def test_f10_exemption_covers_bad_anchor(self):
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "ghost: " + BT + "lib/sample/ghost.cpp:3" + BT + "\n")
        contract = json.loads(json.dumps(CONTRACT))
        contract["exemptions"] = [{"doc": DOC, "raw": "lib/sample/ghost.cpp:3",
                                   "reason": "synthetic exemption", "kind": "TEST"}]
        write(os.path.join(self.tmp, CONTRACT_REL), json.dumps(contract, ensure_ascii=False))
        self.assert_clean()

    def test_f11_restore_after_each_injection(self):
        """每次注入后还原，检查器必须回到绿——失败由注入引起，非恒真。"""
        cases = []
        os.remove(os.path.join(self.tmp, SRC)); cases.append("delete")
        self.assert_red("C2_anchor_resolved", "delete")
        write(os.path.join(self.tmp, SRC), "\n".join(SRC_BODY) + "\n")
        self.assert_clean()
        write(os.path.join(self.tmp, SRC), "int sample_kernel(int x) {\n")
        cases.append("truncate")
        self.assert_red("C3_range_in_bounds", "truncate")
        write(os.path.join(self.tmp, SRC), "\n".join(SRC_BODY) + "\n")
        self.assert_clean()
        self.assertEqual(cases, ["delete", "truncate"])


if __name__ == "__main__":
    unittest.main()
