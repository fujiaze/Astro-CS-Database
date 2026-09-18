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
    """最小活动面 fake 树 —— **必须满足 ci/check_version.py 的锚存活合同**。

    W4-A3 订正：原夹具按迁移前布局构造（`cli/version_generated.h.in`、根
    `REVIEW.md`/`HANDOVER.md`），而 ci/check_version.py 的 [0] 锚存活判据
    （ENGINEERING_SPEC §8 fail-closed）要求 CLI_DIR_REL =
    `lib/infrastructure/cli`、CLI_TEMPLATE_REL =
    `lib/infrastructure/cli/version_generated.h.in` 与 DOC_SET_FILES/DOC_SET_DIRS
    全集存在 ⇒ 任一缺失即 rc=2（ANCHOR_STALE），mutation 用例的 rc=1 断言被
    fail-closed 遮蔽（实测 6 条断言 2 != 1）。

    现行做法：锚集**单源**取自 ci/check_version.py 的常量（模块内 load_check()
    后直接读 DOC_SET_FILES / DOC_SET_DIRS / CLI_TEMPLATE_REL），不在本测试里
    重复维护第二份列表（同一事实两处判据正是本任务要消除的缺陷型）。
    """
    cv = load_check()
    os.makedirs(os.path.join(dst, "cli"), exist_ok=True)
    with open(os.path.join(dst, "VERSION"), "w", encoding="utf-8") as f:
        f.write(version + "\n")
    with open(os.path.join(dst, "CMakeLists.txt"), "w", encoding="utf-8") as f:
        f.write("cmake_minimum_required(VERSION 3.24)\n"
                f"project(astrocs VERSION {project} LANGUAGES C CXX)\n"
                'file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION ASTROCS_BASE_VERSION)\n'
                "configure_file(" + cv.CLI_TEMPLATE_REL + " "
                "${CMAKE_CURRENT_BINARY_DIR}/version_generated.h @ONLY)\n")
    # cli/CMakeLists.txt 保留原位（不在构建图内）：check_version.py 的 CLI_CMAKE_REL
    # 三条 chain 判据的输入。
    with open(os.path.join(dst, "cli", "CMakeLists.txt"), "w",
              encoding="utf-8") as f:
        f.write('file(READ ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION BASE_VERSION)\n'
                "configure_file(version_generated.h.in version_generated.h @ONLY)\n")
    template = os.path.join(dst, cv.CLI_TEMPLATE_REL)
    os.makedirs(os.path.dirname(template), exist_ok=True)
    with open(template, "w", encoding="utf-8") as f:
        f.write('#define ASTROCS_VERSION_STRING "@ASTROCS_VERSION_STRING@"\n')
    # 活动文档集：README 承载 doc 字面量，其余成员只需存在（锚存活 + [5]/[6]）
    for rel in cv.DOC_SET_FILES:
        full = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        body = (f"# t\n\n> 目标产品：`{doc}`（根 VERSION）。\n"
                if rel == "README.md" else "t\n")
        with open(full, "w", encoding="utf-8") as f:
            f.write(body)
    for d in cv.DOC_SET_DIRS:
        full = os.path.join(dst, d)
        os.makedirs(full, exist_ok=True)
        if not os.listdir(full):
            with open(os.path.join(full, "placeholder.md"), "w",
                      encoding="utf-8") as f:
                f.write("t\n")
    with open(os.path.join(dst, "docs", "governance", "VERSION_NAMESPACES.md"),
              "w", encoding="utf-8") as f:
        f.write(f"# govn\n\n- 根 VERSION：`{doc}`\n")


