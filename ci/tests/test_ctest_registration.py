# -*- coding: utf-8 -*-
"""CI-REG-002 单测：CTest 目标 → CI 检查项注册闭包（STD-F7 处置 1/2/4 + STD-F10）。

覆盖四层：
1. 注册闭包（负向必败）—— 在内存 fixture CMake 源上故意加一个未注册的
   add_test 目标 → 判定 FAIL 且逐条列出 target <- source；反向：同一目标经
   ci/checks.json 的 ctest_targets 显式登记（且 command 携带）→ PASS；
   冻结基线覆盖 → PASS；陈旧模式/未跑登记/基线漂移三类反向漂移均 FAIL。
2. 主仓库真实面 —— 现场 CMake 源 100% 被覆盖（无未注册目标）、注册模式无
   悬空、基线无漂移、每个 CTEST-* 检查项 waivable=false 且 ctest_targets
   与该检查 command 的 -R 目标一致（登记即真跑）。
3. STD-F7 处置 2/3 —— linux-main 含 CTEST-LINUX-FULL 全量 ctest 门；
   BUILD-GCC-RELEASE / DEEP-SAN-ASAN / DEEP-COV-CPP waivable=false。
4. STD-F10 —— p1wcs_astropy_cross 检查项的 prerequisite_tools 登记
   python3:astropy / python3:numpy，且 run.py 的 <exe>:<module> 探测三分支
   （模块在位 → PASS；模块缺失 → FAIL(prerequisite)；waivable 检查 → SKIPPED）。

fixture 全部落内存/临时目录；只读主仓库资产，不修改任何被检文件。
"""
from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

from ci.tests import _helpers as H  # noqa: E402

_TOOL = _REPO / "tools" / "quality" / "check_ctest_registration.py"
_spec = importlib.util.spec_from_file_location("check_ctest_registration", _TOOL)
TOOL = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(TOOL)

_REGISTRY = json.loads((_REPO / "ci" / "checks.json").read_text(encoding="utf-8"))
_BASELINE = json.loads((_REPO / "ci" / "ctest_baseline.json").read_text(encoding="utf-8"))
_CHECKS = {c["id"]: c for c in _REGISTRY["checks"]}

# 本轮（ASTROCS-CONSTITUTION-ALIGNMENT-V1）新增测试目标 → 显式检查项
NEW_TARGET_CHECKS = {
    "p1001_real_nodes": "CTEST-P1001-REAL-NODES",
    "p2001_real_nodes": "CTEST-P2001-REAL-NODES",
    "p2002_unc_rej_prov": "CTEST-P2002-UNC-REJ-PROV",
    "p3002_real_nodes": "CTEST-P3002-REAL-NODES",
    "p3002_uncertainty": "CTEST-P3002-UNCERTAINTY",
    "p3_projection_units": "CTEST-P3-PROJECTION-UNITS",
    "p3_projection_fault": "CTEST-P3-PROJECTION-FAULT",
    "p1wcs_apbp": "CTEST-P1WCS-APBP",
    "p1wcs_astropy_cross": "CTEST-P1WCS-ASTROPY-CROSS",
    "aio_abi_units": "CTEST-AIO-ABI-UNITS",
    "aio_abi_negative": "CTEST-AIO-ABI-NEGATIVE",
    "aio_abi_selfcheck": "CTEST-AIO-ABI-SELFCHECK",
    "hips_publish_atomic_units": "CTEST-AIO-HIPS-PUBLISH-ATOMIC-UNITS",
    "hips_publish_atomic": "CTEST-AIO-HIPS-PUBLISH-ATOMIC",
    "rt001_unique_executor": "CTEST-RT001-UNIQUE-EXECUTOR",
}

FIXTURE_SRC = (
    "add_executable(demo_test demo_test.cpp)\n"
    "add_test(NAME demo_units COMMAND demo_test units)\n"
)
FIXTURE_SRC_UNREGISTERED = FIXTURE_SRC + (
    "add_test(NAME ci_reg_002_negative_injection COMMAND demo_test nope)\n"
)


def _empty_registry() -> dict:
    return {"schema_version": 1, "checks": []}


