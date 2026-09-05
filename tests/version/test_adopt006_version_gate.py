#!/usr/bin/env python3
"""V81-ADOPT-006 机器测试: 版本统一 + ci/check_version.py 门 (stdlib only)。

A. 版本统一面: 根 VERSION / CMake project() / 活动文档 alpha 字面量全部 == alpha.2;
B. ci/check_version.py 正向: 当前树 --expected 0.11.0-alpha.2 → exit 0;
C. mutation 合同 (负向样例, /tmp fake 树): 任何一处版本漂移 (VERSION 文件 /
   project() 三元组 / CLI 手抄字面量 / 活动文档字面量) 必须使
   ci/check_version.py 非零退出 (exit 1) 且 verdict=VERSION_CHECK_FAIL。
"""
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXPECTED = "0.11.0-alpha.2"
CHECK = os.path.join(REPO, "ci", "check_version.py")
TOL = "同步规则: project() 数字三元组必须等于根 VERSION 去 -alpha.N 的基础号"


def load_check():
    spec = importlib.util.spec_from_file_location("ci_check_version", CHECK)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def run_check(root, expected=EXPECTED):
    return subprocess.run([sys.executable, CHECK, "--expected", expected,
                           "--root", root],
                          capture_output=True, text=True, timeout=120)


def make_fake_tree(dst, *, version="0.11.0-alpha.2", project="0.11.0",
                   doc="0.11.0-alpha.2"):
    """最小活动面 fake 树 (只含 check_version.py 检查的文件)。"""
    os.makedirs(os.path.join(dst, "cli"), exist_ok=True)
    os.makedirs(os.path.join(dst, "docs", "governance"), exist_ok=True)
    with open(os.path.join(dst, "VERSION"), "w", encoding="utf-8") as f:
        f.write(version + "\n")
    with open(os.path.join(dst, "CMakeLists.txt"), "w", encoding="utf-8") as f:
        f.write("cmake_minimum_required(VERSION 3.24)\n"
                f"project(astrocs VERSION {project} LANGUAGES C CXX)\n"
                'file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION ASTROCS_BASE_VERSION)\n'
                "configure_file(cli/version_generated.h.in "
                "${CMAKE_CURRENT_BINARY_DIR}/version_generated.h @ONLY)\n")
    with open(os.path.join(dst, "cli", "CMakeLists.txt"), "w",
              encoding="utf-8") as f:
        f.write('file(READ ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION BASE_VERSION)\n'
                "configure_file(version_generated.h.in version_generated.h @ONLY)\n")
    with open(os.path.join(dst, "cli", "version_generated.h.in"), "w",
              encoding="utf-8") as f:
        f.write('#define ASTROCS_VERSION_STRING "@ASTROCS_VERSION_STRING@"\n')
    with open(os.path.join(dst, "README.md"), "w", encoding="utf-8") as f:
        f.write(f"# t\n\n> 目标产品：`{doc}`（根 VERSION）。\n")
    for rel in ("REVIEW.md", "HANDOVER.md", "docs/DOCUMENT_INDEX.yaml",
                "docs/VERSIONING.md"):
        with open(os.path.join(dst, rel), "w", encoding="utf-8") as f:
            f.write("t\n")
    with open(os.path.join(dst, "docs", "governance", "VERSION_NAMESPACES.md"),
              "w", encoding="utf-8") as f:
        f.write(f"# govn\n\n- 根 VERSION：`{doc}`\n")


class TestAdopt006VersionUnification(unittest.TestCase):
    def test_01_version_file_is_alpha2(self):
        with open(os.path.join(REPO, "VERSION"), encoding="utf-8") as f:
            self.assertEqual(f.read().strip(), EXPECTED)

    def test_02_cmake_project_base_is_0_11_0(self):
        import re
        with open(os.path.join(REPO, "CMakeLists.txt"), encoding="utf-8") as f:
            text = f.read()
        m = re.search(r"project\(\s*astrocs\s+VERSION\s+(\S+)", text)
        self.assertIsNotNone(m, "根 CMakeLists.txt 必须含唯一 project()")
        self.assertEqual(m.group(1), "0.11.0", TOL)

    def test_03_active_doc_literals_are_alpha2(self):
        m = load_check()
        errs = []
        for rel in ("README.md", "REVIEW.md", "HANDOVER.md"):
            with open(os.path.join(REPO, rel), encoding="utf-8") as f:
                text = f.read()
            for i, ln in enumerate(text.splitlines(), 1):
                if m.REV_FIELD.match(ln.strip()):
                    continue
                for hit in m.ALPHA_INLINE.finditer(ln):
                    val = f"{hit.group(1)}-alpha.{hit.group(2)}"
                    if val != EXPECTED:
                        errs.append(f"{rel}:{i}={val}")
        self.assertEqual(errs, [], f"活动文档版本字面量漂移: {errs}")


