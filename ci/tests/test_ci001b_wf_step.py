# -*- coding: utf-8 -*-
"""CI-001B 单测：声明步派发器 ci/wf_step.py（fail-closed 候选门）。

覆盖：
1. 现网声明 WINDOWS-VALIDATE-CANDIDATE 在 candidate 缺失时 **FAIL（exit 1）**，
   输出 ::error:: 且不含任何 ::warning:: 跳过路径，并落 FAIL_MISSING_OUTPUT 记录；
2. candidate 存在且结构合法时真正执行 ci/validate_candidate.py → exit 0；
3. 声明与注册表漂移必败：绑定检查被改成 waivable=true → exit 4；
   require_outputs 不在该检查 outputs 内 → exit 5；exec 目标缺失 → exit 3；
4. 未知 step_id → exit 2；超时 → exit 124（所有外部命令带 timeout）；
5. linux 侧声明步（serves_checks=[UT-BACKEND, UT-CLI]）dry-run 契约可见。

全部离线，在临时 fixture 仓库内完成；解释器名统一解析为当前 sys.executable。
"""
from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]
_SUPPORT = ("ci/checks.json", "ci/actions.lock.json", "ci/workflow_binding.json",
            "ci/validate_candidate.py")
_ZIP_REL = "artifacts/candidate/AstroCS-candidate.zip"


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def valid_candidate_members() -> dict:
    """最小合法 candidate 成员集（清单在 zip 根、SHA256SUMS 双向完备）。"""
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
    sums = "".join(_sha256(d) + "  " + n + "\n" for n, d in sorted(members.items()))
    members["SHA256SUMS"] = sums.encode("utf-8")
    return members


def write_candidate(root: Path) -> Path:
    out = root / _ZIP_REL
    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, data in sorted(valid_candidate_members().items()):
            zf.writestr(name, data)
    return out


def make_fixture(root: Path) -> Path:
    (root / "ci").mkdir(parents=True, exist_ok=True)
    for rel in _SUPPORT:
        shutil.copy2(_REPO / rel, root / rel)
    shutil.copytree(_REPO / "ci" / "steps", root / "ci" / "steps")
    return root


def run_wf_step(root: Path, *args: str, timeout: float = 120):
    return subprocess.run(
        [sys.executable, str(_REPO / "ci" / "wf_step.py"),
         "--manifest", str(root / "ci" / "workflow_binding.json"),
         "--registry", str(root / "ci" / "checks.json"),
         "--repo-root", str(root), *args],
        cwd=str(_REPO), capture_output=True, text=True, timeout=timeout,
        encoding="utf-8", errors="replace")


class TestCandidateGate(unittest.TestCase):
    """目标 2：候选缺失必须 FAIL，不得 warning 跳过。"""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = make_fixture(Path(self._tmp.name))

    def tearDown(self):
        self._tmp.cleanup()

    def test_missing_candidate_fails_closed(self):
        res = run_wf_step(self.root, "--step", "WINDOWS-VALIDATE-CANDIDATE")
        self.assertEqual(res.returncode, 1, res.stdout + res.stderr)
        self.assertIn("::error::", res.stderr)
        self.assertIn("fail-closed", res.stderr)
        self.assertNotIn("::warning::", res.stdout + res.stderr)
        self.assertNotIn("skip", res.stdout.lower())
        record = json.loads((self.root / "run" / "ci" / "wf_step"
                             / "WINDOWS-VALIDATE-CANDIDATE.json").read_text(encoding="utf-8"))
        self.assertEqual(record["verdict"], "FAIL_MISSING_OUTPUT")
        self.assertEqual(record["missing_outputs"], [_ZIP_REL])
        self.assertFalse(record["fail_closed"] is False)

    def test_present_candidate_runs_structural_validation(self):
        write_candidate(self.root)
        res = run_wf_step(self.root, "--step", "WINDOWS-VALIDATE-CANDIDATE", "--json")
        self.assertEqual(res.returncode, 0, res.stdout + res.stderr)
        record = json.loads(res.stdout.strip().splitlines()[-1])
        self.assertEqual(record["exit_code"], 0)
        self.assertEqual(record["missing_outputs"], [])

    def test_tampered_candidate_fails(self):
        path = write_candidate(self.root)
        with zipfile.ZipFile(path, "a") as zf:
            zf.writestr("stowaway.dll", b"not in sums")
        res = run_wf_step(self.root, "--step", "WINDOWS-VALIDATE-CANDIDATE")
        self.assertEqual(res.returncode, 1, res.stdout + res.stderr)

    def test_dry_run_reports_contract(self):
        write_candidate(self.root)
        res = run_wf_step(self.root, "--step", "WINDOWS-VALIDATE-CANDIDATE", "--dry-run", "--json")
        self.assertEqual(res.returncode, 0, res.stdout + res.stderr)
        record = json.loads(res.stdout.strip().splitlines()[-1])
        self.assertEqual(record["binds_check"], "WIN-PACKAGE-CANDIDATE")
        self.assertEqual(record["require_outputs"], [_ZIP_REL])
        self.assertTrue(record["dry_run"])


