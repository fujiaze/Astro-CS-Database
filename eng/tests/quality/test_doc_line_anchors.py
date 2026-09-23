#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-ANCHOR-001 / DOC-DRIFT-FIX-01：文档源码行号锚复测器
（docs/algorithms/anchors/check_doc_line_anchors.py）的机器验收测试。

结构（先红后绿 + 负向注入必败）：
  1) 基线：真实仓库全量锚复测必须 rc=0 / verdict=PASS；
  2) 合成最小仓库 fixture：逐条注入漂移/破坏，检查器必须 rc!=0 且给出对应规则码；
     注入移除后必须回到 rc=0（证明失败由注入引起，非恒真装饰）；
  3) 确定性：同 cwd 双跑 + 跨 cwd 跑的 JSON 输出逐字节相同（无时间戳/无并发，
     1/N worker parity 不适用——检查器为单进程串行）。

DOC-DRIFT-FIX-01 新增覆盖：C6 边界空行（start/end 各一例）、C7 规模声明漂移/缺失、
C8 未解析登记（登记即绿 / 新增未登记即红 / 登记项陈旧即红 / 棘轮越界即红）。
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

BT = chr(96)

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
CHECKER = os.path.join(REPO, "docs", "algorithms", "anchors", "check_doc_line_anchors.py")
CONTRACT_REL = os.path.join("docs", "algorithms", "anchors", "anchor_contract.json")
REGISTRY_REL = os.path.join("docs", "algorithms", "anchors", "unresolved_registry.json")
CONTRACT_DOC_REL = os.path.join("docs", "algorithms", "anchors", "ANCHOR_CONTRACT.md")

DOC = "docs/algorithms/SAMPLE.md"
SRC = "lib/sample/sample_impl.cpp"
OTHER = "lib/sample/other/sample_impl.cpp"
GHOST = "lib/sample/ghost.cpp"

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

REGISTRY = {
    "schema": "astrocs/doc-anchor-unresolved-registry/v1",
    "purpose": "synthetic",
    "max_entries": 0,
    "entries": [],
}

# C7 规范句式：数字必须与 fixture 实测逐项相等
SCALE_LINE = ("现行规模（C7 逐字复测）：**%d 文档 / %d 锚** = "
              "%d 目标锚 + %d 登记豁免 + %d 未解析登记。")


def contract_doc(docs=1, anchors=2, targets=2, exempt=0, unresolved=0):
    return "# SYNTH ANCHOR CONTRACT\n\n" + SCALE_LINE % (docs, anchors, targets, exempt,
                                                         unresolved) + "\n"


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


def git_init(root):
    """fixture 必须是真 git 仓：检查器对「git 不可用」是 fail-closed（C1 判红）。"""
    env = dict(os.environ)
    env.update({"GIT_AUTHOR_NAME": "selftest", "GIT_AUTHOR_EMAIL": "selftest@local",
                "GIT_COMMITTER_NAME": "selftest", "GIT_COMMITTER_EMAIL": "selftest@local"})
    for cmd in (["git", "init", "-q"], ["git", "add", "-A"],
                ["git", "commit", "-q", "-m", "fixture"]):
        subprocess.run(cmd, cwd=root, check=True, capture_output=True, env=env)


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="sci_anchor_001_")
        write(os.path.join(self.tmp, CONTRACT_REL), json.dumps(CONTRACT, ensure_ascii=False))
        write(os.path.join(self.tmp, REGISTRY_REL), json.dumps(REGISTRY, ensure_ascii=False))
        write(os.path.join(self.tmp, CONTRACT_DOC_REL), contract_doc())
        write(os.path.join(self.tmp, DOC), "\n".join(DOC_BODY) + "\n")
        write(os.path.join(self.tmp, SRC), "\n".join(SRC_BODY) + "\n")
        git_init(self.tmp)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def set_registry(self, entries, max_entries=None):
        reg = dict(REGISTRY)
        reg["entries"] = entries
        reg["max_entries"] = len(entries) if max_entries is None else max_entries
        write(os.path.join(self.tmp, REGISTRY_REL), json.dumps(reg, ensure_ascii=False))

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

    def test_t04_print_scale_matches_measurement(self):
        proc = subprocess.run([sys.executable, CHECKER, "--root", self.tmp, "--print-scale"],
                              capture_output=True, text=True, timeout=300, cwd=REPO)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn(SCALE_LINE % (1, 2, 2, 0, 0), proc.stdout)
        with open(os.path.join(self.tmp, CONTRACT_DOC_REL), encoding="utf-8") as fh:
            self.assertIn(proc.stdout.strip(), fh.read())


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
              "\n".join(DOC_BODY) + "ghost: " + BT + GHOST + ":3" + BT + "\n")
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
        write(os.path.join(self.tmp, CONTRACT_DOC_REL), contract_doc(anchors=3, targets=3))
        self.assert_clean()

    def test_f09_stale_exemption(self):
        contract = json.loads(json.dumps(CONTRACT))
        contract["exemptions"] = [{"doc": DOC, "raw": "nosuch.cpp:1",
                                   "reason": "test", "kind": "TEST"}]
        write(os.path.join(self.tmp, CONTRACT_REL), json.dumps(contract, ensure_ascii=False))
        self.assert_red("C5_exemptions_live", "stale exemption")

    def test_f10_exemption_covers_bad_anchor(self):
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "ghost: " + BT + GHOST + ":3" + BT + "\n")
        contract = json.loads(json.dumps(CONTRACT))
        contract["exemptions"] = [{"doc": DOC, "raw": GHOST + ":3",
                                   "reason": "synthetic exemption", "kind": "TEST"}]
        write(os.path.join(self.tmp, CONTRACT_REL), json.dumps(contract, ensure_ascii=False))
        write(os.path.join(self.tmp, CONTRACT_DOC_REL),
              contract_doc(anchors=3, targets=2, exempt=1))
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


