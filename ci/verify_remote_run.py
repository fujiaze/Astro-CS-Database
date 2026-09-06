#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/verify_remote_run.py — V8-CI-012 F-D4：hosted run 机器复验（stdlib-only）。

复验派发单 ledger_command 语义：
  python3 ci/verify_remote_run.py --sha <sha> --workflows <name>... \
      [--profile <id>] [--timeout 15]

对每个 --workflows 键做唯一指认（候选 = 数字 id / path / 文件名 / 文件名主干 /
name 全名；全部未中时退回词集包含唯一匹配——台账写法 "linux-ci" 即以词集
{linux, ci} 唯一指认 ci-linux.yml，其 name 为 "AstroCS Linux CI"）：
  1. GET /repos/{owner}/{repo}/actions/workflows            （解析 workflow）；
  2. GET .../workflows/{id}/runs?head_sha=<sha>             （该 SHA 的 run；
     event 优先级 workflow_dispatch > push，同 event 取 API 序最前 = 最新）；
  3. 核验 run 存在、head_sha 匹配、status=completed，
     GET .../runs/{id}/jobs 逐 job conclusion == success；
  4. 给了 --profile：GET .../runs/{id}/artifacts 取名称含该 SHA 的 artifact，
     内存解 zip 读 CI_RESULT.json，核验 profile 字段与 --profile 一致
     （summary 仅透传报告；deep run 数值判定属检查域，本脚本不判阈值）。

仓库标识解析顺序：--repo > GITHUB_REPOSITORY > 本地 git remote origin
（只读 git 查询；无任何 git 写操作）。

Exit code（与 ci/verify_actions_lock.py 同风格）：
  0 = PASS（全部 workflow 核验通过）
  1 = 有失败 verdict（run 缺失/未完成、head_sha 不符、job 失败、
      profile 证据不一致/不可判）
  2 = API/环境问题（网络不可达、API 非 2xx、仓库标识不可解析、artifact 下载失败）
  3 = 用法错误（参数缺失/形态非法、--workflows 无法唯一解析）

--offline：结构模式，仅校验参数并输出报告契约，不做任何网络请求（单测用）。

