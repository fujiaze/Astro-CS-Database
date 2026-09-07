# -*- coding: utf-8 -*-
"""V8-CI-009 负向测试（owner=SA-CI-32）：离线注入违反守卫的样例并确认拒绝路径。

- 矩阵编号 N01..N13 与 evidence/v8_1_ci_control/tasks/V8-CI-009/NEGATIVE_MATRIX.md 对齐。
- 所有注入仅在 tempfile 临时副本 / fixture 仓库上进行，绝不改动主仓库既有守卫脚本、
  workflow 与 registry 的生效内容。
- GAP 补齐轮（前台裁决）：
  * GAP-G1（WRITE_LEASE 消费）→ **POLICY(控制面保障)**，不实现：WRITE_LEASE 是控制面
    状态，其校验由前台 dispatch 流程人工保障，不进 ci/run.py（hosted runner 上无
    lease 文件）；对应用例为登记 POLICY 裁决的说明性测试（锚点断言 run.py 无
    WRITE_LEASE 消费，裁决改变时翻红提示更新矩阵）。
  * GAP-G2（硬编码线程）→ 已补齐：ci/validate_registry.py 增 R9
    （hardcoded_core_in_command），registry 维度用例翻红为拒绝断言；
    执行链 env 直通维度登记 POLICY（命令参数通道已被 R9 封锁）。
  * GAP-G3（zip 路径逃逸）→ 已补齐：ci/validate_candidate.py 增
    path_escape_in_zip 校验，用例翻红为拒绝断言。
- 类 6/7/12/13 的守卫位于 ci/tests/test_fatduck_workflow.py / test_workflow_lock.py
  （测试层守卫），本文件以只读镜像断言（_assert_fatduck_shape / load_lock）演示
  注入样例被拒绝；镜像断言自身先对真实 fatduck.yml 做 sanity 校验（防镜像空洞）。
"""
from __future__ import annotations

import copy
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

import yaml

import _helpers as H  # noqa: E402

REPO = H.REPO
RUNNER = H.RUNNER
PY = sys.executable
FATDUCK_YML = REPO / ".github" / "workflows" / "fatduck.yml"
LOCK_PATH = REPO / "ci" / "actions.lock.json"
VALIDATOR = REPO / "ci" / "validate_registry.py"
VERIFY_LOCK = REPO / "ci" / "verify_actions_lock.py"
BOOTSTRAP = REPO / "ci" / "bootstrap.py"
VALIDATE_CAND = REPO / "ci" / "validate_candidate.py"


# ---------------------------------------------------------------- 工具 ----

def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _mk_repo() -> Path:
    """fixture 仓库（git init + 结果 schema 副本），执行层注入一律用它。"""
    root = tempfile.mkdtemp(prefix="n9_")
    repo = H.make_repo(Path(root))
    H.write_ci_result_schema(repo)
    return repo


def _check(**overrides) -> dict:
    base = {
        "id": "N-T", "profiles": ["fast"], "platform": "any",
        "command": [PY, "-c", "print('ok')"], "timeout_seconds": 30,
        "heavy": False, "mutates_workspace": False, "outputs": [],
        "waivable": False, "changed_paths": [], "requires_monitor": False,
    }
    base.update(overrides)
    return base


def _write_registry(repo: Path, checks: list[dict]) -> Path:
    (repo / "ci").mkdir(parents=True, exist_ok=True)
    path = repo / "ci" / "checks.json"
    path.write_text(json.dumps({"schema_version": 1, "checks": checks},
                               ensure_ascii=False), encoding="utf-8")
    return path


def _run_runner(repo: Path, *extra: str, timeout: float = 120):
    return H.sh([PY, str(RUNNER), "--repo-root", str(repo), "--profile", "fast",
                 *extra], cwd=repo, timeout=timeout)


def _ci_result(proc) -> dict | None:
    """从 run.py stdout 的 `result: <path>` 行读取 CI_RESULT.json。"""
    for line in proc.stdout.splitlines():
        if line.startswith("result: "):
            return json.loads(Path(line[8:].strip()).read_text(encoding="utf-8"))
    return None