class TestRegistrationClosureNegative(unittest.TestCase):
    """负向注入必败：未注册的新 add_test 目标一律 FAIL。"""

    def _verdict(self, source: str, registry: dict, baseline: dict) -> list:
        targets, structural = TOOL.parse_targets({"CMakeLists.txt": source})
        return structural + TOOL.evaluate(targets, registry, baseline)["errors"]

    def test_unregistered_new_add_test_fails(self):
        errors = self._verdict(FIXTURE_SRC_UNREGISTERED, _empty_registry(), {"targets": []})
        self.assertTrue(errors, "未注册的新 add_test 目标必须 FAIL")
        joined = "\n".join(errors)
        self.assertIn("C3", joined)
        self.assertIn("ci_reg_002_negative_injection <- CMakeLists.txt", joined)

    def test_registered_target_passes(self):
        errors = self._verdict(
            FIXTURE_SRC,
            {"schema_version": 1, "checks": [{
                "id": "DEMO-CTEST", "command": ["ctest", "-R", "^demo_units$"],
                "ctest_targets": ["demo_units"]}]},
            {"targets": []})
        self.assertEqual(errors, [])

    def test_baseline_covered_target_passes(self):
        errors = self._verdict(FIXTURE_SRC, _empty_registry(),
                               {"targets": ["demo_units"]})
        self.assertEqual(errors, [])

    def test_stale_pattern_fails(self):
        errors = self._verdict(
            FIXTURE_SRC,
            {"schema_version": 1, "checks": [{
                "id": "DEMO-CTEST", "command": ["ctest", "-R", "^ghost$"],
                "ctest_targets": ["ghost"]}]},
            {"targets": ["demo_units"]})
        self.assertTrue(any("C4" in e for e in errors), errors)

    def test_registered_but_not_executed_fails(self):
        """C6：ctest_targets 登记了目标但 command 里没有它（登记但未真跑）。"""
        errors = self._verdict(
            FIXTURE_SRC,
            {"schema_version": 1, "checks": [{
                "id": "DEMO-CTEST", "command": ["ctest", "-R", "^other$"],
                "ctest_targets": ["demo_units"]}]},
            {"targets": []})
        self.assertTrue(any("C6" in e for e in errors), errors)

    def test_stale_baseline_fails(self):
        errors = self._verdict(FIXTURE_SRC, _empty_registry(),
                               {"targets": ["demo_units", "removed_target"]})
        self.assertTrue(any("C5" in e for e in errors), errors)

    def test_non_name_add_test_fails_closed(self):
        """非 NAME 形式的 add_test 无法静态枚举 → 结构违规。"""
        errors = self._verdict("add_test(demo_units demo_test)\n",
                               _empty_registry(), {"targets": []})
        self.assertTrue(any("C1" in e for e in errors), errors)

    def test_cli_selftest_all_cases_pass(self):
        proc = subprocess.run([sys.executable, str(_TOOL), "--selftest"],
                              cwd=str(_REPO), capture_output=True, text=True,
                              encoding="utf-8", errors="replace", timeout=300)
        self.assertEqual(proc.returncode, 0, proc.stdout[-800:] + proc.stderr[-400:])
        self.assertEqual(json.loads(proc.stdout)["verdict"], "PASS")


class TestRealRepoRegistration(unittest.TestCase):
    """主仓库真实面：现场零未注册目标 + 登记面自洽。"""

    @classmethod
    def setUpClass(cls):
        cls.targets, cls.structural = TOOL.collect_real(_REPO)
        cls.verdict = TOOL.evaluate(cls.targets, _REGISTRY, _BASELINE)

    def test_no_unregistered_targets(self):
        self.assertEqual(self.structural, [])
        self.assertEqual(self.verdict["unregistered"], [],
                         "现场存在未注册的 add_test 目标（新增测试必须同提交注册）")

    def test_no_dangling_or_stale(self):
        self.assertEqual(self.verdict["dangling_patterns"], [])
        self.assertEqual(self.verdict["stale_baseline"], [])
        self.assertEqual(self.verdict["pattern_not_in_command"], [])

    def test_cli_verdict_pass_and_evidence_written(self):
        out = "run/ci/ut-ctest-registration/registration.json"
        proc = subprocess.run(
            [sys.executable, str(_TOOL), "--output", out],
            cwd=str(_REPO), capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=300)
        self.assertEqual(proc.returncode, 0, proc.stdout[-800:])
        report = json.loads((_REPO / out).read_text(encoding="utf-8"))
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["unregistered"], [])
        self.assertGreater(report["targets_total"], 100)

    def test_new_targets_are_explicitly_registered(self):
        for target, cid in NEW_TARGET_CHECKS.items():
            self.assertIn(target, self.targets, target)
            self.assertIn(cid, _CHECKS, cid)
            check = _CHECKS[cid]
            self.assertIn(target, check["ctest_targets"], cid)
            self.assertIn(target, check["command"], cid)
            self.assertFalse(check["waivable"], cid)
            self.assertEqual(check["profiles"], ["linux-main"], cid)

    def test_full_ctest_gate_registered_for_linux_main(self):
        """STD-F7 处置 2：linux-main 必须包含全量 ctest 门。"""
        gate = _CHECKS["CTEST-LINUX-FULL"]
        self.assertIn("linux-main", gate["profiles"])
        self.assertFalse(gate["waivable"])
        self.assertIn("ctest-full", gate["command"])


