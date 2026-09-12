# -*- coding: utf-8 -*-
"""V8-CI-005 单测：linux hosted main/deep profile（prerequisite_tools 与 deep 检查注册）。

覆盖四层：
1. prerequisite_tools 探测（fixture 仓库真跑 runner）——
   工具齐备 → 正常执行 PASS；工具缺失 + waivable → SKIPPED(waivable)（理由含工具名）；
   工具缺失 + 不可 waivable → FAIL(prerequisite)；无该字段的检查不受影响。
2. 注册表结构 —— validate_registry --strict 接受合法 prerequisite_tools、
   拒绝空数组/非字符串/非字符串数组元素；主仓库 ci/checks.json（99 项）strict PASS。
3. profile 选择（只读主仓库 plan-only）——fast=59（不含新 deep 项；含
   CI-REG-002 的 CTEST-REGISTRATION 与 CI-BASELINE-001 的
   KNOWN-FAILURES-BASELINE-VERIFY）、linux-main=90（含 BUILD-GCC-RELEASE 与
   CI-REG-002 的 CTEST-LINUX-FULL + 15 个逐目标 CTEST-*）、linux-deep=7（7 个新 id
   全选，command 自含 ci/resource_monitor.py 包裹前缀）。
4. 工具行为 —— check_complexity.py 实跑（exit 0、threshold=null、placeholder、
   ACR/legacy/third_party 排除常量在位）；deep_ci_driver.py 仓库外 --build-dir 受控报错、
   步骤超时返回 124；ci_coverage_runner.py 仓库外 --output-dir exit 2。

fixture 全部落 tempfile；只读主仓库资产，不修改任何被检文件。
"""
from __future__ import annotations

import io
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

CI_DIR = Path(__file__).resolve().parents[1]
REPO = CI_DIR.parent
sys.path.insert(0, str(CI_DIR / "tests"))

import _helpers as H  # noqa: E402

sys.path.insert(0, str(REPO))
from ci import validate_registry as VR  # noqa: E402

REGISTRY_PATH = CI_DIR / "checks.json"
_REGISTRY = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
NEW_IDS = {"BUILD-GCC-RELEASE", "DEEP-CLANG-BUILD", "DEEP-SAN-ASAN",
           "DEEP-SAN-TSAN", "DEEP-COV-CPP", "DEEP-COV-PY", "DEEP-COMPLEXITY"}
# CI-REG-002 / STD-F7 处置 1：本轮新增测试目标逐个显式登记为 CI 检查项
NEW_TEST_TARGETS = {
    "p1001_real_nodes", "p2001_real_nodes", "p2002_unc_rej_prov",
    "p3002_real_nodes", "p3002_uncertainty", "p3_projection_units",
    "p3_projection_fault", "p1wcs_apbp", "p1wcs_astropy_cross",
    "aio_abi_units", "aio_abi_negative", "aio_abi_selfcheck",
    "hips_publish_atomic_units", "hips_publish_atomic", "rt001_unique_executor",
}
MISSING_TOOL = "astrocs-cmake-nonexistent-xyz"  # 探测负例：不会存在于任何 PATH


def make_check(**overrides) -> dict:
    base = H.check(id="PREREQ-T", waivable=True, timeout_seconds=60)
    base.update(overrides)
    return base


def run_profile(repo: Path, profile: str, extra: list[str] | None = None):
    return H.run_runner(["--profile", profile, *(extra or [])], repo, timeout=300)