def _registry_verdict(registry: Path) -> subprocess.CompletedProcess:
    """validate_registry --strict（临时 registry，cwd=主仓库以复用脚本锚点）。"""
    return H.sh([PY, str(VALIDATOR), "--registry", str(registry), "--strict"],
                cwd=REPO, timeout=60)


def _strict_policy(tmp: Path) -> Path:
    """版本漂移注入：主机不可能满足的 toolchain policy。"""
    policy = tmp / "strict_policy.json"
    policy.write_text(json.dumps({
        "linux_hosted": {"runner": "ubuntu-24.04", "architecture": "x86_64",
                         "primary_compiler": "gcc-99", "secondary_compiler": "clang-99",
                         "cmake": "99.99.99", "generator": "Ninja"},
        "windows_hosted": {"runner": "windows-2022", "architecture": "x64",
                           "visual_studio_major": 17, "platform_toolset": "v143",
                           "cmake": "99.99.99"},
    }), encoding="utf-8")
    return policy


def _good_zip_manifest(extra: dict[str, bytes]) -> dict[str, bytes]:
    """构造可通过 manifest/SHA256SUMS 结构校验的 zip 条目，再叠加注入成员。"""
    readme = b"readme\n"
    dll = b"MZ" + b"\x00" * 32
    provenance = json.dumps({
        "schema_version": 1, "source_sha": "a" * 40,
        "built_utc": "2026-01-01T00:00:00Z", "preset": {"x": "y"},
        "acr_enabled": False}).encode()
    manifest = json.dumps({
        "schema_version": 1, "file_count": 2,
        "files": [{"path": "README.txt", "sha256": _sha256(readme)},
                  {"path": "lib/a.dll", "sha256": _sha256(dll)}]}).encode()
    entries = {"README.txt": readme, "lib/a.dll": dll,
               "BUILD_PROVENANCE.json": provenance,
               "SOURCE_MANIFEST.json": manifest}
    for name, blob in extra.items():
        entries[name] = blob
    entries["SHA256SUMS"] = "".join(
        f"{_sha256(blob)}  {name}\n" for name, blob in sorted(entries.items())
    ).encode()
    return entries


def _make_zip(tmp: Path, tag: str, entries: dict[str, bytes]) -> Path:
    zip_path = tmp / f"cand_{tag}.zip"
    with zipfile.ZipFile(zip_path, "w") as zf:
        for name, blob in sorted(entries.items()):
            zf.writestr(name, blob)
    return zip_path


def _validate_zip(zip_path: Path) -> subprocess.CompletedProcess:
    return H.sh([PY, str(VALIDATE_CAND), str(zip_path), "--json"],
                cwd=REPO, timeout=60)


# ------------------------------------------- fatduck.yml 守卫只读镜像 ----

def _on(doc: dict):
    return doc[True] if True in doc else doc.get("on")


