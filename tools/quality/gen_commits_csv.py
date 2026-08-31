#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_commits_csv.py — GOV-001 任务→commit 绑定生成器。

从 `git log --first-parent main` 提取每个任务的 result_commit，与 TASK_RESULT.json 的
parent_commit 对照，生成 COMMITS.csv：

  task_id,parent_commit,result_commit,commit_subject,committed_utc,pushed_origin_main,changed_paths_sha256,task_result_sha256

规则：
- parent_commit 来自 TASK_RESULT.json（任务开始 SHA）；
- result_commit 只能从 git log --first-parent 提取（commit subject 以 <TASK-ID>: 开头）；
- 若某个 commit-required 任务缺 commit 或 subject 不含唯一 task ID → 失败；
- 故意篡改 parent SHA / 交换任务 / 缺 push 由 check 脚本捕获（本脚本只做诚实提取）。

用法:
  python3 tools/quality/gen_commits_csv.py --repo ROOT --results-dir evidence/v6_1_rework/tasks --out COMMITS.csv
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import subprocess
import sys
from pathlib import Path

SUBJECT_RE = r"^([A-Z0-9]+-[0-9]{3}):"


def git(repo: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", "-C", str(repo), *args],
                          capture_output=True, text=True, timeout=60)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--results-dir", type=Path,
                        default=Path("evidence/v6_1_rework/tasks"))
    parser.add_argument("--out", type=Path, default=Path("evidence/v6_1_rework/COMMITS.csv"))
    parser.add_argument("--first-parent", type=Path, default=None,
                        help="可选：指定 first-parent 提交列表文件（测试用）")
    args = parser.parse_args(argv)
    repo: Path = args.repo.resolve()
    results_dir: Path = args.results_dir.resolve()
    out: Path = args.out.resolve()

    if args.first_parent is not None:
        lines = [line.strip() for line in args.first_parent.read_text(encoding="utf-8").splitlines() if line.strip()]
    else:
        # V6.1 起点: 从 R0-001 提交开始扫描(排除 V6 旧历史中的同名任务前缀, 如旧 CPU-001/002)
        proc = git(repo, "log", "--first-parent", "--format=%H %s", "main")
        if proc.returncode != 0:
            print(f"COMMITS_GEN_FAIL: git log failed: {proc.stderr}", file=sys.stderr)
            return 1
        lines = proc.stdout.splitlines()
        start = git(repo, "log", "--first-parent", "--format=%H", "main",
                    "--grep=^R0-001:")
        if start.returncode == 0 and start.stdout.strip():
            start_commit = start.stdout.splitlines()[0].strip()
            # 截取从 start_commit 开始(含)的子序列
            idx = next((i for i, ln in enumerate(lines) if ln.startswith(start_commit)), -1)
            if idx >= 0:
                lines = lines[:idx + 1]

    # map: task_id -> [commit records] from git log (all matches on subject prefix)
    commit_of: dict[str, list[dict]] = {}
    for line in lines:
        parts = line.split(" ", 1)
        if len(parts) != 2:
            continue
        commit, subject = parts
        import re
        # 支持双任务合并前缀 "CPU-001/CPU-002:" → 同时映射两个任务
        m = re.match(r"^([A-Z0-9]+-[0-9]{3})/([A-Z0-9]+-[0-9]{3}):", subject)
        if m:
            for tid in (m.group(1), m.group(2)):
                commit_of.setdefault(tid, []).append({"commit": commit, "subject": subject, "parent": ""})
            continue
        m = re.match(SUBJECT_RE, subject)
        if m:
            task_id = m.group(1)
            commit_of.setdefault(task_id, []).append({"commit": commit, "subject": subject, "parent": ""})

    # parent from git (commit^) for chain verification
    for records in commit_of.values():
        for record in records:
            proc = git(repo, "rev-parse", record["commit"] + "^")
            if proc.returncode == 0:
                record["parent"] = proc.stdout.strip()

    # read TASK_RESULT.json files
    rows: list[dict] = []
    for result_path in sorted(results_dir.glob("*/TASK_RESULT.json")):
        try:
            doc = json.loads(result_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            print(f"COMMITS_GEN_FAIL: bad TASK_RESULT {result_path}: {exc}", file=sys.stderr)
            return 1
        task_id = doc.get("task_id", "")
        if task_id not in commit_of:
            print(f"COMMITS_GEN_FAIL: no first-parent commit for task {task_id}", file=sys.stderr)
            return 1
        candidates = commit_of[task_id]
        parent = doc.get("parent_commit", "")
        # 优先选 parent==parent_commit 的候选(TASK_RESULT 记录任务开始 SHA);
        # 其次选 git log 中最新的候选(双任务合并提交等场景, 如 CPU-001/002 共享一个 commit)。
        record = next((c for c in candidates if c["parent"] == parent), None)
        if record is None:
            record = candidates[0]
        rows.append({
            "task_id": task_id,
            "parent_commit": parent,
            "result_commit": record["commit"],
            "commit_subject": record["subject"],
            "committed_utc": _commit_utc(repo, record["commit"]),
            "pushed_origin_main": "true" if _is_pushed(repo, record["commit"]) else "false",
            "changed_paths_sha256": _changed_paths_sha256(repo, record["commit"]),
            "task_result_sha256": sha256(result_path),
        })

    with out.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=[
            "task_id", "parent_commit", "result_commit", "commit_subject",
            "committed_utc", "pushed_origin_main", "changed_paths_sha256",
            "task_result_sha256"])
        writer.writeheader()
        writer.writerows(rows)
    print(f"COMMITS_GEN_PASS tasks={len(rows)} out={out}")
    return 0


def _commit_utc(repo: Path, commit: str) -> str:
    proc = git(repo, "log", "-1", "--format=%cI", commit)
    return proc.stdout.strip() if proc.returncode == 0 else ""


def _is_pushed(repo: Path, commit: str) -> bool:
    proc = git(repo, "merge-base", "--is-ancestor", commit, "origin/main")
    return proc.returncode == 0


def _changed_paths_sha256(repo: Path, commit: str) -> str:
    proc = git(repo, "show", "--name-only", "--format=", commit)
    if proc.returncode != 0:
        return ""
    h = hashlib.sha256()
    h.update(proc.stdout.encode("utf-8"))
    return h.hexdigest()


if __name__ == "__main__":
    raise SystemExit(main())
