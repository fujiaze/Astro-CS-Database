#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/select_candidate.py —— V8-CI-008 Fatduck 候选选择器。

从 GitHub API（api.github.com，urllib，无第三方库）选择最新的 Fatduck 合格
候选：main 上最新 ``AstroCS Windows CI``（ci-windows.yml）success run，
且同一 head_sha 存在 success 的 Linux CI（ci-linux.yml）run，且该 windows
run 的 artifact 列表含 ``astrocs-windows-candidate-<head_sha>``。

触发场景（--event）：

- ``workflow_run``：由 Fatduck workflow 直传触发 run 的
  ``--head-branch/--conclusion/--run-id``；先做本地契约校验
  （head_branch==main、conclusion==success），再取 run 详情确认它是
  ``AstroCS Windows CI`` 的 run 并拿 head_sha，然后查 Linux 同 SHA 与 artifact。
- ``schedule``：无触发上下文，自行查询 ci-windows.yml 在 main 上最新的
  success run 作为候选。

Token：仅从环境 ``GITHUB_TOKEN``/``GH_TOKEN`` 读取，仅进 Authorization 头，
不打印、不落盘、不写日志。

退出码：
- 0 = 有合格候选；
- 2 = 环境/网络不可用（repository_context_missing / network_unavailable /
  api_error）；
- 3 = no-candidate（结构化 reason_code 区分：head_branch_not_main /
  trigger_run_not_success / trigger_run_not_windows_workflow /
  no_success_windows_run / linux_not_green / candidate_artifact_missing）。

输出：
- ``--json``：stdout 输出完整 JSON 报告（候选或 no-candidate 均结构化）；
- 非 --json：stdout 仅输出 GITHUB_OUTPUT 兼容行摘要；
- 无论是否 --json，若环境存在 ``GITHUB_OUTPUT``（CI 场景）则把
  has_candidate/source_sha/windows_run_id/artifact_name/artifact_digest/
  artifact_size/no_candidate_reason 追加写入，供下游 job 用 steps 输出。

纪律：每次 HTTP 带 timeout（默认 20s），网络失败重试 <=2 次（线性退避）；
查询 URL 与响应摘要逐条落 ``--log``（JSONL），不写响应正文全文。
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

API_BASE = "https://api.github.com"

WINDOWS_WORKFLOW = "ci-windows.yml"
LINUX_WORKFLOW = "ci-linux.yml"
CANDIDATE_ARTIFACT_PREFIX = "astrocs-windows-candidate-"
TARGET_BRANCH = "main"

EXIT_OK = 0                    # 有合格候选
EXIT_ENV_OR_NETWORK = 2        # 环境/网络/API 不可用
EXIT_NO_CANDIDATE = 3          # 结构化 no-candidate

MAX_RETRIES = 2
REQUEST_TIMEOUT_DEFAULT = 20.0
PER_PAGE = 20

# 结构化 reason_code（exit 3，区分各失败环节）
NO_CANDIDATE_REASONS = (
    "head_branch_not_main",
    "trigger_run_not_success",
    "trigger_run_not_windows_workflow",
    "no_success_windows_run",
    "linux_not_green",
    "candidate_artifact_missing",
)
# 结构化 reason_code（exit 2，环境/网络）
ENV_FAILURE_REASONS = (
    "repository_context_missing",
    "network_unavailable",
    "api_error",
)


def utc_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def read_token() -> str:
    """从环境读 token（GITHUB_TOKEN 优先，GH_TOKEN 兜底）；不落盘。"""
    return os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN") or ""


class NoCandidate(Exception):
    """查询成功但不满足候选契约（exit 3）。"""

    def __init__(self, reason: str, detail: str = ""):
        super().__init__(f"{reason}: {detail}" if detail else reason)
        self.reason = reason
        self.detail = detail


class EnvUnavailable(Exception):
    """环境/网络/API 不可用（exit 2）。"""

    def __init__(self, reason: str, detail: str = ""):
        super().__init__(f"{reason}: {detail}" if detail else reason)
        self.reason = reason
        self.detail = detail


