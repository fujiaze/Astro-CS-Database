#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UT-QUALITY：结论锚门的正/负例自检（一页纸 S1-2）。

挂在 UT-QUALITY（eng/ci/checks.json::UT-QUALITY 跑 eng/tests/quality 全量 discover），
因此本测试进门即在册；无需改 checks.json（那是前台冻结面）。

覆盖的正/负例（由被测门自己构造沙箱执行）：
  N1 registry 生成器：删掉证据源（module_adapters.cpp）⇒ 非零退出且不写盘；
  N1' 删掉授权面索引（DOCUMENT_INDEX.yaml）⇒ 非零退出；
  N2 cfitsio 清单生成器：删掉证据源目录 ⇒ 非零退出且不覆空在册清单；
  N3 无锚结论字面量 ⇒ 判红；写 NOT_VERIFIED 或补证据锚 ⇒ 转绿；
  N4 生成页正文被手改（body-sha256 不符）⇒ 判红；
  N5 产物脚本里结论字段写死为绿值 ⇒ 判红；标 # conclusion-anchor: ⇒ 转绿；
  N6 shell 里 sed 把 FAIL 改写成 PASS ⇒ 判红。
"""
import os
import subprocess
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))      # eng/tests/quality/x.py → 仓库根
GATE = os.path.join(REPO, "eng", "tools", "quality", "check_conclusion_anchors.py")


class TestConclusionAnchors(unittest.TestCase):
    def test_gate_exists(self):
        self.assertTrue(os.path.isfile(GATE), "结论锚门缺失: %s" % GATE)

    def test_self_test_positive_and_negative(self):
        """门自检必须正负例全过（含"删证据源⇒非零退出"）。"""
        r = subprocess.run([sys.executable, GATE, "--self-test"], cwd=REPO,
                           capture_output=True, text=True, timeout=600)
        self.assertEqual(r.returncode, 0,
                         "结论锚门自检失败:\n%s\n%s" % (r.stdout[-2000:], r.stderr[-2000:]))
        self.assertIn("CONCLUSION_ANCHORS_SELFTEST PASS", r.stdout)

    def test_delivered_tree_is_green(self):
        """现行树必须绿（有违规即红，不许把违规留成"已知"）。"""
        r = subprocess.run([sys.executable, GATE], cwd=REPO,
                           capture_output=True, text=True, timeout=600)
        self.assertEqual(r.returncode, 0,
                         "现行树存在无锚结论/锚漂移:\n%s\n%s"
                         % (r.stdout[-3000:], r.stderr[-2000:]))


if __name__ == "__main__":
    unittest.main()