def make_absence_tree(dst):
    """§12 absence 夹具: 完全无版本信息面。

    无 VERSION / 无 project() VERSION / 无 file(READ VERSION) 生成链 /
    无 CLI 版本模板 / 文档与 CLI 源码均无 alpha 字面量。
    锚集单源取自 ci/check_version.py; 非版本锚 (CLI_DIR_REL / DOC_SET_*) 仍须存活,
    否则 absence 模式仍应因锚失效判红 (防移空)。
    """
    cv = load_check()
    os.makedirs(dst, exist_ok=True)
    with open(os.path.join(dst, "CMakeLists.txt"), "w", encoding="utf-8") as f:
        f.write("cmake_minimum_required(VERSION 3.24)\n"
                "project(astrocs LANGUAGES C CXX)\n")
    cli_dir = os.path.join(dst, cv.CLI_DIR_REL)
    os.makedirs(cli_dir, exist_ok=True)
    with open(os.path.join(cli_dir, "placeholder.txt"), "w", encoding="utf-8") as f:
        f.write("// 无版本信息面\n")
    for rel in cv.DOC_SET_FILES:
        full = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as f:
            f.write("# t\n\n版本信息面: 无 (Alpha 前, ASTROCS_DESIGN §12)\n")
    for d in cv.DOC_SET_DIRS:
        full = os.path.join(dst, d)
        os.makedirs(full, exist_ok=True)
        if not os.listdir(full):
            with open(os.path.join(full, "placeholder.md"), "w",
                      encoding="utf-8") as f:
                f.write("版本信息面: 无\n")


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
        """活动文档 alpha 字面量统一性 —— 扫描面**单源**取自 ci/check_version.py。

        W4-A3 订正：原实现硬编码 ("README.md", "REVIEW.md", "HANDOVER.md")。
        REVIEW.md / HANDOVER.md 已随 ROOT-007 归档（根下已不存在）⇒ FileNotFoundError；
        而 check_version.py 的权威活动文档集是 DOC_SET_FILES（10 项，含 docs/owner/**）。
        按 §8「注册表双向一致 / 锚存活」口径改绑到该常量，消除两套不同步判据。
        """
        m = load_check()
        scanned = [rel for rel in m.DOC_SET_FILES if os.path.isfile(os.path.join(REPO, rel))]
        self.assertTrue(scanned, "活动文档集不得为空（锚存活）")
        missing = [rel for rel in m.DOC_SET_FILES if not os.path.isfile(os.path.join(REPO, rel))]
        self.assertEqual(missing, [], f"DOC_SET_FILES 成员缺失（锚失效）: {missing}")
        errs = []
        for rel in scanned:
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
        """CLI 手抄版本漂移必须判红 —— 注入点随 ROOT-008 迁移改绑。

        W4-A3 订正：CLI 字面量扫描面已由 `cli/**` 迁到 `lib/infrastructure/cli/**`
        （check_version.py 的 CLI_DIR_REL）；原夹具把漂移写进 `cli/legacy.cpp`，
        迁移后该目录已不在扫描面内 ⇒ 注入变成 no-op、断言 0 != 1。注入点改由
        CLI_DIR_REL 单源派生。
        """
        cv = load_check()
        with tempfile.TemporaryDirectory() as td:
            make_fake_tree(td)
            cli_dir = os.path.join(td, cv.CLI_DIR_REL)
            os.makedirs(cli_dir, exist_ok=True)
            with open(os.path.join(cli_dir, "legacy.cpp"), "w",
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
        """活动文档面被移空必须判红 —— 口径 = §8 fail-closed（rc=2 且点名缺失锚）。

        W4-A3 订正：原夹具删根 `HANDOVER.md`（已归档 ⇒ FileNotFoundError）；且
        ci/check_version.py 对该情形的现行口径是 **ANCHOR_STALE / rc=2**
        （[0] 锚存活 fail-closed），不是 rc=1。断言随之改绑：仍要求**必红**，
        且必须点名缺失的 DOC_SET_FILES 成员（不许静默通过）。
        """
        cv = load_check()
        victim = "docs/owner/RELEASE_STATUS.md"
        self.assertIn(victim, cv.DOC_SET_FILES, "受害者必须是现行 DOC_SET_FILES 成员")
        with tempfile.TemporaryDirectory() as td:
            make_fake_tree(td)
            os.remove(os.path.join(td, victim))  # 活动文档面被移空
            r = run_check(td)
            self.assertEqual(r.returncode, 2, "锚失效 = harness 级失败（fail-closed）")
            self.assertEqual(json.loads(r.stdout)["verdict"], "ANCHOR_STALE")
            self.assertIn(victim, r.stderr, "必须点名缺失的锚（不许静默降级）")

    def test_11_cmake_project_alpha_suffix_rejected(self):
        """project() 不可能携带 alpha 后缀 (CMake 数字语法); 出现即 FAIL。"""
        with tempfile.TemporaryDirectory() as td:
            make_fake_tree(td, project="0.11.0-alpha.2")
            r = run_check(td)
            self.assertEqual(r.returncode, 1)


    def test_12_absence_no_version_info_passes(self):
        """§12 门方向: 版本信息完全不存在 ⇒ 不判红 (RELEASE-02 CI-HYGIENE)。

        旧实现反向强制版本存在 (anchor_alive_VERSION_REL ⇒ ANCHOR_STALE exit 2),
        CI 绿灯 = 必然违反 §12。新判据 = 版本信息存在则校验一致性, 不存在不得判红;
        absence 模式必须显式留痕 version_absence_alpha_pre (不静默通过)。
        """
        with tempfile.TemporaryDirectory() as td:
            make_absence_tree(td)
            r = subprocess.run([sys.executable, CHECK, "--root", td],
                               capture_output=True, text=True, timeout=120)
            self.assertEqual(r.returncode, 0,
                             f"无版本信息不得判红:\n{r.stdout[-1600:]}{r.stderr}")
            out = json.loads(r.stdout)
            self.assertEqual(out["verdict"], "VERSION_CHECK_PASS")
            self.assertTrue(
                any(c["id"] == "version_absence_alpha_pre" for c in out["checks"]),
                "absence 模式必须在 checks 中显式留痕")

    def test_13_absence_tree_with_version_literal_fails(self):
        """版本信息存在 ⇒ 必须校验一致性, 不得借 absence 模式放行。

        在 absence 夹具里注入一处 CLI alpha 字面量: 探测到版本信息后转入一致性校验,
        而 VERSION 文件缺席 ⇒ 版本锚失效 (ANCHOR_STALE) 或格式 FAIL, 总之必红。
        """
        cv = load_check()
        with tempfile.TemporaryDirectory() as td:
            make_absence_tree(td)
            cli_dir = os.path.join(td, cv.CLI_DIR_REL)
            with open(os.path.join(cli_dir, "legacy.cpp"), "w",
                      encoding="utf-8") as f:
                f.write('static const char* kV = "0.10.' + '0-alpha.2";\n')
            r = subprocess.run([sys.executable, CHECK, "--root", td],
                               capture_output=True, text=True, timeout=120)
            self.assertNotEqual(r.returncode, 0,
                                "版本信息存在即必须校验, 不得走 absence 模式放行")
            self.assertIn("version_presence_detected", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