class TestPrerequisiteToolsProbe(unittest.TestCase):
    """prerequisite_tools 探测三分支（fixture 仓库真跑）。"""

    def _probe_result(self, check: dict, extra: dict | None = None):
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            H.write_registry(repo, [check, *([extra] if extra else [])])
            H.write_ci_result_schema(repo)
            proc = run_profile(repo, "fast")
            result = json.loads((sorted((repo / "artifacts").rglob("CI_RESULT.json"))[-1])
                                .read_text(encoding="utf-8"))
            return proc, result["checks"][0]

    def test_tool_present_executes_normally(self):
        proc, entry = self._probe_result(make_check(
            prerequisite_tools=["python3"]))
        self.assertEqual(proc.returncode, 0, proc.stderr[-400:])
        self.assertEqual(entry["verdict"], "PASS")

    def test_missing_tool_waivable_skips(self):
        # 单一 SKIPPED(waivable) 检查的 profile 总 verdict 为 FAIL（不允许空集 PASS），
        # 故伴随一个平凡 PASS 检查，断言重点在该检查自身的 verdict 与理由。
        proc, entry = self._probe_result(make_check(
            prerequisite_tools=[MISSING_TOOL]),
            extra=H.check(id="T-PASS"))
        self.assertEqual(proc.returncode, 0)
        self.assertEqual(entry["verdict"], "SKIPPED(waivable)")
        self.assertIn(MISSING_TOOL, entry["reason"])

    def test_missing_tool_not_waivable_fails_prerequisite(self):
        proc, entry = self._probe_result(make_check(
            waivable=False, prerequisite_tools=[MISSING_TOOL]))
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(entry["verdict"], "FAIL(prerequisite)")
        self.assertIn(MISSING_TOOL, entry["reason"])

    def test_without_field_unaffected(self):
        proc, entry = self._probe_result(make_check())
        self.assertEqual(entry["verdict"], "PASS")


class TestRegistryStrict(unittest.TestCase):
    """--strict 对 prerequisite_tools 的接受/拒绝。"""

    def test_valid_prerequisite_tools_pass(self):
        data = {"schema_version": 1, "checks": [make_check(
            id="PREREQ-GOOD", prerequisite_tools=["cmake", "clang"])]}
        errors, n = self._validate(data)
        self.assertEqual((errors, n), ([], 1))

    def test_empty_list_rejected(self):
        data = {"schema_version": 1, "checks": [make_check(
            id="PREREQ-EMPTY", prerequisite_tools=[])]}
        errors, _ = self._validate(data)
        self.assertTrue(any("prerequisite_tools" in e for e in errors))

    def test_non_string_element_rejected(self):
        data = {"schema_version": 1, "checks": [make_check(
            id="PREREQ-BAD", prerequisite_tools=["cmake", 7])]}
        errors, _ = self._validate(data)
        self.assertTrue(any("prerequisite_tools" in e for e in errors))

    def test_field_absent_still_valid(self):
        data = {"schema_version": 1, "checks": [make_check(id="PREREQ-NONE")]}
        errors, n = self._validate(data)
        self.assertEqual((errors, n), ([], 1))

    def test_main_registry_strict_pass_104(self):
        # V8-CI-006 注册 WIN-* 三项后 77 → 80；CI-REG-002 注册
        # CTEST-REGISTRATION + CTEST-LINUX-FULL + 15 个逐目标 CTEST-* 后 80 → 97；
        # CI-BASELINE-001 注册 KNOWN-FAILURES-BASELINE-VERIFY（fast/linux-main/
        # windows-main）与 KNOWN-FAILURES-BASELINE-CHECK（linux-main）后 97 → 99；
        # CI-001B 注册 WORKFLOW-REGISTRY-BINDING / CI-BINDING-TESTS（三 profile）、
        # LINUX-MAIN-FIXTURES / LINUX-MAIN-BUILD-TREE（linux-main）、
        # WIN-CANDIDATE-VALIDATE（windows-main）后 99 → 104。
        errors, n = VR.validate(REGISTRY_PATH, strict=True)
        self.assertEqual(errors, [])
        self.assertEqual(n, 104)

    @staticmethod
    def _validate(data: dict):
        # strict 校验 R4 要求 command[1] 为仓库内文件；-c 内联命令不在其白名单，
        # 故 fixture 一律携带 script.py（与被测的 prerequisite_tools 字段无关）。
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "checks.json"
            for c in data["checks"]:
                if c.get("command", [None, None])[1:2] == ["-c"]:
                    # R4 以主仓库根解析 command[1]，fixture 统一指真实脚本
                    c["command"] = ["python3", "ci/validate_registry.py"]
            path.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
            return VR.validate(path, strict=True)