def _assert_fatduck_shape(doc: dict) -> None:
    """镜像 ci/tests/test_fatduck_workflow.py 的形状守卫（只读，不改原测试）。

    违反任一约束即 AssertionError —— 注入样例依赖该断言失败来证明"守卫拒绝"。
    覆盖：触发器白名单、workflow_run 约束、permissions 矩阵（仅 notify-owner 持
    issues:write）、fatduck-validate 无 checkout/无仓库脚本/单 run 步、全 uses 锁 40hex。
    """
    on = _on(doc)
    assert isinstance(on, dict), f"触发器形状异常：{type(on)}"
    assert set(on) == {"workflow_run", "schedule"}, f"触发器白名单外多出：{sorted(on)}"
    wr = on["workflow_run"]
    assert wr["workflows"] == ["AstroCS Windows CI"], wr
    assert wr["types"] == ["completed"], wr
    assert wr["branches"] == ["main"], wr
    assert on["schedule"] and "cron" in on["schedule"][0], on["schedule"]

    jobs = doc["jobs"]
    assert set(jobs) == {"select-candidate", "fatduck-validate", "notify-owner"}
    assert doc["permissions"] == {"contents": "read"}, \
        f"顶层 permissions 越界：{doc['permissions']}"
    writers = [name for name, job in jobs.items()
               if (job.get("permissions") or {}).get("issues")]
    assert writers == ["notify-owner"], f"issues:write 越界：{writers}"

    validate_job = jobs["fatduck-validate"]
    for step in validate_job["steps"]:
        if "uses" in step:
            assert not step["uses"].startswith("actions/checkout"), \
                f"fatduck-validate 出现 checkout：{step['uses']}"
    run_steps = [s for s in validate_job["steps"] if "run" in s]
    # V8-CIQA-001 P2-GAP-3 后为两个 run 步：独立 digest 复核步 + 固定 harness
    assert len(run_steps) == 2, "fatduck-validate 允许 digest 复核 + harness 两个 run 步骤"
    assert "Get-FileHash" in run_steps[0]["run"], "首个 run 步必须是 digest 复核"
    script = run_steps[1]["run"]
    for token in (r"\bpython3?\b", r"\bpip3?\b", r"\bgit\b"):
        import re as _re
        assert not _re.search(token, script), f"run 步骤出现禁用调用：{token}"
    assert "ci/" not in script, "fatduck-validate 出现仓库相对路径"
    assert run_steps[1].get("shell") == "pwsh", "run 步骤必须固定 pwsh"
    for s in run_steps:
        import re as _re
        assert not _re.search(r"\bpython3?\b", s["run"]), "复核步不得引入 python"
        assert s.get("shell") == "pwsh"

    import json as _json
    lock_shas = {e["commit_sha"] for e in
                 _json.loads(LOCK_PATH.read_text(encoding="utf-8"))["entries"]}
    for job_name, job in jobs.items():
        for step in job["steps"]:
            uses = step.get("uses")
            if not uses:
                continue
            ref = uses.split("@", 1)[1].split()[0]
            assert len(ref) == 40 and all(c in "0123456789abcdef" for c in ref), \
                f"{job_name}: {uses} 未锁完整 SHA（tag/短 SHA 注入被拒）"
            assert ref in lock_shas, f"{job_name}: {uses} 不在 actions.lock"


class _MirrorSanity(unittest.TestCase):
    """镜像守卫对真实 fatduck.yml 必须通过 —— 保证注入用例的拒绝并非镜像空洞。"""

    def test_mirror_passes_real_fatduck_yml(self):
        doc = yaml.safe_load(FATDUCK_YML.read_text(encoding="utf-8"))
        _assert_fatduck_shape(doc)  # 不抛即通过


# ------------------------------------------------------------- N01..N03 ----

class TestN01FakePassJson(unittest.TestCase):
    """类 1：假 PASS JSON（伪造 verdict PASS 的结果文件喂结果消费路径）。"""

    def test_invalid_json_registry_rejected_exit2(self):
        repo = _mk_repo()
        (repo / "ci" / "checks.json").write_text("{oops", encoding="utf-8")
        proc = _run_runner(repo)
        self.assertEqual(proc.returncode, 2)
        self.assertIn("JSON 解析失败", proc.stderr)

    def test_duplicate_check_id_rejected_exit2(self):
        repo = _mk_repo()
        _write_registry(repo, [_check(id="N-DUP"), _check(id="N-DUP")])
        proc = _run_runner(repo)
        self.assertEqual(proc.returncode, 2)
        self.assertIn("重复检查 ID", proc.stderr)

    def test_forged_pass_overwritten_by_execution(self):
        repo = _mk_repo()
        _write_registry(repo, [_check(id="N-T")])
        out = repo / "artifacts" / "ci" / "forged"
        (out / "checks").mkdir(parents=True)
        (out / "checks" / "N-T.json").write_text(json.dumps(
            {"id": "N-T", "verdict": "PASS", "exit_code": 0, "forged": True}),
            encoding="utf-8")
        (out / "CI_RESULT.json").write_text(json.dumps(
            {"schema_version": 1, "verdict": "PASS", "checks": []}), encoding="utf-8")
        proc = _run_runner(repo, "--output-root", str(out))
        self.assertEqual(proc.returncode, 0)
        per_check = json.loads((out / "checks" / "N-T.json").read_text(encoding="utf-8"))
        ci = json.loads((out / "CI_RESULT.json").read_text(encoding="utf-8"))
        self.assertNotIn("forged", per_check)          # 伪造字段被重算清除
        self.assertEqual(ci["generated_by"], "ci/run.py (V8-CI-002)")
        self.assertEqual(len(ci["checks"]), 1)          # 伪造空 checks 被覆盖

    def test_forged_pass_real_fail_still_fail(self):
        repo = _mk_repo()
        _write_registry(repo, [_check(command=[PY, "-c", "import sys;sys.exit(3)"])])
        out = repo / "artifacts" / "ci" / "forged"
        (out / "checks").mkdir(parents=True)
        (out / "checks" / "N-T.json").write_text(json.dumps(
            {"id": "N-T", "verdict": "PASS", "exit_code": 0}), encoding="utf-8")
        (out / "CI_RESULT.json").write_text(json.dumps(
            {"schema_version": 1, "verdict": "PASS", "checks": []}), encoding="utf-8")
        proc = _run_runner(repo, "--output-root", str(out))
        self.assertEqual(proc.returncode, 1)
        per_check = json.loads((out / "checks" / "N-T.json").read_text(encoding="utf-8"))
        self.assertEqual(per_check["verdict"], "FAIL")
        self.assertEqual(per_check["exit_code"], 3)
        self.assertEqual(
            json.loads((out / "CI_RESULT.json").read_text(encoding="utf-8"))["verdict"],
            "FAIL")


