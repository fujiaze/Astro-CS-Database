# -*- coding: utf-8 -*-
"""ci/tests/test_ci_repair_round.py — CI 修复常驻线轮报器回归（CI-REPAIR-001）。

测试设计（冻结宪章 §17 + 控制包 06 §3）：
  * 先红后绿：本文件先于 ci/ci_repair_round.py 落地，模块缺失时全部 ERROR。
  * 负向注入必败（不得静默丢弃/不得假绿）：
      N1 未登记归因域的红灯         -> domain=UNKNOWN + needs_mapping=True，strict 下 exit 3；
      N2 失败 job 日志端点 5xx      -> log_status=unavailable（取证缺口显式登记），strict 下 exit 3；
      N3 run 仍 in_progress         -> exit 4，绝不判定 GREEN；
      N4 401/404（仓库/SHA 不可读） -> exit 2，不产出 GREEN 报告。
  * 确定性 bitwise：固定 --now 连续两次运行，FINDINGS.json 逐字节一致。
  * 纯本地 fixture HTTP 服务器（127.0.0.1）驱动真实代码路径，禁真实网络。
"""
from __future__ import annotations

import hashlib
import http.server
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import unittest
import urllib.parse
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOL = REPO / "ci" / "ci_repair_round.py"
SHA = "e6254d4da99770f05718cc95c868ff1c52339370"
REPO_SLUG = "owner/name"
NOW = "2026-09-12T00:00:00Z"


def _check_run(cid, name, conclusion, status="completed"):
    return {
        "id": cid, "name": name, "status": status, "conclusion": conclusion,
        "html_url": "https://example.invalid/check/%d" % cid,
        "details_url": "https://example.invalid/run/%d" % cid,
        "started_at": "2026-09-11T16:12:22Z", "completed_at": "2026-09-11T16:37:29Z",
        "app": {"name": "GitHub Actions"},
    }


def _run(rid, name, conclusion, status="completed"):
    return {
        "id": rid, "name": name, "event": "push", "status": status,
        "conclusion": conclusion, "run_attempt": 1, "head_sha": SHA,
        "created_at": "2026-09-11T16:12:22Z",
        "html_url": "https://example.invalid/actions/runs/%d" % rid,
    }


def _job(jid, rid, name, conclusion, status="completed"):
    return {
        "id": jid, "run_id": rid, "name": name, "status": status,
        "conclusion": conclusion,
        "started_at": "2026-09-11T16:12:26Z", "completed_at": "2026-09-11T16:37:29Z",
        "html_url": "https://example.invalid/job/%d" % jid,
        "steps": [
            {"name": "Checkout", "number": 1, "status": "completed", "conclusion": "success"},
            {"name": "Run CI", "number": 2, "status": "completed", "conclusion": conclusion},
        ],
    }


class _Handler(http.server.BaseHTTPRequestHandler):
    routes = {}          # (kind, key) -> (status, body_bytes, content_type)
    requests = []        # 请求记录（供断言：token 头不透出到产物外的纪律）

    def log_message(self, *a):  # 静默
        pass

    def do_GET(self):  # noqa: N802
        parsed = urllib.parse.urlparse(self.path)
        parts = [p for p in parsed.path.split("/") if p]
        query = urllib.parse.parse_qs(parsed.query)
        page = int(query.get("page", ["1"])[0])
        kind, key = None, None
        if parts[-1:] == ["check-runs"] and parts[0] == "repos":
            kind, key = "check-runs", page
        elif parts[-1:] == ["runs"] and parts[0] == "repos":
            kind, key = "runs", page
        elif parts[-1:] == ["jobs"] and parts[0] == "repos":
            kind, key = "jobs", parts[5]
        elif parts[-1] == "logs" and "jobs" in parts:
            kind, key = "log", parts[parts.index("jobs") + 1]
        else:
            kind, key = "unknown", self.path
        _Handler.requests.append((kind, key))
        entry = _Handler.routes.get((kind, str(key)))
        if entry is None and kind == "jobs":
            entry = _Handler.routes.get(("jobs", "*"))
        if entry is None:
            body = json.dumps({"message": "Not Found"}).encode()
            self.send_response(404)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        status, body, ctype = entry
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class _RoundCase(unittest.TestCase):
    """公共基类：起本地 fixture API 服务器 + 建轮目录。"""

    fixture = None   # 子类覆盖：dict[(kind,key)] = (status, obj, ctype)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ci_repair_round_")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.monitor = self.root / "run" / "ci" / "monitor"
        self.monitor.mkdir(parents=True)
        (self.monitor / "BUILD-GCC-RELEASE.json").write_text(
            json.dumps({"cpu_samples": [1, 2, 3], "frozen_gate": {"verdict": "pass"}}),
            encoding="utf-8")
        _Handler.routes = self._materialise(self.fixture or {})
        _Handler.requests = []
        self.httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
        self.addCleanup(self.httpd.server_close)
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()
        self.addCleanup(self.httpd.shutdown)
        self.api = "http://127.0.0.1:%d" % self.httpd.server_address[1]
        self.out = self.root / "round"

    @staticmethod
    def _materialise(fixture):
        out = {}
        for (kind, key), (status, obj, ctype) in fixture.items():
            if isinstance(obj, (dict, list)):
                body = json.dumps(obj).encode()
            elif isinstance(obj, str):
                body = obj.encode()
            else:
                body = obj
            out[(kind, str(key))] = (status, body, ctype)
        return out

    def run_tool(self, *extra, expect_rc=None, env=None):
        argv = [sys.executable, str(TOOL), "--repo", REPO_SLUG, "--sha", SHA,
                "--round-dir", str(self.out), "--monitor-dir", str(self.monitor),
                "--api-base", self.api, "--now", NOW, "--timeout", "5"]
        argv.extend(extra)
        e = dict(os.environ)
        e.pop("GITHUB_TOKEN", None)
        e.pop("GH_TOKEN", None)
        e["GITHUB_TOKEN"] = "ghp_FAKE_TOKEN_FOR_TESTS"
        if env:
            e.update(env)
        proc = subprocess.run(argv, cwd=str(REPO), capture_output=True, text=True,
                              timeout=180, env=e)
        if expect_rc is not None:
            self.assertEqual(proc.returncode, expect_rc,
                             "rc=%d stdout=%s stderr=%s" % (proc.returncode, proc.stdout, proc.stderr))
        return proc

    def findings(self):
        return json.loads((self.out / "FINDINGS.json").read_text(encoding="utf-8"))


