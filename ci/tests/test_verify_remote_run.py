# -*- coding: utf-8 -*-
"""V8-CI-012 F-D4 单测：ci/verify_remote_run.py（fixture 假 API，零真网络）。

覆盖：
1. --offline 结构模式：参数合法 exit 0 + 报告契约（verdict/mode/workflows）；
   参数非法（sha 形态、缺 --workflows、timeout<=0）exit 3 中文报错；
   全程不触 transport；
2. 在线核验（monkeypatch _http_get/_http_get_bytes 注入假 API 响应）：
   dispatch 优先于 push、push 回退、全过 exit 0；
3. 失败 verdict → exit 1：该 SHA 无 run、head_sha 不符、run 未完成、
   job 失败、profile 证据不一致、artifact/CI_RESULT.json 缺失；
4. API/环境问题 → exit 2：网络异常（URLError）、API 非 2xx；
5. 用法/解析 → exit 3：--workflows 无法唯一解析、--repo 形态非法；
6. 凭据纪律：Authorization 仅在 GITHUB_TOKEN 环境时注入；token 不进输出。

传输层接缝：verify_remote_run._http_get / _http_get_bytes（stdlib-only，
生产路径 urllib；测试路径按 URL 路由到 fixture 数据）。
"""
from __future__ import annotations

import importlib.util
from importlib.util import module_from_spec, spec_from_file_location
import io
import json
import os
import re
import subprocess
import sys
import unittest
import urllib.error
import zipfile
from pathlib import Path
from unittest import mock

_REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_REPO))

SCRIPT = _REPO / "ci" / "verify_remote_run.py"
REPO_ID = "astrocs/astrocs"          # 测试经 --repo 显式传仓库，不依赖 git remote
SHA = "d416fc9df7767b6a407994048203667a0ef8cbaa"


def _load_module():
    spec = importlib.util.spec_from_file_location(
        f"verify_remote_run_{id(unittest)}", SCRIPT)
    mod = module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _run_cli(*args: str):
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args], cwd=str(_REPO),
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=60)


def _ci_result_zip(profile: str, *, include: bool = True) -> bytes:
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as zf:
        if include:
            zf.writestr("artifacts/ci/CI_RESULT.json", json.dumps(
                {"profile": profile, "summary": {"verdict": "FAIL",
                                                 "total": 7, "pass": 3}}))
    return buf.getvalue()


def _fake_world(*, workflows=None, runs=None, jobs=None, artifacts=None,
                zip_body=None, list_status=200, runs_status=200,
                jobs_status=200, artifacts_status=200):
    """构造按 URL 路由的假传输层；返回 (fake_get, fake_bytes, calls)。"""
    workflows = [{"id": 99001, "name": "AstroCS Linux CI",
                  "path": ".github/workflows/ci-linux.yml", "state": "active"},
                 {"id": 99002, "name": "AstroCS Windows CI",
                  "path": ".github/workflows/ci-windows.yml", "state": "active"}
                 ] if workflows is None else workflows
    runs = [{"id": 555001, "event": "workflow_dispatch", "status": "completed",
             "conclusion": "success", "head_sha": SHA,
             "html_url": "https://example.invalid/run/555001"},
            {"id": 555002, "event": "push", "status": "completed",
             "conclusion": "success", "head_sha": SHA}] if runs is None else runs
    jobs = [{"id": 1, "name": "linux", "conclusion": "success"}
            ] if jobs is None else jobs
    artifacts = [{"id": 777, "name": f"linux-ci-{SHA}", "expired": False}
                 ] if artifacts is None else artifacts
    zip_body = (_ci_result_zip("linux-deep") if zip_body is None else zip_body)
    calls: list[str] = []

    def fake_get(url: str, timeout: float):
        calls.append(url)
        if url.startswith("https://api.github.com/repos/") and \
                url.split("?", 1)[0].endswith("/actions/workflows"):
            return list_status, json.dumps(
                {"workflows": workflows}).encode("utf-8")
        if "/actions/workflows/99001/runs" in url:
            return runs_status, json.dumps(
                {"workflow_runs": runs, "total_count": len(runs)}
            ).encode("utf-8")
        run_match = re.search(r"/actions/runs/(\d+)/jobs", url)
        if run_match:
            return jobs_status, json.dumps({"jobs": jobs}).encode("utf-8")
        if re.search(r"/actions/runs/(\d+)/artifacts", url):
            return artifacts_status, json.dumps(
                {"artifacts": artifacts, "total_count": len(artifacts)}
            ).encode("utf-8")
        return 404, b"{}"

    def fake_bytes(url: str, timeout: float):
        calls.append(url)
        assert "/actions/artifacts/777/zip" in url, url
        return 200, zip_body

    return fake_get, fake_bytes, calls