class TestDocDriftFix01(Base):
    """C6/C7/C8：DOC-DRIFT-FIX-01 新增判据的负例注入与恢复。"""

    def test_g01_boundary_blank_start(self):
        # SRC 第 5 行是空行（SRC_BODY[4]）
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "blank: " + BT + SRC + ":5" + BT + "\n")
        self.assert_red("C6_boundary_blank", "anchor start on blank line")

    def test_g02_boundary_blank_end(self):
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "blankend: " + BT + SRC + ":4-5" + BT + "\n")
        self.assert_red("C6_boundary_blank", "anchor end on blank line")

    def test_g03_boundary_blank_red_then_green(self):
        """注入 ⇒ 红；还原 ⇒ 绿（证明失败由注入引起）。"""
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "blank: " + BT + SRC + ":5" + BT + "\n")
        self.assert_red("C6_boundary_blank", "blank start")
        write(os.path.join(self.tmp, DOC), "\n".join(DOC_BODY) + "\n")
        self.assert_clean()

    def test_g04_scale_claim_drift(self):
        write(os.path.join(self.tmp, CONTRACT_DOC_REL), contract_doc(anchors=3))
        self.assert_red("C7_contract_scale", "scale claim drift")

    def test_g05_scale_claim_missing(self):
        write(os.path.join(self.tmp, CONTRACT_DOC_REL), "# SYNTH ANCHOR CONTRACT\n\n无规模声明\n")
        self.assert_red("C7_contract_scale", "scale claim removed")

    def test_g06_scale_claim_red_then_green(self):
        write(os.path.join(self.tmp, CONTRACT_DOC_REL), contract_doc(docs=2))
        self.assert_red("C7_contract_scale", "doc count drift")
        write(os.path.join(self.tmp, CONTRACT_DOC_REL), contract_doc())
        self.assert_clean()

    def test_g07_unregistered_unresolved_anchor(self):
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "ghost: " + BT + GHOST + ":3" + BT + "\n")
        self.assert_red("C2_anchor_resolved", "unresolved anchor not registered")

    def test_g08_registered_unresolved_anchor_is_green(self):
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "ghost: " + BT + GHOST + ":3" + BT + "\n")
        self.set_registry([{"doc": DOC, "raw": GHOST + ":3", "kind": "TEST",
                            "reason": "synthetic", "owner": "selftest"}])
        write(os.path.join(self.tmp, CONTRACT_DOC_REL),
              contract_doc(anchors=3, targets=2, unresolved=1))
        self.assert_clean()

    def test_g09_stale_registry_entry(self):
        self.set_registry([{"doc": DOC, "raw": "nosuch.cpp:1", "kind": "TEST",
                            "reason": "synthetic", "owner": "selftest"}])
        self.assert_red("C8_registry_stale", "registry entry no longer matched")

    def test_g10_registry_ratchet(self):
        write(os.path.join(self.tmp, DOC),
              "\n".join(DOC_BODY) + "ghost: " + BT + GHOST + ":3" + BT + "\n")
        self.set_registry([{"doc": DOC, "raw": GHOST + ":3", "kind": "TEST",
                            "reason": "synthetic", "owner": "selftest"}], max_entries=0)
        write(os.path.join(self.tmp, CONTRACT_DOC_REL),
              contract_doc(anchors=3, targets=2, unresolved=1))
        self.assert_red("C8_registry_ratchet", "registry over max_entries")

    def test_g11_registry_file_missing_is_fail_closed(self):
        os.remove(os.path.join(self.tmp, REGISTRY_REL))
        self.assert_red("C0_contract_exists", "registry missing => fail-closed")


if __name__ == "__main__":
    unittest.main()
