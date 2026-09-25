#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ARCH-001 测试: PRODUCTION_EXECUTION_INVENTORY 机器门。

一页纸 S1 整改（本文件，独立审查第 22／29 条）
  * 第 22 条：test_05 原先「先读跟踪件字节、再在原地重跑生成器覆写它」⇒ 真实差异在
    第一跑即被抹掉、第二跑必然通过。现改为把生成器写到**临时目录**再与跟踪件比对，
    被校对象只读：跑完 git status 必须干净（UT-ARCH 的 mutates_workspace 登记随之
    不再有任何写面）。
  * 第 29 条：test_04 原先断言「清单里 production exe 数 == 0」—— 生产入口 acsd 在
    登记面零登记，而这个缺失本身被固化成了通过条件。现改为读**真实构建图**
    （eng/ci/cmake_graph.py），断言「登记集合 包含 构建产出的可执行目标集合」，
    且 production 行恰一 = 登记表里的生产入口。
判据本体在 arch_invariants.py（读构建图/安装面/登记面的纯函数 + 可注入违规的夹具）。
"""
import csv
import hashlib
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arch_invariants as ai  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
INV = os.path.join(REPO, "docs", "architecture", "PRODUCTION_EXECUTION_INVENTORY.csv")
GEN = os.path.join(REPO, "eng", "tools", "arch", "build_production_execution_inventory.py")
CATS = {"exe_target", "openmp_kernel", "thread_creation", "lock", "queue", "acr_boundary", "io_writer"}


def rows():
    with open(INV, encoding="utf-8") as f:
        return list(csv.DictReader(f))


def _sha(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


class TestInventory(unittest.TestCase):
    def test_01_categories_covered(self):
        got = {r["category"] for r in rows()}
        self.assertEqual(got, CATS, "七类原语必须全覆盖")

    def test_02_no_archive_or_third_party_evidence(self):
        for r in rows():
            self.assertNotIn("/archive/", r["evidence"], "archive 死代码不入清单")
            self.assertNotIn("third_party", r["evidence"], "第三方源不入自研清单")

    def test_03_acr_rows_are_boundary_only(self):
        for r in rows():
            if r["category"] == "acr_boundary":
                self.assertIn("配置守卫", r["thread_model"], "ACR 仅允许配置边界, 禁计算调用")
                self.assertTrue(r["risk_note"], "ACR 行必须带 V5 风险注记")

    def test_04_registration_covers_build_graph_executables(self):
        """命题（方向 = 包含）：登记面 ⊇ 构建产出的可执行目标集合。

        旧断言是 assertEqual(len(prod), 0)：生产入口 acsd（根 CMakeLists.txt:979）在
        登记面零登记，而这个缺失本身被当成了通过条件（一页纸 S1 第 29 条）。
        """
        findings = ai.registration_findings(REPO)
        self.assertEqual(findings, [],
                         "登记面与真实构建图不一致（判词逐条列全）：" + chr(10)
                         + chr(10).join(findings))

    def test_05_regeneration_idempotent_without_writing_tracked_file(self):
        """生成器写临时目录再比对；被校对象只读（首跑即判红，不是二跑恒绿）。"""
        before = _sha(INV)
        with tempfile.TemporaryDirectory() as td:
            out = os.path.join(td, "PRODUCTION_EXECUTION_INVENTORY.csv")
            proc = subprocess.run([sys.executable, GEN, "--out", out], cwd=REPO,
                                  capture_output=True, text=True, timeout=600)
            self.assertEqual(proc.returncode, 0,
                             "生成器非零退出：" + (proc.stderr or proc.stdout)[-2000:])
            with open(out, "rb") as f:
                produced = f.read()
        self.assertEqual(_sha(INV), before,
                         "被校对象必须只读：生成器不得覆写 %s（一页纸 S1 第 22 条）" % INV)
        with open(INV, "rb") as f:
            tracked = f.read()
        self.assertEqual(tracked, produced,
                         "跟踪件 != 生成器输出（登记面落后于树）：跑 "
                         "eng/tools/arch/build_production_execution_inventory.py 重新生成")

    def test_06_negative_injection(self):
        """负例注入（改前放过 / 改后判红）：命题被违反时必须判红，合规夹具不得误伤。"""
        with tempfile.TemporaryDirectory() as td:
            base = pathlib.Path(td)
            clean = ai.build_fixture(base / "clean")
            self.assertEqual(ai.registration_findings(clean), [],
                             "保护性正例：合规夹具被误伤 = 假门")

            dropped = ai.build_fixture(base / "dropped", csv_drop=("acsd",))
            got = ai.registration_findings(dropped)
            self.assertTrue(got, "负例「删掉登记面里的生产入口行」必须判红")
            self.assertTrue(any(f.startswith("R2") for f in got), "判词须指出未登记：" + str(got))

            unreg = ai.build_fixture(base / "unreg", extra_exe="new_entry",
                                     csv_exes=[("acsd", "production")])
            got = ai.registration_findings(unreg)
            self.assertTrue(any("new_entry" in f for f in got),
                            "负例「新增可执行目标而未登记」必须判红：" + str(got))

            zero = ai.build_fixture(base / "zero", csv_exes=[("acsd", "tool")])
            got = ai.registration_findings(zero)
            self.assertTrue(any(f.startswith("R3") for f in got),
                            "负例「production 行 0 条」必须判红（旧断言把它当通过）：" + str(got))

            dead = ai.build_fixture(base / "dead", csv_bad_anchor=True)
            got = ai.registration_findings(dead)
            self.assertTrue(any(f.startswith("R5") for f in got),
                            "负例「登记行的锚文件不存在」必须判红：" + str(got))

            empty = ai.build_fixture(base / "empty")
            os.remove(empty / ai.INVENTORY)
            got = ai.registration_findings(empty)
            self.assertTrue(got and got[0].startswith("FAIL-CLOSED"),
                            "锚点缺失必须 fail-closed 判红：" + str(got))

    def test_07_doc_text_is_not_the_object(self):
        """判据读的是被检对象：夹具里没有 docs/architecture/ARCHITECTURE.md，判据照样成立。"""
        with tempfile.TemporaryDirectory() as td:
            clean = ai.build_fixture(pathlib.Path(td) / "clean")
            self.assertFalse((clean / "docs/architecture/ARCHITECTURE.md").exists())
            self.assertEqual(ai.registration_findings(clean), [])
            self.assertEqual(ai.single_entry_findings(clean), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