# ---------------------------------------------------------------- 离线模式 ----

class TestOfflineMode(unittest.TestCase):
    """--offline：仅校验参数与输出契约，零网络。"""

    def test_offline_pass_contract_and_no_transport(self):
        VRR = _load_module()
        with mock.patch.object(VRR, "_http_get",
                               side_effect=AssertionError("offline 触网")), \
             mock.patch.object(VRR, "_http_get_bytes",
                               side_effect=AssertionError("offline 触网")):
            rc = VRR.main(["--offline", "--sha", SHA, "--workflows", "linux-ci",
                           "--profile", "linux-deep", "--json"])
        self.assertEqual(rc, 0)

    def test_offline_cli_json_shape(self):
        res = _run_cli("--offline", "--sha", SHA, "--workflows", "linux-ci",
                       "--profile", "linux-deep", "--json")
        self.assertEqual(res.returncode, 0, res.stderr)
        report = json.loads(res.stdout)
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["mode"], "offline")
        self.assertEqual(report["sha"], SHA)
        self.assertEqual(report["profile"], "linux-deep")
        self.assertEqual([w["key"] for w in report["workflows"]], ["linux-ci"])

    def test_offline_bad_sha_exit3(self):
        res = _run_cli("--offline", "--sha", "abc123", "--workflows", "linux-ci")
        self.assertEqual(res.returncode, 3)
        self.assertIn("用法错误", res.stderr)

    def test_offline_missing_workflows_exit3(self):
        res = _run_cli("--offline", "--sha", SHA)
        self.assertEqual(res.returncode, 3)
        self.assertIn("用法错误", res.stderr)

    def test_offline_bad_timeout_exit3(self):
        res = _run_cli("--offline", "--sha", SHA, "--workflows", "linux-ci",
                       "--timeout", "0")
        self.assertEqual(res.returncode, 3)
        self.assertIn("用法错误", res.stderr)


# ---------------------------------------------------------------- 在线核验 ----

