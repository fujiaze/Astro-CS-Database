#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ARCH-002 测试: 单一用户入口架构机器门。

一页纸 S1 整改（本文件，独立审查第 26 条）
  * 原 test_04 以「清单里 production exe 数 == 0」为通过条件 —— 与命题（每平台恰好
    一个用户入口）**方向相反**；五条用例又只对文档文本做正则、零查构建面 ⇒ 入口唯一
    性没有实现判据（与第 22 条同型：恒真式当证据）。
  * 现把不变量的判据改为对**被检对象**求值、方向 = 恰一：根 CMake 构建图（可执行目标、
    生产闭包、登记的生产入口）、安装规则 eng/cmake/install_layout.cmake（被安装的可执行
    目标）、安装清单 eng/packaging/install-tree.contract.json（kind=exe 单元）、登记面
    PRODUCTION_EXECUTION_INVENTORY.csv（production 行恰一）。
  * 文档文本用例保留（它们查的是文档表述唯一性），但它们不再是入口唯一性的判据：
    判据本体 arch_invariants.single_entry_findings 完全不读文档文字。
"""
import os
import pathlib
import re
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arch_invariants as ai  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
ARCH = os.path.join(REPO, "docs", "architecture", "ARCHITECTURE.md")
MODULE = os.path.join(REPO, "docs", "architecture", "MODULE_MAP.md")


class TestSingleCLI(unittest.TestCase):
    def test_01_single_entry_statement(self):
        s = open(ARCH, encoding="utf-8").read()
        hits = re.findall(r"唯一(?:生产)?(?:用户)?入口", s)
        self.assertGreaterEqual(len(hits), 1, "必须有唯一入口声明")
        for banned in ("正式运行入口只有 orchestrator.exe", "astrocs-stage2（Phase2）入口"):
            self.assertNotIn(banned, s, "旧双入口表述必须清除")

    def test_02_no_second_entry_in_map(self):
        s = open(MODULE, encoding="utf-8").read()
        self.assertNotIn("orchestrator.exe", s) if os.path.exists(MODULE) else None

    def test_03_migration_section_exists(self):
        s = open(ARCH, encoding="utf-8").read()
        self.assertIn("迁移", s, "必须声明旧 exe 迁移")
        for old in ("orchestrator.exe", "astrocs-stage2", "healpix_browser_qt"):
            self.assertIn(old, s, "旧目标 %s 必须显式列出迁移处置" % old)

    def test_04_exactly_one_user_entry_on_the_object(self):
        """命题（方向 = 恰一）：每平台恰好一个用户入口 = 登记的生产入口。

        判据从构建图 + 安装规则 + 安装清单 + 登记面实算（arch_invariants），
        不是从本文件的清单里数「production exe == 0」。
        """
        findings = ai.single_entry_findings(REPO)
        self.assertEqual(findings, [],
                         "单一用户入口不变量被违反（判词逐条列全）：" + chr(10)
                         + chr(10).join(findings))

    def test_05_three_phases_in_process(self):
        s = open(ARCH, encoding="utf-8").read()
        for k in ("Phase1", "Phase2", "Phase3"):
            self.assertIn(k, s)
        self.assertIn("in-process", s, "三 Phase 必须声明 CLI 进程内调用")

    def test_06_negative_injection(self):
        """负例注入：把**被检对象**改成违反命题 ⇒ 必红；合规夹具不得误伤。"""
        with tempfile.TemporaryDirectory() as td:
            base = pathlib.Path(td)
            clean = ai.build_fixture(base / "clean")
            self.assertEqual(ai.single_entry_findings(clean), [],
                             "保护性正例：合规夹具被误伤 = 假门")

            second = ai.build_fixture(base / "second", extra_exe="acsd2",
                                      install_extra_exe=True)
            got = ai.single_entry_findings(second)
            self.assertTrue(any(f.startswith("E3") for f in got),
                            "负例「安装规则里多出第二个可执行入口」必须判红：" + str(got))

            contract = ai.build_fixture(base / "contract", extra_exe="acsd2",
                                        contract_extra_exe=True)
            got = ai.single_entry_findings(contract)
            self.assertTrue(any(f.startswith("E4") for f in got),
                            "负例「安装清单多出一个 exe 单元」必须判红：" + str(got))

            two_rows = ai.build_fixture(base / "two_rows", extra_exe="acsd2",
                                        csv_exes=[("acsd", "production"),
                                                  ("acsd2", "production")])
            got = ai.single_entry_findings(two_rows)
            self.assertTrue(any(f.startswith("R3") for f in got),
                            "负例「登记面两条 production 行」必须判红：" + str(got))

            zero = ai.build_fixture(base / "zero_prod",
                                    csv_exes=[("acsd", "tool")])
            got = ai.single_entry_findings(zero)
            self.assertTrue(any(f.startswith("R3") for f in got),
                            "负例「production exe 数 == 0」必须判红（旧断言把它当通过）："
                            + str(got))

            no_entry = ai.build_fixture(base / "no_entry", entry="acsd",
                                        graph_entry="not_acsd",
                                        csv_exes=[("not_acsd", "production")])
            got = ai.single_entry_findings(no_entry)
            self.assertTrue(any(f.startswith("E1") or f.startswith("FAIL-CLOSED")
                                for f in got),
                            "负例「生产入口不在构建图」必须判红"
                            "（入口不在图 ⇒ fail-closed，不是判绿）：" + str(got))

    def test_07_object_judgement_ignores_doc_text(self):
        """判据读对象、不读文档文字：夹具里根本没有 ARCHITECTURE.md 也照样判绿。"""
        with tempfile.TemporaryDirectory() as td:
            clean = ai.build_fixture(pathlib.Path(td) / "clean")
            self.assertFalse((clean / "docs/architecture/ARCHITECTURE.md").exists())
            self.assertEqual(ai.single_entry_findings(clean), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