class TestProfileSelection(unittest.TestCase):
    """只读主仓库 plan-only：三 profile 选择数与新 id 落位。"""

    @classmethod
    def plan(cls, profile: str) -> dict:
        proc = subprocess.run(
            [sys.executable, str(H.RUNNER), "--profile", profile, "--plan-only"],
            cwd=str(REPO), capture_output=True, text=True,
            encoding="utf-8", errors="replace",  # F-R2-06 接入侧：cp1252 不脆断
            timeout=120)
        assert proc.returncode == 0, proc.stderr[-400:]
        return json.loads(proc.stdout)

    def test_fast_61_excludes_deep(self):
        plan = self.plan("fast")
        ids = {c["id"] for c in plan["checks"]}
        self.assertEqual(plan["selected_count"], 61)
        self.assertFalse(ids & NEW_IDS)
        # CI-REG-002：注册闭包校验器是静态源扫描，可进 fast（无需构建树）
        self.assertIn("CTEST-REGISTRATION", ids)
        # 真跑类 CTEST-* 门只在 linux-main（需要构建树）
        self.assertFalse([i for i in ids if i.startswith("CTEST-")
                          and i != "CTEST-REGISTRATION"])

    def test_linux_main_94_includes_gcc_release_and_ctest_gates(self):
        plan = self.plan("linux-main")
        ids = {c["id"] for c in plan["checks"]}
        self.assertEqual(plan["selected_count"], 94)
        # CI-001B：workflow 侧前置步收编为注册表检查项（UT-BACKEND/UT-CLI 的前置）
        self.assertIn("LINUX-MAIN-FIXTURES", ids)
        self.assertIn("LINUX-MAIN-BUILD-TREE", ids)
        self.assertIn("BUILD-GCC-RELEASE", ids)
        self.assertFalse(ids & (NEW_IDS - {"BUILD-GCC-RELEASE"}))
        # CI-REG-002 / STD-F7 处置 1+2：全量 ctest 门 + 逐目标门，且全部不可豁免
        self.assertIn("CTEST-LINUX-FULL", ids)
        by_id = {c["id"]: c for c in plan["checks"]}
        self.assertFalse(by_id["CTEST-LINUX-FULL"]["waivable"])
        # plan-only 只输出执行所需字段（不含 ctest_targets），注册闭包按注册表断言
        registered = {t for c in _REGISTRY["checks"] for t in c.get("ctest_targets", [])}
        self.assertEqual(registered, NEW_TEST_TARGETS)

    def test_linux_deep_exactly_seven_new(self):
        plan = self.plan("linux-deep")
        ids = {c["id"] for c in plan["checks"]}
        self.assertEqual(plan["selected_count"], 7)
        self.assertEqual(ids, NEW_IDS)

    def test_heavy_commands_self_contain_monitor_prefix(self):
        plan = self.plan("linux-deep")
        for c in plan["checks"]:
            if c["requires_monitor"]:
                self.assertEqual(c["command"][:2], ["python3", "ci/resource_monitor.py"],
                                 c["id"])


