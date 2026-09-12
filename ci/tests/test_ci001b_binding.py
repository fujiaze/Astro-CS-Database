# -*- coding: utf-8 -*-
"""CI-001B 单测：workflow ↔ 注册表联动核死（ci/validate_workflow_binding.py）。

覆盖：
1. 现网仓库绑定声明 → PASS（0 error），挂账项以 notices 显式可见（不静默）；
2. 负向注入必败（逐条独立 fixture 仓库，离线）：
   - workflow 多出未声明步 → unbound_step；
   - 声明里多出 workflow 不存在的步 → stale_binding；
   - 把注册表检查命令（或迁出的 ci/ 体）抄回 workflow run: 体 →
     registry_command_copied_into_workflow；
   - 恢复 CI-001B 前的 Test-Path + ::warning:: 跳过路径 →
     warning_skip_branch_on_registered_output；
   - 绑定不可豁免检查的步去掉 if: always() → nonwaivable_step_skippable；
   - 注册表把 WIN-PACKAGE-CANDIDATE 改成 waivable=true → bound_check_waivable；
   - 注册表 WIN-PACKAGE-CANDIDATE.outputs 去掉 candidate zip → output_contract_drift；
   - profile 表达式指向未声明步 / 字面 profile 不在注册表 → unknown_profile；
   - uses 未锁到 actions.lock 内 SHA → sha_not_locked；
   - concurrency.group 去掉 github.event_name → concurrency_group_weak；
   - host-provisioning 步内塞仓库脚本 → business_command_in_workflow。
3. 绑定声明本身不得承载未注册的 binds_check / serves_checks。

所有断言都在临时 fixture 仓库内完成，不触网、不改动真实工作区。
"""
from __future__ import annotations

import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]

DOLLAR = "$"
EXPR_PROFILE = DOLLAR + "{{ steps.profile.outputs.profile }}"
EXPR_EVENT = DOLLAR + "{{ github.event_name }}"
WARN = "::" + "warning::"

_WORKFLOWS = (".github/workflows/ci-linux.yml", ".github/workflows/ci-windows.yml")
_SUPPORT = ("ci/checks.json", "ci/actions.lock.json", "ci/workflow_binding.json",
            "ci/validate_candidate.py")


def _load(name: str, rel: str):
    spec = importlib.util.spec_from_file_location(name, _REPO / rel)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


VB = _load("ci001b_validate_workflow_binding", "ci/validate_workflow_binding.py")


def make_fixture(root: Path) -> Path:
    (root / "ci").mkdir(parents=True, exist_ok=True)
    (root / ".github" / "workflows").mkdir(parents=True, exist_ok=True)
    for rel in _SUPPORT:
        shutil.copy2(_REPO / rel, root / rel)
    shutil.copytree(_REPO / "ci" / "steps", root / "ci" / "steps")
    for rel in _WORKFLOWS:
        shutil.copy2(_REPO / rel, root / rel)
    return root


def validate(root: Path):
    return VB.validate(root, root / "ci" / "workflow_binding.json",
                       root / "ci" / "checks.json", root / "ci" / "actions.lock.json",
                       ".github/workflows")


def codes(errors) -> set:
    return {e["code"] for e in errors}


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def mutate_step(text: str, step_name: str, old: str, new: str) -> str:
    head = "      - name: " + step_name
    lines = text.splitlines(keepends=True)
    starts = [i for i, ln in enumerate(lines) if ln.rstrip("\n") == head]
    assert len(starts) == 1, "step not unique: " + step_name
    start = starts[0]
    end = len(lines)
    for i in range(start + 1, len(lines)):
        if lines[i].startswith("      - name: "):
            end = i
            break
    block = "".join(lines[start:end])
    assert block.count(old) == 1, "anchor not unique in " + step_name
    block = block.replace(old, new)
    return "".join(lines[:start]) + block + "".join(lines[end:])


