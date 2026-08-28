#!/usr/bin/env python3
"""ARCH-001 测试: PRODUCTION_EXECUTION_INVENTORY 机器门。"""
import csv, os, subprocess, sys, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INV = os.path.join(REPO, "docs", "architecture", "PRODUCTION_EXECUTION_INVENTORY.csv")
GEN = os.path.join(REPO, "tools", "arch", "build_production_execution_inventory.py")
CATS = {"exe_target", "openmp_kernel", "thread_creation", "lock", "queue", "acr_boundary", "io_writer"}

def rows():
    with open(INV, encoding="utf-8") as f:
        return list(csv.DictReader(f))

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

    def test_04_single_exe_strategy(self):
        prod = [r for r in rows() if r["category"] == "exe_target" and r["classification"] == "production"]
        self.assertEqual(len(prod), 0, "astrocs CLI 未建立前不得有 production exe(CLI-001 建立)")

    def test_05_regeneration_idempotent(self):
        before = open(INV, "rb").read()
        subprocess.run([sys.executable, GEN], cwd=REPO, check=True, timeout=300)
        self.assertEqual(before, open(INV, "rb").read(), "生成器必须幂等(逐字节一致)")

if __name__ == "__main__":
    unittest.main(verbosity=2)
