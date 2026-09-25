#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/verify_actions_lock.py —— V8-CI-007 action 锁机器复验。

读 ``eng/ci/actions.lock.json``，逐条在线查询 GitHub API（api.github.com）
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

REPO = Path(__file__).resolve().parent.parent.parent
DEFAULT_LOCK = Path(__file__).resolve().parent / "actions.lock.json"
API_BASE = "https://api.github.com"

EXIT_OK = 0                # 全部条目复验一致
EXIT_SHA_MISMATCH = 1      # tag 不存在或 SHA 不一致
EXIT_NETWORK = 2           # 网络不可达 / API 非 2xx（超时、DNS、限流）
EXIT_LOCK_INVALID = 3      # 锁文件结构非法

MAX_RETRIES = 2            # 网络失败重试 <=2 次
REQUEST_TIMEOUT_DEFAULT = 20.0
# 合同化 SKIP（ctest SKIP_RETURN_CODE 同语义）：宿主能力缺失（本机无外网）时，
# 在线复验**未执行**——必须记 SKIP 而不是 PASS（run_checks/run.py 只在 waivable
# 单元上承认 77，非 waivable 单元用 77 = 自我豁免 ⇒ 判红）。
SKIP_EXIT_CODE = 77

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


# ------------------------------------------------------- workflow ↔ lock 交叉核对 ----
# 为什么必须有这一节（GATE-SOLID-01 / 独立审查节点一页纸 S2-A「判据无注册承载」）：
#   本脚本的 tag→SHA **在线复验**是真实现，但此前 checks.json 零注册、workflow 只
#   在注释里提到它、CI 只跑 --offline（= 只做锁文件自身的结构校验）。于是
#   「锁文件说什么」与「workflow 实际锁了什么」之间没有任何判据：workflow 里写了
#   一个不在锁文件里的 SHA，或者退回可移动 tag，都不会红。
# 判据：**读真实产品**（.github/workflows/*.yml 的 uses: 行 + eng/ci/actions.lock.json），
#   1. 每个 uses: <owner>/<repo>@<ref> 的 ref 必须是完整 40 位 SHA；
#   2. (action, ref) 必须能在锁文件条目里精确匹配；
#   3. 本地引用（./path）与 docker:// 不计入（不是 GitHub action 锁面）；
#   4. workflow 面 0 个文件、或 0 条 uses: 引用 ⇒ 判红（扫描面为空 = 门没看对象）。
USES_RE = re.compile(r"^\s*(?:-\s*)?uses:\s*(\S+)\s*(?:#.*)?$")


def _rel(repo: Path, path: str) -> str:
    try:
        return Path(path).resolve().relative_to(repo.resolve()).as_posix()
    except ValueError:
        return path


def workflow_refs(workflow_dir: Path) -> list:
    """逐行抽出 workflow 的 uses: 引用（带 文件:行，判词可定位）。"""
    files = sorted(set(list(workflow_dir.glob("*.yml")) + list(workflow_dir.glob("*.yaml"))))
    files = [f for f in files if f.is_file()]
    refs: list = []
    for f in files:
        try:
            text = f.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            refs.append({"workflow": str(f), "line": 0, "raw": None,
                         "error": f"无法读取：{exc}"})
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            m = USES_RE.match(line)
            if m:
                refs.append({"workflow": str(f), "line": lineno, "raw": m.group(1)})
    return refs


