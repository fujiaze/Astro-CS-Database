#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/verify_actions_lock.py —— V8-CI-007 action 锁机器复验。

读 ``ci/actions.lock.json``，逐条在线查询 GitHub API（api.github.com）
比对 ``tag -> commit_sha`` 是否一致：

- 一致            -> exit 0（PASS）；
- tag 不存在 /
  指向 SHA 不一致 -> exit 1（sha_mismatch）；
- 网络不可达 /
  API 非 2xx      -> exit 2（network_unavailable）；
- 锁文件结构非法  -> exit 3（lock_invalid）。

``--offline``：不做任何网络请求，仅做结构校验（单测用）。

设计纪律：
- 每条 HTTP 查询带 timeout（默认 20s，可 --timeout 覆盖），失败重试 <=2 次；
- 匿名 API，不读取、不落盘任何 token/凭据；
- 请求 URL 与响应摘要逐条落到 --log 指定文件（JSONL），不写响应正文全文；
- workflow 未使用的旧条目允许存在（used_by 为空仅告警不失败）。

Exit code：0=PASS；1=sha_mismatch/tag 缺失；2=network_unavailable；3=lock_invalid。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_LOCK = Path(__file__).resolve().parent / "actions.lock.json"
API_BASE = "https://api.github.com"

EXIT_OK = 0                # 全部条目复验一致
EXIT_SHA_MISMATCH = 1      # tag 不存在或 SHA 不一致
EXIT_NETWORK = 2           # 网络不可达 / API 非 2xx（超时、DNS、限流）
EXIT_LOCK_INVALID = 3      # 锁文件结构非法

MAX_RETRIES = 2            # 网络失败重试 <=2 次
REQUEST_TIMEOUT_DEFAULT = 20.0

REQUIRED_ENTRY_FIELDS = ("action", "tag", "commit_sha", "verified_utc")
_SHA_RE = re.compile(r"^[0-9a-f]{40}$")
_ACTION_RE = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
_TAG_RE = re.compile(r"^[0-9A-Za-z][0-9A-Za-z._-]*$")


def utc_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


class LockInvalid(Exception):
    """锁文件结构非法（exit 3）。"""


class NetworkUnavailable(Exception):
    """GitHub API 不可达（exit 2）。"""


class ShaMismatch(Exception):
    """tag 不存在或 tag->SHA 与锁文件不一致（exit 1）。"""

    def __init__(self, action: str, tag: str, reason: str):
        super().__init__(f"{action}@{tag}: {reason}")
        self.action = action
        self.tag = tag
        self.reason = reason


# ----------------------------------------------------------------- 结构校验 ----

def load_lock(path: Path) -> dict:
    """读并结构校验锁文件；非法即 raise LockInvalid（不吞异常）。"""
    if not path.is_file():
        raise LockInvalid(f"锁文件不存在：{path}")
    try:
        lock = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise LockInvalid(f"锁文件非合法 JSON：{exc}") from exc
    if not isinstance(lock, dict):
        raise LockInvalid("锁文件顶层必须是 JSON object")
    if lock.get("schema_version") != 1:
        raise LockInvalid(f"schema_version 必须为 1，实际 {lock.get('schema_version')!r}")
    entries = lock.get("entries")
    if not isinstance(entries, list) or not entries:
        raise LockInvalid("entries 必须是非空数组")
    seen: set[tuple[str, str]] = set()
    for i, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise LockInvalid(f"entries[{i}] 必须是 object")
        for field in REQUIRED_ENTRY_FIELDS:
            if not isinstance(entry.get(field), str) or not entry[field].strip():
                raise LockInvalid(f"entries[{i}].{field} 必须是非空字符串")
        action, tag, sha = entry["action"], entry["tag"], entry["commit_sha"]
        if not _ACTION_RE.match(action):
            raise LockInvalid(f"entries[{i}].action 形态非法：{action!r}")
        if not _TAG_RE.match(tag):
            raise LockInvalid(f"entries[{i}].tag 形态非法：{tag!r}")
        if not _SHA_RE.match(sha):
            raise LockInvalid(
                f"entries[{i}].commit_sha 必须是 40 位完整 SHA：{sha!r}"
                "（禁止短 SHA / 移动 tag 引用）")
        key = (action, tag)
        if key in seen:
            raise LockInvalid(f"entries 重复条目：{action}@{tag}")
        seen.add(key)
    return lock


