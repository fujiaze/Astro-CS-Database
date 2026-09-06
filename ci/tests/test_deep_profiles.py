# -*- coding: utf-8 -*-
"""V8-CI-005 单测：linux hosted main/deep profile（prerequisite_tools 与 deep 检查注册）。

覆盖四层：
1. prerequisite_tools 探测（fixture 仓库真跑 runner）——
   工具齐备 → 正常执行 PASS；工具缺失 + waivable → SKIPPED(waivable)（理由含工具名）；
   工具缺失 + 不可 waivable → FAIL(prerequisite)；无该字段的检查不受影响。
2. 注册表结构 —— validate_registry --strict 接受合法 prerequisite_tools、
   拒绝空数组/非字符串/非字符串数组元素；主仓库 ci/checks.json（80 项）strict PASS。
3. profile 选择（只读主仓库 plan-only）——fast=57（不含新 deep 项）、
   linux-main=71（含 BUILD-GCC-RELEASE）、linux-deep=7（7 个新 id 全选，
   command 自含 ci/resource_monitor.py 包裹前缀）。
4. 工具行为 —— check_complexity.py 实跑（exit 0、threshold=null、placeholder、
   ACR/legacy/third_party 排除常量在位）；deep_ci_driver.py 仓库外 --build-dir 受控报错、
   步骤超时返回 124；ci_coverage_runner.py 仓库外 --output-dir exit 2。

fixture 全部落 tempfile；只读主仓库资产，不修改任何被检文件。
"""
from __future__ import annotations

import json
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
NEW_IDS = {"BUILD-GCC-RELEASE", "DEEP-CLANG-BUILD", "DEEP-SAN-ASAN",
           "DEEP-SAN-TSAN", "DEEP-COV-CPP", "DEEP-COV-PY", "DEEP-COMPLEXITY"}
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

    def test_main_registry_strict_pass_80(self):
        # V8-CI-006 注册 WIN-BUILD-RELEASE / WIN-TEST-UNIT /
        # WIN-PACKAGE-CANDIDATE 三项 windows-main 检查后 77 → 80。
        errors, n = VR.validate(REGISTRY_PATH, strict=True)
        self.assertEqual(errors, [])
        self.assertEqual(n, 80)

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

    def test_fast_unchanged_57_excludes_deep(self):
        plan = self.plan("fast")
        ids = {c["id"] for c in plan["checks"]}
        self.assertEqual(plan["selected_count"], 57)
        self.assertFalse(ids & NEW_IDS)

    def test_linux_main_71_includes_gcc_release(self):
        plan = self.plan("linux-main")
        ids = {c["id"] for c in plan["checks"]}
        self.assertEqual(plan["selected_count"], 71)
        self.assertIn("BUILD-GCC-RELEASE", ids)
        self.assertFalse(ids & (NEW_IDS - {"BUILD-GCC-RELEASE"}))

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
                         ["llvm-18", "python3-pytest", "python3-pytest-cov"])
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