def _red_sha_fixture(extra_checks=(), log_status=200, log_body="boom\nAssertionError: nope\n"):
    """上轮 e6254d4d 形状的最小 fixture：2 个 run，1 红 job + 1 红 check。"""
    checks = [_check_run(1, "VERSION-CONSISTENCY", "success"),
              _check_run(2, "UT-BACKEND", "failure"),
              _check_run(3, "AGENTS-GOV", "failure")]
    checks.extend(extra_checks)
    fx = {
        ("check-runs", "1"): (200, {"total_count": len(checks), "check_runs": checks}, "application/json"),
        ("runs", "1"): (200, {"total_count": 1, "workflow_runs": [_run(34620635766, "AstroCS Linux CI", "failure")]},
                        "application/json"),
        ("jobs", "34620635766"): (200, {"total_count": 1,
                                        "jobs": [_job(103333440630, 34620635766, "linux", "failure")]},
                                  "application/json"),
        ("log", "103333440630"): (log_status, log_body, "text/plain"),
    }
    return fx


class TestRedInventory(_RoundCase):
    fixture = _red_sha_fixture()

    def test_01_red_lights_all_captured_with_domain_and_log(self):
        proc = self.run_tool(expect_rc=1)
        f = self.findings()
        self.assertEqual(f["verdict"], "RED")
        names = sorted(r["name"] for r in f["reds"])
        self.assertEqual(names, ["AGENTS-GOV", "UT-BACKEND", "linux"])
        self.assertEqual(f["counters"]["reds"], 3)
        by = {r["name"]: r for r in f["reds"]}
        self.assertEqual(by["UT-BACKEND"]["domain"], "SCI")
        self.assertEqual(by["AGENTS-GOV"]["domain"], "GOV")
        self.assertEqual(by["linux"]["kind"], "job")
        # check-run 无独立日志端点（GitHub 语义：日志挂在 job 上）；
        # job 级红灯必须带日志摘录，两者不得混淆。
        self.assertEqual(by["UT-BACKEND"]["log_status"], "not_available_for_check_run")
        self.assertEqual(by["linux"]["log_status"], "excerpt")
        self.assertIn("AssertionError", "\n".join(by["linux"]["log_excerpt"]))
        self.assertTrue((self.out / "PULL.json").is_file())
        self.assertTrue((self.out / "raw" / "check-runs.json").is_file())
        self.assertTrue((self.out / "FINDINGS.json").is_file())
        self.assertNotIn("ghp_FAKE_TOKEN_FOR_TESTS", proc.stdout + proc.stderr)

    def test_02_monitor_evidence_copied_and_indexed(self):
        self.run_tool(expect_rc=1)
        self.assertTrue((self.out / "monitor" / "BUILD-GCC-RELEASE.json").is_file())
        f = self.findings()
        self.assertEqual([m["name"] for m in f["monitor"]], ["BUILD-GCC-RELEASE.json"])
        self.assertRegex(f["monitor"][0]["sha256"], r"^[0-9a-f]{64}$")


