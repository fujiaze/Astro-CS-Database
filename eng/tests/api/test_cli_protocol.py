#!/usr/bin/env python3
"""API-002 测试: CLI 协议合同机器门(04 §6 条款 1-5 的合同态实现)。"""
import json, os, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DOC = os.path.join(REPO, "docs", "api", "CLI_PROTOCOL_V1.md")
# 断链修复（第二次，2026-09-16，负责人裁决 C）：上一版 LEDGER_04 指向
# artifacts/evidence/prerelease-v5/AUDIT_REVIEW/control/04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md，
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
        # CLI-001 把用户命令树切换为 ASTROCS_DESIGN §6.2 的唯一七行树
        # (normalize/mosaic/export + help/--version/doctor/benchmark)；旧 phase1|2|3 与
        # config */modules */selftest/test synthetic/verify*/drizzle/benchmark cpu|
        # verify-profile/hardware inspect 全部删除且 rc=2（phase 仅内部指代）。
        # 断言对象 = 本 tracked 权威文档 docs/api/CLI_PROTOCOL_V1.md §1 的命令树 +
        # 显式删除声明（旧期望值随 CLI-001 过期，此处只改期望、不放宽语义）。
        for cmd in ("astrocs --version [--json]",
                    "astrocs normalize (--json <config.json> | --template [-o <path>] | --help)",
                    "astrocs mosaic (--json <config.json> | --template [-o <path>] | --help)",
                    "astrocs export (--json <config.json> | --template [-o <path>] | --help)",
                    "astrocs help",
                    "astrocs doctor [--json]",
                    "astrocs benchmark"):
            self.assertIn(cmd, self.s, f"§6.2 命令树缺行: {cmd}")
        for legacy in ("phase1|2|3", "config *", "modules *", "selftest",
                       "test synthetic", "verify*", "drizzle", "benchmark cpu",
                       "verify-profile", "hardware inspect"):
            self.assertIn(legacy, self.s, f"旧命令面删除声明缺 {legacy}")
        self.assertIn("全部删除且 rc=2", self.s)
        # 唯一可执行: 发布 manifest 不含旧 Phase exe（§6-5，CLI-001 后=恰一 astrocs）
        self.assertIn("恰一 astrocs", self.s)

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
        # CLI-001 后 §1 = §6.2 七行树；doctor 的冻结写法是 "astrocs doctor [--json]"
        # （可选参数形），旧 "astrocs doctor --json" 期望随命令树切换过期。
        for cmd in ("astrocs --version [--json]", "astrocs doctor [--json]",
                    "astrocs normalize (--json <config.json> | --template [-o <path>] | --help)"):
            self.assertIn(cmd, self.s04, "权威命令存在性交叉核对")

if __name__ == "__main__":
    unittest.main(verbosity=2)