class TestDeclarationDrift(unittest.TestCase):
    """声明与注册表漂移必须让派发器 fail（而不是继续跑）。"""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = make_fixture(Path(self._tmp.name))
        self.manifest = json.loads((self.root / "ci" / "workflow_binding.json").read_text(encoding="utf-8"))
        self.registry = json.loads((self.root / "ci" / "checks.json").read_text(encoding="utf-8"))

    def tearDown(self):
        self._tmp.cleanup()

    def _write(self):
        (self.root / "ci" / "workflow_binding.json").write_text(
            json.dumps(self.manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        (self.root / "ci" / "checks.json").write_text(
            json.dumps(self.registry, ensure_ascii=False, indent=2), encoding="utf-8")

    def test_unknown_step_usage_error(self):
        res = run_wf_step(self.root, "--step", "NO-SUCH-STEP")
        self.assertEqual(res.returncode, 2, res.stderr)

    def test_bound_check_waivable_drift(self):
        for check in self.registry["checks"]:
            if check["id"] == "WIN-PACKAGE-CANDIDATE":
                check["waivable"] = True
        self._write()
        res = run_wf_step(self.root, "--step", "WINDOWS-VALIDATE-CANDIDATE", "--dry-run")
        self.assertEqual(res.returncode, 4, res.stdout + res.stderr)
        self.assertIn("bound_check_waivable", res.stderr)

    def test_output_contract_drift(self):
        for check in self.registry["checks"]:
            if check["id"] == "WIN-PACKAGE-CANDIDATE":
                check["outputs"] = [o for o in check["outputs"] if "AstroCS-candidate.zip" not in o]
        self._write()
        res = run_wf_step(self.root, "--step", "WINDOWS-VALIDATE-CANDIDATE", "--dry-run")
        self.assertEqual(res.returncode, 5, res.stdout + res.stderr)
        self.assertIn("output_contract_drift", res.stderr)

    def test_exec_target_missing(self):
        for step in self.manifest["steps"]:
            if step["step_id"] == "WINDOWS-VALIDATE-CANDIDATE":
                step["exec"] = ["python", "ci/no_such_validator.py", _ZIP_REL]
        self._write()
        res = run_wf_step(self.root, "--step", "WINDOWS-VALIDATE-CANDIDATE", "--dry-run")
        self.assertEqual(res.returncode, 3, res.stdout + res.stderr)
        self.assertIn("exec_target_missing", res.stderr)

    def test_linux_declaration_step_dry_run(self):
        res = run_wf_step(self.root, "--step", "LINUX-PREPARE-FIXTURES", "--dry-run", "--json")
        self.assertEqual(res.returncode, 0, res.stdout + res.stderr)
        record = json.loads(res.stdout.strip().splitlines()[-1])
        self.assertEqual(record["serves_checks"], ["UT-BACKEND", "UT-CLI"])
        self.assertIsNone(record["binds_check"])


class TestTimeout(unittest.TestCase):
    """所有外部命令带 timeout：超时必须 124 而不是挂死。"""

    def test_timeout_enforced(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = make_fixture(Path(tmp))
            slow = root / "ci" / "_slow.py"
            slow.write_text("import time\ntime.sleep(30)\n", encoding="utf-8")
            manifest = json.loads((root / "ci" / "workflow_binding.json").read_text(encoding="utf-8"))
            manifest["steps"].append({
                "step_id": "TMP-SLOW", "workflow": ".github/workflows/ci-linux.yml",
                "job": "linux", "name": "TMP slow", "role": "infra-step",
                "exec": ["python3", "ci/_slow.py"], "timeout_seconds": 1, "fail_closed": False,
            })
            (root / "ci" / "workflow_binding.json").write_text(
                json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
            res = run_wf_step(root, "--step", "TMP-SLOW", timeout=60)
            self.assertEqual(res.returncode, 124, res.stdout + res.stderr)
            record = json.loads((root / "run" / "ci" / "wf_step" / "TMP-SLOW.json").read_text(encoding="utf-8"))
            self.assertTrue(record["timed_out"])


if __name__ == "__main__":
    unittest.main()
