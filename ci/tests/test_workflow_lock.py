# -*- coding: utf-8 -*-
"""V8-CI-007 单测：Workflow 与 action 锁（全程离线，Linux 宿主可全绿）。

覆盖：
1. actions.lock 结构校验（合法/短 SHA/重复条目/缺字段 -> LockInvalid）；
2. workflow YAML —— 可解析、runs-on/permissions/concurrency/timeout 形状、
   触发器（push main + dispatch choice + schedule cron）、if: always() 上传
   （路径=run.py 输出目录 artifacts/ci/）、if: failure() bootstrap 诊断步
   （BOOTSTRAP_DIAG.json）、无未替换占位符（__XXX_FULL_SHA__ 等）、
   uses 全部命中锁内完整 SHA、无算法命令/科学参数字样；
3. verify_actions_lock --offline 结构检查（真实锁 exit 0；坏结构 exit 3）；
4. bootstrap --json —— 真实 policy 在本地宿主（缺 gcc-14 属预期）-> exit 2 +
   结构化 stderr + items[] 形状 + observed_from 透传（policy 驱动 cmake
   下限/clang-18 期望）；mock 探测全通过 -> PASS；policy 缺失 exit 2；
   mock 单项版本不足 -> exit 2 且该 item ok=False；
5. select_profile 三事件规则 + dispatch 白名单 + 未知事件/空请求 exit 2 +
   GITHUB_OUTPUT 兼容单行输出 + --json 形状；
6. validate_candidate —— 最小合法 zip exit 0；缺清单/SHA256SUMS 哈希不一致/
   源码入 zip/build cache 入 zip/SOURCE_MANIFEST testdata 路径/缺溯源字段/
   未登记 zip 成员，各拒绝路径逐一断言；
7. 配置计数回归 —— fast 57 / linux-main 71 / linux-deep 7 / windows-main 61
   （经 plan-only selected_count 实测）+ registry strict 校验不受影响。
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import yaml

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

from ci.tests import _helpers as H  # noqa: E402

_WF_DIR = _REPO / ".github" / "workflows"
_LOCK_PATH = _REPO / "ci" / "actions.lock.json"
_POLICY_PATH = _REPO / "ci" / "toolchain.policy.json"


def _load_module(name: str, relpath: str):
    spec = importlib.util.spec_from_file_location(name, _REPO / relpath)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


VAL = _load_module("verify_actions_lock", "ci/verify_actions_lock.py")
BS = _load_module("bootstrap", "ci/bootstrap.py")
SP = _load_module("select_profile", "ci/select_profile.py")
VC = _load_module("validate_candidate", "ci/validate_candidate.py")


def run_script(script: Path, *args: str, timeout: float = 120):
    """带 timeout 跑仓库内脚本（纪律：外部命令全部 timeout）。"""
    return subprocess.run(
        [sys.executable, str(script), *args], cwd=str(_REPO),
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=timeout)  # F-R2-06 接入侧：显式 UTF-8 解码（cp1252 不脆断）


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# ---------------------------------------------------------------- 锁与 YAML ----

class TestActionsLockStructure(unittest.TestCase):
    """ci/actions.lock.json 结构校验（VAL.load_lock）。"""

    def test_real_lock_loads_valid(self):
        lock = VAL.load_lock(_LOCK_PATH)
        self.assertGreaterEqual(len(lock["entries"]), 2)
        for entry in lock["entries"]:
            self.assertRegex(entry["commit_sha"], r"^[0-9a-f]{40}$")
            self.assertRegex(entry["tag"], r"^v\d")

    def test_rejects_short_sha(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "lock.json"
            bad.write_text(json.dumps({
                "schema_version": 1, "entries": [
                    {"action": "actions/checkout", "tag": "v7.0.1",
                     "commit_sha": "3d3c42e5", "verified_utc": "2026-01-01T00:00:00Z"},
                ]}), encoding="utf-8")
            with self.assertRaises(VAL.LockInvalid):
                VAL.load_lock(bad)

    def test_rejects_duplicate_entry(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "lock.json"
            entry = {"action": "actions/checkout", "tag": "v7.0.1",
                     "commit_sha": "a" * 40, "verified_utc": "2026-01-01T00:00:00Z"}
            bad.write_text(json.dumps(
                {"schema_version": 1, "entries": [entry, dict(entry)]}),
                encoding="utf-8")
            with self.assertRaises(VAL.LockInvalid):
                VAL.load_lock(bad)

    def test_rejects_missing_field(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "lock.json"
            bad.write_text(json.dumps({
                "schema_version": 1, "entries": [
                    {"action": "actions/checkout", "tag": "v7.0.1",
                     "commit_sha": "a" * 40},  # 缺 verified_utc
                ]}), encoding="utf-8")
            with self.assertRaises(VAL.LockInvalid):
                VAL.load_lock(bad)


class TestWorkflowYaml(unittest.TestCase):
    """两份 workflow 的 YAML 形状、占位符、锁定 SHA、安全属性。"""

    @classmethod
    def setUpClass(cls):
        cls.docs = {name: yaml.safe_load((_WF_DIR / name).read_text(encoding="utf-8"))
                    for name in ("ci-linux.yml", "ci-windows.yml")}

    def test_yaml_parse_and_job_shape(self):
        for name, doc in self.docs.items():
            job = doc["jobs"][{"ci-linux.yml": "linux",
                               "ci-windows.yml": "windows"}[name]]
            self.assertEqual(len(doc["jobs"]), 1, name)
            self.assertIn(job["runs-on"], ("ubuntu-24.04", "windows-2022"), name)
            self.assertEqual(job["timeout-minutes"], 330, name)
            self.assertEqual(job["steps"][0]["uses"].split("@")[-1].split()[0],
                             VAL.load_lock(_LOCK_PATH)["entries"][0]["commit_sha"])

    def test_no_unresolved_placeholders(self):
        for name, text in ((n, (_WF_DIR / n).read_text(encoding="utf-8"))
                           for n in self.docs):
            self.assertIsNone(
                re.search(r"__[A-Z0-9_]+__", text), f"{name} 有未替换占位符")

    def test_uses_pinned_to_locked_full_sha(self):
        lock_shas = {e["commit_sha"] for e in VAL.load_lock(_LOCK_PATH)["entries"]}
        for name, doc in self.docs.items():
            for step in doc["jobs"][{"ci-linux.yml": "linux",
                                     "ci-windows.yml": "windows"}[name]]["steps"]:
                uses = step.get("uses")
                if not uses:
                    continue
                sha = uses.split("@", 1)[1].split()[0]
                self.assertRegex(sha, r"^[0-9a-f]{40}$",
                                 f"{name}: {uses} 未锁完整 SHA")
                self.assertIn(sha, lock_shas, f"{name}: {uses} 不在 actions.lock")

    def test_permissions_contents_read_and_concurrency(self):
        for name, doc in self.docs.items():
            self.assertEqual(doc["permissions"], {"contents": "read"}, name)
            self.assertFalse(doc["concurrency"]["cancel-in-progress"], name)

    def test_linux_triggers(self):
        doc = self.docs["ci-linux.yml"]
        on = doc[True] if True in doc else doc["on"]
        self.assertEqual(on["push"]["branches"], ["main"])
        self.assertEqual(on["workflow_dispatch"]["inputs"]["profile"]["options"],
                         ["linux-main", "linux-deep"])
        self.assertEqual(len(on["schedule"]), 1)
        self.assertRegex(on["schedule"][0]["cron"], r"^\S+ \S+ \S+ \S+ \S+$")

    def test_windows_candidate_gate_is_registry_bound(self):
        """CI-001B 目标 2：候选门收编为不可豁免注册表检查项（STD-F8 残留项收口）。

        旧形态（workflow 内 Test-Path + ::warning:: 跳过）已删除：候选缺失现在
        由 ci/checks.json 的 WIN-CANDIDATE-VALIDATE（waivable=false）在
        ci/run.py 内 FAIL，且 workflow 体内不再承载任何业务命令。
        """
        doc = self.docs["ci-windows.yml"]
        steps = doc["jobs"]["windows"]["steps"]
        self.assertEqual([s for s in steps if s.get("name") == "Validate candidate"], [])
        for step in steps:
            body = str(step.get("run") or "")
            # 旧失败模式：存在性探针 + warning 跳过（候选缺失被静默吞掉）
            if "AstroCS-candidate.zip" in body:
                self.assertNotIn("::warning::", body, step.get("name"))
                self.assertNotIn("Test-Path", body, step.get("name"))
        registry = json.loads((_REPO / "ci" / "checks.json").read_text(encoding="utf-8"))
        gate = [c for c in registry["checks"] if c["id"] == "WIN-CANDIDATE-VALIDATE"]
        self.assertEqual(len(gate), 1)
        self.assertIs(gate[0]["waivable"], False)
        self.assertIn("windows-main", gate[0]["profiles"])
        self.assertEqual(gate[0]["command"],
                         ["python3", "ci/wf_step.py", "--step", "WINDOWS-VALIDATE-CANDIDATE"])
        binding = json.loads((_REPO / "ci" / "workflow_binding.json").read_text(encoding="utf-8"))
        entry = [s for s in binding["steps"] if s.get("step_id") == "WINDOWS-VALIDATE-CANDIDATE"]
        self.assertEqual(len(entry), 1)
        self.assertEqual(entry[0]["check_id"], "WIN-CANDIDATE-VALIDATE")
        self.assertEqual(entry[0]["binds_check"], "WIN-PACKAGE-CANDIDATE")
        self.assertIn("artifacts/candidate/AstroCS-candidate.zip", entry[0]["require_outputs"])
        self.assertTrue(entry[0]["fail_closed"])
        self.assertTrue(entry[0]["exec"][1].endswith("ci/validate_candidate.py"))

    def test_public_evidence_uploaded_if_always(self):
        for name, doc in self.docs.items():
            uploads = [s for s in doc["jobs"][{"ci-linux.yml": "linux",
                                               "ci-windows.yml": "windows"}[name]]["steps"]
                       if s.get("uses", "").startswith("actions/upload-artifact")]
            self.assertTrue(uploads, name)
            self.assertTrue(any(s.get("if") == "always()" for s in uploads), name)
            for s in uploads:
                self.assertTrue(s["with"].get("if-no-files-found"), name)

    def test_evidence_upload_path_matches_run_output_dir(self):
        """evidence 上传路径 = run.py 实际输出目录 artifacts/ci/（契约 07）。

        V8-CI-010 F-R3-02 接入侧：path 支持多路径列表（逐行展开）——
        ci-windows.yml 额外上传 run/ci/win-build-summary.json（cmake 真实
        错误证据），artifacts/ci/ 仍必须在上传集合内。
        """
        for name, doc in self.docs.items():
            uploads = [s for s in doc["jobs"][{"ci-linux.yml": "linux",
                                               "ci-windows.yml": "windows"}[name]]["steps"]
                       if s.get("uses", "").startswith("actions/upload-artifact")]
            paths = {ln.strip()
                     for s in uploads
                     for ln in str(s["with"]["path"]).splitlines()
                     if ln.strip()}
            self.assertIn("artifacts/ci/", paths, f"{name} 缺 artifacts/ci/ 上传路径")
            self.assertNotIn("artifacts/ci-public/", paths,
                             f"{name} 仍上传无生产者的 artifacts/ci-public/")
        win_doc = self.docs["ci-windows.yml"]
        win_paths = {ln.strip()
                     for s in win_doc["jobs"]["windows"]["steps"]
                     if s.get("uses", "").startswith("actions/upload-artifact")
                     for ln in str(s["with"]["path"]).splitlines() if ln.strip()}
        self.assertIn("run/ci/win-build-summary.json", win_paths,
                      "ci-windows.yml 缺 run/ci/win-build-summary.json 上传（F-R3-02）")
        # F-R4-02: ctest junit 与 driver test 汇总随 evidence artifact 上传，
        # ctest 失败用例名可离线定位（路径 = checks.json WIN-TEST-UNIT outputs）
        self.assertIn("run/ci/win-test-junit.xml", win_paths,
                      "ci-windows.yml 缺 run/ci/win-test-junit.xml 上传（F-R4-02）")
        self.assertIn("run/ci/win-test-summary.json", win_paths,
                      "ci-windows.yml 缺 run/ci/win-test-summary.json 上传（F-R4-02）")

    def test_collect_bootstrap_diagnostics_step_on_failure(self):
        """两平台各有一个 if: failure() 的 bootstrap 诊断步，产出 BOOTSTRAP_DIAG.json。"""
        for name, doc in self.docs.items():
            steps = doc["jobs"][{"ci-linux.yml": "linux",
                                 "ci-windows.yml": "windows"}[name]]["steps"]
            diags = [s for s in steps if s.get("name") == "Collect bootstrap diagnostics"]
            self.assertEqual(len(diags), 1, name)
            self.assertEqual(diags[0].get("if"), "failure()", name)
            self.assertIn("run", diags[0], name)
            self.assertNotIn("uses", diags[0], name)
            # CI-001B：诊断体迁入 ci/ 声明体（workflow 体只保留派发调用），
            # 证据路径断言随之绑定到声明 exec 指向的 ci/ 脚本本体。
            body = str(diags[0]["run"])
            binding = json.loads((_REPO / "ci" / "workflow_binding.json").read_text(encoding="utf-8"))
            entry = [s for s in binding["steps"]
                     if s.get("name") == diags[0]["name"]
                     and s.get("workflow") == ".github/workflows/" + name]
            self.assertEqual(len(entry), 1, name)
            self.assertIn("ci/wf_step.py --step " + entry[0]["step_id"], body, name)
            script = _REPO / entry[0]["exec"][1]
            self.assertTrue(script.is_file(), entry[0]["exec"][1])
            script_text = script.read_text(encoding="utf-8")
            self.assertIn("BOOTSTRAP_DIAG.json", script_text, name)
            self.assertIn("artifacts/ci", script_text, name)

    def test_no_algorithm_or_science_params(self):
        banned = ("toleran", "snr", "psf", "benchmark", "--iter", "threshold")
        for name in self.docs:
            text = (_WF_DIR / name).read_text(encoding="utf-8").lower()
            for word in banned:
                self.assertNotIn(word, text, f"{name} 含疑似科学/算法参数 {word!r}")

    def test_every_step_named(self):
        for name, doc in self.docs.items():
            for i, step in enumerate(doc["jobs"][{"ci-linux.yml": "linux",
                                                  "ci-windows.yml": "windows"}[name]]["steps"]):
                self.assertTrue(step.get("name"), f"{name} step#{i} 未命名")


# ------------------------------------------------------- verify_actions_lock ----

class TestVerifyActionsLockOffline(unittest.TestCase):
    """verify_actions_lock --offline 结构检查（不触网）。"""

    def test_real_lock_offline_pass(self):
        res = run_script(_REPO / "ci" / "verify_actions_lock.py",
                         "--offline", "--json")
        self.assertEqual(res.returncode, 0, res.stderr)
        report = json.loads(res.stdout)
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["mode"], "offline")

    def test_bad_sha_format_exit3(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "lock.json"
            bad.write_text(json.dumps({
                "schema_version": 1, "entries": [
                    {"action": "actions/checkout", "tag": "v7.0.1",
                     "commit_sha": "zzzz", "verified_utc": "x"}]}),
                encoding="utf-8")
            res = run_script(_REPO / "ci" / "verify_actions_lock.py",
                             "--offline", "--lock", str(bad), "--json")
            self.assertEqual(res.returncode, 3)
            self.assertEqual(json.loads(res.stdout)["verdict"], "LOCK_INVALID")

    def test_missing_lock_exit3(self):
        res = run_script(_REPO / "ci" / "verify_actions_lock.py",
                         "--offline", "--lock", "no/such/lock.json")
        self.assertEqual(res.returncode, 3)


# ------------------------------------------------------------------ bootstrap ----

class TestBootstrap(unittest.TestCase):
    """bootstrap --json 三态（本地缺工具属预期路径）。"""

    def test_local_host_fail_exit2_structured(self):
        """本地宿主非 hosted runner -> exit 2 + 结构化 stderr + 合法 JSON 报告。"""
        res = run_script(_REPO / "ci" / "bootstrap.py",
                         "--policy", "ci/toolchain.policy.json",
                         "--platform", "linux", "--json")
        self.assertEqual(res.returncode, 2)
        fail_payload = json.loads(res.stderr.strip().splitlines()[-1])
        self.assertEqual(fail_payload["verdict"], "FAIL")
        self.assertTrue(fail_payload["failed_tools"])
        report = json.loads(res.stdout)
        self.assertFalse(report["ok"])
        for item in report["items"]:
            self.assertIn("tool", item)
            self.assertIn("required", item)
            self.assertIn("observed", item)
            self.assertIn("ok", item)

    def test_all_pass_with_mocked_probes(self):
        """mock 全部探测命中 -> PASS（exit 0，无需真 hosted 宿主）。"""
        with mock.patch.object(BS, "probe_command_version",
                               side_effect=lambda tool, args=("--version",): {
                                   "gcc-14": (True, "gcc-14 (test) 14.2.0"),
                                   "clang-18": (True, "clang version 18.1.3"),
                                   "cmake": (True, "cmake version 3.31.12"),
                                   "ninja": (True, "1.12.1"),
                               }[tool]), \
             mock.patch.object(BS, "probe_hosted_linux_runner", return_value={
                 "hosted": True, "image_os": "ubuntu24", "runner_os": "Linux",
                 "runner_name": "test-runner", "os_release_id": "ubuntu",
                 "os_release_version": "24.04"}), \
             mock.patch.object(BS, "platform") as plat:
            plat.machine.return_value = "x86_64"
            plat.system.return_value = "Linux"
            policy = json.loads(_POLICY_PATH.read_text(encoding="utf-8"))
            items = BS.check_linux(policy["linux_hosted"])
        self.assertTrue(all(it["ok"] for it in items),
                        [it["tool"] for it in items if not it["ok"]])

    def test_version_below_minimum_fails_that_item_only(self):
        """cmake 版本不足 -> 仅该项 ok=False（其余 mock 命中）。"""
        with mock.patch.object(BS, "probe_command_version",
                               side_effect=lambda tool, args=("--version",): {
                                   "gcc-14": (True, "gcc-14 (test) 14.2.0"),
                                   "clang-18": (True, "clang version 18.1.3"),
                                   "cmake": (True, "cmake version 3.30.0"),
                                   "ninja": (True, "1.12.1"),
                               }[tool]), \
             mock.patch.object(BS, "probe_hosted_linux_runner", return_value={
                 "hosted": True, "image_os": "ubuntu24", "runner_os": "Linux",
                 "runner_name": "t", "os_release_id": "ubuntu",
                 "os_release_version": "24.04"}), \
             mock.patch.object(BS, "platform") as plat:
            plat.machine.return_value = "x86_64"
            plat.system.return_value = "Linux"
            policy = json.loads(_POLICY_PATH.read_text(encoding="utf-8"))
            items = BS.check_linux(policy["linux_hosted"])
        bad = {it["tool"] for it in items if not it["ok"]}
        self.assertEqual(bad, {"cmake"})
    def test_missing_policy_exit2(self):
        res = run_script(_REPO / "ci" / "bootstrap.py",
                         "--policy", "no/such/policy.json",
                         "--platform", "linux", "--json")
        self.assertEqual(res.returncode, 2)
        self.assertIn("FAIL", res.stderr)

    def test_report_carries_observed_from_and_policy_driven_versions(self):
        """--json 透传 observed_from；cmake 下限与 secondary 跟随 policy（修复轮 2）。"""
        res = run_script(_REPO / "ci" / "bootstrap.py",
                         "--policy", "ci/toolchain.policy.json",
                         "--platform", "linux", "--json")
        report = json.loads(res.stdout)
        obs = report["observed_from"]
        self.assertIsInstance(obs, dict)
        self.assertEqual(obs["cmake"], "3.31.6")
        self.assertEqual(obs["image_batch"], "20260831.293.1")
        cmake_item = next(it for it in report["items"] if it["tool"] == "cmake")
        self.assertEqual(cmake_item["required"], ">=3.31.6")
        secondary = next(it for it in report["items"]
                         if it["tool"].startswith("clang-"))
        self.assertEqual(secondary["tool"], "clang-18")

    def test_cmake_min_from_policy_and_fallback(self):
        """cmake 下限由 policy 字段推导；非版本格式/缺失字段回退 fallback。"""
        self.assertEqual(BS._cmake_min_from({"cmake": "3.31.6"}), (3, 31, 6))
        self.assertEqual(
            BS._cmake_min_from({"cmake": "project_minimum_or_newer"}),
            BS._CMAKE_FALLBACK_MIN)
        self.assertEqual(BS._cmake_min_from({}), BS._CMAKE_FALLBACK_MIN)


# -------------------------------------------------------------- select_profile ----

class TestSelectProfile(unittest.TestCase):
    """三事件规则 + 白名单 + GITHUB_OUTPUT 兼容输出。"""

    SCRIPT = _REPO / "ci" / "select_profile.py"

    def test_push_maps_linux_main(self):
        res = run_script(self.SCRIPT, "--event", "push")
        self.assertEqual((res.returncode, res.stdout), (0, "profile=linux-main\n"))

    def test_schedule_maps_linux_deep(self):
        res = run_script(self.SCRIPT, "--event", "schedule")
        self.assertEqual((res.returncode, res.stdout), (0, "profile=linux-deep\n"))

    def test_dispatch_valid_choices(self):
        for requested in ("linux-main", "linux-deep"):
            res = run_script(self.SCRIPT, "--event", "workflow_dispatch",
                             "--requested", requested)
            self.assertEqual((res.returncode, res.stdout),
                             (0, f"profile={requested}\n"))

    def test_dispatch_empty_request_exit2(self):
        res = run_script(self.SCRIPT, "--event", "workflow_dispatch")
        self.assertEqual(res.returncode, 2)
        self.assertIn("INVALID", res.stderr)

    def test_dispatch_invalid_choice_exit2(self):
        res = run_script(self.SCRIPT, "--event", "workflow_dispatch",
                         "--requested", "fast")
        self.assertEqual(res.returncode, 2)
        self.assertIn("INVALID", res.stderr)

    def test_unknown_event_exit2(self):
        res = run_script(self.SCRIPT, "--event", "cron")
        self.assertEqual(res.returncode, 2)

    def test_json_report_shape(self):
        res = run_script(self.SCRIPT, "--event", "push", "--json")
        payload = json.loads(res.stdout)
        self.assertEqual(payload["profile"], "linux-main")
        self.assertEqual(payload["event"], "push")
        self.assertIn("reason", payload)


# ---------------------------------------------------------- validate_candidate ----

def _good_zip_files() -> dict[str, bytes]:
    """最小合法 candidate：两产物 + 三清单（SHA256SUMS 覆盖含清单自身）。"""
    readme = b"AstroCS candidate readme\n"
    dll = b"MZ" + b"\x00" * 32
    provenance = json.dumps({
        "schema_version": 1, "source_sha": "a" * 40,
        "built_utc": "2026-01-01T00:00:00Z",
        "preset": {"configure": "win", "test": "win-rel"},
        "acr_enabled": False,
    }).encode("utf-8")
    manifest = json.dumps({
        "schema_version": 1, "algorithm": "sha256", "file_count": 2,
        "files": [
            {"path": "README.md", "sha256": _sha256(readme)},
            {"path": "include/astrocs.h", "sha256": _sha256(dll)},
        ],
    }).encode("utf-8")
    members = {"README.txt": readme, "lib/astrocs.dll": dll,
               "BUILD_PROVENANCE.json": provenance,
               "SOURCE_MANIFEST.json": manifest}
    sums = "".join(f"{_sha256(data)}  {name}\n"
                   for name, data in sorted(members.items())).encode("utf-8")
    members["SHA256SUMS"] = sums
    return members


def _write_zip(directory: Path, members: dict[str, bytes]) -> Path:
    import zipfile
    path = directory / "AstroCS-candidate.zip"
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
        for name in sorted(members):
            zf.writestr(name, members[name])
    return path


class TestValidateCandidate(unittest.TestCase):
    """最小合法 zip + 各拒绝路径（结构/哈希级，全程离线）。"""

    def test_minimal_valid_zip_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            res = run_script(_REPO / "ci" / "validate_candidate.py",
                             str(_write_zip(Path(tmp), _good_zip_files())), "--json")
            self.assertEqual(res.returncode, 0, res.stdout + res.stderr)
            self.assertEqual(json.loads(res.stdout)["verdict"], "PASS")

    def test_missing_manifest_rejected(self):
        members = _good_zip_files()
        del members["SOURCE_MANIFEST.json"]
        members["SHA256SUMS"] = "".join(
            f"{_sha256(d)}  {n}\n" for n, d in sorted(members.items())
            if n != "SHA256SUMS").encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            res = run_script(_REPO / "ci" / "validate_candidate.py",
                             str(_write_zip(Path(tmp), members)), "--json")
            self.assertEqual(res.returncode, 1)
            codes = {e["code"] for e in json.loads(res.stdout)["errors"]}
            self.assertIn("manifest_missing", codes)

    def test_sums_hash_mismatch_rejected(self):
        members = _good_zip_files()
        members["README.txt"] = b"tampered\n"
        with tempfile.TemporaryDirectory() as tmp:
            res = run_script(_REPO / "ci" / "validate_candidate.py",
                             str(_write_zip(Path(tmp), members)), "--json")
            self.assertEqual(res.returncode, 1)
            codes = {e["code"] for e in json.loads(res.stdout)["errors"]}
            self.assertIn("sums_hash_mismatch", codes)

    def test_source_file_in_zip_rejected(self):
        members = _good_zip_files()
        members["lib/helper.py"] = b"print('source must not ship')\n"
        members["SHA256SUMS"] = "".join(
            f"{_sha256(d)}  {n}\n" for n, d in sorted(members.items())
            if n != "SHA256SUMS").encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            res = run_script(_REPO / "ci" / "validate_candidate.py",
                             str(_write_zip(Path(tmp), members)), "--json")
            self.assertEqual(res.returncode, 1)
            codes = {e["code"] for e in json.loads(res.stdout)["errors"]}
            self.assertIn("excluded_entry_in_zip", codes)

    def test_build_cache_in_zip_rejected(self):
        members = _good_zip_files()
        members["CMakeFiles/cache-obj.o"] = b"\x00" * 8
        members["SHA256SUMS"] = "".join(
            f"{_sha256(d)}  {n}\n" for n, d in sorted(members.items())
            if n != "SHA256SUMS").encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            res = run_script(_REPO / "ci" / "validate_candidate.py",
                             str(_write_zip(Path(tmp), members)), "--json")
            self.assertEqual(res.returncode, 1)
            codes = {e["code"] for e in json.loads(res.stdout)["errors"]}
            self.assertIn("excluded_entry_in_zip", codes)

    def test_manifest_testdata_path_rejected(self):
        members = _good_zip_files()
        manifest = json.loads(members["SOURCE_MANIFEST.json"])
        manifest["files"].append(
            {"path": "testdata/index/extra.dat", "sha256": _sha256(b"x")})
        manifest["file_count"] = len(manifest["files"])
        members["SOURCE_MANIFEST.json"] = json.dumps(manifest).encode("utf-8")
        members["SHA256SUMS"] = "".join(
            f"{_sha256(d)}  {n}\n" for n, d in sorted(members.items())
            if n != "SHA256SUMS").encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            res = run_script(_REPO / "ci" / "validate_candidate.py",
                             str(_write_zip(Path(tmp), members)), "--json")
            self.assertEqual(res.returncode, 1)
            codes = {e["code"] for e in json.loads(res.stdout)["errors"]}
            self.assertIn("source_manifest_excluded", codes)

    def test_provenance_missing_field_rejected(self):
        members = _good_zip_files()
        provenance = json.loads(members["BUILD_PROVENANCE.json"])
        del provenance["acr_enabled"]
        members["BUILD_PROVENANCE.json"] = json.dumps(provenance).encode("utf-8")
        members["SHA256SUMS"] = "".join(
            f"{_sha256(d)}  {n}\n" for n, d in sorted(members.items())
            if n != "SHA256SUMS").encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            res = run_script(_REPO / "ci" / "validate_candidate.py",
                             str(_write_zip(Path(tmp), members)), "--json")
            self.assertEqual(res.returncode, 1)
            codes = {e["code"] for e in json.loads(res.stdout)["errors"]}
            self.assertIn("provenance_missing_field", codes)

    def test_unregistered_zip_member_rejected(self):
        members = _good_zip_files()
        members["stowaway.dll"] = b"not in sums"
        with tempfile.TemporaryDirectory() as tmp:
            res = run_script(_REPO / "ci" / "validate_candidate.py",
                             str(_write_zip(Path(tmp), members)), "--json")
            self.assertEqual(res.returncode, 1)
            codes = {e["code"] for e in json.loads(res.stdout)["errors"]}
            self.assertIn("sums_missing_entry", codes)

    def test_missing_zip_file_rejected(self):
        res = run_script(_REPO / "ci" / "validate_candidate.py",
                         "no/such/candidate.zip", "--json")
        self.assertEqual(res.returncode, 1)
        self.assertEqual(json.loads(res.stdout)["verdict"], "FAIL")


# ---------------------------------------------------------- 配置计数回归 ----

class TestProfileCountsRegression(unittest.TestCase):
    """plan-only selected_count 基线：61/94/7/66（CI-001B 后：WORKFLOW-REGISTRY-BINDING
    与 CI-BINDING-TESTS 进 fast/linux-main/windows-main；LINUX-MAIN-FIXTURES /
    LINUX-MAIN-BUILD-TREE 进 linux-main；WIN-CANDIDATE-VALIDATE 进 windows-main——
    workflow 侧两条 linux 业务步与 windows 候选校验步同提交收编为注册表检查项）。
    此前 CI-BASELINE-001 后为 59/90/7/63，CI-REG-002 后为 58/88/7/62。"""

    BASELINE = {"fast": 61, "linux-main": 94, "linux-deep": 7, "windows-main": 66}

    def test_plan_only_counts_unchanged(self):
        for profile, expected in self.BASELINE.items():
            res = H.sh([sys.executable, str(_REPO / "ci" / "run.py"),
                        "--profile", profile, "--plan-only"],
                       cwd=_REPO, timeout=120)
            self.assertEqual(res.returncode, 0, f"{profile}: {res.stderr}")
            plan = json.loads(res.stdout)
            self.assertEqual(plan.get("selected_count"), expected,
                             f"{profile} 计数回归：{plan.get('selected_count')}")

    def test_registry_strict_still_passes(self):
        res = run_script(_REPO / "ci" / "validate_registry.py",
                         "--registry", "ci/checks.json", "--strict")
        self.assertEqual(res.returncode, 0, res.stderr)


if __name__ == "__main__":
    unittest.main()
