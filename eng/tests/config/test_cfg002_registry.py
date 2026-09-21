#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CFG-002（W5-CFG-002）登记类门的 unittest 包装。

跑法（与 CFG-001 同目录，一次发现全跑）：
  python3 -m unittest discover -s eng/tests/config -t eng/tests/config
本文件只调用同目录 check_cfg002_registry.py 的 10 项检查与 15 类故障注入，
不重复实现断言（单一事实源）；负例入口见测试 test_self_test_injections_all_red。
"""
import os
import unittest

import cfg_common as C
import check_cfg002_registry as K


class TestCfg002Registry(unittest.TestCase):
    """10 项检查在真实仓库必须全绿。"""

    def test_all_ten_checks_green(self):
        results = K.run_checks(C.REPO)
        fails = [r for r in results if not r["ok"]]
        self.assertEqual([], fails, "CFG002 检查未全绿：%s" % fails)

    def test_check_inventory_is_exactly_registered(self):
        """检查清单按名登记：新增/删改检查必须先改这里（防漏测）。"""
        expected = ["CFG002-%02d" % i for i in range(1, 12)]
        self.assertEqual(expected, [cid for cid, _ in K.CHECKS])

    def test_every_check_has_a_fault_injection(self):
        """ENGINEERING_SPEC §8：每项检查必须有可执行负例面（此处按检查项覆盖）。"""
        covered = {expect for _, expect, _ in K.INJECTIONS}
        self.assertEqual({cid for cid, _ in K.CHECKS}, covered)

    def test_self_test_injections_all_red(self):
        """负例注入：真实仓库全绿 + 15 类故障在沙箱中逐条必红（rc 语义）。"""
        ok, problems, _real = K.self_test(C.REPO, verbose=False)
        self.assertTrue(ok, "自检失败：%s" % problems)

    def test_os_abi_negative_fixture_rejected_by_enum(self):
        """6.6 值域冻结的负例：越出 {linux, windows} 必被 schema 拒。"""
        validator = K._validator(C.REPO)
        schema = C.load_json(K.CPU_SCHEMA)
        neg = C.load_json(K.CPU_NEG)
        self.assertNotIn(neg["host"]["os_abi"], K.load_json(C.REPO, K.CPU_SCHEMA)
                         ["$defs"]["profile_v2"]["properties"]["host"]["properties"]["os_abi"]["enum"])
        self.assertTrue(validator.validate(neg, schema), "越界 os_abi 必须被拒")
        pos = C.load_json("eng/tests/config/fixtures/positive/cpu_profile_v2.json")
        self.assertEqual([], validator.validate(pos, schema))

    def test_filter_name_rule_is_frozen_with_no_alias(self):
        """6.5 匹配语义：exact + 无别名（任何归一化/别名都必须显式登记）。"""
        look = C.load_json("eng/packaging/config/filters.json")["lookup"]
        self.assertEqual("exact", look["match"])
        self.assertIs(True, look["case_sensitive"])
        self.assertEqual("none", look["normalization"])
        self.assertEqual({}, look["aliases"])
        self.assertEqual("error", look["unknown_filter"])
        lits = {e["literal"] for e in look["non_key_examples"]}
        self.assertLessEqual({"bader r", "bader v"}, lits, "设计示例串必须登记为 non_key_examples")

    def test_registry_pointer_is_declared_from_defaults(self):
        """6.1/6.2 的唯一登记面：defaults.json 必须指向登记册（不得双份数值）。"""
        ref = C.load_json("eng/packaging/config/defaults.json")["registry_ref"]
        self.assertEqual("eng/packaging/config/config_registry.json", ref["path"])
        self.assertTrue(os.path.isfile(os.path.join(C.REPO, ref["path"])))


if __name__ == "__main__":
    unittest.main(verbosity=2)
