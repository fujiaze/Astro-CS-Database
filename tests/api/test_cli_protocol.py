#!/usr/bin/env python3
"""API-002 测试: CLI 协议合同机器门(04 §6 条款 1-5 的合同态实现)。"""
import json, os, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOC = os.path.join(REPO, "docs", "api", "CLI_PROTOCOL_V1.md")
LEDGER_04 = os.path.join(REPO, "工程控制", "RELEASE_V5",
                         "AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828",
                         "04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md")

class TestCliProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s = open(DOC, encoding="utf-8").read()
        cls.s04 = open(LEDGER_04, encoding="utf-8").read()

    def test_01_command_tree_covers_04(self):
        for cmd in ("--version", "hardware inspect", "config init", "config validate",
                    "config show-effective", "benchmark cpu", "doctor", "test synthetic",
                    "phase1 run", "phase2 run", "phase3 run", "run --phases", "verify --run-manifest"):
            self.assertIn(cmd, self.s, f"缺命令 {cmd}")
        self.assertIn("禁另发 benchmark exe", self.s)

    def test_02_exit_codes_complete(self):
        for c in (" 0 ", " 2 ", " 3 ", " 4 ", " 5 ", " 6 ", " 7 ", " 8 ", " 9 ", " 10 ", " 70 "):
            self.assertIn(c, self.s, f"缺退出码 {c.strip()}")
        self.assertIn("exit_codes.h", self.s, "退出码必须声明唯一源")

    def test_03_jsonl_fields_frozen(self):
        for f in ("schema_version", "event_id", "run_id", "timestamp_utc", "sequence",
                  "kind", "severity", "phase", "stage", "message",
                  "cpu_cores_used", "rss_bytes", "sha256", "backend_id", "eta_seconds"):
            self.assertIn(f, self.s)
        self.assertIn("从 0 单调递增", self.s)

    def test_04_cancel_crash_semantics(self):
        self.assertIn("acs_cancel", self.s)
        self.assertIn("exit 9", self.s)
        self.assertIn("不得留下看似完整", self.s)
        self.assertIn("70", self.s)
        self.assertIn("脱敏 crash report", self.s)

    def test_05_checker_contract_six_items(self):
        for i, k in enumerate(("help", "schema", "唯一源", "追溯", "旧 Phase exe", "双平台 golden"), 1):
            self.assertIn(k, self.s, f"04 §6-{i} 缺 {k}")

    def test_06_04_is_authoritative_reference(self):
        self.assertIn("以 04 为准", self.s)
        for cmd in ("astrocs --version --json", "astrocs doctor --json"):
            self.assertIn(cmd, self.s04, "04 权威命令存在性交叉核对")

if __name__ == "__main__":
    unittest.main(verbosity=2)