class TestToolBehaviour(unittest.TestCase):
    """check_complexity / deep_ci_driver / ci_coverage_runner 行为。"""

    def test_complexity_real_run_baseline_contract(self):
        out = Path(tempfile.mkdtemp()) / "complexity.json"
        proc = H.sh(["python3", str(REPO / "tools/quality/check_complexity.py"),
                     "--paths", "cli", "--output", str(out.relative_to(REPO))
                     if False else "run/ci/ut-deep-complexity.json"],
                    cwd=REPO, timeout=120)
        self.assertEqual(proc.returncode, 0)
        report = json.loads((REPO / "run/ci/ut-deep-complexity.json")
                            .read_text(encoding="utf-8"))
        self.assertIsNone(report["threshold"])
        self.assertTrue(report["placeholder"])
        self.assertGreater(report["totals"]["files"], 0)

    def test_complexity_excludes_acr_and_third_party(self):
        from importlib.util import spec_from_file_location, module_from_spec
        spec = spec_from_file_location(
            "cxc", REPO / "tools/quality/check_complexity.py")
        mod = module_from_spec(spec)
        spec.loader.exec_module(mod)
        for part in ("lib/acr", "legacy", "third_party"):
            self.assertIn(part, mod.EXCLUDE_PARTS)

    def test_driver_rejects_outside_build_dir(self):
        proc = H.sh(["python3", str(REPO / "tools/quality/deep_ci_driver.py"),
                     "build-gcc-release", "--build-dir", "/tmp/astrocs-outside"],
                    cwd=REPO, timeout=60)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("仓库内", proc.stderr + proc.stdout)

    def test_driver_step_timeout_returns_124(self):
        from importlib.util import spec_from_file_location, module_from_spec
        spec = spec_from_file_location(
            "dcd", REPO / "tools/quality/deep_ci_driver.py")
        mod = module_from_spec(spec)
        spec.loader.exec_module(mod)
        res = mod.run_step(["sleep", "5"], timeout=1)
        self.assertEqual(res["exit_code"], 124)
        self.assertTrue(res["timed_out"])

    def test_driver_qa_sanitize_cache_flag_sanitizer_runtime(self):
        """V8-CI-012 F-D1：qa-sanitize 传 -DASTROCS_SANITIZE_RUNTIME=ON。

        CMakeLists.txt 的 qa-sanitize 自定义目标由 SANITIZE_RUNTIME option
        生成；旧行为误传 ENABLE_SANITIZERS（QA-002 旗标）→ 目标从未定义 →
        hosted "No rule to make target 'qa-sanitize'" exit 2。注入 run_step
        捕获 configure argv（不触网、不真跑 cmake、零落盘）。
        """
        from importlib.util import spec_from_file_location, module_from_spec
        spec = spec_from_file_location(
            "dcd_f1a", REPO / "tools/quality/deep_ci_driver.py")
        mod = module_from_spec(spec)
        spec.loader.exec_module(mod)
        captured: list[list[str]] = []
        with mock.patch.object(mod, "run_step",
                               side_effect=lambda argv, **kw:
                                   (captured.append(argv),
                                    {"argv": argv, "exit_code": 0,
                                     "timed_out": False, "output_tail": ""})[1]):
            args = mod.build_parser().parse_args(
                ["qa-sanitize", "--build-dir", "run/ci/build-qa-asan-ut"])
            self.assertEqual(mod.cmd_qa_sanitize(args), 0)
        configure = captured[0]
        self.assertIn("-DASTROCS_SANITIZE_RUNTIME=ON", configure)
        self.assertNotIn("-DASTROCS_ENABLE_SANITIZERS=ON", configure)
        self.assertNotIn("-DASTROCS_SANITIZE_THREAD=ON", configure)
        self.assertIn("--target", captured[-1])
        self.assertIn("qa-sanitize", captured[-1])
        self.assertNotIn("qa-sanitize-tsan", captured[-1])

    def test_driver_qa_sanitize_cache_flag_tsan_unchanged(self):
        """TSan 分支同构交叉印证：仍传 SANITIZE_THREAD=ON（基线行为不回归）。"""
        from importlib.util import spec_from_file_location, module_from_spec
        spec = spec_from_file_location(
            "dcd_f1b", REPO / "tools/quality/deep_ci_driver.py")
        mod = module_from_spec(spec)
        spec.loader.exec_module(mod)
        captured: list[list[str]] = []
        with mock.patch.object(mod, "run_step",
                               side_effect=lambda argv, **kw:
                                   (captured.append(argv),
                                    {"argv": argv, "exit_code": 0,
                                     "timed_out": False, "output_tail": ""})[1]):
            args = mod.build_parser().parse_args(
                ["qa-sanitize-tsan", "--build-dir", "run/ci/build-qa-tsan-ut"])
            self.assertEqual(mod.cmd_qa_sanitize(args), 0)
        configure = captured[0]
        self.assertIn("-DASTROCS_SANITIZE_THREAD=ON", configure)
        self.assertNotIn("-DASTROCS_SANITIZE_RUNTIME=ON", configure)
        self.assertNotIn("-DASTROCS_ENABLE_SANITIZERS=ON", configure)
        self.assertIn("qa-sanitize-tsan", captured[-1])

    def test_coverage_runner_outside_output_dir_exit_2(self):
        proc = H.sh(["python3", str(REPO / "tools/quality/ci_coverage_runner.py"),
                     "--output-dir", "/tmp/astrocs-outside-cov"],
                    cwd=REPO, timeout=120)
        self.assertEqual(proc.returncode, 2)
        self.assertIn("仓库内", proc.stderr)

    def test_coverage_runner_repo_relative_output_dir_hosted_shape(self):
        """V8-CI-012 F6：仓库相对 --output-dir（hosted 实跑形态）不再 ValueError。

        修复前：校验步把 out_dir 改写为仓库相对路径，--cov-report 组装处对
        绝对 REPO 再调 relative_to → ValueError 秒败（hosted run 实证）。
        进程内 mock subprocess（不真跑 pytest），REPO 用真实临时仓库。
        """
        from importlib.util import spec_from_file_location, module_from_spec
        spec = spec_from_file_location(
            "ccr_f6", REPO / "tools/quality/ci_coverage_runner.py")
        mod = module_from_spec(spec)
        spec.loader.exec_module(mod)
        fake = mock.Mock(returncode=0, stdout="", stderr="")
        with tempfile.TemporaryDirectory() as td:
            repo_root = Path(td) / "repo-root"
            (repo_root / "tests").mkdir(parents=True)
            with mock.patch.object(mod, "REPO", repo_root), \
                 mock.patch.object(mod.subprocess, "run", return_value=fake) as run:
                rc = mod.main(["--output-dir", "run/ci/coverage-py",
                               "--tests", "tests"])
            self.assertEqual(rc, 0)
            argv = run.call_args.args[0]
            self.assertEqual(run.call_args.kwargs["cwd"], str(repo_root))
            self.assertIn("--cov-report=xml:run/ci/coverage-py/coverage.xml", argv)
            self.assertIn("--cov-report=json:run/ci/coverage-py/coverage.json",
                          argv)
            self.assertEqual(argv[argv.index("-m") + 2], "tests")  # 收集根相对形态
            self.assertEqual(run.call_args.kwargs["env"]["COVERAGE_FILE"],
                             str(repo_root / "run/ci/coverage-py/.coverage"))

    def test_coverage_runner_repo_relative_output_dir_subprocess(self):
        """F6 端到端（hosted argv 形态，经真实仓库 run/ 承载，测试后清理）。"""
        out_rel = "run/ci/test-f6-ut-cov"
        self.addCleanup(shutil.rmtree, REPO / out_rel, ignore_errors=True)
        proc = H.sh(["python3", str(REPO / "tools/quality/ci_coverage_runner.py"),
                     "--output-dir", out_rel, "--tests", "tests-nonexistent-xyz"],
                    cwd=REPO, timeout=120)
        # 本地无 pytest → 1（No module named pytest）/ 3（解释器缺失）；
        # 有 pytest 无收集根 → 4/5；均非 2 且无未捕获异常
        self.assertIn(proc.returncode, (1, 3, 4, 5), proc.stderr[-400:])
        self.assertNotIn("ValueError", proc.stderr)
        self.assertNotIn("Traceback", proc.stderr)

    def test_coverage_runner_escape_subpath_still_controlled_exit2(self):
        """F6 邻域：书写仓库相对但 resolve 后逃逸出仓库 → 仍受控 exit 2。"""
        proc = H.sh(["python3", str(REPO / "tools/quality/ci_coverage_runner.py"),
                     "--output-dir", "../outside-cov-escape"],
                    cwd=REPO, timeout=120)
        self.assertEqual(proc.returncode, 2)
        self.assertIn("仓库内", proc.stderr)
        self.assertNotIn("Traceback", proc.stderr)
        self.assertFalse((REPO.parent / "outside-cov-escape").exists())

    def _coverage_runner_with_fake_pytest(self, returncode: int,
                                          stdout: str, stderr: str = "",
                                          tmp_tag: str = "f9a"):
        """进程内 mock pytest 子进程，返回落盘 coverage-summary.json 载荷。"""
        from importlib.util import spec_from_file_location, module_from_spec
        spec = spec_from_file_location(
            f"ccr_{tmp_tag}", REPO / "tools/quality/ci_coverage_runner.py")
        mod = module_from_spec(spec)
        spec.loader.exec_module(mod)
        fake = mock.Mock(returncode=returncode, stdout=stdout, stderr=stderr)
        with tempfile.TemporaryDirectory() as td:
            repo_root = Path(td) / "repo-root"
            (repo_root / "tests").mkdir(parents=True)
            with mock.patch.object(mod, "REPO", repo_root), \
                 mock.patch.object(mod.subprocess, "run", return_value=fake):
                rc = mod.main(["--output-dir", "run/ci/coverage-py",
                               "--tests", "tests"])
            summary = json.loads(
                (repo_root / "run/ci/coverage-py/coverage-summary.json")
                .read_text(encoding="utf-8"))
        return rc, summary

    def test_coverage_runner_failure_collects_pytest_tail(self):
        """V8-CI-012 修复轮 3（F9-a）：pytest 失败时 summary 采 pytest_tail。

        收集期 exit 2（hosted 实证形态）：stdout 含收集错误行（pytest -q
        把收集错误写 stdout）与大量噪声行；修复前 runner 只采 stderr 8 行
        且 hosted stderr 为空 → 失败模块不可见。断言：过滤行入选、纯噪声
        不入选、exit code 透传、stderr_tail 语义不变。
        """
        noise = [f"collected-noise {i}" for i in range(30)]
        err_lines = [
            "ERROR collecting tests/legacy/test_dep.py",
            "tests/legacy/test_dep.py:3: in <module>",
            "    import yaml",
            "E   ModuleNotFoundError: No module named 'yaml'",
            "========== short test summary info ==========",
            "Interrupted: 1 error during collection",
        ]
        rc, summary = self._coverage_runner_with_fake_pytest(
            2, "\n".join(noise + err_lines))
        self.assertEqual(rc, 2)
        tail = summary["pytest_tail"]
        for want in err_lines:
            self.assertIn(want, tail)
        self.assertNotIn(noise[0], tail)
        self.assertEqual(summary["pytest"]["exit_code"], 2)
        self.assertIn("stderr_tail", summary["pytest"])

    def test_coverage_runner_pytest_tail_capped_at_120(self):
        """F9-a cap（R7 合同 50→120）：错误行风暴下 pytest_tail 恰 120 行（防 hosted 日志爆炸）。"""
        storm = [f"ERROR tests/t{i}.py" for i in range(200)]
        rc, summary = self._coverage_runner_with_fake_pytest(
            2, "\n".join(storm), tmp_tag="f9a_cap")
        self.assertEqual(rc, 2)
        self.assertEqual(len(summary["pytest_tail"]), 120)
        self.assertEqual(summary["pytest_tail"][0], storm[0])  # 保序取前 120

    def test_coverage_runner_pytest_tail_fallback_on_unfiltered(self):
        """F9-a 退化：失败但无关键词命中 → 原始尾窗兜底（永不为空黑箱）。"""
        plain = [f"plain-line-{i}" for i in range(30)]
        rc, summary = self._coverage_runner_with_fake_pytest(
            4, "\n".join(plain), tmp_tag="f9a_fb")
        self.assertEqual(rc, 4)
        self.assertEqual(summary["pytest_tail"], plain[-50:])

    def test_coverage_runner_success_has_empty_pytest_tail(self):
        """F9-a 回归：pytest 全过（rc=0）→ pytest_tail 空（不噪声化成功路径）。"""
        rc, summary = self._coverage_runner_with_fake_pytest(
            0, "5 passed in 0.1s", tmp_tag="f9a_ok")
        self.assertEqual(rc, 0)
        self.assertEqual(summary["pytest_tail"], [])


