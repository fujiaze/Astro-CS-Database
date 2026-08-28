#!/usr/bin/env python3
"""ARCH-002 测试: V5 单一 CLI 架构机器门。"""
import os, re, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ARCH = os.path.join(REPO, "docs", "architecture", "ARCHITECTURE.md")
MODULE = os.path.join(REPO, "docs", "architecture", "MODULE_MAP.md")
INV = os.path.join(REPO, "docs", "architecture", "PRODUCTION_EXECUTION_INVENTORY.csv")

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
            self.assertIn(old, s, f"旧目标 {old} 必须显式列出迁移处置")

    def test_04_inventory_agrees(self):
        import csv
        rows = list(csv.DictReader(open(INV, encoding="utf-8")))
        prod = [r for r in rows if r["category"] == "exe_target" and r["classification"] == "production"]
        self.assertEqual(len(prod), 0, "单一 CLI 未建前清单不得含 production exe")

    def test_05_three_phases_in_process(self):
        s = open(ARCH, encoding="utf-8").read()
        for k in ("Phase1", "Phase2", "Phase3"):
            self.assertIn(k, s)
        self.assertIn("in-process", s, "三 Phase 必须声明 CLI 进程内调用")

if __name__ == "__main__":
    unittest.main(verbosity=2)