# ----------------------------------------------------------------- 在线复验 ----

def _http_get_json(url: str, timeout: float) -> tuple[int, dict | str]:
    """GET 一个 URL，返回 (status, body)。网络异常上抛（由调用方重试）。"""
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "astrocs-ci-verify-actions-lock",
            # 匿名 API：显式不带任何 Authorization 头（凭据不进本脚本）。
        },
        method="GET",
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read().decode("utf-8", errors="replace")
            return response.status, body
    except urllib.error.HTTPError as exc:
        # HTTPError 是响应（4xx/5xx），status 交给调用方分类，不算 network 异常
        return exc.code, exc.read().decode("utf-8", errors="replace")


def fetch_tag_commit(owner: str, repo: str, tag: str, *, timeout: float,
                     log_file=None) -> str:
    """在线查 repos/<owner>/<repo>/git/refs/tags/<tag>，返回其 commit SHA。

    refs API 对 annotated tag 返回 type=tag（对象 SHA 而非 commit），
    此时解引用 /git/tags/<sha> 取其 object.commit.sha（两个官方仓库
    当前均为 type=commit 的轻量 tag，此分支为防御性正确性保留）。
    """
    import urllib.parse
    url = f"{API_BASE}/repos/{owner}/{repo}/git/refs/tags/{urllib.parse.quote(tag)}"
    last_err: Exception | None = None
    for attempt in range(1 + MAX_RETRIES):
        try:
            status, body = _http_get_json(url, timeout)
            if status == 200:
                payload = json.loads(body)
                obj = payload.get("object") or {}
                if obj.get("type") == "commit" and obj.get("sha"):
                    _log_line(log_file, {"utc": utc_iso(), "url": url,
                                         "status": status,
                                         "object_type": "commit",
                                         "commit_sha": obj["sha"]})
                    return obj["sha"]
                # annotated tag -> 解引用到 commit
                tag_url = f"{API_BASE}/repos/{owner}/{repo}/git/tags/{obj.get('sha', '')}"
                t_status, t_body = _http_get_json(tag_url, timeout)
                if t_status == 200:
                    commit = (json.loads(t_body).get("object") or {})
                    if commit.get("type") == "commit" and commit.get("sha"):
                        _log_line(log_file, {"utc": utc_iso(), "url": url,
                                             "status": status,
                                             "object_type": "tag",
                                             "commit_sha": commit["sha"]})
                        return commit["sha"]
                raise ShaMismatch(action_str(owner, repo), tag,
                                  f"annotated tag 解引用失败（status={t_status}）")
            if status == 404:
                raise ShaMismatch(action_str(owner, repo), tag,
                                  "tag 不存在（GitHub API 404）")
            if status in (403, 429):
                last_err = NetworkUnavailable(
                    f"API 限流/拒绝（status={status}）")
            else:
                last_err = NetworkUnavailable(f"API 非 2xx（status={status}）")
        except (urllib.error.URLError, TimeoutError, OSError, json.JSONDecodeError) as exc:
            last_err = NetworkUnavailable(f"网络不可达：{exc}")
        if attempt < MAX_RETRIES:
            time.sleep(1.0 * (attempt + 1))  # 线性退避
    assert last_err is not None
    raise last_err


def action_str(owner: str, repo: str) -> str:
    return f"{owner}/{repo}"


def _log_line(log_file, payload: dict) -> None:
    if log_file is None:
        return
    log_file.parent.mkdir(parents=True, exist_ok=True)
    with log_file.open("a", encoding="utf-8") as fh:
        fh.write(json.dumps(payload, ensure_ascii=False) + "\n")


def split_action(action: str) -> tuple[str, str]:
    owner, _, repo = action.partition("/")
    return owner, repo