class TestCoverageCppDriver(unittest.TestCase):
    """V8-CI-012 修复轮 2：COV-CPP ctest 失败仍产出 coverage 报告（verdict 仍 FAIL）。

    修复前：cmd_coverage_cpp 在 ccov-target（内含 ctest）非零时早退，
    qa_coverage_report.sh（profdata merge + llvm-cov export）与 profraw
    归集均被跳过 → 覆盖率数值永不产出（轮 2 hosted 实证 Error 8 阻断）。
    """

    def _coverage_cmd(self, mod, results: dict[str, int]):
        """构造 args + mock run_step（按步骤名回放退出码）；返回 (captured, out)。"""
        build_dir = REPO / "run/ci/test-covcpp-ut"
        out_dir = REPO / "run/ci/test-covcpp-ut-out"
        self.addCleanup(shutil.rmtree, build_dir, ignore_errors=True)
        self.addCleanup(shutil.rmtree, out_dir, ignore_errors=True)
        cov_dir = build_dir / "coverage"
        cov_dir.mkdir(parents=True, exist_ok=True)
        (cov_dir / "42-1234.profraw").write_bytes(b"fake-profraw")
        (cov_dir / "astrocs.profdata").write_bytes(b"fake-profdata")
        (cov_dir / "coverage.json").write_text("{}", encoding="utf-8")
        (cov_dir / "unrelated.txt").write_text("skip", encoding="utf-8")

        def step_name(argv: list[str]) -> str:
            if argv[0] == "/bin/sh":
                return "coverage-merge-report"
            if "--target" in argv:
                return "ccov-target"
            if "-S" in argv:
                return "cmake-configure"
            return "cmake-build"

        def fake_run_step(argv, **kw):
            name = step_name(argv)
            code = results.get(name, 0)
            return {"argv": argv, "exit_code": code, "timed_out": False,
                    "output_tail": f"{name} rc={code}"}

        captured: list[list[str]] = []
        out = io.StringIO()
        with mock.patch.object(mod, "run_step",
                               side_effect=lambda argv, **kw:
                                   (captured.append(argv),
                                    fake_run_step(argv, **kw))[1]), \
             mock.patch("sys.stdout", out):
            args = mod.build_parser().parse_args(
                ["coverage-cpp", "--build-dir", "run/ci/test-covcpp-ut",
                 "--output-dir", "run/ci/test-covcpp-ut-out"])
            rc = mod.cmd_coverage_cpp(args)
        return captured, out.getvalue(), rc, cov_dir, out_dir

    def _load_driver_mod(self):
        from importlib.util import spec_from_file_location, module_from_spec
        spec = spec_from_file_location(
            "dcd_covcpp", REPO / "tools/quality/deep_ci_driver.py")
        mod = module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod

    def test_ctest_failure_still_runs_merge_report_and_collects(self):
        """ccov-target 失败（ctest Error 8）→ merge/report 照跑 + 产物归集 + rc=8。"""
        mod = self._load_driver_mod()
        captured, out, rc, cov_dir, out_dir = self._coverage_cmd(
            mod, {"cmake-configure": 0, "cmake-build": 0, "ccov-target": 8,
                  "coverage-merge-report": 0})
        self.assertEqual(rc, 8)                     # 首个失败步骤退出码 = FAIL
        names = [argv for argv in captured]
        merge = [a for a in names if a[0] == "/bin/sh"]
        self.assertEqual(len(merge), 1)             # 失败后 merge/report 仍执行
        self.assertIn("qa_coverage_report.sh", merge[0][1])
        payload = json.loads(out.splitlines()[-1])
        self.assertEqual(payload["verdict"], "FAIL")
        copied = set(payload["copied_outputs"])
        self.assertIn("run/ci/test-covcpp-ut-out/astrocs.profdata", copied)
        self.assertIn("run/ci/test-covcpp-ut-out/coverage.json", copied)
        self.assertIn("run/ci/test-covcpp-ut-out/42-1234.profraw", copied)
        self.assertNotIn("run/ci/test-covcpp-ut-out/unrelated.txt", copied)
        self.assertTrue((out_dir / "coverage.json").exists())

    def test_all_pass_verdict_pass_and_rc0(self):
        """全步骤成功 → rc 0 / verdict PASS（回归：正向路径不破坏）。"""
        mod = self._load_driver_mod()
        captured, out, rc, _, out_dir = self._coverage_cmd(mod, {})
        self.assertEqual(rc, 0)
        payload = json.loads(out.splitlines()[-1])
        self.assertEqual(payload["verdict"], "PASS")
        self.assertEqual(len(captured), 4)          # configure/build/ccov/merge
        self.assertTrue((out_dir / "astrocs.profdata").exists())

    def test_merge_report_failure_surfaced(self):
        """merge/report 自身失败 → 以其退出码上报（报告不可得必须如实失败）。"""
        mod = self._load_driver_mod()
        captured, out, rc, _, _ = self._coverage_cmd(
            mod, {"cmake-configure": 0, "cmake-build": 0, "ccov-target": 0,
                  "coverage-merge-report": 1})
        self.assertEqual(rc, 1)
        payload = json.loads(out.splitlines()[-1])
        self.assertEqual(payload["verdict"], "FAIL")

    def test_coverage_cpp_configure_forces_clang_toolchain(self):
        """V8-CI-012 修复轮 3（F8）：coverage-cpp configure 显式 clang。

        旧行为不指定编译器 → hosted 默认 GNU cc → clang 专属
        -fprofile-instr-generate/-fcoverage-mapping 无插桩语义 →
        profraw 零产出 → C++ 覆盖率数值缺位（轮 3 hosted 实证 C compiler
        identification is GNU）。与 sanitizer 分支同构：注入 run_step 捕获
        configure argv（不触网、不真跑 cmake、零落盘）断言旗标存在且
        SANITIZE/SANITIZER 旗标不误入。
        """
        mod = self._load_driver_mod()
        captured, _, rc, _, _ = self._coverage_cmd(
            mod, {"cmake-configure": 0, "cmake-build": 0, "ccov-target": 0,
                  "coverage-merge-report": 0})
        self.assertEqual(rc, 0)
        configure = next(a for a in captured if "-S" in a)
        self.assertIn("-DCMAKE_C_COMPILER=clang", configure)
        self.assertIn("-DCMAKE_CXX_COMPILER=clang++", configure)
        self.assertIn("-DASTROCS_BUILD_COVERAGE=ON", configure)
        self.assertNotIn("-DASTROCS_SANITIZE_RUNTIME=ON", configure)
        self.assertNotIn("-DASTROCS_SANITIZE_THREAD=ON", configure)
        self.assertNotIn("-DASTROCS_ENABLE_SANITIZERS=ON", configure)


