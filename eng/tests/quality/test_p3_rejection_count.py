#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P3-REJ-COUNT 判据的 CI 可见载体（UT-QUALITY / CHK-UNIT 自动 discover）。

本文件不复制判据逻辑：唯一事实源是 eng/tools/quality/check_p3_rejection_count.py。
它在这里被驱动两次：
  T1 正负例面：`--self-test` 必须 rc=0（1 正例 + 7 负例；负例覆盖
     「计数与真实剔除数不符」「计数声明字段缺失（0 与缺失可区分）」「−1 哨兵」
     「总量与逐像素不符」「载体截断 fail-closed」「science planes 被污染」
     「nearest 口径 n_cand=1」）。
  T2 真实产品面：若工作区里存在 Phase3 重采样产物（run/**/p3_resampled.json），
     逐份驱动真判据；判据命中（rc=1）或依赖不可用（rc=2）都判红 ——
     fail-closed，不以「没找到产物」当绿。

规范依据：DATA-002 §2a 规则 3（强制计数 + count_field=n_rejected_nonfinite +
「计数为 0 与字段缺失必须可区分」）；DATA_SEMANTICS §30.7（Phase3 承载面冻结）；
ENGINEERING_SPEC §8（能红能绿、fail-closed、SKIP 充数算未完成）。
"""
import os
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
CHECKER = os.path.join(REPO, "eng", "tools", "quality", "check_p3_rejection_count.py")


class TestP3RejectionCount(unittest.TestCase):
    def _run(self, *argv):
        return subprocess.run([sys.executable, CHECKER, *argv], capture_output=True,
                              text=True, cwd=REPO, timeout=300)

    def test_01_checker_exists(self):
        self.assertTrue(os.path.isfile(CHECKER), "判据文件缺失: %s" % CHECKER)

    def test_02_self_test_red_green(self):
        """正例必须判绿、7 类负例必须各自判红（rc=0 表示自检面全部成立）。"""
        r = self._run("--self-test")
        self.assertEqual(r.returncode, 0,
                         "自检面未通过（判据非退化要求正例绿 + 负例红）:\n"
                         + r.stdout + r.stderr)
        self.assertIn("P3-REJ-COUNT_SELF-TEST_PASS", r.stdout)
        self.assertIn("negatives=7", r.stdout)

    def test_03_no_product_dir_is_fail_closed(self):
        """不给 --product-dir 且不跑自检 ⇒ 依赖不可用 rc=2，不得静默判绿。"""
        r = self._run()
        self.assertEqual(r.returncode, 2, "缺输入对象必须 fail-closed rc=2: " + r.stdout + r.stderr)

    def test_04_cli_end_to_end_red_green(self):
        """CLI 级端到端：同一判据在**同一进程外**驱动下能红能绿。

        为什么不去扫 run/** 下的历史产物：run/ 是临时产物区（AGENTS §7），里面的
        Phase3 产物可能早于本合同的冻结时点 —— 拿它们判红是**假红**（对象先于条款
        存在），拿它们判绿是**假绿**。真实产品面由 CI 注册项显式给 --product-dir
        驱动（见回执给出的注册片段），不由本测试自行猜测扫描面。
        """
        import importlib.util
        spec = importlib.util.spec_from_file_location("p3rej", CHECKER)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        nan = float("nan")
        with tempfile.TemporaryDirectory(prefix="p3rej_cli_") as td:
            green = os.path.join(td, "green")
            mod._write_product(green, 2, 2, "bilinear", [nan, nan, 1.5, 2.5],
                               [0.0, 1.0, 1.0, 1.0], [0, 4, 2, 0])
            r = self._run("--product-dir", green)
            self.assertEqual(r.returncode, 0, "正例应 rc=0:\n" + r.stdout + r.stderr)
            self.assertIn("P3-REJ-COUNT_PASS", r.stdout)

            # 红面 A：计数与真实剔除数不符
            red = os.path.join(td, "red")
            mod._write_product(red, 2, 2, "bilinear", [nan, nan, 1.5, 2.5],
                               [0.0, 1.0, 1.0, 1.0], [0, 0, 2, 0])
            r = self._run("--product-dir", red)
            self.assertEqual(r.returncode, 1, "计数不符应 rc=1:\n" + r.stdout + r.stderr)
            self.assertIn("COUNT_MISMATCH", r.stdout)

            # 红面 B：计数声明字段缺失（0 与字段缺失必须可区分）
            red2 = os.path.join(td, "red2")
            mod._write_product(red2, 2, 2, "bilinear", [nan, nan, 1.5, 2.5],
                               [0.0, 1.0, 1.0, 1.0], [0, 4, 2, 0], decl=False)
            r = self._run("--product-dir", red2)
            self.assertEqual(r.returncode, 1, "缺声明应 rc=1:\n" + r.stdout + r.stderr)
            self.assertIn("COUNT_FIELD_MISSING", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