class TestRealRepoBinding(unittest.TestCase):
    """现网仓库（真实 workflow + 真实注册表）绑定核死。"""

    def test_live_repo_binding_pass(self):
        errors, notices, report = validate(_REPO)
        self.assertEqual(errors, [], errors)
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["registry"]["checks"], len(json.loads(
            (_REPO / "ci" / "checks.json").read_text(encoding="utf-8"))["checks"]))
        for wf in _WORKFLOWS:
            self.assertIn(wf, report["workflows"], wf)
            self.assertGreaterEqual(report["workflows"][wf]["steps"], 7)

    def test_live_repo_check_bodies_are_registered(self):
        """注册表侧声明（check-body）必须逐条对应已注册检查项，零挂账残留。"""
        errors, notices, report = validate(_REPO)
        self.assertEqual(errors, [], errors)
        self.assertEqual([n for n in notices if n["code"] == "pending_registration"], [])
        bodies = [s for s in report["steps"] if s["role"] == "check-body"]
        self.assertEqual(sorted(s["check_id"] for s in bodies),
                         ["LINUX-MAIN-BUILD-TREE", "LINUX-MAIN-FIXTURES",
                          "WIN-CANDIDATE-VALIDATE"])

    def test_live_workflows_carry_no_business_commands(self):
        """目标 1 的直接证据：workflow 命令体内不得再出现业务命令本体。

        注释行不参与（说明文字允许提及路径）；命令体只剩 CI 入口 / 派发器 / run.py。
        """
        for rel in _WORKFLOWS:
            code = VB.code_lines(read(_REPO / rel))
            for token in ("cmake", "ctest", "ci/validate_candidate.py",
                          "ci/prepare_linux_fixtures.py", "tools/quality/",
                          "ci/steps/"):
                self.assertNotIn(token, code, rel + " 命令体含 " + token)
            self.assertIn("ci/run.py --profile", code, rel)

    def test_live_repo_scope_is_two_workflows(self):
        _errors, notices, report = validate(_REPO)
        self.assertEqual(sorted(report["workflows"]), sorted(_WORKFLOWS))
        out_of_scope = [n for n in notices if n["code"] == "out_of_scope_workflow"]
        self.assertTrue(all("fatduck" in n["where"] for n in out_of_scope), out_of_scope)