class TestNegativeInjection(_RoundCase):
    """负向注入必须失败：伪造/缺失证据不得产出 GREEN，也不得静默丢弃。"""

    def test_03_unknown_check_fails_closed(self):
        self.fixture = _red_sha_fixture(extra_checks=[_check_run(9, "BRAND-NEW-GATE", "failure")])
        _RoundCase.setUp(self)
        self.run_tool("--strict", expect_rc=3)
        f = self.findings()
        unknown = [r for r in f["reds"] if r["name"] == "BRAND-NEW-GATE"]
        self.assertEqual(len(unknown), 1, "未登记红灯被静默丢弃")
        self.assertEqual(unknown[0]["domain"], "UNKNOWN")
        self.assertTrue(unknown[0]["needs_mapping"])
        self.assertGreaterEqual(f["counters"]["gaps"], 1)

    def test_04_log_endpoint_5xx_is_recorded_not_dropped(self):
        self.fixture = _red_sha_fixture(log_status=503)
        _RoundCase.setUp(self)
        self.run_tool("--strict", expect_rc=3)
        f = self.findings()
        job = [r for r in f["reds"] if r["kind"] == "job"][0]
        self.assertEqual(job["log_status"], "unavailable")
        self.assertIsNone(job["log_excerpt"])
        self.assertEqual(job["conclusion"], "failure")
        self.assertGreaterEqual(f["counters"]["gaps"], 1)

    def test_05_incomplete_run_never_green(self):
        self.fixture = {
            ("check-runs", "1"): (200, {"total_count": 1, "check_runs": [_check_run(1, "VERSION-CONSISTENCY", "success")]},
                                  "application/json"),
            ("runs", "1"): (200, {"total_count": 1, "workflow_runs": [_run(7, "AstroCS Linux CI", None, status="in_progress")]},
                            "application/json"),
            ("jobs", "7"): (200, {"total_count": 0, "jobs": []}, "application/json"),
        }
        _RoundCase.setUp(self)
        self.run_tool(expect_rc=4)
        f = self.findings()
        self.assertEqual(f["verdict"], "INCOMPLETE")
        self.assertEqual(f["counters"]["incomplete_runs"], 1)
        self.assertNotEqual(f["verdict"], "GREEN")

    def test_06_unreadable_repo_or_sha_exits_2(self):
        self.fixture = {("check-runs", "1"): (404, {"message": "Not Found"}, "application/json")}
        _RoundCase.setUp(self)
        proc = self.run_tool(expect_rc=2)
        self.assertFalse((self.out / "FINDINGS.json").exists(),
                         "取证失败时不得落任何 FINDINGS（防伪造 PASS 面）")
        self.assertIn("404", proc.stderr + proc.stdout)


class TestDeterminism(_RoundCase):
    fixture = _red_sha_fixture()

    def test_07_bitwise_identical_findings(self):
        self.run_tool(expect_rc=1)
        first = (self.out / "FINDINGS.json").read_bytes()
        self.run_tool(expect_rc=1)
        second = (self.out / "FINDINGS.json").read_bytes()
        self.assertEqual(hashlib.sha256(first).hexdigest(), hashlib.sha256(second).hexdigest())

    def test_08_green_sha_is_green(self):
        self.fixture = {
            ("check-runs", "1"): (200, {"total_count": 1, "check_runs": [_check_run(1, "UT-API", "success")]},
                                  "application/json"),
            ("runs", "1"): (200, {"total_count": 1, "workflow_runs": [_run(9, "AstroCS Linux CI", "success")]},
                            "application/json"),
            ("jobs", "9"): (200, {"total_count": 1, "jobs": [_job(90, 9, "linux", "success")]}, "application/json"),
        }
        _RoundCase.setUp(self)
        self.run_tool(expect_rc=0)
        f = self.findings()
        self.assertEqual(f["verdict"], "GREEN")
        self.assertEqual(f["counters"]["reds"], 0)


class TestAttributionTable(_RoundCase):
    fixture = _red_sha_fixture()

    def test_09_every_attribution_entry_is_complete(self):
        sys.path.insert(0, str(REPO / "ci"))
        try:
            import ci_repair_round as M
        finally:
            sys.path.pop(0)
        self.assertTrue(M.ATTRIBUTION, "归因表为空")
        for key, entry in M.ATTRIBUTION.items():
            for field in ("domain", "owner_node", "root_cause", "in_whitelist"):
                self.assertIn(field, entry, "%s 缺字段 %s" % (key, field))
            self.assertIn(entry["in_whitelist"], (True, False))
        # 白名单三域之外的 in_whitelist=True 一律拒绝（写域零越界的机器化防线）
        for key, entry in M.ATTRIBUTION.items():
            if entry["in_whitelist"]:
                rc = entry.get("root_cause_path", "")
                self.assertTrue(rc.startswith(("ci/", "tools/quality/", "run/ci_repair/")),
                                "%s 声明域内但根因路径 %r 不在白名单" % (key, rc))


