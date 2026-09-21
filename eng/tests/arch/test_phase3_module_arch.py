#!/usr/bin/env python3
"""ARCH-005 测试: Phase3 模块架构逐 claim 追溯 + 科学选择不在 cache/loader 机器门。"""
import os, re, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOC = os.path.join(REPO, "docs", "architecture", "PHASE3_MODULE_ARCH.md")

class TestPhase3Arch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s = open(DOC, encoding="utf-8").read()

    def test_01_four_units_and_direction(self):
        for u in ("HiPSReader", "TileCache", "Resampler", "FitsWriter"):
            self.assertIn(u, self.s)

    def test_02_no_science_in_cache(self):
        self.assertIn("cache 不做任何插值/加权/order 决策", self.s)
        self.assertIn("cache 永不伪造数据", self.s)

    def test_03_memory_bound_frozen(self):
        self.assertIn("M ≤", self.s) and self.assertIn("max_tiles", self.s)
        self.assertIn("rc=MEM_BUDGET", self.s, "超预算必须显式错误码")

    def test_04_concurrency_follows_budget(self):
        self.assertIn("禁硬编码", self.s) and self.assertIn("host budget", self.s)
        self.assertIn("取消时 FitsWriter 不发生", self.s)

    def test_05_traceability_table(self):
        for a in ("ALG-P3-001", "ALG-P3-002", "ALG-P3-003", "ALG-P3-004"):
            self.assertIn(a, self.s, f"缺 {a} 追溯锚")

    def test_06_no_silent_degrade(self):
        self.assertIn("禁静默降级", self.s)
        self.assertIn("不进入半成品 run", self.s)

if __name__ == "__main__":
    unittest.main(verbosity=2)