class TestOnlineVerify(unittest.TestCase):
    """假 API 注入：优先级 / 回退 / 全过路径。"""

    def _main(self, extra=None, world=None):
        world = world or _fake_world()
        fake_get, fake_bytes, calls = world
        VRR = _load_module()
        argv = ["--sha", SHA, "--workflows", "linux-ci", "--json",
                "--repo", REPO_ID, "--profile", "linux-deep", *(extra or [])]
        out = io.StringIO()
        with mock.patch.object(VRR, "_http_get", side_effect=fake_get), \
             mock.patch.object(VRR, "_http_get_bytes", side_effect=fake_bytes), \
             mock.patch("sys.stdout", out):
            rc = VRR.main(argv)
        return VRR, rc, calls

    def test_pass_dispatch_preferred_over_push(self):
        # 经 main + stdout 捕获复核 JSON 报告（报告以 stdout 为准）
        VRR = _load_module()
        fake_get, fake_bytes, _ = _fake_world()
        out = io.StringIO()
        with mock.patch.object(VRR, "_http_get", side_effect=fake_get), \
             mock.patch.object(VRR, "_http_get_bytes", side_effect=fake_bytes), \
             mock.patch("sys.stdout", out):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci", "--json",
                           "--repo", REPO_ID, "--profile", "linux-deep"])
        self.assertEqual(rc, 0)
        report = json.loads(out.getvalue())
        self.assertEqual(report["verdict"], "PASS")
        entry = report["workflows"][0]
        self.assertEqual(entry["run"]["id"], 555001)       # dispatch 优先
        self.assertEqual(entry["run"]["event"], "workflow_dispatch")
        self.assertTrue(entry["profile_evidence"]["ok"])
        self.assertEqual(entry["profile_evidence"]["observed_profile"],
                         "linux-deep")
        self.assertEqual(entry["workflow"]["path"],
                         ".github/workflows/ci-linux.yml")  # linux-ci 词集指认

    def test_pass_push_fallback_when_no_dispatch(self):
        world = _fake_world(runs=[{"id": 555002, "event": "push",
                                   "status": "completed",
                                   "conclusion": "success", "head_sha": SHA}])
        VRR, rc, _ = self._main(world=world)
        self.assertEqual(rc, 0)

    def test_fail_no_run_for_sha(self):
        world = _fake_world(runs=[])
        VRR, rc, _ = self._main(world=world)
        self.assertEqual(rc, 1)

    def test_fail_head_sha_mismatch(self):
        world = _fake_world(runs=[{"id": 555001, "event": "workflow_dispatch",
                                   "status": "completed",
                                   "conclusion": "success",
                                   "head_sha": "b" * 40}])
        VRR, rc, _ = self._main(world=world)
        self.assertEqual(rc, 1)

    def test_fail_run_not_completed(self):
        world = _fake_world(runs=[{"id": 555001, "event": "workflow_dispatch",
                                   "status": "in_progress",
                                   "conclusion": None, "head_sha": SHA}])
        VRR, rc, _ = self._main(world=world)
        self.assertEqual(rc, 1)

    def test_fail_job_failure(self):
        world = _fake_world(jobs=[{"id": 1, "name": "linux",
                                   "conclusion": "failure"}])
        VRR, rc, _ = self._main(world=world)
        self.assertEqual(rc, 1)

    def test_fail_profile_mismatch(self):
        world = _fake_world(zip_body=_ci_result_zip("linux-main"))
        VRR, rc, _ = self._main(world=world)
        self.assertEqual(rc, 1)

    def test_fail_ci_result_member_missing(self):
        world = _fake_world(zip_body=_ci_result_zip("linux-deep", include=False))
        VRR, rc, _ = self._main(world=world)
        self.assertEqual(rc, 1)

    def test_fail_artifact_missing(self):
        world = _fake_world(artifacts=[])
        VRR, rc, _ = self._main(world=world)
        self.assertEqual(rc, 1)

    def test_fail_no_profile_given_skips_evidence(self):
        # 不给 --profile：不做 artifact 核验（也即不请求 zip）
        VRR = _load_module()
        fake_get, fake_bytes, calls = _fake_world()
        with mock.patch.object(VRR, "_http_get", side_effect=fake_get), \
             mock.patch.object(VRR, "_http_get_bytes", side_effect=fake_bytes):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci", "--json",
                           "--repo", REPO_ID])
        self.assertEqual(rc, 0)
        self.assertFalse(any("/zip" in c for c in calls))


# ----------------------------------------------------------- API/用法错误 ----

class TestApiAndUsage(unittest.TestCase):
    """exit 2 = API/环境；exit 3 = 用法/解析。"""

    def test_env_network_unreachable_exit2(self):
        VRR = _load_module()
        # patch urlopen 而非 _http_get：覆盖生产路径 URLError → ApiError 转换
        with mock.patch("urllib.request.urlopen",
                        side_effect=urllib.error.URLError("connection refused")):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci",
                           "--repo", REPO_ID])
        self.assertEqual(rc, 2)

    def test_env_api_non_2xx_exit2(self):
        world = _fake_world(list_status=404)
        fake_get, fake_bytes, _ = world
        VRR = _load_module()
        with mock.patch.object(VRR, "_http_get", side_effect=fake_get):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci",
                           "--repo", REPO_ID])
        self.assertEqual(rc, 2)

    def test_usage_workflow_unresolvable_exit3(self):
        world = _fake_world(workflows=[
            {"id": 1, "name": "Unrelated", "path": ".github/workflows/other.yml"}])
        fake_get, fake_bytes, _ = world
        VRR = _load_module()
        with mock.patch.object(VRR, "_http_get", side_effect=fake_get):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci",
                           "--repo", REPO_ID])
        self.assertEqual(rc, 3)

    def test_usage_ambiguous_workflow_exit3(self):
        world = _fake_world(workflows=[
            {"id": 1, "name": "Linux One", "path": ".github/workflows/a.yml"},
            {"id": 2, "name": "Linux Two", "path": ".github/workflows/b.yml"}])
        fake_get, fake_bytes, _ = world
        VRR = _load_module()
        with mock.patch.object(VRR, "_http_get", side_effect=fake_get):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux",
                           "--repo", REPO_ID])
        self.assertEqual(rc, 3)

    def test_usage_bad_repo_exit3(self):
        VRR = _load_module()
        with mock.patch.object(VRR, "_http_get",
                               side_effect=AssertionError("不应触网")):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci",
                           "--repo", "not-a-repo-slash"])
        self.assertEqual(rc, 3)


