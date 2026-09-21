#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""审计包「排除保证」机器门（ROOT-006 的验收负例，取自 run/ 证据脚本并固化为 tracked 测试）。

背景：凭据/接入类文档曾随审计包（外发产物）扇出。ROOT-006 的修复 = 白名单移除 +
`DENY_PATHS`/`DENY_NAME_RE` 排除保证；本测试把该保证固化为可复跑机器门，防止回归：

1. 结构层：`allowed()` 对拒绝路径（含 Windows 分隔符与 `./` 变体）必须为 False；
   且**与白名单解耦**——即使白名单被误加回，仍必须拒绝；
2. 端到端层：走真实打包路径（模块级 `legacy_main`，打包入口 `main` 已按裁决退役），
   产物 zip 条目名中不得出现被拒文件，且不得误伤其它根文件。

依据：AGENTS.md §5（不读取/打印凭据；只做路径判定与条目名统计）、
ENGINEERING_SPEC §7/§8（根白名单；检查必须能红能绿、不允许静默坏掉）、
ROOT-006 任务卡与 reports/PROJECT-GOVERNANCE-01/security/SECURITY_NOTE.md。

本测试**不读取任何被拒文件的内容**，只断言路径判定与 zip 条目名。
"""
import importlib.util
import pathlib
import tempfile
import unittest
import zipfile

REPO = pathlib.Path(__file__).resolve().parents[2]
PACKER = REPO / "eng" / "tools" / "pack_audit_package.py"
DENIED_SAMPLE = "FATDUCK_ACCESS.md"  # 仅路径名，不涉及内容


def _load_packer():
    spec = importlib.util.spec_from_file_location("pack_audit_package_under_test", PACKER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class TestPackAuditExclusion(unittest.TestCase):
    def setUp(self):
        self.packer = _load_packer()

    # ── ① 结构层：白名单已移除 ──
    def test_01_not_in_whitelist(self):
        self.assertNotIn(DENIED_SAMPLE, self.packer.ROOT_FILES)

    # ── ② 结构层：allowed() 拒绝（含路径变体）──
    def test_02_allowed_rejects_canonical(self):
        self.assertEqual(self.packer.allowed(DENIED_SAMPLE), (False, ""))

    def test_03_allowed_rejects_path_variants(self):
        for variant in (".\\" + DENIED_SAMPLE, "./" + DENIED_SAMPLE,
                        "sub/dir/../" + DENIED_SAMPLE):
            with self.subTest(variant=variant):
                self.assertFalse(self.packer.allowed(variant)[0], variant)

    # ── ③ 结构层：排除保证与白名单解耦（回归哨兵）──
    def test_04_deny_survives_whitelist_regression(self):
        original = self.packer.ROOT_FILES
        try:
            self.packer.ROOT_FILES = original | {DENIED_SAMPLE}
            self.assertEqual(self.packer.allowed(DENIED_SAMPLE), (False, ""))
        finally:
            self.packer.ROOT_FILES = original

    # ── ④ 端到端：真实打包路径产物不含被拒文件 ──
    def test_05_end_to_end_zip_excludes_denied(self):
        if not hasattr(self.packer, "legacy_main"):
            self.fail("打包器缺少模块级 legacy_main（退役后仍须保留可复跑的打包路径）")
        with tempfile.TemporaryDirectory() as td:
            self.packer.OUT = pathlib.Path(td)
            rc = self.packer.legacy_main()
            zips = sorted(pathlib.Path(td).glob("AUDIT_PACKAGE_*.zip"))
            self.assertEqual(rc, 0, "打包路径必须可跑通（rc=0）")
            self.assertEqual(len(zips), 1, "应恰好产出一个审计包")
            with zipfile.ZipFile(zips[0]) as zf:
                names = zf.namelist()
            self.assertFalse(any(DENIED_SAMPLE in n for n in names),
                             "审计包条目名不得包含被拒文件")
            self.assertIn("README.md", names)
            self.assertIn("AGENTS.md", names)


if __name__ == "__main__":
    unittest.main()