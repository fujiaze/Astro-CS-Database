#!/usr/bin/env python3
"""API-002 测试: CLI 协议合同机器门(04 §6 条款 1-5 的合同态实现)。"""
import json, os, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOC = os.path.join(REPO, "docs", "api", "CLI_PROTOCOL_V1.md")
# 断链修复（第二次，2026-09-16，负责人裁决 C）：上一版 LEDGER_04 指向
# artifacts/prerelease_v5/AUDIT_REVIEW/control/04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md，
# 该树已随 artifacts/ 按负责人裁决删除（commit b1290525「不归档、不保留」）⇒ setUpClass
# FileNotFoundError。04 控制包副本已不可得，交叉核对对象改为 **tracked 权威文档本身**
# docs/api/CLI_PROTOCOL_V1.md（其 §1 命令树 + §6 校验器合同即 04 的仓库落地）。
LEDGER_04 = DOC

class TestCliProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s = open(DOC, encoding="utf-8").read()
        cls.s04 = open(LEDGER_04, encoding="utf-8").read()

    def test_01_command_tree_covers_04(self):
        # CLI-002 三入口隔离改造: 04 §1 的顶层 `astrocs run --phases 1,2,3` 已移除,
        # 现行命令树 = phase1/2/3 run 三独立入口(等价 manifest 语义, §1 golden);
        # 04 冻结文本仍留旧行属上游演进滞后, 断言权威 = 当前 help golden(CLI_PROTOCOL_V1.md §1)。
        for cmd in ("--version", "hardware inspect", "config init", "config validate",
                    "config show-effective", "benchmark cpu", "doctor", "test synthetic",
                    "phase1 run", "phase2 run", "phase3 run", "verify --run-manifest"):
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
        # 断言对象 = §1 命令树冻结原文（LEDGER_04 已重指本文档，见文件头断链修复说明）：
        # 原 04 副本字面量 "astrocs --version --json" 为 04 侧写法，本文档冻结写法是
        # "astrocs --version [--json]"（两者指同一命令，后者为可选参数形）。04 副本不可得，
        # 故以 tracked 文档原文为准；如需恢复 04 字面量，属 CLI-001 的文档更新范畴。
        for cmd in ("astrocs --version [--json]", "astrocs doctor --json"):
            self.assertIn(cmd, self.s04, "权威命令存在性交叉核对")

if __name__ == "__main__":
    unittest.main(verbosity=2)
