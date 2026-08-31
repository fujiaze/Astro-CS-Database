#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_commits_csv.py — GOV-001 COMMITS.csv 验证器。

验证（对应 GOV-001 验收）：
1. 每个 commit subject 只出现一个 task ID（生成器已保证，此处复核 CSV 内唯一）；
2. parent/result 形成连续链：result_commit 的父提交 == parent_commit；
3. 每个 result commit 是最终 source commit（HEAD/main）祖先；
4. origin_main 证明存在：pushed_origin_main=true 且 merge-base --is-ancestor result origin/main；
5. task_result_sha256 与 TASK_RESULT.json 文件实际 hash 一致；
6. changed_paths_sha256 与 git show --name-only 一致；
7. committed_utc 与 git 记录一致。

负面 fixture（--tamper）：
- tamper-sha：篡改一个 parent_commit → 必须失败；
- swap：交换两个任务的 result_commit → 必须失败；
- unpushe d：pushed_origin_main 改 false → 必须失败。

用法:
  python3 tools/quality/check_commits_csv.py --repo ROOT --commits COMMITS.csv --results-dir evidence/v6_1_rework/tasks
  python3 tools/quality/check_commits_csv.py --selftest
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

REQUIRED = {"task_id", "parent_commit", "result_commit", "commit_subject",
            "committed_utc", "pushed_origin_main", "changed_paths_sha256",
            "task_result_sha256"}
SHA40 = set("0123456789abcdef")


def git(repo: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", "-C", str(repo), *args],
                          capture_output=True, text=True, timeout=60)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def is_40hex(value: str) -> bool:
    return len(value) == 40 and all(c in SHA40 for c in value)


def check(repo: Path, commits_csv: Path, results_dir: Path) -> list[str]:
    errors: list[str] = []
    with commits_csv.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None or not REQUIRED.issubset(set(reader.fieldnames)):
            return [f"COMMITS.csv columns mismatch: {reader.fieldnames}"]
        rows = list(reader)

    seen_tasks: set[str] = set()
    seen_commits: set[str] = set()
    for row in rows:
        task_id, result, parent = row["task_id"], row["result_commit"], row["parent_commit"]
        if task_id in seen_tasks:
            errors.append(f"duplicate task_id: {task_id}")
        seen_tasks.add(task_id)
        if result in seen_commits:
            errors.append(f"duplicate result_commit {result} for {task_id}（两个任务不可共享 commit）")
        seen_commits.add(result)
        if not is_40hex(result) or not is_40hex(parent):
            errors.append(f"{task_id}: bad sha (result={result} parent={parent})")
        if not row["commit_subject"].startswith(task_id + ":"):
            errors.append(f"{task_id}: subject does not start with task id")
        # parent chain
        proc = git(repo, "rev-parse", result + "^")
        actual_parent = proc.stdout.strip() if proc.returncode == 0 else ""
        if actual_parent != parent:
            errors.append(f"{task_id}: result parent {actual_parent} != parent_commit {parent}")
        # ancestor of HEAD/main
        if git(repo, "merge-base", "--is-ancestor", result, "main").returncode != 0:
            errors.append(f"{task_id}: result {result} is not an ancestor of main")
        # pushed to origin/main
        pushed = row["pushed_origin_main"].lower() == "true"
        on_origin = git(repo, "merge-base", "--is-ancestor", result, "origin/main").returncode == 0
        if pushed and not on_origin:
            errors.append(f"{task_id}: claims pushed but result {result} not on origin/main")
        if not pushed and on_origin:
            errors.append(f"{task_id}: claims unpushed but result {result} IS on origin/main")
        # committed_utc
        actual_utc = git(repo, "log", "-1", "--format=%cI", result).stdout.strip()
        if row["committed_utc"] != actual_utc:
            errors.append(f"{task_id}: committed_utc mismatch {row['committed_utc']} != {actual_utc}")
        # changed_paths_sha256
        paths = git(repo, "show", "--name-only", "--format=", result).stdout
        h = hashlib.sha256(); h.update(paths.encode("utf-8"))
        if row["changed_paths_sha256"] != h.hexdigest():
            errors.append(f"{task_id}: changed_paths_sha256 mismatch")
        # task_result_sha256
        result_path = results_dir / task_id / "TASK_RESULT.json"
        if not result_path.is_file():
            errors.append(f"{task_id}: TASK_RESULT.json missing")
        elif row["task_result_sha256"] != sha256(result_path):
            errors.append(f"{task_id}: task_result_sha256 mismatch")
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--commits", type=Path)
    parser.add_argument("--results-dir", type=Path,
                        default=Path("evidence/v6_1_rework/tasks"))
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)

    repo: Path = args.repo.resolve()

    if args.selftest:
        return _selftest(repo)

    if args.commits is None:
        print("COMMITS_CHECK_FAIL: --commits required", file=sys.stderr)
        return 2
    errors = check(repo, args.commits.resolve(), args.results_dir.resolve())
    if errors:
        print("COMMITS_CHECK_FAIL")
        for err in errors[:40]:
            print(f"  - {err}", file=sys.stderr)
        return 1
    print("COMMITS_CHECK_PASS")
    return 0