class TestDeepCoverageToolsInstall(unittest.TestCase):
    """V8-CI-012 F-D3：ci-linux.yml deep 路径 coverage 工具安装步（纯文本断言）。"""

    def test_install_step_gated_on_deep_profile(self):
        yml = (REPO / ".github" / "workflows" / "ci-linux.yml").read_text(
            encoding="utf-8")
        self.assertIn("Install deep coverage tools", yml)
        # 仅 linux-deep 生效：main push 不安装（时长与镜像漂移风险最小化）
        self.assertIn(
            "if: steps.profile.outputs.profile == 'linux-deep'", yml)
        # 安装步必须位于 Select profile 之后、Run registered checks 之前
        self.assertLess(yml.index("Select profile"),
                        yml.index("Install deep coverage tools"))
        self.assertLess(yml.index("Install deep coverage tools"),
                        yml.index("Run registered checks"))
        # llvm-18 无后缀二进制落点必须进 GITHUB_PATH（prerequisite 探测按名字）
        self.assertIn('/usr/lib/llvm-18/bin" >> "$GITHUB_PATH"', yml)
        # 工具清单与 policy 登记一致（llvm-18 + pytest + pytest-cov 插件）
        self.assertIn("llvm-18 python3-pytest python3-pytest-cov", yml)
        # apt 双超时保护（update/install 各自 timeout；install 命令折行）
        self.assertRegex(yml, r"timeout 240 sudo apt-get update")
        self.assertIn("timeout 480 sudo DEBIAN_FRONTEND=noninteractive", yml)
        self.assertIn("apt-get install -y -qq --no-install-recommends", yml)
        # 凭据纪律：不出现 token 字面量
        self.assertNotIn("github_pat", yml)

    def test_policy_registers_deep_coverage_tools(self):
        policy = json.loads(
            (CI_DIR / "toolchain.policy.json").read_text(encoding="utf-8"))
        section = policy["linux_hosted"]["deep_coverage_tools"]
        self.assertEqual(section["apt_packages"],
                         ["llvm-18", "python3-pytest", "python3-pytest-cov",
                          "python3-numpy", "python3-astropy", "python3-scipy",
                          "python3-yaml", "libgsl-dev"])
        self.assertEqual(section["path_tools"],
                         ["llvm-profdata", "llvm-cov", "pytest"])
        self.assertEqual(section["module_tools"], ["pytest-cov"])
        # 与 checks.json prerequisite_tools 对应（纯文本字段读，无网络）
        checks = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
        by_id = {c["id"]: c.get("prerequisite_tools") for c in checks["checks"]}
        self.assertEqual(by_id["DEEP-COV-CPP"],
                         ["cmake", "llvm-profdata", "llvm-cov"])
        self.assertEqual(by_id["DEEP-COV-PY"], ["pytest"])


if __name__ == "__main__":
    unittest.main()