# ------------------------------------------------------------------ HTTP ----

def gh_api(url: str, *, timeout: float, log_file=None) -> tuple[int, dict]:
    """GET 一个 GitHub API URL，返回 (status, parsed_json)。

    token 只从环境读取、只进请求头；网络异常重试 <=2 次。
    """
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "astrocs-ci-select-candidate",
    }
    token = read_token()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers, method="GET")
    last_err: Exception | None = None
    for attempt in range(1 + MAX_RETRIES):
        try:
            with urllib.request.urlopen(request, timeout=timeout) as response:
                body = response.read().decode("utf-8", errors="replace")
                _log_line(log_file, {
                    "utc": utc_iso(), "url": _redact(url),
                    "status": response.status,
                    "token_used": bool(token)})
                return response.status, json.loads(body)
        except urllib.error.HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")[:200]
            _log_line(log_file, {"utc": utc_iso(), "url": _redact(url),
                                 "status": exc.code,
                                 "token_used": bool(token),
                                 "error": "http_error"})
            if exc.code in (403, 429):
                last_err = EnvUnavailable("network_unavailable",
                                          f"API 限流/拒绝 status={exc.code}")
            else:
                last_err = EnvUnavailable("api_error",
                                          f"API status={exc.code} {detail}")
            # 4xx 明确响应不重试（限流除外已归网络）
        except (urllib.error.URLError, TimeoutError, OSError,
                json.JSONDecodeError) as exc:
            last_err = EnvUnavailable("network_unavailable", str(exc))
        if isinstance(last_err, EnvUnavailable) and \
                last_err.reason == "api_error":
            break  # 明确 API 拒绝（非限流）重试无意义
        if attempt < MAX_RETRIES:
            time.sleep(1.0 * (attempt + 1))
    assert last_err is not None
    raise last_err


def _redact(url: str) -> str:
    """日志用 URL 脱敏（token 只在头里，URL 本身无凭据；防御性裁剪）。"""
    parsed = urllib.parse.urlsplit(url)
    return urllib.parse.urlunsplit(
        (parsed.scheme, parsed.netloc, parsed.path, parsed.query, ""))


def _log_line(log_file, payload: dict) -> None:
    if log_file is None:
        return
    log_file.parent.mkdir(parents=True, exist_ok=True)
    with log_file.open("a", encoding="utf-8") as fh:
        fh.write(json.dumps(payload, ensure_ascii=False) + "\n")


# ---------------------------------------------------------------- 查询流程 ----

def _query_string(params: dict) -> str:
    return urllib.parse.urlencode(
        {k: v for k, v in params.items() if v not in (None, "")})


def latest_success_run(api: str, repo: str, workflow_file: str, *,
                       head_sha: str = "", timeout: float,
                       log_file=None) -> dict | None:
    """main 上该 workflow 最新的 success run（schedule 场景入口）。"""
    params = _query_string({"branch": TARGET_BRANCH, "status": "success",
                            "head_sha": head_sha, "per_page": PER_PAGE})
    url = f"{api}/repos/{repo}/actions/workflows/{workflow_file}/runs?{params}"
    status, payload = gh_api(url, timeout=timeout, log_file=log_file)
    if status != 200:
        raise EnvUnavailable("api_error",
                             f"workflow runs status={status} ({workflow_file})")
    runs = payload.get("workflow_runs") or []
    return runs[0] if runs else None


def _workflow_run_detail(api: str, repo: str, run_id: str, *,
                         timeout: float, log_file=None) -> dict:
    url = f"{api}/repos/{repo}/actions/runs/{urllib.parse.quote(str(run_id))}"
    status, payload = gh_api(url, timeout=timeout, log_file=log_file)
    if status == 404:
        raise NoCandidate("trigger_run_not_success",
                          f"run {run_id} 不存在（404）")
    if status != 200:
        raise EnvUnavailable("api_error", f"run detail status={status}")
    return payload


def _same_sha_linux_success(api: str, repo: str, head_sha: str, *,
                            timeout: float, log_file=None) -> dict | None:
    return latest_success_run(api, repo, LINUX_WORKFLOW, head_sha=head_sha,
                              timeout=timeout, log_file=log_file)