class TestN02NonZeroExit(unittest.TestCase):
    """类 2/3 执行语义：非零退出 / 超时 / 信号 → FAIL/TIMEOUT/SIGNAL，run.py 退出 1。"""

    def test_nonzero_exit_verdict_fail(self):
        repo = _mk_repo()
        _write_registry(repo, [_check(command=[PY, "-c", "import sys;sys.exit(3)"])])
        proc = _run_runner(repo)
        self.assertEqual(proc.returncode, 1)
        ci = _ci_result(proc)
        self.assertEqual(ci["checks"][0]["verdict"], "FAIL")
        self.assertEqual(ci["summary"]["fail"], 1)

    def test_timeout_verdict_timeout(self):
        repo = _mk_repo()
        _write_registry(repo, [_check(
            command=[PY, "-c", "import time;time.sleep(4)"], timeout_seconds=1)])
        proc = _run_runner(repo)
        self.assertEqual(proc.returncode, 1)
        ci = _ci_result(proc)
        self.assertEqual(ci["checks"][0]["verdict"], "TIMEOUT")

    def test_sigkill_verdict_signal(self):
        repo = _mk_repo()
        _write_registry(repo, [_check(
            command=[PY, "-c",
                     "import os,signal;os.kill(os.getpid(),signal.SIGKILL)"])])
        proc = _run_runner(repo)
        self.assertEqual(proc.returncode, 1)
        ci = _ci_result(proc)
        self.assertEqual(ci["checks"][0]["verdict"], "SIGNAL")


class TestN03VerdictSource(unittest.TestCase):
    """类 3：PASS 判定唯一来源 = 实际执行（plan-only 无判定、summary 由执行重算）。"""

    def test_plan_only_has_no_verdict(self):
        proc = H.sh([PY, str(RUNNER), "--profile", "fast", "--plan-only"],
                    cwd=REPO, timeout=60)
        self.assertEqual(proc.returncode, 0)
        plan = json.loads(proc.stdout)
        self.assertNotIn("verdict", plan)
        checks = plan.get("checks") or []
        for entry in checks:
            if isinstance(entry, dict):
                self.assertNotIn("verdict", entry)

    def test_summary_recomputed_from_execution(self):
        repo = _mk_repo()
        _write_registry(repo, [_check(command=[PY, "-c", "import sys;sys.exit(3)"])])
        proc = _run_runner(repo)
        self.assertEqual(proc.returncode, 1)
        ci = _ci_result(proc)
        self.assertEqual(ci["summary"]["fail"], 1)
        self.assertEqual(ci["summary"]["pass"], 0)
        self.assertEqual(ci["summary"]["total"], 1)


# ------------------------------------------------------------- N04..N05 ----