if __name__ == "__main__":
    unittest.main(verbosity=2)


class TestCiResultIngestion(_RoundCase):
    """profile 内检查项红灯（CI_RESULT.json）必须并入 findings，且防跨轮污染。"""

    fixture = _red_sha_fixture()

    def _write_ci_result(self, source_sha=SHA):
        d = self.root / "ci_result"
        (d / "logs").mkdir(parents=True, exist_ok=True)
        (d / "logs" / "AGENTS-GOV.log").write_text(
            "# check_id: AGENTS-GOV\nGOV_CHECK_FAIL missing=['main-only']\n", encoding="utf-8")
        (d / "CI_RESULT.json").write_text(json.dumps({
            "schema_version": 1, "source_sha": source_sha, "profile": "linux-main",
            "run_id": "20260911T161708Z-1325fb91", "verdict": "FAIL",
            "checks": [
                {"id": "UT-API", "verdict": "PASS", "exit_code": 0},
                {"id": "AGENTS-GOV", "verdict": "FAIL", "exit_code": 1, "reason": "命令非零退出：1"},
                {"id": "UT-CLI", "verdict": "FAIL(dirty)", "exit_code": 1,
                 "dirty_violations": ["alloc_report.json", "alloc_samples.csv"]},
            ],
        }), encoding="utf-8")
        return d / "CI_RESULT.json"

    def test_10_profile_checks_merged_with_log_and_aggregation(self):
        spec = self._write_ci_result()
        self.run_tool("--ci-result", str(spec), expect_rc=1)
        f = self.findings()
        internal = [r for r in f["reds"] if r["kind"] == "check"]
        self.assertEqual(sorted(r["name"] for r in internal), ["AGENTS-GOV", "UT-CLI"])
        by = {r["name"]: r for r in internal}
        self.assertEqual(by["AGENTS-GOV"]["domain"], "GOV")
        self.assertEqual(by["UT-CLI"]["domain"], "CLI")
        self.assertEqual(by["UT-CLI"]["dirty_violations"], ["alloc_report.json", "alloc_samples.csv"])
        self.assertTrue(by["AGENTS-GOV"]["in_whitelist"] is False)
        self.assertEqual(by["AGENTS-GOV"]["log_status"], "excerpt")
        self.assertIn("GOV_CHECK_FAIL", "\n".join(by["AGENTS-GOV"]["log_excerpt"]))
        self.assertEqual(f["counters"]["red_checks_internal"], 2)
        self.assertEqual(f["counters"]["ci_results_ingested"], 1)
        job = [r for r in f["reds"] if r["kind"] == "job"][0]
        self.assertEqual(job["aggregates"], ["AGENTS-GOV", "UT-CLI"])

    def test_11_sha_mismatch_rejected_fail_closed(self):
        spec = self._write_ci_result(source_sha="0" * 40)
        proc = self.run_tool("--ci-result", str(spec), expect_rc=2)
        self.assertFalse((self.out / "FINDINGS.json").exists(),
                         "跨轮证据（source_sha 不符）必须整体拒绝，不得产出报告")
        self.assertIn("source_sha", proc.stderr)



class TestWaivableSkip(_RoundCase):
    """宿主能力 SKIP（SKIPPED(waivable)/exit 77）不是红灯，但必须显式计数。"""

    fixture = _red_sha_fixture()

    def test_12_waivable_skip_not_red_but_counted(self):
        d = self.root / "ci_result"
        d.mkdir(parents=True, exist_ok=True)
        (d / "CI_RESULT.json").write_text(json.dumps({
            "source_sha": SHA, "profile": "linux-main", "verdict": "FAIL",
            "checks": [
                {"id": "UT-CPU-AVX512", "verdict": "SKIPPED(waivable)", "exit_code": 77,
                 "reason": "宿主能力 gate"},
                {"id": "THREAD-BUDGET", "verdict": "FAIL", "exit_code": 1},
            ],
        }), encoding="utf-8")
        self.run_tool("--ci-result", str(d / "CI_RESULT.json"), expect_rc=1)
        f = self.findings()
        internal = [r for r in f["reds"] if r["kind"] == "check"]
        self.assertEqual([r["name"] for r in internal], ["THREAD-BUDGET"],
                         "宿主能力 SKIP 被误判为红灯")
        self.assertEqual(f["counters"]["skipped_waivable_internal"], 1)
        meta = f["ci_results"][0]
        self.assertEqual([s["id"] for s in meta["skipped"]], ["UT-CPU-AVX512"])
        self.assertEqual(meta["skipped"][0]["exit_code"], 77)

