# -*- coding: utf-8 -*-
"""V8-CI-008 单测：Fatduck workflow / select_candidate / notify 构造（全离线）。

覆盖：
1. fatduck.yml 存在且 yaml.safe_load 可解析；恰好三个 job 且名字固定；
2. 无手动触发（workflow_dispatch 词与键均不出现）；workflow_run 只来自
   ["AstroCS Windows CI"]（types completed、branches main）+ schedule 每 6 小时；
3. 三个 job runs-on 正确（self-hosted job 仅 [self-hosted, fatduck-realdata]）；
4. 权限矩阵：顶层与 select/fatduck 均 contents: read，仅 notify-owner 有
   issues: write；
5. concurrency group fatduck-<head_sha||schedule> 且 cancel-in-progress false；
6. fatduck-validate job 禁项：无 actions/checkout、run 步骤无
   python/pip/git 词、恰一个 run 步骤（固定本地 harness 入口
   D:\AstroCSRunner\harness\run_validation.ps1，固定 -CandidateZip/-SourceSha/
   -ResultDir 形态，无仓库相对路径 ci/）；download-artifact 指定 run-id 与
   github-token；upload 仅固定 publish 白名单目录（if-no-files-found: error）；
7. 所有 uses 锁定完整 SHA 且存在于 ci/actions.lock.json；
8. notify-owner：无 checkout、只评论单一固定 Issue（绝不新建）、变量缺失时
   结构化 skip（bash 实跑提取脚本 + mock gh CLI，两条路径）；
9. select_candidate 逻辑（进程内 mock gh_api，零网络）：workflow_run 与
   schedule 两条 happy path；head_branch!=main / conclusion!=success /
   windows 无 success / linux 同 SHA 不绿 / artifact 不匹配 -> exit 3 且
   reason_code 区分；网络失败与缺 repository -> exit 2；GITHUB_OUTPUT 行与
   --json 报告形状。
"""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import re
import stat
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import yaml

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

_WF_DIR = _REPO / ".github" / "workflows"
_FATDUCK_YML = _WF_DIR / "fatduck.yml"
_LOCK_PATH = _REPO / "ci" / "actions.lock.json"
_SHA_RE = re.compile(r"^[0-9a-f]{40}$")

_JOBS = ("select-candidate", "fatduck-validate", "notify-owner")


def _load_module(name: str, relpath: str):
    spec = importlib.util.spec_from_file_location(name, _REPO / relpath)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


SC = _load_module("select_candidate", "ci/select_candidate.py")


def _on(doc: dict) -> dict:
    """PyYAML 1.1 把裸 `on:` 解析成 True 键；两种形态都接受。"""
    return doc[True] if True in doc else doc["on"]


def _doc() -> dict:
    return yaml.safe_load(_FATDUCK_YML.read_text(encoding="utf-8"))


def _text() -> str:
    return _FATDUCK_YML.read_text(encoding="utf-8")


# ------------------------------------------------------------ workflow YAML ----