def _candidate_artifact(api: str, repo: str, run_id: str, head_sha: str, *,
                        timeout: float, log_file=None) -> dict:
    expected = CANDIDATE_ARTIFACT_PREFIX + head_sha
    url = (f"{api}/repos/{repo}/actions/runs/"
           f"{urllib.parse.quote(str(run_id))}/artifacts"
           f"?{_query_string({'per_page': 100})}")
    status, payload = gh_api(url, timeout=timeout, log_file=log_file)
    if status != 200:
        raise EnvUnavailable("api_error", f"artifacts status={status}")
    for artifact in payload.get("artifacts") or []:
        if artifact.get("name") == expected:
            return artifact
    raise NoCandidate("candidate_artifact_missing",
                      f"run {run_id} 无 artifact {expected}")


def select_candidate(*, event: str, head_branch: str, conclusion: str,
                     run_id: str, api: str, repository: str,
                     timeout: float, log_file=None) -> dict:
    """主流程：返回候选报告 dict；不满足契约 raise NoCandidate。"""
    if event == "workflow_run":
        if head_branch != TARGET_BRANCH:
            raise NoCandidate("head_branch_not_main",
                              f"head_branch={head_branch!r}")
        if conclusion != "success":
            raise NoCandidate("trigger_run_not_success",
                              f"conclusion={conclusion!r}")
        if not run_id:
            raise NoCandidate("trigger_run_not_success", "run_id 缺失")
        run = _workflow_run_detail(api, repository, run_id, timeout=timeout,
                                   log_file=log_file)
        # workflow 身份绑定文件路径（path 是 GitHub Actions 的真实身份键）；
        # display name 可被同名外部 workflow 伪造（V8-CIQA-001 P1-GAP-2）。
        expected_path = f".github/workflows/{WINDOWS_WORKFLOW}"
        run_path = run.get("path") or ""
        if run_path != expected_path:
            raise NoCandidate("trigger_run_not_windows_workflow",
                              f"run path={run_path!r}（预期 {expected_path}）")
        head_sha = run.get("head_sha") or ""
        windows_run = run
    elif event == "schedule":
        windows_run = latest_success_run(api, repository, WINDOWS_WORKFLOW,
                                         timeout=timeout, log_file=log_file)
        if windows_run is None:
            raise NoCandidate("no_success_windows_run",
                              f"{TARGET_BRANCH} 上无 success 的 {WINDOWS_WORKFLOW} run")
        head_sha = windows_run.get("head_sha") or ""
    else:
        raise EnvUnavailable("api_error", f"未知 event：{event!r}")

    if not head_sha:
        raise NoCandidate("no_success_windows_run", "windows run 无 head_sha")

    linux_run = _same_sha_linux_success(api, repository, head_sha,
                                        timeout=timeout, log_file=log_file)
    if linux_run is None:
        raise NoCandidate("linux_not_green",
                          f"head_sha={head_sha} 无同 SHA success Linux run")

    artifact = _candidate_artifact(api, repository,
                                   windows_run.get("id"), head_sha,
                                   timeout=timeout, log_file=log_file)
    return {
        "verdict": "candidate",
        "event": event,
        "repository": repository,
        "verified_utc": utc_iso(),
        "source_sha": head_sha,
        "windows_run_id": str(windows_run.get("id", "")),
        "windows_run_url": windows_run.get("html_url", ""),
        "linux_run_id": str(linux_run.get("id", "")),
        "artifact_name": artifact.get("name", ""),
        "artifact_id": str(artifact.get("id", "")),
        "artifact_digest": artifact.get("digest"),
        "artifact_size": artifact.get("size_in_bytes"),
        "artifact_expired": artifact.get("expired"),
    }


# ----------------------------------------------------------------- 输出 ----

