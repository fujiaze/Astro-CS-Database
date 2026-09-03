#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-001 试金石测试：tools/traceability/check_traceability_matrix.py 的 mutation 验证。

用 tests/traceability/fixtures/*.json 六类负面样本证明检查器：
  - 空单元格（空串）必 FAIL（验收：不能以空字符串通过）；
  - 断链（TEST VERIFIED 而 SRC MISSING）必 FAIL 且报具体模块；
  - 重复 module_id 必 FAIL；
  - 悬空引用（文件不存在）必 FAIL 且报具体路径；
  - ID 格式非法必 FAIL；
  - 缺必需列必 FAIL。
同时证明正样本（真实矩阵）PASS，检查器不崩溃（exit != 3）。

纯 Python 3.10+ stdlib；python3 -m unittest tests.traceability.test_traceability_matrix
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CHECKER = os.path.join(REPO, "tools", "traceability", "check_traceability_matrix.py")
FIXTURES = os.path.join(REPO, "tests", "traceability", "fixtures")
# 检查器以固定相对路径解析：把 fixture 作为 docs/traceability/TRACEABILITY_MATRIX.json，
# 并复制 schema/LAYERS（视图 CSV 不复制，避免 parity 噪音；缺 CSV 的 MISSING_FILE 不掩盖断言代码）
RELS = {
    "docs/traceability/TRACEABILITY_LAYERS.csv": os.path.join(REPO, "docs", "traceability", "TRACEABILITY_LAYERS.csv"),
    "schemas/traceability_matrix.schema.json": os.path.join(REPO, "schemas", "traceability_matrix.schema.json"),
}


def build_env(td, fixture_name):
    dst_dir = os.path.join(td, "docs", "traceability")
    os.makedirs(dst_dir, exist_ok=True)
    os.makedirs(os.path.join(td, "schemas"), exist_ok=True)
    shutil.copy(os.path.join(FIXTURES, fixture_name), os.path.join(dst_dir, "TRACEABILITY_MATRIX.json"))
    for rel, src in RELS.items():
        shutil.copy(src, os.path.join(td, rel))


def run_checker(root):
    return subprocess.run([sys.executable, CHECKER, "--root", root],
                          capture_output=True, text=True, timeout=120)


class TestMatrixChecker(unittest.TestCase):

    def test_01_real_matrix_passes(self):
        r = run_checker(REPO)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("TRACEABILITY_MATRIX_PASS", r.stdout)

    def test_02_each_fixture_fails_without_crash(self):
        """六类负面样本全部 FAIL，且不崩溃（exit != 3），且带期望错误代码。"""
        expected_codes = {
            "empty_cell.json": "EMPTY_CELL_VIOLATION",
            "chain_break.json": "CHAIN_BREAK",
            "duplicate_module.json": "DUPLICATE_ID",
            "dangling_ref.json": "DANGLING_REF",
            "bad_id_format.json": "ID_FORMAT_VIOLATION",
            "missing_column.json": "SCHEMA_VIOLATION",
        }
        for fn, code in expected_codes.items():
            with self.subTest(fixture=fn):
                with tempfile.TemporaryDirectory() as td:
                    build_env(td, fn)
                    r = run_checker(td)
                    self.assertNotEqual(r.returncode, 0, f"{fn} 应 FAIL 但返回 0")
                    self.assertNotEqual(r.returncode, 3, f"{fn} 检查器崩溃: {r.stdout}{r.stderr}")
                    self.assertIn(code, r.stdout, f"{fn} 缺期望代码 {code}\n{r.stdout}")

    def test_03_broken_chain_reports_module(self):
        """断链必须报具体 module_id（验收：机器检查输出具体断链而不崩溃）。"""
        with tempfile.TemporaryDirectory() as td:
            build_env(td, "chain_break.json")
            r = run_checker(td)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("MOD-FIX-001", r.stdout)

    def test_04_dangling_ref_reports_path(self):
        """悬空引用必须报具体不存在的路径。"""
        with tempfile.TemporaryDirectory() as td:
            build_env(td, "dangling_ref.json")
            r = run_checker(td)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("no/such/file.c", r.stdout)


if __name__ == "__main__":
    unittest.main()