class TestN04DirtyWorkspaceAndLease(unittest.TestCase):
    """类 4：dirty workspace / WRITE_LEASE 写操作路径。

    - N04a（REJECTED）：mutates_workspace=false 检查改动工作区 → FAIL(dirty)。
    - N04b（GAP-G1 → **POLICY(控制面保障)**，前台裁决不实现）：WRITE_LEASE 是
      控制面状态，其校验由前台 dispatch 流程人工保障，不进 ci/run.py
      （hosted runner 上无 lease 文件）。对应用例为登记裁决的说明性测试：
      锚点断言 run.py 无 WRITE_LEASE 消费逻辑；若未来裁决变更将其下沉
      run.py，该断言翻红，提示同步更新 NEGATIVE_MATRIX.md 与 TASK_RESULT.json。
    """

    def test_workspace_mutation_rejected_fail_dirty(self):
        repo = _mk_repo()
        _write_registry(repo, [_check(
            id="N-DIRTY",
            command=[PY, "-c", "open('dirty_probe.txt','w').write('x')"])])
        proc = _run_runner(repo)
        self.assertEqual(proc.returncode, 1)
        per_check = json.loads(
            (repo / "artifacts" / "ci").glob("**/checks/N-DIRTY.json").__next__()
            .read_text(encoding="utf-8"))
        self.assertEqual(per_check["verdict"], "FAIL(dirty)")
        self.assertEqual(per_check["dirty"]["violations"], ["dirty_probe.txt"])

    def test_write_lease_control_plane_policy__GAP_G1(self):
        """GAP-G1 POLICY 裁决（不实现 run.py 守卫）：说明性登记测试。

        裁决（V8-CI-009 GAP 补齐轮）：WRITE_LEASE 是控制面状态，其校验由前台
        dispatch 流程人工保障；hosted runner 上无 lease 文件，故 ci/run.py
        不消费 WRITE_LEASE。本用例不做注入、不断言拒绝，仅锚定裁决现状。
        """
        src = (REPO / "ci" / "run.py").read_text(encoding="utf-8")
        self.assertNotIn("WRITE_LEASE", src,
                         "GAP-G1 裁决变更：run.py 出现 WRITE_LEASE 消费，"
                         "请同步把 NEGATIVE_MATRIX.md N04b 更新为 REJECTED")
        self.assertNotIn("write_lease", src.lower())


class TestN05UnregisteredScript(unittest.TestCase):
    """类 5：未登记脚本被执行链拒绝（run.py --check 未登记 ID / validator R4 路径）。"""

    def test_unknown_check_id_rejected_exit2(self):
        repo = _mk_repo()
        _write_registry(repo, [_check(id="N-OK")])
        proc = _run_runner(repo, "--check", "N-GHOST")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("未登记的检查 ID", proc.stderr)

    def test_unregistered_relative_script_rejected_by_r4(self):
        with tempfile.TemporaryDirectory() as tmp:
            reg = Path(tmp) / "checks.json"
            reg.write_text(json.dumps({"schema_version": 1, "checks": [
                _check(id="N-UNREG", command=["python3", "ci/nonexist.py"])]}),
                encoding="utf-8")
            proc = _registry_verdict(reg)
            self.assertEqual(proc.returncode, 1)
            self.assertIn("file not found", proc.stdout)


# ------------------------------------------------------------- N06..N07 ----

class TestN06PrForkTriggerFatduck(unittest.TestCase):
    """类 6：PR/fork 触发 Fatduck → 触发器白名单守卫拒绝（测试层守卫，临时副本注入）。"""

    def _inject(self, mutate) -> Path:
        doc = yaml.safe_load(FATDUCK_YML.read_text(encoding="utf-8"))
        mutate(doc)
        tmp = Path(tempfile.mkdtemp(prefix="n9_wf6_"))
        path = tmp / "fatduck_pr.yml"
        path.write_text(yaml.safe_dump(doc, sort_keys=False), encoding="utf-8")
        return path

    def test_pull_request_trigger_rejected(self):
        path = self._inject(lambda d: _on(d).update(
            {"pull_request": {"branches": ["main"]}}))
        doc = yaml.safe_load(path.read_text(encoding="utf-8"))
        with self.assertRaises(AssertionError):
            _assert_fatduck_shape(doc)

    def test_workflow_dispatch_trigger_rejected(self):
        path = self._inject(lambda d: _on(d).update({"workflow_dispatch": None}))
        doc = yaml.safe_load(path.read_text(encoding="utf-8"))
        with self.assertRaises(AssertionError):
            _assert_fatduck_shape(doc)