class TestFatduckWorkflowYaml(unittest.TestCase):
    """fatduck.yml 形状、触发器、权限、并发、job 禁项（全离线）。"""

    @classmethod
    def setUpClass(cls):
        cls.doc = _doc()

    def test_yaml_parse_and_exactly_three_jobs(self):
        doc = self.doc
        self.assertEqual(doc["name"], "AstroCS Fatduck Validation")
        self.assertEqual(tuple(doc["jobs"]), _JOBS)

    def test_no_manual_trigger(self):
        on = _on(self.doc)
        for banned in ("workflow_dispatch", "workflow_call", "push",
                       "pull_request", "release"):
            self.assertNotIn(banned, on, f"出现禁止触发器 {banned}")
        self.assertNotIn("workflow_dispatch", _text())

    def test_triggers_workflow_run_and_6h_schedule(self):
        on = _on(self.doc)
        wr = on["workflow_run"]
        self.assertEqual(wr["workflows"], ["AstroCS Windows CI"])
        self.assertEqual(wr["types"], ["completed"])
        self.assertEqual(wr["branches"], ["main"])
        self.assertEqual(on["schedule"], [{"cron": "0 */6 * * *"}])

    def test_runs_on_matrix(self):
        jobs = self.doc["jobs"]
        self.assertEqual(jobs["select-candidate"]["runs-on"], "ubuntu-24.04")
        self.assertEqual(jobs["notify-owner"]["runs-on"], "ubuntu-24.04")
        self.assertEqual(jobs["fatduck-validate"]["runs-on"],
                         ["self-hosted", "fatduck-realdata"])

    def test_permissions_matrix_only_notify_has_issues_write(self):
        doc = self.doc
        self.assertEqual(doc["permissions"], {"contents": "read"})
        jobs = doc["jobs"]
        self.assertEqual(jobs["select-candidate"]["permissions"],
                         {"contents": "read"})
        self.assertEqual(jobs["fatduck-validate"]["permissions"],
                         {"contents": "read"})
        self.assertEqual(jobs["notify-owner"]["permissions"],
                         {"contents": "read", "issues": "write"})
        writers = [name for name, job in jobs.items()
                   if (job.get("permissions") or {}).get("issues")]
        self.assertEqual(writers, ["notify-owner"])

    def test_concurrency_keeps_pending_no_cancel(self):
        c = self.doc["concurrency"]
        self.assertEqual(c["cancel-in-progress"], False)
        self.assertIn("fatduck-", c["group"])
        self.assertIn("github.event.workflow_run.head_sha", c["group"])
        self.assertIn("'schedule'", c["group"])

    def test_uses_pinned_to_locked_full_sha(self):
        lock_shas = {e["commit_sha"]
                     for e in json.loads(_LOCK_PATH.read_text(encoding="utf-8"))
                     ["entries"]}
        for job_name, job in self.doc["jobs"].items():
            for i, step in enumerate(job["steps"]):
                uses = step.get("uses")
                if not uses:
                    continue
                sha = uses.split("@", 1)[1].split()[0]
                self.assertRegex(sha, _SHA_RE,
                                 f"{job_name} step#{i}: {uses} 未锁完整 SHA")
                self.assertIn(sha, lock_shas,
                              f"{job_name} step#{i}: {uses} 不在 actions.lock")

    def test_validate_job_has_no_checkout_and_no_repo_script(self):
        job = self.doc["jobs"]["fatduck-validate"]
        uses = [s["uses"] for s in job["steps"] if "uses" in s]
        self.assertTrue(uses, "fatduck-validate 必须至少有 download/upload 步骤")
        for u in uses:
            self.assertFalse(u.startswith("actions/checkout"),
                             f"fatduck-validate 出现 checkout: {u}")
        run_steps = [s for s in job["steps"] if "run" in s]
        self.assertEqual(len(run_steps), 1,
                         "fatduck-validate 只允许一个 run 步骤（固定 harness）")
        script = run_steps[0]["run"]
        self.assertFalse(re.search(r"\bpython3?\b", script),
                         "fatduck-validate run 步骤出现 python")
        self.assertFalse(re.search(r"\bpip3?\b", script),
                         "fatduck-validate run 步骤出现 pip")
        self.assertFalse(re.search(r"\bgit\b", script),
                         "fatduck-validate run 步骤出现 git 调用")
        self.assertNotIn("ci/", script, "fatduck-validate 出现仓库相对路径")
        self.assertEqual(run_steps[0].get("shell"), "pwsh")

    def test_validate_job_fixed_harness_entry_and_args(self):
        script = next(s["run"] for s in self.doc["jobs"]["fatduck-validate"]["steps"]
                      if "run" in s)
        self.assertIn("& 'D:\\AstroCSRunner\\harness\\run_validation.ps1'", script)
        self.assertIn("-CandidateZip 'D:\\AstroCSRunner\\runs\\incoming\\AstroCS-candidate.zip'",
                      script)
        self.assertIn("-SourceSha '${{ needs.select-candidate.outputs.source_sha }}'",
                      script)
        self.assertIn("-ResultDir 'D:\\AstroCSRunner\\runs\\publish\\",
                      script)
        self.assertIn("${{ needs.select-candidate.outputs.source_sha }}'", script)

    def test_validate_job_downloads_specify_run_id_and_token(self):
        downloads = [s for s in self.doc["jobs"]["fatduck-validate"]["steps"]
                     if s.get("uses", "").startswith("actions/download-artifact")]
        self.assertEqual(len(downloads), 2, "candidate + CI evidence 两次下载")
        for step in downloads:
            with_ = step["with"]
            self.assertIn("run-id", with_, step["name"])
            self.assertIn("github-token", with_, step["name"])
        names = {s["with"]["name"] for s in downloads}
        self.assertIn("${{ needs.select-candidate.outputs.artifact_name }}", names)
        self.assertIn("windows-ci-${{ needs.select-candidate.outputs.source_sha }}",
                      names)

    def test_validate_job_uploads_fixed_publish_whitelist_only(self):
        uploads = [s for s in self.doc["jobs"]["fatduck-validate"]["steps"]
                   if s.get("uses", "").startswith("actions/upload-artifact")]
        self.assertEqual(len(uploads), 1)
        with_ = uploads[0]["with"]
        self.assertTrue(with_["path"].startswith("D:\\AstroCSRunner\\runs\\publish\\"),
                        "只允许上传固定 publish 白名单目录")
        self.assertTrue(with_["path"].endswith("public"),
                        "上传目录必须是固定 publish 白名单 public 子目录")
        self.assertEqual(with_["if-no-files-found"], "error")
        self.assertEqual(uploads[0].get("if"), "success()")

    def test_validate_job_gate_and_notify_conditions(self):
        jobs = self.doc["jobs"]
        self.assertEqual(jobs["fatduck-validate"]["if"],
                         "needs.select-candidate.outputs.has_candidate == 'true'")
        self.assertEqual(jobs["notify-owner"]["if"], "always()")
        for step in jobs["notify-owner"]["steps"]:
            self.assertIn("needs.fatduck-validate.result == 'success'",
                          step.get("if", ""), step["name"])

    def test_notify_owner_no_checkout_single_issue_comment_only(self):
        job = self.doc["jobs"]["notify-owner"]
        for step in job["steps"]:
            self.assertFalse(step.get("uses", "").startswith("actions/checkout"),
                             "notify-owner 不得 checkout")
        script = next(s["run"] for s in job["steps"] if "run" in s)
        self.assertFalse(re.search(r"gh\s+issue\s+create", script),
                         "notify-owner 不得新建 issue")
        self.assertFalse(re.search(r"issues\s+create", script),
                         "notify-owner 不得新建 issue")
        self.assertIn("/issues/$issue/comments", script)
        self.assertIn("${{ vars.ASTROCS_OWNER_REVIEW_ISSUE }}",
                      next(s for s in job["steps"] if "run" in s)["env"]
                      ["OWNER_REVIEW_ISSUE"])
        # JPG artifact 链接 + 本地打开命令（V8-FAT-003）
        self.assertIn("#artifacts", script)
        self.assertIn("astrocs-cli review open --run", script)
        # 变量缺失/非法 -> 结构化 skip
        self.assertIn("notify=skipped_missing_issue_variable", script)
        self.assertIn("notify=skipped_invalid_issue_variable", script)

    def test_select_job_wires_selector_outputs_to_gate(self):
        job = self.doc["jobs"]["select-candidate"]
        sel = next(s for s in job["steps"] if s.get("id") == "sel")
        script = sel["run"]
        self.assertIn("ci/select_candidate.py", script)
        for flag in ("--event", "--head-branch", "--conclusion", "--run-id"):
            self.assertIn(flag, script)
        self.assertIn("GITHUB_TOKEN", sel.get("env", {}))
        self.assertEqual(job["outputs"]["has_candidate"],
                         "${{ steps.gate.outputs.proceed }}")