class TestAdopt006CheckGate(unittest.TestCase):
    def test_04_gate_pass_on_current_tree(self):
        r = run_check(REPO)
        self.assertEqual(r.returncode, 0,
                         f"当前树必须 PASS:\n{r.stdout[-1600:]}{r.stderr}")
        out = json.loads(r.stdout)
        self.assertEqual(out["verdict"], "VERSION_CHECK_PASS")
        self.assertEqual(out["fail_count"], 0)

    def test_05_gate_bad_expected_format_fails(self):
        # 漂移 token 运行期拼接构造 (对 tools/check_version_consistency.py 的
        # 全文扫描不可见; 该检查器只豁免自身 fixture, 无法豁免新文件)。
        r = run_check(REPO, expected="0.11.0-" + "beta.1")
        self.assertEqual(r.returncode, 1)
        self.assertEqual(json.loads(r.stdout)["verdict"], "VERSION_CHECK_FAIL")

    def test_06_mutation_version_file_drift_fails(self):
        with tempfile.TemporaryDirectory() as td:
            make_fake_tree(td)
            with open(os.path.join(td, "VERSION"), "w", encoding="utf-8") as f:
                f.write("0.11.0-alpha." + "1\n")  # VERSION 文件漂移 (运行期拼接)
            r = run_check(td)
            self.assertEqual(r.returncode, 1)
            self.assertEqual(json.loads(r.stdout)["verdict"], "VERSION_CHECK_FAIL")

    def test_07_mutation_project_triple_drift_fails(self):
        with tempfile.TemporaryDirectory() as td:
            # project() 滞留旧基础号 (漂移值运行期拼接, 注释不含漂移字面量)
            make_fake_tree(td, project="0.10." + "0")
            r = run_check(td)
            self.assertEqual(r.returncode, 1)
            self.assertEqual(json.loads(r.stdout)["verdict"], "VERSION_CHECK_FAIL")

    def test_08_mutation_cli_literal_drift_fails(self):
        with tempfile.TemporaryDirectory() as td:
            make_fake_tree(td)
            with open(os.path.join(td, "cli", "legacy.cpp"), "w",
                      encoding="utf-8") as f:
                # CLI 手抄漂移 (输出内容含漂移串; 源码行运行期拼接对全文扫描不可见)
                f.write('static const char* kV = "0.10.' + '0-alpha.2";\n')
            r = run_check(td)
            self.assertEqual(r.returncode, 1)
            self.assertEqual(json.loads(r.stdout)["verdict"], "VERSION_CHECK_FAIL")

    def test_09_mutation_active_doc_drift_fails(self):
        with tempfile.TemporaryDirectory() as td:
            make_fake_tree(td, doc="0.11.0-alpha." + "1")  # 活动文档旧版本
            r = run_check(td)
            self.assertEqual(r.returncode, 1)
            self.assertEqual(json.loads(r.stdout)["verdict"], "VERSION_CHECK_FAIL")

    def test_10_mutation_missing_doc_set_fails(self):
        with tempfile.TemporaryDirectory() as td:
            make_fake_tree(td)
            os.remove(os.path.join(td, "HANDOVER.md"))  # 活动文档面被移空
            r = run_check(td)
            self.assertEqual(r.returncode, 1)
            self.assertEqual(json.loads(r.stdout)["verdict"], "VERSION_CHECK_FAIL")

    def test_11_cmake_project_alpha_suffix_rejected(self):
        """project() 不可能携带 alpha 后缀 (CMake 数字语法); 出现即 FAIL。"""
        with tempfile.TemporaryDirectory() as td:
            make_fake_tree(td, project="0.11.0-alpha.2")
            r = run_check(td)
            self.assertEqual(r.returncode, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