def verify_online(lock: dict, *, timeout: float, log_file=None) -> dict:
    """逐条在线比对 tag -> commit_sha；返回报告 dict（含每条结果）。"""
    results: list[dict] = []
    mismatched: list[str] = []
    for entry in lock["entries"]:
        action, tag, expected = entry["action"], entry["tag"], entry["commit_sha"]
        owner, repo = split_action(action)
        try:
            observed = fetch_tag_commit(owner, repo, tag, timeout=timeout,
                                        log_file=log_file)
        except NetworkUnavailable:
            raise
        except ShaMismatch as exc:
            results.append({"action": action, "tag": tag, "expected_sha": expected,
                            "observed_sha": None, "ok": False,
                            "reason": exc.reason})
            mismatched.append(f"{action}@{tag}")
            continue
        ok = observed == expected
        if not ok:
            mismatched.append(f"{action}@{tag}")
        results.append({"action": action, "tag": tag,
                        "expected_sha": expected, "observed_sha": observed,
                        "ok": ok,
                        "reason": None if ok else
                        "tag->SHA 与锁文件不一致"})
    return {"entries": results, "mismatched": mismatched}


# --------------------------------------------------------------------- CLI ----

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ci/verify_actions_lock.py",
        description="V8-CI-007：机器复验 ci/actions.lock.json（tag->SHA 在线比对）。")
    parser.add_argument("--lock", default=str(DEFAULT_LOCK),
                        help="锁文件路径（默认 ci/actions.lock.json）")
    parser.add_argument("--offline", action="store_true",
                        help="离线模式：仅结构校验，不做网络请求（单测用）")
    parser.add_argument("--timeout", type=float, default=REQUEST_TIMEOUT_DEFAULT,
                        help=f"单次 HTTP 超时秒数（默认 {REQUEST_TIMEOUT_DEFAULT:g}）")
    parser.add_argument("--json", action="store_true",
                        help="以 JSON 输出完整报告")
    parser.add_argument("--log", default=None,
                        help="查询日志 JSONL 路径（默认不落盘）")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    log_file = Path(args.log) if args.log else None
    try:
        lock = load_lock(Path(args.lock))
    except LockInvalid as exc:
        report = {"verdict": "LOCK_INVALID", "error": str(exc), "utc": utc_iso()}
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            print(f"verify_actions_lock: LOCK_INVALID: {exc}", file=sys.stderr)
        return EXIT_LOCK_INVALID

    if args.offline:
        report = {"verdict": "PASS", "mode": "offline", "utc": utc_iso(),
                  "entry_count": len(lock["entries"]),
                  "note": "结构校验通过（--offline 不做网络比对）"}
        print(json.dumps(report, ensure_ascii=False, indent=2)
              if args.json else
              f"verify_actions_lock: PASS (offline, {len(lock['entries'])} entries)")
        return EXIT_OK

    try:
        online = verify_online(lock, timeout=args.timeout, log_file=log_file)
    except NetworkUnavailable as exc:
        report = {"verdict": "NETWORK_UNAVAILABLE", "error": str(exc),
                  "utc": utc_iso()}
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            print(f"verify_actions_lock: NETWORK_UNAVAILABLE: {exc}",
                  file=sys.stderr)
        return EXIT_NETWORK

    report = {"verdict": "PASS" if not online["mismatched"] else "SHA_MISMATCH",
              "mode": "online", "utc": utc_iso(),
              "entry_count": len(online["entries"]),
              "entries": online["entries"]}
    if online["mismatched"]:
        report["mismatched"] = online["mismatched"]
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            for item in online["entries"]:
                if not item["ok"]:
                    print(f"verify_actions_lock: SHA_MISMATCH: "
                          f"{item['action']}@{item['tag']}: "
                          f"expected={item['expected_sha']} "
                          f"observed={item['observed_sha']} ({item['reason']})",
                          file=sys.stderr)
        return EXIT_SHA_MISMATCH

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        for item in online["entries"]:
            print(f"verify_actions_lock: OK {item['action']}@{item['tag']} "
                  f"-> {item['observed_sha']}")
        print(f"verify_actions_lock: PASS ({len(online['entries'])} entries)")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