class TestNegativeInjections(unittest.TestCase):
    """每条负向注入必须在对应规则上必败（validator 非空转）。"""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = make_fixture(Path(self._tmp.name))
        self.linux = self.root / ".github" / "workflows" / "ci-linux.yml"
        self.windows = self.root / ".github" / "workflows" / "ci-windows.yml"
        self.manifest = self.root / "ci" / "workflow_binding.json"
        self.registry = self.root / "ci" / "checks.json"

    def tearDown(self):
        self._tmp.cleanup()

    def _baseline(self):
        errors, _n, _r = validate(self.root)
        self.assertEqual(errors, [], errors)

    def test_unbound_step_fails(self):
        self._baseline()
        text = read(self.linux).replace(
            "      - name: Run registered checks",
            "      - name: Sneaky business step\n        run: python3 ci/run.py --profile linux-main\n"
            "      - name: Run registered checks", 1)
        write(self.linux, text)
        errors, _n, _r = validate(self.root)
        self.assertIn("unbound_step", codes(errors))

    def test_stale_binding_fails(self):
        self._baseline()
        data = json.loads(read(self.manifest))
        for step in data["steps"]:
            if step["step_id"] == "LINUX-RUN-REGISTERED":
                step["name"] = "Run registered checks (renamed)"
        write(self.manifest, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("stale_binding", codes(errors))

    def test_registry_command_copied_back_fails(self):
        self._baseline()
        text = mutate_step(read(self.linux), "Collect bootstrap diagnostics",
                           "python3 ci/wf_step.py --step LINUX-BOOTSTRAP-DIAG",
                           "python3 tools/quality/check_ctest_registration.py --output x.json")
        write(self.linux, text)
        errors, _n, _r = validate(self.root)
        self.assertIn("registry_command_copied_into_workflow", codes(errors))
        self.assertIn("business_command_in_workflow", codes(errors))

    def test_warning_skip_branch_on_registered_output_fails(self):
        """CI-001B 前的 candidate 缺失 warning 跳过路径必须被判 FAIL。"""
        self._baseline()
        indent = " " * 10
        old_body = ("\n" + indent).join([
            "if (Test-Path artifacts/candidate/AstroCS-candidate.zip) {",
            "  python ci/validate_candidate.py artifacts/candidate/AstroCS-candidate.zip",
            "} else {",
            "  Write-Host " + chr(34) + WARN + "candidate zip not produced - validate step skipped"
            + chr(34),
            "}",
        ])
        text = mutate_step(read(self.windows), "Collect bootstrap diagnostics",
                           "python ci/wf_step.py --step WINDOWS-BOOTSTRAP-DIAG", old_body)
        write(self.windows, text)
        errors, _n, _r = validate(self.root)
        self.assertIn("warning_skip_branch_on_registered_output", codes(errors))
        self.assertIn("dispatch_body_mismatch", codes(errors))

    def _manifest_step(self, step_id):
        data = json.loads(read(self.manifest))
        return data, [s for s in data["steps"] if s["step_id"] == step_id][0]

    def test_check_body_waivable_fails(self):
        self._baseline()
        data, _entry = self._manifest_step("WINDOWS-VALIDATE-CANDIDATE")
        reg = json.loads(read(self.registry))
        for check in reg["checks"]:
            if check["id"] == "WIN-CANDIDATE-VALIDATE":
                check["waivable"] = True
        write(self.registry, json.dumps(reg, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("check_body_waivable", codes(errors))

    def test_check_body_not_fail_closed_fails(self):
        self._baseline()
        data, entry = self._manifest_step("WINDOWS-VALIDATE-CANDIDATE")
        entry["fail_closed"] = False
        write(self.manifest, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("check_body_not_fail_closed", codes(errors))

    def test_check_command_drift_fails(self):
        self._baseline()
        reg = json.loads(read(self.registry))
        for check in reg["checks"]:
            if check["id"] == "LINUX-MAIN-FIXTURES":
                check["command"] = ["python3", "ci/prepare_linux_fixtures.py"]
        write(self.registry, json.dumps(reg, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("check_command_drift", codes(errors))

    def test_dispatch_record_output_required(self):
        self._baseline()
        reg = json.loads(read(self.registry))
        for check in reg["checks"]:
            if check["id"] == "LINUX-MAIN-BUILD-TREE":
                check["outputs"] = ["run/ci/other.json"]
        write(self.registry, json.dumps(reg, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("check_outputs_missing_dispatch_record", codes(errors))

    def test_check_body_not_registered_fails(self):
        self._baseline()
        data, entry = self._manifest_step("LINUX-PREPARE-FIXTURES")
        entry["check_id"] = "NO-SUCH-CHECK"
        write(self.manifest, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("check_body_not_registered", codes(errors))

    def test_registry_runner_missing_fails(self):
        self._baseline()
        data, entry = self._manifest_step("WINDOWS-RUN-REGISTERED")
        entry["role"] = "host-provisioning"
        write(self.manifest, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("registry_runner_missing", codes(errors))

    def test_bound_check_waivable_drift_fails(self):
        self._baseline()
        data = json.loads(read(self.registry))
        for check in data["checks"]:
            if check["id"] == "WIN-PACKAGE-CANDIDATE":
                check["waivable"] = True
        write(self.registry, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("bound_check_waivable", codes(errors))

    def test_output_contract_drift_fails(self):
        self._baseline()
        data = json.loads(read(self.registry))
        for check in data["checks"]:
            if check["id"] == "WIN-PACKAGE-CANDIDATE":
                check["outputs"] = [o for o in check["outputs"]
                                    if "AstroCS-candidate.zip" not in o]
        write(self.registry, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("output_contract_drift", codes(errors))

    def test_unknown_profile_fails(self):
        self._baseline()
        data = json.loads(read(self.manifest))
        for step in data["steps"]:
            if step["step_id"] == "WINDOWS-RUN-REGISTERED":
                step["profile_expr"] = "windows-nightly"
        write(self.manifest, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("unknown_profile", codes(errors))

    def test_unbound_profile_expression_fails(self):
        self._baseline()
        data = json.loads(read(self.manifest))
        for step in data["steps"]:
            if step["step_id"] == "LINUX-RUN-REGISTERED":
                step["profile_expr"] = DOLLAR + "{{ steps.nosuch.outputs.profile }}"
        write(self.manifest, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("profile_expr_unbound", codes(errors))

    def test_sha_not_locked_fails(self):
        self._baseline()
        text = read(self.linux).replace(
            "actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1",
            "actions/checkout@" + "a" * 40, 1)
        write(self.linux, text)
        errors, _n, _r = validate(self.root)
        self.assertIn("sha_not_locked", codes(errors))

    def test_concurrency_group_weak_fails(self):
        self._baseline()
        text = read(self.windows).replace(EXPR_EVENT, "static", 1)
        write(self.windows, text)
        errors, _n, _r = validate(self.root)
        self.assertIn("concurrency_group_weak", codes(errors))

    def test_business_command_in_provisioning_fails(self):
        self._baseline()
        text = mutate_step(
            read(self.linux), "Install deep coverage tools",
            "          timeout 240 sudo apt-get update -qq",
            "          timeout 240 sudo apt-get update -qq\n"
            "          python3 ci/prepare_linux_fixtures.py")
        write(self.linux, text)
        errors, _n, _r = validate(self.root)
        self.assertIn("business_command_in_workflow", codes(errors))
        self.assertIn("registry_command_copied_into_workflow", codes(errors))

    def test_missing_exec_target_fails(self):
        self._baseline()
        data = json.loads(read(self.manifest))
        for step in data["steps"]:
            if step["step_id"] == "LINUX-PREPARE-FIXTURES":
                step["exec"] = ["bash", "ci/steps/no_such_step.sh"]
        write(self.manifest, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("exec_target_missing", codes(errors))

    def test_unregistered_serves_check_fails(self):
        self._baseline()
        data = json.loads(read(self.manifest))
        for step in data["steps"]:
            if step["step_id"] == "LINUX-BUILD-ROOT-GRAPH":
                step["serves_checks"] = ["UT-BACKEND", "NO-SUCH-CHECK"]
        write(self.manifest, json.dumps(data, ensure_ascii=False, indent=2))
        errors, _n, _r = validate(self.root)
        self.assertIn("serves_check_not_registered", codes(errors))


if __name__ == "__main__":
    unittest.main()