class TestN07TagAction(unittest.TestCase):
    """类 7：tag action（workflow 引用 tag 而非 SHA → verify_actions_lock/单测守卫拒绝）。"""

    def test_workflow_run_branch_dev_rejected(self):
        doc = yaml.safe_load(FATDUCK_YML.read_text(encoding="utf-8"))
        _on(doc)["workflow_run"]["branches"] = ["dev"]
        with self.assertRaises(AssertionError):
            _assert_fatduck_shape(doc)

    def test_tag_pinned_use_rejected(self):
        doc = yaml.safe_load(FATDUCK_YML.read_text(encoding="utf-8"))
        step = doc["jobs"]["select-candidate"]["steps"][0]
        step["uses"] = "actions/checkout@v7.0.1"        # tag 注入
        with self.assertRaises(AssertionError):
            _assert_fatduck_shape(doc)

    def test_tampered_lock_short_sha_rejected(self):
        sys.path.insert(0, str(REPO / "ci"))
        try:
            import verify_actions_lock as VAL
        finally:
            sys.path.pop(0)
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "lock.json"
            bad.write_text(json.dumps({
                "schema_version": 1, "entries": [
                    {"action": "actions/checkout", "tag": "v7.0.1",
                     "commit_sha": "shortsha", "verified_utc": "2026-01-01T00:00:00Z"}]}),
                encoding="utf-8")
            with self.assertRaises(VAL.LockInvalid):
                VAL.load_lock(bad)


# ---------------------------------------------------------------- N08 ----

class TestN08HardcodedCore(unittest.TestCase):
    """类 8：hardcoded core（注册表 check 命令硬编码线程 / 执行链环境注入）。

    GAP-G2 → **REJECTED(本轮补齐)**：ci/validate_registry.py 增规则 R9
    （hardcoded_core_in_command）——command 全元素出现 -j<N>/--jobs/--threads/
    NPROC/*_NUM_THREADS 即 strict 拒绝；ci/resource_monitor.py 前缀与 `--`
    之间的 monitor 自身参数（--timeout/--output）白名单放行。
    执行链 env 维度 → **POLICY**：run.py 不清洗子进程 env；R9 已从 registry
    源头封锁硬编码线程参数通道，hosted runner 环境由控制面保障，
    对应用例为登记裁决现状的说明性测试（锚点断言）。
    """

    def test_registry_j4_argument_rejected_r9(self):
        with tempfile.TemporaryDirectory() as tmp:
            reg = Path(tmp) / "checks.json"
            reg.write_text(json.dumps({"schema_version": 1, "checks": [
                _check(id="N-J4",
                       command=["python3", "ci/select_profile.py", "-j4"])]}),
                encoding="utf-8")
            proc = _registry_verdict(reg)
            self.assertEqual(proc.returncode, 1)
            self.assertIn("R9", proc.stdout)
            self.assertIn("hardcoded_core_in_command", proc.stdout)

    def test_registry_threads_argument_rejected_r9(self):
        with tempfile.TemporaryDirectory() as tmp:
            reg = Path(tmp) / "checks.json"
            reg.write_text(json.dumps({"schema_version": 1, "checks": [
                _check(id="N-THR",
                       command=["python3", "ci/select_profile.py", "--threads=64"])]}),
                encoding="utf-8")
            proc = _registry_verdict(reg)
            self.assertEqual(proc.returncode, 1)
            self.assertIn("R9", proc.stdout)
            self.assertIn("hardcoded_core_in_command", proc.stdout)

    def test_monitor_prefixed_command_allowed_by_r9(self):
        """R9 白名单：resource_monitor 前缀 + `--` 后子命令均无线程参数 → PASS。"""
        with tempfile.TemporaryDirectory() as tmp:
            reg = Path(tmp) / "checks.json"
            reg.write_text(json.dumps({"schema_version": 1, "checks": [
                _check(id="N-MON", profiles=["linux-deep"], heavy=True,
                       requires_monitor=True,
                       command=["python3", "ci/resource_monitor.py",
                                "--timeout", "3500", "--output", "run/m.json",
                                "--", "python3", "ci/select_profile.py"])]}),
                encoding="utf-8")
            proc = _registry_verdict(reg)
            self.assertEqual(proc.returncode, 0, proc.stdout)

    def test_execution_chain_thread_env_policy__GAP_G2(self):
        """GAP-G2 执行链 env 维度 POLICY 裁决（不实现 run.py env 清洗）：说明性登记。

        裁决（V8-CI-009 GAP 补齐轮）：硬编码线程的注入通道是 registry 命令参数，
        已由 R9 在校验层拒绝；run.py 不负责清洗子进程环境变量（OMP_NUM_THREADS
        等由 hosted runner 干净环境保障）。本用例仅锚定裁决现状，不做注入。
        """
        src = (REPO / "ci" / "run.py").read_text(encoding="utf-8")
        self.assertNotIn("OMP_NUM_THREADS", src,
                         "GAP-G2 裁决变更：run.py 出现 env 清洗逻辑，"
                         "请同步把 NEGATIVE_MATRIX.md N08 env 维度更新为 REJECTED")