def verify_workflow_bindings(lock: dict, *, workflow_dir: Path, repo: Path) -> dict:
    """workflow 的 uses: 引用 vs 锁文件条目（只读真实产品；返回含 problems 的报告）。"""
    problems: list = []
    files = sorted(set(list(workflow_dir.glob("*.yml")) + list(workflow_dir.glob("*.yaml"))))
    files = [f for f in files if f.is_file()]
    if not files:
        problems.append(
            "WORKFLOW_SCAN_EMPTY: workflow 目录不存在或没有任何 *.yml：%s"
            "（扫描面为空 = 门没看对象；fail-closed 判红）" % workflow_dir)
        return {"workflow_dir": str(workflow_dir), "files": [], "refs": [],
                "action_refs": 0, "problems": problems}

    locked = {(e["action"], e["commit_sha"]) for e in lock["entries"]}
    refs = workflow_refs(workflow_dir)
    action_refs = 0
    for ref in refs:
        rel = _rel(repo, ref["workflow"])
        if ref.get("error"):
            problems.append("%s: %s" % (rel, ref["error"]))
            continue
        raw = ref["raw"]
        if raw.startswith("./") or raw.startswith("docker://"):
            continue  # 本地 action / 容器 action：不属 GitHub action 锁面
        action_refs += 1
        if "@" not in raw:
            problems.append("%s:%d uses %s —— 未锁 SHA（可移动引用；actions.lock 复验要求 "
                            "owner/repo@<40 位 SHA>）" % (rel, ref["line"], raw))
            continue
        action, _, sha = raw.rpartition("@")
        if not _SHA_RE.match(sha):
            problems.append("%s:%d uses %s —— ref 不是完整 40 位 SHA（禁止 tag/branch 等"
                            "可移动引用）" % (rel, ref["line"], raw))
            continue
        if (action, sha) not in locked:
            known = sorted(a for a, _s in locked if a == action)
            problems.append(
                "%s:%d uses %s —— 与 eng/ci/actions.lock.json 无匹配条目%s"
                % (rel, ref["line"], raw,
                   ("（同名 action 在册 SHA：%s）" % ", ".join(known)) if known else ""))
    if action_refs == 0:
        problems.append("WORKFLOW_SCAN_EMPTY: %d 个 workflow 文件里 0 条 GitHub action "
                        "uses: 引用（扫描面为空 = 门没看对象；fail-closed 判红）" % len(files))
    return {"workflow_dir": str(workflow_dir),
            "files": [_rel(repo, str(f)) for f in files],
            "refs": refs, "action_refs": action_refs, "problems": problems}



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
        prog="eng/ci/verify_actions_lock.py",
        description="V8-CI-007：机器复验 eng/ci/actions.lock.json（tag->SHA 在线比对）。")
    parser.add_argument("--lock", default=str(DEFAULT_LOCK),
                        help="锁文件路径（默认 eng/ci/actions.lock.json）")
    parser.add_argument("--offline", action="store_true",
                        help="离线模式：仅结构校验，不做网络请求（单测用）")
    parser.add_argument("--timeout", type=float, default=REQUEST_TIMEOUT_DEFAULT,
                        help=f"单次 HTTP 超时秒数（默认 {REQUEST_TIMEOUT_DEFAULT:g}）")
    parser.add_argument("--json", action="store_true",
                        help="以 JSON 输出完整报告")
    parser.add_argument("--log", default=None,
                        help="查询日志 JSONL 路径（默认不落盘）")
    parser.add_argument("--workflow-dir", default=".github/workflows",
                        help="workflow 目录（默认 .github/workflows；相对仓库根）——"
                             "离线/在线都做 workflow↔lock 交叉核对（读真实产品）")
    parser.add_argument("--allow-network-skip", action="store_true",
                        help="网络不可达时以合同化 SKIP(77) 退出而不是 exit 2："
                             "判据未执行须记 SKIP(waivable)，不是 PASS（供注册项使用）")
    parser.add_argument("--selftest", action="store_true",
                        help="负例面：tag 引用 / 锁外 SHA / 空 workflow 面必须判红")
    parser.add_argument("--report-out", default=None,
                        help="把完整判定报告写入该路径（仓库相对；注册项 outputs 用）")
    return parser


# 自检用的内存锁夹具（与真实 actions.lock.json 同 schema；真实文件仍被主流程读取）
LOCK_FIXTURE = {"schema_version": 1, "entries": [
    {"action": "actions/checkout", "tag": "v7.0.1",
     "commit_sha": "3d3c42e5aac5ba805825da76410c181273ba90b1",
     "verified_utc": "2026-01-01T00:00:00Z"}]}


def selftest() -> int:
    """负例面（GATE-SOLID-01 / S2-A）：workflow↔lock 交叉核对必须能红。

    N1 tag 引用（可移动）⇒ 红；N2 锁外 SHA ⇒ 红；N3 空 workflow 面 ⇒ 红；
    N4 只有本地 action（0 条 action 引用）⇒ 红；P1/P2 正确锁定 ⇒ 绿。
    全部在临时目录上跑，不碰真实仓库、不发网络请求。
    """
    import tempfile
    cases: list = []
    lock = LOCK_FIXTURE
    sha = lock["entries"][0]["commit_sha"]

    def run(name, body, expect_problems):
        with tempfile.TemporaryDirectory() as tmp:
            wf = Path(tmp) / "wf"
            wf.mkdir()
            if body is not None:
                (wf / "a.yml").write_text(body, encoding="utf-8")
            rep = verify_workflow_bindings(lock, workflow_dir=wf, repo=Path(tmp))
            got = bool(rep["problems"])
            cases.append({"case": name, "ok": got == expect_problems,
                          "problems": rep["problems"][:2]})

    run("P1_locked_sha_green",
        "jobs:\n  a:\n    steps:\n      - uses: actions/checkout@%s\n" % sha, False)
    run("N1_tag_ref_is_red",
        "jobs:\n  a:\n    steps:\n      - uses: actions/checkout@v7.0.1\n", True)
    run("N2_sha_not_in_lock_is_red",
        "jobs:\n  a:\n    steps:\n      - uses: actions/checkout@%s\n" % ("0" * 40),
        True)
    run("N3_empty_workflow_face_is_red", None, True)
    run("N4_local_action_only_is_red",
        "jobs:\n  a:\n    steps:\n      - uses: ./.github/actions/x\n", True)
    run("P2_local_plus_locked_green",
        "jobs:\n  a:\n    steps:\n      - uses: ./.github/actions/x\n"
        "      - uses: actions/checkout@%s\n" % sha, False)

    ok = all(c["ok"] for c in cases)
    for c in cases:
        print("SELFTEST %s %s%s" % ("PASS" if c["ok"] else "FAIL", c["case"],
                                    "" if c["ok"] else "  " + repr(c["problems"])))
    print("VERIFY_ACTIONS_LOCK_SELFTEST_%s: %d/%d"
          % ("PASS" if ok else "FAIL", sum(1 for c in cases if c["ok"]), len(cases)))
    return 0 if ok else 1