# ---------------------------------------------------------------- 凭据纪律 ----

class TestCredentialDiscipline(unittest.TestCase):
    """token 仅经 GITHUB_TOKEN 环境内联进 Authorization；零落盘、零输出。"""

    def test_token_header_only_from_env(self):
        VRR = _load_module()
        with mock.patch.dict(os.environ, {}, clear=False):
            import os as _os
            _os.environ.pop("GITHUB_TOKEN", None)
            self.assertNotIn("Authorization", VRR._build_headers())
        with mock.patch.dict(os.environ, {"GITHUB_TOKEN": "tok-secret-123"}):
            headers = VRR._build_headers()
        self.assertEqual(headers.get("Authorization"), "Bearer tok-secret-123")

    def test_token_never_in_output(self):
        VRR = _load_module()
        fake_get, fake_bytes, _ = _fake_world()
        out = io.StringIO()
        with mock.patch.dict(os.environ, {"GITHUB_TOKEN": "tok-secret-123"}), \
             mock.patch.object(VRR, "_http_get", side_effect=fake_get), \
             mock.patch.object(VRR, "_http_get_bytes", side_effect=fake_bytes), \
             mock.patch("sys.stdout", out), \
             mock.patch("sys.stderr", out):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci", "--json",
                           "--repo", REPO_ID, "--profile", "linux-deep"])
        self.assertEqual(rc, 0)
        self.assertNotIn("tok-secret-123", out.getvalue())


# -------------------------------------------------------- 重定向凭据纪律 ----

class _FakeRedirectResponse:
    """fake opener 返回的假 2xx 响应（context manager 形态）。"""

    def __init__(self, status: int, body: bytes):
        self.status = status
        self._body = body

    def read(self) -> bytes:
        return self._body

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        return False


class _FakeRedirectOpener:
    """可注入 transport：按脚本序列返回响应/抛 3xx HTTPError，并记录每跳请求。

    entries 元素：(status, body, location|None)。status∈{301,302,307} 且
    location 非空时抛对应 HTTPError（含 Location 头）；否则返回 2xx 响应。
    requests 列表记录每跳 (full_url, Authorization|None)。
    """

    def __init__(self, entries):
        self.entries = list(entries)
        self.requests: list[tuple[str, str | None]] = []

    def open(self, request, timeout=None):
        self.requests.append((request.get_full_url(),
                              request.get_header("Authorization")))
        status, body, location = self.entries.pop(0)
        if status in (301, 302, 307) and location:
            headers = {"Location": location}
            raise urllib.error.HTTPError(request.get_full_url(), status,
                                         "redirect", headers, io.BytesIO(b""))
        return _FakeRedirectResponse(status, body)