# ---------------------------------------------------------------- N09 ----

class TestN09HeavyNoMonitor(unittest.TestCase):
    """类 9：heavy 无 monitor → validate_registry --strict R7/R8 拒绝（exit 1）。"""

    def test_heavy_without_monitor_rejected_r7(self):
        with tempfile.TemporaryDirectory() as tmp:
            reg = Path(tmp) / "checks.json"
            reg.write_text(json.dumps({"schema_version": 1, "checks": [
                _check(id="N-HEAVY", heavy=True, requires_monitor=False,
                       profiles=["linux-deep"],
                       command=["python3", "ci/select_profile.py"])]}),
                encoding="utf-8")
            proc = _registry_verdict(reg)
            self.assertEqual(proc.returncode, 1)
            self.assertIn("R7", proc.stdout)

    def test_heavy_in_fast_profile_rejected_r8(self):
        with tempfile.TemporaryDirectory() as tmp:
            reg = Path(tmp) / "checks.json"
            reg.write_text(json.dumps({"schema_version": 1, "checks": [
                _check(id="N-FASTHEAVY", mutates_workspace=True,
                       profiles=["fast"],
                       command=["python3", "ci/select_profile.py"])]}),
                encoding="utf-8")
            proc = _registry_verdict(reg)
            self.assertEqual(proc.returncode, 1)
            self.assertIn("R8", proc.stdout)


# ---------------------------------------------------------------- N10 ----

class TestN10VersionDrift(unittest.TestCase):
    """类 10：版本漂移 → bootstrap exit 2（结构化 failed_tools）。"""

    def test_strict_policy_rejected_exit2(self):
        with tempfile.TemporaryDirectory() as tmp:
            policy = _strict_policy(Path(tmp))
            proc = H.sh([PY, str(BOOTSTRAP), "--platform", "linux",
                         "--policy", str(policy), "--json"], cwd=REPO, timeout=60)
            self.assertEqual(proc.returncode, 2)
            report = json.loads(proc.stdout)
            self.assertFalse(report["ok"])
            self.assertIn("gcc-99", report["failures"])

    def test_real_policy_on_this_host_rejected_exit2(self):
        proc = H.sh([PY, str(BOOTSTRAP), "--platform", "linux", "--json"],
                    cwd=REPO, timeout=60)
        self.assertEqual(proc.returncode, 2)


# ---------------------------------------------------------------- N11 ----