class TestStdF7WaivableTightening(unittest.TestCase):
    """STD-F7 处置 3：关键构建/插桩/覆盖门不可豁免。"""

    def test_key_gates_not_waivable(self):
        for cid in ("BUILD-GCC-RELEASE", "DEEP-SAN-ASAN", "DEEP-COV-CPP"):
            self.assertIn(cid, _CHECKS, cid)
            self.assertFalse(_CHECKS[cid]["waivable"], cid)


class TestStdF10PythonModulePrerequisites(unittest.TestCase):
    """STD-F10：第三方 Python 模块依赖登记与探测。"""

    def test_astropy_cross_registers_python_modules(self):
        check = _CHECKS["CTEST-P1WCS-ASTROPY-CROSS"]
        tools = check.get("prerequisite_tools", [])
        self.assertIn("python3:astropy", tools)
        self.assertIn("python3:numpy", tools)

    def test_module_probe_three_branches(self):
        from ci import run as R
        ok, reason = R.probe_prerequisite_tool("python3:json")
        self.assertTrue(ok, reason)
        ok, reason = R.probe_prerequisite_tool(
            "python3:astrocs_nonexistent_module_xyz")
        self.assertFalse(ok)
        self.assertIn("astrocs_nonexistent_module_xyz", reason)
        ok, reason = R.probe_prerequisite_tool("astrocs-no-such-exe-xyz:json")
        self.assertFalse(ok)
        self.assertIn("astrocs-no-such-exe-xyz", reason)
        # 既有纯工具名语义不变
        ok, reason = R.probe_prerequisite_tool("astrocs-no-such-exe-xyz")
        self.assertFalse(ok)
        self.assertIn("依赖工具不在 PATH", reason)

    def test_missing_module_prerequisite_fails_closed(self):
        """非 waivable 检查缺 Python 模块依赖 → FAIL(prerequisite)（经真 runner）。"""
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            H.write_registry(repo, [H.check(
                id="MOD-DEP", waivable=False,
                command=["python3", "-c", "print('ok')"],
                prerequisite_tools=["python3:astrocs_nonexistent_module_xyz"])])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast"], repo, timeout=300)
            self.assertNotEqual(proc.returncode, 0)
            result = json.loads((sorted((repo / "artifacts").rglob("CI_RESULT.json"))[-1])
                                .read_text(encoding="utf-8"))
            entry = result["checks"][0]
            self.assertEqual(entry["verdict"], "FAIL(prerequisite)")
            self.assertIn("astrocs_nonexistent_module_xyz", entry["reason"])

    def test_missing_module_prerequisite_waivable_skips_with_reason(self):
        """waivable 检查缺模块依赖 → SKIPPED(waivable) 且理由含模块名（非静默绿）。"""
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            H.write_registry(repo, [H.check(
                id="MOD-DEP-W", waivable=True,
                command=["python3", "-c", "print('ok')"],
                prerequisite_tools=["python3:astrocs_nonexistent_module_xyz"])])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast"], repo, timeout=300)
            result = json.loads((sorted((repo / "artifacts").rglob("CI_RESULT.json"))[-1])
                                .read_text(encoding="utf-8"))
            entry = result["checks"][0]
            self.assertEqual(entry["verdict"], "SKIPPED(waivable)")
            self.assertIn("astrocs_nonexistent_module_xyz", entry["reason"])


if __name__ == "__main__":
    unittest.main()