# -------------------------------------------------- select_candidate 逻辑 ----

def _fake_gh_api(routes: dict):
    """按 URL 子串分发 (status, payload) 的 gh_api mock 工厂。"""
    def fake(url, *, timeout, log_file=None):
        for needle, (status, payload) in routes.items():
            if needle in url:
                return status, payload
        raise AssertionError(f"mock 未覆盖 URL: {url}")
    return fake


_S = "b" * 40  # 测试 source SHA


class TestSelectCandidateLogic(unittest.TestCase):
    """进程内 mock gh_api，零网络；逐分支断言退出码与 reason_code。"""

    def _run(self, argv):
        out = io.StringIO()
        err = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = SC.main(argv)
        return rc, out.getvalue(), err.getvalue()

    def _happy_routes(self, windows_first: bool):
        run_shape = {"id": 777 if windows_first else None, "name": "AstroCS Windows CI",
                     "head_sha": _S, "head_branch": "main",
                     "html_url": "https://github.invalid/o/r/actions/runs/777"}
        if windows_first:
            detail = dict(run_shape, id=777)
            return {
                # 先长后短：artifacts URL 含 "/actions/runs/777" 子串，必须先匹配
                "/actions/runs/777/artifacts": (200, {"artifacts": [
                    {"id": 55, "name": f"astrocs-windows-candidate-{_S}",
                     "digest": "sha256:abc", "size_in_bytes": 1234,
                     "expired": False}]}),
                "ci-linux.yml/runs": (200, {"workflow_runs": [
                    {"id": 9, "head_sha": _S}]}),
                "/actions/runs/777": (200, detail),
            }
        return {
            "ci-windows.yml/runs": (200, {"workflow_runs": [
                {"id": 777, "head_sha": _S, "html_url": "u"}]}),
            "ci-linux.yml/runs": (200, {"workflow_runs": [
                {"id": 9, "head_sha": _S}]}),
            "/actions/runs/777/artifacts": (200, {"artifacts": [
                {"id": 55, "name": f"astrocs-windows-candidate-{_S}",
                 "digest": "sha256:abc", "size_in_bytes": 1234,
                 "expired": False}]}),
        }

    def test_workflow_run_happy_path(self):
        argv = ["--event", "workflow_run", "--head-branch", "main",
                "--conclusion", "success", "--run-id", "777",
                "--repository", "o/r", "--json"]
        with mock.patch.object(SC, "gh_api",
                               side_effect=_fake_gh_api(self._happy_routes(True))):
            rc, out, _ = self._run(argv)
        self.assertEqual(rc, 0)
        report = json.loads(out)
        self.assertEqual(report["verdict"], "candidate")
        self.assertEqual(report["source_sha"], _S)
        self.assertEqual(report["windows_run_id"], "777")
        self.assertEqual(report["artifact_name"],
                         f"astrocs-windows-candidate-{_S}")
        self.assertEqual(report["artifact_digest"], "sha256:abc")
        self.assertEqual(report["artifact_size"], 1234)

    def test_schedule_happy_path(self):
        argv = ["--event", "schedule", "--repository", "o/r", "--json"]
        with mock.patch.object(SC, "gh_api",
                               side_effect=_fake_gh_api(self._happy_routes(False))):
            rc, out, _ = self._run(argv)
        self.assertEqual(rc, 0)
        report = json.loads(out)
        self.assertEqual(report["verdict"], "candidate")
        self.assertEqual(report["event"], "schedule")
        self.assertEqual(report["source_sha"], _S)

    def test_head_branch_not_main_exit3_no_api_call(self):
        argv = ["--event", "workflow_run", "--head-branch", "feature/x",
                "--conclusion", "success", "--run-id", "777",
                "--repository", "o/r", "--json"]
        with mock.patch.object(SC, "gh_api",
                               side_effect=AssertionError("不应触网")) as m:
            rc, out, _ = self._run(argv)
        self.assertEqual(rc, 3)
        m.assert_not_called()
        report = json.loads(out)
        self.assertEqual(report["verdict"], "no_candidate")
        self.assertEqual(report["reason_code"], "head_branch_not_main")

    def test_conclusion_not_success_exit3(self):
        argv = ["--event", "workflow_run", "--head-branch", "main",
                "--conclusion", "failure", "--run-id", "777",
                "--repository", "o/r", "--json"]
        with mock.patch.object(SC, "gh_api",
                               side_effect=AssertionError("不应触网")) as m:
            rc, out, _ = self._run(argv)
        self.assertEqual(rc, 3)
        m.assert_not_called()
        self.assertEqual(json.loads(out)["reason_code"], "trigger_run_not_success")

    def test_trigger_run_not_windows_workflow_exit3(self):
        routes = {"/actions/runs/777": (200, {"name": "Other Workflow",
                                              "head_sha": _S})}
        argv = ["--event", "workflow_run", "--head-branch", "main",
                "--conclusion", "success", "--run-id", "777",
                "--repository", "o/r", "--json"]
        with mock.patch.object(SC, "gh_api", side_effect=_fake_gh_api(routes)):
            rc, out, _ = self._run(argv)
        self.assertEqual(rc, 3)
        self.assertEqual(json.loads(out)["reason_code"],
                         "trigger_run_not_windows_workflow")

    def test_no_success_windows_run_exit3(self):
        routes = {"ci-windows.yml/runs": (200, {"workflow_runs": []})}
        argv = ["--event", "schedule", "--repository", "o/r", "--json"]
        with mock.patch.object(SC, "gh_api", side_effect=_fake_gh_api(routes)):
            rc, out, _ = self._run(argv)
        self.assertEqual(rc, 3)
        self.assertEqual(json.loads(out)["reason_code"],
                         "no_success_windows_run")

    def test_linux_not_green_exit3(self):
        routes = {
            "ci-windows.yml/runs": (200, {"workflow_runs": [
                {"id": 777, "head_sha": _S, "html_url": "u"}]}),
            "ci-linux.yml/runs": (200, {"workflow_runs": []}),
        }
        argv = ["--event", "schedule", "--repository", "o/r", "--json"]
        with mock.patch.object(SC, "gh_api", side_effect=_fake_gh_api(routes)):
            rc, out, _ = self._run(argv)
        self.assertEqual(rc, 3)
        self.assertEqual(json.loads(out)["reason_code"], "linux_not_green")

    def test_candidate_artifact_missing_exit3(self):
        routes = {
            "ci-windows.yml/runs": (200, {"workflow_runs": [
                {"id": 777, "head_sha": _S, "html_url": "u"}]}),
            "ci-linux.yml/runs": (200, {"workflow_runs": [
                {"id": 9, "head_sha": _S}]}),
            "/actions/runs/777/artifacts": (200, {"artifacts": [
                {"id": 56, "name": "windows-ci-other", "digest": None,
                 "size_in_bytes": 1}]}),
        }
        argv = ["--event", "schedule", "--repository", "o/r", "--json"]
        with mock.patch.object(SC, "gh_api", side_effect=_fake_gh_api(routes)):
            rc, out, _ = self._run(argv)
        self.assertEqual(rc, 3)
        self.assertEqual(json.loads(out)["reason_code"],
                         "candidate_artifact_missing")

    def test_network_unavailable_exit2(self):
        argv = ["--event", "schedule", "--repository", "o/r", "--json"]
        with mock.patch.object(SC, "gh_api", side_effect=SC.EnvUnavailable(
                "network_unavailable", "mock down")):
            rc, out, _ = self._run(argv)
        self.assertEqual(rc, 2)
        self.assertEqual(json.loads(out)["reason_code"], "network_unavailable")

    def test_repository_context_missing_exit2(self):
        argv = ["--event", "schedule", "--json"]
        saved = os.environ.pop("GITHUB_REPOSITORY", None)
        try:
            rc, out, err = self._run(argv)
        finally:
            if saved is not None:
                os.environ["GITHUB_REPOSITORY"] = saved
        self.assertEqual(rc, 2)
        self.assertEqual(json.loads(out)["reason_code"],
                         "repository_context_missing")

    def test_github_output_lines_candidate_and_no_candidate(self):
        candidate = {"verdict": "candidate", "source_sha": _S,
                     "windows_run_id": "777",
                     "artifact_name": f"astrocs-windows-candidate-{_S}",
                     "artifact_digest": "sha256:abc", "artifact_size": 1234}
        lines = SC.github_output_lines(candidate)
        self.assertIn("has_candidate=true", lines)
        self.assertIn(f"source_sha={_S}", lines)
        self.assertIn("windows_run_id=777", lines)
        self.assertIn("artifact_digest=sha256:abc", lines)
        self.assertIn("no_candidate_reason=\n", lines + "\n")
        failed = {"verdict": "no_candidate", "reason_code": "linux_not_green"}
        lines2 = SC.github_output_lines(failed)
        self.assertIn("has_candidate=false", lines2)
        self.assertIn("no_candidate_reason=linux_not_green", lines2)

    def test_append_github_output_writes_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            out_path = Path(tmp) / "github_output.txt"
            saved = os.environ.get("GITHUB_OUTPUT")
            os.environ["GITHUB_OUTPUT"] = str(out_path)
            try:
                SC.append_github_output({"verdict": "candidate",
                                         "source_sha": _S,
                                         "windows_run_id": "777",
                                         "artifact_name": "a",
                                         "artifact_digest": "d",
                                         "artifact_size": 9})
            finally:
                if saved is None:
                    os.environ.pop("GITHUB_OUTPUT", None)
                else:
                    os.environ["GITHUB_OUTPUT"] = saved
            content = out_path.read_text(encoding="utf-8")
        self.assertIn("has_candidate=true", content)
        self.assertIn(f"source_sha={_S}", content)