def _emit(report: dict, args) -> None:
    """把判定报告落盘（注册项 outputs 锚；--report-out 未给时不写）。"""
    if not getattr(args, "report_out", None):
        return
    p = Path(args.report_out)
    if not p.is_absolute():
        p = REPO / p
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                 encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.selftest:
        return selftest()
    log_file = Path(args.log) if args.log else None
    try:
        lock = load_lock(Path(args.lock))
    except LockInvalid as exc:
        report = {"verdict": "LOCK_INVALID", "error": str(exc), "utc": utc_iso()}
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            print(f"verify_actions_lock: LOCK_INVALID: {exc}", file=sys.stderr)
        _emit(report, args)
        return EXIT_LOCK_INVALID

    wf_dir = Path(args.workflow_dir)
    if not wf_dir.is_absolute():
        wf_dir = REPO / wf_dir
    bind = verify_workflow_bindings(lock, workflow_dir=wf_dir, repo=REPO)

    if args.offline:
        # --offline 也不许"只读锁文件自己"：workflow↔lock 交叉核对**读真实产品**
        # （.github/workflows/*.yml 的 uses: 行），无网络即可执行 —— 这正是
        # S2-A「注册项至少一条命令读真实产品」的落点。
        verdict = "PASS" if not bind["problems"] else "WORKFLOW_LOCK_MISMATCH"
        report = {"verdict": verdict, "mode": "offline", "utc": utc_iso(),
                  "entry_count": len(lock["entries"]),
                  "workflow_binding": bind,
                  "note": "结构校验 + workflow↔lock 交叉核对（--offline 不做网络比对）"}
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        elif bind["problems"]:
            for p in bind["problems"]:
                print("verify_actions_lock: WORKFLOW_LOCK_MISMATCH: %s" % p,
                      file=sys.stderr)
        else:
            print("verify_actions_lock: PASS (offline, %d entries, workflow action refs=%d "
                  "in %d file(s))"
                  % (len(lock["entries"]), bind["action_refs"], len(bind["files"])))
        _emit(report, args)
        return EXIT_SHA_MISMATCH if bind["problems"] else EXIT_OK

    try:
        online = verify_online(lock, timeout=args.timeout, log_file=log_file)
    except NetworkUnavailable as exc:
        report = {"verdict": "NETWORK_UNAVAILABLE", "error": str(exc),
                  "utc": utc_iso(), "workflow_binding": bind}
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            print("verify_actions_lock: NETWORK_UNAVAILABLE: %s" % exc, file=sys.stderr)
        if args.allow_network_skip:
            # 宿主能力缺失（本机无外网）⇒ 判据**未执行**，按合同化 SKIP(77) 上报，
            # 由 runner 记 SKIPPED(waivable) 并计数 —— 不是 PASS。CI（有外网）上
            # 该旗标不改变判定：有网络就必须给出真判定，SHA 不符仍判红。
            print("verify_actions_lock: SKIP: 网络不可达，在线复验未执行（判据未执行 ≠ 通过）",
                  file=sys.stderr)
            _emit(report, args)
            return SKIP_EXIT_CODE
        _emit(report, args)
        return EXIT_NETWORK

    if bind["problems"]:
        report = {"verdict": "WORKFLOW_LOCK_MISMATCH", "mode": "online",
                  "utc": utc_iso(), "entry_count": len(lock["entries"]),
                  "workflow_binding": bind}
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            for p in bind["problems"]:
                print("verify_actions_lock: WORKFLOW_LOCK_MISMATCH: %s" % p, file=sys.stderr)
        _emit(report, args)
        return EXIT_SHA_MISMATCH

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
    _emit(report, args)
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