class TestRedirectCredentialDiscipline(unittest.TestCase):
    """F5：artifact zip 302 → Azure Blob 跨主机重定向必须剥离 Authorization。"""

    ZIP_BODY = _ci_result_zip("linux-deep")

    def test_cross_host_302_strips_authorization(self):
        # 首跳 api.github.com 带 Authorization；302 → blob 端点二跳必须无
        # Authorization（跨主机剥离）；最终拿到 zip 并 PASS。
        opener = _FakeRedirectOpener([
            (302, b"", "https://productionresultssa.blob.core.windows.net/x?sig=1"),
            (200, self.ZIP_BODY, None)])
        world = _fake_world()
        fake_get, _, _ = world
        VRR = _load_module()
        out = io.StringIO()
        with mock.patch.dict(os.environ, {"GITHUB_TOKEN": "tok-secret-123"}), \
             mock.patch.object(VRR, "_http_get", side_effect=fake_get), \
             mock.patch.object(VRR, "_OPENER", opener), \
             mock.patch("sys.stdout", out), \
             mock.patch("sys.stderr", out):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci", "--json",
                           "--repo", REPO_ID, "--profile", "linux-deep"])
        self.assertEqual(rc, 0, out.getvalue())
        self.assertEqual(len(opener.requests), 2)
        first_url, first_auth = opener.requests[0]
        second_url, second_auth = opener.requests[1]
        self.assertIn("/actions/artifacts/777/zip", first_url)
        self.assertEqual(first_auth, "Bearer tok-secret-123")   # 首跳带凭据
        self.assertIn("blob.core.windows.net", second_url)
        self.assertIsNone(second_auth)                          # 二跳已剥离
        report = json.loads(out.getvalue())
        self.assertEqual(report["verdict"], "PASS")

    def test_same_host_302_keeps_authorization(self):
        # 同主机 302（Location 相对路径）保留 Authorization，不做过度剥离。
        opener = _FakeRedirectOpener([
            (302, b"", "/repos/astrocs/astrocs/actions/artifacts/777/zip?again=1"),
            (200, self.ZIP_BODY, None)])
        world = _fake_world()
        fake_get, _, _ = world
        VRR = _load_module()
        with mock.patch.dict(os.environ, {"GITHUB_TOKEN": "tok-secret-123"}), \
             mock.patch.object(VRR, "_http_get", side_effect=fake_get), \
             mock.patch.object(VRR, "_OPENER", opener):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci",
                           "--repo", REPO_ID, "--profile", "linux-deep"])
        self.assertEqual(rc, 0)
        self.assertEqual(len(opener.requests), 2)
        second_url, second_auth = opener.requests[1]
        self.assertTrue(second_url.startswith("https://api.github.com/"))
        self.assertEqual(second_auth, "Bearer tok-secret-123")  # 同主机保留

    def test_redirect_loop_capped_at_5(self):
        # 302 无限互跳：上限 5 跳后按最后 3xx 返回（非 2xx → ApiError → exit 2）。
        opener = _FakeRedirectOpener([
            (302, b"", "https://blob.invalid/hop1"),
            (302, b"", "https://blob.invalid/hop2"),
            (302, b"", "https://blob.invalid/hop3"),
            (302, b"", "https://blob.invalid/hop4"),
            (302, b"", "https://blob.invalid/hop5"),
            (302, b"", "https://blob.invalid/hop6")])
        world = _fake_world()
        fake_get, _, _ = world
        VRR = _load_module()
        with mock.patch.dict(os.environ, {"GITHUB_TOKEN": "tok-secret-123"}), \
             mock.patch.object(VRR, "_http_get", side_effect=fake_get), \
             mock.patch.object(VRR, "_OPENER", opener):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci",
                           "--repo", REPO_ID, "--profile", "linux-deep"])
        self.assertEqual(rc, 2)                       # 非重定向凭据问题的 401 同类
        self.assertEqual(len(opener.requests), 6)     # 首跳 + 5 跳跟随

    def test_302_cross_host_without_token_adds_no_header(self):
        # 无 GITHUB_TOKEN：全程无 Authorization，302 跟随正常取 zip。
        opener = _FakeRedirectOpener([
            (302, b"", "https://blob.core.windows.net/z"),
            (200, self.ZIP_BODY, None)])
        world = _fake_world()
        fake_get, _, _ = world
        VRR = _load_module()
        env = {k: v for k, v in os.environ.items() if k != "GITHUB_TOKEN"}
        with mock.patch.dict(os.environ, env, clear=True), \
             mock.patch.object(VRR, "_http_get", side_effect=fake_get), \
             mock.patch.object(VRR, "_OPENER", opener):
            rc = VRR.main(["--sha", SHA, "--workflows", "linux-ci",
                           "--repo", REPO_ID, "--profile", "linux-deep"])
        self.assertEqual(rc, 0)
        self.assertIsNone(opener.requests[0][1])
        self.assertIsNone(opener.requests[1][1])


# ---------------------------------------------------------------- 辅助 ----

if __name__ == "__main__":
    unittest.main()
