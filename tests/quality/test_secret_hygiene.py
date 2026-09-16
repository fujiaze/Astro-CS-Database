#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-SECRET-HYGIENE 的能绿能红证据（ROOT-006 验收门）。

正例（绿）：
  * test_positive_clean_tree_is_green                  —— 干净目录树 rc=0；
  * test_positive_real_repo_pack_scope_is_green        —— 真仓审计包范围 rc=0；
  * test_positive_bare_hex_in_normal_file_is_info_only —— 普通文件里的 commit 哈希只记 INFO；
  * test_positive_placeholder_and_env_ref_are_not_hits —— 占位符/环境变量引用不算命中。

负例（红）：
  * test_negative_fabricated_secret_file_is_red         —— 自造假形态文件（私钥头/sk-/password=）rc!=0；
  * test_negative_credential_named_file_with_hex_is_red —— 凭据类文件名 + 裸 hex → 升级判红；
  * test_negative_read_failure_is_red                   —— 文件不存在/不可读 → fail-closed 判红；
  * test_negative_oversize_text_is_red                  —— 文本超限未扫 → fail-closed 判红；
  * test_negative_empty_scan_set_is_red                 —— 扫描集为空 → fail-closed 判红；
  * test_negative_walk_root_missing_is_red              —— 扫描根不存在 → fail-closed 判红；
  * test_negative_packer_missing_is_red                 —— 打包器不可用 → fail-closed 判红。

输出纪律证据：
  * test_report_leaks_no_matched_value                  —— 命中值（哨兵串）不得出现在报告/stdout；
  * test_binary_extension_is_skipped_and_counted        —— 二进制按声明边界跳过且计数上报。

审计包排除保证证据（ROOT-006 交付物 1）：
  * test_packer_denies_credential_file_even_if_whitelisted —— 白名单误加回也不入包。