# ------------------------------------------------- notify 命令构造（实跑） ----

class TestNotifyCommandConstruction(unittest.TestCase):
    """提取 notify 脚本在 bash 中实跑（mock gh CLI，零网络、零写仓库）。"""

    @classmethod
    def setUpClass(cls):
        job = _doc()["jobs"]["notify-owner"]
        script = next(s["run"] for s in job["steps"] if "run" in s)
        # CI 表达式 -> 本地 env（命令构造测试的最小替换，明确注释）
        script = script.replace("${{ github.repository }}", "${NOTIFY_REPO}")
        cls.script = script

    def _run_bash(self, env_extra: dict):
        with tempfile.TemporaryDirectory() as tmp:
            tmpdir = Path(tmp)
            script_path = tmpdir / "notify.sh"
            script_path.write_text(self.script, encoding="utf-8")
            bin_dir = tmpdir / "bin"
            bin_dir.mkdir()
            call_log = tmpdir / "gh_calls.log"
            body_dump = tmpdir / "gh_body.txt"
            gh = bin_dir / "gh"
            gh.write_text(
                "#!/usr/bin/env bash\n"
                "printf '%s\\n' \"$@\" >> \"$GH_CALL_LOG\"\n"
                "for arg in \"$@\"; do\n"
                "  case \"$arg\" in body=*) printf '%s' \"${arg#body=}\""
                " > \"$GH_BODY_DUMP\";; esac\n"
                "done\n"
                "echo '{\"html_url\":\"https://github.invalid/x/issues/7\"}'\n",
                encoding="utf-8")
            gh.chmod(gh.stat().st_mode | stat.S_IEXEC)
            env = {"PATH": f"{bin_dir}:{os.environ['PATH']}",
                   "GH_CALL_LOG": str(call_log),
                   "GH_BODY_DUMP": str(body_dump),
                   "NOTIFY_REPO": "astrocs-owner/astrocs",
                   "SOURCE_SHA": _S,
                   "RUN_URL": "https://github.invalid/astrocs-owner/astrocs/actions/runs/42"}
            env.update(env_extra)
            import subprocess
            proc = subprocess.run(["bash", str(script_path)], env=env,
                                  capture_output=True, text=True,
                                  encoding="utf-8", errors="replace",
                                  timeout=60)
            calls = call_log.read_text(encoding="utf-8") if call_log.exists() else ""
            body = body_dump.read_text(encoding="utf-8") if body_dump.exists() else ""
        return proc, calls, body

    def test_skip_when_issue_variable_missing(self):
        proc, calls, body = self._run_bash({})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("notify=skipped_missing_issue_variable", proc.stdout)
        self.assertEqual(calls, "", "issue 变量缺失时绝不能调用 gh api")
        self.assertEqual(body, "")

    def test_skip_when_issue_variable_not_numeric(self):
        proc, calls, _ = self._run_bash({"OWNER_REVIEW_ISSUE": "abc"})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("notify=skipped_invalid_issue_variable", proc.stdout)
        self.assertEqual(calls, "")

    def test_comment_single_issue_contains_jpg_link_and_open_command(self):
        proc, calls, body = self._run_bash({"OWNER_REVIEW_ISSUE": "7"})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("notify=commented issue=7", proc.stdout)
        # 单一固定 Issue 追加评论：POST .../issues/7/comments
        self.assertIn("--method", calls)
        self.assertIn("POST", calls)
        self.assertIn("/repos/astrocs-owner/astrocs/issues/7/comments", calls)
        self.assertNotIn("issues create", calls)
        self.assertNotIn("issues -X", calls)
        # 评论体：JPG artifact 链接、指标摘要、source SHA、本地打开命令
        self.assertIn("#artifacts", body)
        self.assertIn("astrocs-cli review open --run", body)
        self.assertIn(_S, body)
        self.assertIn("fatduck-public-", body)


if __name__ == "__main__":
    unittest.main()