def _selftest(repo: Path) -> int:
    """用当前仓库真实提交生成三类篡改 fixture，逐一验证必须失败。"""
    import re
    proc = git(repo, "log", "--first-parent", "--format=%H %s", "main")
    lines = proc.stdout.splitlines()
    subjects: list[tuple[str, str]] = []
    for line in lines:
        m = re.match(r"^([0-9a-f]{40}) ([A-Z0-9]+-[0-9]{3}):", line)
        if m:
            subjects.append((m.group(1), m.group(2)))
    if len(subjects) < 2:
        print("SELFTEST_FAIL: need at least 2 task commits", file=sys.stderr)
        return 1

    results_dir = (repo / "evidence" / "v6_1_rework" / "tasks").resolve()
    gen = (repo / "tools" / "quality" / "gen_commits_csv.py").resolve()
    import subprocess as sp
    with tempfile.TemporaryDirectory() as tmp:
        tmpd = Path(tmp)
        real = tmpd / "COMMITS_real.csv"
        proc = sp.run([sys.executable, str(gen), "--repo", str(repo),
                       "--results-dir", str(results_dir), "--out", str(real)],
                      capture_output=True, text=True, timeout=120)
        if proc.returncode != 0:
            print(f"SELFTEST_FAIL: gen failed: {proc.stderr}", file=sys.stderr)
            return 1

        # 1. tamper parent sha
        rows = list(csv.DictReader(open(real, encoding="utf-8")))
        rows[0]["parent_commit"] = "0" * 40
        tampered = tmpd / "COMMITS_tamper.csv"
        with tampered.open("w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
        errs = check(repo, tampered, results_dir)
        if not any("parent_commit" in e or "parent" in e for e in errs):
            print("SELFTEST_FAIL: tampered parent not caught", file=sys.stderr)
            return 1

        # 2. swap two result commits
        rows = list(csv.DictReader(open(real, encoding="utf-8")))
        rows[0]["result_commit"], rows[1]["result_commit"] = rows[1]["result_commit"], rows[0]["result_commit"]
        swapped = tmpd / "COMMITS_swap.csv"
        with swapped.open("w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
        errs = check(repo, swapped, results_dir)
        if not any("parent" in e or "subject" in e for e in errs):
            print("SELFTEST_FAIL: swapped commits not caught", file=sys.stderr)
            return 1

        # 3. mark unpushed
        rows = list(csv.DictReader(open(real, encoding="utf-8")))
        rows[0]["pushed_origin_main"] = "false"
        unpushed = tmpd / "COMMITS_unpushed.csv"
        with unpushed.open("w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
        errs = check(repo, unpushed, results_dir)
        if not any("pushed" in e or "origin" in e for e in errs):
            print("SELFTEST_FAIL: unpushed not caught", file=sys.stderr)
            return 1

        # positive: real passes
        errs = check(repo, real, results_dir)
        if errs:
            print(f"SELFTEST_FAIL: real COMMITS fails: {errs[:5]}", file=sys.stderr)
            return 1
    print("SELFTEST_PASS tamper/swap/unpushed all caught + real PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