凭据纪律：token 仅经环境变量 GITHUB_TOKEN 内联进 Authorization 头；
零落盘（本脚本无任何文件写入）；token 不进 stdout/stderr。
仅 stdlib；每请求带 --timeout（默认 15s，AGENTS 纪律）；中文报错。
"""
from __future__ import annotations

import argparse
import io
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.request
import zipfile
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
API_BASE = "https://api.github.com"

EXIT_OK = 0             # 全部 workflow 核验通过
EXIT_VERDICT_FAIL = 1   # 有失败 verdict（run/job/profile 证据级）
EXIT_ENV = 2            # API/环境问题（网络、非 2xx、仓库不可解析）
EXIT_USAGE = 3          # 用法错误（参数形态、workflow 键无法唯一解析）

_SHA_RE = re.compile(r"^[0-9a-f]{40}$")
_REPO_RE = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
_TIMEOUT_DEFAULT = 15.0


class ApiError(Exception):
    """API/环境问题（exit 2）。"""


class UsageError(Exception):
    """用法错误（exit 3）。"""


def utc_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# ------------------------------------------------------------------ 传输层 ----
# 可注入接缝：单测 monkeypatch _http_get / _http_get_bytes（fixture 假 API），
# 不做真网络。

def _build_headers() -> dict:
    headers = {
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "astrocs-ci-verify-remote-run",
    }
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = "Bearer " + token
    return headers


def _http_get(url: str, timeout: float) -> tuple[int, bytes]:
    """GET 一个 URL，返回 (status, body bytes)；网络异常归一为 ApiError。"""
    request = urllib.request.Request(url, headers=_build_headers(), method="GET")
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read()
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        raise ApiError(f"网络不可达：{exc}") from exc


def _http_get_bytes(url: str, timeout: float) -> tuple[int, bytes]:
    """artifact zip 等二进制下载（与 _http_get 同形，便于独立注入）。"""
    return _http_get(url, timeout)


def _get_json(url: str, timeout: float) -> tuple[int, dict]:
    status, body = _http_get(url, timeout)
    try:
        return status, json.loads(body.decode("utf-8", "replace"))
    except json.JSONDecodeError as exc:
        raise ApiError(f"API 响应非合法 JSON（status={status}）：{url}") from exc


# ------------------------------------------------------- 仓库/workflow 解析 ----

def resolve_repo(explicit: str | None) -> str:
    """--repo > GITHUB_REPOSITORY > git remote origin（只读）。"""
    if explicit:
        if not _REPO_RE.match(explicit):
            raise UsageError(f"--repo 形态非法（应为 owner/repo）：{explicit!r}")
        return explicit
    env = os.environ.get("GITHUB_REPOSITORY", "")
    if _REPO_RE.match(env or ""):
        return env
    try:
        proc = subprocess.run(["git", "remote", "get-url", "origin"],
                              cwd=str(REPO_ROOT), capture_output=True,
                              text=True, timeout=10)
        url = proc.stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        url = ""
    match = re.search(r"github\.com[:/](.+?/.+?)(?:\.git)?/?$", url)
    if proc.returncode == 0 and match and _REPO_RE.match(match.group(1)):
        return match.group(1)
    raise ApiError("仓库标识不可解析（无 --repo、无 GITHUB_REPOSITORY、"
                   "git remote origin 无 github.com 地址）")


def _tokens(text: str) -> frozenset:
    return frozenset(t for t in re.split(r"[^0-9a-z]+", text.lower()) if t)


def resolve_workflow(workflows: list[dict], key: str) -> dict:
    """按 精确指认 → 词集包含唯一匹配 解析 --workflows 键；失败 raise UsageError。"""
    for wf in workflows:
        path = str(wf.get("path") or "")
        name = str(wf.get("name") or "")
        candidates = {
            str(wf.get("id") or ""), path,
            Path(path).name, Path(path).stem, name.lower(),
        }
        if key in candidates or key in (Path(path).name + ".yml",
                                        Path(path).name + ".yaml"):
            return wf
    key_tokens = _tokens(key)
    if key_tokens:
        fuzzy = [
            wf for wf in workflows
            if key_tokens <= (_tokens(str(wf.get("name") or ""))
                              | _tokens(str(wf.get("path") or "")))
        ]
        if len(fuzzy) == 1:
            return fuzzy[0]
        if len(fuzzy) > 1:
            raise UsageError(
                f"--workflows {key!r} 词集匹配到多个 workflow："
                + ", ".join(str(w.get("path")) for w in fuzzy))
    raise UsageError(f"--workflows {key!r} 无法唯一解析到任何 workflow")


# ------------------------------------------------------------------ 核验域 ----

_EVENT_PRIORITY = ("workflow_dispatch", "push")


def _pick_run(runs: list[dict], sha: str) -> dict | None:
    """event 优先级 workflow_dispatch > push，同 event 取最前（API 序 = 最新）。"""
    for event in _EVENT_PRIORITY:
        for run in runs:
            if run.get("event") == event:
                return run
    return None


def _verify_profile_evidence(repo: str, run_id: int, sha: str,
                             profile: str, timeout: float) -> dict:
    """artifact CI_RESULT.json 的 profile 证据一致性（最小实现）。"""
    status, arts = _get_json(
        f"{API_BASE}/repos/{repo}/actions/runs/{run_id}"
        f"/artifacts?per_page=100", timeout)
    if status != 200:
        raise ApiError(f"artifacts API 非 2xx（status={status}）")
    cands = [a for a in (arts.get("artifacts") or [])
             if sha in str(a.get("name") or "") and not a.get("expired")]
    if not cands:
        return {"ok": False, "artifact": None, "observed_profile": None,
                "summary": None,
                "reason": f"无名称含 {sha} 的 evidence artifact"}
    artifact = cands[0]
    z_status, z_body = _http_get_bytes(
        f"{API_BASE}/repos/{repo}/actions/artifacts/{artifact['id']}/zip",
        timeout)
    if z_status != 200:
        raise ApiError(f"artifact 下载非 2xx（status={z_status}，"
                       f"artifact={artifact.get('name')}；匿名下载受限时请经 "
                       "GITHUB_TOKEN 提供凭据）")
    try:
        with zipfile.ZipFile(io.BytesIO(z_body)) as zf:
            members = [n for n in zf.namelist()
                       if n == "CI_RESULT.json" or n.endswith("/CI_RESULT.json")]
            if not members:
                return {"ok": False, "artifact": artifact.get("name"),
                        "observed_profile": None, "summary": None,
                        "reason": "artifact zip 内无 CI_RESULT.json"}
            payload = json.loads(zf.read(members[0]).decode("utf-8", "replace"))
    except (zipfile.BadZipFile, json.JSONDecodeError) as exc:
        return {"ok": False, "artifact": artifact.get("name"),
                "observed_profile": None, "summary": None,
                "reason": f"CI_RESULT.json 解析失败：{exc}"}
    observed = payload.get("profile")
    ok = observed == profile
    return {"ok": ok, "artifact": artifact.get("name"),
            "observed_profile": observed,
            "summary": payload.get("summary"),
            "reason": None if ok else
            f"CI_RESULT.profile={observed!r} 与 --profile {profile!r} 不一致"}


def verify_workflow(repo: str, key: str, wf: dict, sha: str,
                    profile: str | None, timeout: float) -> dict:
    """单个 workflow 的核验；返回报告条目（ok/reasons/run/jobs/profile_evidence）。"""
    wf_id = wf.get("id")
    reasons: list[str] = []
    status, runs_payload = _get_json(
        f"{API_BASE}/repos/{repo}/actions/workflows/{wf_id}"
        f"/runs?head_sha={sha}&per_page=100", timeout)
    if status != 200:
        raise ApiError(f"workflow runs API 非 2xx（status={status}，key={key}）")
    runs = runs_payload.get("workflow_runs") or []
    entry: dict = {"key": key,
                   "workflow": {"id": wf_id, "name": wf.get("name"),
                                "path": wf.get("path")},
                   "ok": False, "reasons": reasons,
                   "run": None, "jobs": [], "profile_evidence": None}
    run = _pick_run(runs, sha)
    if run is None:
        reasons.append("该 SHA 无 run（event∈{workflow_dispatch, push}）")
        return entry
    run_id = run.get("id")
    sha_ok = run.get("head_sha") == sha
    completed = run.get("status") == "completed"
    entry["run"] = {"id": run_id, "event": run.get("event"),
                    "status": run.get("status"),
                    "conclusion": run.get("conclusion"),
                    "head_sha_ok": sha_ok, "html_url": run.get("html_url")}
    if not sha_ok:
        reasons.append(f"head_sha 不匹配：observed={run.get('head_sha')}")
    if not completed:
        reasons.append(f"run 未完成（status={run.get('status')}）")
    j_status, jobs_payload = _get_json(
        f"{API_BASE}/repos/{repo}/actions/runs/{run_id}/jobs?per_page=100",
        timeout)
    if j_status != 200:
        raise ApiError(f"jobs API 非 2xx（status={j_status}，run={run_id}）")
    jobs = jobs_payload.get("jobs") or []
    job_entries = [{"name": j.get("name"), "conclusion": j.get("conclusion"),
                    "ok": j.get("conclusion") == "success"}
                   for j in jobs]
    entry["jobs"] = job_entries
    if not job_entries:
        reasons.append("run 无 job（异常形态）")
    elif not all(j["ok"] for j in job_entries):
        bad = ", ".join(f"{j['name']}={j['conclusion']}"
                        for j in job_entries if not j["ok"])
        reasons.append(f"job 失败：{bad}")
    if profile is not None:
        evidence = _verify_profile_evidence(repo, run_id, sha, profile, timeout)
        entry["profile_evidence"] = evidence
        if not evidence["ok"]:
            reasons.append(f"profile 证据不一致：{evidence['reason']}")
    entry["ok"] = not reasons
    return entry


def verify_all(args: argparse.Namespace) -> dict:
    repo = resolve_repo(args.repo)
    status, payload = _get_json(
        f"{API_BASE}/repos/{repo}/actions/workflows?per_page=100",
        args.timeout)
    if status != 200:
        raise ApiError(f"workflows API 非 2xx（status={status}，repo={repo}；"
                       "仓库不存在或需凭据时请检查 --repo/GITHUB_TOKEN）")
    workflows = payload.get("workflows") or []
    entries = []
    for key in args.workflows:
        wf = resolve_workflow(workflows, key)
        entries.append(verify_workflow(repo, key, wf, args.sha,
                                       args.profile, args.timeout))
    verdict = "PASS" if all(e["ok"] for e in entries) else "FAIL"
    return {"tool": "ci/verify_remote_run.py", "utc": utc_iso(),
            "mode": "online", "repo": repo, "sha": args.sha,
            "profile": args.profile, "verdict": verdict,
            "workflows": entries}


# --------------------------------------------------------------------- CLI ----

class _ArgParser(argparse.ArgumentParser):
    """argparse 用法错误统一 exit 3（与合同 exit 语义对齐，不占 exit 2）。"""

    def error(self, message):
        print(f"verify_remote_run: 用法错误: {message}", file=sys.stderr)
        raise SystemExit(EXIT_USAGE)


def build_parser() -> argparse.ArgumentParser:
    ap = _ArgParser(prog="ci/verify_remote_run.py",
                    description="V8-CI-012 F-D4：hosted run 机器复验"
                                "（sha/workflows/profile 证据一致性）")
    ap.add_argument("--sha", default=None, help="待核验的 40 位完整 commit SHA")
    ap.add_argument("--workflows", nargs="+", default=None, metavar="NAME",
                    help="workflow 标识（数字 id/文件名/文件名主干/name 全名/"
                         "词集唯一匹配），可多个")
    ap.add_argument("--profile", default=None,
                    help="核验 artifact CI_RESULT.json 的 profile 字段一致")
    ap.add_argument("--timeout", type=float, default=_TIMEOUT_DEFAULT,
                    help=f"单请求超时秒数（默认 {_TIMEOUT_DEFAULT:g}）")
    ap.add_argument("--repo", default=None,
                    help="owner/repo（默认 GITHUB_REPOSITORY > git remote origin）")
    ap.add_argument("--offline", action="store_true",
                    help="结构模式：仅校验参数并输出报告契约，不做网络请求")
    ap.add_argument("--json", action="store_true", help="以 JSON 输出完整报告")
    return ap


def _validate_args(args: argparse.Namespace) -> None:
    if not args.sha or not _SHA_RE.match(args.sha):
        raise UsageError("--sha 必须是 40 位完整 commit SHA（小写 hex）")
    if not args.workflows or any((not k.strip()) for k in args.workflows):
        raise UsageError("--workflows 至少提供一个非空 workflow 标识")
    if args.profile is not None and not args.profile.strip():
        raise UsageError("--profile 不可为空字符串")
    if not (args.timeout > 0):
        raise UsageError(f"--timeout 必须为正数，实际 {args.timeout}")


def _emit(report: dict, as_json: bool) -> None:
    if as_json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return
    for entry in report.get("workflows", []):
        run = entry.get("run") or {}
        line = (f"verify_remote_run: {'OK' if entry['ok'] else 'FAIL'} "
                f"{entry['key']} -> run={run.get('id')} "
                f"event={run.get('event')} status={run.get('status')} "
                f"conclusion={run.get('conclusion')}")
        if entry.get("profile_evidence") is not None:
            ev = entry["profile_evidence"]
            line += (f" profile_evidence={'OK' if ev['ok'] else 'FAIL'}"
                     f"(observed={ev.get('observed_profile')})")
        print(line)
        for reason in entry.get("reasons", []):
            print(f"verify_remote_run:   理由: {reason}", file=sys.stderr)
    print(f"verify_remote_run: {report.get('verdict')} "
          f"(mode={report.get('mode')}, workflows={len(report.get('workflows', []))})")


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        _validate_args(args)
    except UsageError as exc:
        print(f"verify_remote_run: 用法错误: {exc}", file=sys.stderr)
        return EXIT_USAGE

    if args.offline:
        # 结构模式：参数已校验合法，输出报告契约骨架，不触网
        report = {"tool": "ci/verify_remote_run.py", "utc": utc_iso(),
                  "mode": "offline", "sha": args.sha,
                  "profile": args.profile, "verdict": "PASS",
                  "workflows": [{"key": k, "ok": True, "reasons": [],
                                 "run": None, "jobs": [],
                                 "profile_evidence": None}
                                for k in args.workflows],
                  "note": "结构模式（--offline）：仅校验参数与输出契约，"
                          "不做任何网络请求"}
        _emit(report, args.json)
        return EXIT_OK

    try:
        report = verify_all(args)
    except UsageError as exc:
        print(f"verify_remote_run: 用法错误: {exc}", file=sys.stderr)
        return EXIT_USAGE
    except ApiError as exc:
        err = {"tool": "ci/verify_remote_run.py", "utc": utc_iso(),
               "mode": "online", "verdict": "API_UNAVAILABLE",
               "error": str(exc)}
        if args.json:
            print(json.dumps(err, ensure_ascii=False, indent=2))
        else:
            print(f"verify_remote_run: API_UNAVAILABLE: {exc}", file=sys.stderr)
        return EXIT_ENV

    _emit(report, args.json)
    return EXIT_OK if report["verdict"] == "PASS" else EXIT_VERDICT_FAIL


if __name__ == "__main__":
    sys.exit(main())