class TestN11ArtifactExfil(unittest.TestCase):
    """类 11：上传 FITS/headers/绝对路径（validate_candidate 排除规则 / 逃逸守卫）。

    - N11a/N11b（REJECTED）：.fits / .h 成员（含登记进 SHA256SUMS）→ exit 1
      excluded_entry_in_zip。
    - N11c（GAP-G3 → **REJECTED(本轮补齐)**）：绝对路径 / `..` 段成员（含
      SHA256SUMS 内登记路径）→ exit 1 path_escape_in_zip。
    """

    def test_fits_member_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            entries = _good_zip_manifest({"data/frame.fits": b"SIMPLE  = T"})
            proc = _validate_zip(_make_zip(Path(tmp), "fits", entries))
            self.assertEqual(proc.returncode, 1)
            self.assertIn("excluded_entry_in_zip", proc.stdout)

    def test_header_member_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            entries = _good_zip_manifest({"include/extra.h": b"#pragma once\n"})
            proc = _validate_zip(_make_zip(Path(tmp), "header", entries))
            self.assertEqual(proc.returncode, 1)
            self.assertIn("excluded_entry_in_zip", proc.stdout)

    def test_absolute_and_dotdot_members_rejected__GAP_G3_fixed(self):
        with tempfile.TemporaryDirectory() as tmp:
            entries = _good_zip_manifest({
                "/abs/escape.dll": b"UP", "../escape.dll": b"UP"})
            proc = _validate_zip(_make_zip(Path(tmp), "escape", entries))
            self.assertEqual(proc.returncode, 1)
            self.assertIn("path_escape_in_zip", proc.stdout)

    def test_sums_registered_escape_path_rejected__GAP_G3_fixed(self):
        """逃逸路径仅出现在 SHA256SUMS 登记行（zip 内无对应文件）同样拒绝。"""
        with tempfile.TemporaryDirectory() as tmp:
            entries = _good_zip_manifest({})
            sums = entries["SHA256SUMS"].decode("utf-8")
            fake = b"UP"
            entries["SHA256SUMS"] = (
                sums + f"{_sha256(fake)}  ../sums_escape.dll\n"
                       f"{_sha256(fake)}  /abs/sums_escape.dll\n").encode("utf-8")
            proc = _validate_zip(_make_zip(Path(tmp), "sums_escape", entries))
            self.assertEqual(proc.returncode, 1)
            self.assertIn("path_escape_in_zip", proc.stdout)


# ------------------------------------------------------------ N12..N13 ----

class TestN12FatduckCheckout(unittest.TestCase):
    """类 12：fatduck-validate 注入 checkout 步骤 → 测试层守卫拒绝。"""

    def test_checkout_step_rejected(self):
        doc = yaml.safe_load(FATDUCK_YML.read_text(encoding="utf-8"))
        sha = next(e["commit_sha"] for e in json.loads(
            LOCK_PATH.read_text(encoding="utf-8"))["entries"])
        doc["jobs"]["fatduck-validate"]["steps"].insert(0, {
            "name": "checkout", "uses": f"actions/checkout@{sha}"})
        with self.assertRaises(AssertionError):
            _assert_fatduck_shape(doc)

    def test_extra_run_step_rejected(self):
        doc = yaml.safe_load(FATDUCK_YML.read_text(encoding="utf-8"))
        doc["jobs"]["fatduck-validate"]["steps"].append({
            "name": "free", "shell": "pwsh", "run": "Write-Host 'free'"})
        with self.assertRaises(AssertionError):
            _assert_fatduck_shape(doc)

    def test_repo_relative_ci_path_rejected(self):
        doc = yaml.safe_load(FATDUCK_YML.read_text(encoding="utf-8"))
        step = next(s for s in doc["jobs"]["fatduck-validate"]["steps"]
                    if "run" in s)
        step["run"] = step["run"] + "\npython3 ci/validate_candidate.py"
        with self.assertRaises(AssertionError):
            _assert_fatduck_shape(doc)


class TestN13IssueTokenFatduck(unittest.TestCase):
    """类 13：Issue 写 token 落到 Fatduck → permissions 矩阵守卫拒绝。"""

    def test_issues_write_on_validate_job_rejected(self):
        doc = yaml.safe_load(FATDUCK_YML.read_text(encoding="utf-8"))
        doc["jobs"]["fatduck-validate"]["permissions"] = {
            "contents": "read", "issues": "write"}
        with self.assertRaises(AssertionError):
            _assert_fatduck_shape(doc)

    def test_top_level_issues_write_rejected(self):
        doc = yaml.safe_load(FATDUCK_YML.read_text(encoding="utf-8"))
        doc["permissions"] = {"contents": "read", "issues": "write"}
        with self.assertRaises(AssertionError):
            _assert_fatduck_shape(doc)


if __name__ == "__main__":
    unittest.main()