def github_output_lines(report: dict) -> str:
    """GITHUB_OUTPUT 兼容单值行（下游 steps 输出）。"""
    no_reason = (report.get("no_candidate_reason")
                 or report.get("reason_code") or "")
    lines = [
        f"has_candidate={'true' if report.get('verdict') == 'candidate' else 'false'}",
        f"source_sha={report.get('source_sha', '')}",
        f"windows_run_id={report.get('windows_run_id', '')}",
        f"artifact_name={report.get('artifact_name', '')}",
        f"artifact_digest={report.get('artifact_digest') or ''}",
        f"artifact_size={report.get('artifact_size') if report.get('artifact_size') is not None else ''}",
        f"no_candidate_reason={no_reason}",
    ]
    return "\n".join(lines) + "\n"


def append_github_output(report: dict) -> None:
    """CI 环境（GITHUB_OUTPUT 指向文件）时追加输出；本地无此 env 则跳过。"""
    path = os.environ.get("GITHUB_OUTPUT")
    if not path:
        return
    out = Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("a", encoding="utf-8") as fh:
        fh.write(github_output_lines(report))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ci/select_candidate.py",
        description="V8-CI-008：选择最新 Fatduck 合格候选（GitHub API，无第三方库）。")
    parser.add_argument("--event", required=True,
                        choices=["workflow_run", "schedule"],
                        help="触发场景")
    parser.add_argument("--head-branch", default="",
                        help="workflow_run 场景：触发 run 的 head_branch")
    parser.add_argument("--conclusion", default="",
                        help="workflow_run 场景：触发 run 的 conclusion")
    parser.add_argument("--run-id", default="",
                        help="workflow_run 场景：触发 run 的数据库 id")
    parser.add_argument("--repository", default=os.environ.get("GITHUB_REPOSITORY", ""),
                        help="owner/repo（默认 GITHUB_REPOSITORY env）")
    parser.add_argument("--api", default=API_BASE,
                        help=f"API base（默认 {API_BASE}，测试可覆盖）")
    parser.add_argument("--timeout", type=float, default=REQUEST_TIMEOUT_DEFAULT,
                        help=f"单次 HTTP 超时秒数（默认 {REQUEST_TIMEOUT_DEFAULT:g}）")
    parser.add_argument("--json", action="store_true",
                        help="stdout 输出完整 JSON 报告")
    parser.add_argument("--log", default=None,
                        help="查询日志 JSONL 路径（默认不落盘）")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    log_file = Path(args.log) if args.log else None
    if not args.repository or "/" not in args.repository:
        report = {"verdict": "env_unavailable",
                  "reason_code": "repository_context_missing",
                  "detail": "无 --repository 且无 GITHUB_REPOSITORY（hosted 环境才注入）",
                  "event": args.event, "utc": utc_iso()}
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            print(f"select_candidate: ENV_UNAVAILABLE: "
                  f"{report['reason_code']}: {report['detail']}", file=sys.stderr)
        append_github_output(report)
        return EXIT_ENV_OR_NETWORK

    try:
        report = select_candidate(
            event=args.event, head_branch=args.head_branch,
            conclusion=args.conclusion, run_id=args.run_id,
            api=args.api, repository=args.repository,
            timeout=args.timeout, log_file=log_file)
    except NoCandidate as exc:
        report = {"verdict": "no_candidate", "reason_code": exc.reason,
                  "detail": exc.detail, "event": args.event,
                  "repository": args.repository, "utc": utc_iso()}
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            print(f"select_candidate: NO_CANDIDATE: {exc.reason}: {exc.detail}",
                  file=sys.stderr)
        append_github_output(report)
        return EXIT_NO_CANDIDATE
    except EnvUnavailable as exc:
        report = {"verdict": "env_unavailable", "reason_code": exc.reason,
                  "detail": exc.detail, "event": args.event,
                  "repository": args.repository, "utc": utc_iso()}
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            print(f"select_candidate: ENV_UNAVAILABLE: {exc.reason}: {exc.detail}",
                  file=sys.stderr)
        append_github_output(report)
        return EXIT_ENV_OR_NETWORK

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(f"select_candidate: CANDIDATE source_sha={report['source_sha']} "
              f"windows_run_id={report['windows_run_id']} "
              f"artifact={report['artifact_name']}", file=sys.stderr)
    append_github_output(report)
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