"""
from __future__ import annotations

import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
CHECKER = REPO / "tools" / "quality" / "check_secret_hygiene.py"
PACKER = REPO / "tools" / "pack_audit_package.py"

# 自造形态：只用于负例，全部是**假**值（非任何真实凭据）
FAKE_PRIVATE_KEY = "-----BEGIN " + "OPENSSH PRIVATE KEY-----"
FAKE_SK = "sk-" + "FAKE0000000000000000000000000000"
FAKE_PASSWORD_LINE = "password = " + '"' + "F4ke-Passw0rd-NotReal" + '"'
SENTINEL = "SENTINEL" + "D0NotPrint0123456789abcdef"
FAKE_HEX40 = "0123456789abcdef0123456789abcdef01234567"
# 基础设施标识样例：CGNAT 文档样例地址 + 组装式私钥名（避免把真实节点标识再写进仓库）
FAKE_CGNAT_IP = "100.64.9.9"
FAKE_KEYNAME = "id_" + "ed25519"


def run_checker(*args, timeout=900):
    proc = subprocess.run([sys.executable, str(CHECKER), *args],
                          capture_output=True, text=True, timeout=timeout)
    report = None
    if "--json-out" in args:
        out = pathlib.Path(args[args.index("--json-out") + 1])
        if out.is_file():
            report = json.loads(out.read_text(encoding="utf-8"))
    return proc.returncode, proc.stdout + proc.stderr, report


def tree_with(root: pathlib.Path, files: dict) -> None:
    for rel, body in files.items():
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")


class SecretHygienePositiveTest(unittest.TestCase):
    def test_positive_clean_tree_is_green(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "clean"
            root.mkdir()
            tree_with(root, {"src/main.py": "print('ok')\n",
                             "docs/readme.md": "普通文档，无凭据形态。\n"})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root), "--quiet",
                                          "--json-out", str(root / "r.json"))
            self.assertEqual(rc, 0, out)
            self.assertEqual(report["verdict"], "PASS")
            self.assertEqual(report["counts"]["files_scanned"], 2)

    def test_positive_real_repo_pack_scope_is_green(self):
        """真仓 + 审计包范围：树上不得存在可被直接利用的凭据形态。

        并发说明：本工作区有多条线在写/提交，`git ls-files` 与读盘之间可能竞态
        （列出的文件刚被别的线删除），fail-closed 会把这种竞态判成「不可判定」→ FAIL。
        CI 单写者环境不会触发；此处最多重试 3 次以消除竞态抖动（不放松判定）。
        """
        last = None
        for _ in range(3):
            with tempfile.TemporaryDirectory() as td:
                rc, out, report = run_checker("--scope", "pack", "--quiet",
                                              "--json-out", str(pathlib.Path(td) / "r.json"),
                                              timeout=1800)
            last = (rc, out, report)
            if rc == 0:
                break
        rc, out, report = last
        self.assertEqual(rc, 0, out[:2000])
        self.assertEqual(report["verdict"], "PASS", report.get("verdict_reasons"))
        self.assertEqual(report["fatal_paths"], [])
        self.assertGreater(report["counts"]["files_scanned"], 1000)

    def test_positive_bare_hex_in_normal_file_is_info_only(self):
        """普通文件里的 40 位 hex（提交哈希/校验和）只记 INFO，不判红——否则仓库恒红。"""
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "hexok"
            root.mkdir()
            tree_with(root, {"reports/r.md": "commit " + FAKE_HEX40 + " 已合入\n"})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root),
                                          "--quiet", "--json-out", str(root / "r.json"))
            self.assertEqual(rc, 0, out)
            entry = report["files_with_findings"][0]
            self.assertIn("HEX40", entry["info_patterns"])
            self.assertEqual(entry["fatal_patterns"], [])
            self.assertEqual(entry["patterns"]["HEX40"], [1])

    def test_positive_placeholder_and_env_ref_are_not_hits(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "ph"
            root.mkdir()
            tree_with(root, {"conf/settings.py": "password = \"${DB_PASSWORD}\"\ntoken = os.environ[\"API_TOKEN\"]\n"})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root), "--quiet",
                                          "--json-out", str(root / "r.json"))
            self.assertEqual(rc, 0, out)
            self.assertEqual(report["files_with_findings"], [])


class SecretHygieneNegativeTest(unittest.TestCase):
    def test_negative_fabricated_secret_file_is_red(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "bad"
            root.mkdir()
            tree_with(root, {"notes/creds.md": FAKE_PRIVATE_KEY + "\n" + FAKE_SK + "\n" + FAKE_PASSWORD_LINE + "\n"})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root),
                                          "--quiet", "--json-out", str(root / "r.json"))
            self.assertNotEqual(rc, 0)
            self.assertEqual(report["verdict"], "FAIL")
            self.assertIn("secret_shape_detected", report["verdict_reasons"])
            self.assertEqual(report["fatal_paths"], ["notes/creds.md"])
            entry = report["files_with_findings"][0]
            self.assertIn("PRIVATE_KEY_HEADER", entry["fatal_patterns"])
            self.assertIn("SK_API_TOKEN", entry["fatal_patterns"])
            # 赋值形态在普通文件里只记 INFO（见 checker 策略：ESCALATE 仅在凭据类文件判红）
            self.assertIn("PASSWORD_ASSIGN", entry["info_patterns"])

    def test_negative_credential_named_file_with_password_literal_is_red(self):
        """凭据类文件名里的赋值字面量升级判红（正是本次 ROOT-006 的风险形态）。"""
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "senspw"
            root.mkdir()
            tree_with(root, {"ops/node_access.md": FAKE_PASSWORD_LINE + "\n"})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root),
                                          "--quiet", "--json-out", str(root / "r.json"))
            self.assertNotEqual(rc, 0)
            entry = report["files_with_findings"][0]
            self.assertTrue(entry["sensitive_class"])
            self.assertIn("PASSWORD_ASSIGN", entry["fatal_patterns"])

    def test_negative_credential_named_file_with_hex_is_red(self):
        """凭据类文件名（含 secret/credential 词）里的裸 hex 升级判红。"""
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "sens"
            root.mkdir()
            tree_with(root, {"ops/credentials.md": "token-hash: " + FAKE_HEX40 + "\n"})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root),
                                          "--quiet", "--json-out", str(root / "r.json"))
            self.assertNotEqual(rc, 0)
            self.assertTrue(report["files_with_findings"][0]["sensitive_class"])
            self.assertIn("HEX40", report["files_with_findings"][0]["fatal_patterns"])

    def test_negative_read_failure_is_red(self):
        """fail-closed：无法读取（不存在）的文件不得静默跳过。"""
        with tempfile.TemporaryDirectory() as td:
            rc, out, report = run_checker("--files", "missing.md", "--root", td, "--quiet",
                                          "--json-out", str(pathlib.Path(td) / "r.json"))
            self.assertNotEqual(rc, 0)
            self.assertIn("undecidable_input", report["verdict_reasons"])
            self.assertEqual(report["undecidable"][0]["reason"], "file_unreadable:FileNotFoundError")

    def test_negative_oversize_text_is_red(self):
        """fail-closed：文本超过扫描上限 → 判红（不允许"没扫到就算干净"）。"""
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "big"
            root.mkdir()
            tree_with(root, {"big.txt": "x" * 5000})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root),
                                          "--max-bytes", "1024", "--quiet",
                                          "--json-out", str(root / "r.json"))
            self.assertNotEqual(rc, 0)
            self.assertIn("undecidable_input", report["verdict_reasons"])
            self.assertEqual(report["counts"]["files_oversize"], 1)

    def test_negative_empty_scan_set_is_red(self):
        """fail-closed：扫描集为空 → 判红（防止"零文件扫描"恒绿）。"""
        with tempfile.TemporaryDirectory() as td:
            rc, out, report = run_checker("--files", "--root", td, "--quiet",
                                          "--json-out", str(pathlib.Path(td) / "r.json"))
            self.assertNotEqual(rc, 0)
            self.assertIn("empty_scan_set", report["verdict_reasons"])

    def test_negative_walk_root_missing_is_red(self):
        with tempfile.TemporaryDirectory() as td:
            rc, out, report = run_checker("--scope", "walk", "--root", "/nonexistent-astrocs-root",
                                          "--quiet", "--json-out", str(pathlib.Path(td) / "r.json"))
            self.assertNotEqual(rc, 0)
            self.assertIn("walk_root_missing", report["verdict_reasons"])

    def test_negative_packer_missing_is_red(self):
        with tempfile.TemporaryDirectory() as td:
            rc, out, report = run_checker("--scope", "pack", "--packer", "/nonexistent_packer.py",
                                          "--quiet", "--json-out", str(pathlib.Path(td) / "r.json"))
            self.assertNotEqual(rc, 0)
            self.assertIn("packer_unavailable:missing", report["verdict_reasons"])


class SecretHygieneOutputDisciplineTest(unittest.TestCase):
    def test_report_leaks_no_matched_value(self):
        """输出纪律：命中值（哨兵串）不得出现在报告 JSON 与 stdout。"""
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "leak"
            root.mkdir()
            secret_line = "api_key = " + '"' + SENTINEL + '"'
            tree_with(root, {"cfg/api_secret.conf": secret_line + "\n"})
            out_path = root / "r.json"
            rc, out, report = run_checker("--scope", "walk", "--root", str(root),
                                          "--json-out", str(out_path))
            self.assertNotEqual(rc, 0)
            text = out_path.read_text(encoding="utf-8")
            self.assertNotIn(SENTINEL, text)
            self.assertNotIn(SENTINEL, out)
            self.assertNotIn(SENTINEL, json.dumps(report, ensure_ascii=False))
            self.assertFalse(report["output_contains_values"])
            # 仍然给出可行动的定位信息（路径 + 形态 + 行号）
            self.assertEqual(report["files_with_findings"][0]["patterns"]["SECRET_ASSIGN"], [1])

    def test_binary_extension_is_skipped_and_counted(self):
        """边界显式登记：二进制不做文本扫描，但必须计数上报（非静默跳过）。"""
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "bin"
            root.mkdir()
            tree_with(root, {"a/ok.txt": "clean\n", "a/blob.png": FAKE_SK + "\n"})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root), "--quiet",
                                          "--json-out", str(root / "r.json"))
            self.assertEqual(rc, 0, out)
            self.assertEqual(report["counts"]["files_skipped_binary"], 1)
            self.assertNotIn("a/blob.png", report["fatal_paths"])

    def test_report_only_mode_records_findings_without_failing(self):
        """--report-only：审计模式（白名单结构扫描用）只报告、不判红；fail-closed 仍生效。"""
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "ro"
            root.mkdir()
            tree_with(root, {"c/creds.md": FAKE_SK + "\n"})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root),
                                          "--report-only", "--quiet",
                                          "--json-out", str(root / "r.json"))
            self.assertEqual(rc, 0, out)
            self.assertEqual(report["verdict"], "PASS")
            self.assertEqual(report["files_with_findings"][0]["fatal_patterns"], ["SK_API_TOKEN"])


class SecretHygieneInfraInfoTest(unittest.TestCase):
    """基础设施侦察信息（非凭据）：纳入扫描形态但只记 INFO，不判红。"""

    def test_infra_identifiers_are_info_not_fatal(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "infra"
            root.mkdir()
            tree_with(root, {"ops/runbook.md": "ssh -i /home/ops/.ssh/" + FAKE_KEYNAME +
                             " ops@" + FAKE_CGNAT_IP + "\n"})
            rc, out, report = run_checker("--scope", "walk", "--root", str(root), "--quiet",
                                          "--json-out", str(root / "r.json"))
            self.assertEqual(rc, 0, out)
            entry = report["files_with_findings"][0]
            self.assertEqual(entry["fatal_patterns"], [])
            for pid in ("INFRA_CGNAT_IP", "INFRA_SSH_TARGET", "INFRA_PRIVATE_KEY_REF"):
                self.assertIn(pid, entry["info_patterns"])
                self.assertEqual(entry["patterns"][pid], [1])
            # 连基础设施标识的字面量也不落报告（只留布尔与行号）
            text = (root / "r.json").read_text(encoding="utf-8")
            self.assertNotIn(FAKE_CGNAT_IP, text)
            self.assertNotIn(FAKE_KEYNAME, text)


class SecretHygieneConcurrencyAndPolicyTest(unittest.TestCase):
    """并发竞态判定 与 规则定义文件降级注册表。"""

    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location("_csh_under_test", CHECKER)
        cls.mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.mod)

    def test_pattern_definition_registry_is_exact(self):
        """降级清单必须精确等于「规则本体 + 其测试」，不得被偷偷扩大。"""
        self.assertEqual(set(self.mod.PATTERN_DEFINITION_FILES),
                         {"tools/quality/check_secret_hygiene.py",
                          "tests/quality/test_secret_hygiene.py"})

    def test_definition_files_are_downgraded_but_still_listed(self):
        """规则定义文件命中降级为 INFO（仍列 路径/形态/行号），不判红。"""
        report, rc = self.mod.build_report("tracked", REPO,
                                           ["tools/quality/check_secret_hygiene.py"],
                                           self.mod.DEFAULT_MAX_BYTES, False)
        self.assertEqual(report["verdict"], "PASS", report.get("verdict_reasons"))
        self.assertEqual(report["fatal_paths"], [])
        entry = report["files_with_findings"][0]
        self.assertTrue(entry["policy_downgraded"])
        self.assertNotEqual(entry["patterns"], {})      # 仍逐条列出，不是跳过

    def test_vanished_after_listing_is_not_fatal(self):
        """清单枚举后文件被并发删除 → 计入 files_vanished，不按不可判定判红。"""
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "repo"
            root.mkdir()
            subprocess.run(["git", "init", "-q", str(root)], check=True, timeout=120)
            (root / "a.txt").write_text("clean\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(root), "add", "a.txt"], check=True, timeout=120)
            report, rc = self.mod.build_report("tracked", root, ["a.txt", "gone.txt"],
                                               self.mod.DEFAULT_MAX_BYTES, False, git_backed=True)
            self.assertEqual(report["counts"]["files_vanished"], 1)
            self.assertEqual(report["undecidable"], [])
            self.assertEqual(report["verdict"], "PASS")

    def test_all_vanished_is_red(self):
        """fail-closed 兜底：全消失（零文件实扫）仍判红，不能变成恒绿。"""
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "repo2"
            root.mkdir()
            subprocess.run(["git", "init", "-q", str(root)], check=True, timeout=120)
            report, rc = self.mod.build_report("tracked", root, ["gone.txt"],
                                               self.mod.DEFAULT_MAX_BYTES, False, git_backed=True)
            self.assertNotEqual(rc, 0)
            self.assertIn("nothing_scanned", report["verdict_reasons"])


class PackerDenyGuaranteeTest(unittest.TestCase):
    """审计包排除保证：凭据类文件即使被显式指定/白名单误加回也不入包。"""

    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location("_packer_under_test", PACKER)
        cls.packer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.packer)

    def test_packer_denies_credential_file_even_if_whitelisted(self):
        self.assertNotIn("FATDUCK_ACCESS.md", self.packer.ROOT_FILES)
        self.assertEqual(self.packer.allowed("FATDUCK_ACCESS.md"), (False, ""))
        self.assertTrue(self.packer.denied("FATDUCK_ACCESS.md"))
        restored = self.packer.ROOT_FILES | {"FATDUCK_ACCESS.md"}
        try:
            self.packer.ROOT_FILES = restored
            self.assertEqual(self.packer.allowed("FATDUCK_ACCESS.md"), (False, ""),
                             "白名单误加回后仍须被排除保证拦下")
        finally:
            self.packer.ROOT_FILES = restored - {"FATDUCK_ACCESS.md"}

    def test_packer_denies_common_credential_path_shapes(self):
        for rel in ("ops/.env", "ops/.env.local", "keys/server.pem", "keys/id_ed25519",
                    "x/NODE_ACCESS.md", "cfg/client.key"):
            self.assertTrue(self.packer.denied(rel), rel)
        for rel in ("README.md", "docs/design.md", "tests/quality/test_secret_hygiene.py"):
            self.assertFalse(self.packer.denied(rel), rel)


if __name__ == "__main__":
    unittest.main()
